#ifndef ROS2_ROBOT_MIDDLEWARE_DOMAIN_VFH_AVOIDANCE_HPP_
#define ROS2_ROBOT_MIDDLEWARE_DOMAIN_VFH_AVOIDANCE_HPP_

/// @file   vfh_avoidance.hpp
/// @brief  Vector Field Histogram (VFH) local avoidance — steers around
///         obstacles near the goal direction (G2-B).
///
/// Sits between PurePursuit and the collision guard in the motor loop:
///   - PurePursuit  → global-path tracking (v, w)
///   - VFH          → if an obstacle is near the goal bearing, replace w
///                    with a turn toward the nearest passable gap
///   - CollisionGuard → safety floor: clamp v, hard stop on contact
///
/// Only the angular command is modified; linear velocity is reduced while
/// turning (the guard still clamps it as the last word). VFH is stateless —
/// each steer() call recomputes from the latest scan.
///
/// Simplified single-layer VFH: sector histogram → passable gaps → choose
/// the gap closest to the goal bearing. No polar/hysteresis costmap (kept
/// minimal and unit-testable; see docs/design/20260807-g2-vfh-avoidance.md).
///
/// Pure domain logic — no ROS2.

#include "ros2_robot_middleware/domain/execution/collision_guard.hpp"  // ScanData

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace amr {
namespace domain {
namespace execution {

class VfhAvoidance {
public:
  struct Params {
    float active_range = 3.0F;      // obstacle within → steer (m)
    float passable_threshold = 0.5F; // sector passable if nearest > this (m)
    float fov_half = 0.7854F;       // intervention looks at goal ± 45°
    float max_steering = 1.5F;      // angular velocity limit (rad/s)
    float min_linear = 0.05F;       // below → stationary, do not steer (m/s)
    int bin_count = 60;             // sectors over 360° (6° each)
    int min_gap_bins = 3;           // gap needs ≥ 3 consecutive passable bins
  };

  struct SteerResult {
    float steering = 0.0F;  // angular velocity command (rad/s); 0 = no change
    bool blocked = false;   // no passable gap found (surrounded)
  };

  VfhAvoidance() = default;
  explicit VfhAvoidance(const Params &p) : params_(p) {}

  /// Clear the hysteresis gap memory (call on new goal / replan).
  void reset() { last_gap_center_ = std::numeric_limits<float>::infinity(); }

  /// Compute the avoidance steering for a scan + goal bearing (robot frame).
  /// Returns zero when no near obstacle blocks the goal direction, or when
  /// the robot is stationary. Stateful: locks onto a passable gap across
  /// calls (hysteresis) so the robot does not oscillate between two candidate
  /// gaps as the scan shifts frame to frame.
  SteerResult steer(const ScanData &scan, float goal_angle, float cmd_v) {
    // Stationary: never rotate in place from avoidance — let PurePursuit
    // handle orientation while stopped.
    if (cmd_v <= params_.min_linear) return {0.0F, false};

    // Intervention gate: only act when an obstacle is near the goal bearing.
    if (nearest_in_goal_fov(scan, goal_angle) > params_.active_range) {
      return {0.0F, false};  // path clear — keep last gap memory
    }

    // Sector histogram over the scan.
    const float bin_width = (2.0F * static_cast<float>(M_PI))
                          / static_cast<float>(params_.bin_count);
    std::vector<float> bins(static_cast<std::size_t>(params_.bin_count),
                            std::numeric_limits<float>::infinity());
    build_histogram(scan, bins, bin_width);

    const float fresh = best_gap_center(bins, bin_width, goal_angle);
    if (!std::isfinite(fresh)) {
      last_gap_center_ = std::numeric_limits<float>::infinity();
      return {0.0F, true};  // surrounded — collision guard takes over
    }

    // Hysteresis: stick to the previously chosen gap while it is still
    // passable, instead of chasing the freshly-closest one each frame —
    // the original stateless VFH oscillated between left/right gaps.
    float chosen = fresh;
    if (std::isfinite(last_gap_center_) &&
        gap_passable_at(bins, bin_width, last_gap_center_)) {
      chosen = last_gap_center_;
    } else {
      last_gap_center_ = fresh;
    }

    // Steer toward the chosen gap center (robot frame): ω = 2·θgap, clamped.
    const float steer = std::clamp(2.0F * chosen,
                                   -params_.max_steering, params_.max_steering);
    return {steer, false};
  }

  const Params &params() const { return params_; }

private:
  /// Nearest obstacle within ±fov_half of the goal bearing (robot frame).
  float nearest_in_goal_fov(const ScanData &scan, float goal_angle) const {
    float nearest = std::numeric_limits<float>::infinity();
    for (std::size_t i = 0; i < scan.ranges.size(); ++i) {
      const float r = scan.ranges[i];
      if (!std::isfinite(r) || r <= 0.0F) continue;
      const float a = scan.angle_min
                    + static_cast<float>(i) * scan.angle_increment;
      if (std::fabs(wrap(a - goal_angle)) > params_.fov_half) continue;
      nearest = std::min(nearest, r);
    }
    return nearest;
  }

  void build_histogram(const ScanData &scan,
                       std::vector<float> &bins, float bin_width) const {
    for (std::size_t i = 0; i < scan.ranges.size(); ++i) {
      const float r = scan.ranges[i];
      if (!std::isfinite(r) || r <= 0.0F) continue;
      const float a = scan.angle_min
                    + static_cast<float>(i) * scan.angle_increment;
      const float na = wrap(a);
      int bin = static_cast<int>((na + static_cast<float>(M_PI)) / bin_width);
      bin = std::clamp(bin, 0, static_cast<int>(bins.size()) - 1);
      bins[static_cast<std::size_t>(bin)] =
          std::min(bins[static_cast<std::size_t>(bin)], r);
    }
  }

  /// Passable-gap center closest to the goal bearing; +inf when no gap.
  float best_gap_center(const std::vector<float> &bins,
                        float bin_width, float goal_angle) const {
    float best_center = std::numeric_limits<float>::infinity();
    float best_score = std::numeric_limits<float>::infinity();

    std::size_t i = 0;
    const std::size_t n = bins.size();
    while (i < n) {
      if (bins[i] > params_.passable_threshold) {
        std::size_t start = i;
        std::size_t count = 0;
        while (i < n && bins[i] > params_.passable_threshold) { ++i; ++count; }
        if (count >= static_cast<std::size_t>(params_.min_gap_bins)) {
          const float center = wrap(-static_cast<float>(M_PI)
              + (static_cast<float>(start) + static_cast<float>(count) / 2.0F
                 + 0.5F) * bin_width);
          const float score = std::fabs(wrap(center - goal_angle));
          if (score < best_score) {
            best_score = score;
            best_center = center;
          }
        }
      } else {
        ++i;
      }
    }
    return best_center;
  }

  /// Is the sector at `angle` (±1 bin tolerance) still passable?
  /// Used by hysteresis to decide whether to keep the last chosen gap.
  bool gap_passable_at(const std::vector<float> &bins,
                       float bin_width, float angle) const {
    int bin = static_cast<int>((wrap(angle) + static_cast<float>(M_PI))
                               / bin_width);
    for (int off = -1; off <= 1; ++off) {
      int b = std::clamp(bin + off, 0, static_cast<int>(bins.size()) - 1);
      if (bins[static_cast<std::size_t>(b)] <= params_.passable_threshold) {
        return false;
      }
    }
    return true;
  }

  /// Wrap an angle to [-π, π].
  static float wrap(float a) {
    return std::atan2(std::sin(a), std::cos(a));
  }

  Params params_;
  float last_gap_center_ = std::numeric_limits<float>::infinity();
};

}  // namespace execution
}  // namespace domain
}  // namespace amr

#endif
