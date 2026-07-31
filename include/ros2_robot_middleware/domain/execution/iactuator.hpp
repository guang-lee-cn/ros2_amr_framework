#ifndef ROS2_ROBOT_MIDDLEWARE_DOMAIN_IACTUATOR_HPP_
#define ROS2_ROBOT_MIDDLEWARE_DOMAIN_IACTUATOR_HPP_

/// @file   iactuator.hpp
/// @brief  Actuator hardware abstraction — bidirectional command/feedback.
///
/// Complements ISensor<T> (one-way input). Actuators have a command path
/// (write) and a feedback path (read). Both are pull-based — the caller
/// ticks at control rate.
///
///   IActuator<Cmd, Fb>
///     ├── write(cmd)  → 下发指令到硬件（CAN/UART/以太网）
///     └── read(fb)    → 读回编码器/状态（闭环反馈）
///
/// Data types (ROS2-free):
///   struct WheelCmd { float vx; float vy; float wz; };   // cmd_vel
///   struct WheelFeedback { float left_wheel_rps; float right_wheel_rps; };
///
/// Example: diff-drive base
///   class DiffDriveAdapter : public ActuatorBase<DiffDriveAdapter, WheelCmd, WheelFeedback> {
///     bool write_impl(const WheelCmd &cmd) override { can_send(cmd); }
///     bool read_impl(WheelFeedback &fb) override { can_recv(fb); }
///   };

#include <cstdint>

namespace amr {
namespace domain {
namespace execution {

template <typename Cmd, typename Fb>
class IActuator {
public:
  virtual ~IActuator() = default;

  /// Send command to hardware. Returns false on transport failure.
  virtual bool write(const Cmd &cmd) = 0;

  /// Read feedback from hardware. Returns false if no new data.
  virtual bool read(Fb &feedback) = 0;

  virtual bool init() { return true; }
  virtual void shutdown() {}
  virtual int health() const { return health_; }

protected:
  int health_ = 0;
};

// ── CRTP helper — compile-time dispatch for concrete implementations ──
template <typename Derived, typename Cmd, typename Fb>
class ActuatorBase : public IActuator<Cmd, Fb> {
public:
  bool write(const Cmd &cmd) final { return static_cast<Derived *>(this)->write_impl(cmd); }
  bool read(Fb &fb) final { return static_cast<Derived *>(this)->read_impl(fb); }
  bool init() final { return static_cast<Derived *>(this)->init_impl(); }
  void shutdown() final { static_cast<Derived *>(this)->shutdown_impl(); }

  bool init_impl() { return true; }
  void shutdown_impl() {}
};

}  // namespace execution
}  // namespace domain
}  // namespace amr

#endif
