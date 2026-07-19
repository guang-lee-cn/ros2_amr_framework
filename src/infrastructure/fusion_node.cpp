#include "ros2_robot_middleware/infrastructure/fusion_node.hpp"
#include "ros2_robot_middleware/observability/logging.hpp"
#include "ros2_robot_middleware/observability/metrics_registry.hpp"
#include "ros2_robot_middleware/observability/trace_points.hpp"
#include "ros2_robot_middleware/observability/tracer.hpp"

#include <rclcpp_components/register_node_macro.hpp>

#include <cmath>

// ── Constructors ─────────────────────────────────────────────────────

FusionNode::FusionNode() : rclcpp_lifecycle::LifecycleNode("fusion") {
  declare_sensor_parameters();
}

FusionNode::FusionNode(const rclcpp::NodeOptions &options)
  : rclcpp_lifecycle::LifecycleNode("fusion", options) {
  declare_sensor_parameters();
}

FusionNode::FusionNode(const rclcpp::NodeOptions &options,
                       const amr::domain::perception::DegradationPolicy::Config &deg_config)
  : rclcpp_lifecycle::LifecycleNode("fusion", options) {
  declare_sensor_parameters();
  // Create sensors immediately for test hook (skips on_configure lifecycle)
  create_sensors();
  perception_.emplace(*lidar_, *imu_, *camera_,
                      amr::domain::perception::ClusterDetector::Params{}, deg_config);
}

// ── Sensor parameter declaration ─────────────────────────────────────

void FusionNode::declare_sensor_parameters() {
  this->declare_parameter("sensors.lidar.type", "simulated");
  this->declare_parameter("sensors.lidar.topic", "/scan");
  this->declare_parameter("sensors.imu.type", "simulated");
  this->declare_parameter("sensors.imu.topic", "/imu/data");
  this->declare_parameter("sensors.camera.type", "simulated");
  this->declare_parameter("sensors.camera.topic", "/camera/color/image_raw");
}

void FusionNode::create_sensors() {
  using amr::infrastructure::sensors::SensorFactory;

  lidar_cfg_.type  = this->get_parameter("sensors.lidar.type").as_string();
  lidar_cfg_.topic = this->get_parameter("sensors.lidar.topic").as_string();
  lidar_  = SensorFactory::create_lidar(lidar_cfg_);

  imu_cfg_.type  = this->get_parameter("sensors.imu.type").as_string();
  imu_cfg_.topic = this->get_parameter("sensors.imu.topic").as_string();
  imu_    = SensorFactory::create_imu(imu_cfg_);

  camera_cfg_.type  = this->get_parameter("sensors.camera.type").as_string();
  camera_cfg_.topic = this->get_parameter("sensors.camera.topic").as_string();
  camera_ = SensorFactory::create_camera(camera_cfg_);
}

// ── Lifecycle callbacks ──────────────────────────────────────────────

FusionNode::CallbackReturn FusionNode::on_configure(const rclcpp_lifecycle::State &) {
  // Create sensors from YAML-driven params
  create_sensors();
  lidar_->init();
  imu_->init();
  camera_->init();

  // Coordinate transform provider (tf2_ros::Buffer + TransformListener)
  tf2_ = std::make_unique<amr::infrastructure::Tf2TransformProvider>(
      this->get_clock());

  // Wire domain layer after sensors + tf2 are ready
  perception_.emplace(*lidar_, *imu_, *camera_);
  perception_->set_transform(tf2_.get());

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
  fusion_pub_.reset();
  heartbeat_pub_.reset();
  perception_.reset();
  if (lidar_)  lidar_->shutdown();
  if (imu_)    imu_->shutdown();
  if (camera_) camera_->shutdown();
  lidar_.reset();
  imu_.reset();
  camera_.reset();
  return CallbackReturn::SUCCESS;
}

FusionNode::CallbackReturn FusionNode::on_shutdown(const rclcpp_lifecycle::State &) {
  timer_.reset();
  heartbeat_timer_.reset();
  fusion_pub_.reset();
  heartbeat_pub_.reset();
  return CallbackReturn::SUCCESS;
}

// ── Timer callback — delegates to domain layer ───────────────────────

void FusionNode::timer_callback() {
  TRACE_SCOPE(amr::trace::FUSION_TIMER);

  auto t_start = std::chrono::steady_clock::now();

  auto now = this->now();
  if (last_tick_.nanoseconds() > 0) {
    double dt = (now - last_tick_).seconds();
    if (dt > 0.001 && dt < 1.0 && perception_) {
      perception_->tick(dt);
    }
  }
  last_tick_ = now;

  if (!perception_) return;

  auto old_level = current_level_;
  current_level_ = perception_->evaluate_degradation();

  auto msg            = ros2_robot_middleware::msg::PerceptionObjects{};
  msg.header.stamp    = this->now();
  msg.header.frame_id = "base_link";

  auto clusters = perception_->fuse(current_level_);
  for (const auto &c : clusters) {
    auto obj = ros2_robot_middleware::msg::Object{};
    obj.id = c.id; obj.x = c.x; obj.y = c.y; obj.z = c.z;
    msg.objects.push_back(obj);
  }

  fusion_pub_->publish(msg);

  // ── Observability ────────────────────────────────────────────────
  auto &m = amr::observability::shared_metrics();
  m.fusion_cycle_count.fetch_add(1, std::memory_order_relaxed);
  m.object_count.store(static_cast<int32_t>(msg.objects.size()), std::memory_order_relaxed);
  m.degradation_level.store(static_cast<int32_t>(current_level_), std::memory_order_relaxed);

  if (current_level_ != old_level) {
    m.degradation_events.fetch_add(1, std::memory_order_relaxed);
    TRACE_EVENT(amr::trace::FUSION_DEGRADATION);
  }

  auto t_end = std::chrono::steady_clock::now();
  m.fusion_latency.record(
      std::chrono::duration_cast<std::chrono::microseconds>(t_end - t_start).count());

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
  if (perception_) {
    msg.data = amr::application::PerceptionService::heartbeat_for(current_level_);
  } else {
    msg.data = "inactive";
  }
  heartbeat_pub_->publish(msg);
}

FusionNode::DegradationLevel FusionNode::degradation_level() const { return current_level_; }

RCLCPP_COMPONENTS_REGISTER_NODE(FusionNode)
