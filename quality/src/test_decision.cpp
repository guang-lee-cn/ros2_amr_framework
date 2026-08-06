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
// Identity TF keeps the dispatched goal equal to the map-frame goal here.
// LifecycleNode does NOT derive from rclcpp::Node (jazzy composes the node
// interfaces) — publish the TF from a plain rclcpp::Node instead.
void publish_identity_map_odom_tf() {
  auto tf_node = std::make_shared<rclcpp::Node>("decision_tf_pub");
  auto tf_broadcaster =
      std::make_shared<tf2_ros::StaticTransformBroadcaster>(tf_node);
  geometry_msgs::msg::TransformStamped tf;
  tf.header.stamp = tf_node->now();
  tf.header.frame_id = "map";
  tf.child_frame_id = "amr/odom";
  tf.transform.translation.x = 0.0;
  tf.transform.translation.y = 0.0;
  tf.transform.translation.z = 0.0;
  tf.transform.rotation.w = 1.0;
  tf_broadcaster->sendTransform(tf);
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
  publish_identity_map_odom_tf();

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

// A* must find a path to the task goal; when the goal cell is blocked, no
// goal may be dispatched (previously send_goal ran unconditionally).
TEST_F(DecisionTest, BlockedGoal_DoesNotSendGoal) {
  auto decision = std::make_shared<DecisionNode>();
  decision->configure();
  decision->set_parameter(rclcpp::Parameter("goal_x", 3.0));
  decision->set_parameter(rclcpp::Parameter("goal_y", 0.0));
  decision->activate();
  publish_identity_map_odom_tf();

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

  // Object placed exactly at the task goal → grid marks it → A* has no path.
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
