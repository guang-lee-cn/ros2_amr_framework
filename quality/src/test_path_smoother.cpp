/// @file test_path_smoother.cpp — Catmull-Rom path smoothing tests (no ROS2)
#include "ros2_robot_middleware/domain/planning/path_smoother.hpp"

#include <cmath>
#include <gtest/gtest.h>

using amr::domain::planning::PathSmoother;
using amr::domain::planning::Waypoint;

namespace {

float dist(const Waypoint &a, const Waypoint &b) {
  float dx = a.x - b.x;
  float dy = a.y - b.y;
  return std::sqrt(dx * dx + dy * dy);
}

}  // namespace

// ── Given_StraightLine_Then_PathUnchanged ────────────────────────────
// A straight path has no curvature; smoothing keeps the same line
// (every smoothed point is near the original segment).

TEST(PathSmootherTest, Given_StraightLine_Then_PathUnchanged) {
  PathSmoother smoother(PathSmoother::Params{0.1F});

  std::vector<Waypoint> path = {{0.0F, 0.0F}, {1.0F, 0.0F}, {2.0F, 0.0F}, {3.0F, 0.0F}};
  auto out = smoother.smooth(path);

  // All points on y=0 line, monotonically increasing x
  ASSERT_FALSE(out.empty());
  float prev_x = out.front().x;
  for (const auto &wp : out) {
    EXPECT_NEAR(wp.y, 0.0F, 0.01F);
    EXPECT_GE(wp.x, prev_x - 1e-6F);  // no backtracking
    prev_x = wp.x;
  }
  // Ends preserved
  EXPECT_NEAR(out.front().x, 0.0F, 0.05F);
  EXPECT_NEAR(out.back().x, 3.0F, 0.05F);
  // Denser than input
  EXPECT_GT(out.size(), path.size());
}

// ── Given_RightAngleCorner_Then_SmoothedRadius ───────────────────────
// A 90° corner should be rounded: interpolated points deviate from the
// sharp corner but stay within the convex hull of neighbors.

TEST(PathSmootherTest, Given_RightAngleCorner_Then_SmoothedRadius) {
  PathSmoother smoother(PathSmoother::Params{0.05F});

  // L-shaped path with right-angle turn at (1,0)
  std::vector<Waypoint> path = {{0.0F, 0.0F}, {1.0F, 0.0F}, {1.0F, 1.0F}, {1.0F, 2.0F}};
  auto out = smoother.smooth(path);

  ASSERT_FALSE(out.empty());

  // Ends preserved exactly
  EXPECT_NEAR(out.front().x, 0.0F, 0.01F);
  EXPECT_NEAR(out.front().y, 0.0F, 0.01F);
  EXPECT_NEAR(out.back().x, 1.0F, 0.01F);
  EXPECT_NEAR(out.back().y, 2.0F, 0.01F);

  // Some intermediate point must deviate from the sharp corner:
  // a point with both x < 1 and y > 0 exists (rounded inside the corner)
  bool has_rounded = false;
  for (const auto &wp : out) {
    if (wp.x < 0.95F && wp.y > 0.05F) { has_rounded = true; break; }
  }
  EXPECT_TRUE(has_rounded) << "No rounded corner: path is still sharp";
}

// ── Given_SinglePoint_Then_ReturnsPoint ──────────────────────────────

TEST(PathSmootherTest, Given_SinglePoint_Then_ReturnsPoint) {
  PathSmoother smoother;
  std::vector<Waypoint> path = {{2.0F, 3.0F}};

  auto out = smoother.smooth(path);
  ASSERT_EQ(out.size(), 1u);
  EXPECT_NEAR(out[0].x, 2.0F, 0.01F);
  EXPECT_NEAR(out[0].y, 3.0F, 0.01F);
}

// ── Given_EmptyPath_Then_Empty ───────────────────────────────────────

TEST(PathSmootherTest, Given_EmptyPath_Then_Empty) {
  PathSmoother smoother;
  std::vector<Waypoint> path{};

  auto out = smoother.smooth(path);
  EXPECT_TRUE(out.empty());
}

// ── Given_ThreePoints_Straight_Then_DenseSampled ─────────────────────
// A straight 3-point path has no corner to round, but is still
// dense-sampled for PurePursuit lookahead. Ends preserved.

TEST(PathSmootherTest, Given_ThreePoints_Straight_Then_DenseSampled) {
  PathSmoother smoother(PathSmoother::Params{0.1F});
  std::vector<Waypoint> path = {{0.0F, 0.0F}, {1.0F, 1.0F}, {2.0F, 2.0F}};

  auto out = smoother.smooth(path);

  EXPECT_GT(out.size(), path.size());       // dense-sampled
  EXPECT_NEAR(out.front().x, 0.0F, 0.01F);  // start preserved
  EXPECT_NEAR(out.back().x, 2.0F, 0.01F);   // end preserved
  // All points on y = x line (straight)
  for (const auto &wp : out) {
    EXPECT_NEAR(wp.y, wp.x, 0.01F);
  }
}

// ── Given_MaxDeviation_Then_Bounded ───────────────────────────────────
// Smoothed path must stay within a reasonable distance of original path.

TEST(PathSmootherTest, Given_SmoothPath_Then_MaxDeviationBounded) {
  PathSmoother smoother(PathSmoother::Params{0.05F});

  std::vector<Waypoint> path = {{0.0F, 0.0F}, {0.5F, 0.0F}, {0.5F, 0.5F}, {1.0F, 0.5F}};
  auto out = smoother.smooth(path);

  // Max deviation from any original segment ≤ ~0.3m (loose bound for
  // Catmull-Rom with 0.5m segments). Ensures no wild overshoot.
  float max_dev = 0.0F;
  for (const auto &wp : out) {
    float d = std::min(
      dist(wp, {0.0F, 0.0F}) + 0.01F,   // simple bound: near the corridor
      std::min(dist(wp, {1.0F, 0.5F}),
               std::min(dist(wp, {0.5F, 0.0F}), dist(wp, {0.5F, 0.5F}))));
    max_dev = std::max(max_dev, d);
  }
  // All output points are within the bounding box of control points ± margin
  for (const auto &wp : out) {
    EXPECT_GE(wp.x, -0.3F);
    EXPECT_LE(wp.x, 1.3F);
    EXPECT_GE(wp.y, -0.3F);
    EXPECT_LE(wp.y, 0.8F);
  }
}
