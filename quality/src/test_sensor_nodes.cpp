/// @file test_sensor_nodes.cpp — Lidar/IMU/Camera sensor layer tests
#include "ros2_robot_middleware/infrastructure/camera_node.hpp"
#include "ros2_robot_middleware/infrastructure/imu_node.hpp"
#include "ros2_robot_middleware/infrastructure/lidar_node.hpp"

#include <sensor_msgs/msg/image.hpp>
#include <sensor_msgs/msg/imu.hpp>
#include <sensor_msgs/msg/laser_scan.hpp>

#include <gtest/gtest.h>
#include "test_helpers.hpp"

#include <rclcpp/rclcpp.hpp>

class SensorNodeTest : public ::testing::Test {
protected:
  static void SetUpTestSuite() { rclcpp::init(0, nullptr); }
  static void TearDownTestSuite() { rclcpp::shutdown(); }
};

// ── LidarNode ────────────────────────────────────────────────────────

TEST_F(SensorNodeTest, LidarNode_TimerFires_RangesInPhysicalBounds) {
  auto node = std::make_shared<LidarNode>();
  node->configure();
  node->activate();

  sensor_msgs::msg::LaserScan::SharedPtr last_msg;
  auto sub = node->create_subscription<sensor_msgs::msg::LaserScan>(
      "/sensor/lidar", rclcpp::QoS(10).best_effort(),
      [&last_msg](sensor_msgs::msg::LaserScan::SharedPtr msg) { last_msg = msg; });

  ASSERT_TRUE(spin_until(node->get_node_base_interface(),
                         [&last_msg] { return last_msg != nullptr; },
                         std::chrono::seconds(3)));

  EXPECT_EQ(last_msg->ranges.size(), 360u);
  EXPECT_EQ(last_msg->intensities.size(), 360u);
  for (size_t i = 0; i < last_msg->ranges.size(); ++i) {
    EXPECT_GE(last_msg->ranges[i], 0.1F);
    EXPECT_LE(last_msg->ranges[i], 6.5F);
  }
}

// ── ImuNode ──────────────────────────────────────────────────────────

TEST_F(SensorNodeTest, ImuNode_TimerFires_DataWithinSensorSpec) {
  auto node = std::make_shared<ImuNode>();
  node->configure();
  node->activate();

  sensor_msgs::msg::Imu::SharedPtr last_msg;
  auto sub = node->create_subscription<sensor_msgs::msg::Imu>(
      "/sensor/imu", rclcpp::QoS(10).reliable(),
      [&last_msg](sensor_msgs::msg::Imu::SharedPtr msg) { last_msg = msg; });

  ASSERT_TRUE(spin_until(node->get_node_base_interface(),
                         [&last_msg] { return last_msg != nullptr; },
                         std::chrono::seconds(3)));

  EXPECT_NEAR(last_msg->angular_velocity.x, 0.0, 0.04);
  EXPECT_NEAR(last_msg->angular_velocity.y, 0.0, 0.04);
  EXPECT_NEAR(last_msg->angular_velocity.z, 0.0, 0.04);
  EXPECT_NEAR(last_msg->linear_acceleration.x, 0.0, 0.2);
  EXPECT_NEAR(last_msg->linear_acceleration.y, 0.0, 0.2);
  EXPECT_NEAR(last_msg->linear_acceleration.z, 0.0, 0.2);
  EXPECT_EQ(last_msg->header.frame_id, "imu_link");
}

// ── CameraNode ───────────────────────────────────────────────────────

TEST_F(SensorNodeTest, CameraNode_TimerFires_640x480Rgb8Image) {
  auto node = std::make_shared<CameraNode>();
  node->configure();
  node->activate();

  sensor_msgs::msg::Image::SharedPtr last_msg;
  auto sub = node->create_subscription<sensor_msgs::msg::Image>(
      "/sensor/camera", rclcpp::QoS(10).best_effort(),
      [&last_msg](sensor_msgs::msg::Image::SharedPtr msg) { last_msg = msg; });

  ASSERT_TRUE(spin_until(node->get_node_base_interface(),
                         [&last_msg] { return last_msg != nullptr; },
                         std::chrono::seconds(3)));

  EXPECT_EQ(last_msg->width, 640u);
  EXPECT_EQ(last_msg->height, 480u);
  EXPECT_EQ(last_msg->step, 1920u);
  EXPECT_EQ(last_msg->encoding, "rgb8");
  EXPECT_EQ(last_msg->data.size(), 921600u);
}
