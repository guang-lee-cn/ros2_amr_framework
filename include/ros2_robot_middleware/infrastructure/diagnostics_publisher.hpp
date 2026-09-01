#ifndef ROS2_ROBOT_MIDDLEWARE_INFRA_DIAGNOSTICS_PUBLISHER_HPP_
#define ROS2_ROBOT_MIDDLEWARE_INFRA_DIAGNOSTICS_PUBLISHER_HPP_

#include <functional>
#include <string>
#include <vector>

#include "diagnostic_msgs/msg/diagnostic_array.hpp"
#include "diagnostic_msgs/msg/diagnostic_status.hpp"
#include "rclcpp/rclcpp.hpp"
#include "ros2_robot_middleware/infrastructure/qos_profiles.hpp"

#include <rclcpp_lifecycle/lifecycle_node.hpp>

#include "ros2_robot_middleware/domain/monitoring/heartbeat_analyzer.hpp"

// Publishes ROS2 standard diagnostics (/diagnostics topic).
// Extracted from HealthMonitorNode — single responsibility.
//
// Usage:
//   DiagnosticsPublisher pub(node, []{ return statuses; });
//   pub.publish();

class DiagnosticsPublisher {
public:
  using NodeStatus = amr::domain::monitoring::NodeStatus;
  using StatusProvider = std::function<std::vector<std::pair<std::string, NodeStatus>>()>;

  DiagnosticsPublisher(rclcpp_lifecycle::LifecycleNode *node, StatusProvider provider)
    : provider_(std::move(provider)) {
    pub_ = node->create_publisher<diagnostic_msgs::msg::DiagnosticArray>(
      "/diagnostics", amr::qos::reliable_stream());
  }

  void on_activate() { pub_->on_activate(); }
  void on_deactivate() { pub_->on_deactivate(); }

  void publish(rclcpp::Time stamp) {
    auto msg = diagnostic_msgs::msg::DiagnosticArray{};
    msg.header.stamp = stamp;
    msg.header.frame_id = "health_monitor";

    for (const auto &[name, status] : provider_()) {
      auto diag = diagnostic_msgs::msg::DiagnosticStatus{};
      diag.name = name;
      diag.level = to_diag_level(status);
      diag.message = amr::domain::monitoring::to_string(status);
      msg.status.push_back(diag);
    }

    pub_->publish(msg);
  }

private:
  static uint8_t to_diag_level(NodeStatus status) {
    using amr::domain::monitoring::NodeStatus;
    switch (status) {
      case NodeStatus::OK:    return diagnostic_msgs::msg::DiagnosticStatus::OK;
      case NodeStatus::WARN:  return diagnostic_msgs::msg::DiagnosticStatus::WARN;
      case NodeStatus::ERROR: return diagnostic_msgs::msg::DiagnosticStatus::ERROR;
      case NodeStatus::STALE: return diagnostic_msgs::msg::DiagnosticStatus::STALE;
      case NodeStatus::FATAL: return diagnostic_msgs::msg::DiagnosticStatus::ERROR;
      default:                return diagnostic_msgs::msg::DiagnosticStatus::ERROR;
    }
  }

  rclcpp_lifecycle::LifecyclePublisher<diagnostic_msgs::msg::DiagnosticArray>::SharedPtr pub_;
  StatusProvider provider_;
};

#endif
