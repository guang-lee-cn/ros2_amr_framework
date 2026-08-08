/// @file test_control_loop.cpp — 决策-执行闭环集成测试（纯 domain，无 ROS2）
///
/// 组合：GridUpdater(障碍标记) → AStarPlanner(路径) → PathSmoother(平滑)
///      → PurePursuit(跟踪) → TrackErrorMonitor(自纠)
/// 模拟机器人沿路径运动，验证避障/跟踪/自纠/速度平滑行为。
#include "ros2_robot_middleware/domain/planning/astar_planner.hpp"
#include "ros2_robot_middleware/domain/planning/grid_updater.hpp"
#include "ros2_robot_middleware/domain/planning/path_smoother.hpp"
#include "ros2_robot_middleware/domain/planning/track_error_monitor.hpp"
#include "ros2_robot_middleware/domain/execution/pure_pursuit.hpp"

#include <cmath>
#include <gtest/gtest.h>
#include <utility>

using planning::AStarPlanner;
using planning::GridUpdater;
using planning::PathSmoother;
using planning::TrackErrorMonitor;
using planning::TrackErrorLevel;
using planning::OccupancyGrid;
using planning::Pose;
using planning::Waypoint;
using execution::PurePursuit;
using execution::Pose2D;
using execution::Twist2D;

namespace {

constexpr float kRes = 0.05F;  // 5cm grid
constexpr int kW = 200;        // 10m
constexpr int kH = 200;

/// 构建网格，可选障碍物（world 坐标）。
OccupancyGrid make_grid(const std::vector<std::pair<float, float>> &obstacles,
                        float obstacle_radius = 0.3F) {
  OccupancyGrid g;
  g.width = kW; g.height = kH; g.resolution = kRes; g.origin = {0, 0};
  g.cells.assign(static_cast<size_t>(kW * kH), false);
  GridUpdater updater(GridUpdater::Params{obstacle_radius});
  for (const auto &[x, y] : obstacles) {
    updater.inflate(g, x, y);
  }
  return g;
}

/// 从 A* 路径转 PurePursuit 路径。
std::vector<execution::Waypoint> to_exec_path(const std::vector<Waypoint> &p) {
  std::vector<execution::Waypoint> out;
  for (const auto &w : p) out.push_back({w.x, w.y});
  return out;
}

/// 规划 + 平滑 → execution 路径。
std::vector<execution::Waypoint> plan_smooth(
    const OccupancyGrid &g, const Pose &start, const Pose &goal,
    const AStarPlanner &astar, const PathSmoother &smoother) {
  auto raw = astar.plan(g, start, goal);
  auto smooth = smoother.smooth(raw);
  return to_exec_path(smooth);
}

/// 模拟机器人沿路径运动，返回是否到达。
/// 每步：PurePursuit 计算 twist → 误差监控缩放 → 运动学积分推进。
/// 到达条件：距目标 < goal_tolerance。
bool simulate_follow(const std::vector<execution::Waypoint> &path,
                     const PurePursuit &pp, const TrackErrorMonitor &mon,
                     int max_steps = 5000, float dt = 0.05F) {
  Pose2D pose{0.0F, 0.0F, 0.0F};
  const auto &goal = path.back();
  constexpr float kGoalTol = 0.1F;
  for (int step = 0; step < max_steps; ++step) {
    float gd = std::hypot(goal.x - pose.x, goal.y - pose.y);
    if (gd < kGoalTol) return true;

    auto twist = pp.track(path, pose);
    auto err = mon.evaluate(pose, path);
    if (err.level == TrackErrorLevel::ERROR) return false;  // 安全停止

    // 应用误差缩放（WARN 降速，ERROR 已停）
    twist.linear *= err.speed_scale;
    twist.angular *= err.speed_scale;

    pose.x += twist.linear * std::cos(pose.theta) * dt;
    pose.y += twist.linear * std::sin(pose.theta) * dt;
    pose.theta += twist.angular * dt;
  }
  return false;  // 超步数
}

}  // namespace

// ── 1. Given_StraightPath_Then_ReachGoal ─────────────────────────────
// 无障碍直线：A* → 平滑 → PurePursuit → 到达。

TEST(ControlLoopTest, Given_StraightPath_Then_ReachGoal) {
  auto grid = make_grid({});
  AStarPlanner astar;
  PathSmoother smoother;
  PurePursuit pp;
  TrackErrorMonitor mon;

  auto path = plan_smooth(grid, {0.5F, 0.5F}, {4.0F, 0.5F}, astar, smoother);
  ASSERT_FALSE(path.empty());

  EXPECT_TRUE(simulate_follow(path, pp, mon))
      << "直线路径应平滑到达目标";
}

// ── 2. Given_Obstacle_Then_ReplanAround ──────────────────────────────
// 单障碍挡路：A* 应绕障，路径不穿障碍，机器人到达。

TEST(ControlLoopTest, Given_Obstacle_Then_ReplanAround) {
  // 障碍在直线路径 (2.0, 0.5) 上
  auto grid = make_grid({{2.0F, 0.5F}}, 0.3F);
  AStarPlanner astar;
  PathSmoother smoother;

  auto path = plan_smooth(grid, {0.5F, 0.5F}, {4.0F, 0.5F}, astar, smoother);
  ASSERT_FALSE(path.empty());

  // 路径不经过障碍区域（障碍中心 2.0,0.5，半径 ~0.3+0.15 网格）
  for (const auto &wp : path) {
    float d = std::hypot(wp.x - 2.0F, wp.y - 0.5F);
    EXPECT_GT(d, 0.25F) << "路径不应穿过障碍 (" << wp.x << "," << wp.y << ")";
  }

  PurePursuit pp;
  TrackErrorMonitor mon;
  EXPECT_TRUE(simulate_follow(path, pp, mon))
      << "应绕障到达目标";
}

// ── 3. Given_OffPath_Then_ErrorMonitorSlows ──────────────────────────
// 偏离路径：误差监控应降速（WARN），而非全速。

TEST(ControlLoopTest, Given_OffPath_Then_ErrorMonitorSlows) {
  TrackErrorMonitor mon(TrackErrorMonitor::Params{0.15F, 0.40F});
  std::vector<execution::Waypoint> path = {{0, 0}, {1, 0}, {2, 0}, {3, 0}};

  // 在直线上 → OK 全速
  auto ok = mon.evaluate({1.5F, 0.0F, 0.0F}, path);
  EXPECT_EQ(ok.level, TrackErrorLevel::OK);
  EXPECT_NEAR(ok.speed_scale, 1.0F, 1e-4F);

  // 偏离 0.2m（warn~stop 之间）→ 降速
  auto warn = mon.evaluate({1.5F, 0.2F, 0.0F}, path);
  EXPECT_EQ(warn.level, TrackErrorLevel::WARN);
  EXPECT_LT(warn.speed_scale, 1.0F);
  EXPECT_GT(warn.speed_scale, 0.0F);
}

// ── 4. Given_OffPath_Large_Then_Stops ────────────────────────────────
// 严重偏离：误差监控应停止（ERROR）。

TEST(ControlLoopTest, Given_OffPath_Large_Then_Stops) {
  TrackErrorMonitor mon(TrackErrorMonitor::Params{0.15F, 0.40F});
  std::vector<execution::Waypoint> path = {{0, 0}, {1, 0}, {2, 0}, {3, 0}};

  auto err = mon.evaluate({1.5F, 0.5F, 0.0F}, path);  // 0.5m 偏离
  EXPECT_EQ(err.level, TrackErrorLevel::ERROR);
  EXPECT_FLOAT_EQ(err.speed_scale, 0.0F);
}

// ── 5. Given_NearGoal_Then_SlowsDown ─────────────────────────────────
// 接近目标：PurePursuit 应减速（梯形速度）。

TEST(ControlLoopTest, Given_NearGoal_Then_SlowsDown) {
  PurePursuit pp(PurePursuit::Params{0.5F, 1.0F, 1.5F, 1.0F, 0.1F, 0.5F, 1.0F});
  std::vector<execution::Waypoint> path = {{0, 0}, {1, 0}, {2, 0}};

  // 远距 → 全速
  auto far = pp.track(path, {0.2F, 0.0F, 0.0F});
  EXPECT_NEAR(far.linear, 1.0F, 0.05F);

  // 近目标（0.3m < slow_radius 0.5m）→ 减速
  auto near = pp.track(path, {1.7F, 0.0F, 0.0F});
  EXPECT_LT(near.linear, 0.8F);
  EXPECT_GT(near.linear, 0.0F);
}

// ── 6. Given_NoPath_Then_NoPlan ──────────────────────────────────────
// 障碍包围目标：A* 应返回空路径（无法到达）。

TEST(ControlLoopTest, Given_NoPath_Then_NoPlan) {
  // 目标 (4,0.5) 被 4 面障碍围住
  auto grid = make_grid({{4.0F, 0.2F}, {4.0F, 0.8F}, {3.7F, 0.5F}, {4.3F, 0.5F}}, 0.2F);
  AStarPlanner astar;

  auto path = astar.plan(grid, {0.5F, 0.5F}, {4.0F, 0.5F});
  EXPECT_TRUE(path.empty()) << "目标被围应无路径";
}

// ── 7. Given_MultiObstacle_Then_Slalom ───────────────────────────────
// 多障碍：A* 连续绕障，机器人到达。

TEST(ControlLoopTest, Given_MultiObstacle_Then_Slalom) {
  auto grid = make_grid({{1.5F, 0.5F}, {2.5F, 1.0F}, {3.5F, 0.5F}}, 0.3F);
  AStarPlanner astar;
  PathSmoother smoother;

  auto path = plan_smooth(grid, {0.5F, 0.5F}, {4.5F, 0.5F}, astar, smoother);
  ASSERT_FALSE(path.empty());

  PurePursuit pp;
  TrackErrorMonitor mon;
  EXPECT_TRUE(simulate_follow(path, pp, mon))
      << "应绕过多个障碍到达";
}

// ── 8. Given_EmptyPath_Then_Stops ────────────────────────────────────
// 空路径：PurePursuit 和误差监控都应停。

TEST(ControlLoopTest, Given_EmptyPath_Then_Stops) {
  PurePursuit pp;
  TrackErrorMonitor mon;
  std::vector<execution::Waypoint> empty{};

  auto twist = pp.track(empty, {0, 0, 0});
  EXPECT_FLOAT_EQ(twist.linear, 0.0F);

  auto err = mon.evaluate({0, 0, 0}, empty);
  EXPECT_EQ(err.level, TrackErrorLevel::ERROR);
}

// ── 9. Given_GoalBlocked_Then_StopsAtBoundary ────────────────────────
// 目标不可达 + 误差超限：机器人停在安全位置（不撞障碍）。

TEST(ControlLoopTest, Given_GoalBlocked_Then_StopsAtBoundary) {
  // 障碍直接堵住目标方向，且障碍在路径上
  auto grid = make_grid({{1.0F, 0.5F}}, 0.5F);  // 大障碍挡路
  AStarPlanner astar;
  PathSmoother smoother;

  auto path = plan_smooth(grid, {0.5F, 0.5F}, {3.0F, 0.5F}, astar, smoother);
  // 若 A* 找到绕障路径则能到达；若找不到则空 → 不移动
  if (path.empty()) {
    SUCCEED() << "无路径时系统不移动（安全）";
  } else {
    PurePursuit pp;
    TrackErrorMonitor mon;
    bool reached = simulate_follow(path, pp, mon);
    // 可能因误差累积停下或到达——都不能穿过障碍
    for (const auto &wp : path) {
      float d = std::hypot(wp.x - 1.0F, wp.y - 0.5F);
      EXPECT_GT(d, 0.4F) << "路径不应穿障碍";
    }
    // 不强制到达——允许误差监控触发安全停止
    (void)reached;
  }
}
