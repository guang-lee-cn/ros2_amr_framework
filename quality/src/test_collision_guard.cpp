/// @file test_collision_guard.cpp — CollisionGuard unit tests (no ROS2)
#include "ros2_robot_middleware/domain/execution/collision_guard.hpp"

#include <cmath>
#include <chrono>
#include <gtest/gtest.h>

using amr::domain::execution::CollisionGuard;
using amr::domain::execution::ScanData;
using Clock = std::chrono::steady_clock;
using ms = std::chrono::milliseconds;

namespace {

constexpr float kStop = 0.3F;
constexpr float kSafe = 0.8F;
constexpr float kFovHalf = 0.7854F;   // 45°
constexpr float kNoEcho = 20.0F;      // > range_max (20) → ignored

CollisionGuard::Params kParams{kStop, kSafe, kFovHalf, kNoEcho, ms{500}};

/// 7 beams across ±90°. Beam i=3 (0°) at distance `d`; beams at ±30° are
/// inside the FOV, beams at ±60° and ±90° are outside. Pass kNoEcho to
/// place no obstacle on that beam.
ScanData scan_with_ahead(float d) {
  ScanData s;
  s.angle_min = -1.5708F;      // -90°
  s.angle_increment = 0.5236F; // 30° steps → -90,-60,-30,0,30,60,90
  s.ranges = {kNoEcho, kNoEcho, kNoEcho, d, kNoEcho, kNoEcho, kNoEcho};
  return s;
}

/// Obstacle at ±90° (outside FOV), clear ahead.
ScanData scan_with_side(float d) {
  ScanData s;
  s.angle_min = -1.5708F;
  s.angle_increment = 0.5236F;
  s.ranges = {d, kNoEcho, kNoEcho, kNoEcho, kNoEcho, kNoEcho, d};
  return s;
}

}  // namespace

class CollisionGuardTest : public ::testing::Test {
protected:
  void SetUp() override { t0_ = Clock::now(); }
  Clock::time_point t0_;
  CollisionGuard guard_{kParams};
};

// ── GivenNoObstacleInFov_WhenClamp_KeepsCommandVelocity ────────────────
// Open field (all echoes beyond range_max): the guard passes the command.

TEST_F(CollisionGuardTest, GivenNoObstacleInFov_WhenClamp_KeepsCommandVelocity) {
  guard_.set_scan(scan_with_ahead(kNoEcho), t0_);
  EXPECT_FLOAT_EQ(guard_.clamp(0.5F, t0_), 0.5F);
  EXPECT_FALSE(guard_.stopped(t0_));
}

// ── GivenObstacleBeyondSafeDistance_WhenClamp_KeepsCommandVelocity ─────

TEST_F(CollisionGuardTest,
       GivenObstacleBeyondSafeDistance_WhenClamp_KeepsCommandVelocity) {
  guard_.set_scan(scan_with_ahead(1.0F), t0_);  // outside safe_dist
  EXPECT_FLOAT_EQ(guard_.clamp(0.5F, t0_), 0.5F);
}

// ── GivenObstacleBetweenSafeAndStop_WhenClamp_ScalesVelocityLinearly ───
// d=0.55 → t = (0.55-0.30)/(0.80-0.30) = 0.5 → v = 0.5·0.5 = 0.25.

TEST_F(CollisionGuardTest,
       GivenObstacleBetweenSafeAndStop_WhenClamp_ScalesVelocityLinearly) {
  guard_.set_scan(scan_with_ahead(0.55F), t0_);
  EXPECT_NEAR(guard_.clamp(0.5F, t0_), 0.25F, 1e-4F);
}

// ── GivenObstacleWithinStopDistance_WhenClamp_CommandsFullStop ─────────

TEST_F(CollisionGuardTest,
       GivenObstacleWithinStopDistance_WhenClamp_CommandsFullStop) {
  guard_.set_scan(scan_with_ahead(0.2F), t0_);
  EXPECT_FLOAT_EQ(guard_.clamp(0.5F, t0_), 0.0F);
  EXPECT_TRUE(guard_.stopped(t0_));
  EXPECT_NEAR(guard_.nearest_distance(), 0.2F, 1e-4F);
}

// ── GivenObstacleOutsideFov_WhenClamp_DoesNotAffectVelocity ────────────
// Side obstacle (close!) is outside the forward FOV → no clamp.

TEST_F(CollisionGuardTest,
       GivenObstacleOutsideFov_WhenClamp_DoesNotAffectVelocity) {
  guard_.set_scan(scan_with_side(0.2F), t0_);
  EXPECT_FLOAT_EQ(guard_.clamp(0.5F, t0_), 0.5F);
  EXPECT_FALSE(guard_.stopped(t0_));
}

// ── GivenInfAndNanRanges_WhenClamp_IgnoresThem ─────────────────────────
// inf/NaN beams are ignored; a valid obstacle ahead still governs.

TEST_F(CollisionGuardTest, GivenInfAndNanRanges_WhenClamp_IgnoresThem) {
  ScanData s;
  s.angle_min = -1.5708F;
  s.angle_increment = 0.5236F;
  s.ranges = {std::numeric_limits<float>::infinity(),
              std::numeric_limits<float>::quiet_NaN(),
              kNoEcho, 0.55F, kNoEcho,
              std::numeric_limits<float>::quiet_NaN(),
              std::numeric_limits<float>::infinity()};
  guard_.set_scan(std::move(s), t0_);
  // 0.55 → t=0.5 → 0.25, proving inf/NaN did not corrupt the nearest.
  EXPECT_NEAR(guard_.clamp(0.5F, t0_), 0.25F, 1e-4F);
}

// ── GivenStaleScan_WhenClamp_CommandsFullStop ──────────────────────────
// Scan received, then > stale_timeout (500ms) elapses → conservative stop.

TEST_F(CollisionGuardTest, GivenStaleScan_WhenClamp_CommandsFullStop) {
  guard_.set_scan(scan_with_ahead(kNoEcho), t0_);
  auto late = t0_ + ms{1000};
  EXPECT_FLOAT_EQ(guard_.clamp(0.5F, late), 0.0F);
  EXPECT_TRUE(guard_.stopped(late));
}

// ── GivenEmptyScan_WhenClamp_CommandsFullStop ──────────────────────────
// Sensor delivered 0 beams → treat as sensor failure, stop.

TEST_F(CollisionGuardTest, GivenEmptyScan_WhenClamp_CommandsFullStop) {
  guard_.set_scan(ScanData{}, t0_);
  EXPECT_FLOAT_EQ(guard_.clamp(0.5F, t0_), 0.0F);
  EXPECT_TRUE(guard_.stopped(t0_));
}

// ── GivenObstacleAhead_WhenClamp_DoesNotModifyAngular ──────────────────
// The guard only clamps linear velocity; the angular channel is caller-owned
// (no angular parameter exists). Verify a stop still reports the obstacle.

TEST_F(CollisionGuardTest, GivenObstacleAhead_WhenClamp_DoesNotModifyAngular) {
  guard_.set_scan(scan_with_ahead(0.2F), t0_);
  const float v = guard_.clamp(0.5F, t0_);
  EXPECT_FLOAT_EQ(v, 0.0F);      // linear clamped
  EXPECT_NEAR(guard_.nearest_distance(), 0.2F, 1e-4F);  // obstacle reported
}

// ── GivenObstaclePersistsBeyondTimeout_WhenClamp_ReportsBlocked ────────
// Start from a clear pass (last_ok_time_ set), then obstacle holds the robot
// stopped → blocked_for() grows past the motor's 3s anti-deadlock timeout.

TEST_F(CollisionGuardTest,
       GivenObstaclePersistsBeyondTimeout_WhenClamp_ReportsBlocked) {
  guard_.set_scan(scan_with_ahead(kNoEcho), t0_);
  guard_.clamp(0.5F, t0_);                       // pass → last_ok_time_ = t0_

  guard_.set_scan(scan_with_ahead(0.2F), t0_);   // obstacle appears
  EXPECT_FLOAT_EQ(guard_.clamp(0.5F, t0_), 0.0F);

  auto t_blocked = t0_ + ms{4000};
  EXPECT_TRUE(guard_.stopped(t_blocked));
  EXPECT_GE(guard_.blocked_for(t_blocked), ms{3900});  // ≈ 4s held
}

// ── GivenObstacleClearsBeforeTimeout_WhenClamp_NotBlocked ──────────────
// Obstacle moves away before the timeout → stop resets, blocked_for ≈ 0.

TEST_F(CollisionGuardTest,
       GivenObstacleClearsBeforeTimeout_WhenClamp_NotBlocked) {
  guard_.set_scan(scan_with_ahead(kNoEcho), t0_);
  guard_.clamp(0.5F, t0_);                       // pass → last_ok_time_ = t0_

  guard_.set_scan(scan_with_ahead(0.2F), t0_);
  guard_.clamp(0.5F, t0_);                       // stop

  auto t_clear = t0_ + ms{1000};
  guard_.set_scan(scan_with_ahead(kNoEcho), t_clear);  // obstacle gone
  EXPECT_FLOAT_EQ(guard_.clamp(0.5F, t_clear), 0.5F);
  EXPECT_FALSE(guard_.stopped(t_clear));
  EXPECT_LT(guard_.blocked_for(t_clear), ms{50});       // reset
}

// ── GivenAllInfScanWithMinEchoes_WhenClamp_CommandsFullStop ────────────
// 2026-08-17 incident regression: gpu_lidar degraded to all-inf while still
// publishing on time — stale/empty checks never fired and "open field"
// let a blind robot drive through the racks. With min_valid_echoes set,
// an omni-directionally echo-less scan is sensor death → hard stop.

TEST_F(CollisionGuardTest,
       GivenAllInfScanWithMinEchoes_WhenClamp_CommandsFullStop) {
  CollisionGuard::Params p = kParams;
  p.min_valid_echoes = 2;
  guard_.set_params(p);

  ScanData s;
  s.angle_min = -1.5708F;
  s.angle_increment = 0.5236F;
  s.ranges = {std::numeric_limits<float>::infinity(),
              std::numeric_limits<float>::infinity(),
              std::numeric_limits<float>::infinity(),
              std::numeric_limits<float>::infinity(),
              std::numeric_limits<float>::infinity(),
              std::numeric_limits<float>::infinity(),
              std::numeric_limits<float>::infinity()};
  guard_.set_scan(std::move(s), t0_);
  EXPECT_FLOAT_EQ(guard_.clamp(0.5F, t0_), 0.0F);
  EXPECT_TRUE(guard_.stopped(t0_));
}

// ── GivenAllInfScanWithoutMinEchoes_WhenClamp_PassesThrough ────────────
// Real-hardware default (min_valid_echoes = 0): a legal echo-less open
// square must keep passing through — the fail-safe is opt-in.

TEST_F(CollisionGuardTest,
       GivenAllInfScanWithoutMinEchoes_WhenClamp_PassesThrough) {
  ScanData s;
  s.angle_min = -1.5708F;
  s.angle_increment = 0.5236F;
  s.ranges.assign(7, std::numeric_limits<float>::infinity());
  guard_.set_scan(std::move(s), t0_);
  EXPECT_FLOAT_EQ(guard_.clamp(0.5F, t0_), 0.5F);
  EXPECT_FALSE(guard_.stopped(t0_));
}

// ── GivenFilteredPipelineEchoAt035_WhenStopDist040_CommandsFullStop ────
// Alignment regression: the sim scan_filter drops every echo below its
// 0.35 m cut, so a filtered pipeline reports "nearest = 0.35" while
// grinding into the rack. stop_dist must sit above the cut for the hard
// stop to be reachable at all.

TEST_F(CollisionGuardTest,
       GivenFilteredPipelineEchoAt035_WhenStopDist040_CommandsFullStop) {
  CollisionGuard::Params p = kParams;
  p.stop_dist = 0.40F;  // sim launch override (> scan_filter cut 0.35)
  guard_.set_params(p);

  guard_.set_scan(scan_with_ahead(0.35F), t0_);  // closest the filter can report
  EXPECT_FLOAT_EQ(guard_.clamp(0.5F, t0_), 0.0F);
  EXPECT_TRUE(guard_.stopped(t0_));
}

// ── GivenBorderDitheringCrawl_WhenClamp_BlockedForGrows ────────────────
// 2026-08-17 rack deadlock regression: nearest sitting just above stop_dist
// clamps the output to a few mm/s. Those crawling frames used to refresh
// the hold timer every cycle, so the 3s abort never accumulated and the
// robot ground into the rack at 4 mm/s forever. A pressed crawl must keep
// blocked_for() growing.

TEST_F(CollisionGuardTest,
       GivenBorderDitheringCrawl_WhenClamp_BlockedForGrows) {
  CollisionGuard::Params p = kParams;
  p.stop_dist = 0.40F;
  p.safe_dist = 0.80F;
  guard_.set_params(p);

  guard_.set_scan(scan_with_ahead(kNoEcho), t0_);
  guard_.clamp(0.5F, t0_);                     // mobile anchor → last_ok = t0_

  // d = 0.401 → t = 0.0025 → out = 1.25 mm/s < crawl (20 mm/s), pressed.
  auto t1 = t0_ + ms{100};
  guard_.set_scan(scan_with_ahead(0.401F), t1);
  EXPECT_NEAR(guard_.clamp(0.5F, t1), 0.00125F, 1e-4F);  // crawling, not stopped
  EXPECT_FALSE(guard_.stopped(t1));                       // ... yet blocked:

  auto t2 = t1 + ms{4000};
  guard_.set_scan(scan_with_ahead(0.401F), t2);           // fresh, still crawling
  guard_.clamp(0.5F, t2);
  EXPECT_GE(guard_.blocked_for(t2), ms{3900});            // grew from t0_
}

// ── GivenTurningInPlaceNoObstacle_WhenClamp_NotBlocked ─────────────────
// A diff-drive turning in place commands v = 0 legally (PurePursuit heading
// correction). With no obstacle pressing, those zero-speed frames must NOT
// grow the hold — only obstacle-pressed slowness is a deadlock signal.

TEST_F(CollisionGuardTest,
       GivenTurningInPlaceNoObstacle_WhenClamp_NotBlocked) {
  guard_.set_scan(scan_with_ahead(kNoEcho), t0_);
  guard_.clamp(0.5F, t0_);  // mobile frame → last_ok_time_ = t0_

  auto t_turn = t0_ + ms{4000};
  guard_.set_scan(scan_with_ahead(kNoEcho), t_turn);
  EXPECT_FLOAT_EQ(guard_.clamp(0.0F, t_turn), 0.0F);       // turning in place
  EXPECT_LT(guard_.blocked_for(t_turn), ms{50});           // timer refreshed
}

// ── GivenNearObstaclePassingNormally_WhenClamp_NotBlocked ──────────────
// Slowdown-zone transit at a real speed (0.25 m/s through d = 0.55) is
// normal negotiation, not a deadlock — the hold must keep refreshing.

TEST_F(CollisionGuardTest,
       GivenNearObstaclePassingNormally_WhenClamp_NotBlocked) {
  CollisionGuard::Params p = kParams;
  p.stop_dist = 0.40F;
  p.safe_dist = 0.80F;
  guard_.set_params(p);

  guard_.set_scan(scan_with_ahead(kNoEcho), t0_);
  guard_.clamp(0.5F, t0_);                     // mobile anchor → last_ok = t0_

  auto t_late = t0_ + ms{4000};
  guard_.set_scan(scan_with_ahead(0.55F), t_late);
  EXPECT_NEAR(guard_.clamp(0.5F, t_late), 0.1875F, 1e-4F); // t=0.375
  EXPECT_LT(guard_.blocked_for(t_late), ms{50});           // mobile, refreshed
}

// ── GivenFreshGoal_WhenClearHold_TimerRestarts ─────────────────────────
// Each dispatched goal deserves its own 3s window. Without clear_hold the
// hold accumulated by the replaced goal made every redispatch abort on its
// first frame (200 ms abort→dispatch spin with the robot already stopped).

TEST_F(CollisionGuardTest, GivenFreshGoal_WhenClearHold_TimerRestarts) {
  guard_.set_scan(scan_with_ahead(kNoEcho), t0_);
  guard_.clamp(0.5F, t0_);                     // mobile anchor → last_ok = t0_

  guard_.set_scan(scan_with_ahead(0.2F), t0_);  // hard-stop obstacle
  guard_.clamp(0.5F, t0_);                      // hold starts accumulating
  EXPECT_TRUE(guard_.stopped(t0_));

  auto t_new_goal = t0_ + ms{2000};
  EXPECT_GE(guard_.blocked_for(t_new_goal), ms{1900});  // still holding…

  guard_.clear_hold(t_new_goal);                 // fresh goal dispatched
  EXPECT_LT(guard_.blocked_for(t_new_goal), ms{50});    // window restarted
  EXPECT_FALSE(guard_.stopped(t_new_goal));
}
