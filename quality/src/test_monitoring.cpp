/// @file test_monitoring.cpp — HealthMonitor + MonitoringService tests
#include "ros2_robot_middleware/application/monitoring_service.hpp"
#include "ros2_robot_middleware/domain/monitoring/heartbeat_analyzer.hpp"
#include "ros2_robot_middleware/domain/monitoring/recovery_policy.hpp"

#include <gtest/gtest.h>

using amr::application::MonitoringService;
using amr::domain::monitoring::HeartbeatAnalyzer;
using amr::domain::monitoring::NodeHeartbeat;
using amr::domain::monitoring::NodeStatus;
using amr::domain::monitoring::RecoveryPolicy;

// ── HeartbeatAnalyzer ────────────────────────────────────────────────

TEST(HeartbeatAnalyzerTest, FreshHeartbeat_ReturnsOk) {
  HeartbeatAnalyzer a;
  NodeHeartbeat hb{"lidar", 1.5, 0.3};
  EXPECT_EQ(a.evaluate(hb), NodeStatus::OK);
}

TEST(HeartbeatAnalyzerTest, ExceededTimeout_ReturnsError) {
  HeartbeatAnalyzer a;
  NodeHeartbeat hb{"lidar", 1.5, 1.6};  // 1.6 > 1.5 timeout → ERROR
  EXPECT_EQ(a.evaluate(hb), NodeStatus::ERROR);
}

TEST(HeartbeatAnalyzerTest, AtThreshold_ReturnsWarn) {
  HeartbeatAnalyzer a;
  NodeHeartbeat hb{"lidar", 1.5, 1.3};  // 1.3 > 1.5*0.8=1.2 → WARN
  EXPECT_EQ(a.evaluate(hb), NodeStatus::WARN);
}

TEST(HeartbeatAnalyzerTest, NeverReceived_ReturnsStale) {
  HeartbeatAnalyzer a;
  NodeHeartbeat hb{"camera", 3.0, -1.0};  // never received data
  EXPECT_EQ(a.evaluate(hb), NodeStatus::STALE);
}

// ── RecoveryPolicy ───────────────────────────────────────────────────

TEST(RecoveryPolicyTest, Error_TriggersRecovery) {
  RecoveryPolicy rp;
  RecoveryPolicy::RecoveryState rec{};
  EXPECT_TRUE(rp.should_recover(NodeStatus::ERROR, rec));
}

TEST(RecoveryPolicyTest, Ok_DoesNotTriggerRecovery) {
  RecoveryPolicy rp;
  RecoveryPolicy::RecoveryState rec{};
  EXPECT_FALSE(rp.should_recover(NodeStatus::OK, rec));
}

TEST(RecoveryPolicyTest, MaxRetries_Exceeded_StopsRecovery) {
  RecoveryPolicy rp;
  RecoveryPolicy::RecoveryState rec{};
  rec.attempts = RecoveryPolicy::kMaxRetries + 1;
  EXPECT_FALSE(rp.should_recover(NodeStatus::ERROR, rec));
}

TEST(RecoveryPolicyTest, Escalate_AfterMaxRetries_ReturnsFatal) {
  RecoveryPolicy rp;
  RecoveryPolicy::RecoveryState rec{};
  rec.attempts = RecoveryPolicy::kMaxRetries;
  EXPECT_EQ(rp.escalate(NodeStatus::ERROR, rec), NodeStatus::FATAL);
}

TEST(RecoveryPolicyTest, Ok_DoesNotTrigger_AndDoesNotModifyState) {
  RecoveryPolicy rp;
  RecoveryPolicy::RecoveryState rec{};
  rec.attempts = 2;
  EXPECT_FALSE(rp.should_recover(NodeStatus::OK, rec));
  EXPECT_EQ(rec.attempts, 2u);  // const& — not modified
}

// ── MonitoringService ────────────────────────────────────────────────

class MonitoringServiceTest : public ::testing::Test {
protected:
  void SetUp() override {
    svc_.register_node("lidar", 1.5);
    svc_.register_node("imu", 0.5);
    svc_.register_node("camera", 3.0);
  }
  MonitoringService svc_;
};

TEST_F(MonitoringServiceTest, AllNodesOk_AfterHeartbeat) {
  svc_.heartbeat_received("lidar");
  svc_.heartbeat_received("imu");
  svc_.heartbeat_received("camera");

  svc_.tick(0.1);  // age by 100ms

  EXPECT_EQ(svc_.evaluate("lidar"), NodeStatus::OK);
  EXPECT_EQ(svc_.evaluate("imu"), NodeStatus::OK);
  EXPECT_EQ(svc_.evaluate("camera"), NodeStatus::OK);
}

TEST_F(MonitoringServiceTest, ImuTimeout_Detected) {
  svc_.heartbeat_received("lidar");
  svc_.heartbeat_received("imu");
  svc_.heartbeat_received("camera");

  // Age IMU past its 0.5s timeout
  svc_.tick(0.51);

  EXPECT_EQ(svc_.evaluate("lidar"), NodeStatus::OK);     // 1.5s > 0.51s
  EXPECT_EQ(svc_.evaluate("imu"), NodeStatus::ERROR);     // 0.5s < 0.51s
  EXPECT_EQ(svc_.evaluate("camera"), NodeStatus::OK);     // 3.0s > 0.51s
}

TEST_F(MonitoringServiceTest, ImuError_TriggersRecovery) {
  svc_.heartbeat_received("imu");

  // Force IMU into ERROR via repeated aging
  for (int i = 0; i < 3; ++i) {
    svc_.tick(0.51);  // each tick pushes IMU past timeout
    svc_.evaluate("imu");
  }

  EXPECT_TRUE(svc_.should_recover("imu"));
}

TEST_F(MonitoringServiceTest, RecoveryMaxRetries_Exceeded) {
  // Force ERROR and trigger recovery kMaxRetries times
  for (int attempt = 0; attempt < RecoveryPolicy::kMaxRetries; ++attempt) {
    for (int j = 0; j < 3; ++j) {
      svc_.tick(0.51);
      svc_.evaluate("imu");
    }
    // HealthMonitorNode calls should_recover → if true, increments attempts internally
    svc_.should_recover("imu");
  }
  // One more round: should now be at kMaxRetries, next call returns false
  for (int j = 0; j < 3; ++j) {
    svc_.tick(0.51);
    svc_.evaluate("imu");
  }
  EXPECT_FALSE(svc_.should_recover("imu"));
}

TEST_F(MonitoringServiceTest, MixedStatus_EachNodeIndependent) {
  svc_.heartbeat_received("lidar");   // OK (timeout 1.5s)
  svc_.heartbeat_received("imu");     // OK for now
  // camera: never received → STALE from start

  svc_.tick(0.6);  // IMU now stale (0.6 > 0.5)

  EXPECT_EQ(svc_.evaluate("lidar"), NodeStatus::OK);
  EXPECT_EQ(svc_.evaluate("imu"), NodeStatus::ERROR);   // 0.6 > 0.5 timeout
  EXPECT_EQ(svc_.evaluate("camera"), NodeStatus::STALE); // never received (last_seen=-1)
}
