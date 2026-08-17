#pragma once
/// @brief  SimulatedScene — ray-cast 2D scene for demoing the compute container
///         without a physics simulator (gz-sim gpu_lidar has engine bugs that
///         break dynamic scans on this platform — see architecture doc §7).
///
/// Provides: ray-cast LaserScan generation (walls + box obstacles) and
/// kinematic odom integration. Pure domain logic — unit-testable, no ROS2.
///
/// Demo topology (warehouse, metres):
///   walls:  x∈[0,19], y∈[-2,2]
///   box:    centered (8,0), 0.5×0.5
///
/// Thread safety: const member generate_scan() is read-only over the scene;
/// step() is a static pure function.

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace amr::domain::simulation {

struct Pose {
  float x = 0.0F;
  float y = 0.0F;
  float theta = 0.0F;
};

struct Segment {
  float x1, y1, x2, y2;
};

struct SceneParams {
  float range_max = 10.0F;
  int beam_count = 360;
  float lidar_offset_x = 0.25F;
  // 场景预设：rack_4box（4 个孤立 box）| rack_3c（3C 料架排窄通道）| warehouse_open（空旷）
  std::string scene_name = "rack_4box";
};

class SimulatedScene {
public:
  SimulatedScene() : SimulatedScene(SceneParams{}) {}

  explicit SimulatedScene(const SceneParams &p) : params_(p) {
    walls_ = {
        {0.0F, -5.0F, 0.0F, 5.0F},    // west x=0
        {19.0F, -5.0F, 19.0F, 5.0F},  // east x=19
        {0.0F, -5.0F, 19.0F, -5.0F},  // south y=-5
        {0.0F, 5.0F, 19.0F, 5.0F},    // north y=5
    };
    // box helper: center (cx,cy) size (sx,sy) → 4 edges
    auto box = [](float cx, float cy, float sx, float sy) {
      float hx = sx * 0.5F, hy = sy * 0.5F;
      return std::vector<Segment>{
          {cx - hx, cy - hy, cx + hx, cy - hy}, {cx + hx, cy - hy, cx + hx, cy + hy},
          {cx + hx, cy + hy, cx - hx, cy + hy}, {cx - hx, cy + hy, cx - hx, cy - hy}};
    };
    obstacles_.clear();
    if (params_.scene_name == "rack_3c") {
      // 3C 半导体车间：4 排料架（6m×0.5m，间距 1.7m，通道 1.2m）+ 2 机台
      for (float ry : {-2.5F, -0.8F, 0.8F, 2.5F}) {
        auto segs = box(7.0F, ry, 6.0F, 0.5F);
        obstacles_.insert(obstacles_.end(), segs.begin(), segs.end());
      }
      auto m1 = box(17.0F, 4.0F, 1.0F, 1.0F);
      auto m2 = box(17.0F, -4.0F, 1.0F, 1.0F);
      obstacles_.insert(obstacles_.end(), m1.begin(), m1.end());
      obstacles_.insert(obstacles_.end(), m2.begin(), m2.end());
    } else if (params_.scene_name == "warehouse_open") {
      // 空旷，无障碍
    } else {
      // rack_4box（默认）：4 个孤立 box
      for (auto [cx, cy] : std::initializer_list<std::pair<float, float>>{
               {8.0F, 0.0F}, {5.0F, 3.0F}, {12.0F, -3.0F}, {14.0F, 3.0F}}) {
        auto segs = box(cx, cy, 0.5F, 0.5F);
        obstacles_.insert(obstacles_.end(), segs.begin(), segs.end());
      }
    }
  }

  /// Ray-cast a full scan from the robot pose. Beam i points at lidar-frame
  /// angle (-π + i·(2π/beam_count)): ranges[0] is the rear beam, the heading
  /// beam (0°) sits at i=beam_count/2. This matches angle_min=-π so consumers
  /// (collision_guard forward FOV, cluster_detector) read forward at the
  /// centred index — fixes the 180° misalignment (B3). Lidar sits
  /// lidar_offset_x ahead of origin.
  std::vector<float> generate_scan(float x, float y, float theta) const {
    const float step = 2.0F * static_cast<float>(M_PI) / params_.beam_count;
    const float start = theta - static_cast<float>(M_PI);  // rear beam first
    const float ox = x + params_.lidar_offset_x * std::cos(theta);
    const float oy = y + params_.lidar_offset_x * std::sin(theta);
    std::vector<float> ranges(static_cast<std::size_t>(params_.beam_count),
                              params_.range_max);
    for (int i = 0; i < params_.beam_count; ++i) {
      const float angle = start + static_cast<float>(i) * step;
      ranges[static_cast<std::size_t>(i)] = cast(ox, oy, angle);
    }
    return ranges;
  }

  /// Kinematic odometry integration (unicycle model).
  static Pose step(const Pose &p, float v, float w, float dt) {
    Pose out = p;
    out.theta += w * dt;
    out.x += v * std::cos(out.theta) * dt;
    out.y += v * std::sin(out.theta) * dt;
    return out;
  }

  const SceneParams &params() const { return params_; }

private:
  /// Nearest intersection of the ray (ox,oy,d) with a scene edge, or range_max.
  float cast(float ox, float oy, float angle) const {
    const float dx = std::cos(angle);
    const float dy = std::sin(angle);
    float best = params_.range_max;
    for (const auto &s : walls_) {
      best = std::min(best, ray_segment(ox, oy, dx, dy, s));
    }
    for (const auto &s : obstacles_) {
      best = std::min(best, ray_segment(ox, oy, dx, dy, s));
    }
    return best;
  }

  /// Ray (origin o, direction d) ∩ segment s. Returns distance or +inf.
  static float ray_segment(float ox, float oy, float dx, float dy,
                           const Segment &s) {
    const float rx = s.x2 - s.x1, ry = s.y2 - s.y1;
    const float denom = dx * ry - dy * rx;
    if (std::fabs(denom) < 1e-9F) return std::numeric_limits<float>::infinity();
    const float wx = s.x1 - ox, wy = s.y1 - oy;
    const float t = (wx * ry - wy * rx) / denom;
    const float u = (wx * dy - wy * dx) / denom;
    if (t >= 0.0F && u >= 0.0F && u <= 1.0F) return t;
    return std::numeric_limits<float>::infinity();
  }

  std::vector<Segment> walls_, obstacles_;
  SceneParams params_;
};

}  // namespace amr::domain::simulation
