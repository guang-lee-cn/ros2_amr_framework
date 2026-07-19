#ifndef ROS2_ROBOT_MIDDLEWARE_HEALTH_MONITOR_NODE_HPP_
#define ROS2_ROBOT_MIDDLEWARE_HEALTH_MONITOR_NODE_HPP_

#include <array>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>

#include "diagnostic_msgs/msg/diagnostic_array.hpp"
#include "diagnostic_msgs/msg/diagnostic_status.hpp"
#include "lifecycle_msgs/srv/change_state.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

#include "ros2_robot_middleware/application/monitoring_service.hpp"
#include "ros2_robot_middleware/msg/health_report.hpp"
#include "ros2_robot_middleware/msg/health_status.hpp"
#include "ros2_robot_middleware/srv/set_param.hpp"

#include <rclcpp_lifecycle/lifecycle_node.hpp>

// Thin ROS2 adapter — delegates heartbeat analysis + recovery to MonitoringService.
class HealthMonitorNode : public rclcpp_lifecycle::LifecycleNode {
public:
  HealthMonitorNode();

  using CallbackReturn = rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;

  CallbackReturn on_configure(const rclcpp_lifecycle::State &);
  CallbackReturn on_activate(const rclcpp_lifecycle::State &);
  CallbackReturn on_deactivate(const rclcpp_lifecycle::State &);
  CallbackReturn on_cleanup(const rclcpp_lifecycle::State &);
  CallbackReturn on_shutdown(const rclcpp_lifecycle::State &);

private:
  void declare_parameters();
  void load_parameters();
  void create_subscriptions();
  void create_report_publisher();
  void create_health_timer();
  void create_service_server();
  void check_health();

  void create_restart_clients();
  bool try_restart_sequence(const std::string &node_name);

  void setup_prometheus();
  void prometheus_accept();
  std::string prometheus_metrics() const;

  void publish_diagnostics();
  uint8_t to_diag_level(amr::domain::monitoring::NodeStatus status) const;

  // Domain layer
  amr::application::MonitoringService monitor_;

  static constexpr int kNumNodes       = 6;
  static constexpr int kPrometheusPort = 9090;

  struct NodeConfig {
    const char *node;
    const char *topic;
    double default_timeout_s;
  };
  static constexpr std::array<NodeConfig, kNumNodes> kNdes{{
    {"lidar",      "/sensor/lidar/heartbeat",     1.5},
    {"imu",        "/sensor/imu/heartbeat",       0.5},
    {"camera",     "/sensor/camera/heartbeat",    3.0},
    {"fusion",     "/sensor/fusion/heartbeat",    1.0},
    {"decision",   "/decision/heartbeat",         2.0},
    {"motor_ctrl", "/cmd/status",                 2.0},
  }};

  // ROS2 infrastructure
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr subs_[kNumNodes];
  rclcpp::TimerBase::SharedPtr timer_;
  rclcpp_lifecycle::LifecyclePublisher<ros2_robot_middleware::msg::HealthReport>::SharedPtr pub_;
  rclcpp::Service<ros2_robot_middleware::srv::SetParam>::SharedPtr health_srv_;

  std::unordered_map<std::string, double> timeouts_;
  double check_interval_s_ = 1.0;

  // Watchdog + diagnostics
  std::unordered_map<std::string, rclcpp::Client<lifecycle_msgs::srv::ChangeState>::SharedPtr> restart_clients_;
  rclcpp_lifecycle::LifecyclePublisher<diagnostic_msgs::msg::DiagnosticArray>::SharedPtr diagnostic_pub_;

  int prom_socket_ = -1;
  std::thread prom_thread_;
  rclcpp::Time last_tick_;
};

#endif
