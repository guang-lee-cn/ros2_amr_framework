#ifndef ROS2_ROBOT_MIDDLEWARE_ALIASES_HPP_
#define ROS2_ROBOT_MIDDLEWARE_ALIASES_HPP_

#include "ros2_robot_middleware/action/move_to_pose.hpp"
#include "ros2_robot_middleware/msg/object.hpp"
#include "ros2_robot_middleware/msg/perception_objects.hpp"
#include "ros2_robot_middleware/srv/set_param.hpp"

#include <rclcpp_action/rclcpp_action.hpp>

// ── Namespace aliases ────────────────────────────────────────────
namespace rrm = ros2_robot_middleware::msg;
namespace rra = ros2_robot_middleware::action;
namespace rrs = ros2_robot_middleware::srv;

// ── Type aliases (usable in all .cpp files) ──────────────────────
using Object            = rrm::Object;
using PerceptionObjects = rrm::PerceptionObjects;

using MoveToPose         = rra::MoveToPose;
using MoveToPoseGoal     = MoveToPose::Goal;
using MoveToPoseResult   = MoveToPose::Result;
using MoveToPoseFeedback = MoveToPose::Feedback;
using ClientGoalHandle = rclcpp_action::ClientGoalHandle<MoveToPose>;
using ServerGoalHandle = rclcpp_action::ServerGoalHandle<MoveToPose>;

using SetParam     = rrs::SetParam;
using SetParamReq  = SetParam::Request;
using SetParamResp = SetParam::Response;

#endif  // ROS2_ROBOT_MIDDLEWARE_ALIASES_HPP_
