#ifndef ROS2_ROBOT_MIDDLEWARE_DOMAIN_COLLISION_GUARD_HPP_
#define ROS2_ROBOT_MIDDLEWARE_DOMAIN_COLLISION_GUARD_HPP_

/// @file   collision_guard.hpp
/// @brief  Collision speed guard — clamps forward velocity by the nearest
///         obstacle in the forward FOV from the LiDAR scan (G2-C).
///
/// Safety layer between PurePursuit output and /cmd_vel:
///   - obstacle within stop_dist   → hard stop (v = 0)
///   - obstacle within safe_dist   → linear slowdown to stop_dist
///   - otherwise                   → command passes through
///
/// Only forward velocity is clamped — angular velocity is never touched,
/// so a diff-drive robot keeps the ability to steer around.
///
/// Scan lifecycle:
///   - no scan yet (pre-first-echo)  → pass through (sensor boot race)
///   - scan stale (> stale_timeout)  → hard stop (protection expired)
///   - empty scan (0 ranges)         → hard stop (sensor gave nothing)
///   - all inf/NaN (open field)      → pass through (no echoes ≠ obstacle)
///
/// Thread safety: set_scan() runs on the sensor callback thread while
/// clamp()/blocked_for() run on the motor control thread — all state access
/// is guarded by an internal mutex (data race between on_scan and execute
/// caused the guard to miss obstacles in the sim).
///
/// Anti-deadlock: an obstacle holding the robot stopped grows blocked_for();
/// the motor layer fails the goal beyond a timeout so the decision layer
/// replans (see docs/design/20260806-g2-collision-guard.md).
///
/// Pure domain logic — no ROS2.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <limits>
#include <mutex>
#include <vector>

namespace amr {
namespace domain {
namespace execution {

struct ScanData {
  std::vector<float> ranges;   // measured ranges (m); inf/NaN = no echo
  float angle_min = 0.0F;      // angle of ranges[0] (rad)
  float angle_increment = 0.0F; // angular spacing between beams (rad)
};

class CollisionGuard {
public:
  struct Params {
    float stop_dist = 0.30F;    // hard stop within this distance (m)
    float safe_dist = 0.80F;    // linear slowdown within this distance (m)
    float fov_half = 0.7854F;   // forward FOV half-angle (rad) ≈ 45°
    float range_max = 20.0F;    // ranges beyond = no echo (m)
    std::chrono::milliseconds stale_timeout{500};  // protection expiry
  };

  CollisionGuard() = default;
  explicit CollisionGuard(const Params &p) : params_(p) {}

  /// Store the latest scan. `now` timestamps it for staleness checks.
  void set_scan(ScanData scan, const std::chrono::steady_clock::time_point &now);

  /// Clamp the commanded forward velocity by the nearest FOV obstacle.
  /// Returns the clamped linear velocity (angular is caller-owned).
  float clamp(float cmd_v, const std::chrono::steady_clock::time_point &now);

  /// True if the last clamp() ended in a guard-forced stop (obstacle / stale /
  /// empty scan). Start-up pass-through (no scan yet) is not a stop.
  bool stopped(const std::chrono::steady_clock::time_point &now) const;

  /// How long the robot has been held stopped by the guard (anti-deadlock
  /// timeout source). 0 when never stopped / never clamped.
  std::chrono::milliseconds blocked_for(
      const std::chrono::steady_clock::time_point &now) const;

  /// Nearest obstacle distance inside the FOV (m); +inf when none.
  float nearest_distance() const;

  const Params &params() const { return params_; }

private:
  struct Snapshot {
    std::vector<float> ranges;
    float angle_min = 0.0F;
    float angle_increment = 0.0F;
    std::chrono::steady_clock::time_point recv_time =
        std::chrono::steady_clock::time_point::min();  // never received
  };

  bool stale_locked(const std::chrono::steady_clock::time_point &now) const;
  float nearest_in_fov_locked() const;

  Params params_;
  Snapshot scan_;
  std::chrono::steady_clock::time_point last_ok_time_ =
      std::chrono::steady_clock::time_point::min();  // last un-stopped clamp
  bool last_stop_ = false;  // most recent clamp() ended in a forced stop
  mutable std::mutex mtx_;
};

// ── Inline implementation ────────────────────────────────────────────────────

inline void CollisionGuard::set_scan(
    ScanData scan, const std::chrono::steady_clock::time_point &now) {
  std::lock_guard<std::mutex> lock(mtx_);
  scan_.ranges = std::move(scan.ranges);
  scan_.angle_min = scan.angle_min;
  scan_.angle_increment = scan.angle_increment;
  scan_.recv_time = now;
}

inline bool CollisionGuard::stale_locked(
    const std::chrono::steady_clock::time_point &now) const {
  return (now - scan_.recv_time) > params_.stale_timeout;
}

inline float CollisionGuard::nearest_in_fov_locked() const {
  float best = std::numeric_limits<float>::infinity();
  for (std::size_t i = 0; i < scan_.ranges.size(); ++i) {
    const float r = scan_.ranges[i];
    if (!std::isfinite(r) || r <= 0.0F || r > params_.range_max) continue;
    const float a = scan_.angle_min
                  + static_cast<float>(i) * scan_.angle_increment;
    if (std::fabs(a) > params_.fov_half) continue;
    best = std::min(best, r);
  }
  return best;
}

inline float CollisionGuard::clamp(
    float cmd_v, const std::chrono::steady_clock::time_point &now) {
  std::lock_guard<std::mutex> lock(mtx_);

  const bool have_scan = scan_.recv_time !=
      std::chrono::steady_clock::time_point::min();
  const float d = nearest_in_fov_locked();
  last_stop_ = have_scan && (stale_locked(now) || scan_.ranges.empty()
                             || d <= params_.stop_dist);
  if (last_stop_) {
    // last_ok_time_ held → blocked_for() grows from the last un-stopped clamp.
    return 0.0F;
  }
  last_ok_time_ = now;

  // d > stop_dist is guaranteed here (last_stop_ was false). Linear slowdown
  // from safe_dist down to just above stop_dist.
  if (std::isfinite(d) && d <= params_.safe_dist) {
    const float t = (d - params_.stop_dist)
                  / (params_.safe_dist - params_.stop_dist);
    return cmd_v * std::clamp(t, 0.0F, 1.0F);
  }
  return cmd_v;
}

inline bool CollisionGuard::stopped(
    const std::chrono::steady_clock::time_point &) const {
  std::lock_guard<std::mutex> lock(mtx_);
  return last_stop_;
}

inline std::chrono::milliseconds CollisionGuard::blocked_for(
    const std::chrono::steady_clock::time_point &now) const {
  std::lock_guard<std::mutex> lock(mtx_);
  if (last_ok_time_ == std::chrono::steady_clock::time_point::min()) {
    // Never un-stopped: measure from the first scan (conservative — always
    // shorter than real, safe for the anti-deadlock timeout).
    if (scan_.recv_time == std::chrono::steady_clock::time_point::min()) {
      return {};
    }
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now - scan_.recv_time);
  }
  return std::chrono::duration_cast<std::chrono::milliseconds>(
      now - last_ok_time_);
}

inline float CollisionGuard::nearest_distance() const {
  std::lock_guard<std::mutex> lock(mtx_);
  return nearest_in_fov_locked();
}

}  // namespace execution
}  // namespace domain
}  // namespace amr

#endif
