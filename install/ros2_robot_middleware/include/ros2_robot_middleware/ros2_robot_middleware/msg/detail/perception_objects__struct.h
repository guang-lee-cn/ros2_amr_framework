// NOLINT: This file starts with a BOM since it contain non-ASCII characters
// generated from rosidl_generator_c/resource/idl__struct.h.em
// with input from ros2_robot_middleware:msg/PerceptionObjects.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "ros2_robot_middleware/msg/perception_objects.h"


#ifndef ROS2_ROBOT_MIDDLEWARE__MSG__DETAIL__PERCEPTION_OBJECTS__STRUCT_H_
#define ROS2_ROBOT_MIDDLEWARE__MSG__DETAIL__PERCEPTION_OBJECTS__STRUCT_H_

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
// Member 'objects'
#include "ros2_robot_middleware/msg/detail/object__struct.h"

/// Struct defined in msg/PerceptionObjects in the package ros2_robot_middleware.
/**
  * Fused perception output — detected objects with positions
 */
typedef struct ros2_robot_middleware__msg__PerceptionObjects
{
  std_msgs__msg__Header header;
  ros2_robot_middleware__msg__Object__Sequence objects;
} ros2_robot_middleware__msg__PerceptionObjects;

// Struct for a sequence of ros2_robot_middleware__msg__PerceptionObjects.
typedef struct ros2_robot_middleware__msg__PerceptionObjects__Sequence
{
  ros2_robot_middleware__msg__PerceptionObjects * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} ros2_robot_middleware__msg__PerceptionObjects__Sequence;

#ifdef __cplusplus
}
#endif

#endif  // ROS2_ROBOT_MIDDLEWARE__MSG__DETAIL__PERCEPTION_OBJECTS__STRUCT_H_
