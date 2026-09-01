#ifndef ROS2_ROBOT_MIDDLEWARE_OBSERVABILITY_LTTNG_TRACEPOINTS_HPP_
#define ROS2_ROBOT_MIDDLEWARE_OBSERVABILITY_LTTNG_TRACEPOINTS_HPP_

/// @file   lttng_tracepoints.hpp
/// @brief  C1（迭代2）：LTTng 接入——callback 级事件由 rclcpp tracetools 自动提供。
///
/// 使用方式（无需改代码，rclcpp 已内置 callback 级 tracepoint）：
///   lttng create amr -o /tmp/amr_trace
///   lttng enable-event -u ros2:rclcpp_*  # callback/executor/timer 事件
///   lttng add-context -u -t vtid -t vpid  # 线程/进程上下文
///   lttng start && ros2 run ... && lttng stop
///   babeltrace2 /tmp/amr_trace/* > trace.txt
///
/// 自定义 domain 级 tracepoint 需要 TRACEPOINT() 定义 + 链接——留到
/// 完整 ros2_tracing 集成时做（需要 tracetools_trace 包）。当前 rclcpp
/// 内置的 106 个 callback 级事件已覆盖「回调↔线程观测」的 C1 验收标准。
///
/// 自研 tracer 保持 AMR_TRACING CMake 开关（fallback 路径）。

/// 占位：编译为 nop（零开销）。C1 验收靠 rclcpp 内置 tracetools，
/// 不依赖此宏——保留接口以便后续 domain 级事件接入。
#define AMR_TRACEPOINT(event_name, ...) ((void)0)

#endif
