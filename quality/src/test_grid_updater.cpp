/// @file test_grid_updater.cpp — GridUpdater uint8 代价场 + 指数 inflation
#include "ros2_robot_middleware/domain/planning/astar_planner.hpp"
#include "ros2_robot_middleware/domain/planning/grid_updater.hpp"

#include <gtest/gtest.h>
#include <limits>

using amr::domain::planning::OccupancyGrid;
using amr::domain::planning::PerceivedObject;
using amr::domain::planning::GridUpdater;
using amr::domain::planning::AStarPlanner;
using amr::domain::planning::Pose;

namespace {

OccupancyGrid make_grid(int w, int h, float res = 0.1F) {
  OccupancyGrid g;
  g.width = w;
  g.height = h;
  g.resolution = res;
  g.origin = {0.0F, 0.0F};
  g.cells.assign(static_cast<size_t>(w * h), OccupancyGrid::FREE);
  return g;
}

}  // namespace

// ── 旧语义适配 uint8（mark_obstacles / skip_idx / A* 集成）─────────────

TEST(GridUpdaterTest, Given_Obstacle_Then_CenterLethal) {
  auto g = make_grid(20, 20, 0.1F);
  GridUpdater gu;
  PerceivedObject obs{0.5F, 0.5F, "o1"};
  gu.mark_obstacles(g, &obs, 1, std::numeric_limits<size_t>::max());
  EXPECT_EQ(g.cost_at(5, 5), OccupancyGrid::LETHAL);
}

TEST(GridUpdaterTest, Given_TargetExcluded_Then_TargetFree) {
  auto g = make_grid(20, 20, 0.1F);
  GridUpdater gu;
  PerceivedObject objects[2] = {{0.5F, 0.5F, "o0"}, {1.5F, 0.5F, "o1"}};
  gu.mark_obstacles(g, objects, 2, 0);  // skip objects[0]
  EXPECT_EQ(g.cost_at(5, 5), OccupancyGrid::FREE);
  EXPECT_EQ(g.cost_at(15, 5), OccupancyGrid::LETHAL);
}

TEST(GridUpdaterTest, Given_SingleTarget_Then_NothingMarked) {
  auto g = make_grid(20, 20, 0.1F);
  GridUpdater gu;
  PerceivedObject obs{0.5F, 0.5F, "o0"};
  gu.mark_obstacles(g, &obs, 1, 0);
  for (const auto &c : g.cells) EXPECT_EQ(c, OccupancyGrid::FREE);
}

TEST(GridUpdaterTest, Given_ObstacleBlocking_Then_APlansAround) {
  auto g = make_grid(20, 20, 0.1F);
  GridUpdater gu;
  AStarPlanner planner;
  PerceivedObject obs{1.0F, 0.5F, "blocker"};
  gu.mark_obstacles(g, &obs, 1, std::numeric_limits<size_t>::max());
  Pose start{0.5F, 0.5F}, goal{1.5F, 0.5F};
  auto path = planner.plan(g, start, goal);
  ASSERT_FALSE(path.empty());
  for (const auto &wp : path) {
    int gx = 0, gy = 0;
    world_to_grid(g, wp.x, wp.y, gx, gy);
    EXPECT_TRUE(g.is_traversable(gx, gy)) << "path 穿障碍";
  }
}

// ── 新：指数 inflation（S1，NAV2 InflationLayer 公式）─────────────────

TEST(GridUpdaterTest, Given_LethalPoint_WhenInflate_CenterIs254) {
  auto g = make_grid(40, 40, 0.05F);
  GridUpdater gu;
  gu.inflate(g, 1.0F, 1.0F);  // 质心 → cell (20,20)
  EXPECT_EQ(g.cost_at(20, 20), OccupancyGrid::LETHAL);
}

TEST(GridUpdaterTest, Given_CellAtInscribed_WhenInflate_AtLeastInscribed) {
  auto g = make_grid(40, 40, 0.05F);
  GridUpdater gu;
  gu.inflate(g, 1.0F, 1.0F);
  // 0.2m (4 cells) ≤ inscribed 0.22m → INSCRIBED
  EXPECT_GE(g.cost_at(24, 20), OccupancyGrid::INSCRIBED);
}

TEST(GridUpdaterTest, Given_CellOutsideInflation_WhenInflate_Is0) {
  auto g = make_grid(40, 40, 0.05F);
  GridUpdater gu;
  gu.inflate(g, 1.0F, 1.0F);
  // 0.75m (15 cells) > inflation 0.55m → FREE
  EXPECT_EQ(g.cost_at(35, 20), OccupancyGrid::FREE);
}

TEST(GridUpdaterTest, Given_CellBetweenInscribedAndRadius_WhenInflate_Decreasing) {
  auto g = make_grid(40, 40, 0.05F);
  GridUpdater gu;
  gu.inflate(g, 1.0F, 1.0F);
  const uint8_t near_c = g.cost_at(24, 20);  // 0.2m
  const uint8_t mid_c = g.cost_at(28, 20);   // 0.4m
  const uint8_t far_c = g.cost_at(30, 20);   // 0.5m
  EXPECT_GT(near_c, mid_c);  // 指数衰减：越远越小
  EXPECT_GT(mid_c, far_c);
  EXPECT_GT(far_c, 0);  // 仍在 inflation 内
}

TEST(GridUpdaterTest, Given_MultipleInflations_WhenOverlap_TakesMax) {
  auto g = make_grid(40, 40, 0.05F);
  GridUpdater gu;
  gu.inflate(g, 1.0F, 1.0F);
  gu.inflate(g, 1.05F, 1.0F);
  EXPECT_EQ(g.cost_at(20, 20), OccupancyGrid::LETHAL);
}
