#ifndef ROS2_ROBOT_MIDDLEWARE_MOTOR_CTRL_NODE_HPP_
#define ROS2_ROBOT_MIDDLEWARE_MOTOR_CTRL_NODE_HPP_

#include "ros2_robot_middleware/action/move_to_pose.hpp"
#include "ros2_robot_middleware/application/execution_service.hpp"
#include "ros2_robot_middleware/srv/set_param.hpp"
#include "std_msgs/msg/string.hpp"

#include <rclcpp/rclcpp.hpp>
#include <rclcpp_action/rclcpp_action.hpp>
#include <rclcpp_lifecycle/lifecycle_node.hpp>

// Thin ROS2 adapter — delegates motion interpolation to ExecutionService.
class MotorCtrlNode : public rclcpp_lifecycle::LifecycleNode {
public:
  MotorCtrlNode();

  using CallbackReturn = rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;

  CallbackReturn on_configure(const rclcpp_lifecycle::State &);
  CallbackReturn on_activate(const rclcpp_lifecycle::State &);
  CallbackReturn on_deactivate(const rclcpp_lifecycle::State &);
  CallbackReturn on_cleanup(const rclcpp_lifecycle::State &);
  CallbackReturn on_shutdown(const rclcpp_lifecycle::State &);

  explicit MotorCtrlNode(const rclcpp::NodeOptions &options);

private:
  rclcpp_action::GoalResponse
  handle_goal(const rclcpp_action::GoalUUID &uuid,
              std::shared_ptr<const ros2_robot_middleware::action::MoveToPose::Goal> goal);

  rclcpp_action::CancelResponse handle_cancel(
    const std::shared_ptr<rclcpp_action::ServerGoalHandle<ros2_robot_middleware::action::MoveToPose>> goal_handle);

  void execute(
    const std::shared_ptr<rclcpp_action::ServerGoalHandle<ros2_robot_middleware::action::MoveToPose>> goal_handle);

  void handle_set_param(const std::shared_ptr<ros2_robot_middleware::srv::SetParam::Request> request,
                        std::shared_ptr<ros2_robot_middleware::srv::SetParam::Response> response);

  // Domain layer — pure math
  amr::application::ExecutionService execution_;

  // ROS2 infrastructure
  rclcpp_action::Server<ros2_robot_middleware::action::MoveToPose>::SharedPtr action_server_;
  rclcpp::Service<ros2_robot_middleware::srv::SetParam>::SharedPtr service_server_;
  rclcpp_lifecycle::LifecyclePublisher<std_msgs::msg::String>::SharedPtr status_pub_;
  rclcpp::TimerBase::SharedPtr status_timer_;
};

#endif  // ROS2_ROBOT_MIDDLEWARE_MOTOR_CTRL_NODE_HPP_
