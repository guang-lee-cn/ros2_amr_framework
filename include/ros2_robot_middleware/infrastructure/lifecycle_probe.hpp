#ifndef ROS2_ROBOT_MIDDLEWARE_INFRASTRUCTURE_LIFECYCLE_PROBE_HPP_
#define ROS2_ROBOT_MIDDLEWARE_INFRASTRUCTURE_LIFECYCLE_PROBE_HPP_

/// @file   lifecycle_probe.hpp
/// @brief  NAV2 lifecycle 节点活性探针（F2）—— stock 节点不发 AmrNode 心跳，
///         改用 lifecycle get_state 的 ACTIVE 响应作为存活信号。
///
/// 为什么不用话题活性：NAV2 server 空闲时不发业务话题（planner 无目标不发
/// /plan），用流量判挂死会误杀。lifecycle 状态是 stock NAV2 的权威健康信号，
/// 对所有 server 统一，且空闲态仍可查询。
///
/// 判定语义与既有订阅探针共用 last_seen_s 超时模型：
///   service 就绪 + 返回 ACTIVE → 上报「活着」（heartbeat_received）
///   服务不可达 / 非 ACTIVE     → 不上报（由既有 timeout 自然升级 ERROR）
/// 因此 HeartbeatAnalyzer 零改动。

#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "lifecycle_msgs/msg/state.hpp"
#include "lifecycle_msgs/srv/get_state.hpp"
#include "rclcpp/create_client.hpp"
#include "rclcpp/node_interfaces/node_base_interface.hpp"
#include "rclcpp/node_interfaces/node_graph_interface.hpp"
#include "rclcpp/node_interfaces/node_services_interface.hpp"
#include "rclcpp/rclcpp.hpp"

namespace amr {
namespace infrastructure {

class LifecycleProbe
{
public:
  using AliveCallback = std::function<void(const std::string &)>;

  /// @param node_base/node_graph/node_services 宿主节点接口——LifecycleNode 不继承
  ///        rclcpp::Node，故传接口（客户端挂宿主名下，由同一 executor 驱动回调）
  /// @param targets   目标节点名（服务名取 /<name>/get_state，与 NAV2 无命名空间形态一致）
  /// @param on_alive  判定为活时的回调（通常转 monitor_.heartbeat_received）
  LifecycleProbe(rclcpp::node_interfaces::NodeBaseInterface::SharedPtr node_base,
                 rclcpp::node_interfaces::NodeGraphInterface::SharedPtr node_graph,
                 rclcpp::node_interfaces::NodeServicesInterface::SharedPtr node_services,
                 const std::vector<std::string> & targets,
                 AliveCallback on_alive)
  : names_(targets), on_alive_(std::move(on_alive))
  {
    for (const auto & name : targets) {
      clients_.push_back(rclcpp::create_client<lifecycle_msgs::srv::GetState>(
        node_base, node_graph, node_services, "/" + name + "/get_state"));
    }
  }

  /// 轮询一次（由宿主 1Hz 调用）。服务未就绪则跳过——不堆积 future，
  /// 同时「不可达」自然表现为不上报，交给超时模型判死。
  void poll()
  {
    for (size_t i = 0; i < clients_.size(); ++i) {
      if (!clients_[i]->service_is_ready()) continue;
      auto req = std::make_shared<lifecycle_msgs::srv::GetState::Request>();
      const std::string name = names_[i];
      clients_[i]->async_send_request(
        req,
        [this, name](rclcpp::Client<lifecycle_msgs::srv::GetState>::SharedFuture f) {
          try {
            const auto resp = f.get();
            if (resp &&
                resp->current_state.id ==
                lifecycle_msgs::msg::State::PRIMARY_STATE_ACTIVE) {
              on_alive_(name);
            }
          } catch (const std::exception &) {
            // 调用异常 = 不可达，等同不上报（超时模型接手）
          }
        });
    }
  }

private:
  std::vector<rclcpp::Client<lifecycle_msgs::srv::GetState>::SharedPtr> clients_;
  std::vector<std::string> names_;
  AliveCallback on_alive_;
};

}  // namespace infrastructure
}  // namespace amr

#endif  // ROS2_ROBOT_MIDDLEWARE_INFRASTRUCTURE_LIFECYCLE_PROBE_HPP_
