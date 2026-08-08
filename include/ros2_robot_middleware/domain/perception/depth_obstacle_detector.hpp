#pragma once
/// @file   depth_obstacle_detector.hpp
/// @brief  Camera depth scanline → low-obstacle clusters (lidar blind-spot fill).
///
/// The depth camera produces a 1D horizontal scanline (one range per ray over
/// a forward FOV). This detector treats it as a mini-2D-lidar: consecutive
/// valid ranges are clustered into obstacle candidates, then merged with the
/// lidar clusters (dedup — lidar is more accurate, so a depth cluster that
/// overlaps a lidar cluster is dropped).
///
/// Pure domain logic, zero ROS2, no members — all static.

#include "ros2_robot_middleware/domain/perception/icluster_algorithm.hpp"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace amr {
namespace domain {
namespace perception {

struct DepthObstacleParams {
  size_t   num_rays        = 121;
  float    fov_rad         = 1.0472F;   // 60°
  float    min_depth_m     = 0.2F;
  float    max_depth_m     = 5.0F;
  uint16_t invalid_depth   = 0;         // sentinel: no return
  int      min_run_length  = 3;         // min consecutive valid rays to form a cluster
  float    depth_break_m   = 0.3F;      // adjacent-depth gap > this → split (wall behind)
  float    camera_x = 0.0F, camera_y = 0.0F;  // camera mount offset in body frame
};

/// Depth scanline → clusters. Camera-detected obstacles carry category="low"
/// (they are blind spots for the 2D lidar, e.g. low steps / pallet feet).
class DepthObstacleDetector {
public:
  static std::vector<Cluster> detect(const std::vector<uint16_t> &depth_mm,
                                     const DepthObstacleParams &p = {}) {
    if (depth_mm.size() != p.num_rays) return {};
    if (p.num_rays < 2) return {};

    const float min_mm = p.min_depth_m * 1000.0F;
    const float max_mm = p.max_depth_m * 1000.0F;

    std::vector<Cluster> clusters;
    std::vector<std::pair<float, float>> run;  // (x, y) in body frame
    float last_valid_mm = 0.0F;

    const auto flush = [&]() {
      if (static_cast<int>(run.size()) >= p.min_run_length) {
        float cx = 0.0F, cy = 0.0F;
        for (const auto &pt : run) { cx += pt.first; cy += pt.second; }
        Cluster c;
        c.x = cx / static_cast<float>(run.size());
        c.y = cy / static_cast<float>(run.size());
        c.z = 0.0F;
        c.point_count = static_cast<int>(run.size());
        c.id = "cam_" + std::to_string(clusters.size());
        c.category = "low";
        clusters.push_back(c);
      }
      run.clear();
    };

    for (size_t i = 0; i < p.num_rays; ++i) {
      const float d_mm = static_cast<float>(depth_mm[i]);
      const bool valid = (d_mm != 0.0F) && (d_mm > min_mm) && (d_mm < max_mm);

      if (!valid) { flush(); last_valid_mm = 0.0F; continue; }

      const float theta = -p.fov_rad / 2.0F +
                          static_cast<float>(i) * (p.fov_rad / static_cast<float>(p.num_rays - 1));
      const float d_m = d_mm / 1000.0F;
      const float px = p.camera_x + d_m * std::cos(theta);
      const float py = p.camera_y + d_m * std::sin(theta);

      // Split on depth discontinuity: an obstacle in front of a wall produces
      // two distinct ranges → two clusters, not one wall-fused blob.
      if (!run.empty() && std::fabs(d_mm - last_valid_mm) > p.depth_break_m * 1000.0F) {
        flush();
      }
      run.emplace_back(px, py);
      last_valid_mm = d_mm;
    }
    flush();
    return clusters;
  }

  /// Merge lidar clusters with depth clusters — dedup by centroid distance.
  /// Lidar is the reference (higher accuracy); overlapping depth clusters drop.
  static std::vector<Cluster> merge(const std::vector<Cluster> &lidar,
                                    const std::vector<Cluster> &depth,
                                    float merge_radius = 0.4F) {
    std::vector<Cluster> out = lidar;
    for (const auto &dc : depth) {
      bool overlap = false;
      for (const auto &lc : lidar) {
        const float dx = dc.x - lc.x;
        const float dy = dc.y - lc.y;
        if (std::sqrt(dx * dx + dy * dy) <= merge_radius) { overlap = true; break; }
      }
      if (!overlap) out.push_back(dc);
    }
    return out;
  }
};

}  // namespace perception
}  // namespace domain
}  // namespace amr
