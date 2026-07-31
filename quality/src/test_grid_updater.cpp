/// @file test_grid_updater.cpp — Occupancy grid obstacle marking tests (no ROS2)
#include "ros2_robot_middleware/domain/planning/astar_planner.hpp"
#include "ros2_robot_middleware/domain/planning/grid_updater.hpp"

#include <cmath>
#include <gtest/gtest.h>
#include <limits>

using amr::domain::planning::OccupancyGrid;
using amr::domain::planning::PerceivedObject;
using amr::domain::planning::GridUpdater;
using amr::domain::planning::AStarPlanner;
using amr::domain::planning::Pose;

namespace {

OccupancyGrid make_grid(int w, int h, float res = 0.1F) {
  OccupancyGrid grid;
  grid.width = w;
  grid.height = h;
  grid.resolution = res;
  grid.origin = {0.0F, 0.0F};
  grid.cells.assign(static_cast<size_t>(w * h), false);
  return grid;
}

}  // namespace

// ── Given_Obstacle_Then_CellsOccupied ────────────────────────────────
// An obstacle at (0.5, 0.5) with 0.3m inflation covers a 3x3 cell block
// (0.1m cells → radius 3 cells) around it.

TEST(GridUpdaterTest, Given_Obstacle_Then_CellsOccupied) {
  auto grid = make_grid(20, 20, 0.1F);
  GridUpdater updater(GridUpdater::Params{0.30F});

  PerceivedObject obs{0.5F, 0.5F, "o1"};
  updater.mark_obstacles(grid, &obs, 1, std::numeric_limits<size_t>::max());

  // Center cell (5,5) occupied
  EXPECT_TRUE(grid.is_free(5, 5) == false);
  // Edge of inflation (0.8, 0.5) → cell (8,5), dist 0.3 = 3 cells → occupied
  EXPECT_TRUE(grid.is_free(8, 5) == false);
  // Outside inflation (0.9, 0.5) → cell (9,5), dist 0.4 → free
  EXPECT_TRUE(grid.is_free(9, 5));
}

// ── Given_TargetExcluded_Then_TargetNotMarked ─────────────────────────
// skip_idx excludes the navigation target from being marked as obstacle.

TEST(GridUpdaterTest, Given_TargetExcluded_Then_TargetFree) {
  auto grid = make_grid(20, 20, 0.1F);
  GridUpdater updater(GridUpdater::Params{0.30F});

  PerceivedObject objects[2] = {{0.5F, 0.5F, "o0"}, {1.5F, 0.5F, "o1"}};
  updater.mark_obstacles(grid, objects, 2, 0);  // skip objects[0] (target)

  // Target cell (5,5) stays free
  EXPECT_TRUE(grid.is_free(5, 5));
  // Non-target obstacle (1.5,0.5) → cell (15,5) occupied
  EXPECT_TRUE(grid.is_free(15, 5) == false);
}

// ── Given_AllTargets_Then_NothingMarked ──────────────────────────────
// count == skip_idx+1 effectively marks nothing (single object, is target).

TEST(GridUpdaterTest, Given_SingleTarget_Then_NothingMarked) {
  auto grid = make_grid(20, 20, 0.1F);
  GridUpdater updater;

  PerceivedObject obs{0.5F, 0.5F, "o0"};
  updater.mark_obstacles(grid, &obs, 1, 0);  // skip the only object

  for (const auto &occ : grid.cells) {
    EXPECT_FALSE(occ) << "No cells should be occupied";
  }
}

// ── Given_ObstacleBlocking_Then_APlansAround ─────────────────────────
// End-to-end: obstacle on the direct path → A* must route around it.

TEST(GridUpdaterTest, Given_ObstacleBlocking_Then_APlansAround) {
  auto grid = make_grid(20, 20, 0.1F);
  GridUpdater updater(GridUpdater::Params{0.15F});
  AStarPlanner planner;

  // Obstacle at (1.0, 0.5) blocks the straight path from (0.5,0.5) to (1.5,0.5)
  PerceivedObject obs{1.0F, 0.5F, "blocker"};
  updater.mark_obstacles(grid, &obs, 1, std::numeric_limits<size_t>::max());

  Pose start{0.5F, 0.5F};
  Pose goal{1.5F, 0.5F};
  auto path = planner.plan(grid, start, goal);

  ASSERT_FALSE(path.empty()) << "A* must find an alternative route";
  // Path must not pass through the blocked corridor (x≈1.0, y≈0.5)
  for (const auto &wp : path) {
    int gx = 0, gy = 0;
    world_to_grid(grid, wp.x, wp.y, gx, gy);
    bool blocked = !grid.is_free(gx, gy);
    EXPECT_FALSE(blocked) << "Path passes through obstacle at (" << wp.x << "," << wp.y << ")";
  }
}
