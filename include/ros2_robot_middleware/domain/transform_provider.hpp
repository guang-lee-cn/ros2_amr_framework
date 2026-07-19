#pragma once
/// @file   transform_provider.hpp
/// @brief  Abstract coordinate transform interface (DDD domain layer).
///
/// Pure C++ — zero ROS2 dependency. Infrastructure layer provides
/// tf2-based implementation (Tf2TransformProvider).

#include "ros2_robot_middleware/domain/perception/sensor_interface.hpp"

#include <string>

namespace amr::domain {

class ITransformProvider {
public:
  virtual ~ITransformProvider() = default;

  /// Transform a LiDAR scan from its native frame to target_frame.
  /// Each point (range, angle) is rotated and translated by the
  /// rigid-body transform obtained from the TF tree.
  ///
  /// Returns true on success, false if transform is unavailable.
  virtual bool transform_scan(const sensor::LidarScan &in,
                               sensor::LidarScan &out,
                               const std::string &target_frame) = 0;
};

} // namespace amr::domain
