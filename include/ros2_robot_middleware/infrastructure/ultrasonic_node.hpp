#ifndef ROS2_ROBOT_MIDDLEWARE_ULTRASONIC_NODE_HPP_
#define ROS2_ROBOT_MIDDLEWARE_ULTRASONIC_NODE_HPP_

/// 超声波测距节点（D1 验收实验新增）。
/// 形态完全照 CLAUDE.md「节点形态」：继承 amr::infrastructure::AmrNode，
/// 参照实现 imu_node；参数在 on_configure 声明；传感器经 HAL 注册表创建。

// D1 验收实验产物（2026-08-26，19.5min 接入实证）转正为官方扩展范本：
// 见 docs/design/20260826-d1-extension-acceptance.md 与 ultrasonic_sensor.hpp 注册路径示例。
#include "ros2_robot_middleware/hal/sensor/isensor.hpp"
#include "ros2_robot_middleware/hal/sensor/ultrasonic_sensor.hpp"
#include "ros2_robot_middleware/infrastructure/amr_node.hpp"
#include <sensor_msgs/msg/range.hpp>

class UltrasonicNode : public amr::infrastructure::AmrNode {
public:
  UltrasonicNode();

  using CallbackReturn = rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;

  CallbackReturn on_configure(const rclcpp_lifecycle::State &);
  CallbackReturn on_activate(const rclcpp_lifecycle::State &);
  CallbackReturn on_deactivate(const rclcpp_lifecycle::State &);
  CallbackReturn on_cleanup(const rclcpp_lifecycle::State &);
  CallbackReturn on_shutdown(const rclcpp_lifecycle::State &);

private:
  void timer_callback();

  rclcpp_lifecycle::LifecyclePublisher<sensor_msgs::msg::Range>::SharedPtr publisher_;
  rclcpp::TimerBase::SharedPtr timer_;
  std::unique_ptr<amr::hal::sensor::ISensor<amr::hal::sensor::UltrasonicData>> sensor_;
  // 心跳 pub/timer 已由基类托管（start/stop_heartbeat）
};

#endif  // ROS2_ROBOT_MIDDLEWARE_ULTRASONIC_NODE_HPP_
