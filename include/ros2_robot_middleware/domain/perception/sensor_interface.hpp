#pragma once
/// @file   sensor_interface.hpp
/// @brief  Hardware abstraction layer — sensor interfaces for domain layer.
///
/// Each sensor type defines a pure-virtual interface. Domain code depends
/// only on these interfaces, never on ROS2 message types or driver SDKs.
///
/// Infrastructure layer (infrastructure/sensors/) provides adapters that
/// translate real sensor data (sensor_msgs, driver-specific types) into
/// these domain types.
///
/// Testing: mock implementations inject synthetic data without launching
/// ROS2 nodes — enabling fast, deterministic domain-layer unit tests.

#include <cstddef>
#include <cstdint>

namespace amr::domain::sensor {

// ── Raw sensor data types (ROS2-free) ───────────────────────────────

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

// ── Abstract sensor interfaces ───────────────────────────────────────

class ILidarSensor {
public:
    virtual ~ILidarSensor() = default;

    /// Read latest scan. Returns true if new data available.
    /// Thread-safe — may be called from real-time callback.
    virtual bool read(LidarScan &out) = 0;

    /// Sensor health status (0=ok, >0=degraded)
    virtual int health() const { return health_; }

protected:
    int health_ = 0;
};

class IImuSensor {
public:
    virtual ~IImuSensor() = default;
    virtual bool read(ImuData &out) = 0;
    virtual int health() const { return health_; }

protected:
    int health_ = 0;
};

class ICameraSensor {
public:
    virtual ~ICameraSensor() = default;
    virtual bool read(CameraFrame &out) = 0;
    virtual int health() const { return health_; }

protected:
    int health_ = 0;
};

} // namespace amr::domain::sensor
