/// @file test_dbscan.cpp — DBSCAN cluster detector unit tests (no ROS2)
#include "ros2_robot_middleware/domain/perception/cluster_detector.hpp"

#include <cmath>
#include <gtest/gtest.h>

using amr::domain::perception::ClusterDetector;

// ── Single cluster ───────────────────────────────────────────────────

TEST(DbscanTest, SingleIsolatedCluster) {
  float ranges[360] = {};
  // Tight cluster at 0° ± 5° at 2m range → centroid near (2.0, 0.0)
  for (int i = 175; i <= 185; ++i) ranges[i] = 2.0F;

  ClusterDetector detector;
  auto clusters = detector.detect(ranges, -M_PI, 2.0F * M_PI / 360.0F);

  ASSERT_GE(clusters.size(), 1u);
  EXPECT_NEAR(clusters[0].x, 2.0F, 0.3F);   // range=2m at 0°
  EXPECT_NEAR(clusters[0].y, 0.0F, 0.3F);
  EXPECT_GT(clusters[0].point_count, 0);
}

// ── Two separated clusters ───────────────────────────────────────────

TEST(DbscanTest, TwoSeparatedClusters) {
  float ranges[360] = {};

  // Cluster 1: at 45°, 1.5m
  for (int i = 40; i <= 50; ++i) ranges[i] = 1.5F;

  // Cluster 2: at 135°, 2.0m (clearly separated)
  for (int i = 130; i <= 140; ++i) ranges[i] = 2.0F;

  ClusterDetector detector;
  auto clusters = detector.detect(ranges, -M_PI, 2.0F * M_PI / 360.0F);

  EXPECT_GE(clusters.size(), 2u);

  // Verify the two clusters are in different spatial locations
  if (clusters.size() >= 2) {
    float dx = clusters[0].x - clusters[1].x;
    float dy = clusters[0].y - clusters[1].y;
    float dist = std::sqrt(dx * dx + dy * dy);
    EXPECT_GT(dist, 0.5F) << "Two clusters should be spatially separated";
  }
}

// ── Noise rejection ──────────────────────────────────────────────────

TEST(DbscanTest, IsolatedPoint_RejectedAsNoise) {
  float ranges[360] = {};
  // Single point at 2m — only 1 point, below min_pts=5
  ranges[180] = 2.0F;

  ClusterDetector detector;
  auto clusters = detector.detect(ranges, -M_PI, 2.0F * M_PI / 360.0F);

  // A single point should NOT form a cluster
  EXPECT_EQ(clusters.size(), 0u);
}

// ── Custom params ────────────────────────────────────────────────────

TEST(DbscanTest, CustomEps_DetectsTightCluster) {
  // eps=0.1m — tighter than default, requires closer points
  ClusterDetector::Params p;
  p.eps = 0.1F;
  p.min_pts = 3;  // need 3 neighbors (= 4 points total) to form cluster
  ClusterDetector detector(p);

  // 3 points at 2m, 1° apart → Cartesian distance ~0.035m < eps=0.1m
  // neighbors=2, with +1 fix → 3 >= min_pts(3) → core point
  float ranges[360] = {};
  ranges[90] = 2.0F;
  ranges[91] = 2.0F;
  ranges[92] = 2.0F;

  auto clusters = detector.detect(ranges, -M_PI, 2.0F * M_PI / 360.0F);
  EXPECT_GE(clusters.size(), 1u);
}

// ── Empty input ──────────────────────────────────────────────────────

TEST(DbscanTest, NoValidRanges_ReturnsEmpty) {
  float ranges[360] = {};  // all zero → below min_range
  ClusterDetector detector;
  auto clusters = detector.detect(ranges, -M_PI, 2.0F * M_PI / 360.0F);
  EXPECT_TRUE(clusters.empty());
}

// ── Max objects limit ────────────────────────────────────────────────

TEST(DbscanTest, RespectsMaxObjects) {
  ClusterDetector::Params p;
  p.max_objects = 2;
  p.eps = 2.0F;    // loose eps to make clustering easy
  p.min_pts = 3;
  ClusterDetector detector(p);

  // Create 4 distinct clusters
  float ranges[360] = {};
  for (int i = 40; i <= 46; ++i) ranges[i] = 1.0F;    // cluster 1
  for (int i = 130; i <= 136; ++i) ranges[i] = 1.5F;   // cluster 2
  for (int i = 220; i <= 226; ++i) ranges[i] = 2.0F;   // cluster 3
  for (int i = 310; i <= 316; ++i) ranges[i] = 2.5F;   // cluster 4

  auto clusters = detector.detect(ranges, -M_PI, 2.0F * M_PI / 360.0F);
  EXPECT_LE(clusters.size(), 2u) << "Should respect max_objects=2";
}
