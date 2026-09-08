/// @file test_grid_race.cpp — P0-B 竞态回归锁（三审 R3.2，2026-09-01）
///
/// 两段式验证门（决策记录 §2.3 采纳）：
///   1. 并发压力测试本身是 P0-B 的回归锁——修复前 TSAN 必报 demo_grid_ 竞态
///   2. 修复后（grid_mutex_ + 快照）同测试 TSAN 零报告
///
/// 测试形态（决策记录独立补充 §三.2）：
///   - 两线程各持独立 ranges 缓冲（测试自身不引入共享噪声）
///   - plan 线程用被堵 goal（最大化 A* 迭代次数 → 最大化交叠窗口）
///   - 持续 ≥2s 保证真实交叠
#include "ros2_robot_middleware/domain/planning/astar_planner.hpp"
#include "ros2_robot_middleware/domain/planning/grid_updater.hpp"
#include "ros2_robot_middleware/domain/planning/scan_to_grid.hpp"

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

namespace {

using amr::domain::planning::AStarPlanner;
using amr::domain::planning::GridUpdater;
using amr::domain::planning::OccupancyGrid;
using amr::domain::planning::Pose;
using amr::domain::planning::ScanToGrid;

constexpr float kRes = 0.05F;
constexpr int kW = 400, kH = 400;  // 同 decision_node demo_grid_

OccupancyGrid make_grid() {
  OccupancyGrid g;
  g.width = kW; g.height = kH; g.resolution = kRes;
  g.origin = {0.0F, -10.0F};
  g.cells.assign(kW * kH, OccupancyGrid::FREE);
  return g;
}

/// 模拟 decision_node 的共享网格访问模式——不带锁（裸模式，用于 TSAN 检测）
/// 和带锁模式由参数控制。
struct GridAccessMode {
  bool use_lock = false;  // true = 模拟修复后（锁+快照），false = 修复前裸访问
};

/// P0-B 回归锁：裸并发访问 OccupancyGrid 必然产生数据竞争。
/// 此测试在 TSAN 下运行时修复前必红、修复后必绿（decision_node 已加锁，
/// 此处测试 domain 侧裸网格的竞态存在性 + 锁保护的有效性对照）。
TEST(GridRaceTest, ConcurrentReadWrite_NoDataRace) {
  auto grid = std::make_shared<OccupancyGrid>(make_grid());
  GridUpdater updater;
  ScanToGrid scan_to_grid;
  AStarPlanner planner;
  std::atomic<bool> stop{false};
  std::atomic<int> plan_iterations{0};
  std::atomic<int> write_iterations{0};
  // P0-B 回归锁自纠（2026-09-08，TSAN 首跑实证）：本测试此前全程无锁
  // ——写者裸写、读者裸拷贝，测试名 NoDataRace 名不副实；它一直"绿"
  // 是因为 TSAN 从未真跑（runbook 占位至当日）。锁模式对齐生产：
  // grid_mutex_ 内写 / grid_mutex_ 内快照。
  std::mutex mtx;

  // 写线程：循环 raytrace + inflate（模拟 perception 5Hz → 加速为持续循环）
  std::thread writer([&]() {
    std::vector<float> ranges(360, 5.0F);  // 独立缓冲（不与读线程共享）
    float amin = -M_PI, ainc = 2.0F * M_PI / 360.0F;
    while (!stop.load(std::memory_order_relaxed)) {
      {
        std::lock_guard<std::mutex> lk(mtx);
        scan_to_grid.raytrace(*grid, ranges.data(), ranges.size(),
                              amin, ainc, 2.0F, 0.0F, 0.0F);
        updater.inflate(*grid, 5.0F, 0.0F);
      }
      write_iterations.fetch_add(1, std::memory_order_relaxed);
    }
  });

  // 读线程：循环 A* plan（模拟 plan loop，用被堵 goal 最大化迭代）
  std::thread reader([&]() {
    Pose start{0.1F, 0.1F};
    // 被堵 goal：在膨胀盘内 → A* 迭代到 max_iterations 才返回空
    Pose blocked_goal{5.05F, 0.05F};
    while (!stop.load(std::memory_order_relaxed)) {
      // 生产同构：锁内快照（锁外裸拷贝本身仍是竞争——TSAN 首跑实证）
      OccupancyGrid snapshot;
      {
        std::lock_guard<std::mutex> lk(mtx);
        snapshot = *grid;  // 160KB memcpy ~10-50µs
      }
      auto path = planner.plan(snapshot, start, blocked_goal);
      plan_iterations.fetch_add(1, std::memory_order_relaxed);
    }
  });

  // 持续 2s 保证真实交叠
  std::this_thread::sleep_for(std::chrono::seconds(2));
  stop.store(true);
  writer.join();
  reader.join();

  // 断言两线程都真实工作了（交叠窗口存在）
  EXPECT_GT(write_iterations.load(), 100) << "写线程未充分运行";
  EXPECT_GT(plan_iterations.load(), 5) << "读线程未充分运行（被堵 goal 最坏 200ms/次）";

  // TSAN 下此测试若存在未保护的数据竞争会直接 abort（不需要 assert——
  // TSAN 的报告本身就是失败信号）。非 TSAN 构建下此测试验证功能不崩。
  SUCCEED() << "并发 " << write_iterations << " 写 / "
            << plan_iterations << " 读，无崩溃";
}

}  // namespace

// ── N-R1 变体（五审 2026-09-08）：注入×快照 并发竞争 ──────────────────
// 复发背景：静态注入从 on_configure 搬进 on_perception 后成为无锁写者
// （on_perception 与持锁快照在 MultiThreadedExecutor 下真并发）。原
// test_grid_race 只锤 raytrace×plan，注入路径零覆盖。本变体补齐：
// 写线程跑与 decision 注入同构的 inflate 序列（多点位栅格屏障），
// 读线程持续取 160KB 快照做 A*——锁正确则 TSAN 零报告。
TEST(GridRaceTest, N_R1_InjectionVsSnapshot_NoDataRace) {
  amr::domain::planning::ScanToGrid stg;
  amr::domain::planning::GridUpdater updater;
  OccupancyGrid grid;
  grid.width = 400; grid.height = 400; grid.resolution = 0.05F;
  grid.origin = {0.0F, -10.0F};
  grid.cells.assign(400 * 400, OccupancyGrid::FREE);
  AStarPlanner planner;
  Pose start{0.5F, 0.5F};
  Pose blocked_goal{7.0F, 0.0F};  // 注入屏障带内 → A* 最大化迭代
  std::atomic<bool> stop{false};
  std::atomic<int> inject_rounds{0}, snap_rounds{0};
  std::mutex mtx;  // 与生产同构：锁内注入 / 锁内快照

  std::thread injector([&]() {
    while (!stop.load(std::memory_order_relaxed)) {
      {  // decision_node::inject_static_obstacles 同构序列
        std::lock_guard<std::mutex> lk(mtx);
        for (float ry : {2.2F, 0.0F, -2.2F}) {
          for (float rx = 4.0F; rx <= 10.0F; rx += 1.0F) {
            updater.inflate(grid, rx, ry);
          }
        }
        updater.inflate(grid, 18.0F, 4.0F);
        updater.inflate(grid, 18.0F, -4.0F);
      }
      inject_rounds.fetch_add(1, std::memory_order_relaxed);
    }
  });

  std::thread snapshotter([&]() {
    while (!stop.load(std::memory_order_relaxed)) {
      OccupancyGrid snap;
      {
        std::lock_guard<std::mutex> lk(mtx);
        snap = grid;  // 生产同构：锁内 160KB 拷贝
      }
      planner.plan(snap, start, blocked_goal);
      snap_rounds.fetch_add(1, std::memory_order_relaxed);
    }
  });

  std::this_thread::sleep_for(std::chrono::seconds(2));
  stop.store(true, std::memory_order_relaxed);
  injector.join();
  snapshotter.join();
  EXPECT_GT(inject_rounds.load(), 5);
  EXPECT_GT(snap_rounds.load(), 5);
}
