#ifndef ROS2_ROBOT_MIDDLEWARE_FUSION_NODE_HPP_
#define ROS2_ROBOT_MIDDLEWARE_FUSION_NODE_HPP_

#include <cstdint>

#include "ros2_robot_middleware/application/perception_service.hpp"
#include "ros2_robot_middleware/domain/perception/degradation_policy.hpp"
#include "ros2_robot_middleware/msg/perception_objects.hpp"
#include <sensor_msgs/msg/image.hpp>
#include <sensor_msgs/msg/imu.hpp>
#include <sensor_msgs/msg/laser_scan.hpp>
#include <std_msgs/msg/string.hpp>

#include <rclcpp/rclcpp.hpp>
#include <rclcpp_lifecycle/lifecycle_node.hpp>

// Thin ROS2 adapter — delegates domain logic to PerceptionService.
//
// DDD refactoring: FusionNode is now an Infrastructure-layer adapter.
// All perception logic (KF, clustering, degradation) lives in
// amr::application::PerceptionService and amr::domain::perception::*.
// FusionNode only handles DDS pub/sub + lifecycle callbacks + timer wiring.
class FusionNode : public rclcpp_lifecycle::LifecycleNode {
public:
  FusionNode();

#ifdef AMR_TEST_HOOKS
  /// Test-only: construct with custom degradation timeouts for faster tests.
  /// NOT compiled into release builds.
  explicit FusionNode(const rclcpp::NodeOptions &options,
                      const amr::domain::perception::DegradationPolicy::Config &deg_config);
#endif

  using CallbackReturn = rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;

  CallbackReturn on_configure(const rclcpp_lifecycle::State &);
  CallbackReturn on_activate(const rclcpp_lifecycle::State &);
  CallbackReturn on_deactivate(const rclcpp_lifecycle::State &);
  CallbackReturn on_cleanup(const rclcpp_lifecycle::State &);
  CallbackReturn on_shutdown(const rclcpp_lifecycle::State &);

  // Expose degradation for tests — delegates to domain layer
  using DegradationLevel = amr::domain::perception::DegradationLevel;
  DegradationLevel degradation_level() const;

  explicit FusionNode(const rclcpp::NodeOptions &options);

private:
  void timer_callback();
  void update_heartbeat_status();

  void lidar_callback(sensor_msgs::msg::LaserScan::SharedPtr msg);
  void imu_callback(sensor_msgs::msg::Imu::SharedPtr msg);
  void camera_callback(sensor_msgs::msg::Image::SharedPtr msg);

  // Domain layer — pure C++, zero ROS2 dependency
  amr::application::PerceptionService perception_;
  amr::domain::perception::DegradationLevel current_level_{};

  // ROS2 infrastructure — DDS pub/sub
  rclcpp_lifecycle::LifecyclePublisher<ros2_robot_middleware::msg::PerceptionObjects>::SharedPtr fusion_pub_;

  rclcpp::Subscription<sensor_msgs::msg::LaserScan>::SharedPtr sub_lidar_;
  rclcpp::Subscription<sensor_msgs::msg::Imu>::SharedPtr sub_imu_;
  rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr sub_camera_;

  sensor_msgs::msg::LaserScan::SharedPtr lidar_cache_;
  sensor_msgs::msg::Imu::SharedPtr imu_cache_;
  sensor_msgs::msg::Image::SharedPtr camera_cache_;

  rclcpp::TimerBase::SharedPtr timer_;
  rclcpp::Time last_tick_;

  rclcpp_lifecycle::LifecyclePublisher<std_msgs::msg::String>::SharedPtr heartbeat_pub_;
  rclcpp::TimerBase::SharedPtr heartbeat_timer_;
};

#endif
