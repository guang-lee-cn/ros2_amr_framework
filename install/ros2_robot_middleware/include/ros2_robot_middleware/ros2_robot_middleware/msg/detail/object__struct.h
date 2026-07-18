// NOLINT: This file starts with a BOM since it contain non-ASCII characters
// generated from rosidl_generator_c/resource/idl__struct.h.em
// with input from ros2_robot_middleware:msg/Object.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "ros2_robot_middleware/msg/object.h"


#ifndef ROS2_ROBOT_MIDDLEWARE__MSG__DETAIL__OBJECT__STRUCT_H_
#define ROS2_ROBOT_MIDDLEWARE__MSG__DETAIL__OBJECT__STRUCT_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Constants defined in the message

// Include directives for member types
// Member 'id'
#include "rosidl_runtime_c/string.h"

/// Struct defined in msg/Object in the package ros2_robot_middleware.
typedef struct ros2_robot_middleware__msg__Object
{
  /// 物体唯一标识
  rosidl_runtime_c__String id;
  /// base_link 坐标系, 前方 (m)
  float x;
  /// base_link 坐标系, 左方 (m)
  float y;
  /// base_link 坐标系, 上方 (m)
  float z;
} ros2_robot_middleware__msg__Object;

// Struct for a sequence of ros2_robot_middleware__msg__Object.
typedef struct ros2_robot_middleware__msg__Object__Sequence
{
  ros2_robot_middleware__msg__Object * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} ros2_robot_middleware__msg__Object__Sequence;

#ifdef __cplusplus
}
#endif

#endif  // ROS2_ROBOT_MIDDLEWARE__MSG__DETAIL__OBJECT__STRUCT_H_
