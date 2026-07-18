// generated from rosidl_generator_c/resource/idl__struct.h.em
// with input from ros2_robot_middleware:msg/HealthReport.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "ros2_robot_middleware/msg/health_report.h"


#ifndef ROS2_ROBOT_MIDDLEWARE__MSG__DETAIL__HEALTH_REPORT__STRUCT_H_
#define ROS2_ROBOT_MIDDLEWARE__MSG__DETAIL__HEALTH_REPORT__STRUCT_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Constants defined in the message

// Include directives for member types
// Member 'header'
#include "std_msgs/msg/detail/header__struct.h"
// Member 'nodes'
#include "ros2_robot_middleware/msg/detail/health_status__struct.h"

/// Struct defined in msg/HealthReport in the package ros2_robot_middleware.
/**
  * Aggregate health report for all monitored nodes
 */
typedef struct ros2_robot_middleware__msg__HealthReport
{
  std_msgs__msg__Header header;
  ros2_robot_middleware__msg__HealthStatus__Sequence nodes;
} ros2_robot_middleware__msg__HealthReport;

// Struct for a sequence of ros2_robot_middleware__msg__HealthReport.
typedef struct ros2_robot_middleware__msg__HealthReport__Sequence
{
  ros2_robot_middleware__msg__HealthReport * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} ros2_robot_middleware__msg__HealthReport__Sequence;

#ifdef __cplusplus
}
#endif

#endif  // ROS2_ROBOT_MIDDLEWARE__MSG__DETAIL__HEALTH_REPORT__STRUCT_H_
