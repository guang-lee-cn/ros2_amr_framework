// generated from rosidl_generator_c/resource/idl__type_support.h.em
// with input from ros2_robot_middleware:srv/SetParam.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "ros2_robot_middleware/srv/set_param.h"


#ifndef ROS2_ROBOT_MIDDLEWARE__SRV__DETAIL__SET_PARAM__TYPE_SUPPORT_H_
#define ROS2_ROBOT_MIDDLEWARE__SRV__DETAIL__SET_PARAM__TYPE_SUPPORT_H_

#include "rosidl_typesupport_interface/macros.h"

#include "ros2_robot_middleware/msg/rosidl_generator_c__visibility_control.h"

#ifdef __cplusplus
extern "C"
{
#endif

#include "rosidl_runtime_c/message_type_support_struct.h"

// Forward declare the get type support functions for this type.
ROSIDL_GENERATOR_C_PUBLIC_ros2_robot_middleware
const rosidl_message_type_support_t *
ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(
  rosidl_typesupport_c,
  ros2_robot_middleware,
  srv,
  SetParam_Request
)(void);

// already included above
// #include "rosidl_runtime_c/message_type_support_struct.h"

// Forward declare the get type support functions for this type.
ROSIDL_GENERATOR_C_PUBLIC_ros2_robot_middleware
const rosidl_message_type_support_t *
ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(
  rosidl_typesupport_c,
  ros2_robot_middleware,
  srv,
  SetParam_Response
)(void);

// already included above
// #include "rosidl_runtime_c/message_type_support_struct.h"

// Forward declare the get type support functions for this type.
ROSIDL_GENERATOR_C_PUBLIC_ros2_robot_middleware
const rosidl_message_type_support_t *
ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(
  rosidl_typesupport_c,
  ros2_robot_middleware,
  srv,
  SetParam_Event
)(void);

#include "rosidl_runtime_c/service_type_support_struct.h"

// Forward declare the get type support functions for this type.
ROSIDL_GENERATOR_C_PUBLIC_ros2_robot_middleware
const rosidl_service_type_support_t *
ROSIDL_TYPESUPPORT_INTERFACE__SERVICE_SYMBOL_NAME(
  rosidl_typesupport_c,
  ros2_robot_middleware,
  srv,
  SetParam
)(void);

// Forward declare the function to create a service event message for this type.
ROSIDL_GENERATOR_C_PUBLIC_ros2_robot_middleware
void *
ROSIDL_TYPESUPPORT_INTERFACE__SERVICE_CREATE_EVENT_MESSAGE_SYMBOL_NAME(
  rosidl_typesupport_c,
  ros2_robot_middleware,
  srv,
  SetParam
)(
  const rosidl_service_introspection_info_t * info,
  rcutils_allocator_t * allocator,
  const void * request_message,
  const void * response_message);

// Forward declare the function to destroy a service event message for this type.
ROSIDL_GENERATOR_C_PUBLIC_ros2_robot_middleware
bool
ROSIDL_TYPESUPPORT_INTERFACE__SERVICE_DESTROY_EVENT_MESSAGE_SYMBOL_NAME(
  rosidl_typesupport_c,
  ros2_robot_middleware,
  srv,
  SetParam
)(
  void * event_msg,
  rcutils_allocator_t * allocator);

#ifdef __cplusplus
}
#endif

#endif  // ROS2_ROBOT_MIDDLEWARE__SRV__DETAIL__SET_PARAM__TYPE_SUPPORT_H_
