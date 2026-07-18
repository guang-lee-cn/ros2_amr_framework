// generated from rosidl_generator_c/resource/idl__struct.h.em
// with input from ros2_robot_middleware:msg/HealthStatus.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "ros2_robot_middleware/msg/health_status.h"


#ifndef ROS2_ROBOT_MIDDLEWARE__MSG__DETAIL__HEALTH_STATUS__STRUCT_H_
#define ROS2_ROBOT_MIDDLEWARE__MSG__DETAIL__HEALTH_STATUS__STRUCT_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Constants defined in the message

// Include directives for member types
// Member 'node_name'
// Member 'status'
#include "rosidl_runtime_c/string.h"

/// Struct defined in msg/HealthStatus in the package ros2_robot_middleware.
/**
  * Per-node health snapshot
 */
typedef struct ros2_robot_middleware__msg__HealthStatus
{
  rosidl_runtime_c__String node_name;
  /// OK | WARN | ERROR | STALE
  rosidl_runtime_c__String status;
  /// seconds since last activity
  double last_seen_s;
  /// configured timeout for this node
  double timeout_s;
} ros2_robot_middleware__msg__HealthStatus;

// Struct for a sequence of ros2_robot_middleware__msg__HealthStatus.
typedef struct ros2_robot_middleware__msg__HealthStatus__Sequence
{
  ros2_robot_middleware__msg__HealthStatus * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} ros2_robot_middleware__msg__HealthStatus__Sequence;

#ifdef __cplusplus
}
#endif

#endif  // ROS2_ROBOT_MIDDLEWARE__MSG__DETAIL__HEALTH_STATUS__STRUCT_H_
