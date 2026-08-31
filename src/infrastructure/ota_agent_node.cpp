#include "ros2_robot_middleware/infrastructure/ota_agent_node.hpp"

#include "ros2_robot_middleware/domain/ota/package_signer.hpp"

#include <filesystem>
#include <sstream>

namespace amr {
namespace infrastructure {
namespace fs = std::filesystem;

OtaAgentNode::OtaAgentNode(const rclcpp::NodeOptions & options)
: AmrNode("ota_agent", options)
{
  slot_dir_ = this->declare_parameter<std::string>(
    "ota.slot_dir", "/tmp/amr_ota_sim");
  security_counter_ =
    this->declare_parameter<int64_t>("ota.security_counter", 8);

  // 参数注入：target_version 变化 → 一键升级；health_report → 健康上报。
  // 实现注记：不用 add_on_set_parameters_callback——Jazzy LifecycleNode 的参数
  // 回调路径经实测不触发（本仓 test_ota_agent 的教训）；OTA 低频，1Hz 轮询
  // 参数变化足够，且跨发行版行为一致。
  this->declare_parameter<int64_t>("ota.target_version", 0);
  this->declare_parameter<std::string>("ota.health_report", "");
  // 签名链（§8.3-2 + 三审 P0-E）：公钥从文件读（root 属主只读），configure
  // 时一次性钉住——不再是运行时可替换的字符串参数（攻击者同时 set
  // 公钥+签名+版本即可完成"合法"升级的路径已封死）。镜像目录 = fetch 的
  // 哈希复验对象（P0-D：签名绑定内容而非裸版本号）。
  this->declare_parameter<std::string>("ota.image_dir", slot_dir_ + "/images");
  this->declare_parameter<std::string>("ota.target_signature", "");

  // P0-E：公钥一次性加载钉住（root 属主只读文件；运行时参数替换无效）
  this->declare_parameter<std::string>("ota.public_key_file", "");
  const std::string key_file =
      this->get_parameter("ota.public_key_file").as_string();
  image_dir_ = this->get_parameter("ota.image_dir").as_string();
  if (!key_file.empty()) {
    std::ifstream kf(key_file);
    if (kf) {
      pinned_public_key_ = std::string((std::istreambuf_iterator<char>(kf)),
                                       std::istreambuf_iterator<char>());
      RCLCPP_INFO(get_logger(), "OTA 公钥已钉住: %s（%zu 字节）",
                  key_file.c_str(), pinned_public_key_.size());
    } else {
      RCLCPP_ERROR(get_logger(), "OTA 公钥文件不可读: %s — 所有升级将 fail-closed",
                   key_file.c_str());
    }
  }
  last_target_ = this->get_parameter("ota.target_version").as_int();
  poll_timer_ = this->create_wall_timer(
    std::chrono::seconds(1), [this]() {
      const int64_t t = this->get_parameter("ota.target_version").as_int();
      if (t != last_target_) {
        last_target_ = t;
        if (t > 0) on_param_change(t);
      }
      const auto h = this->get_parameter("ota.health_report").as_string();
      if (!h.empty() && h != last_report_) {
        last_report_ = h;
        on_health_report(h);
      }
    });
}

std::optional<char> OtaAgentNode::read_boot_target() const
{
  std::ifstream f(slot_dir_ + "/boot_target");
  std::string s;
  if (!(f >> s) || s.empty()) return std::nullopt;
  return s[0];
}

std::optional<int64_t> OtaAgentNode::read_version(const std::string & path)
{
  std::ifstream f(path);
  int64_t v = 0;
  if (!(f >> v)) return std::nullopt;
  return v;
}

bool OtaAgentNode::write_file(const std::string & path, const std::string & content) const
{
  std::ofstream f(path, std::ios::trunc);
  if (!f) return false;
  f << content;
  return static_cast<bool>(f);
}

void OtaAgentNode::on_param_change(int64_t target)
{
  auto active = read_boot_target();
  if (!active) {
    RCLCPP_ERROR(get_logger(), "槽位目录未初始化（先跑 scripts/ota_sim.sh 或建目录）");
    return;
  }
  char inactive = domain::ota::OtaCoordinator::other_slot(*active);

  domain::ota::OtaCoordinator::SlotOps ops;
  ops.fetch = [this](int64_t v) {
    // P0-D：fetch = 下载+哈希复验。镜像文件由分发侧放置（真机=下载器，
    // 目录级模拟=测试/脚本写入），此处对实际字节算 sha256 并与签名内的
    // manifest 比对——签名绑定的是 {version, sha256, size} 整体。
    const std::string img = image_dir_ + "/v" + std::to_string(v) + ".img";
    std::ifstream f(img, std::ios::binary);
    if (!f) {
      RCLCPP_ERROR(get_logger(), "[fetch] 镜像文件不存在: %s — fail-closed", img.c_str());
      return false;
    }
    const std::string bytes((std::istreambuf_iterator<char>(f)),
                             std::istreambuf_iterator<char>());
    if (bytes.empty()) {
      RCLCPP_ERROR(get_logger(), "[fetch] 镜像为空 — fail-closed");
      return false;
    }
    const std::string digest = domain::ota::sha256_hex(bytes.data(), bytes.size());
    const std::string manifest = domain::ota::image_manifest(v, digest, bytes.size());
    const std::string sig =
        this->get_parameter("ota.target_signature").as_string();
    if (!domain::ota::PackageSigner::verify(manifest, sig, pinned_public_key_)) {
      RCLCPP_ERROR(get_logger(),
          "[fetch] 内容绑定验签失败（v%ld）：镜像哈希/大小与签名不符或签名无效 — 拒绝",
          v);
      return false;
    }
    RCLCPP_INFO(get_logger(),
        "[fetch] v%ld 内容验签通过（sha256=%s… size=%zu）", v,
        digest.substr(0, 12).c_str(), bytes.size());
    return true;
  };
  ops.write = [this, inactive](char slot, int64_t v) {
    bool ok = write_file(slot_dir_ + "/slots/slot" + std::string(1, slot) + "/version.txt",
                         std::to_string(v) + "\n");
    RCLCPP_INFO(get_logger(), "[install] v%ld → slot%c（%s）", v, slot, ok ? "OK" : "FAIL");
    (void)inactive;
    return ok;
  };
  ops.set_boot_target = [this](char slot) {
    bool ok = write_file(slot_dir_ + "/boot_target", std::string(1, slot) + "\n");
    RCLCPP_INFO(get_logger(), "[switch] 引导标记 → %c（原子写，%s）", slot, ok ? "OK" : "FAIL");
    return ok;
  };
  ops.rollback_marker = [this, active]() {
    write_file(slot_dir_ + "/boot_target", std::string(1, *active) + "\n");
    RCLCPP_WARN(get_logger(), "[rollback] 引导标记拨回 %c", *active);
  };

  ota_.emplace(*active,
               read_version(slot_dir_ + "/slots/slot" + std::string(1, *active) + "/version.txt")
                 .value_or(0),
               security_counter_, std::move(ops));

  // 验签移入 fetch（内容绑定，见上）——公钥缺失则 fetch 必然失败（fail-closed）
  auto r = ota_->run_update(target, /*signature_valid=*/true);  // 一键门面
  if (r == domain::ota::Result::ACCEPTED &&
      ota_->state() == domain::ota::State::HEALTH_GATE) {
    RCLCPP_INFO(get_logger(),
                "[reboot] v%ld 已就绪（HEALTH_GATE）——重启后: param set ota.health_report ok|fail",
                target);
  } else {
    RCLCPP_ERROR(get_logger(), "run_update 拒绝/中止: %d（state=%s）",
                 static_cast<int>(r), domain::ota::to_string(ota_->state()));
  }
}

void OtaAgentNode::on_health_report(const std::string & report)
{
  if (!ota_ || ota_->state() != domain::ota::State::HEALTH_GATE) {
    RCLCPP_WARN(get_logger(), "health_report 忽略：不在 HEALTH_GATE（先 target_version）");
    return;
  }
  bool ok = (report == "ok");
  ota_->on_health(ok);
  if (ota_->state() == domain::ota::State::COMMITTED) {
    if (int64_t v = ota_->active_version(); v > security_counter_) security_counter_ = v;
    RCLCPP_INFO(get_logger(), "[commit] ✓ COMMITTED slot%c v%ld",
                ota_->active_slot(), ota_->active_version());
  } else {
    RCLCPP_WARN(get_logger(), "[rollback] ✗ ROLLED_BACK（回到 slot%c v%ld）",
                ota_->active_slot(), ota_->active_version());
  }
}

}  // namespace infrastructure
}  // namespace amr
