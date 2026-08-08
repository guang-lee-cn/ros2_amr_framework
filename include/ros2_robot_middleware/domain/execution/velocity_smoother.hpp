#pragma once
/// @brief  VelocitySmoother — time-domain acceleration limits (G2-D).
///
/// PurePursuit's target_speed() only limits speed in the geometry domain
/// (curvature, distance-to-goal). The output can still jump between speeds
/// from one control tick to the next. This smoother clamps the rate of
/// change of linear/angular velocity over dt, so the base accelerates and
/// brakes smoothly instead of snapping — required for comfortable, safe
/// commercial operation.
///
/// Thread safety: stateless const member — no internal state, safe to call
/// from the single motor execute loop.

#include "ros2_robot_middleware/domain/execution/pure_pursuit.hpp"

#include <algorithm>
#include <cmath>

namespace amr::domain::execution {

struct VelocitySmoother {
  struct Params {
    float max_accel = 0.5F;          // m/s²  linear acceleration
    float max_decel = 0.8F;          // m/s²  linear deceleration (braking > accel)
    float max_angular_accel = 1.0F;  // rad/s² angular acceleration
  };

  VelocitySmoother() : params_(Params{}) {}
  explicit VelocitySmoother(const Params &p) : params_(p) {}

  /// Limit `desired` relative to `last` by the acceleration limits over `dt`.
  /// Acceleration uses max_accel, deceleration uses max_decel (asymmetric).
  Twist2D smooth(const Twist2D &desired, const Twist2D &last, float dt) const {
    const float max_lin = (desired.linear > last.linear)
                              ? params_.max_accel * dt
                              : params_.max_decel * dt;
    const float max_ang = params_.max_angular_accel * dt;
    Twist2D out;
    out.linear  = last.linear  + std::clamp(desired.linear  - last.linear,  -max_lin, max_lin);
    out.angular = last.angular + std::clamp(desired.angular - last.angular, -max_ang, max_ang);
    return out;
  }

  const Params &params() const { return params_; }

private:
  Params params_;
};

}  // namespace amr::domain::execution
