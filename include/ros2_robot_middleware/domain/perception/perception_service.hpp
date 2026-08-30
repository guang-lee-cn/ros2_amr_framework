#ifndef ROS2_ROBOT_MIDDLEWARE_DOMAIN_PERCEPTION_SERVICE_HPP_
#define ROS2_ROBOT_MIDDLEWARE_DOMAIN_PERCEPTION_SERVICE_HPP_

#include <array>

#include "ros2_robot_middleware/domain/perception/cluster_detector.hpp"
#include "ros2_robot_middleware/domain/perception/degradation_policy.hpp"
#include "ros2_robot_middleware/domain/perception/depth_obstacle_detector.hpp"
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
      // 目标帧 = amr/chassis（TF 树帧）。曾用 "base_link"（TF 树无此帧）
      // 导致 transform_scan 永远失败、scan 透传 —— B2。
      if (tf_->transform_scan(lidar_scan, transformed, "amr/chassis")) {
        lidar_scan = transformed;
      }
    }

    lidar_age_s_  = lidar_ok  ? 0.0 : lidar_age_s_  + dt;
    imu_age_s_    = imu_ok    ? 0.0 : imu_age_s_    + dt;
    camera_age_s_ = camera_ok ? 0.0 : camera_age_s_ + dt;

    if (lidar_ok) {
      std::copy(lidar_scan.ranges, lidar_scan.ranges + lidar_scan.range_count,
                lidar_ranges_.begin());
      lidar_range_count_ = lidar_scan.range_count;
      lidar_stamp_ns_    = lidar_scan.stamp_ns;  // 快照带戳：消费端 StampGate
                                                // 要能看见「数据是几时的」
      lidar_angle_min_   = lidar_scan.angle_min;
      lidar_angle_inc_   = lidar_scan.angle_increment;
    }
    if (imu_ok) {
      imu_ax_ = imu_data.linear_accel_x;
      imu_ay_ = imu_data.linear_accel_y;
    }

    // Camera depth → low-obstacle clusters (lidar blind-spot fill).
    // Camera stale → clear: depth naturally disappears (incl. the NO_CAMERA
    // degradation window where old data must not linger).
    if (camera_ok && !cam_frame.depth.empty()) {
      depth_clusters_ = DepthObstacleDetector::detect(cam_frame.depth);
    } else {
      depth_clusters_.clear();
    }
  }

  Level evaluate_degradation() const {
    return policy_.evaluate(lidar_age_s_, imu_age_s_, camera_age_s_);
  }

  std::vector<Cluster> fuse(Level degradation) {
    if (!cluster_) return {};
    std::vector<Cluster> clusters;
    switch (degradation) {
      case Level::FULL: case Level::NO_CAMERA: case Level::NO_IMU:
        if (lidar_range_count_ > 0)
          clusters = cluster_->detect(lidar_ranges_.data(), lidar_angle_min_, lidar_angle_inc_);
        break;
      case Level::NO_LIDAR: break;
      case Level::CRITICAL: break;
    }
    // Depth clusters merge only when camera is trusted (FULL / NO_IMU).
    // NO_CAMERA → lidar only; NO_LIDAR/CRITICAL stay empty.
    if (degradation == Level::FULL || degradation == Level::NO_IMU) {
      clusters = DepthObstacleDetector::merge(clusters, depth_clusters_);
    }
    return clusters;
  }

  /// Tracked-output variant: dt from the caller, IMU accel applied NEGATED
  /// (a static obstacle in body frame appears to accelerate opposite the robot).
  std::vector<TrackedObject> fuse_tracked(Level degradation, double dt) {
    auto clusters = fuse(degradation);
    return tracker_.update(clusters, dt, -imu_ax_, -imu_ay_);
  }

  /// 当前 LiDAR 原始点云快照（供可视化发布）。返回 false 表示无数据。
  bool lidar_snapshot(amr::hal::sensor::LidarScan &out) const {
    if (lidar_range_count_ == 0) return false;
    out.range_count = lidar_range_count_;
    out.angle_min = lidar_angle_min_;
    out.angle_increment = lidar_angle_inc_;
    out.stamp_ns = lidar_stamp_ns_;  // 透传上游戳（0=内部合成，消费端按读取时打戳）
    std::copy(lidar_ranges_.data(), lidar_ranges_.data() + lidar_range_count_, out.ranges);
    return true;
  }

  size_t track_count() const { return tracker_.track_count(); }

  static const char *heartbeat_for(Level level) {
    return DegradationPolicy::to_heartbeat_string(level);
  }

  IClusterAlgorithm *cluster_backend() const { return cluster_.get(); }

private:
  std::unique_ptr<IClusterAlgorithm> cluster_;
  DegradationPolicy policy_;
  MultiObjectTracker tracker_;

  LidarSensor  &lidar_;
  ImuSensor    &imu_;
  CameraSensor &camera_;
  ITransformProvider *tf_ = nullptr;

  // P0-A 修复（2026-08-30 三审）：曾为 const float* 指向 tick() 栈局部
  // LidarScan.ranges——tick 返回即悬垂，聚类/快照全在返回后消费（UAF 仅靠
  // 栈布局运气存活）。值语义拷贝，2048×4B=8KB 成员，5Hz 拷贝开销可忽略。
  std::array<float, amr::hal::sensor::LidarScan::kMaxRanges> lidar_ranges_{};
  size_t       lidar_range_count_ = 0;
  float        lidar_angle_min_  = 0.0F;
  int64_t      lidar_stamp_ns_   = 0;
  float        lidar_angle_inc_  = 0.0F;
  double       imu_ax_ = 0.0, imu_ay_ = 0.0;
  std::vector<amr::domain::perception::Cluster> depth_clusters_;  // camera depth → low obstacles

  double lidar_age_s_  = -1.0;
  double imu_age_s_    = -1.0;
  double camera_age_s_ = -1.0;
};

}  // namespace perception
}  // namespace domain
}  // namespace amr
#endif
