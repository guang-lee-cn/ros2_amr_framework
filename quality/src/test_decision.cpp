#include <limits>
/// @file test_decision.cpp — DecisionNode perception→goal dispatch tests
#include "ros2_robot_middleware/infrastructure/decision_node.hpp"

#include "ros2_robot_middleware/action/move_to_pose.hpp"
#include "ros2_robot_middleware/msg/perception_objects.hpp"

#include "geometry_msgs/msg/transform_stamped.hpp"
#include "tf2_ros/static_transform_broadcaster.h"

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp_action/rclcpp_action.hpp>

#include <chrono>
#include <memory>

class DecisionTest : public ::testing::Test {
protected:
  static void SetUpTestSuite() { rclcpp::init(0, nullptr); }
  static void TearDownTestSuite() { rclcpp::shutdown(); }
};

template <typename Predicate>
bool spin_until(rclcpp::node_interfaces::NodeBaseInterface::SharedPtr node_iface,
                Predicate pred, std::chrono::milliseconds timeout) {
  auto start = std::chrono::steady_clock::now();
  rclcpp::executors::SingleThreadedExecutor exec;
  exec.add_node(node_iface);
  while (!pred() && (std::chrono::steady_clock::now() - start) < timeout) {
    exec.spin_once(std::chrono::milliseconds(10));
  }
  exec.remove_node(node_iface);
  return pred();
}

// decision dispatches goals in the motor's odom frame (map→odom TF).
// LifecycleNode does NOT derive from rclcpp::Node (jazzy composes the node
// interfaces) — publish the TF from a plain rclcpp::Node instead.
// Identity TF (0,0) keeps the dispatched goal equal to the map-frame goal;
// a real offset (8,9) reproduces the map-vs-odom dedup bug scenario.
void publish_map_odom_tf(double tx, double ty) {
  auto tf_node = std::make_shared<rclcpp::Node>("decision_tf_pub");
  auto tf_broadcaster =
      std::make_shared<tf2_ros::StaticTransformBroadcaster>(tf_node);
  geometry_msgs::msg::TransformStamped tf;
  tf.header.stamp = tf_node->now();
  tf.header.frame_id = "map";
  tf.child_frame_id = "amr/odom";
  tf.transform.translation.x = tx;
  tf.transform.translation.y = ty;
  tf.transform.translation.z = 0.0;
  tf.transform.rotation.w = 1.0;
  tf_broadcaster->sendTransform(tf);
}

// Fusion-ready signal: dispatch is gated on this heartbeat (cold-start fix).
// Node must be configured — create_publisher returns a lifecycle publisher.
void publish_fusion_heartbeat(rclcpp_lifecycle::LifecycleNode *node,
                              const char *state) {
  auto hb_pub = node->create_publisher<std_msgs::msg::String>(
      "/sensor/fusion/heartbeat", rclcpp::QoS(10).reliable());
  hb_pub->on_activate();
  auto msg = std_msgs::msg::String{};
  msg.data = state;
  hb_pub->publish(msg);
}

// Task-derived goal: perception objects are obstacles, NOT navigation targets.
// Regression: TargetSelector used objects[0] as the goal — in a static scene
// (walls/racks) the robot chased the nearest obstacle forever.
TEST_F(DecisionTest, TaskGoal_IgnoresPerceptionTarget) {
  auto decision = std::make_shared<DecisionNode>();
  decision->configure();
  decision->set_parameter(rclcpp::Parameter("goal_x", 3.0));
  decision->set_parameter(rclcpp::Parameter("goal_y", 0.0));
  decision->activate();
  publish_map_odom_tf(0.0, 0.0);
  publish_fusion_heartbeat(decision.get(), "alive");

  float received_x = -1.0F, received_y = -1.0F;
  bool goal_received = false;

  auto mock_server = rclcpp_action::create_server<ros2_robot_middleware::action::MoveToPose>(
      decision, "/cmd/move_to_pose",
      [](const rclcpp_action::GoalUUID &,
         std::shared_ptr<const ros2_robot_middleware::action::MoveToPose::Goal>) {
        return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
      },
      [](const std::shared_ptr<rclcpp_action::ServerGoalHandle<ros2_robot_middleware::action::MoveToPose>>) {
        return rclcpp_action::CancelResponse::ACCEPT;
      },
      [&](const std::shared_ptr<rclcpp_action::ServerGoalHandle<ros2_robot_middleware::action::MoveToPose>> gh) {
        received_x = gh->get_goal()->target_x;
        received_y = gh->get_goal()->target_y;
        goal_received = true;
        auto result = std::make_shared<ros2_robot_middleware::action::MoveToPose::Result>();
        result->reached = true;
        result->final_x = received_x;
        result->final_y = received_y;
        gh->succeed(result);
      });

  // Perception detects an object at (2.0, 1.5) — this must NOT become the goal.
  auto perception = ros2_robot_middleware::msg::PerceptionObjects{};
  perception.header.frame_id = "base_link";
  auto obj = ros2_robot_middleware::msg::Object{};
  obj.id = "obj_0";
  obj.x = 2.0F;
  obj.y = 1.5F;
  obj.z = 0.0F;
  perception.objects.push_back(obj);

  auto pub = decision->create_publisher<ros2_robot_middleware::msg::PerceptionObjects>(
      "/perception/objects", rclcpp::QoS(10).reliable());
  pub->on_activate();

  // Let the static map→odom TF reach decision's TF listener before perception
  // triggers a dispatch — dispatch defers while the transform is unavailable.
  spin_until(decision->get_node_base_interface(),
             [] { return false; }, std::chrono::milliseconds(300));
  pub->publish(perception);

  ASSERT_TRUE(spin_until(decision->get_node_base_interface(),
                         [&goal_received] { return goal_received; },
                         std::chrono::seconds(3)));

  EXPECT_TRUE(goal_received);
  EXPECT_FLOAT_EQ(received_x, 3.0F);  // task goal, NOT the detected object
  EXPECT_FLOAT_EQ(received_y, 0.0F);  // NOT 1.5
}

// A* must find a path to the task goal; when the goal is buried beyond the
// endpoint-snap radius (0.75m), no goal may be dispatched (previously
// send_goal ran unconditionally). Single obstacle AT the goal now snaps and
// dispatches to the nearest traversable approach — covered by the next test.
TEST_F(DecisionTest, BlockedGoal_DoesNotSendGoal) {
  auto decision = std::make_shared<DecisionNode>();
  decision->configure();
  decision->set_parameter(rclcpp::Parameter("goal_x", 3.0));
  decision->set_parameter(rclcpp::Parameter("goal_y", 0.0));
  decision->activate();
  publish_map_odom_tf(0.0, 0.0);
  publish_fusion_heartbeat(decision.get(), "alive");

  bool goal_received = false;
  auto mock_server = rclcpp_action::create_server<ros2_robot_middleware::action::MoveToPose>(
      decision, "/cmd/move_to_pose",
      [](const rclcpp_action::GoalUUID &,
         std::shared_ptr<const ros2_robot_middleware::action::MoveToPose::Goal>) {
        return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
      },
      [](const std::shared_ptr<rclcpp_action::ServerGoalHandle<ros2_robot_middleware::action::MoveToPose>>) {
        return rclcpp_action::CancelResponse::ACCEPT;
      },
      [&](const std::shared_ptr<rclcpp_action::ServerGoalHandle<ros2_robot_middleware::action::MoveToPose>> gh) {
        goal_received = true;
        auto result = std::make_shared<ros2_robot_middleware::action::MoveToPose::Result>();
        result->reached = true;
        result->final_x = 3.0F;
        result->final_y = 0.0F;
        gh->succeed(result);
      });

  // 3×3 obstacle cluster burying the task goal: nearest traversable cell
  // ≈0.85m away > snap radius 0.75 → A* must return empty (true-blocked).
  auto perception = ros2_robot_middleware::msg::PerceptionObjects{};
  perception.header.frame_id = "base_link";
  for (float ox : {2.7F, 3.0F, 3.3F}) {
    for (float oy : {-0.3F, 0.0F, 0.3F}) {
      auto obj = ros2_robot_middleware::msg::Object{};
      obj.id = "blocker";
      obj.x = ox;
      obj.y = oy;
      obj.z = 0.0F;
      perception.objects.push_back(obj);
    }
  }

  auto pub = decision->create_publisher<ros2_robot_middleware::msg::PerceptionObjects>(
      "/perception/objects", rclcpp::QoS(10).reliable());
  pub->on_activate();

  // Let the static TF reach decision's listener first (see above).
  spin_until(decision->get_node_base_interface(),
             [] { return false; }, std::chrono::milliseconds(300));
  pub->publish(perception);

  // Spin a fixed window; the goal must never arrive.
  spin_until(decision->get_node_base_interface(),
             [&goal_received] { return goal_received; },
             std::chrono::milliseconds(1500));
  EXPECT_FALSE(goal_received);
}

// Docking semantics (20260817 review): a single obstacle sitting ON the goal
// is within the snap radius — A* snaps to the nearest traversable approach
// and decision dispatches (robot parks adjacent instead of bricking).
TEST_F(DecisionTest, BlockedGoalWithinSnap_When_Perception_SnapsAndDispatches) {
  auto decision = std::make_shared<DecisionNode>();
  decision->configure();
  decision->set_parameter(rclcpp::Parameter("goal_x", 3.0));
  decision->set_parameter(rclcpp::Parameter("goal_y", 0.0));
  decision->activate();
  publish_map_odom_tf(0.0, 0.0);
  publish_fusion_heartbeat(decision.get(), "alive");

  bool goal_received = false;
  float received_x = 0.0F, received_y = 0.0F;
  auto mock_server = rclcpp_action::create_server<ros2_robot_middleware::action::MoveToPose>(
      decision, "/cmd/move_to_pose",
      [](const rclcpp_action::GoalUUID &,
         std::shared_ptr<const ros2_robot_middleware::action::MoveToPose::Goal>) {
        return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
      },
      [](const std::shared_ptr<rclcpp_action::ServerGoalHandle<ros2_robot_middleware::action::MoveToPose>>) {
        return rclcpp_action::CancelResponse::ACCEPT;
      },
      [&](const std::shared_ptr<rclcpp_action::ServerGoalHandle<ros2_robot_middleware::action::MoveToPose>> gh) {
        goal_received = true;
        const auto g = gh->get_goal();
        received_x = g->target_x;
        received_y = g->target_y;
        auto result = std::make_shared<ros2_robot_middleware::action::MoveToPose::Result>();
        result->reached = true;
        result->final_x = g->target_x;
        result->final_y = g->target_y;
        gh->succeed(result);
      });

  // Single obstacle exactly at the task goal → goal cell LETHAL but the
  // decay ring (<0.75m) is traversable → snap → path → dispatch.
  auto perception = ros2_robot_middleware::msg::PerceptionObjects{};
  perception.header.frame_id = "base_link";
  auto obj = ros2_robot_middleware::msg::Object{};
  obj.id = "blocker";
  obj.x = 3.0F;
  obj.y = 0.0F;
  obj.z = 0.0F;
  perception.objects.push_back(obj);

  auto pub = decision->create_publisher<ros2_robot_middleware::msg::PerceptionObjects>(
      "/perception/objects", rclcpp::QoS(10).reliable());
  pub->on_activate();

  spin_until(decision->get_node_base_interface(),
             [] { return false; }, std::chrono::milliseconds(300));
  pub->publish(perception);

  ASSERT_TRUE(spin_until(decision->get_node_base_interface(),
                         [&goal_received] { return goal_received; },
                         std::chrono::seconds(3)));
  // Dispatch carries the ORIGINAL task goal (map→odom identity here); the
  // snapped approach lives in the /planning/path the motor tracks.
  EXPECT_FLOAT_EQ(received_x, 3.0F);
  EXPECT_FLOAT_EQ(received_y, 0.0F);
}

// Regression: dedup compared the map-frame goal param against the odom-frame
// dispatched value — with a real map→odom offset (8,9) they never matched, so
// the same goal was re-dispatched forever (robot spun at the goal). The gate
// dedups on the map-frame goal identity.
TEST_F(DecisionTest, Given_SameGoalTwice_When_SecondPerception_DispatchesOnce) {
  auto decision = std::make_shared<DecisionNode>();
  decision->configure();
  decision->set_parameter(rclcpp::Parameter("goal_x", 3.0));
  decision->set_parameter(rclcpp::Parameter("goal_y", 0.0));
  decision->activate();
  publish_map_odom_tf(8.0, 9.0);  // real offset: odom goal != map goal
  publish_fusion_heartbeat(decision.get(), "alive");

  int goal_count = 0;
  auto mock_server = rclcpp_action::create_server<ros2_robot_middleware::action::MoveToPose>(
      decision, "/cmd/move_to_pose",
      [](const rclcpp_action::GoalUUID &,
         std::shared_ptr<const ros2_robot_middleware::action::MoveToPose::Goal>) {
        return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
      },
      [](const std::shared_ptr<rclcpp_action::ServerGoalHandle<ros2_robot_middleware::action::MoveToPose>>) {
        return rclcpp_action::CancelResponse::ACCEPT;
      },
      [&](const std::shared_ptr<rclcpp_action::ServerGoalHandle<ros2_robot_middleware::action::MoveToPose>> gh) {
        goal_count++;
        auto result = std::make_shared<ros2_robot_middleware::action::MoveToPose::Result>();
        result->reached = true;
        result->final_x = gh->get_goal()->target_x;
        result->final_y = gh->get_goal()->target_y;
        gh->succeed(result);
      });

  auto perception = ros2_robot_middleware::msg::PerceptionObjects{};
  perception.header.frame_id = "base_link";

  auto pub = decision->create_publisher<ros2_robot_middleware::msg::PerceptionObjects>(
      "/perception/objects", rclcpp::QoS(10).reliable());
  pub->on_activate();

  // TF + heartbeat reach the node before the first perception triggers dispatch.
  spin_until(decision->get_node_base_interface(),
             [] { return false; }, std::chrono::milliseconds(300));
  pub->publish(perception);

  // First perception dispatches exactly once.
  ASSERT_TRUE(spin_until(decision->get_node_base_interface(),
                         [&goal_count] { return goal_count >= 1; },
                         std::chrono::seconds(3)));
  EXPECT_EQ(goal_count, 1);

  // Let the mock goal finish → active_goal_ cleared before the second round.
  spin_until(decision->get_node_base_interface(),
             [] { return false; }, std::chrono::milliseconds(500));

  // Same task goal perceived again → must NOT re-dispatch.
  pub->publish(perception);
  spin_until(decision->get_node_base_interface(),
             [] { return false; }, std::chrono::milliseconds(1000));
  EXPECT_EQ(goal_count, 1);
}

// Cold-start gate: without a fusion heartbeat the grid is not trustworthy —
// no dispatch (previously A* planned straight through the wall on an empty
// grid before perception arrived).
TEST_F(DecisionTest, Given_NoFusionHeartbeat_When_Perception_DoesNotDispatch) {
  auto decision = std::make_shared<DecisionNode>();
  decision->configure();
  decision->set_parameter(rclcpp::Parameter("goal_x", 3.0));
  decision->set_parameter(rclcpp::Parameter("goal_y", 0.0));
  decision->activate();
  publish_map_odom_tf(0.0, 0.0);
  // No fusion heartbeat → gate holds dispatch.

  bool goal_received = false;
  auto mock_server = rclcpp_action::create_server<ros2_robot_middleware::action::MoveToPose>(
      decision, "/cmd/move_to_pose",
      [](const rclcpp_action::GoalUUID &,
         std::shared_ptr<const ros2_robot_middleware::action::MoveToPose::Goal>) {
        return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
      },
      [](const std::shared_ptr<rclcpp_action::ServerGoalHandle<ros2_robot_middleware::action::MoveToPose>>) {
        return rclcpp_action::CancelResponse::ACCEPT;
      },
      [&](const std::shared_ptr<rclcpp_action::ServerGoalHandle<ros2_robot_middleware::action::MoveToPose>> gh) {
        goal_received = true;
        auto result = std::make_shared<ros2_robot_middleware::action::MoveToPose::Result>();
        result->reached = true;
        result->final_x = gh->get_goal()->target_x;
        result->final_y = gh->get_goal()->target_y;
        gh->succeed(result);
      });

  auto perception = ros2_robot_middleware::msg::PerceptionObjects{};
  perception.header.frame_id = "base_link";

  auto pub = decision->create_publisher<ros2_robot_middleware::msg::PerceptionObjects>(
      "/perception/objects", rclcpp::QoS(10).reliable());
  pub->on_activate();

  spin_until(decision->get_node_base_interface(),
             [] { return false; }, std::chrono::milliseconds(300));
  pub->publish(perception);

  spin_until(decision->get_node_base_interface(),
             [&goal_received] { return goal_received; },
             std::chrono::milliseconds(1500));
  EXPECT_FALSE(goal_received);
}

// Gate tightened: "critical" heartbeat (2+ sensors lost) must NOT unlock
// dispatch — the grid is not trustworthy (cold-start wall-penetration class).
TEST_F(DecisionTest, Given_CriticalHeartbeat_When_Perception_DoesNotDispatch) {
  auto decision = std::make_shared<DecisionNode>();
  decision->configure();
  decision->set_parameter(rclcpp::Parameter("goal_x", 3.0));
  decision->set_parameter(rclcpp::Parameter("goal_y", 0.0));
  decision->activate();
  publish_map_odom_tf(0.0, 0.0);
  publish_fusion_heartbeat(decision.get(), "critical");

  bool goal_received = false;
  auto mock_server = rclcpp_action::create_server<ros2_robot_middleware::action::MoveToPose>(
      decision, "/cmd/move_to_pose",
      [](const rclcpp_action::GoalUUID &,
         std::shared_ptr<const ros2_robot_middleware::action::MoveToPose::Goal>) {
        return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
      },
      [](const std::shared_ptr<rclcpp_action::ServerGoalHandle<ros2_robot_middleware::action::MoveToPose>>) {
        return rclcpp_action::CancelResponse::ACCEPT;
      },
      [&](const std::shared_ptr<rclcpp_action::ServerGoalHandle<ros2_robot_middleware::action::MoveToPose>> gh) {
        goal_received = true;
        auto result = std::make_shared<ros2_robot_middleware::action::MoveToPose::Result>();
        result->reached = true;
        result->final_x = gh->get_goal()->target_x;
        result->final_y = gh->get_goal()->target_y;
        gh->succeed(result);
      });

  auto perception = ros2_robot_middleware::msg::PerceptionObjects{};
  perception.header.frame_id = "base_link";

  auto pub = decision->create_publisher<ros2_robot_middleware::msg::PerceptionObjects>(
      "/perception/objects", rclcpp::QoS(10).reliable());
  pub->on_activate();

  spin_until(decision->get_node_base_interface(),
             [] { return false; }, std::chrono::milliseconds(300));
  pub->publish(perception);

  spin_until(decision->get_node_base_interface(),
             [&goal_received] { return goal_received; },
             std::chrono::milliseconds(1500));
  EXPECT_FALSE(goal_received);
}

// Gate tightened: "degraded_no_lidar" heartbeat (lidar lost) must NOT unlock
// dispatch — without lidar the grid is empty, A* would plan straight through.
TEST_F(DecisionTest, Given_DegradedNoLidarHeartbeat_When_Perception_DoesNotDispatch) {
  auto decision = std::make_shared<DecisionNode>();
  decision->configure();
  decision->set_parameter(rclcpp::Parameter("goal_x", 3.0));
  decision->set_parameter(rclcpp::Parameter("goal_y", 0.0));
  decision->activate();
  publish_map_odom_tf(0.0, 0.0);
  publish_fusion_heartbeat(decision.get(), "degraded_no_lidar");

  bool goal_received = false;
  auto mock_server = rclcpp_action::create_server<ros2_robot_middleware::action::MoveToPose>(
      decision, "/cmd/move_to_pose",
      [](const rclcpp_action::GoalUUID &,
         std::shared_ptr<const ros2_robot_middleware::action::MoveToPose::Goal>) {
        return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
      },
      [](const std::shared_ptr<rclcpp_action::ServerGoalHandle<ros2_robot_middleware::action::MoveToPose>>) {
        return rclcpp_action::CancelResponse::ACCEPT;
      },
      [&](const std::shared_ptr<rclcpp_action::ServerGoalHandle<ros2_robot_middleware::action::MoveToPose>> gh) {
        goal_received = true;
        auto result = std::make_shared<ros2_robot_middleware::action::MoveToPose::Result>();
        result->reached = true;
        result->final_x = gh->get_goal()->target_x;
        result->final_y = gh->get_goal()->target_y;
        gh->succeed(result);
      });

  auto perception = ros2_robot_middleware::msg::PerceptionObjects{};
  perception.header.frame_id = "base_link";

  auto pub = decision->create_publisher<ros2_robot_middleware::msg::PerceptionObjects>(
      "/perception/objects", rclcpp::QoS(10).reliable());
  pub->on_activate();

  spin_until(decision->get_node_base_interface(),
             [] { return false; }, std::chrono::milliseconds(300));
  pub->publish(perception);

  spin_until(decision->get_node_base_interface(),
             [&goal_received] { return goal_received; },
             std::chrono::milliseconds(1500));
  EXPECT_FALSE(goal_received);
}

// ── 三审 P1 / Wave 1.5：goal_pose 入口 finite 校验 ──────────────────────
// NaN/Inf 目标直达 world_to_grid 的 static_cast<int> 是 UB（换编译器即静默
// 数据损坏）。坏帧必须拒绝且不污染当前 goal 参数。

TEST_F(DecisionTest, Given_NanGoalPose_WhenPublished_RejectedNotApplied) {
  auto decision = std::make_shared<DecisionNode>(
      rclcpp::NodeOptions()
          .append_parameter_override("goal_x", 3.0)
          .append_parameter_override("goal_y", 4.0));
  decision->configure();

  // 走真实订阅路径（DDS 发布 → 回调守卫），不 friend 破坏封装
  auto pub_node = std::make_shared<rclcpp::Node>("nan_goal_pub");
  auto pub = pub_node->create_publisher<geometry_msgs::msg::PoseStamped>(
      "/goal_pose", rclcpp::QoS(10).reliable());
  rclcpp::executors::SingleThreadedExecutor exec;
  exec.add_node(decision->get_node_base_interface());
  exec.add_node(pub_node);

  auto spin_ms = [&](int ms) {
    auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (std::chrono::steady_clock::now() < end) {
      exec.spin_once(std::chrono::milliseconds(10));
    }
  };
  spin_ms(400);  // 发现匹配

  geometry_msgs::msg::PoseStamped bad;
  bad.pose.position.x = std::numeric_limits<double>::quiet_NaN();
  bad.pose.position.y = std::numeric_limits<double>::infinity();
  pub->publish(bad);
  spin_ms(400);
  // 守卫拒绝：goal 参数保持原值，未被 NaN 污染
  EXPECT_DOUBLE_EQ(decision->get_parameter("goal_x").as_double(), 3.0);
  EXPECT_DOUBLE_EQ(decision->get_parameter("goal_y").as_double(), 4.0);

  geometry_msgs::msg::PoseStamped good;  // 合法帧仍通过（守卫不误伤）
  good.pose.position.x = 7.0;
  good.pose.position.y = 8.0;
  pub->publish(good);
  spin_ms(400);
  EXPECT_DOUBLE_EQ(decision->get_parameter("goal_x").as_double(), 7.0);
  EXPECT_DOUBLE_EQ(decision->get_parameter("goal_y").as_double(), 8.0);
}
