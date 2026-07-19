#ifndef ROS2_ROBOT_MIDDLEWARE_APPLICATION_PERCEPTION_SERVICE_HPP_
#define ROS2_ROBOT_MIDDLEWARE_APPLICATION_PERCEPTION_SERVICE_HPP_

#include "ros2_robot_middleware/domain/perception/cluster_detector.hpp"
#include "ros2_robot_middleware/domain/perception/degradation_policy.hpp"
#include "ros2_robot_middleware/domain/perception/kalman_filter.hpp"
#include "ros2_robot_middleware/domain/perception/sensor_interface.hpp"
#include "ros2_robot_middleware/domain/perception/tracker.hpp"

#include <vector>

namespace amr {
namespace application {

// PerceptionService — dependency injection via ISensor<DataType> interfaces.
// Sensors injected at construction time — no templates, easily testable with mocks.

class PerceptionService {
public:
  using Level    = amr::domain::perception::DegradationLevel;
  using Cluster  = amr::domain::perception::Cluster;
  using DegradationPolicy = amr::domain::perception::DegradationPolicy;
  using ClusterDetector   = amr::domain::perception::ClusterDetector;
  using LidarSensor   = amr::domain::sensor::ISensor<amr::domain::sensor::LidarScan>;
  using ImuSensor     = amr::domain::sensor::ISensor<amr::domain::sensor::ImuData>;
  using CameraSensor  = amr::domain::sensor::ISensor<amr::domain::sensor::CameraFrame>;

  /// @param lidar, imu, camera — injected sensor interfaces (non-owning)
  PerceptionService(LidarSensor &lidar, ImuSensor &imu, CameraSensor &camera)
    : lidar_(lidar), imu_(imu), camera_(camera) {}

  /// With custom detector params + degradation timeouts (test hook)
  PerceptionService(LidarSensor &lidar, ImuSensor &imu, CameraSensor &camera,
                    const ClusterDetector::Params &cluster_params,
                    const DegradationPolicy::Config &deg_config)
    : detector_(cluster_params), policy_(deg_config),
      lidar_(lidar), imu_(imu), camera_(camera) {}

  // ── Time tick (dt in seconds) — reads sensors, predicts KF ──────────
  void tick(double dt) {
    amr::domain::sensor::LidarScan   lidar_scan;
    amr::domain::sensor::ImuData     imu_data;
    amr::domain::sensor::CameraFrame cam_frame;

    bool lidar_ok  = lidar_.read(lidar_scan);
    bool imu_ok    = imu_.read(imu_data);
    bool camera_ok = camera_.read(cam_frame);

    lidar_age_s_  = lidar_ok  ? 0.0 : lidar_age_s_  + dt;
    imu_age_s_    = imu_ok    ? 0.0 : imu_age_s_    + dt;
    camera_age_s_ = camera_ok ? 0.0 : camera_age_s_ + dt;

    if (lidar_ok) {
      lidar_ranges_      = lidar_scan.ranges;
      lidar_range_count_ = lidar_scan.range_count;
      lidar_angle_min_   = lidar_scan.angle_min;
      lidar_angle_inc_   = lidar_scan.angle_increment;
    }
    if (imu_ok) {
      imu_ax_ = imu_data.linear_accel_x;
      imu_ay_ = imu_data.linear_accel_y;
    }

    kf_.predict(dt, imu_ax_, imu_ay_);
  }

  Level evaluate_degradation() const {
    return policy_.evaluate(lidar_age_s_, imu_age_s_, camera_age_s_);
  }

  std::vector<Cluster> fuse(Level degradation) {
    std::vector<Cluster> clusters;
    switch (degradation) {
      case Level::FULL: case Level::NO_CAMERA: case Level::NO_IMU:
        if (lidar_ranges_ && lidar_range_count_ > 0)
          clusters = detector_.detect(lidar_ranges_, lidar_angle_min_, lidar_angle_inc_);
        break;
      case Level::NO_LIDAR: break;
      case Level::CRITICAL: break;
    }
    if (!clusters.empty()) kf_.update(clusters[0].x, clusters[0].y);
    return clusters;
  }

  /// Tracked objects with persistent IDs (runs tracker after clustering)
  std::vector<amr::domain::perception::TrackedObject> fuse_tracked(Level degradation) {
    auto clusters = fuse(degradation);
    return tracker_.update(clusters);
  }

  size_t track_count() const { return tracker_.track_count(); }

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
  amr::domain::perception::MultiObjectTracker tracker_;

  LidarSensor  &lidar_;
  ImuSensor    &imu_;
  CameraSensor &camera_;

  const float *lidar_ranges_     = nullptr;
  size_t       lidar_range_count_ = 0;
  float        lidar_angle_min_  = 0.0F;
  float        lidar_angle_inc_  = 0.0F;
  double       imu_ax_ = 0.0, imu_ay_ = 0.0;

  double lidar_age_s_  = -1.0;
  double imu_age_s_    = -1.0;
  double camera_age_s_ = -1.0;
};

}  // namespace application
}  // namespace amr
#endif
