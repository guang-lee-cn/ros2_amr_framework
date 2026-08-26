/// @file   ultrasonic_sensor.cpp — 模拟超声波的注册。
///
/// 两条注册路径并存（见 registry.hpp 头注释，D1 实验实证的可靠组合）：
///   - AMR_REGISTER_SENSOR 宏：自注册对象（本 TU 在 lib 内，可能被链接器
///     丢弃——不作为唯一保证）
///   - ensure_ultrasonic_registered()：幂等显式注册，消费节点 on_configure
///     调用（UltrasonicNode 即此用法）——静态库下的可靠路径
#include "ros2_robot_middleware/hal/common/registry.hpp"
#include "ros2_robot_middleware/hal/sensor/ultrasonic_sensor.hpp"

AMR_REGISTER_SENSOR("ultrasonic", "simulated",
                    ::amr::hal::sensor::SimulatedUltrasonic);
