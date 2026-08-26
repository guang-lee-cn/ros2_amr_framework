// D1 验收实验产物（2026-08-26，19.5min 接入实证）转正为官方扩展范本：
// 见 docs/design/20260826-d1-extension-acceptance.md 与 ultrasonic_sensor.hpp 注册路径示例。
#include "ros2_robot_middleware/infrastructure/ultrasonic_node.hpp"

#include "ros2_robot_middleware/hal/common/registry.hpp"
#include "ros2_robot_middleware/hal/sensor/sensor_factory.hpp"
#include "ros2_robot_middleware/observability/tracer.hpp"

#include <chrono>
#include <memory>
#include <string>

UltrasonicNode::UltrasonicNode()
  : amr::infrastructure::AmrNode("ultrasonic")
{
}

UltrasonicNode::CallbackReturn
UltrasonicNode::on_configure(const rclcpp_lifecycle::State &)
{
  // 参数在 on_configure 声明（CLAUDE.md「节点形态」）
  const std::string type  = this->declare_parameter<std::string>("type", "simulated");

  // 传感器经 HAL 注册表创建（category="ultrasonic"，type 来自参数/YAML）。
  // 显式注册：静态库中的自动注册对象会被链接器丢弃（见头文件注释）。
  amr::hal::sensor::ensure_ultrasonic_registered();
  sensor_ = amr::hal::sensor::make_sensor<amr::hal::sensor::UltrasonicData>("ultrasonic", type);
  if (!sensor_) {
    // fail-fast：未注册类型直接拒绝启动（对齐 sensor_factory.hpp 的注释约定）
    RCLCPP_ERROR(this->get_logger(),
                 "ultrasonic: sensor type '%s' not registered in SensorRegistry", type.c_str());
    return CallbackReturn::FAILURE;
  }
  if (!sensor_->init()) {
    RCLCPP_ERROR(this->get_logger(), "ultrasonic: sensor init failed");
    return CallbackReturn::FAILURE;
  }

  publisher_ = create_pub<sensor_msgs::msg::Range>("/sensor/ultrasonic",
                                                   amr::qos::sensor_stream());

  return CallbackReturn::SUCCESS;
}

UltrasonicNode::CallbackReturn
UltrasonicNode::on_activate(const rclcpp_lifecycle::State &)
{
  using namespace std::chrono_literals;
  timer_ = this->create_wall_timer(50ms, [this]() { timer_callback(); });  // 20Hz

  start_heartbeat("/sensor/ultrasonic/heartbeat");
  publisher_->on_activate();

  return CallbackReturn::SUCCESS;
}

UltrasonicNode::CallbackReturn
UltrasonicNode::on_deactivate(const rclcpp_lifecycle::State &)
{
  timer_.reset();
  stop_heartbeat();

  publisher_->on_deactivate();

  return CallbackReturn::SUCCESS;
}

UltrasonicNode::CallbackReturn
UltrasonicNode::on_cleanup(const rclcpp_lifecycle::State &)
{
  publisher_.reset();
  if (sensor_) {
    sensor_->shutdown();
    sensor_.reset();
  }

  return CallbackReturn::SUCCESS;
}

UltrasonicNode::CallbackReturn
UltrasonicNode::on_shutdown(const rclcpp_lifecycle::State &)
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

void UltrasonicNode::timer_callback()
{
  TRACE_SCOPE("ultrasonic::timer_callback");

  amr::hal::sensor::UltrasonicData d;
  if (!sensor_->read(d)) {
    // 读失败（真实硬件断连等）：跳过发布，交给健康监控降级
    RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 5000,
                         "ultrasonic: read failed, frame skipped");
    return;
  }

  auto msg        = sensor_msgs::msg::Range{};
  msg.header.stamp    = this->now();  // HAL stamp_ns==0（未盖章）→ infra 读取边界补戳
  msg.header.frame_id = "ultrasonic_link";
  msg.radiation_type  = sensor_msgs::msg::Range::ULTRASOUND;
  msg.field_of_view   = 0.52F;  // ~30°
  msg.min_range       = d.min_range_m;
  msg.max_range       = d.max_range_m;
  msg.range           = d.range_m;

  publisher_->publish(msg);

  RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 5000,
                       "ultrasonic published: range=%.3f m", msg.range);
}
