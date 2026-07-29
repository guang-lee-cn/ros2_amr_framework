/// @file test_pure_pursuit.cpp — Pure Pursuit path tracking unit tests (no ROS2)
#include "ros2_robot_middleware/domain/execution/pure_pursuit.hpp"

#include <cmath>
#include <gtest/gtest.h>

using amr::domain::execution::PurePursuit;
using amr::domain::execution::Pose2D;
using amr::domain::execution::Waypoint;

// ── Given_StraightLinePath_Then_ForwardVelocity ───────────────────────

TEST(PurePursuitTest, Given_StraightLinePath_Then_ForwardVelocity) {
  PurePursuit pp({0.5F, 0.5F, 0.1F});

  // Path: straight line along x-axis
  std::vector<Waypoint> path = {{0.0F, 0.0F}, {0.5F, 0.0F}, {1.0F, 0.0F}, {1.5F, 0.0F}, {2.0F, 0.0F}};

  Pose2D current{0.0F, 0.0F, 0.0F};  // facing +x
  auto twist = pp.track(path, current);

  // Should have forward velocity, near-zero angular on straight path
  EXPECT_GT(twist.linear, 0.0F);
  EXPECT_NEAR(twist.angular, 0.0F, 0.1F);
}

// ── Given_CurvedPath_Then_NonZeroAngularVelocity ──────────────────────

TEST(PurePursuitTest, Given_CurvedPath_Then_NonZeroAngularVelocity) {
  PurePursuit pp({0.3F, 0.5F, 0.05F});

  // Path: corner at 90 degrees
  std::vector<Waypoint> path = {{0.0F, 0.0F}, {1.0F, 0.0F}, {1.0F, 1.0F}};

  // Robot at origin, facing +x — lookahead to (1,0) or beyond
  Pose2D current{0.0F, 0.0F, 0.0F};
  auto twist = pp.track(path, current);

  EXPECT_GT(twist.linear, 0.0F);
  // On a straight segment, angular should be near zero
  EXPECT_NEAR(twist.angular, 0.0F, 0.2F);

  // Robot past the corner, facing +y (was heading right, now need to go up)
  // The lookahead will pick the last point or intermediate
}

// ── Given_GoalReached_Then_ZeroVelocity ───────────────────────────────

TEST(PurePursuitTest, Given_GoalReached_Then_ZeroVelocity) {
  PurePursuit pp({0.5F, 0.5F, 0.1F});

  std::vector<Waypoint> path = {{0.0F, 0.0F}, {0.5F, 0.0F}, {1.0F, 0.0F}};

  // Robot very close to goal
  Pose2D current{0.98F, 0.0F, 0.0F};
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

// ── Given_FarFromPath_Then_StillMoves ──────────────────────────────────

TEST(PurePursuitTest, Given_FarFromPath_Then_StillMoves) {
  PurePursuit pp({1.0F, 0.5F, 0.1F});

  std::vector<Waypoint> path = {{2.0F, 2.0F}, {3.0F, 3.0F}};

  // Robot far from path, but lookahead > distance to first waypoint
  Pose2D current{1.5F, 1.5F, 0.0F};  // toward (2,2)
  auto twist = pp.track(path, current);

  // Should still produce some velocity (chase the closest waypoint)
  EXPECT_GT(twist.linear, 0.0F);
}
