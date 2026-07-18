// generated from rosidl_generator_c/resource/idl__functions.h.em
// with input from ros2_robot_middleware:msg/PerceptionObjects.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "ros2_robot_middleware/msg/perception_objects.h"


#ifndef ROS2_ROBOT_MIDDLEWARE__MSG__DETAIL__PERCEPTION_OBJECTS__FUNCTIONS_H_
#define ROS2_ROBOT_MIDDLEWARE__MSG__DETAIL__PERCEPTION_OBJECTS__FUNCTIONS_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stdlib.h>

#include "rosidl_runtime_c/action_type_support_struct.h"
#include "rosidl_runtime_c/message_type_support_struct.h"
#include "rosidl_runtime_c/service_type_support_struct.h"
#include "rosidl_runtime_c/type_description/type_description__struct.h"
#include "rosidl_runtime_c/type_description/type_source__struct.h"
#include "rosidl_runtime_c/type_hash.h"
#include "rosidl_runtime_c/visibility_control.h"
#include "ros2_robot_middleware/msg/rosidl_generator_c__visibility_control.h"

#include "ros2_robot_middleware/msg/detail/perception_objects__struct.h"

/// Initialize msg/PerceptionObjects message.
/**
 * If the init function is called twice for the same message without
 * calling fini inbetween previously allocated memory will be leaked.
 * \param[in,out] msg The previously allocated message pointer.
 * Fields without a default value will not be initialized by this function.
 * You might want to call memset(msg, 0, sizeof(
 * ros2_robot_middleware__msg__PerceptionObjects
 * )) before or use
 * ros2_robot_middleware__msg__PerceptionObjects__create()
 * to allocate and initialize the message.
 * \return true if initialization was successful, otherwise false
 */
ROSIDL_GENERATOR_C_PUBLIC_ros2_robot_middleware
bool
ros2_robot_middleware__msg__PerceptionObjects__init(ros2_robot_middleware__msg__PerceptionObjects * msg);

/// Finalize msg/PerceptionObjects message.
/**
 * \param[in,out] msg The allocated message pointer.
 */
ROSIDL_GENERATOR_C_PUBLIC_ros2_robot_middleware
void
ros2_robot_middleware__msg__PerceptionObjects__fini(ros2_robot_middleware__msg__PerceptionObjects * msg);

/// Create msg/PerceptionObjects message.
/**
 * It allocates the memory for the message, sets the memory to zero, and
 * calls
 * ros2_robot_middleware__msg__PerceptionObjects__init().
 * \return The pointer to the initialized message if successful,
 * otherwise NULL
 */
ROSIDL_GENERATOR_C_PUBLIC_ros2_robot_middleware
ros2_robot_middleware__msg__PerceptionObjects *
ros2_robot_middleware__msg__PerceptionObjects__create(void);

/// Destroy msg/PerceptionObjects message.
/**
 * It calls
 * ros2_robot_middleware__msg__PerceptionObjects__fini()
 * and frees the memory of the message.
 * \param[in,out] msg The allocated message pointer.
 */
ROSIDL_GENERATOR_C_PUBLIC_ros2_robot_middleware
void
ros2_robot_middleware__msg__PerceptionObjects__destroy(ros2_robot_middleware__msg__PerceptionObjects * msg);

/// Check for msg/PerceptionObjects message equality.
/**
 * \param[in] lhs The message on the left hand size of the equality operator.
 * \param[in] rhs The message on the right hand size of the equality operator.
 * \return true if messages are equal, otherwise false.
 */
ROSIDL_GENERATOR_C_PUBLIC_ros2_robot_middleware
bool
ros2_robot_middleware__msg__PerceptionObjects__are_equal(const ros2_robot_middleware__msg__PerceptionObjects * lhs, const ros2_robot_middleware__msg__PerceptionObjects * rhs);

/// Copy a msg/PerceptionObjects message.
/**
 * This functions performs a deep copy, as opposed to the shallow copy that
 * plain assignment yields.
 *
 * \param[in] input The source message pointer.
 * \param[out] output The target message pointer, which must
 *   have been initialized before calling this function.
 * \return true if successful, or false if either pointer is null
 *   or memory allocation fails.
 */
ROSIDL_GENERATOR_C_PUBLIC_ros2_robot_middleware
bool
ros2_robot_middleware__msg__PerceptionObjects__copy(
  const ros2_robot_middleware__msg__PerceptionObjects * input,
  ros2_robot_middleware__msg__PerceptionObjects * output);

/// Retrieve pointer to the hash of the description of this type.
ROSIDL_GENERATOR_C_PUBLIC_ros2_robot_middleware
const rosidl_type_hash_t *
ros2_robot_middleware__msg__PerceptionObjects__get_type_hash(
  const rosidl_message_type_support_t * type_support);

/// Retrieve pointer to the description of this type.
ROSIDL_GENERATOR_C_PUBLIC_ros2_robot_middleware
const rosidl_runtime_c__type_description__TypeDescription *
ros2_robot_middleware__msg__PerceptionObjects__get_type_description(
  const rosidl_message_type_support_t * type_support);

/// Retrieve pointer to the single raw source text that defined this type.
ROSIDL_GENERATOR_C_PUBLIC_ros2_robot_middleware
const rosidl_runtime_c__type_description__TypeSource *
ros2_robot_middleware__msg__PerceptionObjects__get_individual_type_description_source(
  const rosidl_message_type_support_t * type_support);

/// Retrieve pointer to the recursive raw sources that defined the description of this type.
ROSIDL_GENERATOR_C_PUBLIC_ros2_robot_middleware
const rosidl_runtime_c__type_description__TypeSource__Sequence *
ros2_robot_middleware__msg__PerceptionObjects__get_type_description_sources(
  const rosidl_message_type_support_t * type_support);

/// Initialize array of msg/PerceptionObjects messages.
/**
 * It allocates the memory for the number of elements and calls
 * ros2_robot_middleware__msg__PerceptionObjects__init()
 * for each element of the array.
 * \param[in,out] array The allocated array pointer.
 * \param[in] size The size / capacity of the array.
 * \return true if initialization was successful, otherwise false
 * If the array pointer is valid and the size is zero it is guaranteed
 # to return true.
 */
ROSIDL_GENERATOR_C_PUBLIC_ros2_robot_middleware
bool
ros2_robot_middleware__msg__PerceptionObjects__Sequence__init(ros2_robot_middleware__msg__PerceptionObjects__Sequence * array, size_t size);

/// Finalize array of msg/PerceptionObjects messages.
/**
 * It calls
 * ros2_robot_middleware__msg__PerceptionObjects__fini()
 * for each element of the array and frees the memory for the number of
 * elements.
 * \param[in,out] array The initialized array pointer.
 */
ROSIDL_GENERATOR_C_PUBLIC_ros2_robot_middleware
void
ros2_robot_middleware__msg__PerceptionObjects__Sequence__fini(ros2_robot_middleware__msg__PerceptionObjects__Sequence * array);

/// Create array of msg/PerceptionObjects messages.
/**
 * It allocates the memory for the array and calls
 * ros2_robot_middleware__msg__PerceptionObjects__Sequence__init().
 * \param[in] size The size / capacity of the array.
 * \return The pointer to the initialized array if successful, otherwise NULL
 */
ROSIDL_GENERATOR_C_PUBLIC_ros2_robot_middleware
ros2_robot_middleware__msg__PerceptionObjects__Sequence *
ros2_robot_middleware__msg__PerceptionObjects__Sequence__create(size_t size);

/// Destroy array of msg/PerceptionObjects messages.
/**
 * It calls
 * ros2_robot_middleware__msg__PerceptionObjects__Sequence__fini()
 * on the array,
 * and frees the memory of the array.
 * \param[in,out] array The initialized array pointer.
 */
ROSIDL_GENERATOR_C_PUBLIC_ros2_robot_middleware
void
ros2_robot_middleware__msg__PerceptionObjects__Sequence__destroy(ros2_robot_middleware__msg__PerceptionObjects__Sequence * array);

/// Check for msg/PerceptionObjects message array equality.
/**
 * \param[in] lhs The message array on the left hand size of the equality operator.
 * \param[in] rhs The message array on the right hand size of the equality operator.
 * \return true if message arrays are equal in size and content, otherwise false.
 */
ROSIDL_GENERATOR_C_PUBLIC_ros2_robot_middleware
bool
ros2_robot_middleware__msg__PerceptionObjects__Sequence__are_equal(const ros2_robot_middleware__msg__PerceptionObjects__Sequence * lhs, const ros2_robot_middleware__msg__PerceptionObjects__Sequence * rhs);

/// Copy an array of msg/PerceptionObjects messages.
/**
 * This functions performs a deep copy, as opposed to the shallow copy that
 * plain assignment yields.
 *
 * \param[in] input The source array pointer.
 * \param[out] output The target array pointer, which must
 *   have been initialized before calling this function.
 * \return true if successful, or false if either pointer
 *   is null or memory allocation fails.
 */
ROSIDL_GENERATOR_C_PUBLIC_ros2_robot_middleware
bool
ros2_robot_middleware__msg__PerceptionObjects__Sequence__copy(
  const ros2_robot_middleware__msg__PerceptionObjects__Sequence * input,
  ros2_robot_middleware__msg__PerceptionObjects__Sequence * output);

#ifdef __cplusplus
}
#endif

#endif  // ROS2_ROBOT_MIDDLEWARE__MSG__DETAIL__PERCEPTION_OBJECTS__FUNCTIONS_H_
