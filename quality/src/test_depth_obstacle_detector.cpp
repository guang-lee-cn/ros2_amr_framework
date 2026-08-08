/// @file test_depth_obstacle_detector.cpp — depth scanline → obstacle clustering
#include "ros2_robot_middleware/domain/perception/depth_obstacle_detector.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

using amr::domain::perception::Cluster;
using amr::domain::perception::DepthObstacleDetector;
using amr::domain::perception::DepthObstacleParams;

namespace {

constexpr size_t kNumRays = 121;

/// Build a depth scanline with a flat obstacle: rays in [begin,end) get d_mm,
/// the rest are invalid (0). This simulates a front-facing obstacle span.
std::vector<uint16_t> make_flat_obstacle(size_t begin, size_t end, float d_m) {
  std::vector<uint16_t> depth(kNumRays, 0U);
  const uint16_t mm = static_cast<uint16_t>(d_m * 1000.0F);
  for (size_t i = begin; i < end; ++i) depth[i] = mm;
  return depth;
}

}  // namespace

// ── detect() ─────────────────────────────────────────────────────────

TEST(DepthObstacleDetectorTest, Given_ObstacleDeadAhead_When_Detect_ReturnsSingleCluster) {
  // Obstacle spans rays 50..70 centered ahead (FOV 60°, ray 60 is dead-ahead).
  auto depth = make_flat_obstacle(50, 70, 1.5F);
  auto clusters = DepthObstacleDetector::detect(depth);
  ASSERT_EQ(clusters.size(), 1u);
  // Centroid must be near the forward axis: |y| small, x ≈ 1.5.
  EXPECT_NEAR(clusters[0].x, 1.5F, 0.1F);
  EXPECT_NEAR(clusters[0].y, 0.0F, 0.15F);
  EXPECT_EQ(clusters[0].category, "low");
  EXPECT_GT(clusters[0].point_count, 0);
}

TEST(DepthObstacleDetectorTest, Given_NoValidDepth_When_Detect_ReturnsEmpty) {
  std::vector<uint16_t> depth(kNumRays, 0U);  // all invalid
  EXPECT_TRUE(DepthObstacleDetector::detect(depth).empty());
}

TEST(DepthObstacleDetectorTest, Given_TwoObstaclesInFOV_When_Detect_ReturnsTwoClusters) {
  auto depth = make_flat_obstacle(20, 35, 1.0F);   // left rays → negative θ
  auto right = make_flat_obstacle(85, 100, 2.0F);  // right rays → positive θ
  for (size_t i = 85; i < 100; ++i) depth[i] = right[i];
  auto clusters = DepthObstacleDetector::detect(depth);
  ASSERT_EQ(clusters.size(), 2u);
  // θ = -fov/2 + i*step: low-index rays (left) have θ<0 → y<0; right rays θ>0 → y>0.
  EXPECT_LT(clusters[0].y, 0.0F);
  EXPECT_GT(clusters[1].y, 0.0F);
}

TEST(DepthObstacleDetectorTest, Given_ObstacleInFrontOfWall_When_Detect_SplitByDepthBreak) {
  // Near obstacle at 1.0m spanning 40..48, wall at 3.0m spanning 49..58.
  // Depth discontinuity 2m > depth_break 0.3 → two clusters, not one wall blob.
  auto depth = make_flat_obstacle(40, 48, 1.0F);
  for (size_t i = 49; i < 58; ++i) depth[i] = static_cast<uint16_t>(3000U);
  auto clusters = DepthObstacleDetector::detect(depth);
  ASSERT_EQ(clusters.size(), 2u);
  EXPECT_NEAR(clusters[0].x, 1.0F, 0.1F);
  EXPECT_NEAR(clusters[1].x, 3.0F, 0.1F);
}

TEST(DepthObstacleDetectorTest, Given_RunBelowMinLength_When_Detect_Ignored) {
  DepthObstacleParams p;
  p.min_run_length = 3;
  auto depth = make_flat_obstacle(60, 62, 1.0F);  // 2 valid rays < 3
  auto clusters = DepthObstacleDetector::detect(depth, p);
  EXPECT_TRUE(clusters.empty());
}

TEST(DepthObstacleDetectorTest, Given_CentroidInBodyFrame_When_MountOffset_Translated) {
  DepthObstacleParams p;
  p.camera_x = 0.2F;
  auto depth = make_flat_obstacle(50, 70, 1.5F);
  auto clusters = DepthObstacleDetector::detect(depth, p);
  ASSERT_EQ(clusters.size(), 1u);
  // camera mount offset shifts the cluster x toward the robot.
  EXPECT_GT(clusters[0].x, 1.5F - 0.05F);
}

// ── merge() ──────────────────────────────────────────────────────────

TEST(DepthObstacleDetectorTest, Given_LidarAndDepthOverlap_When_Merge_DepthDropped) {
  Cluster lidar; lidar.x = 1.5F; lidar.y = 0.0F; lidar.id = "l0";
  Cluster depth; depth.x = 1.55F; depth.y = 0.05F; depth.id = "cam_0";
  auto out = DepthObstacleDetector::merge({lidar}, {depth});
  ASSERT_EQ(out.size(), 1u);
  EXPECT_EQ(out[0].id, "l0");  // lidar wins
}

TEST(DepthObstacleDetectorTest, Given_LidarAndDepthDistinct_When_Merge_BothKept) {
  Cluster lidar; lidar.x = 1.5F; lidar.y = 0.0F; lidar.id = "l0";
  Cluster depth; depth.x = 1.5F; depth.y = 2.0F; depth.id = "cam_0";  // far apart
  auto out = DepthObstacleDetector::merge({lidar}, {depth});
  ASSERT_EQ(out.size(), 2u);
}
