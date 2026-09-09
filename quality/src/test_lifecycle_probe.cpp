/// @file test_lifecycle_probe.cpp — F2 lifecycle get_state 探针行为验证
///
/// 断言探针的判定语义（决定 NAV2 形态是否误杀）：
///   ACTIVE        → 上报（heartbeat_received）
///   非 ACTIVE     → 不上报（INACTIVE/UNCONFIGURED 都算「不工作」）
///   服务不可达    → 不上报、不崩（交给既有 timeout 模型升级 ERROR）
/// 形态对齐生产：SingleThreadedExecutor + poll 与 spin 交替（health_monitor 同款）。
///
/// 顺序刻意如此：先证明「ACTIVE 能上报」（同时证明服务发现已就绪），
/// 再证明「非 ACTIVE 不上报」——否则负例可能因发现未完成而假绿。
#include "ros2_robot_middleware/infrastructure/lifecycle_probe.hpp"

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp_lifecycle/lifecycle_node.hpp>

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace std::chrono_literals;

/// 只 spin 不 poll：排空在途响应。异步请求的固有竞态——状态转换前发出的
/// 请求可能带旧 ACTIVE 快照返回，最多让检测晚一个 poll 周期（超时模型不受影响），
/// 断言前必须先排空，否则会把「在途」误判成「不收敛」。
void settle(rclcpp::executors::SingleThreadedExecutor &exec,
            std::chrono::milliseconds duration) {
  const auto deadline = std::chrono::steady_clock::now() + duration;
  while (std::chrono::steady_clock::now() < deadline) {
    exec.spin_some();
    std::this_thread::sleep_for(10ms);
  }
}

/// 交替 poll + spin 直到条件成立或超时；返回条件是否成立。
template <typename Pred>
bool poll_until(const std::shared_ptr<amr::infrastructure::LifecycleProbe> &probe,
                rclcpp::executors::SingleThreadedExecutor &exec,
                Pred done,
                std::chrono::milliseconds timeout = 4000ms) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    probe->poll();
    exec.spin_some();
    if (done()) return true;
    std::this_thread::sleep_for(20ms);
  }
  return done();
}

class LifecycleProbeTest : public ::testing::Test {
protected:
  static void SetUpTestSuite() { rclcpp::init(0, nullptr); }
  static void TearDownTestSuite() { rclcpp::shutdown(); }

  /// 建宿主 + 目标（lifecycle 节点自带 /<name>/get_state），装探针。
  /// @param node_name    实际创建的 lifecycle 节点名
  /// @param probe_target 探针查询的目标名（可与 node_name 不同 → 模拟不可达）
  void make_fixture(const std::string &node_name = "probe_target",
                    const std::string &probe_target = "") {
    const std::string target_name =
        probe_target.empty() ? node_name : probe_target;
    host_ = std::make_shared<rclcpp::Node>("probe_host");
    target_ = std::make_shared<rclcpp_lifecycle::LifecycleNode>(node_name);
    exec_.add_node(host_->get_node_base_interface());
    exec_.add_node(target_->get_node_base_interface());
    probe_ = std::make_shared<amr::infrastructure::LifecycleProbe>(
        host_->get_node_base_interface(), host_->get_node_graph_interface(),
        host_->get_node_services_interface(),
        std::vector<std::string>{target_name},
        [this](const std::string &n) { last_name_ = n; ++alive_count_; });
  }

  rclcpp::executors::SingleThreadedExecutor exec_;
  std::shared_ptr<rclcpp::Node> host_;
  std::shared_ptr<rclcpp_lifecycle::LifecycleNode> target_;
  std::shared_ptr<amr::infrastructure::LifecycleProbe> probe_;
  int alive_count_ = 0;
  std::string last_name_;
};

TEST_F(LifecycleProbeTest, Given_ActiveTarget_Then_AliveReported) {
  make_fixture();
  target_->configure();
  target_->activate();  // ACTIVE = 权威存活信号

  ASSERT_TRUE(poll_until(probe_, exec_, [this] { return alive_count_ > 0; }))
      << "ACTIVE 目标未被上报（探针或服务发现失效）";
  EXPECT_EQ(last_name_, "probe_target");
}

TEST_F(LifecycleProbeTest, Given_DeactivatedTarget_Then_NoReport) {
  make_fixture();
  target_->configure();
  target_->activate();
  ASSERT_TRUE(poll_until(probe_, exec_, [this] { return alive_count_ > 0; }))
      << "前置条件不成立：ACTIVE 时未上报，本用例无法归因";

  target_->deactivate();  // INACTIVE：进程活着但节点不工作
  settle(exec_, 200ms);   // 排空在途响应
  alive_count_ = 0;
  poll_until(probe_, exec_, [] { return false; }, 600ms);
  EXPECT_EQ(alive_count_, 0) << "非 ACTIVE 仍上报 → 挂死节点永不超时";
}

TEST_F(LifecycleProbeTest, Given_ReactivatedTarget_Then_ReportResumes) {
  make_fixture();
  target_->configure();
  target_->activate();
  ASSERT_TRUE(poll_until(probe_, exec_, [this] { return alive_count_ > 0; }));
  target_->deactivate();
  settle(exec_, 200ms);
  alive_count_ = 0;
  poll_until(probe_, exec_, [] { return false; }, 600ms);
  ASSERT_EQ(alive_count_, 0);

  target_->activate();  // 恢复 = 重启后的新周期
  EXPECT_TRUE(poll_until(probe_, exec_, [this] { return alive_count_ > 0; }))
      << "恢复 ACTIVE 后未重新上报（监控无法自愈）";
}

TEST_F(LifecycleProbeTest, Given_UnknownTarget_Then_NoReportNoCrash) {
  make_fixture("probe_target", "ghost_target");  // 探针查的是不存在的节点
  poll_until(probe_, exec_, [] { return false; }, 800ms);
  EXPECT_EQ(alive_count_, 0) << "不可达目标被误判为活 → 误杀风险";
}

}  // namespace
