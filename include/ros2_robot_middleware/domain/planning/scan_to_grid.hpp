#ifndef ROS2_ROBOT_MIDDLEWARE_DOMAIN_SCAN_TO_GRID_HPP_
#define ROS2_ROBOT_MIDDLEWARE_DOMAIN_SCAN_TO_GRID_HPP_

/// @file   scan_to_grid.hpp
/// @brief  把 LiDAR scan raytrace 到 uint8 代价场（对标 NAV2 ObstacleLayer）。
///
/// 每条有效射线：传感器原点→hit 点的路径标 FREE（clearing，动态清障），
/// hit 点标 LETHAL（障碍本体）。多条射线覆盖障碍表面多点 → box 本体被标
/// LETHAL（不像质心只标一个点 → box 角漏）。替代"质心 + 每帧全清"。
///
/// Pure domain logic — no ROS2.

#include "ros2_robot_middleware/domain/planning/astar_planner.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace amr {
namespace domain {
namespace planning {

class ScanToGrid {
public:
  struct Params {
    float max_range = 6.5F;  // 超出视为无效射线（不清不标）
  };

  ScanToGrid() = default;
  explicit ScanToGrid(const Params &p) : params_(p) {}

  /// Raytrace scan 到 grid。scan 为 chassis/lidar 帧极坐标；robot 为该帧在
  /// grid（map）中的位姿（x,y,theta）。theta 把 lidar 角向转到 grid 角向。
  void raytrace(OccupancyGrid &grid,
                const float *ranges, std::size_t n,
                float angle_min, float angle_increment,
                float robot_x, float robot_y, float robot_theta) const {
    for (std::size_t i = 0; i < n; ++i) {
      const float r = ranges[i];
      if (!std::isfinite(r) || r <= 0.0F || r > params_.max_range) continue;
      const float a = angle_min + static_cast<float>(i) * angle_increment + robot_theta;
      const float hx = robot_x + r * std::cos(a);
      const float hy = robot_y + r * std::sin(a);
      bresenham(grid, robot_x, robot_y, hx, hy);
    }
  }

  const Params &params() const { return params_; }

private:
  /// Bresenham 直线：起点→终点路径标 FREE（clearing），终点标 LETHAL。
  /// 穿货架修复（2026-09-01）：clearing 不降级已是 LETHAL 的格——机器人
  /// 靠近料架时，近距回波 <0.35m 被 scan_filter 滤掉 → 该方向的射线
  /// 全部超 max_range → 不清不标 → 但其他方向的射线 clearing 会把料架
  /// 格清为 FREE → A* 规划穿过。障碍格一旦标记就不再被 clearing 降级。
  void bresenham(OccupancyGrid &grid,
                 float wx0, float wy0, float wx1, float wy1) const {
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    world_to_grid(grid, wx0, wy0, x0, y0);
    world_to_grid(grid, wx1, wy1, x1, y1);
    int dx = std::abs(x1 - x0);
    int dy = std::abs(y1 - y0);
    const int sx = x0 < x1 ? 1 : -1;
    const int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    int x = x0, y = y0;
    while (true) {
      if (x == x1 && y == y1) {
        set_cost(grid, x, y, OccupancyGrid::LETHAL);  // hit 点 = 障碍本体
        break;
      }
      set_free_if_not_lethal(grid, x, y);  // clearing 不降级 LETHAL
      const int e2 = 2 * err;
      if (e2 > -dy) { err -= dy; x += sx; }
      if (e2 < dx)  { err += dx; y += sy; }
    }
  }

  static void set_free_if_not_lethal(OccupancyGrid &g, int gx, int gy) {
    if (gx < 0 || gx >= g.width || gy < 0 || gy >= g.height) return;
    auto &cell = g.cells[static_cast<size_t>(gy) * static_cast<size_t>(g.width) + static_cast<size_t>(gx)];
    if (cell < OccupancyGrid::LETHAL) cell = OccupancyGrid::FREE;
  }

  static void set_cost(OccupancyGrid &g, int gx, int gy, uint8_t c) {
    if (gx < 0 || gx >= g.width || gy < 0 || gy >= g.height) return;
    g.cells[static_cast<size_t>(gy) * static_cast<size_t>(g.width) + static_cast<size_t>(gx)] = c;
  }

  Params params_;
};

}  // namespace planning
}  // namespace domain
}  // namespace amr

#endif
