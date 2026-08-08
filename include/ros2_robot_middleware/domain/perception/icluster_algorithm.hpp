#ifndef ROS2_ROBOT_MIDDLEWARE_DOMAIN_ICLUSTER_ALGORITHM_HPP_
#define ROS2_ROBOT_MIDDLEWARE_DOMAIN_ICLUSTER_ALGORITHM_HPP_

/// @file   icluster_algorithm.hpp
/// @brief  Strategy interface for LiDAR clustering backends.
///         Swappable: DBSCAN ↔ PCL ↔ custom.

#include <string>
#include <vector>

namespace amr {
namespace domain {
namespace perception {

/// Cluster centroid — shared across all clustering backends.
struct Cluster {
  float x = 0.0F;
  float y = 0.0F;
  float z = 0.0F;
  int   point_count = 0;
  std::string id;
  /// 语义类别（预留：相机识别接入后填充；深度低矮障碍 = "low"）。
  std::string category;
};

class IClusterAlgorithm {
public:
  virtual ~IClusterAlgorithm() = default;
  virtual std::vector<Cluster> detect(const float ranges[],
                                       float angle_min,
                                       float angle_increment) const = 0;
};

}  // namespace perception
}  // namespace domain
}  // namespace amr

#endif
