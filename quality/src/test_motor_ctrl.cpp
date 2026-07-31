/// @file test_motor_ctrl.cpp — MotorCtrlNode action + service tests
#include "ros2_robot_middleware/infrastructure/motor_ctrl_node.hpp"

#include "ros2_robot_middleware/action/move_to_pose.hpp"
#include "ros2_robot_middleware/srv/set_param.hpp"

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp_action/rclcpp_action.hpp>

#include <chrono>
#include <memory>
#include <thread>

class MotorCtrlTest : public ::testing::Test {
protected:
  static void SetUpTestSuite() { rclcpp::init(0, nullptr); }
  static void TearDownTestSuite() { rclcpp::shutdown(); }
};

// Spin the node's executor on a dedicated thread — the action execute()
// callback runs a blocking 20Hz control loop, so it must not share the
// test thread (spin_once would block forever waiting on the goal result).
class SpinHelper {
public:
  explicit SpinHelper(rclcpp::node_interfaces::NodeBaseInterface::SharedPtr node_iface) {
    exec_.add_node(node_iface);
    thread_ = std::thread([this]() { exec_.spin(); });
  }
  ~SpinHelper() {
    exec_.cancel();
    if (thread_.joinable()) thread_.join();
  }
private:
  // MultiThreadedExecutor: the action execute() callback blocks in a 20Hz
  // control loop; multiple threads let the executor keep processing the
  // action's cancel/result machinery while execute runs.
  rclcpp::executors::MultiThreadedExecutor exec_;
  std::thread thread_;
};

template <typename Predicate>
bool wait_until(Predicate pred, std::chrono::milliseconds timeout) {
  auto start = std::chrono::steady_clock::now();
  while (!pred() && (std::chrono::steady_clock::now() - start) < timeout) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return pred();
}

TEST_F(MotorCtrlTest, CloseTarget_ReachesImmediately) {
  auto motor = std::make_shared<MotorCtrlNode>();
  motor->configure();
  motor->activate();
  auto client = rclcpp_action::create_client<ros2_robot_middleware::action::MoveToPose>(
      motor, "/cmd/move_to_pose");
  ASSERT_TRUE(client->wait_for_action_server(std::chrono::seconds(2)));

  auto goal = ros2_robot_middleware::action::MoveToPose::Goal{};
  goal.target_x = 0.0F;
  goal.target_y = 0.03F;
  goal.target_theta = 0.0F;
  goal.max_speed = 0.5F;

  bool done = false, reached = false;
  auto opts = rclcpp_action::Client<ros2_robot_middleware::action::MoveToPose>::SendGoalOptions{};
  opts.result_callback =
      [&](const rclcpp_action::ClientGoalHandle<ros2_robot_middleware::action::MoveToPose>::WrappedResult &r) {
        done = true;
        reached = r.result->reached;
      };

  client->async_send_goal(goal, opts);
  SpinHelper spin(motor->get_node_base_interface());
  ASSERT_TRUE(wait_until([&done] { return done; }, std::chrono::seconds(5)));
  EXPECT_TRUE(reached);
}

TEST_F(MotorCtrlTest, FarTarget_StepsUntilReached) {
  auto motor = std::make_shared<MotorCtrlNode>();
  motor->configure();
  motor->activate();
  auto client = rclcpp_action::create_client<ros2_robot_middleware::action::MoveToPose>(
      motor, "/cmd/move_to_pose");
  ASSERT_TRUE(client->wait_for_action_server(std::chrono::seconds(2)));

  auto goal = ros2_robot_middleware::action::MoveToPose::Goal{};
  goal.target_x = 0.0F;
  goal.target_y = 0.15F;
  goal.target_theta = 0.0F;
  goal.max_speed = 0.5F;

  bool done = false, reached = false;
  auto opts = rclcpp_action::Client<ros2_robot_middleware::action::MoveToPose>::SendGoalOptions{};
  opts.result_callback =
      [&](const rclcpp_action::ClientGoalHandle<ros2_robot_middleware::action::MoveToPose>::WrappedResult &r) {
        done = true;
        reached = r.result->reached;
      };

  client->async_send_goal(goal, opts);
  SpinHelper spin(motor->get_node_base_interface());
  ASSERT_TRUE(wait_until([&done] { return done; }, std::chrono::seconds(5)));
  EXPECT_TRUE(reached);
}

TEST_F(MotorCtrlTest, SetParamKnown_UpdatesAndAcks) {
  auto motor = std::make_shared<MotorCtrlNode>();
  motor->configure();
  motor->activate();
  auto client = motor->create_client<ros2_robot_middleware::srv::SetParam>("/cmd/set_param");
  ASSERT_TRUE(client->wait_for_service(std::chrono::seconds(2)));

  auto request = std::make_shared<ros2_robot_middleware::srv::SetParam::Request>();
  request->param_name = "step_size";
  request->value = 0.10;
  auto future = client->async_send_request(request);

  SpinHelper spin(motor->get_node_base_interface());
  ASSERT_TRUE(wait_until(
      [&future] { return future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready; },
      std::chrono::seconds(2)));
  auto response = future.get();
  EXPECT_TRUE(response->success);
  EXPECT_EQ(response->message, "Parameter updated");
}

TEST_F(MotorCtrlTest, SetParamUnknown_ReturnsMessage) {
  auto motor = std::make_shared<MotorCtrlNode>();
  motor->configure();
  motor->activate();
  auto client = motor->create_client<ros2_robot_middleware::srv::SetParam>("/cmd/set_param");
  ASSERT_TRUE(client->wait_for_service(std::chrono::seconds(2)));

  auto request = std::make_shared<ros2_robot_middleware::srv::SetParam::Request>();
  request->param_name = "unknown_param";
  request->value = 1.0;
  auto future = client->async_send_request(request);

  SpinHelper spin(motor->get_node_base_interface());
  ASSERT_TRUE(wait_until(
      [&future] { return future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready; },
                         std::chrono::seconds(2)));
  auto response = future.get();
  EXPECT_TRUE(response->success);
  EXPECT_EQ(response->message, "Unknown parameter");
}
