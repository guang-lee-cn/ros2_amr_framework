/// @file test_scan_to_grid.cpp — ScanToGrid raytrace 单测（NAV2 ObstacleLayer 对标）
#include "ros2_robot_middleware/domain/planning/scan_to_grid.hpp"

#include <gtest/gtest.h>
#include <cmath>

using amr::domain::planning::OccupancyGrid;
using amr::domain::planning::ScanToGrid;

namespace {
OccupancyGrid make_grid(int w, int h, float res = 0.1F) {
  OccupancyGrid g;
  g.width = w; g.height = h; g.resolution = res; g.origin = {0.0F, 0.0F};
  g.cells.assign(static_cast<size_t>(w * h), OccupancyGrid::FREE);
  return g;
}
}  // namespace

// ── Given_ObstacleAhead_WhenRaytrace_HitLethalRayFree ──────────────────
// robot (2,2,0)，前方 1.5m 障碍（range 1.5, angle 0）→ hit (3.5,2) LETHAL，
// 射线路径 (2,2)→(3.5,2) 中间格 FREE（clearing）。
TEST(ScanToGridTest, Given_ObstacleAhead_WhenRaytrace_HitLethalRayFree) {
  auto g = make_grid(40, 40, 0.1F);
  ScanToGrid stg;
  std::vector<float> ranges = {1.5F};  // 单射线，angle 0
  stg.raytrace(g, ranges.data(), 1, 0.0F, 0.1F, 2.0F, 2.0F, 0.0F);
  // hit (2+1.5, 2) = (3.5, 2) → cell (35, 20) LETHAL
  EXPECT_EQ(g.cost_at(35, 20), OccupancyGrid::LETHAL);
  // 射线路径中间 (2.5, 2) → cell (25, 20) FREE（clearing）
  EXPECT_EQ(g.cost_at(25, 20), OccupancyGrid::FREE);
}

// ── Given_InvalidRange_WhenRaytrace_Skipped ────────────────────────────
// range > max_range（6.5）→ 射线跳过，不标不清。
TEST(ScanToGridTest, Given_InvalidRange_WhenRaytrace_NoChange) {
  auto g = make_grid(40, 40, 0.1F);
  ScanToGrid stg;
  std::vector<float> ranges = {10.0F};  // 超 max_range
  stg.raytrace(g, ranges.data(), 1, 0.0F, 0.1F, 2.0F, 2.0F, 0.0F);
  EXPECT_EQ(g.cost_at(35, 20), OccupancyGrid::FREE);  // 不标
}

// ── Given_BoxShapedObstacle_WhenRaytrace_FrontFaceLethal ───────────────
// 模拟 box：前方多射线 hit 不同 y（box 前面），验证前缘多点 LETHAL。
TEST(ScanToGridTest, Given_BoxFrontFace_WhenRaytrace_MultiHitLethal) {
  auto g = make_grid(60, 60, 0.1F);  // 6m×6m
  ScanToGrid stg;
  // robot (2, 3, 0)，box 前缘在 x=5（距 3m），y 跨 2.8-3.2（box 宽 0.4）
  // 5 条射线 angle -0.0666..+0.0666 rad（±3.8°），range 3.0
  std::vector<float> ranges(5, 3.0F);
  float angle_min = -0.0666F, inc = 0.0333F;
  stg.raytrace(g, ranges.data(), 5, angle_min, inc, 2.0F, 3.0F, 0.0F);
  // 中心射线 hit (2+3, 3) = (5, 3) → cell (50, 30) LETHAL
  EXPECT_EQ(g.cost_at(50, 30), OccupancyGrid::LETHAL);
  // 射线 clearing：robot 旁 (3, 3) → cell (30, 30) FREE
  EXPECT_EQ(g.cost_at(30, 30), OccupancyGrid::FREE);
}

// ── Given_RobotTheta_WhenRaytrace_RotatedHit ───────────────────────────
// robot theta=90°（朝北），前方射线 hit 北侧。
TEST(ScanToGridTest, Given_RobotTheta90_WhenRaytrace_HitNorth) {
  auto g = make_grid(40, 40, 0.1F);
  ScanToGrid stg;
  std::vector<float> ranges = {2.0F};
  // robot 居 cell(20,10) 中心=世界(2.05,1.05)，朝北 theta=π/2，range 2.0m
  // → hit 世界(2.05,3.05)=cell(20,30)。用 cell 中心避开边界浮点截断。
  stg.raytrace(g, ranges.data(), 1, 0.0F, 0.1F, 2.05F, 1.05F,
               static_cast<float>(M_PI / 2.0));  // 朝北
  EXPECT_EQ(g.cost_at(20, 30), OccupancyGrid::LETHAL);
}
