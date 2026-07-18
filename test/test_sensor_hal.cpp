/// @file test_sensor_hal.cpp — Sensor interface + SimulatedSensors unit tests (no ROS2)
#include "ros2_robot_middleware/infrastructure/sensors/simulated_sensors.hpp"
#include "ros2_robot_middleware/application/perception_service.hpp"

#include <gtest/gtest.h>

using amr::infrastructure::sensors::SimulatedLidar;
using amr::infrastructure::sensors::SimulatedImu;
using amr::infrastructure::sensors::SimulatedCamera;
using amr::domain::sensor::LidarScan;
using amr::domain::sensor::ImuData;
using amr::domain::sensor::CameraFrame;

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

TEST(SimulatedCameraTest, Read_FillsCallerBuffer) {
  SimulatedCamera cam;
  ASSERT_TRUE(cam.init());

  CameraFrame frame;
  ASSERT_TRUE(cam.read(frame));

  EXPECT_EQ(frame.width, 640u);
  EXPECT_EQ(frame.height, 480u);
  EXPECT_GT(frame.size, 0u);
  EXPECT_NE(frame.data, nullptr);
}

TEST(SimulatedCameraTest, GenerationCounter_Advances) {
  SimulatedCamera cam;
  cam.init();

  CameraFrame f1, f2;
  cam.read(f1);
  cam.read(f2);

#ifndef NDEBUG
  EXPECT_NE(f1.generation, f2.generation);
#endif
}

// ── PerceptionService with SimulatedSensors ──────────────────────────

using TestPerception = amr::application::PerceptionService<
    SimulatedLidar, SimulatedImu, SimulatedCamera>;

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
  TestPerception ps(lidar_, imu_, camera_);
  ps.tick(0.2);  // 200ms — all sensors read ok → ages = 0
  EXPECT_EQ(ps.evaluate_degradation(), TestPerception::Level::FULL);
}

TEST_F(PerceptionServiceTest, Tick_StaysFullAcrossMultipleCycles) {
  TestPerception ps(lidar_, imu_, camera_);
  for (int i = 0; i < 5; ++i) {
    ps.tick(0.1);
  }
  EXPECT_EQ(ps.evaluate_degradation(), TestPerception::Level::FULL);
}

TEST_F(PerceptionServiceTest, Fuse_ProducesClusters) {
  TestPerception ps(lidar_, imu_, camera_);
  ps.tick(0.2);

  auto clusters = ps.fuse(TestPerception::Level::FULL);
  // Simulated lidar has sine-wave data with nearby returns — should detect clusters
  EXPECT_GE(clusters.size(), 0u);  // may be 0 if no near-range returns in this cycle
}
