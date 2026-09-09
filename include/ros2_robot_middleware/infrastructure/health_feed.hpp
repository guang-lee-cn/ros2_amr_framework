#ifndef ROS2_ROBOT_MIDDLEWARE_INFRASTRUCTURE_HEALTH_FEED_HPP_
#define ROS2_ROBOT_MIDDLEWARE_INFRASTRUCTURE_HEALTH_FEED_HPP_

/// @file   health_feed.hpp
/// @brief  /health/report 消费端（F2）——把 health_monitor 的节点级判定翻译成
///         supervisor 的子进程级信号（v2 心跳门的输入端）。
///
/// 分工（supervisor_policy.hpp 头注释自定）：health_monitor 采集心跳 + 判定健康
/// （感知层）；supervisor 据此做进程级恢复（决策层）。本类只做「映射 + 聚合」，
/// 不含判定（判定在域层 HeartbeatAnalyzer），也不做处置（处置在事件机）。
///
/// 聚合语义（一个子进程可映射多个 health 节点，如整栈 launch 进程）：
///   child 存活 = 映射的**全部**节点都 OK；任一 ERROR/FATAL 立即判挂。
///   WARN/STALE **不改变节点视图**——WARN 说明心跳仍在到（只是逼近超时），
///   STALE 多为刚起/刚注册，按挂处置会引发误杀风暴。它们只阻止「转 OK」。
///   仅在上报值翻转时回调，避免 1Hz 重复喂事件机。
///
/// 为什么必须聚合：整栈子进程若「任一 OK 即算活」，map_server 先 ACTIVE 就把整栈
/// 判成就位，之后没起来的节点报 STALE 被忽略 → 半死栈无人发现。聚合后卡在
/// STARTING，由既有 START_TIMEOUT 兜底。

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <rclcpp_lifecycle/lifecycle_node.hpp>

#include "ros2_robot_middleware/infrastructure/qos_profiles.hpp"
#include "ros2_robot_middleware/msg/health_report.hpp"

namespace amr {
namespace infrastructure {

class HealthFeed
{
public:
  using Callback = std::function<void(const std::string &, bool)>;  // (child, alive)

  /// @param node      宿主（订阅挂它名下，由同一 executor 驱动回调）
  /// @param mapping   child name → 该子进程的 health 节点名列表（空 = 不订阅）
  /// @param on_health 信号回调：(子进程名, 是否活着)，仅在翻转时调用
  HealthFeed(rclcpp_lifecycle::LifecycleNode & node,
             const std::map<std::string, std::vector<std::string>> & mapping,
             Callback on_health)
  : on_health_(std::move(on_health))
  {
    for (const auto &[child, nodes] : mapping) {
      auto &view = views_[child];
      view.nodes = nodes;
      for (const auto &n : nodes) node_to_child_[n] = child;
    }
    if (node_to_child_.empty()) return;  // 未启用心跳门：不建订阅（非 NAV2 形态零开销）
    sub_ = node.create_subscription<ros2_robot_middleware::msg::HealthReport>(
      "/health/report", amr::qos::reliable_stream(),
      [this](ros2_robot_middleware::msg::HealthReport::SharedPtr msg) {
        dispatch(*msg);
      });
  }

private:
  enum class NodeView : uint8_t { UNKNOWN, OK, BAD };

  struct ChildView {
    std::vector<std::string> nodes;
    std::map<std::string, NodeView> status;  // 只记 OK/BAD；未出现 = UNKNOWN
    bool alive = false;                      // 已上报的聚合结论
  };

  void dispatch(const ros2_robot_middleware::msg::HealthReport & msg)
  {
    std::vector<std::string> touched;
    for (const auto &st : msg.nodes) {
      const auto it = node_to_child_.find(st.node_name);
      if (it == node_to_child_.end()) continue;  // 未映射的节点（自研栈）忽略
      NodeView v;
      if (st.status == "OK") {
        v = NodeView::OK;
      } else if (st.status == "ERROR" || st.status == "FATAL") {
        v = NodeView::BAD;
      } else {
        continue;  // WARN/STALE：不改变视图
      }
      auto &view = views_[it->second];
      auto cur = view.status.find(st.node_name);
      if (cur != view.status.end() && cur->second == v) { continue; }  // 无翻转
      view.status[st.node_name] = v;
      touched.push_back(it->second);
    }
    for (const auto &child : touched) evaluate(child);
  }

  /// 全节点 OK 才算活——任一 BAD 或尚未确认即判不活。
  void evaluate(const std::string & child)
  {
    auto &view = views_[child];
    bool alive = true;
    for (const auto &n : view.nodes) {
      const auto it = view.status.find(n);
      if (it == view.status.end() || it->second != NodeView::OK) {
        alive = false;
        break;
      }
    }
    if (alive == view.alive) return;
    view.alive = alive;
    on_health_(child, alive);
  }

  std::map<std::string, std::string> node_to_child_;  // health 节点名 → 子进程名
  std::map<std::string, ChildView> views_;            // 子进程名 → 聚合视图
  Callback on_health_;
  rclcpp::Subscription<ros2_robot_middleware::msg::HealthReport>::SharedPtr sub_;
};

}  // namespace infrastructure
}  // namespace amr

#endif  // ROS2_ROBOT_MIDDLEWARE_INFRASTRUCTURE_HEALTH_FEED_HPP_
