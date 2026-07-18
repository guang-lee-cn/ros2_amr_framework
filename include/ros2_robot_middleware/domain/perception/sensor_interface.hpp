#pragma once
/// @file   sensor_interface.hpp
/// @brief  Runtime-polymorphic sensor abstraction for dependency injection.
///
/// ISensor<DataType>  — abstract interface (one virtual call per read())
/// SensorBase<Derived, DataType> — CRTP helper for concrete implementations
///
/// Dependency injection pattern:
///   PerceptionService(ISensor<LidarScan>&, ISensor<ImuData>&, ISensor<CameraFrame>&)
///   FusionNode creates concrete sensors, injects them — no templates needed.
///   Tests inject mock sensors — no ROS2 dependency.
///
/// Overhead: one virtual call per sensor per tick (~5-10ns). At 100Hz IMU →
///           500ns/s. Negligible vs 10ms callback budget.

#include <cstddef>
#include <cstdint>

namespace amr::domain::sensor {

// ── Data types (ROS2-free) ──────────────────────────────────────────

struct LidarScan {
    static constexpr int kMaxRanges = 2048;
    float ranges[kMaxRanges] = {};
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
    static constexpr size_t kMaxSize = kMaxWidth * kMaxHeight * 3;
    uint8_t *data = nullptr;
    size_t   capacity = 0;
    size_t   size = 0;
    uint16_t width = 0;
    uint16_t height = 0;
#ifndef NDEBUG
    uint64_t generation = 0;
#endif
};

// ── Abstract interface (runtime polymorphism) ────────────────────────

template <typename DataType>
class ISensor {
public:
    virtual ~ISensor() = default;
    virtual bool read(DataType &out) = 0;
    virtual bool init()           { return true; }
    virtual void shutdown()       {}
    virtual int  health() const   { return health_; }

protected:
    int health_ = 0;
};

// ── CRTP helper (compile-time dispatch for concrete implementations) ─

template <typename Derived, typename DataType>
class SensorBase : public ISensor<DataType> {
public:
    bool read(DataType &out) final {
        return static_cast<Derived *>(this)->read_impl(out);
    }
    bool init() final {
        return static_cast<Derived *>(this)->init_impl();
    }
    void shutdown() final {
        static_cast<Derived *>(this)->shutdown_impl();
    }

    // Default no-ops — override in derived
    bool init_impl()    { return true; }
    void shutdown_impl() {}
};

} // namespace amr::domain::sensor
