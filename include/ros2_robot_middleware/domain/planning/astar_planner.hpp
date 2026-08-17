#ifndef ROS2_ROBOT_MIDDLEWARE_DOMAIN_ASTAR_PLANNER_HPP_
#define ROS2_ROBOT_MIDDLEWARE_DOMAIN_ASTAR_PLANNER_HPP_

/// @file   astar_planner.hpp
/// @brief  A* over a uint8 cost field（对标 NAV2 SmacPlanner2D）.
///
/// cost-aware：步代价含 cell cost → A* 自动绕高代价区但不必远离；
/// 对角防穿角（两正交邻居得可通行）；heuristic_weight 启用（曾死代码）。
///
/// OccupancyGrid.cells: 0=FREE, 1..252=inflate 梯度, 253=INSCRIBED, 254=LETHAL.
/// is_traversable: cost < INSCRIBED（INSCRIBED/LETHAL 不可走，梯度可走但代价高）。

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <queue>
#include <unordered_set>
#include <vector>

namespace amr {
namespace domain {
namespace planning {

struct Waypoint { float x = 0.0F; float y = 0.0F; };
struct Pose { float x = 0.0F; float y = 0.0F; };

/// 占据栅格 — uint8 代价场（对标 NAV2 costmap_2d）。
struct OccupancyGrid {
  static constexpr uint8_t FREE = 0;
  static constexpr uint8_t INSCRIBED = 253;  // 机器人内切圆内（必碰）
  static constexpr uint8_t LETHAL = 254;     // 障碍本体

  int width = 0;
  int height = 0;
  float resolution = 0.05F;
  std::vector<uint8_t> cells;
  Pose origin{};

  bool is_traversable(int gx, int gy) const {
    if (gx < 0 || gx >= width || gy < 0 || gy >= height) return false;
    return cells[static_cast<size_t>(gy) * static_cast<size_t>(width)
                 + static_cast<size_t>(gx)] < INSCRIBED;
  }
  uint8_t cost_at(int gx, int gy) const {
    if (gx < 0 || gx >= width || gy < 0 || gy >= height) return LETHAL;
    return cells[static_cast<size_t>(gy) * static_cast<size_t>(width)
                 + static_cast<size_t>(gx)];
  }
};

inline void world_to_grid(const OccupancyGrid &g, float wx, float wy, int &gx, int &gy) {
  gx = static_cast<int>((wx - g.origin.x) / g.resolution);
  gy = static_cast<int>((wy - g.origin.y) / g.resolution);
}
inline Waypoint grid_to_world(const OccupancyGrid &g, int gx, int gy) {
  return {g.origin.x + (static_cast<float>(gx) + 0.5F) * g.resolution,
          g.origin.y + (static_cast<float>(gy) + 0.5F) * g.resolution};
}

class AStarPlanner {
public:
  struct Params {
    int max_iterations = 50000;
    float heuristic_weight = 1.0F;  // 1.0=A*, >1 加速（曾死代码，现启用）
    // 端点吸附：start/goal 落不可走格（stale 膨胀盘/靠泊点在膨胀圈内）时，
    // 半径内最近可走格作虚拟端点；0=关闭（端点不可走→空路径，原行为）。
    // 机台停靠后 path_pts=0 死锁修复，见 docs/design/20260817-machine2-deadlock-review.md。
    float endpoint_snap_radius = 0.0F;
  };
  AStarPlanner() = default;
  explicit AStarPlanner(const Params &p) : params_(p) {}

  std::vector<Waypoint> plan(const OccupancyGrid &grid, const Pose &start, const Pose &goal) const {
    int sx = 0, sy = 0, gx = 0, gy = 0;
    world_to_grid(grid, start.x, start.y, sx, sy);
    world_to_grid(grid, goal.x, goal.y, gx, gy);
    if (!snap_endpoint(grid, sx, sy) || !snap_endpoint(grid, gx, gy)) return {};
    if (sx == gx && sy == gy) return {grid_to_world(grid, sx, sy)};
    return search(grid, sx, sy, gx, gy);
  }

  const Params &params() const { return params_; }

private:
  struct Node {
    int x = 0, y = 0;
    float g = 0.0F;
    float h = 0.0F;  // 已乘 heuristic_weight
    int parent = -1;
    float f() const { return g + h; }
    bool operator>(const Node &o) const { return f() > o.f(); }
  };

  static constexpr int kDx[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
  static constexpr int kDy[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
  static constexpr float kCardinal = 1.0F;
  static constexpr float kDiagonal = 1.41421356F;
  /// cost 归一化系数：cell cost [0..253] → 步代价加成 [0..1]
  static constexpr float kCostScale = 1.0F / 253.0F;

  static float heuristic(int x1, int y1, int x2, int y2) {
    int dx = std::abs(x1 - x2);
    int dy = std::abs(y1 - y2);
    return static_cast<float>(std::min(dx, dy)) * kDiagonal
         + static_cast<float>(std::abs(dx - dy)) * kCardinal;
  }
  static uint64_t key(int x, int y) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32)
         | static_cast<uint64_t>(static_cast<uint32_t>(y));
  }

  /// 端点不可走时吸附到 snap 半径内欧氏最近的可走格（虚拟端点）。
  /// 半径内无可走格（真被堵，如目标在墙体深处）→ false，保留空路径语义。
  /// 圆盘窗（d²≤R²，非方形窗）+ 最近优先（环序会先命中超半径的对角格）。
  bool snap_endpoint(const OccupancyGrid &grid, int &x, int &y) const {
    if (grid.is_traversable(x, y)) return true;
    if (params_.endpoint_snap_radius <= 0.0F) return false;
    const float r_cells = params_.endpoint_snap_radius / grid.resolution;
    const float r2 = r_cells * r_cells;
    int best_d2 = std::numeric_limits<int>::max();
    int bx = 0, by = 0;
    bool found = false;
    for (int dy = -static_cast<int>(r_cells); dy <= static_cast<int>(r_cells); ++dy) {
      for (int dx = -static_cast<int>(r_cells); dx <= static_cast<int>(r_cells); ++dx) {
        if (static_cast<float>(dx * dx + dy * dy) > r2) continue;  // 圆盘界
        if (!grid.is_traversable(x + dx, y + dy)) continue;
        const int d2 = dx * dx + dy * dy;
        if (d2 < best_d2) { best_d2 = d2; bx = dx; by = dy; found = true; }
      }
    }
    if (!found) return false;
    x += bx;
    y += by;
    return true;
  }

  std::vector<Waypoint> search(const OccupancyGrid &grid, int sx, int sy, int gx, int gy) const {
    std::vector<Node> closed;
    closed.reserve(1024);
    std::unordered_set<uint64_t> visited;
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;

    open.push({sx, sy, 0.0F, heuristic(sx, sy, gx, gy) * params_.heuristic_weight, -1});

    int iter = 0;
    while (!open.empty() && iter < params_.max_iterations) {
      ++iter;
      Node cur = open.top();
      open.pop();
      const uint64_t ck = key(cur.x, cur.y);
      if (visited.count(ck)) continue;
      visited.insert(ck);
      int ci = static_cast<int>(closed.size());
      closed.push_back(cur);

      if (cur.x == gx && cur.y == gy) return reconstruct(closed, ci, grid);

      for (int i = 0; i < 8; ++i) {
        int nx = cur.x + kDx[i];
        int ny = cur.y + kDy[i];
        if (!grid.is_traversable(nx, ny)) continue;
        if (visited.count(key(nx, ny))) continue;

        const bool diag = (kDx[i] != 0 && kDy[i] != 0);
        if (diag) {
          // 防穿角：对角线的两个正交邻居都必须可通行
          if (!grid.is_traversable(cur.x + kDx[i], cur.y) ||
              !grid.is_traversable(cur.x, cur.y + kDy[i])) continue;
        }
        // cost-aware：步代价 = 距离 + cell 代价（归一化）→ 绕高代价区
        const float base = diag ? kDiagonal : kCardinal;
        const float step = base + static_cast<float>(grid.cost_at(nx, ny)) * kCostScale;
        open.push({nx, ny, cur.g + step,
                   heuristic(nx, ny, gx, gy) * params_.heuristic_weight, ci});
      }
    }
    return {};
  }

  static std::vector<Waypoint> reconstruct(const std::vector<Node> &closed, int gi,
                                            const OccupancyGrid &grid) {
    std::vector<Waypoint> path;
    int idx = gi;
    while (idx >= 0 && idx < static_cast<int>(closed.size())) {
      const auto &n = closed[idx];
      path.push_back(grid_to_world(grid, n.x, n.y));
      idx = n.parent;
    }
    std::reverse(path.begin(), path.end());
    return path;
  }

  Params params_;
};

}  // namespace planning
}  // namespace domain
}  // namespace amr

#endif
