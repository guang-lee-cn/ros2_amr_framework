/// @file test_track_error.cpp — Lateral tracking error monitor tests (no ROS2)
#include "ros2_robot_middleware/domain/planning/track_error_monitor.hpp"

#include <gtest/gtest.h>

using amr::domain::planning::TrackErrorMonitor;
using amr::domain::planning::TrackErrorLevel;
using amr::domain::execution::Pose2D;
using amr::domain::execution::Waypoint;

namespace {

// Straight path along x-axis from (0,0) to (3,0)
std::vector<Waypoint> straight_path() {
  return {{0.0F, 0.0F}, {1.0F, 0.0F}, {2.0F, 0.0F}, {3.0F, 0.0F}};
}

}  // namespace

// ── Given_OnPath_Then_ZeroError ──────────────────────────────────────

TEST(TrackErrorTest, Given_OnPath_Then_ZeroError_FullSpeed) {
  TrackErrorMonitor mon(TrackErrorMonitor::Params{0.15F, 0.40F});

  Pose2D on_path{1.5F, 0.0F, 0.0F};
  auto st = mon.evaluate(on_path, straight_path());

  EXPECT_NEAR(st.lateral_error, 0.0F, 1e-4F);
  EXPECT_EQ(st.level, TrackErrorLevel::OK);
  EXPECT_NEAR(st.speed_scale, 1.0F, 1e-4F);
}

// ── Given_SmallDeviation_Then_WarnAndSlow ────────────────────────────

TEST(TrackErrorTest, Given_SmallDeviation_Then_WarnAndSlow) {
  TrackErrorMonitor mon(TrackErrorMonitor::Params{0.15F, 0.40F});

  // 0.2m off path (between warn 0.15 and stop 0.40)
  Pose2D off{1.5F, 0.2F, 0.0F};
  auto st = mon.evaluate(off, straight_path());

  EXPECT_EQ(st.level, TrackErrorLevel::WARN);
  // speed_scale = 1 - (0.2-0.15)/(0.40-0.15) = 1 - 0.2 = 0.8
  EXPECT_NEAR(st.speed_scale, 0.8F, 1e-3F);
  EXPECT_GT(st.lateral_error, 0.0F);  // left of path
}

// ── Given_LargeDeviation_Then_Stop ───────────────────────────────────

TEST(TrackErrorTest, Given_LargeDeviation_Then_Stop) {
  TrackErrorMonitor mon(TrackErrorMonitor::Params{0.15F, 0.40F});

  // 0.5m off path — beyond stop threshold
  Pose2D way_off{1.5F, 0.5F, 0.0F};
  auto st = mon.evaluate(way_off, straight_path());

  EXPECT_EQ(st.level, TrackErrorLevel::ERROR);
  EXPECT_FLOAT_EQ(st.speed_scale, 0.0F);
}

// ── Given_NegativeDeviation_Then_RightSideHandled ────────────────────
// Deviation to the right of path direction → negative lateral error,
// same thresholds apply (absolute value).

TEST(TrackErrorTest, Given_NegativeDeviation_Then_Handled) {
  TrackErrorMonitor mon(TrackErrorMonitor::Params{0.15F, 0.40F});

  Pose2D off_right{1.5F, -0.2F, 0.0F};
  auto st = mon.evaluate(off_right, straight_path());

  EXPECT_EQ(st.level, TrackErrorLevel::WARN);
  EXPECT_LT(st.lateral_error, 0.0F);  // right of path
  EXPECT_NEAR(st.speed_scale, 0.8F, 1e-3F);
}

// ── Given_EmptyPath_Then_Stop ────────────────────────────────────────

TEST(TrackErrorTest, Given_EmptyPath_Then_Stop) {
  TrackErrorMonitor mon;
  Pose2D p{0.0F, 0.0F, 0.0F};
  auto st = mon.evaluate(p, {});

  EXPECT_EQ(st.level, TrackErrorLevel::ERROR);
  EXPECT_FLOAT_EQ(st.speed_scale, 0.0F);
}

// ── Given_SinglePointPath_Then_Stop ──────────────────────────────────

TEST(TrackErrorTest, Given_SinglePointPath_Then_Stop) {
  TrackErrorMonitor mon;
  Pose2D p{1.0F, 1.0F, 0.0F};
  auto st = mon.evaluate(p, {{1.0F, 1.0F}});

  EXPECT_EQ(st.level, TrackErrorLevel::ERROR);
  EXPECT_FLOAT_EQ(st.speed_scale, 0.0F);
}

// ── Given_OffsetOnCornerPath_Then_NearestSegmentUsed ─────────────────
// On an L-path, robot near the vertical segment measures to that segment,
// not the horizontal one.

TEST(TrackErrorTest, Given_OffsetOnCornerPath_Then_NearestSegment) {
  TrackErrorMonitor mon(TrackErrorMonitor::Params{0.15F, 0.40F});

  // L-path: (0,0)→(1,0)→(1,2)
  std::vector<Waypoint> path = {{0.0F, 0.0F}, {1.0F, 0.0F}, {1.0F, 2.0F}};

  // Robot near vertical segment at x=1, offset left by 0.2 → x=0.8, y=1.5
  Pose2D p{0.8F, 1.5F, 0.0F};
  auto st = mon.evaluate(p, path);

  // Distance to vertical segment (x=1, y∈[0,2]) from (0.8,1.5) = 0.2
  EXPECT_NEAR(std::fabs(st.lateral_error), 0.2F, 1e-3F);
  EXPECT_EQ(st.level, TrackErrorLevel::WARN);
}
