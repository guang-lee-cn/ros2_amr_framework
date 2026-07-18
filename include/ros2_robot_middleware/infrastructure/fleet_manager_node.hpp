#ifndef ROS2_ROBOT_MIDDLEWARE_FLEET_MANAGER_NODE_HPP_
#define ROS2_ROBOT_MIDDLEWARE_FLEET_MANAGER_NODE_HPP_

#include <array>
#include <memory>
#include <string>
#include <unordered_map>

#include "rclcpp/rclcpp.hpp"
#include "rclcpp_lifecycle/lifecycle_node.hpp"
#include "std_msgs/msg/string.hpp"

#include "ros2_robot_middleware/msg/health_report.hpp"

// FleetManager — 多 AMR 集群调度节点
//
// 架构角色：
// - 订阅所有 AMR 的 /health/report，维护集群健康视图
// - 发布 /fleet/status（集群汇总）和 /fleet/mission（任务分发）
// - 面试话术："Fleet Manager 是集群的控制面——它只做决策，不参与数据面。
//   每个 AMR 在独立 namespace 运行，Fleet Manager 作为全局观察者订阅所有
//   健康报告。这和 K8s control plane 的 watch 机制同构。"

class FleetManagerNode : public rclcpp_lifecycle::LifecycleNode {
public:
  FleetManagerNode();

  using CallbackReturn = rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;

  CallbackReturn on_configure(const rclcpp_lifecycle::State &);
  CallbackReturn on_activate(const rclcpp_lifecycle::State &);
  CallbackReturn on_deactivate(const rclcpp_lifecycle::State &);
  CallbackReturn on_cleanup(const rclcpp_lifecycle::State &);
  CallbackReturn on_shutdown(const rclcpp_lifecycle::State &);

private:
  // 管理 AMR 数量（可扩展配置）
  static constexpr int kMaxAmrs          = 3;
  static constexpr int kStatusIntervalS  = 2;

  void health_callback(size_t amr_index,
                       ros2_robot_middleware::msg::HealthReport::SharedPtr report);
  void fleet_status_timer();
  void heartbeat_timer();

  struct AmrConfig {
    const char *name;
    const char *health_topic;
  };
  static constexpr std::array<AmrConfig, kMaxAmrs> kAmrs{{
    {"amr1", "/amr1/health/report"},
    {"amr2", "/amr2/health/report"},
    {"amr3", "/amr3/health/report"},
  }};

  rclcpp::Subscription<ros2_robot_middleware::msg::HealthReport>::SharedPtr subs_[kMaxAmrs];
  ros2_robot_middleware::msg::HealthReport::SharedPtr last_reports_[kMaxAmrs];

  rclcpp_lifecycle::LifecyclePublisher<std_msgs::msg::String>::SharedPtr fleet_status_pub_;
  rclcpp_lifecycle::LifecyclePublisher<ros2_robot_middleware::msg::HealthReport>::SharedPtr fleet_health_pub_;
  rclcpp_lifecycle::LifecyclePublisher<std_msgs::msg::String>::SharedPtr heartbeat_pub_;

  rclcpp::TimerBase::SharedPtr status_timer_;
  rclcpp::TimerBase::SharedPtr heartbeat_timer_;
};

#endif
