#pragma once
/// @file   sensor_interface.hpp
/// @brief  Two-level compile-time sensor abstraction.
///
/// Layer 1 — SensorBase<Derived, DataType> (CRTP):
///   Common contract: read(), init(), shutdown(), health().
///   Zero virtual overhead via static dispatch.
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
///
/// THREAD SAFETY: Structural, not by convention.
///
///   Data types own their memory (stack array, no raw pointer).
///   Each read() produces an independent copy of sensor data.
///   Driver thread and consumer thread never share a buffer —
///   race condition eliminated at the type level.
///
///   Cost: LidarScan = 8KB stack copy per read (10Hz → 80KB/s).
///         CameraFrame = 900KB (640×480×3). At 5Hz → 4.5MB/s.
///         Both well below any modern CPU's memory bandwidth.

#include <cstddef>
#include <cstdint>

namespace amr::domain::sensor {

// ── Data types (ROS2-free, value semantics → thread-safe by construction) ─
//
// Each read() produces an independent copy. No shared pointers between
// sensor driver and consumer threads — race eliminated at the type level.
//
// Cost: LidarScan = 8KB (2048 floats) per read. 10Hz → 80KB/s. Negligible.
//       CameraFrame = 900KB (640×480×3). 5Hz → 4.5MB/s. Acceptable.

struct LidarScan {
    static constexpr int kMaxRanges = 2048;
    float ranges[kMaxRanges] = {};   // owns data — no shared pointer
    size_t range_count = 0;
    float  angle_min = 0.0F;
    float  angle_increment = 0.0F;
};

struct ImuData {
    float linear_accel_x = 0.0F;
    float linear_accel_y = 0.0F;
    float angular_vel_z = 0.0F;
};

struct CameraFrame {
    static constexpr int kMaxWidth  = 640;
    static constexpr int kMaxHeight = 480;
    uint8_t  data[kMaxWidth * kMaxHeight * 3] = {};  // 900KB, value semantics
    size_t   size = 0;
    uint16_t width = 0;
    uint16_t height = 0;
};

// ── Layer 1: CRTP base (common contract) ────────────────────────────

template <typename Derived, typename DataType>
class SensorBase {
public:
    // ── Mandatory: data path ────────────────────────────────────────

    /// Read latest sensor data. Returns false if no new data.
    /// Dispatches to Derived::read_impl via CRTP — zero virtual overhead.
    bool read(DataType &out) {
        return static_cast<Derived *>(this)->read_impl(out);
    }

    // ── Mandatory: health ────────────────────────────────────────────

    /// Sensor health: 0=ok, 1=degraded, 2=error, 3=fatal
    int health() const { return health_; }

    // ── Optional lifecycle hooks (real hardware only) ────────────────
    //
    /// Called once before first read(). Default: no-op.
    /// Override in real sensor adapters for I2C open, register config, etc.
    bool init() { return static_cast<Derived *>(this)->init_impl(); }

    /// Called once after last read(), before destructor. Default: no-op.
    /// Override for graceful shutdown: stop sampling, close fd, power down.
    void shutdown() { static_cast<Derived *>(this)->shutdown_impl(); }

    /// Default no-op implementations — simulated sensors don't need these
    bool init_impl() { return true; }
    void shutdown_impl() {}

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
