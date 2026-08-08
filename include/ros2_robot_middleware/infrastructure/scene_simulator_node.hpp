#pragma once
/// @brief  SceneSimulatorNode — drives the compute container with synthetic
///         sensor data (no physics sim). Replaces Gazebo for the interview
///         demo: gz-sim's gpu_lidar has engine bugs on this platform that
///         break dynamic scans (see architecture doc §7).
///
/// Publishes (20 Hz): /scan, /odom, /amcl_pose, TF (map→amr/odom→amr/chassis).
/// Subscribes: /cmd_vel (the motor's output — closes the loop).
///
/// The robot pose integrates /cmd_vel kinematically; /scan is ray-cast from
/// the pose by SimulatedScene; /amcl_pose mirrors the true pose (perfect
/// localization) so decision's A* works without AMCL.

#include "ros2_robot_middleware/domain/simulation/simulated_scene.hpp"

#include <chrono>
#include <memory>

#include <geometry_msgs/msg/pose_with_covariance_stamped.hpp>
#include <geometry_msgs/msg/twist.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <sensor_msgs/msg/laser_scan.hpp>
#include <visualization_msgs/msg/marker.hpp>
#include <rclcpp/rclcpp.hpp>
#include <tf2_ros/transform_broadcaster.h>

class SceneSimulatorNode : public rclcpp::Node {
public:
  SceneSimulatorNode();

private:
  void tick();

  // Start mid-warehouse (not pressed against the west wall x=0) so the robot
  // can steer without the guard tripping on the wall during the first turn.
  amr::domain::simulation::Pose pose_{2.0F, 0.0F, 0.0F};
  amr::domain::simulation::SimulatedScene scene_;
  geometry_msgs::msg::Twist cmd_{};

  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr cmd_sub_;
  rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr odom_pub_;
  rclcpp::Publisher<sensor_msgs::msg::LaserScan>::SharedPtr scan_pub_;
  rclcpp::Publisher<geometry_msgs::msg::PoseWithCovarianceStamped>::SharedPtr amcl_pub_;
  rclcpp::Publisher<visualization_msgs::msg::Marker>::SharedPtr marker_pub_;
  std::shared_ptr<tf2_ros::TransformBroadcaster> tf_broadcaster_;
  rclcpp::TimerBase::SharedPtr timer_;
};
