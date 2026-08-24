#ifndef ROS2_ROBOT_MIDDLEWARE_INFRASTRUCTURE_QOS_PROFILES_HPP_
#define ROS2_ROBOT_MIDDLEWARE_INFRASTRUCTURE_QOS_PROFILES_HPP_

/// @file   qos_profiles.hpp
/// @brief  QoS 词汇表——全仓库唯一的 QoS 合法来源（收敛件之一）。
///
/// 规则：新代码一律 `amr::qos::xxx()`，禁止手搓 `rclcpp::QoS(10)` 散设。
/// 每个 profile 的档位选择都有实测依据（benchmarks/docs/）：
///   - best_effort 对流式数据的依据：1MB 载荷 reliable 尾部比 best_effort
///     恶化 67×（重传风暴；晚到的帧=死帧，重传无价值）
///   - transient_local 的依据：durability 实测晚加入者补帧 5:0 vs volatile 0
///
/// QoS 不是配置项，是「这条数据的价值随时间怎么衰减」的声明：
///   帧死了 → sensor_stream；帧不能丢 → reliable_stream；
///   晚加入要补最后一帧 → latched_state；旧命令必须被新命令顶掉 → control_stream。

#include <rclcpp/rclcpp.hpp>

#include <cstddef>

namespace amr {
namespace qos {

/// 传感器流：丢帧无害、绝不积压（触觉/雷达/视觉原始流）
inline rclcpp::QoS sensor_stream(std::size_t depth = 5)
{
  return rclcpp::QoS(rclcpp::KeepLast(depth)).best_effort();
}

/// 命令与状态流：可靠投递、浅队列（默认原语，语义显式化）
inline rclcpp::QoS reliable_stream(std::size_t depth = 10)
{
  return rclcpp::QoS(rclcpp::KeepLast(depth)).reliable();
}

/// 关键状态：晚加入者补收最后一帧（机器人描述/静态配置类话题）
inline rclcpp::QoS latched_state()
{
  return rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local();
}

/// 控制流：可靠 + 深度 1——新命令顶掉旧命令，旧命令过期作废
inline rclcpp::QoS control_stream()
{
  return rclcpp::QoS(rclcpp::KeepLast(1)).reliable();
}

}  // namespace qos
}  // namespace amr

#endif  // ROS2_ROBOT_MIDDLEWARE_INFRASTRUCTURE_QOS_PROFILES_HPP_
