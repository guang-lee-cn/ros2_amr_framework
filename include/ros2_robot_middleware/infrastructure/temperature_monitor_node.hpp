#ifndef ROS2_ROBOT_MIDDLEWARE_TEMPERATURE_MONITOR_NODE_HPP_
#define ROS2_ROBOT_MIDDLEWARE_TEMPERATURE_MONITOR_NODE_HPP_

// D1-RUN2: 第三方开发者扩展验收实验——温度消费节点（订阅 /sensor/temperature，
// 滚动窗口均值 + 阈值告警，发布 /monitor/temperature_celsius_mean）。
/// 形态照 CLAUDE.md「节点形态」：继承 amr::infrastructure::AmrNode，参数在
/// on_configure 声明；QoS 一律 amr::qos:: 词汇表。
#include "ros2_robot_middleware/infrastructure/amr_node.hpp"
#include <sensor_msgs/msg/temperature.hpp>
#include <std_msgs/msg/float64.hpp>

#include <deque>

class TemperatureMonitorNode : public amr::infrastructure::AmrNode {
public:
  TemperatureMonitorNode();

  using CallbackReturn = rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;

  CallbackReturn on_configure(const rclcpp_lifecycle::State &);
  CallbackReturn on_activate(const rclcpp_lifecycle::State &);
  CallbackReturn on_deactivate(const rclcpp_lifecycle::State &);
  CallbackReturn on_cleanup(const rclcpp_lifecycle::State &);
  CallbackReturn on_shutdown(const rclcpp_lifecycle::State &);

private:
  void on_temperature(sensor_msgs::msg::Temperature::ConstSharedPtr msg);
  double window_mean() const;

  rclcpp::Subscription<sensor_msgs::msg::Temperature>::SharedPtr subscription_;
  rclcpp_lifecycle::LifecyclePublisher<std_msgs::msg::Float64>::SharedPtr publisher_;

  std::deque<float> window_;
  int    window_size_ = 10;
  double threshold_c_ = 60.0;
  // 心跳 pub/timer 已由基类托管（start/stop_heartbeat）
};

#endif  // ROS2_ROBOT_MIDDLEWARE_TEMPERATURE_MONITOR_NODE_HPP_
