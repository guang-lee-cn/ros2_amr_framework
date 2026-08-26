/// 超声波传感器（HAL）+ UltrasonicNode（AmrNode 形态）验证
/// 注册方式：quality/src/test_*.cpp + add_amr_test（CLAUDE.md「测试与 CI」）
// D1 验收实验产物（2026-08-26，19.5min 接入实证）转正为官方扩展范本：
// 见 docs/design/20260826-d1-extension-acceptance.md 与 ultrasonic_sensor.hpp 注册路径示例。
#include <gtest/gtest.h>

#include "ros2_robot_middleware/hal/sensor/sensor_factory.hpp"
#include "ros2_robot_middleware/hal/sensor/ultrasonic_sensor.hpp"
#include "ros2_robot_middleware/infrastructure/ultrasonic_node.hpp"

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/range.hpp>

#include <atomic>
#include <chrono>
#include <memory>

using amr::hal::sensor::SimulatedUltrasonic;
using amr::hal::sensor::UltrasonicData;

// ── HAL 层：模拟传感器本体 ────────────────────────────────────────────

TEST(UltrasonicHal, InitReadShutdownLifecycle) {
  SimulatedUltrasonic s;
  EXPECT_TRUE(s.init());
  UltrasonicData d;
  EXPECT_TRUE(s.read(d));
  EXPECT_GT(d.range_m, 0.0F);
  EXPECT_LE(d.range_m, 100.0F);
  EXPECT_EQ(d.stamp_ns, 0);  // 合成数据未盖章（打戳规范：infra 边界补）
  EXPECT_EQ(s.health(), 0);
  s.shutdown();  // 不崩溃即可
}

TEST(UltrasonicHal, ReadTracksInjectedObstacle) {
  SimulatedUltrasonic s{1.50F};
  s.set_simulated_obstacle(1.00F);
  UltrasonicData d;
  for (int i = 0; i < 100; ++i) {
    ASSERT_TRUE(s.read(d));
    // 正弦扰动 ±0.05m + 噪声 ±0.01m
    EXPECT_NEAR(d.range_m, 1.00F, 0.10F);
  }
}

TEST(UltrasonicHal, RegistryCreatesSimulatedUltrasonic) {
  // 文档宣称的接入方式：注册宏 → 注册表按 category+type 创建。
  // 显式调用注册函数：静态库中的静态注册对象会被链接器丢弃（D1 实测）。
  amr::hal::sensor::ensure_ultrasonic_registered();
  auto s = amr::hal::sensor::make_sensor<UltrasonicData>("ultrasonic", "simulated");
  ASSERT_NE(s, nullptr);
  EXPECT_TRUE(s->init());
  UltrasonicData d;
  EXPECT_TRUE(s->read(d));
  EXPECT_NEAR(d.range_m, 2.00F, 0.10F);  // 默认障碍 2m
}

TEST(UltrasonicHal, UnknownTypeReturnsNullptr) {
  // fail-fast 约定：未注册类型返回 nullptr（不允许静默回退仿真）
  auto s = amr::hal::sensor::make_sensor<UltrasonicData>("ultrasonic", "does_not_exist");
  EXPECT_EQ(s, nullptr);
}

// ── Infrastructure 层：AmrNode 形态节点发布 /sensor/ultrasonic ────────

class UltrasonicNodeTest : public ::testing::Test {
protected:
  static void SetUpTestSuite() { rclcpp::init(0, nullptr); }
  static void TearDownTestSuite() { rclcpp::shutdown(); }
};

TEST_F(UltrasonicNodeTest, ConfigureActivatePublishesRange) {
  auto node = std::make_shared<UltrasonicNode>();

  ASSERT_EQ(std::string(node->configure().label()), "inactive");
  ASSERT_EQ(std::string(node->activate().label()), "active");

  rclcpp::executors::SingleThreadedExecutor exe;
  exe.add_node(node->get_node_base_interface());

  std::promise<sensor_msgs::msg::Range> got;
  std::future<sensor_msgs::msg::Range> fut = got.get_future();
  auto sub = rclcpp::create_subscription<sensor_msgs::msg::Range>(
      node, "/sensor/ultrasonic", amr::qos::sensor_stream(),
      [&](sensor_msgs::msg::Range::ConstSharedPtr msg) {
        got.set_value(*msg);
      });
  exe.spin_until_future_complete(fut, std::chrono::seconds(5));

  ASSERT_EQ(fut.wait_for(std::chrono::seconds(0)), std::future_status::ready);
  const auto &msg = fut.get();
  EXPECT_EQ(msg.radiation_type, sensor_msgs::msg::Range::ULTRASOUND);
  EXPECT_GE(msg.range, msg.min_range);
  EXPECT_LE(msg.range, 1'000.0F);
  EXPECT_GT(msg.header.stamp.nanosec, 0u);  // infra 边界已补戳

  node->deactivate();
  node->cleanup();
}
