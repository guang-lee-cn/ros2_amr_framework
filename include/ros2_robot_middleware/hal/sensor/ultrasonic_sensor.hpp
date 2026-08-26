#ifndef ROS2_ROBOT_MIDDLEWARE_HAL_SENSOR_ULTRASONIC_SENSOR_HPP_
#define ROS2_ROBOT_MIDDLEWARE_HAL_SENSOR_ULTRASONIC_SENSOR_HPP_

/// @file   ultrasonic_sensor.hpp
/// @brief  模拟超声波测距传感器（D1 验收实验新增）。
///
/// 按文档 doc/guides/11-driver-integration.md 的扩展方式实现：
///   - 数据类型 UltrasonicData（零 ROS2 依赖，stamp_ns==0 = 未盖章）
///   - CRTP 基类 amr::hal::sensor::SensorBase<Derived, DataType>
///   - 注册宏 AMR_REGISTER_SENSOR("ultrasonic", "simulated", ...) 位于
///     src/hal/ultrasonic_sensor.cpp（文档要求注册放 adapter cpp）

// D1 验收实验产物（2026-08-26，19.5min 接入实证）转正为官方扩展范本：
// 见 docs/design/20260826-d1-extension-acceptance.md 与 ultrasonic_sensor.hpp 注册路径示例。
#include "ros2_robot_middleware/hal/common/registry.hpp"
#include "ros2_robot_middleware/hal/sensor/isensor.hpp"

#include <algorithm>
#include <cmath>

namespace amr::hal::sensor {

/// 超声波单点测距数据（单位：米）
struct UltrasonicData {
    float range_m      = 0.0F;
    float min_range_m  = 0.20F;
    float max_range_m  = 4.00F;
    /// 测量时刻（纳秒）。0 = 未盖章：合成路径由 infra 在读取边界补戳
    /// （打戳规则见 isensor.hpp 头注释引用的 timestamp-policy ADR）。
    int64_t stamp_ns   = 0;
};

/// 模拟超声波：围绕一个可配置的障碍距离生成正弦扰动 + 小噪声。
/// 模型对齐 SimulatedLidar/Imu 的「timer/读取时合成」风格。
class SimulatedUltrasonic : public SensorBase<SimulatedUltrasonic, UltrasonicData> {
public:
    SimulatedUltrasonic() = default;

    /// 直接指定障碍距离（测试/场景注入用）
    explicit SimulatedUltrasonic(float obstacle_distance_m)
        : obstacle_m_(obstacle_distance_m) {}

    bool init_impl() {
        // 模拟传感器无硬件资源；健康度 0 = OK（约定见 subsystems/sensor-pipeline.md）
        this->health_ = 0;
        return true;
    }

    bool read_impl(UltrasonicData &out) {
        const float wobble = 0.05F * std::sin(static_cast<float>(seq_) * 0.10F);
        const float noise  = static_cast<float>(std::rand() % 11 - 5) * 0.002F;
        float r = obstacle_m_ + wobble + noise;
        r = std::max(0.0F, std::min(r, 100.0F));

        out.range_m     = r;
        out.min_range_m = 0.20F;
        out.max_range_m = 4.00F;
        out.stamp_ns    = 0;  // 合成数据未盖章，消费端（infra）在边界补 now_ns()
        ++seq_;
        return true;
    }

    void shutdown_impl() {
        // 无资源需要释放
    }

    /// 测试钩子：改写模拟障碍物距离
    void set_simulated_obstacle(float obstacle_distance_m) {
        obstacle_m_ = obstacle_distance_m;
    }

private:
    float    obstacle_m_ = 2.00F;
    uint64_t seq_        = 0;
};

/// 幂等注册：把 "ultrasonic"/"simulated" 编入 SensorRegistry。
///
/// 说明：src/hal/ultrasonic_sensor.cpp 中的静态注册对象会因静态库无引用
/// 被链接器丢弃（实测 2026-08-26 D1 实验）。按框架内置传感器的同构模式
/// （register_builtin_sensors），消费方在首次创建前显式调用本函数。
inline void ensure_ultrasonic_registered() {
    static bool done = false;
    if (done) return;
    done = true;
    ::amr::hal::common::SensorRegistry::instance().register_type(
        "ultrasonic", "simulated",
        []() -> void * { return new SimulatedUltrasonic(); });
}

} // namespace amr::hal::sensor

#endif  // ROS2_ROBOT_MIDDLEWARE_HAL_SENSOR_ULTRASONIC_SENSOR_HPP_
