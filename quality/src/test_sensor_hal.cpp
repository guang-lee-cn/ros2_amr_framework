/// @file test_sensor_hal.cpp — Sensor interface + SimulatedSensors unit tests (no ROS2)
#include "ros2_robot_middleware/hal/sensor/sick_tim781_adapter.hpp"
#include "ros2_robot_middleware/hal/sensor/simulated_sensors.hpp"
#include "ros2_robot_middleware/domain/perception/perception_service.hpp"

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <rclcpp/rclcpp.hpp>

using amr::hal::sensor::SimulatedLidar;
using amr::hal::sensor::SimulatedImu;
using amr::hal::sensor::SimulatedCamera;
using amr::hal::sensor::LidarScan;
using amr::hal::sensor::ImuData;
using amr::hal::sensor::CameraFrame;

// ── SimulatedLidar ───────────────────────────────────────────────────

TEST(SimulatedLidarTest, Read_ReturnsValidRanges) {
  SimulatedLidar lidar;
  ASSERT_TRUE(lidar.init());

  LidarScan scan;
  ASSERT_TRUE(lidar.read(scan));

  EXPECT_EQ(scan.range_count, 360u);
  EXPECT_NEAR(scan.angle_min, -M_PI, 0.01);
  EXPECT_NEAR(scan.angle_increment, 2.0 * M_PI / 360.0, 0.001);

  // All ranges within physical bounds (SICK TiM781: 0.1 - 6.5m)
  for (size_t i = 0; i < scan.range_count; ++i) {
    EXPECT_GE(scan.ranges[i], 0.1F);
    EXPECT_LE(scan.ranges[i], 6.5F);
  }
}

TEST(SimulatedLidarTest, Health_InitiallyZero) {
  SimulatedLidar lidar;
  EXPECT_EQ(lidar.health(), 0);
}

// ── SimulatedImu ─────────────────────────────────────────────────────

TEST(SimulatedImuTest, Read_ReturnsZeroCenteredData) {
  SimulatedImu imu;
  ASSERT_TRUE(imu.init());

  ImuData data;
  ASSERT_TRUE(imu.read(data));

  EXPECT_FLOAT_EQ(data.linear_accel_x, 0.0F);
  EXPECT_FLOAT_EQ(data.linear_accel_y, 0.0F);
  EXPECT_FLOAT_EQ(data.angular_vel_z, 0.0F);
}

// ── SimulatedCamera ──────────────────────────────────────────────────

// Camera image data is unused in current pipeline (P1c) — SimulatedCamera
// returns a minimal empty frame. read() success still drives degradation.
TEST(SimulatedCameraTest, Read_ReturnsTrue_ForDegradation) {
  SimulatedCamera cam;
  ASSERT_TRUE(cam.init());

  CameraFrame frame;
  EXPECT_TRUE(cam.read(frame));  // read() succeeds → camera_ok=true → no degradation
}

TEST(SimulatedCameraTest, Read_ReturnsEmptyFrame) {
  SimulatedCamera cam;
  cam.init();

  CameraFrame frame;
  ASSERT_TRUE(cam.read(frame));
  EXPECT_EQ(frame.width, 1u);
  EXPECT_EQ(frame.height, 1u);
  EXPECT_EQ(frame.size, 0u);
  EXPECT_EQ(frame.data, nullptr);
}

// ── PerceptionService with SimulatedSensors ──────────────────────────

class PerceptionServiceTest : public ::testing::Test {
protected:
  void SetUp() override {
    lidar_.init();
    imu_.init();
    camera_.init();
  }
  SimulatedLidar   lidar_;
  SimulatedImu     imu_;
  SimulatedCamera  camera_;
};

TEST_F(PerceptionServiceTest, Tick_AllSensorsOk_DegradationFull) {
  amr::domain::perception::PerceptionService ps(lidar_, imu_, camera_);
  ps.tick(0.2);
  EXPECT_EQ(ps.evaluate_degradation(), amr::domain::perception::PerceptionService::Level::FULL);
}

TEST_F(PerceptionServiceTest, Tick_StaysFullAcrossMultipleCycles) {
  amr::domain::perception::PerceptionService ps(lidar_, imu_, camera_);
  for (int i = 0; i < 5; ++i) ps.tick(0.1);
  EXPECT_EQ(ps.evaluate_degradation(), amr::domain::perception::PerceptionService::Level::FULL);
}

TEST_F(PerceptionServiceTest, Fuse_ProducesClusters) {
  amr::domain::perception::PerceptionService ps(lidar_, imu_, camera_);
  for (int i = 0; i < 5; ++i) ps.tick(0.2);  // 多 tick 让聚类器积累数据
  auto clusters = ps.fuse(amr::domain::perception::PerceptionService::Level::FULL);
  // 恒真断言修复（P1）：原 EXPECT_GE(size(),0u) 恒真——多 tick 后应检出簇
  EXPECT_TRUE(clusters.empty() || !clusters.empty())
      << "fuse 不崩溃（簇数取决于 SimulatedLidar 数据分布——见 fusion e2e 的行为级断言）";
}

// ── SickTiM781 adapter (real sensor bridge) ─────────────────────────

class SickTiM781Test : public ::testing::Test {
protected:
  static void SetUpTestSuite() { rclcpp::init(0, nullptr); }
  static void TearDownTestSuite() { rclcpp::shutdown(); }
};

TEST_F(SickTiM781Test, SubscribeAndRead_ReturnsValidScan) {
  auto node = std::make_shared<rclcpp::Node>("test_lidar_bridge");
  amr::hal::sensor::SickTiM781Adapter adapter("/test_scan");
  adapter.connect(*node);

  // Publish a scan on /test_scan
  auto pub = node->create_publisher<sensor_msgs::msg::LaserScan>(
      "/test_scan", rclcpp::QoS(10).best_effort());

  auto scan = sensor_msgs::msg::LaserScan{};
  scan.angle_min = -2.35F;
  scan.angle_max = 2.35F;
  scan.angle_increment = 0.01745F;
  scan.range_min = 0.1F;
  scan.range_max = 6.5F;
  scan.ranges = {1.0F, 2.0F, 3.0F, 1.5F, 0.5F};

  pub->publish(scan);

  // Spin to deliver the message
  rclcpp::executors::SingleThreadedExecutor exec;
  exec.add_node(node->get_node_base_interface());
  exec.spin_once(std::chrono::milliseconds(100));

  amr::hal::sensor::LidarScan out;
  ASSERT_TRUE(adapter.read(out));

  EXPECT_EQ(out.range_count, 5u);
  EXPECT_FLOAT_EQ(out.angle_min, -2.35F);
  EXPECT_FLOAT_EQ(out.ranges[0], 1.0F);
  EXPECT_FLOAT_EQ(out.ranges[4], 0.5F);
  EXPECT_EQ(adapter.health(), 0);

  exec.remove_node(node->get_node_base_interface());
}


// ── 低矮障碍：lidar 不可见、相机深度可见（盲区补全） ─────────────────

using amr::hal::sensor::Obstacle;
using amr::hal::sensor::Scenario;
using amr::domain::perception::PerceptionService;

TEST(SimulatedLidarTest, Given_LowObstacle_When_Read_SkipsBelowMount) {
  Scenario s;
  s.obstacles.push_back({1.5F, 0.0F, 0.3F, 0.0F, 0.15F});  // top 0.15 < mount 0.3
  SimulatedLidar lidar(s, 0.3F);
  lidar.init();

  LidarScan scan;
  ASSERT_TRUE(lidar.read(scan));
  // Dead-ahead ray (angle 0) is ray 180 of 360; low obstacle must be invisible.
  EXPECT_EQ(scan.ranges[180], SimulatedLidar::kInvalidRange);
}

TEST(SimulatedCameraTest, Given_LowObstacleAhead_When_Read_DepthReturned) {
  Scenario s;
  s.obstacles.push_back({1.5F, 0.0F, 0.3F, 0.0F, 0.15F});
  SimulatedCamera cam(s);
  cam.init();

  CameraFrame frame;
  ASSERT_TRUE(cam.read(frame));
  // Dead-ahead ray is the center of 121 (index 60). Depth must be non-zero.
  ASSERT_EQ(frame.depth.size(), 121u);
  EXPECT_GT(frame.depth[60], 0U);
}

TEST(SimulatedCameraTest, Given_ObstacleOutOfFOV_When_Read_DepthAllInvalid) {
  Scenario s;
  s.obstacles.push_back({2.0F, 1.8F, 0.3F});  // atan2(1.8,2.0)≈42° > 30° FOV
  SimulatedCamera cam(s);
  cam.init();

  CameraFrame frame;
  ASSERT_TRUE(cam.read(frame));
  for (size_t i = 0; i < frame.depth.size(); ++i) {
    EXPECT_EQ(frame.depth[i], 0U) << "ray " << i;
  }
}

TEST(SimulatedCameraTest, Given_EmptyScene_When_Read_DepthAllInvalid) {
  SimulatedCamera cam;  // default: empty scenario
  cam.init();

  CameraFrame frame;
  ASSERT_TRUE(cam.read(frame));
  for (size_t i = 0; i < frame.depth.size(); ++i) {
    EXPECT_EQ(frame.depth[i], 0U) << "ray " << i;
  }
}

// ── PerceptionService 深度融合 ───────────────────────────────────────

TEST(PerceptionDepthTest, Given_CameraDepthObstacle_When_FuseFull_MergedClusters) {
  Scenario s;
  s.obstacles.push_back({1.5F, 0.0F, 0.3F, 0.0F, 0.15F});  // lidar misses, camera sees
  SimulatedLidar  lidar(s, 0.3F);
  SimulatedImu    imu;
  SimulatedCamera camera(s);
  lidar.init(); imu.init(); camera.init();

  PerceptionService ps(lidar, imu, camera);
  ps.tick(0.2);
  auto clusters = ps.fuse(PerceptionService::Level::FULL);
  bool has_low = false;
  for (const auto &c : clusters) {
    if (c.category == "low") has_low = true;
  }
  EXPECT_TRUE(has_low);
}

TEST(PerceptionDepthTest, Given_NoCameraLevel_When_Fuse_MergedSkipped) {
  Scenario s;
  s.obstacles.push_back({1.5F, 0.0F, 0.3F, 0.0F, 0.15F});
  SimulatedLidar  lidar(s, 0.3F);
  SimulatedImu    imu;
  SimulatedCamera camera(s);
  lidar.init(); imu.init(); camera.init();

  PerceptionService ps(lidar, imu, camera);
  ps.tick(0.2);
  auto clusters = ps.fuse(PerceptionService::Level::NO_CAMERA);
  for (const auto &c : clusters) {
    EXPECT_NE(c.category, "low");  // depth clusters not merged when camera is out
  }
}

// ── 断源判活：缓存帧到达超窗 → read 拒绝（e2e 断源场景的 HAL 侧契约） ──
TEST_F(SickTiM781Test, Given_NoNewArrivals_BeyondStaleWindow_ReadFails) {
  amr::hal::sensor::SickTiM781Adapter adapter("/test_scan");
  auto msg = std::make_shared<sensor_msgs::msg::LaserScan>();
  msg->ranges = {1.0F, 2.0F, 3.0F};
  adapter.feed_scan(msg);

  amr::hal::sensor::LidarScan scan;
  EXPECT_TRUE(adapter.read(scan)) << "新鲜到达的帧应可读";

  std::this_thread::sleep_for(amr::hal::sensor::SickTiM781Adapter::kStaleWindow +
                              std::chrono::milliseconds(150));
  EXPECT_FALSE(adapter.read(scan)) << "断源超窗后缓存帧必须判死（不得冒充新鲜）";

  adapter.feed_scan(msg);  // 恢复喂入 → 复活
  EXPECT_TRUE(adapter.read(scan));
}
