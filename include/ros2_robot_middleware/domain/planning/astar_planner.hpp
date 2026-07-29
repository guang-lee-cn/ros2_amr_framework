#ifndef ROS2_ROBOT_MIDDLEWARE_DOMAIN_ASTAR_PLANNER_HPP_
#define ROS2_ROBOT_MIDDLEWARE_DOMAIN_ASTAR_PLANNER_HPP_

/// @file   astar_planner.hpp
/// @brief  A* grid-based path planner — zero external dependencies.
///
/// Input: OccupancyGrid (2D bool matrix) + start/goal Pose
/// Output: vector<Waypoint> from start to goal, empty if unreachable
///
/// Complexity: O(N log N) with binary heap, N = grid cells explored.
/// For a 100×100 grid, worst case ~10k cells, <1ms on x86.
///
/// Pure domain logic — no ROS2, no Eigen, std::priority_queue only.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <queue>
#include <unordered_set>
#include <vector>

namespace amr {
namespace domain {
namespace planning {

struct Waypoint {
  float x = 0.0F;
  float y = 0.0F;
};

struct Pose {
  float x = 0.0F;
  float y = 0.0F;
};

struct OccupancyGrid {
  int width = 0;
  int height = 0;
  float resolution = 0.05F;  // 5 cm per cell (default)
  std::vector<bool> cells;   // true = occupied, false = free
  Pose origin;                // grid origin in world frame

  bool is_free(int gx, int gy) const {
    if (gx < 0 || gx >= width || gy < 0 || gy >= height) return false;
    return !cells[static_cast<size_t>(gy) * static_cast<size_t>(width) + static_cast<size_t>(gx)];
  }
};

inline void world_to_grid(const OccupancyGrid &grid, float wx, float wy,
                          int &gx, int &gy) {
  gx = static_cast<int>((wx - grid.origin.x) / grid.resolution);
  gy = static_cast<int>((wy - grid.origin.y) / grid.resolution);
}

inline Waypoint grid_to_world(const OccupancyGrid &grid, int gx, int gy) {
  return {grid.origin.x + (static_cast<float>(gx) + 0.5F) * grid.resolution,
          grid.origin.y + (static_cast<float>(gy) + 0.5F) * grid.resolution};
}

class AStarPlanner {
public:
  struct Params {
    int max_iterations;
    float heuristic_weight;  // 1.0 = standard A*, >1.0 = weighted (faster)
  };

  AStarPlanner() : params_{10000, 1.0F} {}
  explicit AStarPlanner(const Params &p) : params_(p) {}

  std::vector<Waypoint> plan(const OccupancyGrid &grid,
                             const Pose &start,
                             const Pose &goal) const {
    int sx = 0, sy = 0, gx = 0, gy = 0;
    world_to_grid(grid, start.x, start.y, sx, sy);
    world_to_grid(grid, goal.x, goal.y, gx, gy);

    if (!grid.is_free(sx, sy) || !grid.is_free(gx, gy)) return {};

    if (sx == gx && sy == gy) return {grid_to_world(grid, sx, sy)};

    return search(grid, sx, sy, gx, gy);
  }

  const Params &params() const { return params_; }

private:
  struct Node {
    int x = 0, y = 0;
    float g = 0.0F;   // cost from start
    float h = 0.0F;   // heuristic to goal
    float f() const { return g + h; }
    int parent_idx = -1;

    // priority_queue is a max-heap by default; greater<Node> makes it min-heap
    bool operator>(const Node &o) const { return f() > o.f(); }
  };

  static constexpr int kDx[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
  static constexpr int kDy[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
  static constexpr float kCardinalCost = 1.0F;
  static constexpr float kDiagonalCost = 1.41421356F;

  static float heuristic(int x1, int y1, int x2, int y2) {
    int dx = std::abs(x1 - x2);
    int dy = std::abs(y1 - y2);
    return static_cast<float>(std::min(dx, dy)) * kDiagonalCost
         + static_cast<float>(std::abs(dx - dy)) * kCardinalCost;
  }

  static uint64_t hash_xy(int x, int y) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32)
         | static_cast<uint64_t>(static_cast<uint32_t>(y));
  }

  std::vector<Waypoint> search(const OccupancyGrid &grid,
                               int sx, int sy, int gx, int gy) const {
    std::vector<Node> closed;
    closed.reserve(1024);
    std::unordered_set<uint64_t> visited;
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;

    open.push({sx, sy, 0.0F, heuristic(sx, sy, gx, gy), -1});

    int iter = 0;
    while (!open.empty() && iter < params_.max_iterations) {
      ++iter;

      Node current = open.top();
      open.pop();

      if (visited.count(hash_xy(current.x, current.y))) continue;
      visited.insert(hash_xy(current.x, current.y));

      int current_idx = static_cast<int>(closed.size());
      closed.push_back(current);

      if (current.x == gx && current.y == gy) {
        return reconstruct_path(closed, current_idx, grid);
      }

      for (int i = 0; i < 8; ++i) {
        int nx = current.x + kDx[i];
        int ny = current.y + kDy[i];

        if (!grid.is_free(nx, ny)) continue;
        if (visited.count(hash_xy(nx, ny))) continue;

        float step_cost = (i == 0 || i == 2 || i == 5 || i == 7) ? kCardinalCost : kDiagonalCost;
        open.push({nx, ny,
                   current.g + step_cost,
                   heuristic(nx, ny, gx, gy),
                   current_idx});
      }
    }
    return {};  // no path found
  }

  static std::vector<Waypoint> reconstruct_path(const std::vector<Node> &closed,
                                                int goal_idx,
                                                const OccupancyGrid &grid) {
    std::vector<Waypoint> path;
    int idx = goal_idx;
    while (idx >= 0 && idx < static_cast<int>(closed.size())) {
      const auto &n = closed[idx];
      path.push_back(grid_to_world(grid, n.x, n.y));
      idx = n.parent_idx;
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
