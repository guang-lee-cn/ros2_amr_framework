/// @file test_health_restart.cpp — P0-C 重启序列异步化后的行为验证（三审 Wave 1.2）
///
/// 断言「重启真的工作」：停掉目标节点心跳（模拟功能性死亡，lifecycle 仍响应）
/// → health_monitor 检出 ERROR → 四步 transition（deactivate→cleanup→
/// configure→activate）自动完成 → 心跳恢复 → 节点被救活。
/// 旧实现（回调内同步 future.wait_for + 单线程 spin）此场景结构性必超时——
/// 本测试即其验收对照。注意用 SingleThreadedExecutor：与生产
/// health_monitor_main 同形态，证明修复不依赖多线程。
#include "ros2_robot_middleware/infrastructure/amr_node.hpp"
#include "ros2_robot_middleware/infrastructure/health_monitor_node.hpp"

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/string.hpp>

#include <atomic>
#include <chrono>
#include <memory>

namespace {

using SteadyClock = std::chrono::steady_clock;

/// 假 lidar：名字/心跳话题与 HealthMonitorNode 的 kNdes[0] 对齐；
/// activate 计数是「被重启救活」的直接证据。
class FakeLidar : public amr::infrastructure::AmrNode {
public:
  FakeLidar() : AmrNode("lidar") {}

  CallbackReturn on_configure(const rclcpp_lifecycle::State &) override {
    return CallbackReturn::SUCCESS;
  }
  CallbackReturn on_activate(const rclcpp_lifecycle::State &) override {
    ++activate_count_;
    start_heartbeat("/sensor/lidar/heartbeat", std::chrono::milliseconds(200));
    return CallbackReturn::SUCCESS;
  }
  CallbackReturn on_deactivate(const rclcpp_lifecycle::State &) override {
    stop_heartbeat();
    return CallbackReturn::SUCCESS;
  }
  CallbackReturn on_cleanup(const rclcpp_lifecycle::State &) override {
    return CallbackReturn::SUCCESS;
  }

  /// 模拟「功能性死亡」：心跳停发但 lifecycle 服务正常（进程活着）
  void kill_heartbeat() { stop_heartbeat(); }

  std::atomic<int> activate_count_{0};
};

class HealthRestartTest : public ::testing::Test {
protected:
  static void SetUpTestSuite() { rclcpp::init(0, nullptr); }
  static void TearDownTestSuite() { rclcpp::shutdown(); }
};

TEST_F(HealthRestartTest, DeadHeartbeat_TriggeredLifecycleRestart_RevivesNode) {
  auto target = std::make_shared<FakeLidar>();
  // 老节点无 NodeOptions 构造——默认时序（check 1Hz / lidar timeout 1.5s），
  // 20s 验证窗口足够覆盖 检出(≤3s)+四步transition(~1s)+心跳恢复(1s)
  auto monitor = std::make_shared<HealthMonitorNode>();

  // 观测桩：心跳恢复的证据
  bool hb_after_kill = false;
  int hb_total = 0;  // 诊断：发布侧 vs 监控侧的区分证据
  auto harness = std::make_shared<rclcpp::Node>("restart_harness");
  auto killed_at = SteadyClock::time_point::max();
  auto hb_sub = harness->create_subscription<std_msgs::msg::String>(
      "/sensor/lidar/heartbeat", rclcpp::QoS(10),
      [&](std_msgs::msg::String::SharedPtr) {
        ++hb_total;
        if (killed_at != SteadyClock::time_point::max() &&
            SteadyClock::now() > killed_at + std::chrono::seconds(1)) {
          hb_after_kill = true;  // kill 之后 1s 才到的心跳 = 重启后的新心跳
        }
      });

  target->configure();
  target->activate();
  monitor->configure();
  monitor->activate();

  rclcpp::executors::SingleThreadedExecutor exec;  // 生产同形态（不靠多线程救场）
  exec.add_node(target->get_node_base_interface());
  exec.add_node(monitor->get_node_base_interface());
  exec.add_node(harness);

  auto spin_until = [&](const std::function<bool()> &pred, int timeout_s) {
    auto end = SteadyClock::now() + std::chrono::seconds(timeout_s);
    while (SteadyClock::now() < end) {
      exec.spin_once(std::chrono::milliseconds(20));
      if (pred()) return true;
    }
    return pred();
  };

  // 阶段 1：边转边等 3.5s——心跳定时器只在执行器 spin 时触发（本测试首版
  // 用裸 sleep：睡眠期零心跳发布 → STALE(never seen) 永不升级 ERROR →
  // 重启不触发，白排障三轮的教训）。3.5s = DDS 发现 + ≥2 个监控检查周期。
  spin_until([&] { return false; }, 3);

  // 阶段 2：功能性死亡（心跳停，lifecycle 活）
  const int activations_before = target->activate_count_;
  target->kill_heartbeat();
  killed_at = SteadyClock::now();

  // 阶段 3：监控检出 ERROR → 异步四步重启 → 救活（activate 计数 +1、心跳恢复）
  const bool revived = spin_until(
      [&] {
        return target->activate_count_ > activations_before && hb_after_kill;
      },
      20);

  EXPECT_TRUE(revived) << "20s 内未观察到重启救活: activations="
                       << target->activate_count_ << " hb_after_kill="
                       << hb_after_kill << " hb_total=" << hb_total;
  EXPECT_GE(target->activate_count_, activations_before + 1)
      << "重启序列未把节点带回 ACTIVE";
}

}  // namespace


// ── 隔离自检 2：lifecycle 节点订阅 + reliable QoS（与监控完全同构）────────
class FakeSubNode : public amr::infrastructure::AmrNode {
public:
  FakeSubNode() : AmrNode("fake_sub") {}
  CallbackReturn on_configure(const rclcpp_lifecycle::State &) override {
    sub_ = create_subscription<std_msgs::msg::String>(
        "/sensor/lidar/heartbeat", rclcpp::QoS(10).reliable(),
        [this](std_msgs::msg::String::SharedPtr) { ++count_; });
    return CallbackReturn::SUCCESS;
  }
  CallbackReturn on_activate(const rclcpp_lifecycle::State &) override { return CallbackReturn::SUCCESS; }
  int count_ = 0;
private:
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr sub_;
};

TEST_F(HealthRestartTest, HeartbeatDelivery_LifecycleReliableSub) {
  auto target = std::make_shared<FakeLidar>();
  auto sub_node = std::make_shared<FakeSubNode>();
  target->configure(); target->activate();
  sub_node->configure(); sub_node->activate();
  rclcpp::executors::SingleThreadedExecutor exec;
  exec.add_node(target->get_node_base_interface());
  exec.add_node(sub_node->get_node_base_interface());
  auto end = SteadyClock::now() + std::chrono::seconds(3);
  while (SteadyClock::now() < end) exec.spin_once(std::chrono::milliseconds(20));
  EXPECT_GT(sub_node->count_, 0) << "lifecycle+reliable 订阅形态不通（监控同构）";
}


