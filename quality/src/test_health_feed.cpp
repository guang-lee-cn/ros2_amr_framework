/// @file test_health_feed.cpp — HealthFeed 聚合语义单测（W5）
///
/// HealthFeed 此前只被 SupervisorNode 的 infra 测试间接覆盖。整栈形态（一个子
/// 进程映射多个 health 节点）引入聚合语义后，判定逻辑本身必须被直接锁定：
///   全部映射节点 OK 才算活；任一 ERROR/FATAL 立即判挂；
///   WARN/STALE 不改变视图（只阻止转 OK）；仅翻转时回调。
///
/// 用例直接建 LifecycleNode + 订阅，不经 supervisor——判定与接线分离定位。
#include "ros2_robot_middleware/infrastructure/health_feed.hpp"
#include "ros2_robot_middleware/infrastructure/qos_profiles.hpp"

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp_lifecycle/lifecycle_node.hpp>

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace std::chrono_literals;

class HealthFeedTest : public ::testing::Test {
protected:
  static void SetUpTestSuite() { rclcpp::init(0, nullptr); }
  static void TearDownTestSuite() { rclcpp::shutdown(); }

  void SetUp() override {
    host_ = std::make_shared<rclcpp_lifecycle::LifecycleNode>("hf_host");
    pub_node_ = std::make_shared<rclcpp::Node>("hf_pub");
    pub_ = pub_node_->create_publisher<ros2_robot_middleware::msg::HealthReport>(
        "/health/report", amr::qos::reliable_stream());
  }

  /// 必须在 HealthFeed 构造**之后**调用：rclcpp 的 Executor 在 add_node 时
  /// 快照节点实体，之后新建的订阅不会被 spin（踩过一次：全用例假绿/假红）。
  void start_spinning() {
    exec_.add_node(host_->get_node_base_interface());
    exec_.add_node(pub_node_);
  }

  void publish(const std::string & node, const std::string & status) {
    ros2_robot_middleware::msg::HealthReport msg;
    msg.header.stamp = pub_node_->now();
    ros2_robot_middleware::msg::HealthStatus s;
    s.node_name = node;
    s.status = status;
    s.last_seen_s = 0.0;
    s.timeout_s = 2.0;
    msg.nodes.push_back(s);
    pub_->publish(msg);
  }

  /// 以 5Hz 重发给定状态，直到 events_ 达到 expect_count 或超时。
  bool pump(const std::vector<std::pair<std::string, std::string>> & statuses,
            size_t expect_count, std::chrono::milliseconds timeout = 3s) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
      for (const auto &[node, status] : statuses) publish(node, status);
      exec_.spin_once(20ms);
      if (events_.size() >= expect_count) return true;
    }
    return events_.size() >= expect_count;
  }

  rclcpp::executors::SingleThreadedExecutor exec_;
  std::shared_ptr<rclcpp_lifecycle::LifecycleNode> host_;
  std::shared_ptr<rclcpp::Node> pub_node_;
  rclcpp::Publisher<ros2_robot_middleware::msg::HealthReport>::SharedPtr pub_;
  std::vector<std::pair<std::string, bool>> events_;
};

TEST_F(HealthFeedTest, Given_TwoNodes_Then_AliveOnlyWhenBothOk) {
  amr::infrastructure::HealthFeed feed(
      *host_, {{"stack", {"hb_a", "hb_b"}}},
      [this](const std::string & child, bool alive) { events_.emplace_back(child, alive); });
  start_spinning();

  // 单节点 OK 不够——整栈形态的半死栈漏洞就在这里
  EXPECT_FALSE(pump({{"hb_a", "OK"}}, 1, 1s))
      << "单节点 OK 就判活（聚合语义失效）";

  ASSERT_TRUE(pump({{"hb_a", "OK"}, {"hb_b", "OK"}}, 1)) << "全节点 OK 后仍未判活";
  EXPECT_EQ(events_[0].first, "stack");
  EXPECT_TRUE(events_[0].second);
}

TEST_F(HealthFeedTest, Given_OneNodeError_Then_LostImmediately) {
  amr::infrastructure::HealthFeed feed(
      *host_, {{"stack", {"hb_a", "hb_b"}}},
      [this](const std::string & child, bool alive) { events_.emplace_back(child, alive); });
  start_spinning();

  ASSERT_TRUE(pump({{"hb_a", "OK"}, {"hb_b", "OK"}}, 1));
  ASSERT_TRUE(pump({{"hb_a", "OK"}, {"hb_b", "ERROR"}}, 2)) << "单节点 ERROR 未判挂";
  EXPECT_FALSE(events_[1].second);
}

TEST_F(HealthFeedTest, Given_WarnStatus_Then_NoFlip) {
  amr::infrastructure::HealthFeed feed(
      *host_, {{"stack", {"hb_a", "hb_b"}}},
      [this](const std::string & child, bool alive) { events_.emplace_back(child, alive); });
  start_spinning();

  ASSERT_TRUE(pump({{"hb_a", "OK"}, {"hb_b", "OK"}}, 1));
  // WARN = 心跳仍在到（只是逼近超时）→ 不得翻转判挂，也不得误报活
  EXPECT_FALSE(pump({{"hb_a", "WARN"}, {"hb_b", "OK"}}, 2, 1s))
      << "WARN 触发了翻转（误杀防线失效）";
  EXPECT_FALSE(pump({{"hb_a", "STALE"}, {"hb_b", "OK"}}, 2, 1s))
      << "STALE 触发了翻转";
}

TEST_F(HealthFeedTest, Given_Recovery_Then_AliveAgain) {
  amr::infrastructure::HealthFeed feed(
      *host_, {{"stack", {"hb_a", "hb_b"}}},
      [this](const std::string & child, bool alive) { events_.emplace_back(child, alive); });
  start_spinning();

  ASSERT_TRUE(pump({{"hb_a", "OK"}, {"hb_b", "OK"}}, 1));
  ASSERT_TRUE(pump({{"hb_a", "ERROR"}, {"hb_b", "OK"}}, 2));
  ASSERT_TRUE(pump({{"hb_a", "OK"}, {"hb_b", "OK"}}, 3)) << "恢复后未重新判活";
  EXPECT_TRUE(events_[2].second);
}

}  // namespace
