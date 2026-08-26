#ifndef ROS2_ROBOT_MIDDLEWARE_TEMPERATURE_NODE_HPP_
#define ROS2_ROBOT_MIDDLEWARE_TEMPERATURE_NODE_HPP_

// D1-RUN2: 第三方开发者扩展验收实验——温度传感器节点（照 UltrasonicNode 范本）。
/// 形态完全照 CLAUDE.md「节点形态」：继承 amr::infrastructure::AmrNode，
/// 参照实现 imu_node；参数在 on_configure 声明；传感器经 HAL 注册表创建。
#include "ros2_robot_middleware/hal/sensor/isensor.hpp"
#include "ros2_robot_middleware/hal/sensor/temperature_sensor.hpp"
#include "ros2_robot_middleware/infrastructure/amr_node.hpp"
#include <sensor_msgs/msg/temperature.hpp>

class TemperatureNode : public amr::infrastructure::AmrNode {
public:
  TemperatureNode();

  using CallbackReturn = rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;

  CallbackReturn on_configure(const rclcpp_lifecycle::State &);
  CallbackReturn on_activate(const rclcpp_lifecycle::State &);
  CallbackReturn on_deactivate(const rclcpp_lifecycle::State &);
  CallbackReturn on_cleanup(const rclcpp_lifecycle::State &);
  CallbackReturn on_shutdown(const rclcpp_lifecycle::State &);

private:
  void timer_callback();

  rclcpp_lifecycle::LifecyclePublisher<sensor_msgs::msg::Temperature>::SharedPtr publisher_;
  rclcpp::TimerBase::SharedPtr timer_;
  std::unique_ptr<amr::hal::sensor::ISensor<amr::hal::sensor::TemperatureData>> sensor_;
  // 心跳 pub/timer 已由基类托管（start/stop_heartbeat）
};

#endif  // ROS2_ROBOT_MIDDLEWARE_TEMPERATURE_NODE_HPP_
