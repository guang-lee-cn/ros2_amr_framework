#include "ros2_robot_middleware/lidar_node.hpp"
#include "ros2_robot_middleware/observability/metrics_registry.hpp"
#include "ros2_robot_middleware/observability/tracer.hpp"

#include <chrono>
#include <cmath>

LidarNode::LidarNode()
  : rclcpp_lifecycle::LifecycleNode("lidar")
{
}

LidarNode::CallbackReturn
LidarNode::on_configure(const rclcpp_lifecycle::State &)
{
  publisher_ = this->create_publisher<sensor_msgs::msg::LaserScan>(
    "/sensor/lidar", rclcpp::QoS(10).best_effort());

  heartbeat_pub_ = this->create_publisher<std_msgs::msg::String>(
    "/sensor/lidar/heartbeat", rclcpp::QoS(10).reliable());

  return CallbackReturn::SUCCESS;
}

LidarNode::CallbackReturn
LidarNode::on_activate(const rclcpp_lifecycle::State &)
{
  using namespace std::chrono_literals;
  timer_ = this->create_wall_timer(100ms, [this]() { timer_callback(); });

  heartbeat_timer_ = this->create_wall_timer(1s, [this]() {
    auto msg = std_msgs::msg::String{};
    msg.data = "alive";
    heartbeat_pub_->publish(msg);
  });

  publisher_->on_activate();
  heartbeat_pub_->on_activate();

  return CallbackReturn::SUCCESS;
}

LidarNode::CallbackReturn
LidarNode::on_deactivate(const rclcpp_lifecycle::State &)
{
  timer_.reset();
  heartbeat_timer_.reset();

  publisher_->on_deactivate();
  heartbeat_pub_->on_deactivate();

  return CallbackReturn::SUCCESS;
}

LidarNode::CallbackReturn
LidarNode::on_cleanup(const rclcpp_lifecycle::State &)
{
  publisher_.reset();
  heartbeat_pub_.reset();

  return CallbackReturn::SUCCESS;
}

LidarNode::CallbackReturn
LidarNode::on_shutdown(const rclcpp_lifecycle::State &)
{
  timer_.reset();
  heartbeat_timer_.reset();
  publisher_.reset();
  heartbeat_pub_.reset();

  return CallbackReturn::SUCCESS;
}

void LidarNode::timer_callback()
{
  TRACE_SCOPE("lidar::timer_callback");
  auto t_start = std::chrono::steady_clock::now();

  auto msg = sensor_msgs::msg::LaserScan{};

  msg.header.stamp    = this->now();
  msg.header.frame_id = "lidar_frame";

  constexpr int    kNumPoints      = 360;
  constexpr float  kAngleMin       = -M_PI;
  constexpr float  kAngleMax       = M_PI;
  constexpr float  kAngleIncrement = (kAngleMax - kAngleMin) / kNumPoints;
  constexpr float  kTimeIncrement  = 0.0001F;

  msg.angle_min       = kAngleMin;
  msg.angle_max       = kAngleMax;
  msg.angle_increment = kAngleIncrement;
  msg.time_increment  = kTimeIncrement;
  msg.scan_time       = 0.1F;    // 10Hz scan
  msg.range_min       = 0.1F;    // SICK TiM781 min range
  msg.range_max       = 6.5F;    // SICK TiM781 max range

  msg.ranges.resize(kNumPoints);
  msg.intensities.resize(kNumPoints);

  for (int i = 0; i < kNumPoints; ++i) {
    float angle = kAngleMin + i * kAngleIncrement;
    msg.ranges[i] = 2.0F + 1.5F * std::sin(angle * 3.0F) * std::cos(angle * 2.0F);
    msg.intensities[i] = 1.0F - msg.ranges[i] / 10.0F;
  }

  publisher_->publish(msg);

  // Observability: latency + rate tracking
  auto t_end = std::chrono::steady_clock::now();
  auto lat_us = std::chrono::duration_cast<std::chrono::microseconds>(
                    t_end - t_start).count();
  auto &m = amr::observability::shared_metrics();
  // Compute rate as 1/dt — simplified, using last interval
  static auto last_ts = t_start;
  auto dt_us = std::chrono::duration_cast<std::chrono::microseconds>(
                   t_start - last_ts).count();
  if (dt_us > 0) {
    m.lidar_rate_ds.store(static_cast<int32_t>(10'000'000 / dt_us),
                          std::memory_order_relaxed);
  }
  last_ts = t_start;
  (void)lat_us; // reserved for future per-sensor histogram

  RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 5000,
                       "LaserScan published: ranges[0..359] avg=%.2fm",
                       msg.ranges[0]);
}
