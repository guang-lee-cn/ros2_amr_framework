#pragma once
/// @file   sensor_interface.hpp
/// @brief  Two-level compile-time sensor abstraction.
///
/// Layer 1 — SensorBase<Derived, DataType> (CRTP):
///   Common contract: read(), health(). Zero virtual overhead via static dispatch.
///
/// Layer 2 — Concrete sensor types:
///   SimulatedLidar / SimulatedImu / SimulatedCamera (timer-based)
///   SickTiM781Adapter / Bmi088ImuAdapter / RealSenseAdapter (real drivers)
///
/// Usage (PerceptionService, template policy):
///   template<typename LidarT, typename ImuT, typename CameraT>
///   class PerceptionService { ... };
///
/// Why not virtual functions:
///   - Sensor type is known at compile time — no runtime polymorphism needed
///   - CRTP enables compiler inlining of adapter code
///   - No vtable pointer, no indirect call overhead

#include <cstddef>
#include <cstdint>

namespace amr::domain::sensor {

// ── Data types (ROS2-free, zero-copy) ───────────────────────────────

struct LidarScan {
    static constexpr int kMaxRanges = 2048;
    const float *ranges = nullptr;
    size_t       range_count = 0;
    float        angle_min = 0.0F;
    float        angle_increment = 0.0F;
};

struct ImuData {
    float linear_accel_x = 0.0F;
    float linear_accel_y = 0.0F;
    float angular_vel_z = 0.0F;
};

struct CameraFrame {
    static constexpr int kMaxWidth  = 1920;
    static constexpr int kMaxHeight = 1080;
    const uint8_t *data = nullptr;
    size_t         size = 0;
    uint16_t       width = 0;
    uint16_t       height = 0;
};

// ── Layer 1: CRTP base (common contract) ────────────────────────────

template <typename Derived, typename DataType>
class SensorBase {
public:
    /// Read latest sensor data. Returns false if no new data.
    /// Dispatches to Derived::read_impl via CRTP — zero virtual overhead.
    bool read(DataType &out) {
        return static_cast<Derived *>(this)->read_impl(out);
    }

    /// Sensor health: 0=ok, >0=degraded
    int health() const { return health_; }

protected:
    int health_ = 0;  // derived class updates this directly
};

// ── Layer 2: Concrete sensor type aliases ───────────────────────────

// Each concrete sensor inherits SensorBase<Self, DataType> and implements:
//   bool read_impl(DataType &out);  — sensor-specific read logic
//
// Example (in infrastructure/sensors/):
//
//   class SimulatedLidar : public SensorBase<SimulatedLidar, LidarScan> {
//       bool read_impl(LidarScan &out) { ... }
//   };
//
//   class SickTiM781Adapter : public SensorBase<SickTiM781Adapter, LidarScan> {
//       bool read_impl(LidarScan &out) { ... }
//   };

// Compile-time contract enforcement (C++17, SFINAE-based)
//
// At template instantiation site (e.g. PerceptionService constructor), use:
//   static_assert(
//       std::is_invocable_r_v<bool, decltype(&SensorT::read), SensorT&, DataType&>,
//       "SensorT must implement bool read(DataType&)");
//
// This gives a clear error message at compile time if the sensor contract
// is violated — no virtual dispatch, no runtime check.

} // namespace amr::domain::sensor
