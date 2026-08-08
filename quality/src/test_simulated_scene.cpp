/// @file test_simulated_scene.cpp — SimulatedScene ray-cast scan + odom integration
#include "ros2_robot_middleware/domain/simulation/simulated_scene.hpp"

#include <cmath>
#include <gtest/gtest.h>

using amr::domain::simulation::SimulatedScene;
using amr::domain::simulation::Pose;

namespace {

// Index of the 0° beam (robot heading). Beam i → angle = heading + i·(2π/360).
constexpr int kZero = 0;

float beam(const std::vector<float> &r, int idx) { return r[idx]; }

}  // namespace

class SimulatedSceneTest : public ::testing::Test {
protected:
  SimulatedScene scene_;
};

// ── GivenRobotFacingEast_WhenGenerateScan_ZeroDegreeBeamHitsEastWall ────
// Robot at (10,-1.5) heading +x (clear of box at y=0; east wall within
// range_max=10). 0° beam from lidar (10.4,-1.5) hits east wall x=19 → 8.6.

TEST_F(SimulatedSceneTest,
       GivenRobotFacingEast_WhenGenerateScan_ZeroDegreeBeamHitsEastWall) {
  auto r = scene_.generate_scan(10.0F, -1.5F, 0.0F);
  ASSERT_EQ(r.size(), 360u);
  EXPECT_NEAR(beam(r, kZero), 8.6F, 0.1F);
}

// ── GivenRobotFacingWest_WhenGenerateScan_PiBeamHitsWestWall ────────────
// Robot at (5,0) heading -x. The beam at heading (180°) from lidar (4.6,0)
// westward hits the west wall x=0 → range ≈ 4.6.

TEST_F(SimulatedSceneTest,
       GivenRobotFacingWest_WhenGenerateScan_HeadingBeamHitsWestWall) {
  auto r = scene_.generate_scan(5.0F, 0.0F, static_cast<float>(M_PI));
  EXPECT_NEAR(beam(r, kZero), 4.6F, 0.1F);
}

// ── GivenObstacleEast_WhenGenerateScan_HeadingBeamShort ─────────────────
// Box obstacle centered (8,0). Robot at origin heading east: lidar (0.4,0),
// box front edge x=7.75 → heading beam ≈ 7.35 (shorter than the 19m wall).

TEST_F(SimulatedSceneTest,
       GivenObstacleEast_WhenGenerateScan_HeadingBeamIsShort) {
  auto r = scene_.generate_scan(0.0F, 0.0F, 0.0F);
  EXPECT_NEAR(beam(r, kZero), 7.35F, 0.1F);
}

// ── GivenOpenDirection_WhenGenerateScan_BeamIsAtRangeMax ────────────────
// Within the warehouse every direction hits a wall or obstacle (enclosed) —
// every beam must be finite. This guards against scan holes.

TEST_F(SimulatedSceneTest, GivenWarehouse_WhenGenerateScan_AllBeamsFinite) {
  auto r = scene_.generate_scan(5.0F, 0.0F, 0.0F);
  for (float v : r) {
    EXPECT_LT(v, scene_.params().range_max + 0.01F);
  }
}

// ── GivenForwardCommand_WhenStep_PoseAdvances ───────────────────────────
// v=0.5, dt=0.1 → x += 0.05 along heading.

TEST_F(SimulatedSceneTest, GivenForwardCommand_WhenStep_PoseAdvances) {
  auto p = SimulatedScene::step({0.0F, 0.0F, 0.0F}, 0.5F, 0.0F, 0.1F);
  EXPECT_NEAR(p.x, 0.05F, 1e-4);
  EXPECT_NEAR(p.y, 0.0F, 1e-4);
}

// ── GivenRotationCommand_WhenStep_HeadingChanges ────────────────────────
// w=1.0, dt=0.1 → theta += 0.1.

TEST_F(SimulatedSceneTest, GivenRotationCommand_WhenStep_HeadingChanges) {
  auto p = SimulatedScene::step({0.0F, 0.0F, 0.0F}, 0.0F, 1.0F, 0.1F);
  EXPECT_NEAR(p.theta, 0.1F, 1e-4);
}
