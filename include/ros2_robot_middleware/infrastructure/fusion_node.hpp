#ifndef ROS2_ROBOT_MIDDLEWARE_FUSION_NODE_HPP_
#define ROS2_ROBOT_MIDDLEWARE_FUSION_NODE_HPP_

#include <cstdint>

#include "ros2_robot_middleware/application/perception_service.hpp"
#include "ros2_robot_middleware/domain/perception/degradation_policy.hpp"
#include "ros2_robot_middleware/infrastructure/sensors/simulated_sensors.hpp"
#include "ros2_robot_middleware/msg/perception_objects.hpp"
#include <std_msgs/msg/string.hpp>

#include <rclcpp/rclcpp.hpp>
#include <rclcpp_lifecycle/lifecycle_node.hpp>

// Thin ROS2 adapter — delegates domain logic to PerceptionService.
//
// Sensors are accessed via HAL (SimulatedLidar/Imu/Camera).
// PerceptionService reads sensors directly in tick() — no ROS2
// callback wiring needed for sensor data.

class FusionNode : public rclcpp_lifecycle::LifecycleNode {
public:
  FusionNode();

  using CallbackReturn = rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;

  CallbackReturn on_configure(const rclcpp_lifecycle::State &);
  CallbackReturn on_activate(const rclcpp_lifecycle::State &);
  CallbackReturn on_deactivate(const rclcpp_lifecycle::State &);
  CallbackReturn on_cleanup(const rclcpp_lifecycle::State &);
  CallbackReturn on_shutdown(const rclcpp_lifecycle::State &);

  using DegradationLevel = amr::domain::perception::DegradationLevel;
  DegradationLevel degradation_level() const;

  explicit FusionNode(const rclcpp::NodeOptions &options);

  /// Construct with custom degradation timeouts (test hook)
  explicit FusionNode(const rclcpp::NodeOptions &options,
                      const amr::domain::perception::DegradationPolicy::Config &deg_config);

private:
  void timer_callback();
  void update_heartbeat_status();

  // HAL sensors (simulated — replace with real adapters for hardware)
  amr::infrastructure::sensors::SimulatedLidar   lidar_;
  amr::infrastructure::sensors::SimulatedImu     imu_;
  amr::infrastructure::sensors::SimulatedCamera  camera_;

  // Domain layer — sensors injected, no templates
  amr::application::PerceptionService perception_{lidar_, imu_, camera_};

  amr::domain::perception::DegradationLevel current_level_{};

  // ROS2 infrastructure — DDS pub/sub
  rclcpp_lifecycle::LifecyclePublisher<ros2_robot_middleware::msg::PerceptionObjects>::SharedPtr fusion_pub_;
  rclcpp_lifecycle::LifecyclePublisher<std_msgs::msg::String>::SharedPtr heartbeat_pub_;
  rclcpp::TimerBase::SharedPtr timer_;
  rclcpp::TimerBase::SharedPtr heartbeat_timer_;
  rclcpp::Time last_tick_;
};

#endif
