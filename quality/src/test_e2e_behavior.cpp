/// @file test_e2e_behavior.cpp — 断言式 e2e（审计行动④ / 迭代2 A5）
///
/// 「车真的动了、到点真的停了、遇障真的绕了、断源真的恢复了」——
/// 外部审计：全项目此前没有任何一条断言式 e2e（integration-plan 停留纸面）。
///
/// 手法：SceneSimulatorNode（合成传感 + 运动学闭环，无 Gazebo 依赖——
/// CI 的 ros:jazzy 容器可跑）+ compute 管线（fusion→decision→motor，
/// 与 compute_container 同款 intra-process 组合）。harness 节点以 DDS
/// 外部视角订阅 /odom 与 /cmd_vel——断言的是「验收者看到的行为」。
///
/// 场景对应 integration-plan：IT-04/05/06（动+到点停）、IT-08（避障）、
/// IT-07 族（断源恢复，进程内 pause 替代 kill——respawn 由 B1 supervisor
/// 的集成验证覆盖，此处验证的是感知降级与恢复语义）。
#include "ros2_robot_middleware/infrastructure/decision_node.hpp"
#include "ros2_robot_middleware/infrastructure/fusion_node.hpp"
#include "ros2_robot_middleware/infrastructure/motor_ctrl_node.hpp"
#include "ros2_robot_middleware/infrastructure/scene_simulator_node.hpp"

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <lifecycle_msgs/msg/state.hpp>
#include <nav_msgs/msg/odometry.hpp>

#include <chrono>
#include <cmath>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace {

using SteadyClock = std::chrono::steady_clock;
using CallbackReturn = rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;

/// 全链栈：世界(scene) + 感知→决策→执行，外加一个外部观测桩。
class E2eStack {
public:
  E2eStack(const std::string &scene, float goal_x, float goal_y) {
    rclcpp::NodeOptions scene_opts;
    scene_opts.append_parameter_override("scene_name", scene);
    scene_ = std::make_shared<SceneSimulatorNode>(scene_opts);

    // compute 管线同款参数（对齐 simulation.launch.py 的 compute 块）
    rclcpp::NodeOptions opts;
    opts.use_intra_process_comms(true);
    opts.append_parameter_override("sensors.lidar.type", "sick_tim781")
        .append_parameter_override("sensors.lidar.topic", "/scan")
        .append_parameter_override("goal_x", goal_x)
        .append_parameter_override("goal_y", goal_y)
        .append_parameter_override("vfh_enabled", false)
        .append_parameter_override("guard_stop_dist", 0.40)
        .append_parameter_override("guard_min_valid_echoes", 50);

    fusion_ = std::make_shared<FusionNode>(opts);
    decision_ = std::make_shared<DecisionNode>(opts);
    motor_ = std::make_shared<MotorCtrlNode>(opts);

    exec_.add_node(scene_);
    exec_.add_node(fusion_->get_node_base_interface());
    exec_.add_node(decision_->get_node_base_interface());
    exec_.add_node(motor_->get_node_base_interface());

    // 观测桩：外部验收者视角（非 intra——跨进程语义的 DDS 通道）
    harness_ = std::make_shared<rclcpp::Node>("e2e_harness");
    odom_sub_ = harness_->create_subscription<nav_msgs::msg::Odometry>(
        "/odom", rclcpp::QoS(10), [this](nav_msgs::msg::Odometry::SharedPtr m) {
          std::lock_guard<std::mutex> l(m_);
          poses_.push_back({m->pose.pose.position.x, m->pose.pose.position.y});
        });
    cmd_sub_ = harness_->create_subscription<geometry_msgs::msg::Twist>(
        "/cmd_vel", rclcpp::QoS(50), [this](geometry_msgs::msg::Twist::SharedPtr m) {
          std::lock_guard<std::mutex> l(m_);
          last_cmd_linear_ = m->linear.x;
        });
    exec_.add_node(harness_);

    spin_thread_ = std::thread([this]() { exec_.spin(); });
  }

  /// 依赖序激活（compute_container 同款：上游先就绪）。
  /// 独立于构造：失败时栈正常析构（spin 线程已被 ~E2eStack cancel+join），
  /// 构造里抛异常会留下未 join 的线程 → terminate 核崩（本地实证）。
  /// Jazzy: configure()/activate() 返回结果 State——按终态判定成败
  void activate_pipeline() {
    auto must = [](const rclcpp_lifecycle::State &s, uint8_t expect, const char *what) {
      if (s.id() != expect) throw std::runtime_error(std::string(what) + " 失败");
    };
    using lifecycle_msgs::msg::State;
    must(fusion_->configure(), State::PRIMARY_STATE_INACTIVE, "fusion configure");
    must(decision_->configure(), State::PRIMARY_STATE_INACTIVE, "decision configure");
    must(motor_->configure(), State::PRIMARY_STATE_INACTIVE, "motor configure");
    must(fusion_->activate(), State::PRIMARY_STATE_ACTIVE, "fusion activate");
    must(decision_->activate(), State::PRIMARY_STATE_ACTIVE, "decision activate");
    must(motor_->activate(), State::PRIMARY_STATE_ACTIVE, "motor activate");
  }

  ~E2eStack() {
    exec_.cancel();
    if (spin_thread_.joinable()) spin_thread_.join();
  }

  SceneSimulatorNode &scene() { return *scene_; }
  FusionNode &fusion() { return *fusion_; }  // 注: degradation_level() 为跨线程读（见 B10）

  double odom_x() const {
    std::lock_guard<std::mutex> l(m_);
    return poses_.empty() ? -1e9 : poses_.back().first;
  }
  double odom_y() const {
    std::lock_guard<std::mutex> l(m_);
    return poses_.empty() ? -1e9 : poses_.back().second;
  }
  double last_cmd_linear() const {
    std::lock_guard<std::mutex> l(m_);
    return last_cmd_linear_;
  }
  /// 全轨迹到某点的最小距离（遇障不穿透断言用）
  double min_dist_to(double cx, double cy) const {
    std::lock_guard<std::mutex> l(m_);
    double best = 1e9;
    for (auto &p : poses_) best = std::min(best, std::hypot(p.first - cx, p.second - cy));
    return best;
  }

  bool wait_for(SteadyClock::duration timeout, const std::function<bool()> &pred) {
    const auto deadline = SteadyClock::now() + timeout;
    while (SteadyClock::now() < deadline) {
      if (pred()) return true;
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return pred();
  }

  /// 等停稳：cmd 连续 quiet_ms 静默（最后逼近段 v≤dist 是正常收敛，不是失败）
  bool wait_until_quiet(SteadyClock::duration timeout, std::chrono::milliseconds quiet_ms,
                        double cmd_thresh = 0.03) {
    auto quiet_since = SteadyClock::time_point::max();
    const auto deadline = SteadyClock::now() + timeout;
    while (SteadyClock::now() < deadline) {
      if (std::abs(last_cmd_linear()) < cmd_thresh) {
        if (quiet_since == SteadyClock::time_point::max()) quiet_since = SteadyClock::now();
        if (SteadyClock::now() - quiet_since >= quiet_ms) return true;
      } else {
        quiet_since = SteadyClock::time_point::max();
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return false;
  }

  /// 「车真停」：duration 内位移 < max_travel 且窗口内观测到的 cmd 峰值 < max_cmd
  bool stays_stopped(SteadyClock::duration duration, double max_travel, double max_cmd) {
    const double x0 = odom_x(), y0 = odom_y();
    double cmd_peak = 0.0;
    const auto deadline = SteadyClock::now() + duration;
    while (SteadyClock::now() < deadline) {  // 50ms 采样窗口内的峰值（防单点运气）
      cmd_peak = std::max(cmd_peak, std::abs(last_cmd_linear()));
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    const double travel = std::hypot(odom_x() - x0, odom_y() - y0);
    if (travel > max_travel || cmd_peak > max_cmd) {
      ADD_FAILURE() << "未真停: travel=" << travel << "m cmd_peak=" << cmd_peak
                    << "m/s (终态 odom=(" << odom_x() << "," << odom_y() << "))";
      return false;
    }
    return true;
  }

 private:
  std::shared_ptr<SceneSimulatorNode> scene_;
  std::shared_ptr<FusionNode> fusion_;
  std::shared_ptr<DecisionNode> decision_;
  std::shared_ptr<MotorCtrlNode> motor_;
  rclcpp::Node::SharedPtr harness_;
  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odom_sub_;
  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr cmd_sub_;
  rclcpp::executors::MultiThreadedExecutor exec_;
  std::thread spin_thread_;
  std::vector<std::pair<double, double>> poses_;  // 全轨迹（20Hz × 分钟级，量级无害）
  double last_cmd_linear_ = 0.0;
  mutable std::mutex m_;
};

class E2eBehaviorTest : public ::testing::Test {
protected:
  static void SetUpTestSuite() { rclcpp::init(0, nullptr); }
  static void TearDownTestSuite() { rclcpp::shutdown(); }
};

// ── IT-04/05/06：车真动 + 到点真停 ────────────────────────────────────
TEST_F(E2eBehaviorTest, OpenScene_ReachesGoalAndTrulyStops) {
  E2eStack stack("warehouse_open", 7.0F, 0.0F);
  stack.activate_pipeline();  // 起点 (2,0)，空旷直行 5m

  // 车真动：里程数据流在跑，且位置离开起点
  ASSERT_TRUE(stack.wait_for(std::chrono::seconds(30),
                             [&] { return stack.odom_x() > 2.5; }))
      << "30s 内车未动起来（odom.x=" << stack.odom_x() << "）——全链没有活";

  // G3 验收：odom.x → 7.0 ± 0.3
  ASSERT_TRUE(stack.wait_for(std::chrono::seconds(60),
                             [&] { return std::abs(stack.odom_x() - 7.0) < 0.3; }))
      << "60s 内未到达 (7,0)，odom=(" << stack.odom_x() << "," << stack.odom_y() << ")";

  // 车真停：先等 cmd 静默 1s（最后逼近段 v≤dist 是收敛不是失败），再 3s 硬断言
  ASSERT_TRUE(stack.wait_until_quiet(std::chrono::seconds(30), std::chrono::milliseconds(1000)))
      << "到达后 30s cmd 未静默";
  EXPECT_TRUE(stack.stays_stopped(std::chrono::seconds(3), 0.1, 0.03));
}

// ── IT-08：遇障真绕——全程不进膨胀盘 + 绕行后仍到达 ───────────────────
TEST_F(E2eBehaviorTest, BoxOnPath_DetoursWithoutPenetration) {
  E2eStack stack("rack_4box", 10.0F, 0.0F);
  stack.activate_pipeline();  // box(8,0, 0.5×0.5) 正对直线路径

  ASSERT_TRUE(stack.wait_for(std::chrono::seconds(90),
                             [&] { return std::abs(stack.odom_x() - 10.0) < 0.5; }))
      << "90s 内未绕达 (10,0)，odom=(" << stack.odom_x() << "," << stack.odom_y() << ")";

  ASSERT_TRUE(stack.wait_until_quiet(std::chrono::seconds(30), std::chrono::milliseconds(1000)))
      << "绕达后 30s cmd 未静默";
  EXPECT_TRUE(stack.stays_stopped(std::chrono::seconds(3), 0.1, 0.03));

  // 不穿透：轨迹到 box 中心最小距离 > 物理不碰撞界 0.57
  // （盒半宽 0.25 + 车外接半径 0.32；实测 0.601。⚠️ finding：A* inscribed 0.55
  // 略低于该物理界——planner 理论上可规划进碰撞区 ~2cm，已记 change journal）
  const double min_dist = stack.min_dist_to(8.0, 0.0);
  EXPECT_GT(min_dist, 0.57) << "轨迹进入物理碰撞盘（min_dist=" << min_dist << "m）";
}

// ── IT-07 族：断源降级 + 恢复后仍到达停车 ─────────────────────────────
TEST_F(E2eBehaviorTest, SensorOutage_DegradesThenRecovers) {
  E2eStack stack("warehouse_open", 7.0F, 0.0F);
  stack.activate_pipeline();

  ASSERT_TRUE(stack.wait_for(std::chrono::seconds(30),
                             [&] { return stack.odom_x() > 2.5; }))
      << "车未起步";

  stack.scene().pause();  // 传感+定位+世界一起断流（传感器死亡）
  std::this_thread::sleep_for(std::chrono::seconds(3));
  // 降级可见（current_level_ 跨线程读为良性竞态，见 fusion B10 已知项）
  const bool degraded =
      stack.fusion().degradation_level() != FusionNode::DegradationLevel::FULL;
  stack.scene().resume();

  EXPECT_TRUE(degraded) << "断源 3s 期间 fusion 未降级——感知守门失效";
  ASSERT_TRUE(stack.wait_for(std::chrono::seconds(90),
                             [&] { return std::abs(stack.odom_x() - 7.0) < 0.3; }))
      << "恢复后 90s 未到达目标，odom=(" << stack.odom_x() << "," << stack.odom_y() << ")";
  ASSERT_TRUE(stack.wait_until_quiet(std::chrono::seconds(30), std::chrono::milliseconds(1000)))
      << "恢复到达后 30s cmd 未静默";
  EXPECT_TRUE(stack.stays_stopped(std::chrono::seconds(3), 0.1, 0.03));
}

}  // namespace
