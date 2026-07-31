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

#include "ros2_robot_middleware/hal/sensor/isensor.hpp"

#include <cmath>
#include <cstring>
#include <mutex>
#include <random>

namespace amr::hal::sensor {

// ══════════════════════════════════════════════════════════════════════
// SimulatedLidar — value type, thread-safe by construction
//
// read_impl() fills a stack-allocated LidarScan. Data lives in the
// caller's stack frame — no shared buffer, no mutex needed.
// ══════════════════════════════════════════════════════════════════════

class SimulatedLidar : public amr::hal::sensor::SensorBase<SimulatedLidar,
                        amr::hal::sensor::LidarScan> {
public:
    bool read_impl(amr::hal::sensor::LidarScan &out) {
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

class SimulatedImu : public amr::hal::sensor::SensorBase<SimulatedImu,
                       amr::hal::sensor::ImuData> {
public:
    bool read_impl(amr::hal::sensor::ImuData &out) {
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

class SimulatedCamera : public amr::hal::sensor::SensorBase<SimulatedCamera,
                          amr::hal::sensor::CameraFrame> {
public:
    // Camera image data is unused in current pipeline — only the read() success/fail
    // (camera_ok) feeds into degradation policy. Return minimal frame with no payload.
    // See ITERATION.md P1c for rationale.
    bool read_impl(amr::hal::sensor::CameraFrame &out) {
        out.data     = nullptr;
        out.size     = 0;
        out.width    = 1;
        out.height   = 1;
        out.capacity = 0;
        return true;
    }
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

} // namespace amr::hal::sensor
