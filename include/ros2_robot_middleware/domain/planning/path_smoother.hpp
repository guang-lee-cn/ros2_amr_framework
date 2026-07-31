#ifndef ROS2_ROBOT_MIDDLEWARE_DOMAIN_PATH_SMOOTHER_HPP_
#define ROS2_ROBOT_MIDDLEWARE_DOMAIN_PATH_SMOOTHER_HPP_

/// @file   path_smoother.hpp
/// @brief  Corner-rounding path smoother — replaces sharp turns with arcs.
///
/// A* produces a polyline path with sharp corners. This smoother replaces
/// each corner (angle below the straightness threshold) with a circular
/// arc of radius `corner_radius`, producing a C¹-smooth path that stays
/// within the obstacle corridor. Deterministic and safe — no overshoot.
///
/// Input:  polyline path (vector<Waypoint>)
/// Output: smoothed path (dense samples, vector<Waypoint>)
///
/// Pure domain logic — no ROS2.

#include "ros2_robot_middleware/domain/planning/astar_planner.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace amr {
namespace domain {
namespace planning {

class PathSmoother {
public:
  struct Params {
    float sample_spacing = 0.05F;   // output sample spacing (m)
    float corner_radius  = 0.20F;   // arc radius at rounded corners (m)
    float straight_deg   = 175.0F;  // angle above which a corner is "straight" (no arc)
  };

  PathSmoother() = default;
  explicit PathSmoother(const Params &p) : params_(p) {}

  /// Smooth a polyline path. Returns input unchanged if < 3 points.
  std::vector<Waypoint> smooth(const std::vector<Waypoint> &path) const {
    if (path.size() < 3) return path;

    std::vector<Waypoint> out;
    out.reserve(static_cast<size_t>(path_length(path) / params_.sample_spacing) + 4);

    // First control point enters unchanged.
    out.push_back(path[0]);

    for (size_t i = 1; i < path.size(); ++i) {
      const Waypoint &prev = path[i - 1];
      const Waypoint &cur  = path[i];
      const bool has_next  = (i + 1 < path.size());
      const Waypoint &next = has_next ? path[i + 1] : path[i];

      // Dense-sample the prev→cur segment (excluding prev, up to the corner
      // rounding start when there is a turn ahead).
      const float in_angle  = std::atan2(cur.y - prev.y, cur.x - prev.x);
      const float seg_len   = dist(prev, cur);
      const float out_angle = has_next ? std::atan2(next.y - cur.y, next.x - cur.x)
                                       : in_angle;

      // Determine if we round the corner at `cur`.
      bool round = false;
      float turn = 0.0F;
      float cut_in = 0.0F;   // arc start distance from cur on incoming edge
      float cut_out = 0.0F;  // arc end distance from cur on outgoing edge
      if (has_next) {
        turn = std::abs(normalize_angle(out_angle - in_angle));
        const float corner_angle = M_PI - turn;  // 180°=straight, 90°=corner
        const float seg_out_len = dist(cur, next);
        round = (corner_angle * 180.0F / M_PI < params_.straight_deg)
             && (seg_len >= params_.corner_radius)
             && (seg_out_len >= params_.corner_radius);
        if (round) { cut_in = params_.corner_radius; cut_out = params_.corner_radius; }
      }

      // Sample incoming edge up to (seg_len - cut_in).
      const float sample_end = seg_len - cut_in;
      const int in_steps = std::max(1, static_cast<int>(sample_end / params_.sample_spacing));
      for (int s = 1; s <= in_steps; ++s) {
        const float t = std::min(1.0F, static_cast<float>(s) / static_cast<float>(in_steps));
        out.push_back({prev.x + (cur.x - prev.x) * t, prev.y + (cur.y - prev.y) * t});
      }

      if (!round) {
        continue;  // corner kept as control point (or this is the end)
      }

      // Inscribed arc: start on incoming edge, end on outgoing edge.
      const Waypoint dir_out{std::cos(out_angle), std::sin(out_angle)};
      const Waypoint arc_start{cur.x - std::cos(in_angle) * cut_in,
                               cur.y - std::sin(in_angle) * cut_in};
      const Waypoint arc_end{cur.x + dir_out.x * cut_out,
                             cur.y + dir_out.y * cut_out};

      // Inward normal: rotate dir_in by 90° in the turn direction.
      const float in_x = std::cos(in_angle), in_y = std::sin(in_angle);
      const float nx = (turn > 0.0F) ? -in_y : in_y;
      const float ny = (turn > 0.0F) ?  in_x : -in_x;
      const Waypoint center{arc_start.x + nx * params_.corner_radius,
                            arc_start.y + ny * params_.corner_radius};

      const float a_start = std::atan2(arc_start.y - center.y, arc_start.x - center.x);
      const float a_end   = std::atan2(arc_end.y   - center.y, arc_end.x   - center.x);
      float sweep = normalize_angle(a_end - a_start);
      if (sweep * turn < 0.0F) {
        sweep = (sweep > 0.0F) ? sweep - static_cast<float>(2 * M_PI)
                               : sweep + static_cast<float>(2 * M_PI);
      }
      const float arc_len = params_.corner_radius * std::abs(sweep);
      const int arc_steps = std::max(1, static_cast<int>(arc_len / params_.sample_spacing));
      for (int s = 1; s <= arc_steps; ++s) {
        const float theta = a_start + sweep * (static_cast<float>(s) / static_cast<float>(arc_steps));
        out.push_back({center.x + params_.corner_radius * std::cos(theta),
                       center.y + params_.corner_radius * std::sin(theta)});
      }
    }

    // Ensure the final waypoint is exact.
    if (out.empty() || out.back().x != path.back().x || out.back().y != path.back().y) {
      out.push_back(path.back());
    }
    return out;
  }

  const Params &params() const { return params_; }

private:
  static float normalize_angle(float a) {
    while (a > M_PI) a -= static_cast<float>(2 * M_PI);
    while (a < -M_PI) a += static_cast<float>(2 * M_PI);
    return a;
  }

  static float dist(const Waypoint &a, const Waypoint &b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
  }

  static float path_length(const std::vector<Waypoint> &path) {
    float total = 0.0F;
    for (size_t i = 1; i < path.size(); ++i) {
      total += dist(path[i - 1], path[i]);
    }
    return total;
  }

  Params params_;
};

}  // namespace planning
}  // namespace domain
}  // namespace amr

#endif
