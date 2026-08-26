// D1-RUN2: 第三方开发者扩展验收实验——温度传感器（HAL）+ TemperatureNode +
// TemperatureMonitorNode（消费端）验证。
/// 注册方式：quality/src/test_*.cpp + add_amr_test（CLAUDE.md「测试与 CI」），
/// 照 test_ultrasonic_hal.cpp 范本。
#include <gtest/gtest.h>

#include "ros2_robot_middleware/hal/sensor/sensor_factory.hpp"
#include "ros2_robot_middleware/hal/sensor/temperature_sensor.hpp"
#include "ros2_robot_middleware/infrastructure/temperature_node.hpp"
#include "ros2_robot_middleware/infrastructure/temperature_monitor_node.hpp"

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/temperature.hpp>
#include <std_msgs/msg/float64.hpp>

#include <chrono>
#include <future>
#include <memory>

using amr::hal::sensor::SimulatedTemperature;
using amr::hal::sensor::TemperatureData;

// ── HAL 层：模拟传感器本体 ────────────────────────────────────────────

TEST(TemperatureHal, InitReadShutdownLifecycle) {
  SimulatedTemperature s;
  EXPECT_TRUE(s.init());
  TemperatureData d;
  EXPECT_TRUE(s.read(d));
  EXPECT_GT(d.temperature_c, -40.0F);
  EXPECT_LT(d.temperature_c, 125.0F);
  EXPECT_EQ(d.stamp_ns, 0);  // 合成数据未盖章（打戳规范：infra 边界补）
  EXPECT_EQ(s.health(), 0);
  s.shutdown();  // 不崩溃即可
}

TEST(TemperatureHal, ReadTracksInjectedAmbient) {
  SimulatedTemperature s{40.0F};
  s.set_simulated_ambient(55.0F);
  TemperatureData d;
  for (int i = 0; i < 100; ++i) {
    ASSERT_TRUE(s.read(d));
    // 慢漂移 ±2.0C + 噪声 ±0.05C
    EXPECT_NEAR(d.temperature_c, 55.0F, 2.10F);
  }
}

TEST(TemperatureHal, RegistryCreatesSimulatedTemperature) {
  // 文档宣称的接入方式：注册表按 category+type 创建。
  // 显式调用注册函数：静态库中的静态注册对象会被链接器丢弃（D1 实测）。
  amr::hal::sensor::ensure_temperature_registered();
  auto s = amr::hal::sensor::make_sensor<TemperatureData>("temperature", "simulated");
  ASSERT_NE(s, nullptr);
  EXPECT_TRUE(s->init());
  TemperatureData d;
  EXPECT_TRUE(s->read(d));
  EXPECT_NEAR(d.temperature_c, 25.0F, 2.10F);  // 默认环境温度 25C
}

TEST(TemperatureHal, UnknownTypeReturnsNullptr) {
  // fail-fast 约定：未注册类型返回 nullptr（不允许静默回退仿真）
  auto s = amr::hal::sensor::make_sensor<TemperatureData>("temperature", "does_not_exist");
  EXPECT_EQ(s, nullptr);
}

// ── Infrastructure 层：驱动节点发布 + 消费节点闭环 ─────────────────────

class TemperatureNodeTest : public ::testing::Test {
protected:
  static void SetUpTestSuite() { rclcpp::init(0, nullptr); }
  static void TearDownTestSuite() { rclcpp::shutdown(); }
};

TEST_F(TemperatureNodeTest, ConfigureActivatePublishesTemperature) {
  auto node = std::make_shared<TemperatureNode>();

  ASSERT_EQ(std::string(node->configure().label()), "inactive");
  ASSERT_EQ(std::string(node->activate().label()), "active");

  rclcpp::executors::SingleThreadedExecutor exe;
  exe.add_node(node->get_node_base_interface());

  std::promise<sensor_msgs::msg::Temperature> got;
  std::future<sensor_msgs::msg::Temperature> fut = got.get_future();
  auto sub = rclcpp::create_subscription<sensor_msgs::msg::Temperature>(
      node, "/sensor/temperature", amr::qos::sensor_stream(),
      [&](sensor_msgs::msg::Temperature::ConstSharedPtr msg) {
        got.set_value(*msg);
      });
  exe.spin_until_future_complete(fut, std::chrono::seconds(5));

  ASSERT_EQ(fut.wait_for(std::chrono::seconds(0)), std::future_status::ready);
  const auto &msg = fut.get();
  EXPECT_NEAR(msg.temperature, 25.0F, 5.0F);   // 默认环境温度 25C ± 漂移
  EXPECT_GE(msg.variance, 0.0F);
  EXPECT_GT(msg.header.stamp.nanosec, 0u);     // infra 边界已补戳

  node->deactivate();
  node->cleanup();
}

TEST_F(TemperatureNodeTest, MonitorConsumesAndPublishesMean) {
  // 端到端链路：TemperatureNode --/sensor/temperature--> TemperatureMonitorNode
  //            --/monitor/temperature_celsius_mean--> 断言（滚动均值）
  auto driver  = std::make_shared<TemperatureNode>();
  auto monitor = std::make_shared<TemperatureMonitorNode>();

  ASSERT_EQ(std::string(driver->configure().label()), "inactive");
  ASSERT_EQ(std::string(driver->activate().label()), "active");
  ASSERT_EQ(std::string(monitor->configure().label()), "inactive");
  ASSERT_EQ(std::string(monitor->activate().label()), "active");

  rclcpp::executors::SingleThreadedExecutor exe;
  exe.add_node(driver->get_node_base_interface());
  exe.add_node(monitor->get_node_base_interface());

  std::promise<std_msgs::msg::Float64> got;
  std::future<std_msgs::msg::Float64> fut = got.get_future();
  auto sub = rclcpp::create_subscription<std_msgs::msg::Float64>(
      monitor, "/monitor/temperature_celsius_mean", amr::qos::reliable_stream(),
      [&](std_msgs::msg::Float64::ConstSharedPtr msg) {
        got.set_value(*msg);
      });
  exe.spin_until_future_complete(fut, std::chrono::seconds(10));

  ASSERT_EQ(fut.wait_for(std::chrono::seconds(0)), std::future_status::ready);
  const auto &mean = fut.get();
  // 环境温度 25C ± 慢漂移 2C：均值应落在 (20, 30) 内
  EXPECT_GT(mean.data, 20.0);
  EXPECT_LT(mean.data, 30.0);

  driver->deactivate();
  driver->cleanup();
  monitor->deactivate();
  monitor->cleanup();
}
