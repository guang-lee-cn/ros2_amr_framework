#ifndef ROS2_ROBOT_MIDDLEWARE_DOMAIN_PURE_PURSUIT_HPP_
#define ROS2_ROBOT_MIDDLEWARE_DOMAIN_PURE_PURSUIT_HPP_

/// @file   pure_pursuit.hpp
/// @brief  Pure Pursuit path tracking — zero external dependencies.
///
/// Input:  path (vector<Waypoint>) + current Pose + lookahead distance
/// Output: Twist (linear + angular velocity)
///
/// Pure domain logic — no ROS2, simple trigonometry.

#include <cmath>
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
    float lookahead = 0.5F;       // lookahead distance (m)
    float max_linear = 0.5F;      // max linear velocity (m/s)
    float goal_tolerance = 0.1F;  // considered "arrived" within this distance (m)
  };

  PurePursuit() = default;
  explicit PurePursuit(const Params &p) : params_(p) {}

  /// Track a path from current pose. Returns zero velocities when goal reached.
  Twist2D track(const std::vector<Waypoint> &path, const Pose2D &current) const {
    if (path.empty()) return {};

    // Find lookahead point — furthest point on path within lookahead distance
    Waypoint lookahead = find_lookahead(path, current);

    float dx = lookahead.x - current.x;
    float dy = lookahead.y - current.y;
    float dist = std::sqrt(dx * dx + dy * dy);

    // Check if goal reached
    const auto &goal = path.back();
    float goal_dx = goal.x - current.x;
    float goal_dy = goal.y - current.y;
    if (std::sqrt(goal_dx * goal_dx + goal_dy * goal_dy) < params_.goal_tolerance) {
      return {0.0F, 0.0F};
    }

    // Pure pursuit curvature: ω = 2 * v * sin(α) / L
    float alpha = std::atan2(dy, dx) - current.theta;
    // Normalize alpha to [-π, π]
    alpha = std::atan2(std::sin(alpha), std::cos(alpha));

    float linear = params_.max_linear;
    float angular = 2.0F * linear * std::sin(alpha) / params_.lookahead;

    return {linear, angular};
  }

  const Params &params() const { return params_; }

private:
  Waypoint find_lookahead(const std::vector<Waypoint> &path,
                            const Pose2D &current) const {
    Waypoint best = path.back();
    float max_dist = 0.0F;

    // Find the furthest point on path that is within lookahead distance
    for (const auto &wp : path) {
      float dx = wp.x - current.x;
      float dy = wp.y - current.y;
      float dist = std::sqrt(dx * dx + dy * dy);
      if (dist <= params_.lookahead && dist > max_dist) {
        max_dist = dist;
        best = wp;
      }
    }
    // If no point within lookahead (robot is closer than lookahead to all points),
    // use the closest point on the path (the next waypoint to chase)
    if (max_dist == 0.0F) {
      float min_dist = std::numeric_limits<float>::max();
      for (const auto &wp : path) {
        float dx = wp.x - current.x;
        float dy = wp.y - current.y;
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist < min_dist) {
          min_dist = dist;
          best = wp;
        }
      }
    }
    return best;
  }

  Params params_;
};

}  // namespace execution
}  // namespace domain
}  // namespace amr

#endif
