#ifndef ROS2_ROBOT_MIDDLEWARE_DECISION_NODE_HPP_
#define ROS2_ROBOT_MIDDLEWARE_DECISION_NODE_HPP_

#include "ros2_robot_middleware/action/move_to_pose.hpp"
#include "ros2_robot_middleware/application/planning_service.hpp"
#include "ros2_robot_middleware/msg/perception_objects.hpp"
#include "std_msgs/msg/string.hpp"

#include <rclcpp/rclcpp.hpp>
#include <rclcpp_action/rclcpp_action.hpp>
#include <rclcpp_lifecycle/lifecycle_node.hpp>

// Thin ROS2 adapter — delegates planning logic to PlanningService.
// Handles only DDS subscription + Action client + lifecycle callbacks.
class DecisionNode : public rclcpp_lifecycle::LifecycleNode {
public:
  DecisionNode();

  using CallbackReturn = rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;

  CallbackReturn on_configure(const rclcpp_lifecycle::State &);
  CallbackReturn on_activate(const rclcpp_lifecycle::State &);
  CallbackReturn on_deactivate(const rclcpp_lifecycle::State &);
  CallbackReturn on_cleanup(const rclcpp_lifecycle::State &);
  CallbackReturn on_shutdown(const rclcpp_lifecycle::State &);

  explicit DecisionNode(const rclcpp::NodeOptions &options);

private:
  void on_perception(const ros2_robot_middleware::msg::PerceptionObjects::SharedPtr &objs);
  void on_goal_response(
    const rclcpp_action::ClientGoalHandle<ros2_robot_middleware::action::MoveToPose>::SharedPtr &goalhdl);
  void on_result(
    const rclcpp_action::ClientGoalHandle<ros2_robot_middleware::action::MoveToPose>::WrappedResult &result);

  void send_goal(float target_x, float target_y);
  void cancel_active_goal();

  // Domain layer
  amr::application::PlanningService planning_;

  // ROS2 infrastructure
  rclcpp::Subscription<ros2_robot_middleware::msg::PerceptionObjects>::SharedPtr decision_sub_;
  rclcpp_lifecycle::LifecyclePublisher<std_msgs::msg::String>::SharedPtr heartbeat_pub_;
  rclcpp::TimerBase::SharedPtr heartbeat_timer_;
  rclcpp_action::Client<ros2_robot_middleware::action::MoveToPose>::SharedPtr client_;

  // Preemption state (ROS2-specific — goal handle lifecycle)
  rclcpp_action::ClientGoalHandle<ros2_robot_middleware::action::MoveToPose>::SharedPtr active_goal_;
  float last_target_x_ = 0.0F;
  float last_target_y_ = 0.0F;
  int retry_count_ = 0;
};

#endif
