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
// SimulatedCamera — caller-owns-buffer, mutex-protected fill
//
// CameraFrame is ~900KB — too large for stack copy and too large to
// allocate on every read(). Caller pre-allocates a heap buffer once,
// passes pointer + capacity in CameraFrame::data/capacity.
//
// read_impl() fills it under mutex lock. Caller reuses the same buffer
// across reads — zero heap allocation in the hot path.
//
// Contrast with Lidar/IMU:
//   - Same CRTP interface:              sensor.read(frame) → bool
//   - Different internal strategy:      mutex instead of value copy
//   - Caller must pre-allocate:         frame.data = new uint8_t[capacity]
// ══════════════════════════════════════════════════════════════════════

class SimulatedCamera : public amr::domain::sensor::SensorBase<SimulatedCamera,
                          amr::domain::sensor::CameraFrame> {
public:
    bool read_impl(amr::domain::sensor::CameraFrame &out) {
        if (!out.data || out.capacity < amr::domain::sensor::CameraFrame::kMaxSize) {
            health_ = 2;  // error: caller didn't provide adequate buffer
            return false;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        // Simulate: fill with random noise
        for (size_t i = 0; i < amr::domain::sensor::CameraFrame::kMaxSize; ++i) {
            out.data[i] = static_cast<uint8_t>(rand() % 256);
        }
        out.size   = amr::domain::sensor::CameraFrame::kMaxSize;
        out.width  = 640;
        out.height = 480;
        return true;
    }

private:
    std::mutex mutex_;
};

// ══════════════════════════════════════════════════════════════════════
// Usage in PerceptionService (same call pattern for all three):
//
//   template<typename LidarT, typename ImuT, typename CameraT>
//   class PerceptionService {
//       LidarT   &lidar_;
//       ImuT     &imu_;
//       CameraT  &camera_;
//       uint8_t   camera_buf_[CameraFrame::kMaxSize];  // pre-allocated once
//   public:
//       void tick() {
//           LidarScan   lidar_scan;
//           ImuData     imu_data;
//           CameraFrame cam_frame;
//           cam_frame.data     = camera_buf_;            // caller provides buffer
//           cam_frame.capacity = CameraFrame::kMaxSize;
//
//           lidar_.read(lidar_scan);    // value copy, ~8KB
//           imu_.read(imu_data);        // value copy, 12B
//           camera_.read(cam_frame);    // mutex-protected fill, 900KB
//           // ... fuse ...
//       }
//   };
// ══════════════════════════════════════════════════════════════════════

} // namespace amr::infrastructure::sensors
