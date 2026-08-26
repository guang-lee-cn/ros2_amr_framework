// D1-RUN2: 第三方开发者扩展验收实验——温度消费节点。
// 订阅 /sensor/temperature（sensor_stream），滚动窗口均值输出到
// /monitor/temperature_celsius_mean（reliable_stream），超阈值节流告警。
#include "ros2_robot_middleware/infrastructure/temperature_monitor_node.hpp"

#include "ros2_robot_middleware/observability/tracer.hpp"

#include <memory>
#include <string>

TemperatureMonitorNode::TemperatureMonitorNode()
  : amr::infrastructure::AmrNode("temperature_monitor")
{
}

TemperatureMonitorNode::CallbackReturn
TemperatureMonitorNode::on_configure(const rclcpp_lifecycle::State &)
{
  // 参数在 on_configure 声明（CLAUDE.md「节点形态」）
  const std::string topic = this->declare_parameter<std::string>("input_topic", "/sensor/temperature");
  window_size_            = this->declare_parameter<int>("window_size", 10);
  threshold_c_            = this->declare_parameter<double>("threshold_c", 60.0);

  subscription_ = this->create_subscription<sensor_msgs::msg::Temperature>(
      topic, amr::qos::sensor_stream(),
      [this](sensor_msgs::msg::Temperature::ConstSharedPtr msg) { on_temperature(msg); });

  publisher_ = create_pub<std_msgs::msg::Float64>("/monitor/temperature_celsius_mean",
                                                   amr::qos::reliable_stream());

  return CallbackReturn::SUCCESS;
}

TemperatureMonitorNode::CallbackReturn
TemperatureMonitorNode::on_activate(const rclcpp_lifecycle::State &)
{
  start_heartbeat("/monitor/temperature/heartbeat");
  publisher_->on_activate();

  return CallbackReturn::SUCCESS;
}

TemperatureMonitorNode::CallbackReturn
TemperatureMonitorNode::on_deactivate(const rclcpp_lifecycle::State &)
{
  stop_heartbeat();

  publisher_->on_deactivate();

  return CallbackReturn::SUCCESS;
}

TemperatureMonitorNode::CallbackReturn
TemperatureMonitorNode::on_cleanup(const rclcpp_lifecycle::State &)
{
  subscription_.reset();
  publisher_.reset();
  window_.clear();

  return CallbackReturn::SUCCESS;
}

TemperatureMonitorNode::CallbackReturn
TemperatureMonitorNode::on_shutdown(const rclcpp_lifecycle::State &)
{
  stop_heartbeat();
  subscription_.reset();
  publisher_.reset();

  return CallbackReturn::SUCCESS;
}

void TemperatureMonitorNode::on_temperature(sensor_msgs::msg::Temperature::ConstSharedPtr msg)
{
  TRACE_SCOPE("temperature_monitor::on_temperature");

  // 消费端数据门控：stamp==0 = 未盖章，保守拒绝（CLAUDE.md 时间戳红线）
  if (msg->header.stamp.sec == 0 && msg->header.stamp.nanosec == 0u) {
    RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 5000,
                         "temperature_monitor: unstamped sample rejected");
    return;
  }

  window_.push_back(msg->temperature);
  while (static_cast<int>(window_.size()) > window_size_) window_.pop_front();

  const double mean = window_mean();

  auto out    = std_msgs::msg::Float64{};
  out.data    = mean;
  publisher_->publish(out);

  if (mean > threshold_c_) {
    RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 5000,
                         "temperature_monitor: OVERHEAT mean=%.2f C > threshold=%.2f C",
                         mean, threshold_c_);
  }
}

double TemperatureMonitorNode::window_mean() const
{
  if (window_.empty()) return 0.0;
  double sum = 0.0;
  for (const float v : window_) sum += v;
  return sum / static_cast<double>(window_.size());
}
