#pragma once
/// @file   trace_points.hpp
/// @brief  Span name registry — single source of truth for all trace points.
///
/// ADDING A NEW SPAN:
///   1. Add a constexpr entry below following the existing naming convention.
///   2. Use TRACE_SCOPE(amr::trace::LIDAR_CALLBACK) in code (never raw strings).
///   3. Code review gates: raw strings in TRACE_SCOPE are rejected.
///
/// Naming convention:
///   SENSOR_ACTION    — lidar_callback, imu_callback, camera_callback
///   FUSION_ACTION    — fusion::timer_callback, fusion::degradation_eval
///   DECISION_ACTION  — decision::on_perception, decision::send_goal
///   MOTOR_ACTION     — motor::execute, motor::cancel
///   MONITOR_ACTION   — health::check_cycle, health::recovery
///   FLEET_ACTION     — fleet::aggregate

namespace amr::trace {

// ── Sensor layer ────────────────────────────────────────────────────
inline constexpr const char *LIDAR_CALLBACK   = "lidar::callback";
inline constexpr const char *IMU_CALLBACK     = "imu::callback";
inline constexpr const char *CAMERA_CALLBACK  = "camera::callback";

// ── Fusion layer ─────────────────────────────────────────────────────
inline constexpr const char *FUSION_TIMER          = "fusion::timer";
inline constexpr const char *FUSION_DEGRADATION    = "fusion::degradation";
inline constexpr const char *FUSION_CLUSTER_DETECT = "fusion::cluster_detect";

// ── Decision layer ───────────────────────────────────────────────────
inline constexpr const char *DECISION_ON_PERCEPTION = "decision::on_perception";
inline constexpr const char *DECISION_SEND_GOAL     = "decision::send_goal";
inline constexpr const char *DECISION_PREEMPT       = "decision::preempt";

// ── Motor control layer ──────────────────────────────────────────────
inline constexpr const char *MOTOR_EXECUTE  = "motor::execute";
inline constexpr const char *MOTOR_CANCEL   = "motor::cancel";
inline constexpr const char *MOTOR_STEP     = "motor::step";

// ── Health monitor layer ─────────────────────────────────────────────
inline constexpr const char *HEALTH_CHECK    = "health::check";
inline constexpr const char *HEALTH_RECOVERY = "health::recovery";

// ── Fleet manager layer ──────────────────────────────────────────────
inline constexpr const char *FLEET_AGGREGATE = "fleet::aggregate";

} // namespace amr::trace
