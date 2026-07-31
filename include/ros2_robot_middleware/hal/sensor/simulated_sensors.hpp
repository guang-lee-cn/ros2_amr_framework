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
#include <vector>

namespace amr::hal::sensor {

// ══════════════════════════════════════════════════════════════════════
// SimulatedLidar — value type, thread-safe by construction
//
// read_impl() fills a stack-allocated LidarScan. Data lives in the
// caller's stack frame — no shared buffer, no mutex needed.
// ══════════════════════════════════════════════════════════════════════

/// 圆形障碍物（场景配置）
struct Obstacle {
    float x = 0.0F;
    float y = 0.0F;
    float radius = 0.3F;
};

/// 演示场景 — 障碍物布局
struct Scenario {
    std::vector<Obstacle> obstacles;
};

/// SimulatedLidar — 场景驱动点云生成。
/// 默认：空旷环境（远处点云）。配置障碍物后，在障碍方向返回障碍表面距离。
class SimulatedLidar : public amr::hal::sensor::SensorBase<SimulatedLidar,
                        amr::hal::sensor::LidarScan> {
public:
    /// 无效距离：未命中障碍的射线返回此值（> 量程），
    /// 下游 DBSCAN 的 max_range 过滤会将其剔除，避免把远空当成"物体"。
    static constexpr float kInvalidRange = 6.5F;

    explicit SimulatedLidar(Scenario scenario = {})
        : scenario_(std::move(scenario)), max_range_(6.0F) {}

    bool read_impl(amr::hal::sensor::LidarScan &out) {
        out.range_count     = 360;
        out.angle_min       = -M_PI;
        out.angle_increment = 2.0F * M_PI / 360.0F;

        for (size_t i = 0; i < out.range_count; ++i) {
            float angle = out.angle_min + i * out.angle_increment;
            // 沿射线找最近障碍物；未命中 = 无效距离（超出量程），
            // 让下游 DBSCAN 的 max_range 过滤掉，避免把远处空当"物体"。
            float hit = kInvalidRange;
            for (const auto &obs : scenario_.obstacles) {
                float d = ray_obstacle_dist(angle, obs);
                if (d < hit) hit = d;
            }
            out.ranges[i] = hit;
        }
        return true;
    }

    const Scenario &scenario() const { return scenario_; }
    void set_scenario(Scenario s) { scenario_ = std::move(s); }

private:
    /// 从原点沿 angle 方向的射线到圆形障碍的距离。
    /// 用圆心到射线的最小距离判断是否相交，返回交点距离。
    static float ray_obstacle_dist(float angle, const Obstacle &obs) {
        // 射线方向
        float dx = std::cos(angle);
        float dy = std::sin(angle);
        // 圆心相对原点
        float cx = obs.x;
        float cy = obs.y;
        // 圆心到射线的投影距离（t = 投影参数）
        float t = cx * dx + cy * dy;
        if (t < 0.0F) return kInvalidRange;  // 障碍在射线反方向
        // 投影点到圆心的垂直距离
        float px = cx - t * dx;
        float py = cy - t * dy;
        float perp = std::sqrt(px * px + py * py);
        if (perp > obs.radius) return kInvalidRange;  // 射线未穿过障碍
        // 进入圆面的距离 = 投影距离 - 弦半长
        float half_chord = std::sqrt(obs.radius * obs.radius - perp * perp);
        float entry = t - half_chord;
        return entry > 0.0F ? entry : kInvalidRange;
    }

    Scenario scenario_;
    float max_range_;
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
