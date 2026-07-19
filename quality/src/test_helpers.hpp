#pragma once
/// @file   test_helpers.hpp
/// @brief  Shared test utilities — fixture helpers, sensor data factories.
///         Included by test_*.cpp files that need ROS2 lifecycle spin or
///         synthetic sensor_msgs for fusion tests.

#include <chrono>
#include <memory>

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <sensor_msgs/msg/imu.hpp>
#include <sensor_msgs/msg/laser_scan.hpp>

#include <cmath>

// ── Spin helper ──────────────────────────────────────────────────────

/// Spin executor until predicate is true or timeout expires.
/// Removes the node from executor on exit (both success and timeout).
template <typename Predicate>
bool spin_until(rclcpp::node_interfaces::NodeBaseInterface::SharedPtr node_iface,
                Predicate pred, std::chrono::milliseconds timeout) {
  auto start = std::chrono::steady_clock::now();
  rclcpp::executors::SingleThreadedExecutor exec;
  exec.add_node(node_iface);
  while (!pred() && (std::chrono::steady_clock::now() - start) < timeout) {
    exec.spin_once(std::chrono::milliseconds(10));
  }
  exec.remove_node(node_iface);
  return pred();
}

// ── Sensor data factories (for fusion tests) ─────────────────────────

inline sensor_msgs::msg::LaserScan make_lidar(float range_near, float range_far,
                                               int center_idx, int half_width) {
  auto msg = sensor_msgs::msg::LaserScan{};
  msg.header.frame_id = "lidar_frame";
  msg.angle_min = -M_PI;
  msg.angle_max = M_PI;
  msg.angle_increment = 2.0F * M_PI / 360.0F;
  msg.time_increment = 0.0001F;
  msg.scan_time = 0.1F;
  msg.range_min = 0.1F;
  msg.range_max = 6.5F;
  msg.ranges.resize(360);
  msg.intensities.resize(360);
  for (int i = 0; i < 360; ++i) {
    int dist = std::abs(i - center_idx);
    if (dist <= half_width) {
      msg.ranges[i] = range_near;
      msg.intensities[i] = 0.8F;
    } else {
      msg.ranges[i] = range_far;
      msg.intensities[i] = 0.1F;
    }
  }
  return msg;
}

inline sensor_msgs::msg::Imu make_imu() {
  auto msg = sensor_msgs::msg::Imu{};
  msg.header.frame_id = "imu_link";
  msg.orientation.w = 1.0;
  return msg;
}

inline sensor_msgs::msg::Image make_camera() {
  auto msg = sensor_msgs::msg::Image{};
  msg.header.frame_id = "camera_link";
  msg.width = 640; msg.height = 480; msg.step = 1920;
  msg.encoding = "rgb8"; msg.is_bigendian = 0;
  msg.data.resize(921600);
  return msg;
}
