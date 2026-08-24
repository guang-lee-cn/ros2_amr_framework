#ifndef ROS2_ROBOT_MIDDLEWARE_IMU_NODE_HPP_
#define ROS2_ROBOT_MIDDLEWARE_IMU_NODE_HPP_

#include "ros2_robot_middleware/infrastructure/amr_node.hpp"
#include <sensor_msgs/msg/imu.hpp>

/// 参照实现：继承 AmrNode——心跳/QoS/门控/指标样板由基类托管，
/// 节点本体只剩业务逻辑（timer_callback 的数据合成）。
class ImuNode : public amr::infrastructure::AmrNode {
public:
  ImuNode();

  using CallbackReturn = rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;

  CallbackReturn on_configure(const rclcpp_lifecycle::State &);
  CallbackReturn on_activate(const rclcpp_lifecycle::State &);
  CallbackReturn on_deactivate(const rclcpp_lifecycle::State &);
  CallbackReturn on_cleanup(const rclcpp_lifecycle::State &);
  CallbackReturn on_shutdown(const rclcpp_lifecycle::State &);

private:
  void timer_callback();

  rclcpp_lifecycle::LifecyclePublisher<sensor_msgs::msg::Imu>::SharedPtr publisher_;
  rclcpp::TimerBase::SharedPtr timer_;
  // 心跳 pub/timer 已由基类托管（start/stop_heartbeat）
};

#endif  // ROS2_ROBOT_MIDDLEWARE_IMU_NODE_HPP_
