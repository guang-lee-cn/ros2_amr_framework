/// @file test_decision.cpp — DecisionNode perception→goal dispatch tests
#include "ros2_robot_middleware/infrastructure/decision_node.hpp"

#include "ros2_robot_middleware/action/move_to_pose.hpp"
#include "ros2_robot_middleware/msg/perception_objects.hpp"

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

TEST_F(DecisionTest, PerceptionArrives_SendsGoalToTargetPose) {
  auto decision = std::make_shared<DecisionNode>();
  decision->configure();
  decision->activate();

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
  pub->publish(perception);

  ASSERT_TRUE(spin_until(decision->get_node_base_interface(),
                         [&goal_received] { return goal_received; },
                         std::chrono::seconds(3)));

  EXPECT_TRUE(goal_received);
  EXPECT_FLOAT_EQ(received_x, 2.0F);
  EXPECT_FLOAT_EQ(received_y, 1.5F);
}
