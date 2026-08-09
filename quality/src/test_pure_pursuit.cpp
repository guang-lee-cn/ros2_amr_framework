/// @file test_pure_pursuit.cpp — Pure Pursuit path tracking unit tests (no ROS2)
#include "ros2_robot_middleware/domain/execution/pure_pursuit.hpp"

#include <cmath>
#include <gtest/gtest.h>

using amr::domain::execution::PurePursuit;
using amr::domain::execution::Pose2D;
using amr::domain::execution::Waypoint;
using Params = PurePursuit::Params;

// ── Given_StraightLinePath_Then_ForwardVelocity ───────────────────────

TEST(PurePursuitTest, Given_StraightLinePath_Then_ForwardVelocity) {
  PurePursuit pp(Params{0.5F, 0.5F, 1.5F, 1.0F, 0.1F, 0.5F, 1.0F});

  std::vector<Waypoint> path = {{0.0F, 0.0F}, {0.5F, 0.0F}, {1.0F, 0.0F}, {1.5F, 0.0F}, {2.0F, 0.0F}};
  Pose2D current{0.0F, 0.0F, 0.0F};  // facing +x

  auto twist = pp.track(path, current);

  EXPECT_GT(twist.linear, 0.0F);
  EXPECT_NEAR(twist.angular, 0.0F, 0.1F);
}

// ── Given_MultiWaypointPath_Then_TracksInOrder ─────────────────────────
// Path-ordered lookahead: robot past the first waypoint must chase the
// next one, not backtrack to the nearest point.

TEST(PurePursuitTest, Given_MultiWaypointPath_Then_TracksInOrder) {
  PurePursuit pp(Params{0.3F, 0.5F, 1.5F, 1.0F, 0.1F, 0.5F, 1.0F});

  // L-shaped path: go right then up
  std::vector<Waypoint> path = {{0.0F, 0.0F}, {1.0F, 0.0F}, {1.0F, 1.0F}, {1.0F, 2.0F}};

  // Robot at the corner, facing up — should chase (1,1)/(1,2), not backtrack to (1,0)
  Pose2D current{1.0F, 0.5F, M_PI / 2.0F};
  auto twist = pp.track(path, current);

  EXPECT_GT(twist.linear, 0.0F);  // moving toward next segment
  // target is straight ahead (up) → near-zero steering
  EXPECT_NEAR(twist.angular, 0.0F, 0.3F);
}

// ── Given_TurnAhead_Then_ReducesSpeed ──────────────────────────────────
// Sharp turn → curvature-limited velocity slows down.

TEST(PurePursuitTest, Given_TurnAhead_Then_ReducesSpeed) {
  PurePursuit pp(Params{0.3F, 0.5F, 0.5F, 1.0F, 0.1F, 0.5F, 1.0F});  // low max_angular

  // 90° corner ahead: go right, then up
  std::vector<Waypoint> path = {{0.0F, 0.0F}, {0.5F, 0.0F}, {0.5F, 0.5F}};

  // Robot close enough to the corner that lookahead crosses into the
  // vertical segment — sharp turn at the target.
  Pose2D current{0.3F, 0.0F, 0.0F};
  auto twist = pp.track(path, current);

  // Curvature limit: v = ω_max·L / (2·sin α) — with ω_max=0.5, L=0.3
  // and α ≈ 68° toward (0.5,0.5), linear must be well below max_linear (0.5).
  EXPECT_GT(twist.linear, 0.0F);
  EXPECT_LT(twist.linear, 0.4F);  // slowed by curvature limit
}

// ── Given_NearGoal_Then_SlowsDown ──────────────────────────────────────
// Trapezoidal profile: decelerate inside slow_radius.

TEST(PurePursuitTest, Given_NearGoal_Then_SlowsDown) {
  PurePursuit pp(Params{0.5F, 1.0F, 1.5F, 1.0F, 0.1F, 0.5F, 1.0F});

  std::vector<Waypoint> path = {{0.0F, 0.0F}, {0.5F, 0.0F}, {1.0F, 0.0F}};

  // Far from goal → full speed
  Pose2D far{0.2F, 0.0F, 0.0F};
  auto twist_far = pp.track(path, far);
  EXPECT_NEAR(twist_far.linear, 1.0F, 0.05F);  // max_linear

  // Near goal (0.3m < slow_radius 0.5m) → reduced speed
  Pose2D near{0.7F, 0.0F, 0.0F};
  auto twist_near = pp.track(path, near);
  EXPECT_LT(twist_near.linear, 0.8F);  // clearly below max_linear 1.0
  EXPECT_GT(twist_near.linear, 0.0F);
}

// ── Given_GoalReached_Then_ZeroVelocity ───────────────────────────────

TEST(PurePursuitTest, Given_GoalReached_Then_ZeroVelocity) {
  PurePursuit pp(Params{0.5F, 0.5F, 1.5F, 1.0F, 0.1F, 0.5F, 1.0F});

  std::vector<Waypoint> path = {{0.0F, 0.0F}, {0.5F, 0.0F}, {1.0F, 0.0F}};
  Pose2D current{0.98F, 0.0F, 0.0F};  // within goal_tolerance

  auto twist = pp.track(path, current);

  EXPECT_FLOAT_EQ(twist.linear, 0.0F);
  EXPECT_FLOAT_EQ(twist.angular, 0.0F);
}

// ── Given_EmptyPath_Then_ZeroVelocity ─────────────────────────────────

TEST(PurePursuitTest, Given_EmptyPath_Then_ZeroVelocity) {
  PurePursuit pp;
  std::vector<Waypoint> path{};
  Pose2D current{0.0F, 0.0F, 0.0F};

  auto twist = pp.track(path, current);

  EXPECT_FLOAT_EQ(twist.linear, 0.0F);
  EXPECT_FLOAT_EQ(twist.angular, 0.0F);
}

// ── Given_FarFromPath_Then_StillMoves ─────────────────────────────────

TEST(PurePursuitTest, Given_FarFromPath_Then_StillMoves) {
  PurePursuit pp(Params{1.0F, 0.5F, 1.5F, 1.0F, 0.1F, 0.5F, 1.0F});

  std::vector<Waypoint> path = {{2.0F, 2.0F}, {3.0F, 3.0F}};
  Pose2D current{1.5F, 1.5F, 0.0F};  // toward (2,2)

  auto twist = pp.track(path, current);

  EXPECT_GT(twist.linear, 0.0F);
}

// ── Given_ProgressAdvanced_WhenRobotBacktracks_NoLookBehind ───────────
// 车推进到 path 后段再后退，progress 单调不回头 → lookahead 仍在前方，
// 不回头追早期 path 点（修复全局最近导致的车转圈）。

TEST(PurePursuitTest, Given_ProgressAdvanced_WhenRobotBacktracks_NoLookBehind) {
  PurePursuit pp(Params{0.5F, 0.5F, 1.5F, 1.0F, 0.1F, 0.5F, 1.0F});
  std::vector<Waypoint> path = {{0,0},{1,0},{2,0},{3,0},{4,0}};

  // 帧1: 车在 path 末段 → progress 推进
  pp.track(path, {3.0F, 0.0F, 0.0F});

  // 帧2: 车退回起点 —— progress 不回头，lookahead 仍朝 path[3+] 前方
  auto twist = pp.track(path, {0.5F, 0.0F, 0.0F});
  EXPECT_GT(twist.linear, 0.0F);  // 仍前进（不原地转追后方 path）
}

// ── Given_Reset_Then_ProgressRestarts ──────────────────────────────────

TEST(PurePursuitTest, Given_Reset_Then_ProgressRestarts) {
  PurePursuit pp(Params{0.5F, 0.5F, 1.5F, 1.0F, 0.1F, 0.5F, 1.0F});
  std::vector<Waypoint> path = {{0,0},{1,0},{2,0},{3,0}};

  pp.track(path, {3.0F, 0.0F, 0.0F});  // progress → 末段
  pp.reset();                           // progress → 0

  auto twist = pp.track(path, {0.0F, 0.0F, 0.0F});
  EXPECT_GT(twist.linear, 0.0F);  // reset 后正常跟踪
}
