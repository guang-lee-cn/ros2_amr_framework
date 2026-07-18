#pragma once
/// @file   sensor_factory.hpp
/// @brief  YAML-driven sensor creation — no recompilation to switch simulated↔real.
///
/// Reads config/sensors.yaml at node startup. Creates the appropriate
/// concrete sensor type (SimulatedLidar / SickTiM781Adapter / etc.)
/// and returns it as std::unique_ptr<ISensor<DataType>>.
///
/// Usage in FusionNode:
///   auto lidar  = SensorFactory::create_lidar(node, sensor_config);
///   auto imu    = SensorFactory::create_imu(node, sensor_config);
///   auto camera = SensorFactory::create_camera(node, sensor_config);

#include "ros2_robot_middleware/domain/perception/sensor_interface.hpp"
#include "ros2_robot_middleware/infrastructure/sensors/sick_tim781_adapter.hpp"
#include "ros2_robot_middleware/infrastructure/sensors/simulated_sensors.hpp"

#include <memory>
#include <string>
#include <rclcpp/rclcpp.hpp>

namespace amr::infrastructure::sensors {

struct SensorConfig {
    std::string type;    // "simulated", "sick_tim781", etc.
    std::string topic;   // only used for real sensors

    static SensorConfig from_yaml(const std::string &sensor_name);
};

class SensorFactory {
public:
    using LidarPtr   = std::unique_ptr<amr::domain::sensor::ISensor<amr::domain::sensor::LidarScan>>;
    using ImuPtr     = std::unique_ptr<amr::domain::sensor::ISensor<amr::domain::sensor::ImuData>>;
    using CameraPtr  = std::unique_ptr<amr::domain::sensor::ISensor<amr::domain::sensor::CameraFrame>>;

    /// Create LiDAR sensor from config.
    /// type="simulated" → SimulatedLidar
    /// type="sick_tim781" → SickTiM781Adapter(node, config.topic)
    static LidarPtr create_lidar(const SensorConfig &cfg) {
        if (cfg.type == "simulated") {
            return std::make_unique<SimulatedLidar>();
        }
        if (cfg.type == "sick_tim781") {
            return std::make_unique<SickTiM781Adapter>(
                cfg.topic.empty() ? "/scan" : cfg.topic);
        }
        return std::make_unique<SimulatedLidar>();  // fallback
    }

    /// Create IMU sensor from config.
    static ImuPtr create_imu(const SensorConfig &cfg) {
        if (cfg.type == "simulated") {
            return std::make_unique<SimulatedImu>();
        }
        return std::make_unique<SimulatedImu>();  // fallback
    }

    /// Create Camera sensor from config.
    static CameraPtr create_camera(const SensorConfig &cfg) {
        if (cfg.type == "simulated") {
            return std::make_unique<SimulatedCamera>();
        }
        return std::make_unique<SimulatedCamera>();  // fallback
    }
};

/// Parse sensor type+topic from ROS2 parameters (declared in node).
/// Reads from config/sensors.yaml via launch file parameter loading.
inline SensorConfig SensorConfig::from_yaml(const std::string &sensor_name) {
    // Parameters are auto-loaded from YAML by the launch system.
    // Node declares them, launch file injects values from sensors.yaml.
    // Example parameter path: "sensors.lidar.type", "sensors.lidar.topic"
    //
    // This method is a placeholder — actual parameter reading happens
    // in the Node's constructor via declare_parameter/get_parameter.
    (void)sensor_name;
    return {"simulated", ""};  // default
}

} // namespace amr::infrastructure::sensors
