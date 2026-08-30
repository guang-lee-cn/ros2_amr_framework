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
  // 签名链（§8.3-2）：公钥烧录在设备（参数注入）；签名随版本一同送达。
  // 缺失/错误/公钥未配置 → 验证失败 → fail-closed 拒绝升级。
  this->declare_parameter<std::string>("ota.public_key_pem", "");
  this->declare_parameter<std::string>("ota.target_signature", "");
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
  ops.fetch = [this](int64_t v) {               // 演示：版本文件即时就位
    RCLCPP_INFO(get_logger(), "[fetch] 镜像 v%ld 就位", v);
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

  // 真实验签（替换 2026-08-25 审计点名的恒真桩）：任何失败 = 拒绝
  const std::string sig =
      this->get_parameter("ota.target_signature").as_string();
  const std::string pub =
      this->get_parameter("ota.public_key_pem").as_string();
  const bool signature_valid = domain::ota::PackageSigner::verify(
      domain::ota::update_manifest(target), sig, pub);
  if (signature_valid) {
    RCLCPP_INFO(get_logger(), "[verify] ed25519 签名通过（v%ld）", target);
  } else {
    RCLCPP_ERROR(get_logger(),
        "[verify] 签名验证失败（v%ld）——fail-closed 拒绝：签名缺失/错误或公钥未配置",
        target);
  }

  auto r = ota_->run_update(target, signature_valid);  // 一键门面
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
