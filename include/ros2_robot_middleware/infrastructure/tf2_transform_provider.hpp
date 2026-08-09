#pragma once
/// @file   tf2_transform_provider.hpp
/// @brief  tf2-based coordinate transform (DDD infrastructure layer).
///
/// Implements ITransformProvider using tf2_ros::Buffer.
/// Static TF frames (lidar→base_link, etc.) are published to /tf_static
/// by the sensor launch files.

#include "ros2_robot_middleware/hal/sensor/isensor.hpp"
#include "ros2_robot_middleware/domain/transform_provider.hpp"

#include <tf2_ros/buffer.h>
#include <tf2_ros/transform_listener.h>
#include <geometry_msgs/msg/transform_stamped.hpp>

#include <cmath>
#include <memory>
#include <string>

namespace amr::infrastructure {

class Tf2TransformProvider : public amr::domain::ITransformProvider {
public:
  explicit Tf2TransformProvider(rclcpp::Clock::SharedPtr clock)
    : buffer_(std::make_shared<tf2_ros::Buffer>(clock)),
      listener_(std::make_shared<tf2_ros::TransformListener>(*buffer_)) {}

  bool transform_scan(const amr::hal::sensor::LidarScan &in,
                       amr::hal::sensor::LidarScan &out,
                       const std::string &target_frame) override
  {
    // Look up static transform: amr/chassis/lidar → target.
    // 源帧与 TF 树一致（曾硬编码 "lidar_frame" 导致 lookup 永远失败、
    // tf2 持续报 "lidar_frame 到 map/base_link 找不到变换" —— B2）。
    geometry_msgs::msg::TransformStamped tf;
    try {
      tf = buffer_->lookupTransform(target_frame, "amr/chassis/lidar",
                                     tf2::TimePointZero, tf2::Duration(0));
    } catch (const tf2::TransformException &) {
      // Transform unavailable — pass through untransformed data
      out = in;
      return false;
    }

    const double tx = tf.transform.translation.x;
    const double ty = tf.transform.translation.y;

    // Quaternion → rotation matrix (2D only: yaw)
    const double qx = tf.transform.rotation.x;
    const double qy = tf.transform.rotation.y;
    const double qz = tf.transform.rotation.z;
    const double qw = tf.transform.rotation.w;
    const double yaw = std::atan2(2.0 * (qw * qz + qx * qy),
                                   1.0 - 2.0 * (qy * qy + qz * qz));
    const double cos_y = std::cos(yaw);
    const double sin_y = std::sin(yaw);

    // Transform each point: polar → Cartesian → rotate+translate → polar
    out.range_count     = in.range_count;
    out.angle_min       = in.angle_min;
    out.angle_increment = in.angle_increment;

    for (size_t i = 0; i < in.range_count; ++i) {
      float r = in.ranges[i];
      float angle = in.angle_min + i * in.angle_increment;

      // Polar → Cartesian (lidar_frame)
      float lx = r * std::cos(angle);
      float ly = r * std::sin(angle);

      // Rotate + translate → Cartesian (base_link)
      float bx = cos_y * lx - sin_y * ly + tx;
      float by = sin_y * lx + cos_y * ly + ty;

      // Cartesian → Polar
      out.ranges[i] = std::sqrt(bx * bx + by * by);
    }

    return true;
  }

private:
  std::shared_ptr<tf2_ros::Buffer> buffer_;
  std::shared_ptr<tf2_ros::TransformListener> listener_;
};

} // namespace amr::infrastructure
