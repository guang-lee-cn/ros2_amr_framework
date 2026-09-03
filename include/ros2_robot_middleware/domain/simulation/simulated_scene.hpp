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
#include <functional>
#include <limits>
#include <random>
#include <string>
#include <utility>
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
  // 随机静态障碍箱（任何场景可叠加）：种子固定 → 部署可复现
  int random_boxes = 0;
  unsigned random_seed = 42u;
  // 移动障碍（动态绕行测试）：N 个匀速直线运动箱，碰墙反弹
  int movers = 0;
  float mover_speed = 0.6F;  // m/s
};

/// 移动障碍（行人/叉车的抽象）：匀速直线，撞墙反弹。
struct MovingBox {
  float cx, cy;      // 中心（世界系）
  float half;        // 半边长
  float vx, vy;      // 速度
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
      // 空旷，无障碍（随机/移动障碍可叠加）
    } else {
      // rack_4box（默认）：4 个孤立 box
      for (auto [cx, cy] : std::initializer_list<std::pair<float, float>>{
               {8.0F, 0.0F}, {5.0F, 3.0F}, {12.0F, -3.0F}, {14.0F, 3.0F}}) {
        auto segs = box(cx, cy, 0.5F, 0.5F);
        obstacles_.insert(obstacles_.end(), segs.begin(), segs.end());
      }
    }
    spawn_random_boxes(box);
    spawn_movers();
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

  /// Advance moving obstacles one tick; bounce off walls (inner margin).
  void update(float dt) {
    for (auto &m : movers_) {
      m.cx += m.vx * dt;
      m.cy += m.vy * dt;
      const float lim_x = 19.0F - m.half, lim_y = 5.0F - m.half;
      if (m.cx < m.half) { m.cx = 2.0F * m.half - m.cx; m.vx = -m.vx; }
      if (m.cx > lim_x) { m.cx = 2.0F * lim_x - m.cx; m.vx = -m.vx; }
      if (m.cy < -lim_y) { m.cy = 2.0F * -lim_y - m.cy; m.vy = -m.vy; }
      if (m.cy > lim_y) { m.cy = 2.0F * lim_y - m.cy; m.vy = -m.vy; }
    }
  }

  const SceneParams &params() const { return params_; }
  const std::vector<MovingBox> &movers() const { return movers_; }
  /// 随机箱中心列表（marker 渲染用；half 边长 0.35）
  const std::vector<std::pair<float, float>> &random_boxes() const {
    return random_centers_;
  }

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
    // 移动障碍：每束光线对 mover 四边求交（mover 少，开销可忽略）
    for (const auto &m : movers_) {
      const float hx = m.half, hy = m.half;
      const Segment edges[4] = {
          {m.cx - hx, m.cy - hy, m.cx + hx, m.cy - hy},
          {m.cx + hx, m.cy - hy, m.cx + hx, m.cy + hy},
          {m.cx + hx, m.cy + hy, m.cx - hx, m.cy + hy},
          {m.cx - hx, m.cy + hy, m.cx - hx, m.cy - hy}};
      for (const auto &e : edges) {
        best = std::min(best, ray_segment(ox, oy, dx, dy, e));
      }
    }
    return best;
  }

  /// 随机静态箱：拒绝采样 —— 离出生点(2,0)≥2.5m、彼此≥2m、不压预设障碍区
  /// （rack_3c 料架 x∈[4,10] 整带跳过）。种子固定 → 同参数同场景。
  void spawn_random_boxes(
      const std::function<std::vector<Segment>(float, float, float, float)> &box) {
    if (params_.random_boxes <= 0) return;
    std::mt19937 rng(params_.random_seed);
    std::uniform_real_distribution<float> xd(3.0F, 17.0F), yd(-4.0F, 4.0F);
    int placed = 0, guard = 0;
    constexpr float kHalf = 0.35F;
    while (placed < params_.random_boxes && guard++ < 1000) {
      const float x = xd(rng), y = yd(rng);
      if (std::hypot(x - 2.0F, y - 0.0F) < 2.5F) continue;
      if (params_.scene_name == "rack_3c" && x > 3.5F && x < 10.5F) continue;
      bool ok = true;
      for (const auto &[px, py] : random_centers_) {
        if (std::hypot(x - px, y - py) < 2.0F) { ok = false; break; }
      }
      if (!ok) continue;
      random_centers_.emplace_back(x, y);
      auto segs = box(x, y, 2.0F * kHalf, 2.0F * kHalf);
      obstacles_.insert(obstacles_.end(), segs.begin(), segs.end());
      ++placed;
    }
  }

  /// 移动障碍生成：随机位置（同拒绝规则）+ 随机方向匀速。
  void spawn_movers() {
    if (params_.movers <= 0) return;
    std::mt19937 rng(params_.random_seed + 1u);
    std::uniform_real_distribution<float> xd(4.0F, 17.0F), yd(-4.0F, 4.0F),
        heading(-3.14159F, 3.14159F);
    int placed = 0, guard = 0;
    while (placed < params_.movers && guard++ < 1000) {
      const float x = xd(rng), y = yd(rng), a = heading(rng);
      if (std::hypot(x - 2.0F, y - 0.0F) < 3.0F) continue;
      movers_.push_back({x, y, 0.3F, params_.mover_speed * std::cos(a),
                         params_.mover_speed * std::sin(a)});
      ++placed;
    }
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
  std::vector<std::pair<float, float>> random_centers_;
  std::vector<MovingBox> movers_;
  SceneParams params_;
};

}  // namespace amr::domain::simulation
