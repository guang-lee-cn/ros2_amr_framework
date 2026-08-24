#ifndef ROS2_ROBOT_MIDDLEWARE_INFRASTRUCTURE_AMR_NODE_HPP_
#define ROS2_ROBOT_MIDDLEWARE_INFRASTRUCTURE_AMR_NODE_HPP_

/// @file   amr_node.hpp
/// @brief  AmrNode 基类——收敛件之二：吸收每个节点重复的样板代码。
///
/// 托管四类横切面（此前每个节点手写一遍）：
///   1. QoS 默认值（经 amr::qos 词汇表，禁止散设）
///   2. 心跳发布（health_monitor 消费的统一格式/话题模式）
///   3. 时间戳新鲜度门控（StampGate 注册与查询一站式）
///   4. 统一时钟源 now_ns()（打戳规范：戳从这拿，不散落 now()）
///
/// 新节点继承本类，从模板起步（见 CLAUDE.md）。

#include "ros2_robot_middleware/domain/perception/stamp_gate.hpp"
#include "ros2_robot_middleware/infrastructure/qos_profiles.hpp"
#include "ros2_robot_middleware/observability/metrics_registry.hpp"

#include <rclcpp/rclcpp.hpp>
#include <rclcpp_lifecycle/lifecycle_node.hpp>
#include <std_msgs/msg/string.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>

namespace amr {
namespace infrastructure {

class AmrNode : public rclcpp_lifecycle::LifecycleNode
{
public:
  explicit AmrNode(const std::string & name,
                   const rclcpp::NodeOptions & options = rclcpp::NodeOptions())
  : rclcpp_lifecycle::LifecycleNode(name, options)
  {
  }

  /// 域内时钟纳秒——打戳与新鲜度判断的统一时间源
  int64_t now_ns() const { return now().nanoseconds(); }

  /// 统一 publisher 工厂：不传 QoS 即挂默认词汇表
  template <typename MsgT>
  typename rclcpp_lifecycle::LifecyclePublisher<MsgT>::SharedPtr
  create_pub(const std::string & topic, rclcpp::QoS qos = amr::qos::reliable_stream())
  {
    return create_publisher<MsgT>(topic, qos);
  }

  /// 心跳托管：on_activate 里调 start，on_deactivate 里调 stop
  void start_heartbeat(const std::string & topic,
                       std::chrono::milliseconds period = std::chrono::milliseconds(1000))
  {
    hb_pub_ = create_publisher<std_msgs::msg::String>(topic, amr::qos::reliable_stream());
    hb_pub_->on_activate();
    hb_timer_ = create_wall_timer(period, [this]() {
      auto m = std_msgs::msg::String{};
      m.data = "alive";
      hb_pub_->publish(m);
    });
  }

  void stop_heartbeat()
  {
    hb_timer_.reset();
    if (hb_pub_) hb_pub_->on_deactivate();
    hb_pub_.reset();
  }

  /// 新鲜度门控托管：on_configure 注册容差，消费点一行判新旧
  void register_stale_gate(const std::string & sensor, int64_t stale_ms)
  {
    gate_.set_tolerance(sensor, stale_ms * 1000000LL);
  }

  /// true = 数据新鲜可用；false = 过期/未盖章（按打戳规范保守拒绝）
  bool fresh(const std::string & sensor, int64_t stamp_ns)
  {
    return gate_.check(sensor, stamp_ns, now_ns()) ==
           amr::domain::perception::StampGate::Verdict::OK;
  }

  amr::observability::MetricsRegistry & metrics()
  {
    return amr::observability::shared_metrics();
  }

private:
  rclcpp_lifecycle::LifecyclePublisher<std_msgs::msg::String>::SharedPtr hb_pub_;
  rclcpp::TimerBase::SharedPtr hb_timer_;
  amr::domain::perception::StampGate gate_;
};

}  // namespace infrastructure
}  // namespace amr

#endif  // ROS2_ROBOT_MIDDLEWARE_INFRASTRUCTURE_AMR_NODE_HPP_
