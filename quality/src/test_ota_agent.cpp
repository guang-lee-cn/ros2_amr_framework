// OTA 一键代理集成测试：进程内参数注入 → 门面 → 槽位落盘 → 健康上报 → 防降级
#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

#include <rclcpp/rclcpp.hpp>
#include "ros2_robot_middleware/infrastructure/ota_agent_node.hpp"
#include "ros2_robot_middleware/domain/ota/package_signer.hpp"

namespace fs = std::filesystem;

namespace {
struct SlotEnv {
  std::string dir;
  explicit SlotEnv(const std::string & tag)
  {
    dir = "/tmp/amr_ota_test_" + tag;
    fs::remove_all(dir);
    fs::create_directories(dir + "/slots/slotA");
    fs::create_directories(dir + "/slots/slotB");
    put(dir + "/slots/slotA/version.txt", "10\n");
    put(dir + "/slots/slotB/version.txt", "9\n");
    put(dir + "/boot_target", "A\n");
    fs::create_directories(dir + "/images");
  }
  ~SlotEnv() { fs::remove_all(dir); }
  static void put(const std::string & p, const std::string & v)
  {
    std::ofstream f(p, std::ios::trunc);
    f << v;
  }
  static std::string get(const std::string & p)
  {
    std::ifstream f(p);
    std::string s;
    std::getline(f, s);
    return s;
  }
};
}  // namespace


namespace {
/// 等参数轮询 tick（1Hz）触发并处理完
void spin_brief(const std::shared_ptr<amr::infrastructure::OtaAgentNode> & n, int ms = 2200)
{
  auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
  while (std::chrono::steady_clock::now() < deadline) {
    rclcpp::spin_some(n->get_node_base_interface());
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
}
}  // namespace

class OtaAgentTest : public ::testing::Test {
protected:
  static void SetUpTestSuite() { rclcpp::init(0, nullptr); }
  static void TearDownTestSuite() { rclcpp::shutdown(); }

  // 签名链测试基建：每例独立密钥对（模拟设备烧公钥 + 交付侧签发）
  std::string priv_, pub_, pub_file_;
  void SetUp() override
  {
    ASSERT_TRUE(amr::domain::ota::PackageSigner::generate_keypair(priv_, pub_));
    // P0-E：公钥走文件（root 属主只读的模拟）
    pub_file_ = "/tmp/amr_ota_test_pub_" +
                std::to_string(reinterpret_cast<uintptr_t>(this)) + ".pem";
    { std::ofstream f(pub_file_, std::ios::trunc); f << pub_; }
  }
  void TearDown() override { std::remove(pub_file_.c_str()); }
  /// 放置镜像 + 内容绑定签名（P0-D：签 {version, sha256, size} 整体）
  void set_signed_target(const std::shared_ptr<amr::infrastructure::OtaAgentNode> & n,
                         int64_t version, const std::string & dir,
                         const std::string & private_key,
                         const std::string & image_content = "FAKE-IMAGE-BYTES")
  {
    const std::string img = dir + "/images/v" + std::to_string(version) + ".img";
    { std::ofstream f(img, std::ios::trunc | std::ios::binary); f << image_content; }
    const auto sig = amr::domain::ota::PackageSigner::sign(
        amr::domain::ota::image_manifest(
            version,
            amr::domain::ota::sha256_hex(image_content.data(), image_content.size()),
            image_content.size()),
        private_key);
    ASSERT_FALSE(sig.empty());
    ASSERT_TRUE(n->set_parameter(rclcpp::Parameter("ota.target_signature", sig)).successful);
    ASSERT_TRUE(n->set_parameter(rclcpp::Parameter("ota.target_version", version)).successful);
  }
};

TEST_F(OtaAgentTest, OneKeyUpgradeCommitsAndFlipsSlot) {
  SlotEnv env("commit");
  auto node = std::make_shared<amr::infrastructure::OtaAgentNode>(
    rclcpp::NodeOptions()
      .append_parameter_override("ota.slot_dir", env.dir)
      .append_parameter_override("ota.security_counter", 8)
      .append_parameter_override("ota.public_key_file", pub_file_));

  // 一键升级：签名+版本注入 → HEALTH_GATE（标记已切、版本已写非活动槽）
  set_signed_target(node, 12, env.dir, priv_);
  spin_brief(node);
  EXPECT_EQ(SlotEnv::get(env.dir + "/boot_target"), "B");        // 引导标记翻转
  EXPECT_EQ(SlotEnv::get(env.dir + "/slots/slotB/version.txt"), "12");  // 只写非活动槽

  // 重启后健康上报：提交
  rclcpp::Parameter health("ota.health_report", std::string("ok"));
  ASSERT_TRUE(node->set_parameter(health).successful);
  spin_brief(node);
  // 提交后安全计数器=12 → 降级请求应被拒：槽位不再变化
  set_signed_target(node, 5, env.dir, priv_);  // 合法签名——防降级由安全计数器拦截
  spin_brief(node);
  EXPECT_EQ(SlotEnv::get(env.dir + "/boot_target"), "B");        // 未被触碰
  EXPECT_EQ(SlotEnv::get(env.dir + "/slots/slotB/version.txt"), "12");
}

TEST_F(OtaAgentTest, HealthFailRollsBack) {
  SlotEnv env("rollback");
  auto node = std::make_shared<amr::infrastructure::OtaAgentNode>(
    rclcpp::NodeOptions()
      .append_parameter_override("ota.slot_dir", env.dir)
      .append_parameter_override("ota.security_counter", 8)
      .append_parameter_override("ota.public_key_file", pub_file_));

  set_signed_target(node, 11, env.dir, priv_);
  spin_brief(node);
  EXPECT_EQ(SlotEnv::get(env.dir + "/boot_target"), "B");        // 已切到候选
  ASSERT_TRUE(node->set_parameter(
    rclcpp::Parameter("ota.health_report", std::string("fail"))).successful);
  spin_brief(node);
  EXPECT_EQ(SlotEnv::get(env.dir + "/boot_target"), "A");        // 回滚：标记拨回旧槽
}


// ── §8.3-2：坏签名/缺签名 → fail-closed 拒绝，槽位不被触碰 ──────────────
TEST_F(OtaAgentTest, BadSignature_RejectedWithoutTouchingSlots) {
  SlotEnv env("badsig");
  auto node = std::make_shared<amr::infrastructure::OtaAgentNode>(
    rclcpp::NodeOptions()
      .append_parameter_override("ota.slot_dir", env.dir)
      .append_parameter_override("ota.security_counter", 8)
      .append_parameter_override("ota.public_key_file", pub_file_));

  // 坏签名（正确格式但不是本设备公钥对应的私钥所签）
  std::string other_priv, other_pub;
  ASSERT_TRUE(amr::domain::ota::PackageSigner::generate_keypair(other_priv, other_pub));
  { std::ofstream f(env.dir + "/images/v12.img", std::ios::trunc | std::ios::binary);
    f << "FAKE-IMAGE-BYTES"; }
  const std::string img_bytes = "FAKE-IMAGE-BYTES";
  const auto forged = amr::domain::ota::PackageSigner::sign(
      amr::domain::ota::image_manifest(
          12, amr::domain::ota::sha256_hex(img_bytes.data(), img_bytes.size()),
          img_bytes.size()),
      other_priv);
  ASSERT_TRUE(node->set_parameter(rclcpp::Parameter("ota.target_signature", forged)).successful);
  ASSERT_TRUE(node->set_parameter(rclcpp::Parameter("ota.target_version", 12)).successful);
  spin_brief(node);
  EXPECT_EQ(SlotEnv::get(env.dir + "/boot_target"), "A");            // 未切
  EXPECT_EQ(SlotEnv::get(env.dir + "/slots/slotB/version.txt"), "9");  // 未写

  // 缺签名（空）同样拒绝——设备不接受无签名包
  ASSERT_TRUE(node->set_parameter(rclcpp::Parameter("ota.target_signature", std::string(""))).successful);
  ASSERT_TRUE(node->set_parameter(rclcpp::Parameter("ota.target_version", 13)).successful);
  spin_brief(node);
  EXPECT_EQ(SlotEnv::get(env.dir + "/boot_target"), "A");            // 仍未切
  EXPECT_EQ(SlotEnv::get(env.dir + "/slots/slotB/version.txt"), "9");
}

TEST_F(OtaAgentTest, MissingPublicKey_EverythingRejected) {
  SlotEnv env("nokey");
  auto node = std::make_shared<amr::infrastructure::OtaAgentNode>(
    rclcpp::NodeOptions()
      .append_parameter_override("ota.slot_dir", env.dir)
      .append_parameter_override("ota.security_counter", 8));  // 未烧公钥
  set_signed_target(node, 12, env.dir, priv_);  // 签名本身合法也没用
  spin_brief(node);
  EXPECT_EQ(SlotEnv::get(env.dir + "/boot_target"), "A");
  EXPECT_EQ(SlotEnv::get(env.dir + "/slots/slotB/version.txt"), "9");
}

// ── 三审 P0-D：镜像内容篡改 → 内容绑定验签拒绝 ──────────────────────────
// 版本号不变、签名合法，但镜像字节被换——旧实现（签裸版本号）放行，
// 新实现对实际字节算 sha256 与签名比对，必须拒绝。
TEST_F(OtaAgentTest, TamperedImageContent_RejectedDespiteValidVersionSig) {
  SlotEnv env("tamper");
  auto node = std::make_shared<amr::infrastructure::OtaAgentNode>(
    rclcpp::NodeOptions()
      .append_parameter_override("ota.slot_dir", env.dir)
      .append_parameter_override("ota.security_counter", 8)
      .append_parameter_override("ota.public_key_file", pub_file_));

  // 签发针对内容 A，实际放置内容 B（同版本号）
  set_signed_target(node, 12, env.dir, priv_, "SIGNED-CONTENT-A");
  { std::ofstream f(env.dir + "/images/v12.img", std::ios::trunc | std::ios::binary);
    f << "TAMPERED-CONTENT-B"; }
  spin_brief(node);
  EXPECT_EQ(SlotEnv::get(env.dir + "/boot_target"), "A");              // 未切
  EXPECT_EQ(SlotEnv::get(env.dir + "/slots/slotB/version.txt"), "9");  // 未写
}
