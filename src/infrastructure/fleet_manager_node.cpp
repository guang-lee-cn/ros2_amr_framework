#include "ros2_robot_middleware/infrastructure/fleet_manager_node.hpp"

#include <sstream>

FleetManagerNode::FleetManagerNode()
  : rclcpp_lifecycle::LifecycleNode("fleet_manager")
{
}

FleetManagerNode::CallbackReturn
FleetManagerNode::on_configure(const rclcpp_lifecycle::State &)
{
  for (size_t i = 0; i < kMaxAmrs; ++i) {
    subs_[i] = this->create_subscription<ros2_robot_middleware::msg::HealthReport>(
      kAmrs[i].health_topic,
      rclcpp::QoS(10).reliable(),
      [this, i](ros2_robot_middleware::msg::HealthReport::SharedPtr msg) {
        health_callback(i, msg);
      });
  }

  fleet_status_pub_ = this->create_publisher<std_msgs::msg::String>(
    "/fleet/status", rclcpp::QoS(10).reliable());

  fleet_health_pub_ = this->create_publisher<ros2_robot_middleware::msg::HealthReport>(
    "/fleet/health", rclcpp::QoS(10).reliable());

  heartbeat_pub_ = this->create_publisher<std_msgs::msg::String>(
    "/fleet/heartbeat", rclcpp::QoS(10).reliable());

  RCLCPP_INFO(this->get_logger(), "FleetManager configured: %d AMRs monitored", kMaxAmrs);

  return CallbackReturn::SUCCESS;
}

FleetManagerNode::CallbackReturn
FleetManagerNode::on_activate(const rclcpp_lifecycle::State &)
{
  using namespace std::chrono_literals;

  status_timer_ = this->create_wall_timer(
    std::chrono::seconds(kStatusIntervalS),
    [this]() { fleet_status_timer(); });

  heartbeat_timer_ = this->create_wall_timer(1s, [this]() { heartbeat_timer(); });

  fleet_status_pub_->on_activate();
  fleet_health_pub_->on_activate();
  heartbeat_pub_->on_activate();

  RCLCPP_INFO(this->get_logger(), "FleetManager activated: status every %ds", kStatusIntervalS);

  return CallbackReturn::SUCCESS;
}

FleetManagerNode::CallbackReturn
FleetManagerNode::on_deactivate(const rclcpp_lifecycle::State &)
{
  status_timer_.reset();
  heartbeat_timer_.reset();

  fleet_status_pub_->on_deactivate();
  fleet_health_pub_->on_deactivate();
  heartbeat_pub_->on_deactivate();

  return CallbackReturn::SUCCESS;
}

FleetManagerNode::CallbackReturn
FleetManagerNode::on_cleanup(const rclcpp_lifecycle::State &)
{
  for (size_t i = 0; i < kMaxAmrs; ++i) subs_[i].reset();
  fleet_status_pub_.reset();
  fleet_health_pub_.reset();
  heartbeat_pub_.reset();

  return CallbackReturn::SUCCESS;
}

FleetManagerNode::CallbackReturn
FleetManagerNode::on_shutdown(const rclcpp_lifecycle::State &)
{
  status_timer_.reset();
  heartbeat_timer_.reset();
  for (size_t i = 0; i < kMaxAmrs; ++i) subs_[i].reset();
  fleet_status_pub_.reset();
  fleet_health_pub_.reset();
  heartbeat_pub_.reset();

  return CallbackReturn::SUCCESS;
}

void FleetManagerNode::health_callback(
  size_t amr_index,
  ros2_robot_middleware::msg::HealthReport::SharedPtr report)
{
  last_reports_[amr_index] = report;
}

void FleetManagerNode::fleet_status_timer()
{
  // 汇聚所有 AMR 健康状态 → 集群级别 HealthReport
  auto fleet_report = ros2_robot_middleware::msg::HealthReport{};
  fleet_report.header.stamp    = this->now();
  fleet_report.header.frame_id = "fleet_manager";

  int ok_count       = 0;
  int warn_count     = 0;
  int error_count    = 0;
  int stale_count    = 0;

  for (size_t i = 0; i < kMaxAmrs; ++i) {
    if (!last_reports_[i]) {
      stale_count++;
      continue;
    }
    for (const auto &node_status : last_reports_[i]->nodes) {
      if (node_status.status == "OK")      ok_count++;
      else if (node_status.status == "WARN")    warn_count++;
      else if (node_status.status == "ERROR")   error_count++;
      else if (node_status.status == "FATAL")   error_count++;
      else stale_count++;
    }

    // 聚合所有 AMR 的节点状态
    for (const auto &ns : last_reports_[i]->nodes) {
      auto s = ns;
      s.node_name = std::string(kAmrs[i].name) + "/" + ns.node_name;
      fleet_report.nodes.push_back(s);
    }
  }

  fleet_health_pub_->publish(fleet_report);

  // 发布可读的集群状态字符串
  std::ostringstream status;
  status << "cluster: " << kMaxAmrs << " AMRs, "
         << ok_count << " OK, " << warn_count << " WARN, "
         << error_count << " ERR, " << stale_count << " STALE";

  auto msg = std_msgs::msg::String{};
  msg.data = status.str();
  fleet_status_pub_->publish(msg);

  RCLCPP_INFO_THROTTLE(this->get_logger(), *this->get_clock(), 10000,
                       "%s", status.str().c_str());
}

void FleetManagerNode::heartbeat_timer()
{
  auto msg  = std_msgs::msg::String{};
  msg.data  = "alive";
  heartbeat_pub_->publish(msg);
}
