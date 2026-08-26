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
#include <visualization_msgs/msg/marker_array.hpp>
#include <rclcpp/rclcpp.hpp>
#include <tf2_ros/transform_broadcaster.h>

class SceneSimulatorNode : public rclcpp::Node {
public:
  SceneSimulatorNode();
  /// NodeOptions 构造（scene_name 等参数经 append_parameter_override 注入；
  /// e2e 测试 test_e2e_behavior 用同一世界+传感闭环做断言式验收）
  explicit SceneSimulatorNode(const rclcpp::NodeOptions &options);

  /// 故障注入钩子：暂停/恢复 20Hz tick（传感+定位+运动学一起断流）。
  /// 正式 API 非 test-only——soak 断源注入、演示"传感器死亡恢复"复用。
  void pause() { timer_->cancel(); }
  void resume() { timer_->reset(); }

private:
  void init();  // 两种构造共用的接线
  void tick();
  /// 构造车体 MarkerArray（底盘+lidar+双轮），frame=amr/chassis。
  /// Marker 是 Foxglove/RViz 原生类型，100% 渲染，不依赖 URDF 加载。
  visualization_msgs::msg::MarkerArray build_robot_markers(const rclcpp::Time &now);
  /// 构造环境 MarkerArray（仓库墙 + box 障碍），frame=map。box 是 ray-cast
  /// 抽象障碍无实体，发 marker 让 rviz2/Foxglove 可见。
  visualization_msgs::msg::MarkerArray build_obstacle_markers(const rclcpp::Time &now);

  // Start mid-warehouse (not pressed against the west wall x=0) so the robot
  // can steer without the guard tripping on the wall during the first turn.
  amr::domain::simulation::Pose pose_{2.0F, 0.0F, 0.0F};
  amr::domain::simulation::SimulatedScene scene_;
  geometry_msgs::msg::Twist cmd_{};

  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr cmd_sub_;
  rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr odom_pub_;
  rclcpp::Publisher<sensor_msgs::msg::LaserScan>::SharedPtr scan_pub_;
  rclcpp::Publisher<geometry_msgs::msg::PoseWithCovarianceStamped>::SharedPtr amcl_pub_;
  rclcpp::Publisher<visualization_msgs::msg::MarkerArray>::SharedPtr marker_pub_;
  rclcpp::Publisher<visualization_msgs::msg::MarkerArray>::SharedPtr obstacle_pub_;
  std::shared_ptr<tf2_ros::TransformBroadcaster> tf_broadcaster_;
  rclcpp::TimerBase::SharedPtr timer_;
};
