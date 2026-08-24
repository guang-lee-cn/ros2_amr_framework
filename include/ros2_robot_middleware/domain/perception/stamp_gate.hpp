#ifndef ROS2_ROBOT_MIDDLEWARE_DOMAIN_PERCEPTION_STAMP_GATE_HPP_
#define ROS2_ROBOT_MIDDLEWARE_DOMAIN_PERCEPTION_STAMP_GATE_HPP_

/// @file   stamp_gate.hpp
/// @brief  时间戳新鲜度门控——融合消费端的外推拒绝（stamp-based staleness gate）。
///
/// 解决的问题：「手里最新的」≠「现在的」。传感器卡顿/队列积压/发布端延迟时，
/// 数据还在到达，但已经过期——按发布时刻盲用等于用旧世界描述现在。
/// Gate 按「事件时刻 vs 当前时刻」判定每路传感器可用性，过期即拒绝。
///
/// 纯 Domain 实现（零 ROS 依赖）：纳秒整数、单调比较，时钟由 infra 层传入。

#include <cstdint>
#include <string>
#include <unordered_map>

namespace amr {
namespace domain {
namespace perception {

class StampGate
{
public:
  enum class Verdict : uint8_t { OK = 0, STALE = 1 };

  /// @param sensor   传感器名（如 "lidar"/"imu"/"camera"）
  /// @param stale_ns 容差：事件时刻距今超过该值判 STALE
  void set_tolerance(const std::string & sensor, int64_t stale_ns)
  {
    tolerances_[sensor] = stale_ns;
  }

  /// 判定 + 计数。stamp_ns==0 视为未盖章——按 STALE 处理（保守），
  /// 强制生产端显式打戳而不是靠默认值蒙混。
  Verdict check(const std::string & sensor, int64_t stamp_ns, int64_t now_ns)
  {
    auto it = tolerances_.find(sensor);
    if (it == tolerances_.end()) {
      ++unknown_rejects_;
      return Verdict::STALE;  // 未注册容差的传感器：拒绝（白名单语义）
    }
    if (stamp_ns == 0 || now_ns - stamp_ns > it->second) {
      ++stale_counts_[sensor];
      return Verdict::STALE;
    }
    return Verdict::OK;
  }

  uint64_t stale_count(const std::string & sensor) const
  {
    auto it = stale_counts_.find(sensor);
    return it == stale_counts_.end() ? 0 : it->second;
  }

  uint64_t unknown_rejects() const { return unknown_rejects_; }

private:
  std::unordered_map<std::string, int64_t> tolerances_;
  std::unordered_map<std::string, uint64_t> stale_counts_;
  uint64_t unknown_rejects_ = 0;
};

}  // namespace perception
}  // namespace domain
}  // namespace amr

#endif  // ROS2_ROBOT_MIDDLEWARE_DOMAIN_PERCEPTION_STAMP_GATE_HPP_
