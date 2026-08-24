// U6 时间戳门控单测：新鲜/过期/未盖章/未注册/边界值/计数
#include <gtest/gtest.h>

#include "ros2_robot_middleware/domain/perception/stamp_gate.hpp"

namespace dp = amr::domain::perception;

TEST(StampGate, FreshSamplePasses) {
  dp::StampGate gate;
  gate.set_tolerance("lidar", 200000000);  // 200ms
  EXPECT_EQ(gate.check("lidar", 1'000'000'000, 1'100'000'000),
            dp::StampGate::Verdict::OK);  // age 100ms < 200ms
}

TEST(StampGate, StaleSampleRejectedAndCounted) {
  dp::StampGate gate;
  gate.set_tolerance("lidar", 200000000);
  EXPECT_EQ(gate.check("lidar", 1'000'000'000, 1'500'000'000),
            dp::StampGate::Verdict::STALE);  // age 500ms > 200ms
  EXPECT_EQ(gate.check("lidar", 1'000'000'000, 1'500'000'000),
            dp::StampGate::Verdict::STALE);
  EXPECT_EQ(gate.stale_count("lidar"), 2u);  // 计数两次
}

TEST(StampGate, UnstampedSampleRejected) {
  // stamp==0 视为未盖章：强制生产端显式打戳，不靠默认值蒙混（保守语义）
  dp::StampGate gate;
  gate.set_tolerance("lidar", 200000000);
  EXPECT_EQ(gate.check("lidar", 0, 1'000'000'000), dp::StampGate::Verdict::STALE);
}

TEST(StampGate, UnregisteredSensorRejected) {
  // 白名单语义：没注册容差的传感器直接拒绝（配置遗漏 fail-fast）
  dp::StampGate gate;
  EXPECT_EQ(gate.check("radar", 1'000'000'000, 1'000'000'000),
            dp::StampGate::Verdict::STALE);
  EXPECT_EQ(gate.unknown_rejects(), 1u);
}

TEST(StampGate, BoundaryAtTolerancePasses) {
  // age == tolerance：未"超过"，判 OK（边界含等号在通过侧）
  dp::StampGate gate;
  gate.set_tolerance("imu", 100000000);
  EXPECT_EQ(gate.check("imu", 1'000'000'000, 1'100'000'000),
            dp::StampGate::Verdict::OK);  // age 恰好 100ms
  EXPECT_EQ(gate.check("imu", 999'999'999, 1'100'000'000),
            dp::StampGate::Verdict::STALE);  // age 100ms+1ns
}
