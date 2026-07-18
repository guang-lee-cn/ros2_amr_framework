#ifndef ROS2_ROBOT_MIDDLEWARE_APPLICATION_PERCEPTION_SERVICE_HPP_
#define ROS2_ROBOT_MIDDLEWARE_APPLICATION_PERCEPTION_SERVICE_HPP_

#include "ros2_robot_middleware/domain/perception/cluster_detector.hpp"
#include "ros2_robot_middleware/domain/perception/degradation_policy.hpp"
#include "ros2_robot_middleware/domain/perception/kalman_filter.hpp"

#include <vector>

namespace amr {
namespace application {

// PerceptionService orchestrates domain services for sensor fusion.
// Dependencies: ClusterDetector + KalmanFilter2D + DegradationPolicy.
// Pure C++ — zero ROS2 dependency, fully unit-testable.
class PerceptionService {
public:
  using Level    = amr::domain::perception::DegradationLevel;
  using Cluster  = amr::domain::perception::Cluster;
  using DegradationPolicy = amr::domain::perception::DegradationPolicy;
  using ClusterDetector   = amr::domain::perception::ClusterDetector;

  PerceptionService()
    : detector_()
    , policy_()
  {}

  explicit PerceptionService(
    const ClusterDetector::Params &cluster_params,
    const DegradationPolicy::Config &deg_config = DegradationPolicy::Config{})
    : detector_(cluster_params)
    , policy_(deg_config)
  {}

  // ── Sensor input (called from ROS2 callbacks) ──────────────────────
  void on_lidar(const float ranges[], float angle_min, float angle_inc) {
    lidar_ranges_    = ranges;
    lidar_angle_min_  = angle_min;
    lidar_angle_inc_  = angle_inc;
    lidar_age_s_      = 0.0;
  }

  void on_imu(double ax, double ay) {
    imu_ax_ = ax;
    imu_ay_ = ay;
    imu_age_s_ = 0.0;
  }

  void on_camera() {
    camera_age_s_ = 0.0;
  }

  // ── Time tick (called from timer, dt in seconds) ───────────────────
  void tick(double dt) {
    // Age all sensors
    lidar_age_s_  += dt;
    imu_age_s_    += dt;
    camera_age_s_ += dt;

    // KF predict step
    kf_.predict(dt, imu_ax_, imu_ay_);
  }

  // ── Evaluate degradation ───────────────────────────────────────────
  Level evaluate_degradation() const {
    return policy_.evaluate(lidar_age_s_, imu_age_s_, camera_age_s_);
  }

  // ── Run fusion (clustering + KF update) ────────────────────────────
  std::vector<Cluster> fuse(Level degradation) {
    std::vector<Cluster> clusters;

    switch (degradation) {
      case Level::FULL:
      case Level::NO_CAMERA:
      case Level::NO_IMU:
        if (lidar_ranges_) {
          clusters = detector_.detect(lidar_ranges_,
                                       lidar_angle_min_,
                                       lidar_angle_inc_);
        }
        break;
      case Level::NO_LIDAR:
        // Camera fallback — placeholder (real implementation in M3)
        break;
      case Level::CRITICAL:
        // No fusion — return empty
        break;
    }

    // KF update step — feed first cluster centroid as observation
    if (!clusters.empty()) {
      kf_.update(clusters[0].x, clusters[0].y);
    }

    return clusters;
  }

  // ── Accessors ──────────────────────────────────────────────────────
  double kf_x()  const { return kf_.x(); }
  double kf_y()  const { return kf_.y(); }
  double kf_vx() const { return kf_.vx(); }
  double kf_vy() const { return kf_.vy(); }

  static const char *heartbeat_for(Level level) {
    return DegradationPolicy::to_heartbeat_string(level);
  }

private:
  ClusterDetector   detector_;
  DegradationPolicy policy_;
  KalmanFilter2D<>  kf_;

  // Current sensor data
  const float *lidar_ranges_    = nullptr;
  float        lidar_angle_min_  = 0.0F;
  float        lidar_angle_inc_  = 0.0F;

  double imu_ax_ = 0.0;
  double imu_ay_ = 0.0;

  // Sensor staleness tracking (seconds since last data)
  double lidar_age_s_  = -1.0;
  double imu_age_s_    = -1.0;
  double camera_age_s_ = -1.0;
};

}  // namespace application
}  // namespace amr

#endif
