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
  std::string priv_, pub_;
  void SetUp() override
  {
    ASSERT_TRUE(amr::domain::ota::PackageSigner::generate_keypair(priv_, pub_));
  }
  /// 先放签名再放目标（agent 1Hz 轮询只对 target 变化触发）
  void set_signed_target(const std::shared_ptr<amr::infrastructure::OtaAgentNode> & n,
                         int64_t version, const std::string & private_key)
  {
    const auto sig = amr::domain::ota::PackageSigner::sign(
        amr::domain::ota::update_manifest(version), private_key);
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
      .append_parameter_override("ota.public_key_pem", pub_));

  // 一键升级：签名+版本注入 → HEALTH_GATE（标记已切、版本已写非活动槽）
  set_signed_target(node, 12, priv_);
  spin_brief(node);
  EXPECT_EQ(SlotEnv::get(env.dir + "/boot_target"), "B");        // 引导标记翻转
  EXPECT_EQ(SlotEnv::get(env.dir + "/slots/slotB/version.txt"), "12");  // 只写非活动槽

  // 重启后健康上报：提交
  rclcpp::Parameter health("ota.health_report", std::string("ok"));
  ASSERT_TRUE(node->set_parameter(health).successful);
  spin_brief(node);
  // 提交后安全计数器=12 → 降级请求应被拒：槽位不再变化
  set_signed_target(node, 5, priv_);  // 合法签名——防降级由安全计数器拦截
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
      .append_parameter_override("ota.public_key_pem", pub_));

  set_signed_target(node, 11, priv_);
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
      .append_parameter_override("ota.public_key_pem", pub_));

  // 坏签名（正确格式但不是本设备公钥对应的私钥所签）
  std::string other_priv, other_pub;
  ASSERT_TRUE(amr::domain::ota::PackageSigner::generate_keypair(other_priv, other_pub));
  const auto forged = amr::domain::ota::PackageSigner::sign(
      amr::domain::ota::update_manifest(12), other_priv);
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
  set_signed_target(node, 12, priv_);  // 签名本身合法也没用
  spin_brief(node);
  EXPECT_EQ(SlotEnv::get(env.dir + "/boot_target"), "A");
  EXPECT_EQ(SlotEnv::get(env.dir + "/slots/slotB/version.txt"), "9");
}
