#include "ros2_robot_middleware/imu_node.hpp"
#include "ros2_robot_middleware/observability/metrics_registry.hpp"
#include "ros2_robot_middleware/observability/tracer.hpp"

#include <chrono>

ImuNode::ImuNode()
  : rclcpp_lifecycle::LifecycleNode("imu")
{
}

ImuNode::CallbackReturn
ImuNode::on_configure(const rclcpp_lifecycle::State &)
{
  publisher_ = this->create_publisher<sensor_msgs::msg::Imu>(
    "/sensor/imu", rclcpp::QoS(10).reliable());

  heartbeat_pub_ = this->create_publisher<std_msgs::msg::String>(
    "/sensor/imu/heartbeat", rclcpp::QoS(10).reliable());

  return CallbackReturn::SUCCESS;
}

ImuNode::CallbackReturn
ImuNode::on_activate(const rclcpp_lifecycle::State &)
{
  using namespace std::chrono_literals;
  timer_ = this->create_wall_timer(10ms, [this]() { timer_callback(); });

  heartbeat_timer_ = this->create_wall_timer(1s, [this]() {
    auto msg = std_msgs::msg::String{};
    msg.data = "alive";
    heartbeat_pub_->publish(msg);
  });

  publisher_->on_activate();
  heartbeat_pub_->on_activate();

  return CallbackReturn::SUCCESS;
}

ImuNode::CallbackReturn
ImuNode::on_deactivate(const rclcpp_lifecycle::State &)
{
  timer_.reset();
  heartbeat_timer_.reset();

  publisher_->on_deactivate();
  heartbeat_pub_->on_deactivate();

  return CallbackReturn::SUCCESS;
}

ImuNode::CallbackReturn
ImuNode::on_cleanup(const rclcpp_lifecycle::State &)
{
  publisher_.reset();
  heartbeat_pub_.reset();

  return CallbackReturn::SUCCESS;
}

ImuNode::CallbackReturn
ImuNode::on_shutdown(const rclcpp_lifecycle::State &)
{
  timer_.reset();
  heartbeat_timer_.reset();
  publisher_.reset();
  heartbeat_pub_.reset();

  return CallbackReturn::SUCCESS;
}

void ImuNode::timer_callback()
{
  TRACE_SCOPE("imu::timer_callback");
  auto t_start = std::chrono::steady_clock::now();

  auto msg = sensor_msgs::msg::Imu{};

  msg.header.stamp    = this->now();
  msg.header.frame_id = "imu_link";

  msg.orientation.w = 1.0;

  msg.angular_velocity.x = (rand() % 2001 - 1000) / 50000.0;
  msg.angular_velocity.y = (rand() % 2002 - 1000) / 50000.0;
  msg.angular_velocity.z = (rand() % 2003 - 1000) / 50000.0;

  msg.linear_acceleration.x = (rand() % 2001 - 1000) / 10000.0;
  msg.linear_acceleration.y = (rand() % 2002 - 1000) / 10000.0;
  msg.linear_acceleration.z = (rand() % 2003 - 1000) / 10000.0;

  publisher_->publish(msg);

  // Observability: rate + e2e timestamp
  static auto last_ts = t_start;
  auto dt_us = std::chrono::duration_cast<std::chrono::microseconds>(
                   t_start - last_ts).count();
  auto &m = amr::observability::shared_metrics();
  if (dt_us > 0) {
    m.imu_rate_ds.store(static_cast<int32_t>(10'000'000 / dt_us),
                        std::memory_order_relaxed);
  }
  last_ts = t_start;
  m.last_sensor_timestamp_ns.store(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          t_start.time_since_epoch()).count(),
      std::memory_order_relaxed);

  RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 5000,
                       "IMU published: ω=(%.3f,%.3f,%.3f) rad/s  a=(%.3f,%.3f,%.3f) m/s²",
                       msg.angular_velocity.x, msg.angular_velocity.y, msg.angular_velocity.z,
                       msg.linear_acceleration.x, msg.linear_acceleration.y, msg.linear_acceleration.z);
}
