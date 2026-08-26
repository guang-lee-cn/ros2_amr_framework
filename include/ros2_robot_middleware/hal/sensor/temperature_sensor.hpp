#ifndef ROS2_ROBOT_MIDDLEWARE_HAL_SENSOR_TEMPERATURE_SENSOR_HPP_
#define ROS2_ROBOT_MIDDLEWARE_HAL_SENSOR_TEMPERATURE_SENSOR_HPP_

// D1-RUN2: 第三方开发者扩展验收实验——模拟温度传感器（照 ultrasonic 范本接入）。
/// @file   temperature_sensor.hpp
/// @brief  模拟温度传感器（电机舱/电池舱环境监测用）。
///
/// 按 doc/guides/11-driver-integration.md + CLAUDE.md 的扩展方式实现：
///   - 数据类型 TemperatureData（零 ROS2 依赖，stamp_ns==0 = 未盖章）
///   - CRTP 基类 amr::hal::sensor::SensorBase<Derived, DataType>
///   - 注册宏 AMR_REGISTER_SENSOR("temperature", "simulated", ...) 位于
///     src/hal/temperature_sensor.cpp（文档要求注册放 adapter cpp）
///   - 可靠路径：幂等 ensure_temperature_registered()，消费节点 on_configure
///     显式调用（静态库下自注册 TU 会被链接器丢弃，见 registry.hpp 头注释）
#include "ros2_robot_middleware/hal/common/registry.hpp"
#include "ros2_robot_middleware/hal/sensor/isensor.hpp"

#include <cmath>

namespace amr::hal::sensor {

/// 单点温度数据（单位：摄氏度）
struct TemperatureData {
    float    temperature_c = 0.0F;
    /// 测量时刻（纳秒）。0 = 未盖章：合成路径由 infra 在读取边界补戳
    /// （打戳规则见 isensor.hpp 头注释引用的 timestamp-policy ADR）。
    int64_t  stamp_ns      = 0;
};

/// 模拟温度传感器：围绕可配置的环境温度生成慢正弦漂移 + 小噪声。
/// 模型对齐 SimulatedUltrasonic 的「读取时合成」风格。
class SimulatedTemperature : public SensorBase<SimulatedTemperature, TemperatureData> {
public:
    SimulatedTemperature() = default;

    /// 直接指定环境温度（测试/场景注入用）
    explicit SimulatedTemperature(float ambient_c)
        : ambient_c_(ambient_c) {}

    bool init_impl() {
        // 模拟传感器无硬件资源；健康度 0 = OK（约定见 subsystems/sensor-pipeline.md）
        this->health_ = 0;
        return true;
    }

    bool read_impl(TemperatureData &out) {
        const float drift = 2.0F * std::sin(static_cast<float>(seq_) * 0.02F);
        const float noise = static_cast<float>(std::rand() % 11 - 5) * 0.01F;

        out.temperature_c = ambient_c_ + drift + noise;
        out.stamp_ns      = 0;  // 合成数据未盖章，消费端（infra）在边界补 now_ns()
        ++seq_;
        return true;
    }

    void shutdown_impl() {
        // 无资源需要释放
    }

    /// 测试钩子：改写模拟环境温度
    void set_simulated_ambient(float ambient_c) {
        ambient_c_ = ambient_c;
    }

private:
    float    ambient_c_ = 25.0F;
    uint64_t seq_       = 0;
};

/// 幂等注册：把 "temperature"/"simulated" 编入 SensorRegistry。
inline void ensure_temperature_registered() {
    static bool done = false;
    if (done) return;
    done = true;
    ::amr::hal::common::SensorRegistry::instance().register_type(
        "temperature", "simulated",
        []() -> void * { return new SimulatedTemperature(); });
}

} // namespace amr::hal::sensor

#endif  // ROS2_ROBOT_MIDDLEWARE_HAL_SENSOR_TEMPERATURE_SENSOR_HPP_
