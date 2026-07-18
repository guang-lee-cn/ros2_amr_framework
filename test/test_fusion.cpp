/// @file test_fusion.cpp — FusionNode detection + sensor degradation tests
#include "ros2_robot_middleware/infrastructure/fusion_node.hpp"

#include "ros2_robot_middleware/msg/perception_objects.hpp"

#include <sensor_msgs/msg/image.hpp>
#include <sensor_msgs/msg/imu.hpp>
#include <sensor_msgs/msg/laser_scan.hpp>

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>

#include <chrono>
#include <memory>

// ── Fixture + helpers ────────────────────────────────────────────────

class FusionTest : public ::testing::Test {
protected:
  static void SetUpTestSuite() { rclcpp::init(0, nullptr); }
  static void TearDownTestSuite() { rclcpp::shutdown(); }
};

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

static sensor_msgs::msg::LaserScan make_lidar(float range_near, float range_far,
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

static sensor_msgs::msg::Imu make_imu() {
  auto msg = sensor_msgs::msg::Imu{};
  msg.header.frame_id = "imu_link";
  msg.orientation.w = 1.0;
  return msg;
}

static sensor_msgs::msg::Image make_camera() {
  auto msg = sensor_msgs::msg::Image{};
  msg.header.frame_id = "camera_link";
  msg.width = 640; msg.height = 480; msg.step = 1920;
  msg.encoding = "rgb8"; msg.is_bigendian = 0;
  msg.data.resize(921600);
  return msg;
}

// Short timeouts for fast degradation tests (production defaults: 0.5/1.5/3.0s)
static constexpr amr::domain::perception::DegradationPolicy::Config kFastConfig{
    0.05,  // imu_timeout_s
    0.10,  // lidar_timeout_s
    0.10   // camera_timeout_s
};

// ── Detection ────────────────────────────────────────────────────────

TEST_F(FusionTest, AllSensorsReady_DetectsNearbyCluster) {
  auto fusion = std::make_shared<FusionNode>();
  fusion->configure();
  fusion->activate();

  ros2_robot_middleware::msg::PerceptionObjects::SharedPtr last_output;
  auto out_sub = fusion->create_subscription<ros2_robot_middleware::msg::PerceptionObjects>(
      "/perception/objects", rclcpp::QoS(10).reliable(),
      [&last_output](ros2_robot_middleware::msg::PerceptionObjects::SharedPtr msg) { last_output = msg; });

  auto lidar_pub = fusion->create_publisher<sensor_msgs::msg::LaserScan>(
      "/sensor/lidar", rclcpp::QoS(10).best_effort());
  auto imu_pub = fusion->create_publisher<sensor_msgs::msg::Imu>(
      "/sensor/imu", rclcpp::QoS(10).reliable());
  auto cam_pub = fusion->create_publisher<sensor_msgs::msg::Image>(
      "/sensor/camera", rclcpp::QoS(10).best_effort());
  lidar_pub->on_activate(); imu_pub->on_activate(); cam_pub->on_activate();

  lidar_pub->publish(make_lidar(1.5F, 10.0F, 180, 10));
  imu_pub->publish(make_imu());
  cam_pub->publish(make_camera());

  ASSERT_TRUE(spin_until(fusion->get_node_base_interface(),
                         [&last_output] { return last_output != nullptr; },
                         std::chrono::seconds(3)));
  EXPECT_GE(last_output->objects.size(), 1u);
}

// ── Degradation: FULL ────────────────────────────────────────────────

TEST_F(FusionTest, AllSensorsPresent_DegradationFull) {
  auto fusion = std::make_shared<FusionNode>(rclcpp::NodeOptions{}, kFastConfig);
  fusion->configure();
  fusion->activate();

  auto lidar_pub = fusion->create_publisher<sensor_msgs::msg::LaserScan>(
      "/sensor/lidar", rclcpp::QoS(10).best_effort());
  auto imu_pub = fusion->create_publisher<sensor_msgs::msg::Imu>(
      "/sensor/imu", rclcpp::QoS(10).reliable());
  auto cam_pub = fusion->create_publisher<sensor_msgs::msg::Image>(
      "/sensor/camera", rclcpp::QoS(10).best_effort());
  lidar_pub->on_activate(); imu_pub->on_activate(); cam_pub->on_activate();

  lidar_pub->publish(make_lidar(2.0F, 10.0F, 180, 10));
  imu_pub->publish(make_imu());
  cam_pub->publish(make_camera());

  bool degraded = false;
  spin_until(fusion->get_node_base_interface(),
             [&] { degraded = (fusion->degradation_level() != FusionNode::DegradationLevel::FULL);
                   return degraded; },
             std::chrono::milliseconds(200));
  EXPECT_FALSE(degraded);
  EXPECT_EQ(fusion->degradation_level(), FusionNode::DegradationLevel::FULL);
}

// ── Degradation: NO_IMU ──────────────────────────────────────────────

TEST_F(FusionTest, ImuMissing_DegradedToNoImu) {
  auto fusion = std::make_shared<FusionNode>(rclcpp::NodeOptions{}, kFastConfig);
  fusion->configure();
  fusion->activate();

  auto lidar_pub = fusion->create_publisher<sensor_msgs::msg::LaserScan>(
      "/sensor/lidar", rclcpp::QoS(10).best_effort());
  auto cam_pub = fusion->create_publisher<sensor_msgs::msg::Image>(
      "/sensor/camera", rclcpp::QoS(10).best_effort());
  lidar_pub->on_activate(); cam_pub->on_activate();

  lidar_pub->publish(make_lidar(2.0F, 10.0F, 180, 10));
  cam_pub->publish(make_camera());

  bool imu_degraded = false;
  ASSERT_TRUE(spin_until(fusion->get_node_base_interface(),
                          [&] { imu_degraded = (fusion->degradation_level() == FusionNode::DegradationLevel::NO_IMU);
                                return imu_degraded; },
                          std::chrono::milliseconds(300)));
  EXPECT_TRUE(imu_degraded);
}

// ── Degradation: NO_LIDAR ────────────────────────────────────────────

TEST_F(FusionTest, LidarMissing_DegradedToNoLidar) {
  auto fusion = std::make_shared<FusionNode>(rclcpp::NodeOptions{},
      amr::domain::perception::DegradationPolicy::Config{0.05, 0.05, 0.05});
  fusion->configure();
  fusion->activate();

  auto imu_pub = fusion->create_publisher<sensor_msgs::msg::Imu>(
      "/sensor/imu", rclcpp::QoS(10).reliable());
  auto cam_pub = fusion->create_publisher<sensor_msgs::msg::Image>(
      "/sensor/camera", rclcpp::QoS(10).best_effort());
  imu_pub->on_activate(); cam_pub->on_activate();

  rclcpp::executors::SingleThreadedExecutor exec;
  exec.add_node(fusion->get_node_base_interface());
  auto start = std::chrono::steady_clock::now();
  auto last_pub = start;
  auto result = FusionNode::DegradationLevel::FULL;

  while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(500)) {
    if (std::chrono::steady_clock::now() - last_pub > std::chrono::milliseconds(20)) {
      imu_pub->publish(make_imu());
      cam_pub->publish(make_camera());
      last_pub = std::chrono::steady_clock::now();
    }
    exec.spin_once(std::chrono::milliseconds(10));
    result = fusion->degradation_level();
    if (result == FusionNode::DegradationLevel::NO_LIDAR) break;
  }
  exec.remove_node(fusion->get_node_base_interface());
  EXPECT_EQ(result, FusionNode::DegradationLevel::NO_LIDAR);
}

// ── Degradation: CRITICAL ────────────────────────────────────────────

TEST_F(FusionTest, ImuAndCameraMissing_DegradedToCritical) {
  auto fusion = std::make_shared<FusionNode>(rclcpp::NodeOptions{},
      amr::domain::perception::DegradationPolicy::Config{0.02, 0.05, 0.05});
  fusion->configure();
  fusion->activate();

  auto lidar_pub = fusion->create_publisher<sensor_msgs::msg::LaserScan>(
      "/sensor/lidar", rclcpp::QoS(10).best_effort());
  lidar_pub->on_activate();

  rclcpp::executors::SingleThreadedExecutor exec;
  exec.add_node(fusion->get_node_base_interface());
  auto start = std::chrono::steady_clock::now();
  auto last_pub = start;
  auto result = FusionNode::DegradationLevel::FULL;

  while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(500)) {
    if (std::chrono::steady_clock::now() - last_pub > std::chrono::milliseconds(10)) {
      lidar_pub->publish(make_lidar(2.0F, 10.0F, 180, 10));
      last_pub = std::chrono::steady_clock::now();
    }
    exec.spin_once(std::chrono::milliseconds(10));
    result = fusion->degradation_level();
    if (result == FusionNode::DegradationLevel::CRITICAL) break;
  }
  exec.remove_node(fusion->get_node_base_interface());
  EXPECT_EQ(result, FusionNode::DegradationLevel::CRITICAL);
}
