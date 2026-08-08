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

/// 圆形障碍物（场景配置）。高度用于区分"低矮障碍"：
/// lidar 安装高度以上的障碍可见，以下（如台阶/矮桩）由相机深度补盲。
struct Obstacle {
    float x = 0.0F;
    float y = 0.0F;
    float radius = 0.3F;
    float bottom_height = 0.0F;   // 底高（m）
    float top_height    = 1.5F;   // 顶高（m）；默认高 → 现有场景行为不变
};

/// 演示场景 — 障碍物布局
struct Scenario {
    std::vector<Obstacle> obstacles;
};

/// 从原点沿 angle 方向的射线到圆形障碍的距离（两传感器共用）。
/// 未命中返回 miss_range（哨兵），与 SimulatedLidar::kInvalidRange 语义一致。
inline float ray_obstacle_dist(float angle, const Obstacle &obs, float miss_range) {
    float dx = std::cos(angle);
    float dy = std::sin(angle);
    float cx = obs.x;
    float cy = obs.y;
    float t = cx * dx + cy * dy;
    if (t < 0.0F) { return miss_range; }  // 障碍在射线反方向
    float px = cx - t * dx;
    float py = cy - t * dy;
    float perp = std::sqrt(px * px + py * py);
    if (perp > obs.radius) { return miss_range; }  // 射线未穿过障碍
    float half_chord = std::sqrt(obs.radius * obs.radius - perp * perp);
    float entry = t - half_chord;
    return entry > 0.0F ? entry : miss_range;
}

/// SimulatedLidar — 场景驱动点云生成。
/// 默认：空旷环境（远处点云）。配置障碍物后，在障碍方向返回障碍表面距离。
class SimulatedLidar : public amr::hal::sensor::SensorBase<SimulatedLidar,
                        amr::hal::sensor::LidarScan> {
public:
    /// 无效距离：未命中障碍的射线返回此值（> 量程），
    /// 下游 DBSCAN 的 max_range 过滤会将其剔除，避免把远空当成"物体"。
    static constexpr float kInvalidRange = 6.5F;

    explicit SimulatedLidar(Scenario scenario = {}, float mount_height = 0.3F)
        : scenario_(std::move(scenario)), max_range_(6.0F), mount_height_(mount_height) {}

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
                // 低矮障碍（顶高低于激光平面）2D lidar 不可见 — 由相机深度补盲。
                if (obs.top_height < mount_height_) continue;
                float d = ray_obstacle_dist(angle, obs, kInvalidRange);
                if (d < hit) hit = d;
            }
            out.ranges[i] = hit;
        }
        return true;
    }

    const Scenario &scenario() const { return scenario_; }
    void set_scenario(Scenario s) { scenario_ = std::move(s); }

private:
    Scenario scenario_;
    float max_range_;
    float mount_height_;
};

// ══════════════════════════════════════════════════════════════════════
// SimulatedImu — value type, 12 bytes, essentially free
//
// Same pattern as Lidar but trivially small. Zero concern.
// ══════════════════════════════════════════════════════════════════════

/// 可配置加速度曲线（确定性、可单测）。默认 0 保持 demo 行为。
struct ImuProfile {
    float accel_x = 0.0F;
    float accel_y = 0.0F;
};

class SimulatedImu : public amr::hal::sensor::SensorBase<SimulatedImu,
                       amr::hal::sensor::ImuData> {
public:
    explicit SimulatedImu(ImuProfile profile = {}) : profile_(profile) {}

    bool read_impl(amr::hal::sensor::ImuData &out) {
        out.linear_accel_x = profile_.accel_x;
        out.linear_accel_y = profile_.accel_y;
        out.angular_vel_z  = 0.0F;
        return true;
    }

private:
    ImuProfile profile_;
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

/// 深度相机参数 — 前方 FOV 射线模型（1D 水平扫描线）。
struct CameraParams {
    float mount_height_m       = 0.5F;   // 相机安装高度
    float min_visible_height_m = 0.05F;  // 障碍顶高 >= 此值相机可见
    float fov_deg              = 60.0F;
    int   num_rays             = 121;    // 0.5° 步进
    float max_depth_m          = 6.0F;
    float min_depth_m          = 0.2F;
};

/// 场景驱动深度相机 — 生成前方 FOV 深度扫描线（补 lidar 盲区：低矮障碍）。
/// RGB 图像缓冲管线未用（保持空）；read() 成功仍驱动降级。
class SimulatedCamera : public amr::hal::sensor::SensorBase<SimulatedCamera,
                          amr::hal::sensor::CameraFrame> {
public:
    static constexpr float kInvalidDepth = 0.0F;  // 哨兵：无返回（毫米值不可能为 0）

    explicit SimulatedCamera(Scenario scenario = {}, CameraParams params = {})
        : scenario_(std::move(scenario)), params_(params) {}

    bool read_impl(amr::hal::sensor::CameraFrame &out) {
        out.data     = nullptr;
        out.size     = 0;
        out.width    = 1;
        out.height   = 1;
        out.capacity = 0;

        // 深度扫描线：射线数 num_rays，角度从 -fov/2 到 +fov/2（正 = 车左，与 lidar y 一致）。
        const float fov_rad = params_.fov_deg * static_cast<float>(M_PI) / 180.0F;
        const int n = params_.num_rays;
        out.depth.assign(static_cast<size_t>(n), 0U);
        for (int i = 0; i < n; ++i) {
            const float angle = -fov_rad / 2.0F +
                                static_cast<float>(i) * (fov_rad / static_cast<float>(n - 1));
            float best = kInvalidDepth;
            for (const auto &obs : scenario_.obstacles) {
                // 相机俯视地面：凡顶高超过可见阈值的障碍均返回深度。
                if (obs.top_height < params_.min_visible_height_m) continue;
                float d = ray_obstacle_dist(angle, obs, kInvalidDepth);
                if (d > 0.0F && (best == kInvalidDepth || d < best)) best = d;
            }
            // 有效深度写入毫米；超出相机量程视为无返回。
            if (best > params_.min_depth_m && best < params_.max_depth_m) {
                out.depth[static_cast<size_t>(i)] = static_cast<uint16_t>(best * 1000.0F);
            }
        }
        return true;
    }

private:
    Scenario scenario_;
    CameraParams params_;
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
