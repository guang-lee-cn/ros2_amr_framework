/// @file bench_cluster.cpp — DBSCAN vs PCL clustering performance comparison.
/// Compares tick() latency with both backends on the same simulated LiDAR data.
#include "ros2_robot_middleware/domain/perception/cluster_detector.hpp"
#include "ros2_robot_middleware/domain/perception/pcl_cluster_backend.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace amr::domain::perception;

static constexpr int kNumPoints = 360;
static constexpr float kAngleMin = -M_PI;
static constexpr float kAngleMax = M_PI;
static constexpr float kAngleInc = (kAngleMax - kAngleMin) / kNumPoints;

// Generate synthetic scan with N clusters
static void fill_scan(float ranges[], int num_clusters) {
  for (int i = 0; i < kNumPoints; ++i) {
    float angle = kAngleMin + i * kAngleInc;
    // Background at 5m
    float r = 5.0F;
    // Add cluster blobs
    for (int c = 0; c < num_clusters; ++c) {
      float center_angle = -2.5F + c * 1.2F;
      float dist = std::abs(angle - center_angle);
      if (dist < 0.15F) r = 2.0F + static_cast<float>(c) * 0.3F;
    }
    ranges[i] = r;
  }
}

template <typename Backend>
static void bench(const char *name, const Backend &backend, int num_clusters, int iterations) {
  float ranges[kNumPoints];
  fill_scan(ranges, num_clusters);

  auto t0 = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i) {
    auto clusters = backend.detect(ranges, kAngleMin, kAngleInc);
    (void)clusters;
  }
  auto t1 = std::chrono::steady_clock::now();
  auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();

  std::printf("%-10s clusters=%d  %5d iters  %6.0f us  avg %5.1f us/iter\n",
              name, num_clusters, iterations,
              static_cast<double>(us), static_cast<double>(us) / iterations);
}

int main() {
  ClusterDetector dbscan{{6.5F, 0.1F, 0.3F, 5, 10, 360}};
  PclClusterBackend pcl{{0.3F, 3, 250, 6.5F, 0.1F, 360}};

  std::printf("=== DBSCAN vs PCL Clustering Benchmark ===\n\n");
  std::printf("%-10s %-12s %-10s %-10s\n", "backend", "scenario", "avg(us)", "rel");
  std::printf("%s\n", std::string(45, '-').c_str());

  for (int clusters : {1, 3, 6, 10}) {
    const int iters = clusters <= 3 ? 5000 : 1000;

    {
      float ranges[kNumPoints];
      fill_scan(ranges, clusters);
      auto t0 = std::chrono::steady_clock::now();
      for (int i = 0; i < iters; ++i) dbscan.detect(ranges, kAngleMin, kAngleInc);
      auto t1 = std::chrono::steady_clock::now();
      auto dbus = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
      double db_avg = static_cast<double>(dbus) / iters;

      auto t2 = std::chrono::steady_clock::now();
      for (int i = 0; i < iters; ++i) pcl.detect(ranges, kAngleMin, kAngleInc);
      auto t3 = std::chrono::steady_clock::now();
      auto pclus = std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2).count();
      double pcl_avg = static_cast<double>(pclus) / iters;

      std::printf("%-10s %-12s %6.1f us  %s\n", "DBSCAN", std::to_string(clusters).c_str(), db_avg, "");
      std::printf("%-10s %-12s %6.1f us  %s\n", "PCL", "", pcl_avg, db_avg > 0 ? (std::to_string(pcl_avg / db_avg) + "x").c_str() : "");
    }
  }

  std::printf("\nDone.\n");
  return 0;
}
