#pragma once
/// @file   sensor_factory.hpp
/// @brief  Registry-driven sensor creation — plugin registration replaces if-else.
///
/// Built-in adapters self-register via AMR_REGISTER_SENSOR. Factory looks up
/// by category+type in SensorRegistry. Adding a new sensor = one macro in a
/// .cpp + config change — no factory edit.
///
/// Usage in FusionNode:
///   auto lidar  = SensorFactory::create_lidar(cfg);
///   auto imu    = SensorFactory::create_imu(cfg);
///   auto camera = SensorFactory::create_camera(cfg);

#include "ros2_robot_middleware/hal/common/registry.hpp"
#include "ros2_robot_middleware/hal/sensor/isensor.hpp"
#include "ros2_robot_middleware/hal/sensor/sick_tim781_adapter.hpp"
#include "ros2_robot_middleware/hal/sensor/simulated_sensors.hpp"

#include <memory>
#include <string>

namespace amr::hal::sensor {

using amr::hal::common::SensorRegistry;

struct SensorConfig {
    std::string type;    // "simulated", "sick_tim781", etc.
    std::string topic;   // only used for real sensors
};

// ── Built-in registration ─────────────────────────────────────────────
// Runs once at static-init; registers all known adapters in this package.
inline void register_builtin_sensors() {
    static bool done = false;
    if (done) return;
    done = true;
    SensorRegistry::instance().register_type("lidar", "simulated",
        []() { return static_cast<void *>(new SimulatedLidar()); });
    SensorRegistry::instance().register_type("lidar", "sick_tim781",
        []() { return static_cast<void *>(new SickTiM781Adapter("/scan")); });
    SensorRegistry::instance().register_type("imu", "simulated",
        []() { return static_cast<void *>(new SimulatedImu()); });
    SensorRegistry::instance().register_type("camera", "simulated",
        []() { return static_cast<void *>(new SimulatedCamera()); });
}

// ── Typed helpers — registry returns void*, cast to ISensor<T> ─────────
template <typename DataType>
std::unique_ptr<ISensor<DataType>> make_sensor(const std::string &category,
                                                const std::string &type) {
    void *raw = SensorRegistry::instance().create(category, type);
    if (!raw) return nullptr;
    return std::unique_ptr<ISensor<DataType>>(
        static_cast<ISensor<DataType> *>(raw));
}

class SensorFactory {
public:
    using LidarPtr  = std::unique_ptr<ISensor<LidarScan>>;
    using ImuPtr    = std::unique_ptr<ISensor<ImuData>>;
    using CameraPtr = std::unique_ptr<ISensor<CameraFrame>>;

    // fail-fast（2026-08-25 审计 P1-c）：未注册类型返回 nullptr，调用方必须
    // 检查并拒绝启动。旧版静默 fallback 到仿真传感器 = 配置拼错时机器人
    // 在产线"看见"假数据——那是最危险的失败模式。
    static LidarPtr create_lidar(const SensorConfig &cfg) {
        register_builtin_sensors();
        return make_sensor<LidarScan>("lidar", cfg.type);
    }

    /// Create simulated LiDAR with a scenario (obstacle layout) — for demo
    /// scenarios where the robot must navigate around known obstacles.
    static LidarPtr create_lidar(const SensorConfig &cfg, const Scenario &scenario) {
        if (cfg.type == "simulated") {
            return std::make_unique<SimulatedLidar>(scenario);
        }
        return create_lidar(cfg);  // real adapter ignores scenario
    }

    static ImuPtr create_imu(const SensorConfig &cfg) {
        register_builtin_sensors();
        return make_sensor<ImuData>("imu", cfg.type);
    }

    static CameraPtr create_camera(const SensorConfig &cfg) {
        register_builtin_sensors();
        return make_sensor<CameraFrame>("camera", cfg.type);
    }

    /// Create simulated camera with a scenario — generates FOV depth from the
    /// same obstacle layout as the lidar (low-obstacle blind-spot detection).
    static CameraPtr create_camera(const SensorConfig &cfg, const Scenario &scenario) {
        if (cfg.type == "simulated") {
            return std::make_unique<SimulatedCamera>(scenario);
        }
        return create_camera(cfg);  // real adapter ignores scenario
    }
};

} // namespace amr::hal::sensor
