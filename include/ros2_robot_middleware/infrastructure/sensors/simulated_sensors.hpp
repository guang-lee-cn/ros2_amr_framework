#pragma once
/// @file   simulated_sensors.hpp
/// @brief  Three sensor implementations — same CRTP interface, different internal strategies.
///
/// Demonstrates the hybrid thread-safety design:
///   - LidarScan:  value type (8KB stack copy)  — thread-safe by construction
///   - ImuData:    value type (12B stack copy)   — thread-safe by construction
///   - CameraFrame: caller-owned heap buffer      — mutex-protected fill
///
/// External API is identical across all three:
///   sensor.read(data) → bool
///   sensor.health()   → int
///   sensor.init()     → bool   (default no-op for simulated)
///   sensor.shutdown()         (default no-op for simulated)

#include "ros2_robot_middleware/domain/perception/sensor_interface.hpp"

#include <cmath>
#include <cstring>
#include <mutex>
#include <random>

namespace amr::infrastructure::sensors {

// ══════════════════════════════════════════════════════════════════════
// SimulatedLidar — value type, thread-safe by construction
//
// read_impl() fills a stack-allocated LidarScan. Data lives in the
// caller's stack frame — no shared buffer, no mutex needed.
// ══════════════════════════════════════════════════════════════════════

class SimulatedLidar : public amr::domain::sensor::SensorBase<SimulatedLidar,
                        amr::domain::sensor::LidarScan> {
public:
    bool read_impl(amr::domain::sensor::LidarScan &out) {
        // Write directly into caller-owned stack buffer. No lock needed —
        // this is the only thread that touches `out`.
        out.range_count     = 360;
        out.angle_min       = -M_PI;
        out.angle_increment = 2.0F * M_PI / 360.0F;

        for (size_t i = 0; i < out.range_count; ++i) {
            float angle = out.angle_min + i * out.angle_increment;
            out.ranges[i] = 2.0F + 1.5F * std::sin(angle * 3.0F) * std::cos(angle * 2.0F);
        }
        return true;
    }
};

// ══════════════════════════════════════════════════════════════════════
// SimulatedImu — value type, 12 bytes, essentially free
//
// Same pattern as Lidar but trivially small. Zero concern.
// ══════════════════════════════════════════════════════════════════════

class SimulatedImu : public amr::domain::sensor::SensorBase<SimulatedImu,
                       amr::domain::sensor::ImuData> {
public:
    bool read_impl(amr::domain::sensor::ImuData &out) {
        out.linear_accel_x = 0.0F;
        out.linear_accel_y = 0.0F;
        out.angular_vel_z  = 0.0F;
        return true;
    }
};

// ══════════════════════════════════════════════════════════════════════
// SimulatedCamera — sensor owns buffer, returns view to caller
//
// CameraFrame is ~900KB — too large for stack copy. Sensor manages its
// own internal heap buffer (single allocation). read_impl() fills it
// under mutex and returns a view (pointer + metadata) to the caller.
//
// Contract: returned pointer is valid until next read() on this sensor.
//           Caller processes and discards — does not save the pointer.
//
// Caller perspective: identical to Lidar/IMU:
//   CameraFrame frame;
//   camera_.read(frame);        // sensor fills in frame
//   process(frame.data, ...);   // caller uses data immediately
// ══════════════════════════════════════════════════════════════════════

class SimulatedCamera : public amr::domain::sensor::SensorBase<SimulatedCamera,
                          amr::domain::sensor::CameraFrame> {
public:
    bool read_impl(amr::domain::sensor::CameraFrame &out) {
        std::lock_guard<std::mutex> lock(mutex_);

        // Fill internal buffer under lock
        for (size_t i = 0; i < amr::domain::sensor::CameraFrame::kMaxSize; ++i) {
            buf_[i] = static_cast<uint8_t>(rand() % 256);
        }

        // Return view into sensor-owned buffer (stable until next read())
        out.data     = buf_;
        out.size     = amr::domain::sensor::CameraFrame::kMaxSize;
        out.width    = 640;
        out.height   = 480;
        out.capacity = 0;  // unused — caller doesn't need to know
        return true;
    }

private:
    std::mutex mutex_;
    uint8_t buf_[amr::domain::sensor::CameraFrame::kMaxSize]{};  // sensor owns this
};

// ══════════════════════════════════════════════════════════════════════
// Usage in PerceptionService — identical call pattern across all three:
//
//   template<typename LidarT, typename ImuT, typename CameraT>
//   class PerceptionService {
//       LidarT   &lidar_;
//       ImuT     &imu_;
//       CameraT  &camera_;
//   public:
//       void tick() {
//           LidarScan   lidar_scan;    // 8KB on stack
//           ImuData     imu_data;      // 12B on stack
//           CameraFrame cam_frame;     // just metadata (ptr+size+dims)
//
//           lidar_.read(lidar_scan);   // value copy
//           imu_.read(imu_data);       // value copy
//           camera_.read(cam_frame);   // view into sensor-owned buffer
//           // ... fuse ...
//       }
//   };
//
// Contrast: Lidar copies data (safe), Camera returns view (efficient).
//           Both use identical read() call — zero API surface difference.
// ══════════════════════════════════════════════════════════════════════

} // namespace amr::infrastructure::sensors
