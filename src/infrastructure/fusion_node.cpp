#include "ros2_robot_middleware/infrastructure/fusion_node.hpp"
#include "ros2_robot_middleware/observability/logging.hpp"
#include "ros2_robot_middleware/observability/metrics_registry.hpp"
#include "ros2_robot_middleware/observability/tracer.hpp"

#include <rclcpp_components/register_node_macro.hpp>

#include <cmath>

// ── Constructor ──────────────────────────────────────────────────────────────

FusionNode::FusionNode() : rclcpp_lifecycle::LifecycleNode("fusion") {
}

FusionNode::FusionNode(const rclcpp::NodeOptions &options)
  : rclcpp_lifecycle::LifecycleNode("fusion", options) {
}

#ifdef AMR_TEST_HOOKS
FusionNode::FusionNode(const rclcpp::NodeOptions &options,
                       const amr::domain::perception::DegradationPolicy::Config &deg_config)
  : rclcpp_lifecycle::LifecycleNode("fusion", options),
    perception_(amr::domain::perception::ClusterDetector::Params{}, deg_config)
{
}
#endif

// ── Lifecycle callbacks ──────────────────────────────────────────────────────

FusionNode::CallbackReturn FusionNode::on_configure(const rclcpp_lifecycle::State &) {
  auto qos_best_effort = rclcpp::QoS(10).best_effort();
  auto qos_reliable    = rclcpp::QoS(10).reliable();

  sub_lidar_ = this->create_subscription<sensor_msgs::msg::LaserScan>(
    "/sensor/lidar", qos_best_effort,
    [this](sensor_msgs::msg::LaserScan::SharedPtr msg) { lidar_callback(msg); });

  sub_imu_ = this->create_subscription<sensor_msgs::msg::Imu>(
    "/sensor/imu", qos_reliable,
    [this](sensor_msgs::msg::Imu::SharedPtr msg) { imu_callback(msg); });

  sub_camera_ = this->create_subscription<sensor_msgs::msg::Image>(
    "/sensor/camera", qos_best_effort,
    [this](sensor_msgs::msg::Image::SharedPtr msg) { camera_callback(msg); });

  fusion_pub_ = this->create_publisher<ros2_robot_middleware::msg::PerceptionObjects>(
    "/perception/objects", rclcpp::QoS(10).reliable());

  heartbeat_pub_ = this->create_publisher<std_msgs::msg::String>(
    "/sensor/fusion/heartbeat", rclcpp::QoS(10).reliable());

  return CallbackReturn::SUCCESS;
}

FusionNode::CallbackReturn FusionNode::on_activate(const rclcpp_lifecycle::State &) {
  using namespace std::chrono_literals;
  timer_ = this->create_wall_timer(200ms, [this]() { timer_callback(); });

  heartbeat_timer_ = this->create_wall_timer(1s, [this]() { update_heartbeat_status(); });

  fusion_pub_->on_activate();
  heartbeat_pub_->on_activate();

  return CallbackReturn::SUCCESS;
}

FusionNode::CallbackReturn FusionNode::on_deactivate(const rclcpp_lifecycle::State &) {
  timer_.reset();
  heartbeat_timer_.reset();
  fusion_pub_->on_deactivate();
  heartbeat_pub_->on_deactivate();
  return CallbackReturn::SUCCESS;
}

FusionNode::CallbackReturn FusionNode::on_cleanup(const rclcpp_lifecycle::State &) {
  sub_lidar_.reset();
  sub_imu_.reset();
  sub_camera_.reset();
  fusion_pub_.reset();
  heartbeat_pub_.reset();
  lidar_cache_.reset();
  imu_cache_.reset();
  camera_cache_.reset();
  return CallbackReturn::SUCCESS;
}

FusionNode::CallbackReturn FusionNode::on_shutdown(const rclcpp_lifecycle::State &) {
  timer_.reset();
  heartbeat_timer_.reset();
  sub_lidar_.reset();
  sub_imu_.reset();
  sub_camera_.reset();
  fusion_pub_.reset();
  heartbeat_pub_.reset();
  return CallbackReturn::SUCCESS;
}

// ── ROS2 → Domain adapters (thin wiring, no business logic) ──────────────────

void FusionNode::lidar_callback(sensor_msgs::msg::LaserScan::SharedPtr msg) {
  lidar_cache_ = msg;
  if (msg) {
    perception_.on_lidar(msg->ranges.data(), msg->angle_min, msg->angle_increment);
  }
}

void FusionNode::imu_callback(sensor_msgs::msg::Imu::SharedPtr msg) {
  imu_cache_ = msg;
  if (msg) {
    perception_.on_imu(msg->linear_acceleration.x, msg->linear_acceleration.y);
  }
}

void FusionNode::camera_callback(sensor_msgs::msg::Image::SharedPtr msg) {
  camera_cache_ = msg;
  perception_.on_camera();
}

// ── Timer callback — delegates to domain layer ───────────────────────────────

void FusionNode::timer_callback() {
  TRACE_SCOPE("fusion::timer_callback");

  auto t_start = std::chrono::steady_clock::now();

  // Adaptive dt for domain tick
  auto now = this->now();
  if (last_tick_.nanoseconds() > 0) {
    double dt = (now - last_tick_).seconds();
    if (dt > 0.001 && dt < 1.0) {
      perception_.tick(dt);
    }
  }
  last_tick_ = now;

  auto old_level = current_level_;
  current_level_ = perception_.evaluate_degradation();

  // Map domain clusters → ROS2 message
  auto msg            = ros2_robot_middleware::msg::PerceptionObjects{};
  msg.header.stamp    = this->now();
  msg.header.frame_id = "base_link";

  auto clusters = perception_.fuse(current_level_);
  for (const auto &c : clusters) {
    auto obj = ros2_robot_middleware::msg::Object{};
    obj.id = c.id;
    obj.x  = c.x;
    obj.y  = c.y;
    obj.z  = c.z;
    msg.objects.push_back(obj);
  }

  fusion_pub_->publish(msg);

  // ── Observability: metrics ────────────────────────────────────────────
  auto &m = amr::observability::shared_metrics();
  m.fusion_cycle_count.fetch_add(1, std::memory_order_relaxed);
  m.object_count.store(static_cast<int32_t>(msg.objects.size()),
                       std::memory_order_relaxed);
  m.degradation_level.store(static_cast<int32_t>(current_level_),
                            std::memory_order_relaxed);

  if (current_level_ != old_level) {
    m.degradation_events.fetch_add(1, std::memory_order_relaxed);
    TRACE_EVENT("degradation_changed", "old", static_cast<int64_t>(old_level),
                "new", static_cast<int64_t>(current_level_));
  }

  auto t_end = std::chrono::steady_clock::now();
  auto lat_us = std::chrono::duration_cast<std::chrono::microseconds>(
                    t_end - t_start).count();
  m.fusion_latency.record(lat_us);

  // ── Existing ROS2 logging (unchanged) ────────────────────────────────
  if (current_level_ != old_level) {
    RCLCPP_WARN(this->get_logger(), "Degradation: %d -> %d",
                 static_cast<int>(old_level), static_cast<int>(current_level_));
  }

  RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 5000,
                       "PerceptionObjects published: %zu object(s) [level=%d]",
                       msg.objects.size(), static_cast<int>(current_level_));
}

void FusionNode::update_heartbeat_status() {
  auto msg = std_msgs::msg::String{};
  msg.data = amr::application::PerceptionService::heartbeat_for(current_level_);
  heartbeat_pub_->publish(msg);
}

FusionNode::DegradationLevel FusionNode::degradation_level() const {
  return current_level_;
}

RCLCPP_COMPONENTS_REGISTER_NODE(FusionNode)
