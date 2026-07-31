#ifndef ROS2_ROBOT_MIDDLEWARE_DOMAIN_GRID_UPDATER_HPP_
#define ROS2_ROBOT_MIDDLEWARE_DOMAIN_GRID_UPDATER_HPP_

/// @file   grid_updater.hpp
/// @brief  Marks perceived objects as obstacles in the occupancy grid.
///
/// Perception produces object positions; the decision layer must avoid
/// them. This updater inflates each obstacle (except the designated
/// target) onto the grid so A* plans around them.
///
/// Pure domain logic — no ROS2.

#include "ros2_robot_middleware/domain/planning/astar_planner.hpp"
#include "ros2_robot_middleware/domain/planning/target_selector.hpp"

#include <cmath>
#include <cstddef>
#include <vector>

namespace amr {
namespace domain {
namespace planning {

class GridUpdater {
public:
  struct Params {
    float inflation_radius = 0.30F;  // obstacle inflation (m) — robot footprint margin
  };

  GridUpdater() = default;
  explicit GridUpdater(const Params &p) : params_(p) {}

  /// Mark obstacles on the grid (in world coords). `target` is excluded —
  /// the navigation goal must remain reachable. `skip_idx` is the obstacle
  /// index to treat as the target (or -1 to treat all as obstacles).
  void mark_obstacles(OccupancyGrid &grid,
                      const PerceivedObject *obstacles, std::size_t count,
                      std::size_t skip_idx) const {
    for (std::size_t i = 0; i < count; ++i) {
      if (i == skip_idx) continue;
      inflate(grid, obstacles[i].x, obstacles[i].y);
    }
  }

  /// Inflate a single world-space point into occupied cells within radius.
  void inflate(OccupancyGrid &grid, float wx, float wy) const {
    const float radius_cells = params_.inflation_radius / grid.resolution;
    const int r = static_cast<int>(std::ceil(radius_cells));

    int cx = 0, cy = 0;
    world_to_grid(grid, wx, wy, cx, cy);

    const float radius2 = radius_cells * radius_cells;
    for (int dy = -r; dy <= r; ++dy) {
      for (int dx = -r; dx <= r; ++dx) {
        const float d2 = static_cast<float>(dx * dx + dy * dy);
        if (d2 > radius2) continue;
        set_occupied(grid, cx + dx, cy + dy);
      }
    }
  }

  const Params &params() const { return params_; }

private:
  static void set_occupied(OccupancyGrid &grid, int gx, int gy) {
    if (gx < 0 || gx >= grid.width || gy < 0 || gy >= grid.height) return;
    grid.cells[static_cast<size_t>(gy) * static_cast<size_t>(grid.width)
               + static_cast<size_t>(gx)] = true;
  }

  Params params_;
};

}  // namespace planning
}  // namespace domain
}  // namespace amr

#endif
