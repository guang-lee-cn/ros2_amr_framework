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

// ── 随机障碍：同种子同场景（部署可复现），异种子异场景 ──────────────────
TEST(SimulatedSceneRandom, GivenSameSeed_WhenConstruct_ThenSameBoxes) {
  amr::domain::simulation::SceneParams p;
  p.scene_name = "warehouse_open";
  p.random_boxes = 5;
  p.random_seed = 7u;
  SimulatedScene a(p), b(p);
  EXPECT_EQ(a.random_boxes(), b.random_boxes());
  EXPECT_EQ(a.random_boxes().size(), 5u);

  p.random_seed = 8u;
  SimulatedScene c(p);
  EXPECT_NE(a.random_boxes(), c.random_boxes());
}

// 随机箱不得落在出生点 2.5m 内（机器人出生 (2,0)）
TEST(SimulatedSceneRandom, GivenRandomBoxes_WhenPlaced_ThenAwayFromSpawn) {
  amr::domain::simulation::SceneParams p;
  p.scene_name = "warehouse_open";
  p.random_boxes = 8;
  for (unsigned seed = 1; seed <= 5; ++seed) {
    p.random_seed = seed;
    SimulatedScene s(p);
    for (const auto &[x, y] : s.random_boxes()) {
      EXPECT_GT(std::hypot(x - 2.0F, y), 2.5F) << "seed=" << seed;
    }
  }
}

// 随机箱必须被雷达看见：空场 + 1 箱（种子定位在 (10,0) 右侧路径上）→
// 东行机器人在箱前必有 < range_max 的回波变化
TEST(SimulatedSceneRandom, GivenRandomBox_WhenScanEast_ThenBoxIsVisible) {
  amr::domain::simulation::SceneParams p_empty;
  p_empty.scene_name = "warehouse_open";
  SimulatedScene empty(p_empty);
  amr::domain::simulation::SceneParams p_box = p_empty;
  p_box.random_boxes = 1;
  p_box.random_seed = 3u;
  SimulatedScene with_box(p_box);
  ASSERT_FALSE(with_box.random_boxes().empty());
  // 机器人在出生点 (2,0) 朝东扫：任何随机箱位置 (bx,by) ∈ [3,17]×[-4,4]
  // 都应在某一束上产生 < 10m 的回波差异
  const auto r0 = empty.generate_scan(2.0F, 0.0F, 0.0F);
  const auto r1 = with_box.generate_scan(2.0F, 0.0F, 0.0F);
  bool differs = false;
  for (std::size_t i = 0; i < r0.size(); ++i) {
    if (std::fabs(r0[i] - r1[i]) > 0.05F) { differs = true; break; }
  }
  EXPECT_TRUE(differs);
}

// ── 移动障碍：update 推进 + 碰墙反弹 ────────────────────────────────────
TEST(SimulatedSceneMover, GivenMover_WhenUpdate_ThenMovesAndBounces) {
  amr::domain::simulation::SceneParams p;
  p.scene_name = "warehouse_open";
  p.movers = 1;
  p.mover_speed = 1.0F;
  p.random_seed = 1u;
  SimulatedScene s(p);
  ASSERT_EQ(s.movers().size(), 1u);
  const auto m0 = s.movers()[0];
  // 推进 1s：位移 = 速度 × 1（未撞墙时）
  s.update(1.0F);
  const auto m1 = s.movers()[0];
  EXPECT_NEAR(std::hypot(m1.cx - m0.cx, m1.cy - m0.cy), 1.0F, 1e-3F);

  // 连续推进 60s：始终在场内 x∈(0,19) y∈(-5,5)（反弹保证）
  for (int i = 0; i < 1200; ++i) s.update(0.05F);
  const auto m2 = s.movers()[0];
  EXPECT_GT(m2.cx, 0.0F);
  EXPECT_LT(m2.cx, 19.0F);
  EXPECT_GT(m2.cy, -5.0F);
  EXPECT_LT(m2.cy, 5.0F);
}

// mover 必须被雷达看见：把 mover 推到机器人正东 3m 处，朝东回波 < 3.5m
TEST(SimulatedSceneMover, GivenMoverAhead_WhenScanForward_ThenDetected) {
  amr::domain::simulation::SceneParams p;
  p.scene_name = "warehouse_open";
  p.movers = 1;
  p.mover_speed = 0.5F;
  SimulatedScene s(p);
  ASSERT_EQ(s.movers().size(), 1u);
  // 用公共接口无法直接设位——通过 update 多步把 mover 推离出生点方向后，
  // 断言扫描与无 mover 场景存在差异（可见性即可，不锁定具体回波值）
  amr::domain::simulation::SceneParams p_none = p;
  p_none.movers = 0;
  SimulatedScene none(p_none);
  bool differs = false;
  for (int t = 0; t < 40 && !differs; ++t) {
    s.update(0.5F);
    const auto r0 = none.generate_scan(2.0F, 0.0F, 0.0F);
    const auto r1 = s.generate_scan(2.0F, 0.0F, 0.0F);
    for (std::size_t i = 0; i < r0.size(); ++i) {
      if (std::fabs(r0[i] - r1[i]) > 0.05F) { differs = true; break; }
    }
  }
  EXPECT_TRUE(differs);
}

// ── mover 避让机器人：任何时刻不进入 1.2m 邻域 ──────────────────────────
TEST(SimulatedSceneMover, GivenRobot_WhenMoversWander_ThenNeverOverlapRobot) {
  amr::domain::simulation::SceneParams p;
  p.scene_name = "warehouse_open";
  p.movers = 2;
  p.mover_speed = 1.5F;  // 高速也必须弹开
  SimulatedScene s(p);
  float rx = 2.0F, ry = 0.0F;  // 机器人出生点
  for (int i = 0; i < 4000; ++i) {  // 200s 模拟
    // 机器人也在动（斜穿场），mover 必须全程避让
    rx = 2.0F + 3.5F * std::sin(i * 0.005F);
    ry = 3.0F * std::sin(i * 0.003F);
    s.update(0.05F, rx, ry);
    for (const auto &m : s.movers()) {
      EXPECT_GT(std::hypot(m.cx - rx, m.cy - ry), 1.0F)
          << "i=" << i << " mover=(" << m.cx << "," << m.cy << ")";
    }
  }
}
