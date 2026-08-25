#ifndef ROS2_ROBOT_MIDDLEWARE_INFRASTRUCTURE_SUPERVISOR_NODE_HPP_
#define ROS2_ROBOT_MIDDLEWARE_INFRASTRUCTURE_SUPERVISOR_NODE_HPP_

/// @file   supervisor_node.hpp
/// @brief  B1 进程级监管节点——声明式配置驱动崩溃监管/依赖序重启/健康门。
///
/// 策略内核在 domain（supervisor_policy.hpp，全单测）；本层只做三件事：
///   1. 装载声明式配置（supervisor.<name>.* 参数）+ 拓扑校验（环/未知依赖拒绝启动）
///   2. posix_spawn 拉起子进程（独立进程组，组式清场防孙进程泄漏）
///   3. 250ms tick：waitpid(WNOHANG) → 喂状态机 → 执行动作（含级联让位）
///
/// 状态出口复用 HealthReport（latched_state），Phase→OK/WARN/STALE/ERROR。
/// 见 docs/design/20260825-b1-supervisor-adr.md。

#include "ros2_robot_middleware/domain/monitoring/supervisor_policy.hpp"
#include "ros2_robot_middleware/infrastructure/amr_node.hpp"
#include "ros2_robot_middleware/msg/health_report.hpp"

#include <map>
#include <string>
#include <vector>

#include <sys/types.h>

namespace amr {
namespace infrastructure {

class SupervisorNode : public AmrNode {
public:
  using CallbackReturn = rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;

  explicit SupervisorNode(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());

  CallbackReturn on_configure(const rclcpp_lifecycle::State &) override;
  CallbackReturn on_activate(const rclcpp_lifecycle::State &) override;
  CallbackReturn on_deactivate(const rclcpp_lifecycle::State &) override;
  CallbackReturn on_cleanup(const rclcpp_lifecycle::State &) override;
  CallbackReturn on_shutdown(const rclcpp_lifecycle::State &) override;

  /// 进程清场（不依赖 lifecycle 正常走完）——supervisor 退出绝不允许泄漏子进程
  void teardown_children();

private:
  struct ProcChild {
    domain::monitoring::ChildSpec spec;
    domain::monitoring::ChildState state;
    pid_t pid = -1;
    std::vector<std::string> cmd;
  };

  // 配置装载与校验
  bool load_children_from_params();
  bool deps_all_running(const ProcChild &c) const;
  std::vector<std::string> transitive_dependents(const std::string &name) const;

  // 进程操作
  bool spawn_child(ProcChild &c);
  void kill_child(ProcChild &c);
  void cascade_yield(const std::string &name);  // 依赖 name 的传递依赖者让位

  // tick 驱动
  void tick();
  void feed(ProcChild &c, domain::monitoring::Event ev);
  void try_bring_up();  // 拓扑序扫描: STOPPED 且依赖全 RUNNING 且未完成 → spawn
  void publish_status();

  std::map<std::string, ProcChild> children_;          // name → child
  std::vector<std::string> topo_;                      // 拉起序（校验通过的证明）
  std::map<std::string, bool> completed_;              // oneshot 完成标记（依赖重启时清除）

  rclcpp::TimerBase::SharedPtr tick_timer_;
  rclcpp::TimerBase::SharedPtr status_timer_;
  rclcpp_lifecycle::LifecyclePublisher<ros2_robot_middleware::msg::HealthReport>::SharedPtr status_pub_;
};

}  // namespace infrastructure
}  // namespace amr

#endif  // ROS2_ROBOT_MIDDLEWARE_INFRASTRUCTURE_SUPERVISOR_NODE_HPP_
