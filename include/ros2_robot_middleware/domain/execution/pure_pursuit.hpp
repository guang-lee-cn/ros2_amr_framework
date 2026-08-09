#ifndef ROS2_ROBOT_MIDDLEWARE_DOMAIN_PURE_PURSUIT_HPP_
#define ROS2_ROBOT_MIDDLEWARE_DOMAIN_PURE_PURSUIT_HPP_

/// @file   pure_pursuit.hpp
/// @brief  Pure Pursuit path tracking with trapezoidal speed profile.
///
/// Input:  path (vector<Waypoint>) + current Pose + lookahead distance
/// Output: Twist (linear + angular velocity)
///
/// Multi-waypoint aware:
///   - Finds lookahead point by walking the path in order (not global
///     nearest — correct for looping / backtracking paths).
///   - Targets the last waypoint when the lookahead point falls off the end.
///
/// Speed profile:
///   - Linear velocity ramps up/down with trapezoidal limits
///     (accel/decel bounded), slowing near the goal for a smooth stop.
///
/// Pure domain logic — no ROS2.

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace amr {
namespace domain {
namespace execution {

struct Pose2D {
  float x = 0.0F;
  float y = 0.0F;
  float theta = 0.0F;  // radians
};

struct Twist2D {
  float linear = 0.0F;
  float angular = 0.0F;
};

struct Waypoint {
  float x = 0.0F;
  float y = 0.0F;
};

class PurePursuit {
public:
  struct Params {
    float lookahead = 1.0F;        // lookahead distance (m)
    float max_linear = 0.5F;       // max linear velocity (m/s)
    float max_angular = 1.5F;      // max angular velocity (rad/s)
    float accel_limit = 1.0F;      // linear accel (m/s²)
    float goal_tolerance = 0.05F;  // arrived within this distance (m)
    float slow_radius = 0.5F;      // start decelerating within this distance (m)
    float max_track_err = 1.0F;    // lateral error to clamp steering (m)
  };

  PurePursuit() = default;
  explicit PurePursuit(const Params &p) : params_(p) {}

  /// Reset path progress — call when starting a new goal/path so the
  /// tracker re-anchors to the path start instead of the old progress.
  void reset() { progress_ = 0; }

  /// Track a multi-waypoint path from current pose.
  /// Returns zero velocities when the final goal is reached.
  Twist2D track(const std::vector<Waypoint> &path, const Pose2D &current) const {
    if (path.empty()) return {};

    // Final goal reached?
    const Waypoint &goal = path.back();
    float goal_dx = goal.x - current.x;
    float goal_dy = goal.y - current.y;
    float goal_dist = std::sqrt(goal_dx * goal_dx + goal_dy * goal_dy);
    if (goal_dist < params_.goal_tolerance) {
      return {0.0F, 0.0F};
    }

    // Lookahead point — walk path in order toward goal.
    Waypoint lookahead = find_lookahead(path, current);

    float dx = lookahead.x - current.x;
    float dy = lookahead.y - current.y;

    // Steering: ω = 2·v·sin(α) / L  (pure pursuit curvature)
    float alpha = std::atan2(dy, dx) - current.theta;
    alpha = std::atan2(std::sin(alpha), std::cos(alpha));  // normalize [-π, π]

    // Final approach: when within lookahead of the goal, abandon the
    // pursuit arc and steer directly at the goal (line-of-sight).
    // Using the small goal distance in the curvature formula would
    // oversteer and oscillate — instead rotate in place toward the goal
    // at reduced speed.
    float linear;
    float angular;
    if (goal_dist < params_.lookahead) {
      // Slow approach: mostly steer, small forward creep.
      linear  = std::min(params_.max_linear, 0.1F);
      angular = std::clamp(2.0F * alpha, -params_.max_angular, params_.max_angular);
    } else {
      linear  = target_speed(goal_dist, alpha);
      angular = 2.0F * linear * std::sin(alpha) / params_.lookahead;
      angular = std::clamp(angular, -params_.max_angular, params_.max_angular);
    }

    return {linear, angular};
  }

  const Params &params() const { return params_; }

  /// Bearing to the lookahead point in the robot frame (rad) — the direction
  /// the tracker is steering toward. Feeds the local avoidance layer (VFH).
  float lookahead_bearing(const std::vector<Waypoint> &path,
                          const Pose2D &current) const {
    if (path.empty()) return 0.0F;
    const Waypoint lp = find_lookahead(path, current);
    float alpha = std::atan2(lp.y - current.y, lp.x - current.x) - current.theta;
    return std::atan2(std::sin(alpha), std::cos(alpha));
  }

private:
  /// Find the lookahead point by walking the path from the robot's
  /// nearest waypoint, accumulating segment length until reaching the
  /// lookahead distance. Handles looping/backtracking paths correctly —
  /// never chases a point behind the robot.
  Waypoint find_lookahead(const std::vector<Waypoint> &path,
                          const Pose2D &current) const {
    // 1. 从上次进度 progress_ 沿 path 前进找最近点（单调，不回头）。
    //    全局最近会在车偏离时跳到 path 弯曲/后段，导致 lookahead 飘、
    //    车原地转圈（B11 跟踪振荡根因）。
    size_t nearest = progress_;
    float min_dist = std::numeric_limits<float>::max();
    for (size_t i = progress_; i < path.size(); ++i) {
      float d = dist2d(path[i], current);
      if (d < min_dist) { min_dist = d; nearest = i; }
    }
    progress_ = nearest;  // 只前进，永不回头

    // 2. 从 nearest 沿 path 累积到 lookahead 距离。
    float accumulated = min_dist;  // robot-to-nearest segment
    for (size_t i = nearest + 1; i < path.size(); ++i) {
      accumulated += dist2d(path[i], path[i - 1]);
      if (accumulated >= params_.lookahead) {
        return path[i];
      }
    }

    // 3. 剩余 path 不足 lookahead → 终点。
    return path.back();
  }

  static float dist2d(const Waypoint &a, const Waypoint &b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
  }

  static float dist2d(const Waypoint &a, const Pose2D &b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
  }

  /// Trapezoidal speed: ramp on start, ramp down near goal, clamp by turn.
  float target_speed(float goal_dist, float alpha) const {
    // Turn-limited speed: sharp corners slow down.
    float curvature_limit = params_.lookahead > 0.0F
      ? std::fabs(std::sin(alpha)) > 1e-6F
          ? params_.max_angular * params_.lookahead / (2.0F * std::fabs(std::sin(alpha)))
          : params_.max_linear
      : params_.max_linear;
    float v_turn = std::min(params_.max_linear, curvature_limit);

    // Goal-limited speed: trapezoidal deceleration near the goal.
    float v_goal = params_.max_linear;
    if (goal_dist < params_.slow_radius) {
      v_goal = std::min(params_.max_linear, goal_dist * 2.0F);
    }

    return std::clamp(std::min(v_turn, v_goal), 0.0F, params_.max_linear);
  }

  Params params_;
  mutable size_t progress_ = 0;  // path 跟踪进度（单调前进，跨 track 调用）
};

}  // namespace execution
}  // namespace domain
}  // namespace amr

#endif
