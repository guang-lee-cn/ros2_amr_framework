#include "ros2_robot_middleware/camera_node.hpp"
#include "ros2_robot_middleware/observability/metrics_registry.hpp"
#include "ros2_robot_middleware/observability/tracer.hpp"

#include <chrono>

CameraNode::CameraNode()
  : rclcpp_lifecycle::LifecycleNode("camera")
{
}

CameraNode::CallbackReturn
CameraNode::on_configure(const rclcpp_lifecycle::State &)
{
  publisher_ = this->create_publisher<sensor_msgs::msg::Image>(
    "/sensor/camera", rclcpp::QoS(10).best_effort());

  heartbeat_pub_ = this->create_publisher<std_msgs::msg::String>(
    "/sensor/camera/heartbeat", rclcpp::QoS(10).reliable());

  return CallbackReturn::SUCCESS;
}

CameraNode::CallbackReturn
CameraNode::on_activate(const rclcpp_lifecycle::State &)
{
  using namespace std::chrono_literals;
  timer_ = this->create_wall_timer(200ms, [this]() { timer_callback(); });

  heartbeat_timer_ = this->create_wall_timer(1s, [this]() {
    auto msg = std_msgs::msg::String{};
    msg.data = "alive";
    heartbeat_pub_->publish(msg);
  });

  publisher_->on_activate();
  heartbeat_pub_->on_activate();

  return CallbackReturn::SUCCESS;
}

CameraNode::CallbackReturn
CameraNode::on_deactivate(const rclcpp_lifecycle::State &)
{
  timer_.reset();
  heartbeat_timer_.reset();

  publisher_->on_deactivate();
  heartbeat_pub_->on_deactivate();

  return CallbackReturn::SUCCESS;
}

CameraNode::CallbackReturn
CameraNode::on_cleanup(const rclcpp_lifecycle::State &)
{
  publisher_.reset();
  heartbeat_pub_.reset();

  return CallbackReturn::SUCCESS;
}

CameraNode::CallbackReturn
CameraNode::on_shutdown(const rclcpp_lifecycle::State &)
{
  timer_.reset();
  heartbeat_timer_.reset();
  publisher_.reset();
  heartbeat_pub_.reset();

  return CallbackReturn::SUCCESS;
}

void CameraNode::timer_callback()
{
  TRACE_SCOPE("camera::timer_callback");
  auto t_start = std::chrono::steady_clock::now();

  auto msg = sensor_msgs::msg::Image{};

  msg.header.stamp    = this->now();
  msg.header.frame_id = "camera_link";

  msg.height       = 480;
  msg.width        = 640;
  msg.step         = 640 * 3;
  msg.encoding     = "rgb8";
  msg.is_bigendian = 0;
  msg.data.resize(480 * 640 * 3);
  for (auto &byte : msg.data) {
    byte = rand() % 256;
  }

  publisher_->publish(msg);

  // Observability: rate tracking
  static auto last_ts = t_start;
  auto dt_us = std::chrono::duration_cast<std::chrono::microseconds>(
                   t_start - last_ts).count();
  if (dt_us > 0) {
    amr::observability::shared_metrics().camera_rate_ds.store(
        static_cast<int32_t>(10'000'000 / dt_us), std::memory_order_relaxed);
  }
  last_ts = t_start;

  RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 10000,
                       "Image published: %dx%d %s %.1fKB",
                       msg.width, msg.height, msg.encoding.c_str(),
                       msg.data.size() / 1024.0);
}
