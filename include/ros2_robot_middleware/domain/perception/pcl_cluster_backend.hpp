#ifndef ROS2_ROBOT_MIDDLEWARE_DOMAIN_PCL_CLUSTER_BACKEND_HPP_
#define ROS2_ROBOT_MIDDLEWARE_DOMAIN_PCL_CLUSTER_BACKEND_HPP_

/// @file   pcl_cluster_backend.hpp
/// @brief  PCL EuclideanClusterExtraction backend for IClusterAlgorithm.
///
/// Uses pcl::EuclideanClusterExtraction with a fixed radius threshold.
/// For 360 LiDAR points this is comparable to DBSCAN in output but
/// benefits from PCL's SIMD-accelerated nearest-neighbor search.

#include "ros2_robot_middleware/domain/perception/icluster_algorithm.hpp"

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/segmentation/extract_clusters.h>

#include <cmath>
#include <string>
#include <vector>

namespace amr {
namespace domain {
namespace perception {

class PclClusterBackend : public IClusterAlgorithm {
public:
  struct Params {
    float cluster_tolerance;
    int   min_cluster_size;
    int   max_cluster_size;
    float max_range;
    float min_range;
    int   num_points;
  };

  PclClusterBackend() : params_{0.3F, 3, 250, 6.5F, 0.1F, 360} {}
  explicit PclClusterBackend(const Params &p) : params_(p) {}

  std::vector<Cluster> detect(const float ranges[],
                               float angle_min,
                               float angle_increment) const override
  {
    // Step 1: polar → Cartesian → PCL point cloud
    pcl::PointCloud<pcl::PointXYZ>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZ>());
    cloud->reserve(static_cast<size_t>(params_.num_points));

    for (int i = 0; i < params_.num_points; ++i) {
      if (ranges[i] > params_.min_range && ranges[i] < params_.max_range) {
        float angle = angle_min + static_cast<float>(i) * angle_increment;
        cloud->push_back(pcl::PointXYZ(
            ranges[i] * std::cos(angle),
            ranges[i] * std::sin(angle),
            0.0F));
      }
    }

    if (cloud->empty()) return {};

    // Step 2: PCL Euclidean Cluster Extraction
    pcl::search::KdTree<pcl::PointXYZ>::Ptr tree(new pcl::search::KdTree<pcl::PointXYZ>());
    tree->setInputCloud(cloud);

    pcl::EuclideanClusterExtraction<pcl::PointXYZ> ec;
    ec.setClusterTolerance(params_.cluster_tolerance);
    ec.setMinClusterSize(params_.min_cluster_size);
    ec.setMaxClusterSize(params_.max_cluster_size);
    ec.setSearchMethod(tree);
    ec.setInputCloud(cloud);

    std::vector<pcl::PointIndices> cluster_indices;
    ec.extract(cluster_indices);

    // Step 3: centroids
    std::vector<Cluster> clusters;
    clusters.reserve(cluster_indices.size());

    for (size_t idx = 0; idx < cluster_indices.size(); ++idx) {
      const auto &indices = cluster_indices[idx];
      if (indices.indices.empty()) continue;

      double cx = 0.0, cy = 0.0;
      for (const auto &pi : indices.indices) {
        cx += (*cloud)[pi].x;
        cy += (*cloud)[pi].y;
      }
      Cluster c;
      c.x = static_cast<float>(cx / static_cast<double>(indices.indices.size()));
      c.y = static_cast<float>(cy / static_cast<double>(indices.indices.size()));
      c.z = 0.0F;
      c.point_count = static_cast<int>(indices.indices.size());
      c.id = "p" + std::to_string(idx);
      clusters.push_back(c);
    }

    return clusters;
  }

  const Params &params() const { return params_; }

private:
  Params params_;
};

}  // namespace perception
}  // namespace domain
}  // namespace amr

#endif
