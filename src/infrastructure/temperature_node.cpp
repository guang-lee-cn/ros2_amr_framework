// D1-RUN2: 第三方开发者扩展验收实验——温度传感器节点（照 UltrasonicNode 范本）。
#include "ros2_robot_middleware/infrastructure/temperature_node.hpp"

#include "ros2_robot_middleware/hal/common/registry.hpp"
#include "ros2_robot_middleware/hal/sensor/sensor_factory.hpp"
#include "ros2_robot_middleware/observability/tracer.hpp"

#include <chrono>
#include <memory>
#include <string>

TemperatureNode::TemperatureNode()
  : amr::infrastructure::AmrNode("temperature")
{
}

TemperatureNode::CallbackReturn
TemperatureNode::on_configure(const rclcpp_lifecycle::State &)
{
  // 参数在 on_configure 声明（CLAUDE.md「节点形态」）
  const std::string type  = this->declare_parameter<std::string>("type", "simulated");

  // 传感器经 HAL 注册表创建（category="temperature"，type 来自参数/YAML）。
  // 显式注册：静态库中的自动注册对象会被链接器丢弃（见 registry.hpp 头注释）。
  amr::hal::sensor::ensure_temperature_registered();
  sensor_ = amr::hal::sensor::make_sensor<amr::hal::sensor::TemperatureData>("temperature", type);
  if (!sensor_) {
    // fail-fast：未注册类型直接拒绝启动（对齐 sensor_factory.hpp 的注释约定）
    RCLCPP_ERROR(this->get_logger(),
                 "temperature: sensor type '%s' not registered in SensorRegistry", type.c_str());
    return CallbackReturn::FAILURE;
  }
  if (!sensor_->init()) {
    RCLCPP_ERROR(this->get_logger(), "temperature: sensor init failed");
    return CallbackReturn::FAILURE;
  }

  publisher_ = create_pub<sensor_msgs::msg::Temperature>("/sensor/temperature",
                                                          amr::qos::sensor_stream());

  return CallbackReturn::SUCCESS;
}

TemperatureNode::CallbackReturn
TemperatureNode::on_activate(const rclcpp_lifecycle::State &)
{
  using namespace std::chrono_literals;
  timer_ = this->create_wall_timer(100ms, [this]() { timer_callback(); });  // 10Hz

  start_heartbeat("/sensor/temperature/heartbeat");
  publisher_->on_activate();

  return CallbackReturn::SUCCESS;
}

TemperatureNode::CallbackReturn
TemperatureNode::on_deactivate(const rclcpp_lifecycle::State &)
{
  timer_.reset();
  stop_heartbeat();

  publisher_->on_deactivate();

  return CallbackReturn::SUCCESS;
}

TemperatureNode::CallbackReturn
TemperatureNode::on_cleanup(const rclcpp_lifecycle::State &)
{
  publisher_.reset();
  if (sensor_) {
    sensor_->shutdown();
    sensor_.reset();
  }

  return CallbackReturn::SUCCESS;
}

TemperatureNode::CallbackReturn
TemperatureNode::on_shutdown(const rclcpp_lifecycle::State &)
{
  timer_.reset();
  stop_heartbeat();
  publisher_.reset();
  if (sensor_) {
    sensor_->shutdown();
    sensor_.reset();
  }

  return CallbackReturn::SUCCESS;
}

void TemperatureNode::timer_callback()
{
  TRACE_SCOPE("temperature::timer_callback");

  amr::hal::sensor::TemperatureData d;
  if (!sensor_->read(d)) {
    // 读失败（真实硬件断连等）：跳过发布，交给健康监控降级
    RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 5000,
                         "temperature: read failed, frame skipped");
    return;
  }

  auto msg              = sensor_msgs::msg::Temperature{};
  msg.header.stamp      = this->now();  // HAL stamp_ns==0（未盖章）→ infra 读取边界补戳
  msg.header.frame_id   = "temperature_link";
  msg.temperature       = d.temperature_c;
  msg.variance          = 0.01F;

  publisher_->publish(msg);

  RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 5000,
                       "temperature published: %.2f C", msg.temperature);
}
