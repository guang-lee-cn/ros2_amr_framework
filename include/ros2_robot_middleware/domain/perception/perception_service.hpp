#ifndef ROS2_ROBOT_MIDDLEWARE_DOMAIN_PERCEPTION_SERVICE_HPP_
#define ROS2_ROBOT_MIDDLEWARE_DOMAIN_PERCEPTION_SERVICE_HPP_

#include "ros2_robot_middleware/domain/perception/cluster_detector.hpp"
#include "ros2_robot_middleware/domain/perception/degradation_policy.hpp"
#include "ros2_robot_middleware/domain/perception/icluster_algorithm.hpp"
#include "ros2_robot_middleware/domain/perception/kalman_filter.hpp"
#include "ros2_robot_middleware/hal/sensor/isensor.hpp"
#include "ros2_robot_middleware/domain/perception/tracker.hpp"
#include "ros2_robot_middleware/domain/transform_provider.hpp"

#include <algorithm>
#include <memory>
#include <vector>

namespace amr {
namespace domain {
namespace perception {

// PerceptionService — sensor fusion orchestration.
// Clustering backend is swappable via IClusterAlgorithm (DBSCAN / PCL / custom).
class PerceptionService {
public:
  using Level    = DegradationLevel;
  using Cluster  = Cluster;
  using LidarSensor   = amr::hal::sensor::ISensor<amr::hal::sensor::LidarScan>;
  using ImuSensor     = amr::hal::sensor::ISensor<amr::hal::sensor::ImuData>;
  using CameraSensor  = amr::hal::sensor::ISensor<amr::hal::sensor::CameraFrame>;

  void set_transform(amr::domain::ITransformProvider *tf) { tf_ = tf; }

  // Default: DBSCAN backend
  PerceptionService(LidarSensor &lidar, ImuSensor &imu, CameraSensor &camera)
    : lidar_(lidar), imu_(imu), camera_(camera) {
    cluster_ = std::make_unique<ClusterDetector>();
  }

  // Custom cluster backend (e.g. PclClusterBackend)
  PerceptionService(LidarSensor &lidar, ImuSensor &imu, CameraSensor &camera,
                    std::unique_ptr<IClusterAlgorithm> cluster_backend,
                    const DegradationPolicy::Config &deg_config = {})
    : cluster_(std::move(cluster_backend)), policy_(deg_config),
      lidar_(lidar), imu_(imu), camera_(camera) {}

  void tick(double dt) {
    amr::hal::sensor::LidarScan   lidar_scan;
    amr::hal::sensor::ImuData     imu_data;
    amr::hal::sensor::CameraFrame cam_frame;

    bool lidar_ok  = lidar_.read(lidar_scan);
    bool imu_ok    = imu_.read(imu_data);
    bool camera_ok = camera_.read(cam_frame);

    if (lidar_ok && tf_) {
      amr::hal::sensor::LidarScan transformed;
      if (tf_->transform_scan(lidar_scan, transformed, "base_link")) {
        lidar_scan = transformed;
      }
    }

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
    if (!cluster_) return {};
    std::vector<Cluster> clusters;
    switch (degradation) {
      case Level::FULL: case Level::NO_CAMERA: case Level::NO_IMU:
        if (lidar_ranges_ && lidar_range_count_ > 0)
          clusters = cluster_->detect(lidar_ranges_, lidar_angle_min_, lidar_angle_inc_);
        break;
      case Level::NO_LIDAR: break;
      case Level::CRITICAL: break;
    }
    if (!clusters.empty()) kf_.update(clusters[0].x, clusters[0].y);
    return clusters;
  }

  std::vector<TrackedObject> fuse_tracked(Level degradation) {
    auto clusters = fuse(degradation);
    return tracker_.update(clusters);
  }

  /// 当前 LiDAR 原始点云快照（供可视化发布）。返回 false 表示无数据。
  bool lidar_snapshot(amr::hal::sensor::LidarScan &out) const {
    if (!lidar_ranges_ || lidar_range_count_ == 0) return false;
    out.range_count = lidar_range_count_;
    out.angle_min = lidar_angle_min_;
    out.angle_increment = lidar_angle_inc_;
    std::copy(lidar_ranges_, lidar_ranges_ + lidar_range_count_, out.ranges);
    return true;
  }

  size_t track_count() const { return tracker_.track_count(); }

  double kf_x()  const { return kf_.x(); }
  double kf_y()  const { return kf_.y(); }
  double kf_vx() const { return kf_.vx(); }
  double kf_vy() const { return kf_.vy(); }

  static const char *heartbeat_for(Level level) {
    return DegradationPolicy::to_heartbeat_string(level);
  }

  IClusterAlgorithm *cluster_backend() const { return cluster_.get(); }

private:
  std::unique_ptr<IClusterAlgorithm> cluster_;
  DegradationPolicy policy_;
  KalmanFilter2D<>  kf_;
  MultiObjectTracker tracker_;

  LidarSensor  &lidar_;
  ImuSensor    &imu_;
  CameraSensor &camera_;
  ITransformProvider *tf_ = nullptr;

  const float *lidar_ranges_     = nullptr;
  size_t       lidar_range_count_ = 0;
  float        lidar_angle_min_  = 0.0F;
  float        lidar_angle_inc_  = 0.0F;
  double       imu_ax_ = 0.0, imu_ay_ = 0.0;

  double lidar_age_s_  = -1.0;
  double imu_age_s_    = -1.0;
  double camera_age_s_ = -1.0;
};

}  // namespace perception
}  // namespace domain
}  // namespace amr
#endif
