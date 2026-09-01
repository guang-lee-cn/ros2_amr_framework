#ifndef ROS2_ROBOT_MIDDLEWARE_HEALTH_MONITOR_NODE_HPP_
#define ROS2_ROBOT_MIDDLEWARE_HEALTH_MONITOR_NODE_HPP_

#include <array>
#include <memory>
#include <string>
#include <unordered_map>

#include "lifecycle_msgs/srv/change_state.hpp"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

#include "ros2_robot_middleware/domain/monitoring/monitoring_service.hpp"
#include "ros2_robot_middleware/infrastructure/diagnostics_publisher.hpp"
#include "ros2_robot_middleware/infrastructure/prometheus_http_server.hpp"
#include "ros2_robot_middleware/msg/health_report.hpp"
#include "ros2_robot_middleware/msg/health_status.hpp"
#include "ros2_robot_middleware/srv/set_param.hpp"

#include <rclcpp_lifecycle/lifecycle_node.hpp>

// HealthMonitorNode — heartbeat-based liveness monitor for 6 business nodes.
// Post-split: Prometheus HTTP server and diagnostics publishing extracted
// to standalone classes (PrometheusHttpServer, DiagnosticsPublisher).
// HealthMonitorNode keeps heartbeat orchestration + watchdog lifecycle restart.
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

  // ── P0-C 异步重启状态机（三审 2026-08-30）─────────────────────────────
  // 旧实现：回调内 wait_for_service(1s) + 4×future.wait_for(2s) 同步等待 +
  // 单线程 spin = 结构性死锁（响应永远无法被处理，每个 transition 必超时，
  // 重启从第一天起就不可能成功）。新实现零阻塞：定时器只发起第一步，
  // 响应回调链式推进，service_is_ready() 替代 wait_for_service。
  struct RestartState {
    std::string node;
    size_t step = 0;       // 0..3: deactivate→cleanup→configure→activate
    bool in_progress = false;
    double deadline = 0.0; // N-3（三审）：超时则放弃，不永久挂起
  };
  static constexpr double kRestartTimeoutS = 30.0;  // 4 步 × DDS 往返 + 余量
  void begin_restart(const std::string &node_name);
  void send_next_transition();
  void handle_transition_response(
      rclcpp::Client<lifecycle_msgs::srv::ChangeState>::SharedFuture future);
  RestartState restart_;
  rclcpp::CallbackGroup::SharedPtr restart_group_;  // 响应回调独立组（可迁移多线程）

  std::string prometheus_metrics() const;

  // Domain layer
  amr::domain::monitoring::MonitoringService monitor_;

  static constexpr int kNumNodes       = 6;
  static constexpr int kPrometheusPort = 9090;

  struct NodeConfig {
    const char *node;
    const char *topic;
    double default_timeout_s;
  };
  static constexpr std::array<NodeConfig, kNumNodes> kNdes{{
    {"lidar",      "/sensor/lidar/heartbeat",     1.5},
    {"imu",        "/sensor/imu/heartbeat",       2.0},  // heartbeat 1Hz → timeout ≥ 2×周期
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

  // Watchdog
  std::unordered_map<std::string, rclcpp::Client<lifecycle_msgs::srv::ChangeState>::SharedPtr> restart_clients_;

  // Extracted components (SRP split from HealthMonitorNode)
  std::unique_ptr<PrometheusHttpServer> prometheus_;
  std::unique_ptr<DiagnosticsPublisher> diagnostics_;

  rclcpp::Time last_tick_;
};

#endif
