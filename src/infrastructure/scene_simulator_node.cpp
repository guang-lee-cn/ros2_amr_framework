#include "ros2_robot_middleware/infrastructure/scene_simulator_node.hpp"

#include <tf2/LinearMath/Quaternion.h>

#include <cmath>

SceneSimulatorNode::SceneSimulatorNode() : Node("scene_simulator") {
  cmd_sub_ = create_subscription<geometry_msgs::msg::Twist>(
      "/cmd_vel", 10,
      [this](geometry_msgs::msg::Twist::SharedPtr m) { cmd_ = *m; });

  odom_pub_ = create_publisher<nav_msgs::msg::Odometry>("/odom", 10);
  scan_pub_ = create_publisher<sensor_msgs::msg::LaserScan>("/scan", 10);
  amcl_pub_ =
      create_publisher<geometry_msgs::msg::PoseWithCovarianceStamped>(
          "/amcl_pose", 10);
  // Robot body marker (CUBE in amr/chassis frame) — reliable visualization in
  // Foxglove without URDF loading / frame matching issues.
  marker_pub_ = create_publisher<visualization_msgs::msg::Marker>(
      "/robot_marker", 10);
  tf_broadcaster_ = std::make_shared<tf2_ros::TransformBroadcaster>(this);

  timer_ = create_wall_timer(std::chrono::milliseconds(50),
                             [this]() { tick(); });  // 20 Hz
}

void SceneSimulatorNode::tick() {
  constexpr float kDt = 0.05F;  // 20 Hz control period
  pose_ = amr::domain::simulation::SimulatedScene::step(
      pose_, cmd_.linear.x, cmd_.angular.z, kDt);
  const auto now = this->now();

  // ── /odom (motor's closed-loop pose source) ─────────────────────────
  nav_msgs::msg::Odometry odom;
  odom.header.stamp = now;
  odom.header.frame_id = "amr/odom";
  odom.child_frame_id = "amr/chassis";
  tf2::Quaternion q;
  q.setRPY(0.0, 0.0, pose_.theta);
  odom.pose.pose.position.x = pose_.x;
  odom.pose.pose.position.y = pose_.y;
  odom.pose.pose.orientation.x = q.x();
  odom.pose.pose.orientation.y = q.y();
  odom.pose.pose.orientation.z = q.z();
  odom.pose.pose.orientation.w = q.w();
  odom_pub_->publish(odom);

  // ── /scan (fusion + motor) ──────────────────────────────────────────
  auto ranges = scene_.generate_scan(pose_.x, pose_.y, pose_.theta);
  sensor_msgs::msg::LaserScan scan;
  scan.header.stamp = now;
  scan.header.frame_id = "amr/chassis/lidar";
  scan.angle_min = -static_cast<float>(M_PI);
  scan.angle_max = static_cast<float>(M_PI);
  scan.angle_increment =
      2.0F * static_cast<float>(M_PI) / scene_.params().beam_count;
  scan.range_min = 0.1F;
  scan.range_max = scene_.params().range_max;
  scan.ranges = std::move(ranges);
  scan_pub_->publish(scan);

  // ── /amcl_pose (decision's A* start; perfect localization for demo) ──
  geometry_msgs::msg::PoseWithCovarianceStamped amcl;
  amcl.header.stamp = now;
  amcl.header.frame_id = "map";
  amcl.pose.pose = odom.pose.pose;  // map ≡ odom aligned
  amcl_pub_->publish(amcl);

  // ── TF: map→amr/odom (fixed identity) + amr/odom→amr/chassis (robot) ─
  geometry_msgs::msg::TransformStamped t;
  t.header.stamp = now;
  t.header.frame_id = "map";
  t.child_frame_id = "amr/odom";
  t.transform.rotation.w = 1.0;
  tf_broadcaster_->sendTransform(t);

  t.header.frame_id = "amr/odom";
  t.child_frame_id = "amr/chassis";
  t.transform.translation.x = pose_.x;
  t.transform.translation.y = pose_.y;
  t.transform.translation.z = 0.0;
  t.transform.rotation.x = q.x();
  t.transform.rotation.y = q.y();
  t.transform.rotation.z = q.z();
  t.transform.rotation.w = q.w();
  tf_broadcaster_->sendTransform(t);

  // amr/chassis → amr/chassis/lidar — /scan is published in this frame;
  // without the TF, Foxglove cannot place the scan and it shows misplaced.
  t.header.frame_id = "amr/chassis";
  t.child_frame_id = "amr/chassis/lidar";
  t.transform.translation.x = 0.25F;
  t.transform.translation.y = 0.0F;
  t.transform.translation.z = 0.30F;
  t.transform.rotation.x = 0.0F;
  t.transform.rotation.y = 0.0F;
  t.transform.rotation.z = 0.0F;
  t.transform.rotation.w = 1.0;
  tf_broadcaster_->sendTransform(t);

  // ── Robot body marker (anchored to amr/chassis — follows the robot) ──
  visualization_msgs::msg::Marker m;
  m.header.stamp = now;
  m.header.frame_id = "amr/chassis";
  m.ns = "robot";
  m.id = 0;
  m.type = visualization_msgs::msg::Marker::CUBE;
  m.action = visualization_msgs::msg::Marker::ADD;
  m.pose.orientation.w = 1.0;
  m.scale.x = 0.6F;
  m.scale.y = 0.4F;
  m.scale.z = 0.2F;
  m.color.r = 0.2F;
  m.color.g = 0.35F;
  m.color.b = 0.9F;
  m.color.a = 1.0F;
  marker_pub_->publish(m);
}
