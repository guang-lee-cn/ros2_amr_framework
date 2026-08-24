// OTA 一键代理集成测试：进程内参数注入 → 门面 → 槽位落盘 → 健康上报 → 防降级
#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

#include <rclcpp/rclcpp.hpp>
#include "ros2_robot_middleware/infrastructure/ota_agent_node.hpp"

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
};

TEST_F(OtaAgentTest, OneKeyUpgradeCommitsAndFlipsSlot) {
  SlotEnv env("commit");
  auto node = std::make_shared<amr::infrastructure::OtaAgentNode>(
    rclcpp::NodeOptions()
      .append_parameter_override("ota.slot_dir", env.dir)
      .append_parameter_override("ota.security_counter", 8));

  // 一键升级：单参数注入 → HEALTH_GATE（标记已切、版本已写非活动槽）
  rclcpp::Parameter target("ota.target_version", 12);
  ASSERT_TRUE(node->set_parameter(target).successful);
  spin_brief(node);
  EXPECT_EQ(SlotEnv::get(env.dir + "/boot_target"), "B");        // 引导标记翻转
  EXPECT_EQ(SlotEnv::get(env.dir + "/slots/slotB/version.txt"), "12");  // 只写非活动槽

  // 重启后健康上报：提交
  rclcpp::Parameter health("ota.health_report", std::string("ok"));
  ASSERT_TRUE(node->set_parameter(health).successful);
  spin_brief(node);
  // 提交后安全计数器=12 → 降级请求应被拒：槽位不再变化
  ASSERT_TRUE(node->set_parameter(rclcpp::Parameter("ota.target_version", 5)).successful);
  spin_brief(node);
  EXPECT_EQ(SlotEnv::get(env.dir + "/boot_target"), "B");        // 未被触碰
  EXPECT_EQ(SlotEnv::get(env.dir + "/slots/slotB/version.txt"), "12");
}

TEST_F(OtaAgentTest, HealthFailRollsBack) {
  SlotEnv env("rollback");
  auto node = std::make_shared<amr::infrastructure::OtaAgentNode>(
    rclcpp::NodeOptions()
      .append_parameter_override("ota.slot_dir", env.dir)
      .append_parameter_override("ota.security_counter", 8));

  ASSERT_TRUE(node->set_parameter(rclcpp::Parameter("ota.target_version", 11)).successful);
  spin_brief(node);
  EXPECT_EQ(SlotEnv::get(env.dir + "/boot_target"), "B");        // 已切到候选
  ASSERT_TRUE(node->set_parameter(
    rclcpp::Parameter("ota.health_report", std::string("fail"))).successful);
  spin_brief(node);
  EXPECT_EQ(SlotEnv::get(env.dir + "/boot_target"), "A");        // 回滚：标记拨回旧槽
}
