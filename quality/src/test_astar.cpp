/// @file test_astar.cpp — A* path planning unit tests (no ROS2)
/// OccupancyGrid uint8 代价场适配 + cost-aware 新测试。
#include "ros2_robot_middleware/domain/planning/astar_planner.hpp"

#include <gtest/gtest.h>

#include <cmath>

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
  grid.cells.assign(static_cast<size_t>(w * h), OccupancyGrid::FREE);
  return grid;
}

void set_obstacle(OccupancyGrid &grid, int gx, int gy) {
  if (gx >= 0 && gx < grid.width && gy >= 0 && gy < grid.height) {
    grid.cells[static_cast<size_t>(gy) * static_cast<size_t>(grid.width)
               + static_cast<size_t>(gx)] = OccupancyGrid::LETHAL;
  }
}

// 画 INSCRIBED 圆盘（r_cells 半径）— 模拟 GridUpdater::inflate 铺的不可走区
void set_inscribed_disk(OccupancyGrid &grid, int cx, int cy, int r_cells) {
  for (int dy = -r_cells; dy <= r_cells; ++dy) {
    for (int dx = -r_cells; dx <= r_cells; ++dx) {
      if (dx * dx + dy * dy > r_cells * r_cells) continue;
      if (cx + dx < 0 || cx + dx >= grid.width || cy + dy < 0 || cy + dy >= grid.height) continue;
      grid.cells[static_cast<size_t>(cy + dy) * static_cast<size_t>(grid.width)
                 + static_cast<size_t>(cx + dx)] = OccupancyGrid::INSCRIBED;
    }
  }
}

float dist(const amr::domain::planning::Waypoint &wp, const Pose &p) {
  return std::hypot(wp.x - p.x, wp.y - p.y);
}

}  // namespace

// ── 原有测试（uint8 适配）──────────────────────────────────────────────

TEST(AStarPlannerTest, Given_EmptyGrid_Then_StraightLinePath) {
  auto grid = make_grid(20, 20, 0.1F);
  AStarPlanner planner;
  Pose start{0.5F, 0.5F}, goal{1.5F, 0.5F};
  auto path = planner.plan(grid, start, goal);
  ASSERT_FALSE(path.empty());
  EXPECT_NEAR(path.front().x, 0.5F, 0.1F);
  EXPECT_NEAR(path.back().x, 1.5F, 0.1F);
}

TEST(AStarPlannerTest, Given_BlockedStart_Then_EmptyPath) {
  auto grid = make_grid(10, 10, 0.1F);
  set_obstacle(grid, 2, 2);
  AStarPlanner planner;
  Pose start{0.25F, 0.25F}, goal{0.75F, 0.75F};
  EXPECT_TRUE(planner.plan(grid, start, goal).empty());
}

TEST(AStarPlannerTest, Given_SameCell_Then_TrivialPath) {
  auto grid = make_grid(10, 10, 0.1F);
  AStarPlanner planner;
  Pose start{0.25F, 0.25F}, goal{0.25F, 0.25F};
  auto path = planner.plan(grid, start, goal);
  ASSERT_EQ(path.size(), 1u);
}

TEST(AStarPlannerTest, Given_ObstacleWall_Then_DetourAround) {
  auto grid = make_grid(20, 20, 0.1F);
  for (int y = 0; y < 19; ++y) set_obstacle(grid, 10, y);
  AStarPlanner planner;
  Pose start{0.45F, 0.95F}, goal{1.45F, 0.95F};
  auto path = planner.plan(grid, start, goal);
  ASSERT_FALSE(path.empty());
  for (const auto &wp : path) {
    int gx = static_cast<int>(wp.x / 0.1F);
    int gy = static_cast<int>(wp.y / 0.1F);
    EXPECT_FALSE(gx == 10 && gy >= 0 && gy < 19);
  }
}

TEST(AStarPlannerTest, Given_NoPath_Then_EmptyResult) {
  auto grid = make_grid(10, 10, 0.1F);
  set_obstacle(grid, 4, 5); set_obstacle(grid, 5, 5); set_obstacle(grid, 6, 5);
  set_obstacle(grid, 4, 6); set_obstacle(grid, 6, 6);
  set_obstacle(grid, 4, 7); set_obstacle(grid, 5, 7); set_obstacle(grid, 6, 7);
  AStarPlanner planner;
  Pose start{0.55F, 0.65F}, goal{0.85F, 0.85F};
  EXPECT_TRUE(planner.plan(grid, start, goal).empty());
}

TEST(AStarPlannerTest, Given_LargeGrid_Then_CompletesWithinIterations) {
  auto grid = make_grid(100, 100, 0.05F);
  AStarPlanner planner;
  Pose start{0.25F, 0.25F}, goal{4.75F, 4.75F};
  auto path = planner.plan(grid, start, goal);
  ASSERT_FALSE(path.empty());
  EXPECT_GT(path.size(), 1u);
}

// ── cost-aware 新测试（S1）─────────────────────────────────────────────

// Given_CostGradient_WhenPlan_PrefersLowerCost：下半高 cost，A* 应走上半。
TEST(AStarPlannerTest, Given_CostGradient_WhenPlan_PrefersLowerCost) {
  auto grid = make_grid(20, 20, 0.1F);
  for (int y = 0; y < 10; ++y)
    for (int x = 0; x < 20; ++x)
      grid.cells[static_cast<size_t>(y) * 20 + x] = 100;  // 高 cost 梯度（非 lethal）
  AStarPlanner planner;
  Pose start{0.05F, 1.85F}, goal{1.85F, 1.85F};  // 上半起终
  auto path = planner.plan(grid, start, goal);
  ASSERT_FALSE(path.empty());
  for (const auto &wp : path) {
    int gy = static_cast<int>(wp.y / 0.1F);
    EXPECT_GE(gy, 9) << "cost-aware A* 应避开下半高 cost 区";
  }
}

// Given_HeuristicWeight_WhenPlan_Applied：weight>1 加速（路径仍可达）
TEST(AStarPlannerTest, Given_HeuristicWeight2_WhenPlan_Reachable) {
  auto grid = make_grid(40, 40, 0.1F);
  AStarPlanner planner(AStarPlanner::Params{50000, 2.0F});  // weight=2
  Pose start{0.05F, 0.05F}, goal{3.85F, 3.85F};
  auto path = planner.plan(grid, start, goal);
  ASSERT_FALSE(path.empty());
  EXPECT_NEAR(path.back().x, 3.85F, 0.2F);
}

// ── 端点有界吸附（机台停靠死锁修复，20260817 评审 §5）──────────────────
// res=0.1，snap 0.75 → 搜索窗 R=7 cells。

// Given_StartInInscribed_WhenSnapOn：起点落 0.5m INSCRIBED 盘内（机台停靠
// 后 stale 膨胀盘场景）→ 吸附最近可走格续算，path 非空且首点在 snap 上限内。
TEST(AStarPlannerTest, Given_StartInInscribed_WhenSnapOn_ThenPathFromNearestTraversable) {
  auto grid = make_grid(40, 40, 0.1F);
  set_inscribed_disk(grid, 20, 20, 5);  // 0.5m 盘，圆心=起点格
  AStarPlanner planner(AStarPlanner::Params{50000, 1.0F, 0.75F});
  Pose start{2.05F, 2.05F}, goal{3.05F, 2.05F};
  auto path = planner.plan(grid, start, goal);
  ASSERT_FALSE(path.empty());
  EXPECT_LE(dist(path.front(), start), 0.75F) << "首点应在 snap 半径内";
}

// Given_GoalInInscribed_WhenSnapOn：目标落盘内（靠泊点在膨胀圈内，D3）→
// path 非空且末点吸附到盘外最近可走格。
TEST(AStarPlannerTest, Given_GoalInInscribed_WhenSnapOn_ThenPathToNearestTraversable) {
  auto grid = make_grid(40, 40, 0.1F);
  set_inscribed_disk(grid, 30, 20, 5);  // 0.5m 盘，圆心=目标格
  AStarPlanner planner(AStarPlanner::Params{50000, 1.0F, 0.75F});
  Pose start{1.05F, 2.05F}, goal{3.05F, 2.05F};
  auto path = planner.plan(grid, start, goal);
  ASSERT_FALSE(path.empty());
  EXPECT_LE(dist(path.back(), goal), 0.75F) << "末点应在 snap 半径内";
}

// Given_GoalBeyondSnapRadius_WhenSnapOn：目标在 1.7m 厚 LETHAL 墙芯，
// snap 半径（0.75m）内无可走格 → 空路径（"真被堵"语义保留，不吞错）。
TEST(AStarPlannerTest, Given_GoalBeyondSnapRadius_WhenSnapOn_ThenEmpty) {
  auto grid = make_grid(60, 40, 0.1F);
  for (int x = 28; x <= 44; ++x)
    for (int y = 0; y < 40; ++y) set_obstacle(grid, x, y);
  AStarPlanner planner(AStarPlanner::Params{50000, 1.0F, 0.75F});
  Pose start{0.55F, 2.05F}, goal{3.65F, 2.05F};  // 目标格 (36,20)，最近自由格 9 cells=0.9m
  EXPECT_TRUE(planner.plan(grid, start, goal).empty());
}

// Given_BlockedStart_WhenSnapOff：snap 默认 0=关闭 → 端点不可走仍空路径
//（现行为回归锁，零扰动承诺）。
TEST(AStarPlannerTest, Given_BlockedStart_WhenSnapOff_ThenEmpty) {
  auto grid = make_grid(20, 20, 0.1F);
  set_inscribed_disk(grid, 5, 5, 3);
  AStarPlanner planner;  // endpoint_snap_radius 默认 0
  Pose start{0.55F, 0.55F}, goal{1.55F, 0.55F};
  EXPECT_TRUE(planner.plan(grid, start, goal).empty());
}

// Given_TraversableEndpoints_WhenSnapOn：端点可走时零吸附（snap 不改变
// 正常规划的首末点）。
TEST(AStarPlannerTest, Given_TraversableEndpoints_WhenSnapOn_ThenNoSnap) {
  auto grid = make_grid(20, 20, 0.1F);
  AStarPlanner planner(AStarPlanner::Params{50000, 1.0F, 0.75F});
  Pose start{0.55F, 0.55F}, goal{1.55F, 0.55F};  // 精确格心
  auto path = planner.plan(grid, start, goal);
  ASSERT_FALSE(path.empty());
  EXPECT_NEAR(dist(path.front(), start), 0.0F, 0.01F);
  EXPECT_NEAR(dist(path.back(), goal), 0.0F, 0.01F);
}
