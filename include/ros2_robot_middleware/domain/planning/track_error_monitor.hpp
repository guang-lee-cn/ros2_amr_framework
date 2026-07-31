#ifndef ROS2_ROBOT_MIDDLEWARE_DOMAIN_TRACK_ERROR_MONITOR_HPP_
#define ROS2_ROBOT_MIDDLEWARE_DOMAIN_TRACK_ERROR_MONITOR_HPP_

/// @file   track_error_monitor.hpp
/// @brief  Lateral tracking error monitor — slows/stops on path deviation.
///
/// Computes the signed lateral distance from the robot to the nearest
/// path segment. Small deviation → slow down; large → stop (safety).
/// Feeds the motor control loop a speed scale in [0,1].
///
/// Pure domain logic — no ROS2.

#include "ros2_robot_middleware/domain/execution/pure_pursuit.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace amr {
namespace domain {
namespace planning {

enum class TrackErrorLevel : uint8_t {
  OK,    // within warn_threshold — full speed
  WARN,  // beyond warn_threshold — scaled speed
  ERROR, // beyond stop_threshold — stop + replan signal
};

class TrackErrorMonitor {
public:
  struct Params {
    float warn_threshold = 0.15F;  // lateral error → slow down (m)
    float stop_threshold = 0.40F;  // lateral error → stop (m)
  };

  struct State {
    float lateral_error = 0.0F;  // signed distance to nearest segment (m)
    float speed_scale = 1.0F;    // [0,1] multiplier for cmd_vel
    TrackErrorLevel level = TrackErrorLevel::OK;
  };

  TrackErrorMonitor() = default;
  explicit TrackErrorMonitor(const Params &p) : params_(p) {}

  State evaluate(const amr::domain::execution::Pose2D &current,
                 const std::vector<amr::domain::execution::Waypoint> &path) const {
    State st;

    if (path.size() < 2) {
      st.lateral_error = 0.0F;
      st.speed_scale = 0.0F;  // no path → do not move
      st.level = TrackErrorLevel::ERROR;
      return st;
    }

    // Signed distance to the nearest path segment.
    st.lateral_error = signed_dist_to_path(current, path);

    if (std::fabs(st.lateral_error) >= params_.stop_threshold) {
      st.speed_scale = 0.0F;
      st.level = TrackErrorLevel::ERROR;
    } else if (std::fabs(st.lateral_error) >= params_.warn_threshold) {
      // Linear slowdown from warn to stop threshold.
      const float t = (std::fabs(st.lateral_error) - params_.warn_threshold)
                    / (params_.stop_threshold - params_.warn_threshold);
      st.speed_scale = std::clamp(1.0F - t, 0.0F, 1.0F);
      st.level = TrackErrorLevel::WARN;
    } else {
      st.speed_scale = 1.0F;
      st.level = TrackErrorLevel::OK;
    }
    return st;
  }

  const Params &params() const { return params_; }

private:
  /// Signed distance from point to the nearest segment.
  /// Sign: + = left of path direction, − = right.
  static float signed_dist_to_path(
      const amr::domain::execution::Pose2D &p,
      const std::vector<amr::domain::execution::Waypoint> &path) {
    float best_dist = std::numeric_limits<float>::max();
    float best_sign = 1.0F;

    for (size_t i = 1; i < path.size(); ++i) {
      const auto &a = path[i - 1];
      const auto &b = path[i];
      float dist = point_segment_dist(p.x, p.y, a.x, a.y, b.x, b.y, &best_sign);
      // Use the segment whose absolute distance is smallest.
      if (std::fabs(dist) < std::fabs(best_dist)) {
        best_dist = dist;
      }
    }
    return best_dist;
  }

  /// Distance from point (px,py) to segment A-B, signed by cross product.
  static float point_segment_dist(float px, float py,
                                  float ax, float ay, float bx, float by,
                                  float *sign_out) {
    const float abx = bx - ax, aby = by - ay;
    const float apx = px - ax, apy = py - ay;
    const float ab2 = abx * abx + aby * aby;

    float t = (ab2 > 1e-12F) ? (apx * abx + apy * aby) / ab2 : 0.0F;
    t = std::clamp(t, 0.0F, 1.0F);

    const float cx = ax + t * abx, cy = ay + t * aby;
    const float dx = px - cx, dy = py - cy;
    const float dist = std::sqrt(dx * dx + dy * dy);

    // Sign from cross product (z) of segment AB × point P.
    const float cross = abx * apy - aby * apx;
    const float sign = (cross >= 0.0F) ? 1.0F : -1.0F;
    *sign_out = sign;
    return sign * dist;
  }

  Params params_;
};

}  // namespace planning
}  // namespace domain
}  // namespace amr

#endif
