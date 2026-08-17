#ifndef ROS2_ROBOT_MIDDLEWARE_DOMAIN_GRID_UPDATER_HPP_
#define ROS2_ROBOT_MIDDLEWARE_DOMAIN_GRID_UPDATER_HPP_

/// @file   grid_updater.hpp
/// @brief  把感知障碍标到 uint8 代价场（对标 NAV2 InflationLayer）。
///
/// 旧版：质心打 0.30m 实心圆盘（二值）—— box 对角 0.71m > 圆盘 0.60m 角空，
///       且无梯度 A* 贴边。新版：质心 LETHAL + 周围指数衰减 inflation
///       cost(d)=253·exp(-cost_scaling_factor·(d-inscribed_radius))。

#include "ros2_robot_middleware/domain/planning/astar_planner.hpp"
#include "ros2_robot_middleware/domain/planning/target_selector.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace amr {
namespace domain {
namespace planning {

class GridUpdater {
public:
  struct Params {
    // 车体外接圆（物理，默认 0.35）。decision_node 显式用 0.55/0.75：guard 用 lidar
    // (车前0.25)测距、A* 用车中心规划，参考点差 lidar_offset，inscribed≥stop_dist(0.30)
    // +lidar_offset(0.25)=0.55 才能让 A* 放行的 path 不被 guard 拦（见 decision_node.hpp）。
    float inscribed_radius = 0.35F;
    float inflation_radius = 0.55F;     // 膨胀半径（m）
    float cost_scaling_factor = 3.0F;   // 指数衰减陡度（越大越敢贴）
  };

  GridUpdater() = default;
  explicit GridUpdater(const Params &p) : params_(p) {}

  /// 标障碍（质心）。`skip_idx` 为目标索引（排除）。
  void mark_obstacles(OccupancyGrid &grid,
                      const PerceivedObject *obstacles, std::size_t count,
                      std::size_t skip_idx) const {
    for (std::size_t i = 0; i < count; ++i) {
      if (i == skip_idx) continue;
      inflate(grid, obstacles[i].x, obstacles[i].y);
    }
  }

  /// 质心 LETHAL + 周围指数 inflation（NAV2 公式）。多障碍叠加取 max。
  void inflate(OccupancyGrid &grid, float wx, float wy) const {
    int cx = 0, cy = 0;
    world_to_grid(grid, wx, wy, cx, cy);
    set_cost(grid, cx, cy, OccupancyGrid::LETHAL);

    const int R = static_cast<int>(std::ceil(params_.inflation_radius / grid.resolution));
    for (int dy = -R; dy <= R; ++dy) {
      for (int dx = -R; dx <= R; ++dx) {
        if (dx == 0 && dy == 0) continue;  // 质心已 LETHAL
        const float d_cells = std::sqrt(static_cast<float>(dx * dx + dy * dy));
        const float d_m = d_cells * grid.resolution;
        if (d_m > params_.inflation_radius) continue;
        const uint8_t cost = (d_m <= params_.inscribed_radius)
            ? OccupancyGrid::INSCRIBED
            : static_cast<uint8_t>(253.0F * std::exp(
                  -params_.cost_scaling_factor * (d_m - params_.inscribed_radius)));
        set_cost_max(grid, cx + dx, cy + dy, cost);
      }
    }
  }

  const Params &params() const { return params_; }

private:
  static void set_cost(OccupancyGrid &g, int gx, int gy, uint8_t c) {
    if (gx < 0 || gx >= g.width || gy < 0 || gy >= g.height) return;
    g.cells[static_cast<size_t>(gy) * static_cast<size_t>(g.width) + static_cast<size_t>(gx)] = c;
  }
  static void set_cost_max(OccupancyGrid &g, int gx, int gy, uint8_t c) {
    if (gx < 0 || gx >= g.width || gy < 0 || gy >= g.height) return;
    uint8_t &cell = g.cells[static_cast<size_t>(gy) * static_cast<size_t>(g.width) + static_cast<size_t>(gx)];
    cell = std::max(cell, c);
  }

  Params params_;
};

}  // namespace planning
}  // namespace domain
}  // namespace amr

#endif
