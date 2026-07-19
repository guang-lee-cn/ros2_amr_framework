#ifndef ROS2_ROBOT_MIDDLEWARE_DOMAIN_CLUSTER_DETECTOR_HPP_
#define ROS2_ROBOT_MIDDLEWARE_DOMAIN_CLUSTER_DETECTOR_HPP_

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace amr {
namespace domain {
namespace perception {

/// 2D Cartesian point for DBSCAN input (polar→Cartesian conversion from ranges[])
struct Point2D {
  float x = 0.0F;
  float y = 0.0F;
  int   source_idx = -1;  // original range array index
};

/// Cluster with centroid + bounding-box metadata
struct Cluster {
  float x = 0.0F;         // centroid x (m)
  float y = 0.0F;         // centroid y (m)
  float z = 0.0F;         // z = 0 (2D LiDAR assumption)
  int   point_count = 0;  // number of points in cluster
  std::string id;
};

// ── DBSCAN clustering (Ester et al., 1996) ──────────────────────────
//
// Density-based: points within eps of each other form clusters.
// Points with < min_pts neighbors are labeled as noise.
//
// Complexity: O(n²) naive, O(n log n) with spatial index (future).
// For 360 LiDAR points, O(n²) = ~130k distance calc — < 1ms on ARM Cortex-A76.
//
// Parameters:
//   eps     = 0.3m — max distance between two points in the same cluster
//   min_pts = 5    — minimum points to form a cluster (at 1° res, ~5° span)
//
// Why replaces scan-line clustering:
//   - Works in Cartesian space → distance is metric, not angular
//   - Separates nearby objects (gap > eps → distinct clusters)
//   - Labels isolated reflections as noise (no phantom objects)
class ClusterDetector {
public:
  struct Params {
    float max_range      = 6.5F;   // sensor max range (m)
    float min_range      = 0.1F;   // sensor min range (m)
    float eps            = 0.3F;   // DBSCAN neighbor radius (m)
    int   min_pts        = 5;      // DBSCAN min points per cluster
    int   max_objects    = 10;     // max clusters to return
    int   num_points     = 360;    // expected ranges[] array size
  };

  ClusterDetector() = default;
  explicit ClusterDetector(const Params &p) : params_(p) {}

  // ── Core algorithm ─────────────────────────────────────────────────
  //
  // 1. Convert valid ranges → Cartesian points (filter min_range < r < max_range)
  // 2. DBSCAN on Cartesian points
  // 3. Compute centroid per cluster
  std::vector<Cluster> detect(const float ranges[],
                               float angle_min,
                               float angle_increment) const
  {
    // Step 1: polar → Cartesian
    std::vector<Point2D> points;
    points.reserve(params_.num_points);
    for (int i = 0; i < params_.num_points; ++i) {
      if (ranges[i] > params_.min_range && ranges[i] < params_.max_range) {
        float angle = angle_min + i * angle_increment;
        points.push_back({ranges[i] * std::cos(angle),
                          ranges[i] * std::sin(angle), i});
      }
    }

    const int n = static_cast<int>(points.size());
    if (n == 0) return {};

    // Step 2: DBSCAN
    std::vector<int> labels(n, -1);   // -1 = unvisited, -2 = noise, >=0 = cluster id
    int cluster_id = 0;

    for (int i = 0; i < n; ++i) {
      if (labels[i] != -1) continue;  // already processed

      auto neighbors = region_query(points, i);
      // min_pts includes the core point itself (standard DBSCAN: Ester et al. 1996).
      // region_query excludes self → need >= min_pts-1 neighbors.
      if (static_cast<int>(neighbors.size()) + 1 < params_.min_pts) {
        labels[i] = -2;  // noise
        continue;
      }

      // Start new cluster
      labels[i] = cluster_id;
      for (size_t j = 0; j < neighbors.size(); ++j) {
        int q = neighbors[j];
        if (labels[q] == -2) labels[q] = cluster_id;  // noise → border point
        if (labels[q] != -1) continue;                 // already in a cluster
        labels[q] = cluster_id;

        auto q_neighbors = region_query(points, q);
        if (static_cast<int>(q_neighbors.size()) >= params_.min_pts) {
          // Merge q's neighbors into current expansion set
          neighbors.insert(neighbors.end(), q_neighbors.begin(), q_neighbors.end());
        }
      }
      ++cluster_id;
      if (cluster_id >= params_.max_objects) break;
    }

    // Step 3: centroid per cluster
    std::vector<Cluster> clusters(cluster_id);
    std::vector<float> sum_x(cluster_id, 0.0F);
    std::vector<float> sum_y(cluster_id, 0.0F);
    std::vector<int>   cnt(cluster_id, 0);

    for (int i = 0; i < n; ++i) {
      if (labels[i] >= 0) {
        int cid = labels[i];
        sum_x[cid] += points[i].x;
        sum_y[cid] += points[i].y;
        cnt[cid]++;
      }
    }

    for (int c = 0; c < cluster_id; ++c) {
      if (cnt[c] > 0) {
        clusters[c].x = sum_x[c] / cnt[c];
        clusters[c].y = sum_y[c] / cnt[c];
        clusters[c].z = 0.0F;
        clusters[c].point_count = cnt[c];
        clusters[c].id = "obj_" + std::to_string(c);
      }
    }

    return clusters;
  }

private:
  /// Find all points within eps of point[idx]
  std::vector<int> region_query(const std::vector<Point2D> &pts, int idx) const {
    std::vector<int> result;
    const float eps2 = params_.eps * params_.eps;
    for (int i = 0; i < static_cast<int>(pts.size()); ++i) {
      if (i == idx) continue;
      float dx = pts[idx].x - pts[i].x;
      float dy = pts[idx].y - pts[i].y;
      if (dx * dx + dy * dy <= eps2) {
        result.push_back(i);
      }
    }
    return result;
  }

  Params params_;
};

}  // namespace perception
}  // namespace domain
}  // namespace amr

#endif
