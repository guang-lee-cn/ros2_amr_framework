// D1-RUN2: 第三方开发者扩展验收实验——模拟温度的注册。
/// @file   temperature_sensor.cpp — 模拟温度传感器的注册。
///
/// 两条注册路径并存（见 registry.hpp 头注释）：
///   - AMR_REGISTER_SENSOR 宏：自注册对象（本 TU 在 lib 内，可能被链接器
///     丢弃——不作为唯一保证）
///   - ensure_temperature_registered()：幂等显式注册，消费节点 on_configure
///     调用（TemperatureNode 即此用法）——静态库下的可靠路径
#include "ros2_robot_middleware/hal/common/registry.hpp"
#include "ros2_robot_middleware/hal/sensor/temperature_sensor.hpp"

AMR_REGISTER_SENSOR("temperature", "simulated",
                    ::amr::hal::sensor::SimulatedTemperature);
