/// @file test_degradation_policy.cpp — heartbeat string round-trip + nominal check
#include "ros2_robot_middleware/domain/perception/degradation_policy.hpp"

#include <gtest/gtest.h>

#include <string>

using amr::domain::perception::DegradationLevel;
using amr::domain::perception::DegradationPolicy;

// ── from_heartbeat_string ────────────────────────────────────────────

TEST(DegradationPolicyTest, Given_AliveHeartbeat_When_Parse_IsNominal) {
  DegradationLevel level;
  ASSERT_TRUE(DegradationPolicy::from_heartbeat_string("alive", level));
  EXPECT_EQ(level, DegradationLevel::FULL);
  EXPECT_TRUE(DegradationPolicy::is_nominal(level));
}

TEST(DegradationPolicyTest, Given_CriticalHeartbeat_When_Parse_NotNominal) {
  DegradationLevel level;
  ASSERT_TRUE(DegradationPolicy::from_heartbeat_string("critical", level));
  EXPECT_EQ(level, DegradationLevel::CRITICAL);
  EXPECT_FALSE(DegradationPolicy::is_nominal(level));
}

TEST(DegradationPolicyTest, Given_DegradedNoLidar_When_Parse_NotNominal) {
  DegradationLevel level;
  ASSERT_TRUE(DegradationPolicy::from_heartbeat_string("degraded_no_lidar", level));
  EXPECT_EQ(level, DegradationLevel::NO_LIDAR);
  EXPECT_FALSE(DegradationPolicy::is_nominal(level));
}

TEST(DegradationPolicyTest, Given_DegradedNoCamera_When_Parse_NotNominal) {
  DegradationLevel level;
  ASSERT_TRUE(DegradationPolicy::from_heartbeat_string("degraded_no_camera", level));
  EXPECT_EQ(level, DegradationLevel::NO_CAMERA);
  EXPECT_FALSE(DegradationPolicy::is_nominal(level));
}

TEST(DegradationPolicyTest, Given_DegradedNoImu_When_Parse_NotNominal) {
  DegradationLevel level;
  ASSERT_TRUE(DegradationPolicy::from_heartbeat_string("degraded_no_imu", level));
  EXPECT_EQ(level, DegradationLevel::NO_IMU);
  EXPECT_FALSE(DegradationPolicy::is_nominal(level));
}

TEST(DegradationPolicyTest, Given_Inactive_When_Parse_Fails) {
  DegradationLevel level;
  EXPECT_FALSE(DegradationPolicy::from_heartbeat_string("inactive", level));
}

TEST(DegradationPolicyTest, Given_Unknown_When_Parse_Fails) {
  DegradationLevel level;
  EXPECT_FALSE(DegradationPolicy::from_heartbeat_string("garbage", level));
}

// ── round-trip: to_heartbeat_string(from_heartbeat_string(s)) == s ───

TEST(DegradationPolicyTest, Given_AllLevels_When_RoundTrip_Match) {
  for (DegradationLevel lvl : {DegradationLevel::FULL, DegradationLevel::NO_LIDAR,
                               DegradationLevel::NO_CAMERA, DegradationLevel::NO_IMU,
                               DegradationLevel::CRITICAL}) {
    DegradationLevel back;
    ASSERT_TRUE(DegradationPolicy::from_heartbeat_string(
        DegradationPolicy::to_heartbeat_string(lvl), back));
    EXPECT_EQ(back, lvl);
  }
}
