/// @file test_vfh_avoidance.cpp — VfhAvoidance unit tests (no ROS2)
#include "ros2_robot_middleware/domain/execution/vfh_avoidance.hpp"

#include <cmath>
#include <limits>
#include <gtest/gtest.h>

using amr::domain::execution::VfhAvoidance;
using amr::domain::execution::ScanData;

namespace {

constexpr float kNoEcho = 20.0F;  // > active_range → treat as open

/// 360 beams, 1° apart, angle_min = -π. Each beam gets a range; a helper
/// places an obstacle at a given bearing (rad, robot frame) and distance.
ScanData scan_with_obstacles(
    const std::vector<std::pair<float, float>> &obs) {
  ScanData s;
  s.angle_min = -static_cast<float>(M_PI);
  s.angle_increment = 2.0F * static_cast<float>(M_PI) / 360.0F;  // 1°
  s.ranges.assign(360, kNoEcho);
  for (const auto &[bearing, range] : obs) {
    int i = static_cast<int>((bearing + static_cast<float>(M_PI))
                             / s.angle_increment);
    i = std::clamp(i, 0, 359);
    s.ranges[static_cast<std::size_t>(i)] = range;
  }
  return s;
}

}  // namespace

class VfhAvoidanceTest : public ::testing::Test {
protected:
  VfhAvoidance vfh_;
};

// ── GivenObstacleAheadInGoalDirection_WhenSteer_ReturnsAvoidanceTurn ───
// Obstacle 0.4m directly ahead (goal bearing 0) → VFH must steer away.

TEST_F(VfhAvoidanceTest,
       GivenObstacleAheadInGoalDirection_WhenSteer_ReturnsAvoidanceTurn) {
  // Obstacle right in front; beams on either side are clear (kNoEcho).
  auto scan = scan_with_obstacles({{0.0F, 0.4F}});
  auto r = vfh_.steer(scan, 0.0F, 0.5F);
  EXPECT_NE(r.steering, 0.0F);   // avoidance turn produced
  EXPECT_FALSE(r.blocked);
}

// ── GivenClearGoalDirection_WhenSteer_ReturnsZero ──────────────────────
// Open field → intervention gate fails → no steering change.

TEST_F(VfhAvoidanceTest, GivenClearGoalDirection_WhenSteer_ReturnsZero) {
  auto scan = scan_with_obstacles({});
  auto r = vfh_.steer(scan, 0.0F, 0.5F);
  EXPECT_FLOAT_EQ(r.steering, 0.0F);
  EXPECT_FALSE(r.blocked);
}

// ── GivenObstacleAwayFromGoalDirection_WhenSteer_ReturnsZero ───────────
// Side obstacle (90°) is outside the goal ±45° FOV → not an intervention.

TEST_F(VfhAvoidanceTest,
       GivenObstacleAwayFromGoalDirection_WhenSteer_ReturnsZero) {
  auto scan = scan_with_obstacles({{1.5708F, 0.3F}});  // 90° left
  auto r = vfh_.steer(scan, 0.0F, 0.5F);
  EXPECT_FLOAT_EQ(r.steering, 0.0F);
}

// ── GivenObstacleAheadWithGapLeft_WhenSteer_TurnsTowardGap ─────────────
// Wide obstacle block across −60°..+10° (a real box covers many beams);
// gap on the left (+11°..+180°). Robot-frame positive angle = robot-left.
// Steering > 0 (CCW turn) toward the left gap.

TEST_F(VfhAvoidanceTest,
       GivenObstacleAheadWithGapLeft_WhenSteer_TurnsTowardGap) {
  auto scan = scan_with_obstacles({});
  const auto set_block = [&scan](float deg_lo, float deg_hi, float range) {
    for (int d = static_cast<int>(deg_lo); d <= static_cast<int>(deg_hi); ++d) {
      int b = static_cast<int>((d * 0.0174533F + static_cast<float>(M_PI))
                               / scan.angle_increment);
      b = std::clamp(b, 0, 359);
      scan.ranges[static_cast<std::size_t>(b)] = range;
    }
  };
  set_block(-60.0F, 10.0F, 0.3F);  // wide wall ahead + right
  auto r = vfh_.steer(scan, 0.0F, 0.5F);
  EXPECT_GT(r.steering, 0.0F);  // gap on the robot's left → turn left
  EXPECT_FALSE(r.blocked);
}

// ── GivenSurroundedByObstacles_WhenSteer_ReportsBlocked ────────────────
// Obstacles every 6° around the full circle (−180°..+180°) → no passable
// gap → blocked flag set.

TEST_F(VfhAvoidanceTest,
       GivenSurroundedByObstacles_WhenSteer_ReportsBlocked) {
  std::vector<std::pair<float, float>> walls;
  for (int deg = -180; deg < 180; deg += 6) {
    walls.emplace_back(static_cast<float>(deg) * 0.0174533F, 0.3F);
  }
  auto scan = scan_with_obstacles(walls);
  auto r = vfh_.steer(scan, 0.0F, 0.5F);
  EXPECT_FLOAT_EQ(r.steering, 0.0F);
  EXPECT_TRUE(r.blocked);
}

// ── GivenMultipleGaps_WhenSteer_SelectsClosestToGoal ───────────────────
// Obstacle wall in the goal direction (+60°), plus right (+90°..+130°) and
// left (−60°..−10°) walls. Passable gaps: −180..−61, −9..+54, +131..+180.
// Goal +60° → the −9..+54 gap (center +22°) is closest → steering > 0.

TEST_F(VfhAvoidanceTest, GivenMultipleGaps_WhenSteer_SelectsClosestToGoal) {
  auto scan = scan_with_obstacles({});
  const auto set_block = [&scan](float deg_lo, float deg_hi, float range) {
    for (int d = static_cast<int>(deg_lo); d <= static_cast<int>(deg_hi); ++d) {
      int b = static_cast<int>((d * 0.0174533F + static_cast<float>(M_PI))
                               / scan.angle_increment);
      b = std::clamp(b, 0, 359);
      scan.ranges[static_cast<std::size_t>(b)] = range;
    }
  };
  set_block(55.0F, 65.0F, 0.3F);   // blocks the goal bearing
  set_block(90.0F, 130.0F, 0.3F);  // right wall
  set_block(-60.0F, -10.0F, 0.3F); // left wall
  auto r = vfh_.steer(scan, 1.0472F, 0.5F);  // goal +60°
  EXPECT_GT(r.steering, 0.0F);  // nearest gap (center +22°) → turn left
  EXPECT_FALSE(r.blocked);
}

// ── GivenInfAndNanRanges_WhenSteer_IgnoresThem ─────────────────────────
// inf/NaN beams are no-echo — only real obstacles matter.

TEST_F(VfhAvoidanceTest, GivenInfAndNanRanges_WhenSteer_IgnoresThem) {
  auto scan = scan_with_obstacles({{0.0F, 0.4F}});
  scan.ranges[10] = std::numeric_limits<float>::quiet_NaN();
  scan.ranges[20] = std::numeric_limits<float>::infinity();
  auto r = vfh_.steer(scan, 0.0F, 0.5F);
  EXPECT_NE(r.steering, 0.0F);  // NaN/inf did not corrupt the avoidance
  EXPECT_FALSE(r.blocked);
}

// ── GivenRobotStationary_WhenSteer_DoesNotTurn ─────────────────────────
// cmd_v below min_linear → no avoidance rotation (let PurePursuit handle).

TEST_F(VfhAvoidanceTest, GivenRobotStationary_WhenSteer_DoesNotTurn) {
  auto scan = scan_with_obstacles({{0.0F, 0.3F}});
  auto r = vfh_.steer(scan, 0.0F, 0.0F);
  EXPECT_FLOAT_EQ(r.steering, 0.0F);
  EXPECT_FALSE(r.blocked);
}

// ── GivenLastGapStillOpen_WhenGoalFlips_SticksToLast (hysteresis) ──────
// 帧1 障碍前 + 左右 gap，goal 偏左 → 选左 gap。帧2 同布局 goal 翻右，
// 无滞后会切右 gap；滞后沿用 last（左 gap 仍 passable）→ steer 仍朝左。

TEST_F(VfhAvoidanceTest, GivenLastGapStillOpen_WhenGoalFlips_SticksToLast) {
  const auto set_block = [](ScanData &s, float deg_lo, float deg_hi, float r) {
    for (int d = static_cast<int>(deg_lo); d <= static_cast<int>(deg_hi); ++d) {
      int b = static_cast<int>((d * 0.0174533F + static_cast<float>(M_PI))
                               / s.angle_increment);
      s.ranges[static_cast<std::size_t>(std::clamp(b, 0, 359))] = r;
    }
  };
  ScanData s = scan_with_obstacles({});
  set_block(s, -30.0F, 30.0F, 0.3F);  // 障碍正前，左右各留大 gap

  auto r1 = vfh_.steer(s, 0.5F, 0.5F);   // goal 偏左 → 选左 gap
  ASSERT_GT(r1.steering, 0.0F) << "帧1 应转向左 gap";

  auto r2 = vfh_.steer(s, -0.5F, 0.5F);  // goal 翻右，但左 gap 仍 passable
  EXPECT_GT(r2.steering, 0.0F) << "滞后应沿用左 gap，不因 goal 翻转切右";
}

// ── GivenLastGapGone_WhenSteer_SwitchesToFreshGap ───────────────────────
// 上次选的 gap 在新 scan 被堵 → 放弃滞后，选新 gap。

TEST_F(VfhAvoidanceTest, GivenLastGapGone_WhenSteer_SwitchesToFreshGap) {
  const auto set_block = [](ScanData &s, float deg_lo, float deg_hi, float r) {
    for (int d = static_cast<int>(deg_lo); d <= static_cast<int>(deg_hi); ++d) {
      int b = static_cast<int>((d * 0.0174533F + static_cast<float>(M_PI))
                               / s.angle_increment);
      s.ranges[static_cast<std::size_t>(std::clamp(b, 0, 359))] = r;
    }
  };
  // 帧1：前 + 左半全堵（-30..180°），仅右 gap → 选右
  ScanData s1 = scan_with_obstacles({});
  set_block(s1, -30.0F, 180.0F, 0.3F);
  auto r1 = vfh_.steer(s1, 0.0F, 0.5F);
  ASSERT_LT(r1.steering, 0.0F) << "帧1 应转向右 gap（唯一）";

  // 帧2：右 gap 被堵（-180..-30° 堵），左 gap 开 → last(右) 不 passable，切左
  ScanData s2 = scan_with_obstacles({});
  set_block(s2, -180.0F, -30.0F, 0.3F);
  set_block(s2, -30.0F, 30.0F, 0.3F);  // 前也堵，左 gap(30..180) 开
  auto r2 = vfh_.steer(s2, 0.0F, 0.5F);
  EXPECT_GT(r2.steering, 0.0F) << "last 右 gap 被堵 → 切左 gap";
}
