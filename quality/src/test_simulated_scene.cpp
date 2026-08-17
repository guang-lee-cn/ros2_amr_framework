/// @file test_simulated_scene.cpp — SimulatedScene ray-cast scan + odom integration
#include "ros2_robot_middleware/domain/simulation/simulated_scene.hpp"

#include <cmath>
#include <gtest/gtest.h>

using amr::domain::simulation::SimulatedScene;
using amr::domain::simulation::Pose;

namespace {

// Beam i → lidar-frame angle = -π + i·(2π/beam_count). The heading beam (0°)
// sits at i = beam_count/2 = 180; ranges[0] is the rear (-π) beam. This matches
// the ROS LaserScan convention (angle_min=-π, forward centred) that consumers
// rely on — collision_guard's forward FOV (|a|<fov_half) and cluster_detector
// both read forward at the centred index (B3).
constexpr int kBeamCount = 360;
constexpr int kForward = kBeamCount / 2;  // 0° (heading) beam index

float beam(const std::vector<float> &r, int idx) { return r[idx]; }

}  // namespace

class SimulatedSceneTest : public ::testing::Test {
protected:
  SimulatedScene scene_;
};

// ── GivenRobotFacingEast_WhenGenerateScan_HeadingBeamHitsEastWall ───────
// Robot at (10,-1.5) heading +x (clear of box at y=0; east wall within
// range_max=10). Heading beam (i=180) from lidar (10.25,-1.5) hits east wall
// x=19 → 8.75 (lidar_offset 0.25, aligned with amr.sdf chassis→lidar TF).

TEST_F(SimulatedSceneTest,
       GivenRobotFacingEast_WhenGenerateScan_HeadingBeamHitsEastWall) {
  auto r = scene_.generate_scan(10.0F, -1.5F, 0.0F);
  ASSERT_EQ(r.size(), 360u);
  EXPECT_NEAR(beam(r, kForward), 8.75F, 0.1F);
}

// ── GivenRobotFacingWest_WhenGenerateScan_HeadingBeamHitsWestWall ───────
// Robot at (5,0) heading -x. Heading beam (i=180) from lidar (4.75,0) westward
// hits the west wall x=0 → range ≈ 4.75 (lidar_offset 0.25).

TEST_F(SimulatedSceneTest,
       GivenRobotFacingWest_WhenGenerateScan_HeadingBeamHitsWestWall) {
  auto r = scene_.generate_scan(5.0F, 0.0F, static_cast<float>(M_PI));
  EXPECT_NEAR(beam(r, kForward), 4.75F, 0.1F);
}

// ── GivenObstacleEast_WhenGenerateScan_HeadingBeamShort ─────────────────
// Box obstacle centered (8,0). Robot at origin heading east: lidar (0.25,0),
// box front edge x=7.75 → heading beam ≈ 7.5 (shorter than the 19m wall).

TEST_F(SimulatedSceneTest,
       GivenObstacleEast_WhenGenerateScan_HeadingBeamIsShort) {
  auto r = scene_.generate_scan(0.0F, 0.0F, 0.0F);
  EXPECT_NEAR(beam(r, kForward), 7.5F, 0.1F);
}

// ── GivenRobotAtOrigin_WhenGenerateScan_RearBeamHitsWestWall ────────────
// ranges[0] is the rear (-π) beam: robot at origin heading +x, rear beam
// points -x → hits the west wall x=0. Lidar at (0.25,0) → range ≈ 0.25.
// Locks the angle_min=-π contract (B3): forward is NOT at index 0.

TEST_F(SimulatedSceneTest,
       GivenRobotAtOrigin_WhenGenerateScan_RearBeamHitsWestWall) {
  auto r = scene_.generate_scan(0.0F, 0.0F, 0.0F);
  EXPECT_NEAR(beam(r, 0), 0.25F, 0.1F);
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
