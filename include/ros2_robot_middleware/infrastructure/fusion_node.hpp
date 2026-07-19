#ifndef ROS2_ROBOT_MIDDLEWARE_FUSION_NODE_HPP_
#define ROS2_ROBOT_MIDDLEWARE_FUSION_NODE_HPP_

#include <cstdint>
#include <memory>
#include <optional>

#include "ros2_robot_middleware/application/perception_service.hpp"
#include "ros2_robot_middleware/domain/perception/degradation_policy.hpp"
#include "ros2_robot_middleware/domain/perception/sensor_interface.hpp"
#include "ros2_robot_middleware/infrastructure/sensors/sensor_factory.hpp"
#include "ros2_robot_middleware/infrastructure/tf2_transform_provider.hpp"
#include "ros2_robot_middleware/msg/perception_objects.hpp"
#include <std_msgs/msg/string.hpp>

#include <rclcpp/rclcpp.hpp>
#include <rclcpp_lifecycle/lifecycle_node.hpp>

// Thin ROS2 adapter — delegates domain logic to PerceptionService.
//
// Sensors are created in on_configure() via SensorFactory, which reads
// sensor type from ROS2 parameters (populated from config/sensors.yaml).
//
// Switch simulated→real without recompilation:
//   config/sensors.yaml:  lidar.type = simulated → sick_tim781

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
  void declare_sensor_parameters();
  void create_sensors();
  void timer_callback();
  void update_heartbeat_status();

  // Sensor parameters (from config/sensors.yaml via ROS2 params)
  amr::infrastructure::sensors::SensorConfig lidar_cfg_;
  amr::infrastructure::sensors::SensorConfig imu_cfg_;
  amr::infrastructure::sensors::SensorConfig camera_cfg_;

  // HAL sensors — created by SensorFactory in on_configure()
  std::unique_ptr<amr::domain::sensor::ISensor<amr::domain::sensor::LidarScan>>   lidar_;
  std::unique_ptr<amr::domain::sensor::ISensor<amr::domain::sensor::ImuData>>     imu_;
  std::unique_ptr<amr::domain::sensor::ISensor<amr::domain::sensor::CameraFrame>> camera_;

  // Coordinate transform (needs ROS2 node → created in on_configure)
  std::unique_ptr<amr::infrastructure::Tf2TransformProvider> tf2_;

  // Domain layer — created after sensors + tf2 are initialized
  std::optional<amr::application::PerceptionService> perception_;

  amr::domain::perception::DegradationLevel current_level_{};

  // ROS2 infrastructure — DDS pub/sub
  rclcpp_lifecycle::LifecyclePublisher<ros2_robot_middleware::msg::PerceptionObjects>::SharedPtr fusion_pub_;
  rclcpp_lifecycle::LifecyclePublisher<std_msgs::msg::String>::SharedPtr heartbeat_pub_;
  rclcpp::TimerBase::SharedPtr timer_;
  rclcpp::TimerBase::SharedPtr heartbeat_timer_;
  rclcpp::Time last_tick_;
};

#endif
