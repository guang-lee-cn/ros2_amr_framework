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
  // 车体 MarkerArray（底盘+lidar+双轮）— Foxglove/RViz 原生渲染，不依赖 URDF。
  marker_pub_ = create_publisher<visualization_msgs::msg::MarkerArray>(
      "/robot_model", 10);
  // 环境 MarkerArray（墙+box）— box 是抽象障碍无实体，发 marker 让可视化可见。
  obstacle_pub_ = create_publisher<visualization_msgs::msg::MarkerArray>(
      "/obstacles", 10);
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

  // ── 车体 MarkerArray（底盘+lidar+双轮，锚定 amr/chassis 跟随车移动）──
  marker_pub_->publish(build_robot_markers(now));
  // ── 环境 MarkerArray（墙+box，锚定 map 静态）──
  obstacle_pub_->publish(build_obstacle_markers(now));
}

visualization_msgs::msg::MarkerArray SceneSimulatorNode::build_obstacle_markers(
    const rclcpp::Time &now) {
  visualization_msgs::msg::MarkerArray ma;
  auto add_box = [&](int id, double x, double y, double z,
                     double sx, double sy, double sz,
                     double r, double g, double b) {
    visualization_msgs::msg::Marker m;
    m.header.stamp = now;
    m.header.frame_id = "map";
    m.ns = "obstacles";
    m.id = id;
    m.type = visualization_msgs::msg::Marker::CUBE;
    m.action = visualization_msgs::msg::Marker::ADD;
    m.pose.position.x = x; m.pose.position.y = y; m.pose.position.z = z;
    m.pose.orientation.w = 1.0;
    m.scale.x = sx; m.scale.y = sy; m.scale.z = sz;
    m.color.r = r; m.color.g = g; m.color.b = b; m.color.a = 0.6;
    ma.markers.push_back(std::move(m));
  };
  // 仓库墙（灰，x∈[0,19] y∈[-2,2]）
  add_box(0, 0.0, 0.0, 0.5, 0.1, 4.0, 1.0, 0.5, 0.5, 0.5);     // west
  add_box(1, 19.0, 0.0, 0.5, 0.1, 4.0, 1.0, 0.5, 0.5, 0.5);    // east
  add_box(2, 9.5, -2.0, 0.5, 19.0, 0.1, 1.0, 0.5, 0.5, 0.5);   // south
  add_box(3, 9.5, 2.0, 0.5, 19.0, 0.1, 1.0, 0.5, 0.5, 0.5);    // north
  // box 障碍（红，0.5×0.5×0.5 @ (8,0)）
  add_box(4, 8.0, 0.0, 0.25, 0.5, 0.5, 0.5, 0.9, 0.2, 0.2);
  return ma;
}

visualization_msgs::msg::MarkerArray SceneSimulatorNode::build_robot_markers(
    const rclcpp::Time &now) {
  using visualization_msgs::msg::Marker;
  visualization_msgs::msg::MarkerArray ma;
  auto add = [&](int id, int type, double x, double y, double z,
                 double sx, double sy, double sz,
                 double r, double g, double b,
                 tf2::Quaternion q = tf2::Quaternion(0, 0, 0, 1)) {
    Marker m;
    m.header.stamp = now;
    m.header.frame_id = "amr/chassis";
    m.ns = "robot";
    m.id = id;
    m.type = type;
    m.action = Marker::ADD;
    m.pose.position.x = x; m.pose.position.y = y; m.pose.position.z = z;
    m.pose.orientation.x = q.x(); m.pose.orientation.y = q.y();
    m.pose.orientation.z = q.z(); m.pose.orientation.w = q.w();
    m.scale.x = sx; m.scale.y = sy; m.scale.z = sz;
    m.color.r = r; m.color.g = g; m.color.b = b; m.color.a = 1.0;
    ma.markers.push_back(std::move(m));
  };
  // 底盘 box 0.6×0.4×0.2（中心离地 0.1m，底部贴地）蓝色
  add(0, Marker::CUBE, 0, 0, 0.10, 0.6, 0.4, 0.2, 0.20, 0.35, 0.90);
  // lidar 圆柱 r0.05 h0.05，挂点 (0.25,0,0.30) 黑色
  add(1, Marker::CYLINDER, 0.25, 0, 0.30, 0.10, 0.10, 0.05, 0.15, 0.15, 0.15);
  // 双轮 r0.075 宽0.04，挂点 (0,±0.25,-0.05)，轴沿 y（绕 x 转 π/2）黑色
  tf2::Quaternion wheel; wheel.setRPY(M_PI / 2, 0, 0);
  add(2, Marker::CYLINDER, 0, 0.25, -0.05, 0.15, 0.15, 0.04, 0.10, 0.10, 0.10, wheel);
  add(3, Marker::CYLINDER, 0, -0.25, -0.05, 0.15, 0.15, 0.04, 0.10, 0.10, 0.10, wheel);
  return ma;
}
