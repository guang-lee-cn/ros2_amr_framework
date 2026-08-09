#ifndef ROS2_ROBOT_MIDDLEWARE_DECISION_NODE_HPP_
#define ROS2_ROBOT_MIDDLEWARE_DECISION_NODE_HPP_

#include "ros2_robot_middleware/action/move_to_pose.hpp"
#include "ros2_robot_middleware/domain/planning/astar_planner.hpp"
#include "ros2_robot_middleware/domain/planning/goal_dispatch_gate.hpp"
#include "ros2_robot_middleware/domain/planning/grid_updater.hpp"
#include "ros2_robot_middleware/domain/planning/path_smoother.hpp"
#include "ros2_robot_middleware/domain/planning/preempt_policy.hpp"
#include "ros2_robot_middleware/domain/planning/target_selector.hpp"
#include "ros2_robot_middleware/domain/planning/scan_to_grid.hpp"
#include "ros2_robot_middleware/msg/perception_objects.hpp"
#include "std_msgs/msg/string.hpp"
#include "sensor_msgs/msg/laser_scan.hpp"
#include "geometry_msgs/msg/pose_array.hpp"
#include "geometry_msgs/msg/pose_with_covariance_stamped.hpp"
#include "tf2_ros/buffer.h"
#include "tf2_ros/transform_listener.h"

#include <atomic>
#include <memory>
#include <mutex>
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
  void on_amcl_pose(const geometry_msgs::msg::PoseWithCovarianceStamped::SharedPtr msg);
  void on_fusion_heartbeat(const std_msgs::msg::String::SharedPtr msg);
  void on_scan(const sensor_msgs::msg::LaserScan::SharedPtr msg);
  void on_goal_response(
    const rclcpp_action::ClientGoalHandle<ros2_robot_middleware::action::MoveToPose>::SharedPtr &goalhdl);
  void on_result(
    const rclcpp_action::ClientGoalHandle<ros2_robot_middleware::action::MoveToPose>::WrappedResult &result);

  void send_goal(float target_x, float target_y);
  void cancel_active_goal();
  void publish_path(const std::vector<amr::domain::planning::Waypoint> &path);

  // Domain layer — direct domain classes (application/ removed)
  amr::domain::planning::TargetSelector selector_;
  amr::domain::planning::PreemptPolicy preempt_;
  amr::domain::planning::AStarPlanner astar_;
  amr::domain::planning::PathSmoother smoother_;
  amr::domain::planning::GridUpdater grid_updater_;
  amr::domain::planning::OccupancyGrid demo_grid_;

  // ROS2 infrastructure
  // Own callback group: the sibling motor_ctrl's action server can starve
  // subscriptions sharing the node default group (rclcpp known behaviour).
  rclcpp::CallbackGroup::SharedPtr perception_cb_group_;
  rclcpp::Subscription<ros2_robot_middleware::msg::PerceptionObjects>::SharedPtr decision_sub_;
  rclcpp_lifecycle::LifecyclePublisher<std_msgs::msg::String>::SharedPtr heartbeat_pub_;
  rclcpp_lifecycle::LifecyclePublisher<geometry_msgs::msg::PoseArray>::SharedPtr path_pub_;
  rclcpp::TimerBase::SharedPtr heartbeat_timer_;
  rclcpp_action::Client<ros2_robot_middleware::action::MoveToPose>::SharedPtr client_;

  // /amcl_pose — robot pose in the map frame, used as the A* start (G1c)
  rclcpp::Subscription<geometry_msgs::msg::PoseWithCovarianceStamped>::SharedPtr amcl_pose_sub_;
  mutable std::mutex pose_mutex_;
  amr::domain::planning::Pose current_pose_{0.0F, 0.0F};
  float current_theta_ = 0.0F;  // robot 朝向（amcl orientation，raytrace 用）
  std::atomic<bool> has_pose_{false};

  // map→odom TF (AMCL publishes it). decision plans in the map frame but the
  // motor tracks odom/world-frame poses — dispatched goals are transformed
  // into the motor's frame (fixes the G1 coordinate mismatch that drove the
  // robot out of the warehouse). Unique_ptrs created in on_configure.
  std::unique_ptr<tf2_ros::Buffer> tf_buffer_;
  std::unique_ptr<tf2_ros::TransformListener> tf_listener_;

  // Preemption state (ROS2-specific — goal handle lifecycle). Guarded by
  // goal_mutex_: on_perception (Reentrant group) reads active_goal_ while
  // on_goal_response/on_result (default group) write it — a cross-group data
  // race without a lock (B4, §0 铁律5 UB 零容忍).
  mutable std::mutex goal_mutex_;
  rclcpp_action::ClientGoalHandle<ros2_robot_middleware::action::MoveToPose>::SharedPtr active_goal_;
  float last_target_x_ = 0.0F;
  float last_target_y_ = 0.0F;
  int retry_count_ = 0;

  // Dispatch gate: fusion-ready gating + same-goal dedup (map-frame identity).
  // Extraction keeps decision logic unit-testable without ROS2.
  amr::domain::planning::GoalDispatchGate gate_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr fusion_hb_sub_;

  // /scan raytrace（NAV2 ObstacleLayer 对标）— 替代质心 mark_obstacles，
  // 覆盖 box 表面多点 LETHAL（治撞 box）。
  amr::domain::planning::ScanToGrid scan_to_grid_;
  rclcpp::Subscription<sensor_msgs::msg::LaserScan>::SharedPtr scan_sub_;
  std::vector<float> latest_ranges_;
  float latest_angle_min_ = 0.0F;
  float latest_angle_inc_ = 0.0F;
  bool has_scan_ = false;
  mutable std::mutex scan_mutex_;
};

#endif
