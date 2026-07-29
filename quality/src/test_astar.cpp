/// @file test_astar.cpp — A* path planning unit tests (no ROS2)
#include "ros2_robot_middleware/domain/planning/astar_planner.hpp"

#include <gtest/gtest.h>

using amr::domain::planning::AStarPlanner;
using amr::domain::planning::OccupancyGrid;
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

void set_obstacle(OccupancyGrid &grid, int gx, int gy) {
  if (gx >= 0 && gx < grid.width && gy >= 0 && gy < grid.height) {
    grid.cells[static_cast<size_t>(gy) * static_cast<size_t>(grid.width)
               + static_cast<size_t>(gx)] = true;
  }
}

}  // namespace

// ── Given_EmptyGrid_Then_StraightLinePath ─────────────────────────────

TEST(AStarPlannerTest, Given_EmptyGrid_Then_StraightLinePath) {
  auto grid = make_grid(20, 20, 0.1F);
  AStarPlanner planner;

  Pose start{0.5F, 0.5F};
  Pose goal{1.5F, 0.5F};

  auto path = planner.plan(grid, start, goal);

  ASSERT_FALSE(path.empty());
  // Start should be near (0.5, 0.5)
  EXPECT_NEAR(path.front().x, 0.5F, 0.1F);
  EXPECT_NEAR(path.front().y, 0.5F, 0.1F);
  // Goal should be near (1.5, 0.5)
  EXPECT_NEAR(path.back().x, 1.5F, 0.1F);
  EXPECT_NEAR(path.back().y, 0.5F, 0.1F);
}

// ── Given_BlockedStart_Then_EmptyPath ──────────────────────────────────

TEST(AStarPlannerTest, Given_BlockedStart_Then_EmptyPath) {
  auto grid = make_grid(10, 10, 0.1F);
  set_obstacle(grid, 2, 2);  // block start cell
  AStarPlanner planner;

  Pose start{0.25F, 0.25F};  // → cell (2,2)
  Pose goal{0.75F, 0.75F};

  auto path = planner.plan(grid, start, goal);
  EXPECT_TRUE(path.empty());
}

// ── Given_SameCell_Then_TrivialPath ────────────────────────────────────

TEST(AStarPlannerTest, Given_SameCell_Then_TrivialPath) {
  auto grid = make_grid(10, 10, 0.1F);
  AStarPlanner planner;

  Pose start{0.25F, 0.25F};
  Pose goal{0.25F, 0.25F};

  auto path = planner.plan(grid, start, goal);
  ASSERT_EQ(path.size(), 1u);
  EXPECT_NEAR(path[0].x, 0.25F, 0.1F);
  EXPECT_NEAR(path[0].y, 0.25F, 0.1F);
}

// ── Given_ObstacleWall_Then_DetourAround ───────────────────────────────

TEST(AStarPlannerTest, Given_ObstacleWall_Then_DetourAround) {
  // 20×20 grid, wall at x=10 from y=0 to y=18 (gap at y=19)
  auto grid = make_grid(20, 20, 0.1F);
  for (int y = 0; y < 19; ++y) set_obstacle(grid, 10, y);
  AStarPlanner planner;

  Pose start{0.45F, 0.95F};   // → cell (4, 9), free
  Pose goal{1.45F, 0.95F};   // → cell (14, 9), free — behind wall

  auto path = planner.plan(grid, start, goal);
  ASSERT_FALSE(path.empty());

  // Verify path does NOT go through obstacle cells at x=10
  for (const auto &wp : path) {
    int gx = static_cast<int>(wp.x / 0.1F);
    int gy = static_cast<int>(wp.y / 0.1F);
    // Allow y=19 (the gap) and x != 10
    bool passes_wall = (gx == 10 && gy >= 0 && gy < 19);
    EXPECT_FALSE(passes_wall) << "Path passes through obstacle at (" << gx << "," << gy << ")";
  }
}

// ── Given_NoPath_Then_EmptyResult ──────────────────────────────────────

TEST(AStarPlannerTest, Given_NoPath_Then_EmptyResult) {
  // Fully enclosed start cell — cannot reach goal
  auto grid = make_grid(10, 10, 0.1F);
  set_obstacle(grid, 4, 5); set_obstacle(grid, 5, 5); set_obstacle(grid, 6, 5);
  set_obstacle(grid, 4, 6); /* start at (5,6) */ set_obstacle(grid, 6, 6);
  set_obstacle(grid, 4, 7); set_obstacle(grid, 5, 7); set_obstacle(grid, 6, 7);
  AStarPlanner planner;

  Pose start{0.55F, 0.65F};  // → cell (5, 6) — enclosed
  Pose goal{0.85F, 0.85F};   // → cell (8, 8) — free

  auto path = planner.plan(grid, start, goal);
  EXPECT_TRUE(path.empty());
}

// ── Given_LargeGrid_Then_CompletesWithinIterations ─────────────────────

TEST(AStarPlannerTest, Given_LargeGrid_Then_CompletesWithinIterations) {
  auto grid = make_grid(100, 100, 0.05F);
  AStarPlanner planner;

  Pose start{0.25F, 0.25F};
  Pose goal{4.75F, 4.75F};
  auto path = planner.plan(grid, start, goal);

  ASSERT_FALSE(path.empty());
  EXPECT_GT(path.size(), 1u);  // long path, not trivial
}
