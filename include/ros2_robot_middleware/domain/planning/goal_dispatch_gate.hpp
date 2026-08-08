#pragma once
/// @file   goal_dispatch_gate.hpp
/// @brief  Decision dispatch gate — fusion-ready gating + same-goal dedup.
///
/// Two integration bugs this gate prevents (discovered in sim validation):
///   1. Cold start: A* planned through the wall because no perception had
///      arrived yet — the grid was empty so the straight line looked clear.
///      Fixed by gating dispatch on the fusion heartbeat (fusion_ready).
///   2. Same-goal re-dispatch: the dedup compared the map-frame goal param
///      against the odom-frame dispatched value — with a real map→odom
///      offset the values never matched and the robot chased its own tail.
///      Fixed by deduping on the map-frame goal identity, independent of the
///      odom-frame value actually sent to the motor.
///
/// Pure domain logic, zero ROS2 — fully unit-testable.

#include <cmath>
#include <mutex>

namespace amr {
namespace domain {
namespace planning {

class GoalDispatchGate {
public:
  static constexpr float kEps = 1e-3F;

  /// Fusion heartbeat set this: false → no dispatch (grid not trustworthy).
  void set_fusion_ready(bool ready) {
    std::lock_guard<std::mutex> lock(mtx_);
    fusion_ready_ = ready;
  }

  bool fusion_ready() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return fusion_ready_;
  }

  /// Dispatch decision for a task goal given in MAP-frame coordinates.
  /// Denied while fusion is not ready, or when the same task goal (map
  /// identity) was already dispatched once.
  bool should_dispatch(float goal_map_x, float goal_map_y) const {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!fusion_ready_) { return false; }
    if (dispatched_ && std::fabs(goal_map_x - last_map_x_) < kEps &&
                       std::fabs(goal_map_y - last_map_y_) < kEps) {
      return false;
    }
    return true;
  }

  /// Record the dispatched task-goal identity (map frame).
  void note_dispatched(float goal_map_x, float goal_map_y) {
    std::lock_guard<std::mutex> lock(mtx_);
    last_map_x_ = goal_map_x;
    last_map_y_ = goal_map_y;
    dispatched_ = true;
  }

  void reset() {
    std::lock_guard<std::mutex> lock(mtx_);
    dispatched_ = false;
  }

private:
  float last_map_x_ = 0.0F;
  float last_map_y_ = 0.0F;
  bool dispatched_ = false;
  bool fusion_ready_ = false;
  mutable std::mutex mtx_;
};

}  // namespace planning
}  // namespace domain
}  // namespace amr
