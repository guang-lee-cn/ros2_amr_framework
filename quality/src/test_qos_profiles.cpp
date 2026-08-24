// 收敛件单测：QoS 词汇表档位断言（词汇表是唯一合法来源，档位错=语义错）
#include <gtest/gtest.h>

#include "ros2_robot_middleware/infrastructure/qos_profiles.hpp"

namespace {
auto profile(const rclcpp::QoS &q) { return q.get_rmw_qos_profile(); }
}  // namespace

TEST(QosProfiles, SensorStreamIsBestEffortShallow) {
  auto p = profile(amr::qos::sensor_stream());
  EXPECT_EQ(p.reliability, RMW_QOS_POLICY_RELIABILITY_BEST_EFFORT);
  EXPECT_EQ(p.depth, 5u);  // 丢帧无害、浅队列防积压
}

TEST(QosProfiles, ReliableStreamDefaults) {
  auto p = profile(amr::qos::reliable_stream());
  EXPECT_EQ(p.reliability, RMW_QOS_POLICY_RELIABILITY_RELIABLE);
  EXPECT_EQ(p.depth, 10u);
}

TEST(QosProfiles, LatchedStateIsTransientLocal) {
  auto p = profile(amr::qos::latched_state());
  EXPECT_EQ(p.reliability, RMW_QOS_POLICY_RELIABILITY_RELIABLE);
  EXPECT_EQ(p.durability, RMW_QOS_POLICY_DURABILITY_TRANSIENT_LOCAL);
  EXPECT_EQ(p.depth, 1u);  // 晚加入者补最后一帧（实测 5:0 vs volatile 0）
}

TEST(QosProfiles, ControlStreamDepthOne) {
  auto p = profile(amr::qos::control_stream());
  EXPECT_EQ(p.reliability, RMW_QOS_POLICY_RELIABILITY_RELIABLE);
  EXPECT_EQ(p.depth, 1u);  // 新命令顶掉旧命令
}
