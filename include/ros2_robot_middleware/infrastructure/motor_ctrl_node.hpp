#ifndef ROS2_ROBOT_MIDDLEWARE_MOTOR_CTRL_NODE_HPP_
#define ROS2_ROBOT_MIDDLEWARE_MOTOR_CTRL_NODE_HPP_

#include "ros2_robot_middleware/action/move_to_pose.hpp"
#include "ros2_robot_middleware/domain/execution/collision_guard.hpp"
#include "ros2_robot_middleware/domain/execution/pure_pursuit.hpp"
#include "ros2_robot_middleware/domain/execution/vfh_avoidance.hpp"
#include "ros2_robot_middleware/domain/execution/velocity_smoother.hpp"
#include "ros2_robot_middleware/domain/planning/track_error_monitor.hpp"
#include "ros2_robot_middleware/srv/set_param.hpp"
#include "std_msgs/msg/string.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "sensor_msgs/msg/laser_scan.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "geometry_msgs/msg/pose_array.hpp"

#include <atomic>
#include <cmath>
#include <mutex>
#include <vector>
#include "ros2_robot_middleware/infrastructure/amr_node.hpp"
#include <rclcpp_action/rclcpp_action.hpp>
#include <rclcpp_lifecycle/lifecycle_node.hpp>

// MotorCtrlNode — Pure Pursuit path tracking with /odom feedback.
// Closed loop: current pose comes from /odom (robot_localization EKF).
// Falls back to kinematic integration when odom not yet available.
class MotorCtrlNode : public amr::infrastructure::AmrNode {
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

  void on_odom(const nav_msgs::msg::Odometry::SharedPtr msg);
  void on_scan(const sensor_msgs::msg::LaserScan::SharedPtr msg);
  void on_path(const geometry_msgs::msg::PoseArray::SharedPtr msg);

  // Publish the velocity command to the base (DiffDrive in sim, IActuator in prod)
  void publish_twist(float linear, float angular);

  // Domain layer — Pure Pursuit path tracking + lateral error monitor +
  // collision guard (G2-C: clamps forward velocity) + VFH avoidance
  // (G2-B: steers around obstacles near the goal bearing)
  amr::domain::execution::PurePursuit tracker_;
  amr::domain::planning::TrackErrorMonitor error_monitor_;
  amr::domain::execution::CollisionGuard guard_;
  amr::domain::execution::VfhAvoidance vhf_;
  // G2-D control-layer hardening: time-domain acceleration limits on the
  // published /cmd_vel (PurePursuit only limits in the geometry domain).
  amr::domain::execution::VelocitySmoother smoother_;
  amr::domain::execution::Twist2D last_cmd_{0.0F, 0.0F};
  bool vfh_enabled_{true};  // VFH local avoidance; demo disables (A* path already avoids)

  // ROS2 infrastructure
  rclcpp_action::Server<ros2_robot_middleware::action::MoveToPose>::SharedPtr action_server_;
  rclcpp::Service<ros2_robot_middleware::srv::SetParam>::SharedPtr service_server_;
  rclcpp_lifecycle::LifecyclePublisher<std_msgs::msg::String>::SharedPtr status_pub_;
  rclcpp::TimerBase::SharedPtr status_timer_;

  // /cmd_vel — velocity command to the base (closed-loop execution output)
  rclcpp_lifecycle::LifecyclePublisher<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_pub_;

  // /odom feedback — closed-loop pose source (robot_localization EKF)
  // Dedicated callback group: the blocking action execute() loop lives in the
  // node's default MutuallyExclusive group and would starve a subscription in
  // the same group (rclcpp known behaviour). Separate group ⇒ runs in parallel.
  rclcpp::CallbackGroup::SharedPtr odom_cb_group_;
  rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odom_sub_;

  // /scan — the guard's obstacle data (2D lidar ranges). on_scan feeds the
  // raw ranges straight to the guard (no ground filter — see on_scan).
  rclcpp::Subscription<sensor_msgs::msg::LaserScan>::SharedPtr scan_sub_;

  // /planning/path — the decision layer's A* global path. The motor tracks
  // it (not a 2-point straight line) so the robot follows the globally-planned
  // avoidance around obstacles. Demo: map≡odom so the map-frame path is used
  // directly; production would transform via TF.
  rclcpp::Subscription<geometry_msgs::msg::PoseArray>::SharedPtr path_sub_;
  std::vector<amr::domain::execution::Waypoint> latest_path_;
  mutable std::mutex path_mutex_;

  // Thread-safe current pose (updated by odom callback, read by execute loop)
  mutable std::mutex pose_mutex_;
  amr::domain::execution::Pose2D current_pose_;
  std::atomic<bool> odom_valid_{false};
};

#endif  // ROS2_ROBOT_MIDDLEWARE_MOTOR_CTRL_NODE_HPP_
