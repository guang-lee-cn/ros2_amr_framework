// generated from rosidl_typesupport_fastrtps_cpp/resource/idl__rosidl_typesupport_fastrtps_cpp.hpp.em
// with input from ros2_robot_middleware:action/MoveToPose.idl
// generated code does not contain a copyright notice

#ifndef ROS2_ROBOT_MIDDLEWARE__ACTION__DETAIL__MOVE_TO_POSE__ROSIDL_TYPESUPPORT_FASTRTPS_CPP_HPP_
#define ROS2_ROBOT_MIDDLEWARE__ACTION__DETAIL__MOVE_TO_POSE__ROSIDL_TYPESUPPORT_FASTRTPS_CPP_HPP_

#include <cstddef>
#include "rosidl_runtime_c/message_type_support_struct.h"
#include "rosidl_typesupport_interface/macros.h"
#include "ros2_robot_middleware/msg/rosidl_typesupport_fastrtps_cpp__visibility_control.h"
#include "ros2_robot_middleware/action/detail/move_to_pose__struct.hpp"

#ifndef _WIN32
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wunused-parameter"
# ifdef __clang__
#  pragma clang diagnostic ignored "-Wdeprecated-register"
#  pragma clang diagnostic ignored "-Wreturn-type-c-linkage"
# endif
#endif
#ifndef _WIN32
# pragma GCC diagnostic pop
#endif

#include "fastcdr/Cdr.h"

namespace ros2_robot_middleware
{

namespace action
{

namespace typesupport_fastrtps_cpp
{

bool
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
cdr_serialize(
  const ros2_robot_middleware::action::MoveToPose_Goal & ros_message,
  eprosima::fastcdr::Cdr & cdr);

bool
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
cdr_deserialize(
  eprosima::fastcdr::Cdr & cdr,
  ros2_robot_middleware::action::MoveToPose_Goal & ros_message);

size_t
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
get_serialized_size(
  const ros2_robot_middleware::action::MoveToPose_Goal & ros_message,
  size_t current_alignment);

size_t
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
max_serialized_size_MoveToPose_Goal(
  bool & full_bounded,
  bool & is_plain,
  size_t current_alignment);

bool
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
cdr_serialize_key(
  const ros2_robot_middleware::action::MoveToPose_Goal & ros_message,
  eprosima::fastcdr::Cdr &);

size_t
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
get_serialized_size_key(
  const ros2_robot_middleware::action::MoveToPose_Goal & ros_message,
  size_t current_alignment);

size_t
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
max_serialized_size_key_MoveToPose_Goal(
  bool & full_bounded,
  bool & is_plain,
  size_t current_alignment);

}  // namespace typesupport_fastrtps_cpp

}  // namespace action

}  // namespace ros2_robot_middleware

#ifdef __cplusplus
extern "C"
{
#endif

ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
const rosidl_message_type_support_t *
  ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_fastrtps_cpp, ros2_robot_middleware, action, MoveToPose_Goal)();

#ifdef __cplusplus
}
#endif
// already included above
// #include <cstddef>
// already included above
// #include "rosidl_runtime_c/message_type_support_struct.h"
// already included above
// #include "rosidl_typesupport_interface/macros.h"
// already included above
// #include "ros2_robot_middleware/msg/rosidl_typesupport_fastrtps_cpp__visibility_control.h"
// already included above
// #include "ros2_robot_middleware/action/detail/move_to_pose__struct.hpp"

#ifndef _WIN32
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wunused-parameter"
# ifdef __clang__
#  pragma clang diagnostic ignored "-Wdeprecated-register"
#  pragma clang diagnostic ignored "-Wreturn-type-c-linkage"
# endif
#endif
#ifndef _WIN32
# pragma GCC diagnostic pop
#endif

// already included above
// #include "fastcdr/Cdr.h"

namespace ros2_robot_middleware
{

namespace action
{

namespace typesupport_fastrtps_cpp
{

bool
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
cdr_serialize(
  const ros2_robot_middleware::action::MoveToPose_Result & ros_message,
  eprosima::fastcdr::Cdr & cdr);

bool
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
cdr_deserialize(
  eprosima::fastcdr::Cdr & cdr,
  ros2_robot_middleware::action::MoveToPose_Result & ros_message);

size_t
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
get_serialized_size(
  const ros2_robot_middleware::action::MoveToPose_Result & ros_message,
  size_t current_alignment);

size_t
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
max_serialized_size_MoveToPose_Result(
  bool & full_bounded,
  bool & is_plain,
  size_t current_alignment);

bool
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
cdr_serialize_key(
  const ros2_robot_middleware::action::MoveToPose_Result & ros_message,
  eprosima::fastcdr::Cdr &);

size_t
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
get_serialized_size_key(
  const ros2_robot_middleware::action::MoveToPose_Result & ros_message,
  size_t current_alignment);

size_t
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
max_serialized_size_key_MoveToPose_Result(
  bool & full_bounded,
  bool & is_plain,
  size_t current_alignment);

}  // namespace typesupport_fastrtps_cpp

}  // namespace action

}  // namespace ros2_robot_middleware

#ifdef __cplusplus
extern "C"
{
#endif

ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
const rosidl_message_type_support_t *
  ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_fastrtps_cpp, ros2_robot_middleware, action, MoveToPose_Result)();

#ifdef __cplusplus
}
#endif
// already included above
// #include <cstddef>
// already included above
// #include "rosidl_runtime_c/message_type_support_struct.h"
// already included above
// #include "rosidl_typesupport_interface/macros.h"
// already included above
// #include "ros2_robot_middleware/msg/rosidl_typesupport_fastrtps_cpp__visibility_control.h"
// already included above
// #include "ros2_robot_middleware/action/detail/move_to_pose__struct.hpp"

#ifndef _WIN32
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wunused-parameter"
# ifdef __clang__
#  pragma clang diagnostic ignored "-Wdeprecated-register"
#  pragma clang diagnostic ignored "-Wreturn-type-c-linkage"
# endif
#endif
#ifndef _WIN32
# pragma GCC diagnostic pop
#endif

// already included above
// #include "fastcdr/Cdr.h"

namespace ros2_robot_middleware
{

namespace action
{

namespace typesupport_fastrtps_cpp
{

bool
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
cdr_serialize(
  const ros2_robot_middleware::action::MoveToPose_Feedback & ros_message,
  eprosima::fastcdr::Cdr & cdr);

bool
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
cdr_deserialize(
  eprosima::fastcdr::Cdr & cdr,
  ros2_robot_middleware::action::MoveToPose_Feedback & ros_message);

size_t
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
get_serialized_size(
  const ros2_robot_middleware::action::MoveToPose_Feedback & ros_message,
  size_t current_alignment);

size_t
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
max_serialized_size_MoveToPose_Feedback(
  bool & full_bounded,
  bool & is_plain,
  size_t current_alignment);

bool
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
cdr_serialize_key(
  const ros2_robot_middleware::action::MoveToPose_Feedback & ros_message,
  eprosima::fastcdr::Cdr &);

size_t
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
get_serialized_size_key(
  const ros2_robot_middleware::action::MoveToPose_Feedback & ros_message,
  size_t current_alignment);

size_t
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
max_serialized_size_key_MoveToPose_Feedback(
  bool & full_bounded,
  bool & is_plain,
  size_t current_alignment);

}  // namespace typesupport_fastrtps_cpp

}  // namespace action

}  // namespace ros2_robot_middleware

#ifdef __cplusplus
extern "C"
{
#endif

ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
const rosidl_message_type_support_t *
  ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_fastrtps_cpp, ros2_robot_middleware, action, MoveToPose_Feedback)();

#ifdef __cplusplus
}
#endif
// already included above
// #include <cstddef>
// already included above
// #include "rosidl_runtime_c/message_type_support_struct.h"
// already included above
// #include "rosidl_typesupport_interface/macros.h"
// already included above
// #include "ros2_robot_middleware/msg/rosidl_typesupport_fastrtps_cpp__visibility_control.h"
// already included above
// #include "ros2_robot_middleware/action/detail/move_to_pose__struct.hpp"

#ifndef _WIN32
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wunused-parameter"
# ifdef __clang__
#  pragma clang diagnostic ignored "-Wdeprecated-register"
#  pragma clang diagnostic ignored "-Wreturn-type-c-linkage"
# endif
#endif
#ifndef _WIN32
# pragma GCC diagnostic pop
#endif

// already included above
// #include "fastcdr/Cdr.h"

namespace ros2_robot_middleware
{

namespace action
{

namespace typesupport_fastrtps_cpp
{

bool
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
cdr_serialize(
  const ros2_robot_middleware::action::MoveToPose_SendGoal_Request & ros_message,
  eprosima::fastcdr::Cdr & cdr);

bool
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
cdr_deserialize(
  eprosima::fastcdr::Cdr & cdr,
  ros2_robot_middleware::action::MoveToPose_SendGoal_Request & ros_message);

size_t
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
get_serialized_size(
  const ros2_robot_middleware::action::MoveToPose_SendGoal_Request & ros_message,
  size_t current_alignment);

size_t
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
max_serialized_size_MoveToPose_SendGoal_Request(
  bool & full_bounded,
  bool & is_plain,
  size_t current_alignment);

bool
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
cdr_serialize_key(
  const ros2_robot_middleware::action::MoveToPose_SendGoal_Request & ros_message,
  eprosima::fastcdr::Cdr &);

size_t
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
get_serialized_size_key(
  const ros2_robot_middleware::action::MoveToPose_SendGoal_Request & ros_message,
  size_t current_alignment);

size_t
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
max_serialized_size_key_MoveToPose_SendGoal_Request(
  bool & full_bounded,
  bool & is_plain,
  size_t current_alignment);

}  // namespace typesupport_fastrtps_cpp

}  // namespace action

}  // namespace ros2_robot_middleware

#ifdef __cplusplus
extern "C"
{
#endif

ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
const rosidl_message_type_support_t *
  ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_fastrtps_cpp, ros2_robot_middleware, action, MoveToPose_SendGoal_Request)();

#ifdef __cplusplus
}
#endif

// already included above
// #include <cstddef>
// already included above
// #include "rosidl_runtime_c/message_type_support_struct.h"
// already included above
// #include "rosidl_typesupport_interface/macros.h"
// already included above
// #include "ros2_robot_middleware/msg/rosidl_typesupport_fastrtps_cpp__visibility_control.h"
// already included above
// #include "ros2_robot_middleware/action/detail/move_to_pose__struct.hpp"

#ifndef _WIN32
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wunused-parameter"
# ifdef __clang__
#  pragma clang diagnostic ignored "-Wdeprecated-register"
#  pragma clang diagnostic ignored "-Wreturn-type-c-linkage"
# endif
#endif
#ifndef _WIN32
# pragma GCC diagnostic pop
#endif

// already included above
// #include "fastcdr/Cdr.h"

namespace ros2_robot_middleware
{

namespace action
{

namespace typesupport_fastrtps_cpp
{

bool
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
cdr_serialize(
  const ros2_robot_middleware::action::MoveToPose_SendGoal_Response & ros_message,
  eprosima::fastcdr::Cdr & cdr);

bool
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
cdr_deserialize(
  eprosima::fastcdr::Cdr & cdr,
  ros2_robot_middleware::action::MoveToPose_SendGoal_Response & ros_message);

size_t
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
get_serialized_size(
  const ros2_robot_middleware::action::MoveToPose_SendGoal_Response & ros_message,
  size_t current_alignment);

size_t
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
max_serialized_size_MoveToPose_SendGoal_Response(
  bool & full_bounded,
  bool & is_plain,
  size_t current_alignment);

bool
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
cdr_serialize_key(
  const ros2_robot_middleware::action::MoveToPose_SendGoal_Response & ros_message,
  eprosima::fastcdr::Cdr &);

size_t
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
get_serialized_size_key(
  const ros2_robot_middleware::action::MoveToPose_SendGoal_Response & ros_message,
  size_t current_alignment);

size_t
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
max_serialized_size_key_MoveToPose_SendGoal_Response(
  bool & full_bounded,
  bool & is_plain,
  size_t current_alignment);

}  // namespace typesupport_fastrtps_cpp

}  // namespace action

}  // namespace ros2_robot_middleware

#ifdef __cplusplus
extern "C"
{
#endif

ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
const rosidl_message_type_support_t *
  ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_fastrtps_cpp, ros2_robot_middleware, action, MoveToPose_SendGoal_Response)();

#ifdef __cplusplus
}
#endif

// already included above
// #include <cstddef>
// already included above
// #include "rosidl_runtime_c/message_type_support_struct.h"
// already included above
// #include "rosidl_typesupport_interface/macros.h"
// already included above
// #include "ros2_robot_middleware/msg/rosidl_typesupport_fastrtps_cpp__visibility_control.h"
// already included above
// #include "ros2_robot_middleware/action/detail/move_to_pose__struct.hpp"

#ifndef _WIN32
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wunused-parameter"
# ifdef __clang__
#  pragma clang diagnostic ignored "-Wdeprecated-register"
#  pragma clang diagnostic ignored "-Wreturn-type-c-linkage"
# endif
#endif
#ifndef _WIN32
# pragma GCC diagnostic pop
#endif

// already included above
// #include "fastcdr/Cdr.h"

namespace ros2_robot_middleware
{

namespace action
{

namespace typesupport_fastrtps_cpp
{

bool
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
cdr_serialize(
  const ros2_robot_middleware::action::MoveToPose_SendGoal_Event & ros_message,
  eprosima::fastcdr::Cdr & cdr);

bool
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
cdr_deserialize(
  eprosima::fastcdr::Cdr & cdr,
  ros2_robot_middleware::action::MoveToPose_SendGoal_Event & ros_message);

size_t
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
get_serialized_size(
  const ros2_robot_middleware::action::MoveToPose_SendGoal_Event & ros_message,
  size_t current_alignment);

size_t
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
max_serialized_size_MoveToPose_SendGoal_Event(
  bool & full_bounded,
  bool & is_plain,
  size_t current_alignment);

bool
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
cdr_serialize_key(
  const ros2_robot_middleware::action::MoveToPose_SendGoal_Event & ros_message,
  eprosima::fastcdr::Cdr &);

size_t
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
get_serialized_size_key(
  const ros2_robot_middleware::action::MoveToPose_SendGoal_Event & ros_message,
  size_t current_alignment);

size_t
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
max_serialized_size_key_MoveToPose_SendGoal_Event(
  bool & full_bounded,
  bool & is_plain,
  size_t current_alignment);

}  // namespace typesupport_fastrtps_cpp

}  // namespace action

}  // namespace ros2_robot_middleware

#ifdef __cplusplus
extern "C"
{
#endif

ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
const rosidl_message_type_support_t *
  ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_fastrtps_cpp, ros2_robot_middleware, action, MoveToPose_SendGoal_Event)();

#ifdef __cplusplus
}
#endif

#include "rmw/types.h"
#include "rosidl_typesupport_cpp/service_type_support.hpp"
// already included above
// #include "rosidl_typesupport_interface/macros.h"
// already included above
// #include "ros2_robot_middleware/msg/rosidl_typesupport_fastrtps_cpp__visibility_control.h"

#ifdef __cplusplus
extern "C"
{
#endif

ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
const rosidl_service_type_support_t *
  ROSIDL_TYPESUPPORT_INTERFACE__SERVICE_SYMBOL_NAME(rosidl_typesupport_fastrtps_cpp, ros2_robot_middleware, action, MoveToPose_SendGoal)();

#ifdef __cplusplus
}
#endif
// already included above
// #include <cstddef>
// already included above
// #include "rosidl_runtime_c/message_type_support_struct.h"
// already included above
// #include "rosidl_typesupport_interface/macros.h"
// already included above
// #include "ros2_robot_middleware/msg/rosidl_typesupport_fastrtps_cpp__visibility_control.h"
// already included above
// #include "ros2_robot_middleware/action/detail/move_to_pose__struct.hpp"

#ifndef _WIN32
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wunused-parameter"
# ifdef __clang__
#  pragma clang diagnostic ignored "-Wdeprecated-register"
#  pragma clang diagnostic ignored "-Wreturn-type-c-linkage"
# endif
#endif
#ifndef _WIN32
# pragma GCC diagnostic pop
#endif

// already included above
// #include "fastcdr/Cdr.h"

namespace ros2_robot_middleware
{

namespace action
{

namespace typesupport_fastrtps_cpp
{

bool
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
cdr_serialize(
  const ros2_robot_middleware::action::MoveToPose_GetResult_Request & ros_message,
  eprosima::fastcdr::Cdr & cdr);

bool
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
cdr_deserialize(
  eprosima::fastcdr::Cdr & cdr,
  ros2_robot_middleware::action::MoveToPose_GetResult_Request & ros_message);

size_t
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
get_serialized_size(
  const ros2_robot_middleware::action::MoveToPose_GetResult_Request & ros_message,
  size_t current_alignment);

size_t
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
max_serialized_size_MoveToPose_GetResult_Request(
  bool & full_bounded,
  bool & is_plain,
  size_t current_alignment);

bool
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
cdr_serialize_key(
  const ros2_robot_middleware::action::MoveToPose_GetResult_Request & ros_message,
  eprosima::fastcdr::Cdr &);

size_t
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
get_serialized_size_key(
  const ros2_robot_middleware::action::MoveToPose_GetResult_Request & ros_message,
  size_t current_alignment);

size_t
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
max_serialized_size_key_MoveToPose_GetResult_Request(
  bool & full_bounded,
  bool & is_plain,
  size_t current_alignment);

}  // namespace typesupport_fastrtps_cpp

}  // namespace action

}  // namespace ros2_robot_middleware

#ifdef __cplusplus
extern "C"
{
#endif

ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
const rosidl_message_type_support_t *
  ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_fastrtps_cpp, ros2_robot_middleware, action, MoveToPose_GetResult_Request)();

#ifdef __cplusplus
}
#endif

// already included above
// #include <cstddef>
// already included above
// #include "rosidl_runtime_c/message_type_support_struct.h"
// already included above
// #include "rosidl_typesupport_interface/macros.h"
// already included above
// #include "ros2_robot_middleware/msg/rosidl_typesupport_fastrtps_cpp__visibility_control.h"
// already included above
// #include "ros2_robot_middleware/action/detail/move_to_pose__struct.hpp"

#ifndef _WIN32
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wunused-parameter"
# ifdef __clang__
#  pragma clang diagnostic ignored "-Wdeprecated-register"
#  pragma clang diagnostic ignored "-Wreturn-type-c-linkage"
# endif
#endif
#ifndef _WIN32
# pragma GCC diagnostic pop
#endif

// already included above
// #include "fastcdr/Cdr.h"

namespace ros2_robot_middleware
{

namespace action
{

namespace typesupport_fastrtps_cpp
{

bool
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
cdr_serialize(
  const ros2_robot_middleware::action::MoveToPose_GetResult_Response & ros_message,
  eprosima::fastcdr::Cdr & cdr);

bool
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
cdr_deserialize(
  eprosima::fastcdr::Cdr & cdr,
  ros2_robot_middleware::action::MoveToPose_GetResult_Response & ros_message);

size_t
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
get_serialized_size(
  const ros2_robot_middleware::action::MoveToPose_GetResult_Response & ros_message,
  size_t current_alignment);

size_t
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
max_serialized_size_MoveToPose_GetResult_Response(
  bool & full_bounded,
  bool & is_plain,
  size_t current_alignment);

bool
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
cdr_serialize_key(
  const ros2_robot_middleware::action::MoveToPose_GetResult_Response & ros_message,
  eprosima::fastcdr::Cdr &);

size_t
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
get_serialized_size_key(
  const ros2_robot_middleware::action::MoveToPose_GetResult_Response & ros_message,
  size_t current_alignment);

size_t
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
max_serialized_size_key_MoveToPose_GetResult_Response(
  bool & full_bounded,
  bool & is_plain,
  size_t current_alignment);

}  // namespace typesupport_fastrtps_cpp

}  // namespace action

}  // namespace ros2_robot_middleware

#ifdef __cplusplus
extern "C"
{
#endif

ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
const rosidl_message_type_support_t *
  ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_fastrtps_cpp, ros2_robot_middleware, action, MoveToPose_GetResult_Response)();

#ifdef __cplusplus
}
#endif

// already included above
// #include <cstddef>
// already included above
// #include "rosidl_runtime_c/message_type_support_struct.h"
// already included above
// #include "rosidl_typesupport_interface/macros.h"
// already included above
// #include "ros2_robot_middleware/msg/rosidl_typesupport_fastrtps_cpp__visibility_control.h"
// already included above
// #include "ros2_robot_middleware/action/detail/move_to_pose__struct.hpp"

#ifndef _WIN32
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wunused-parameter"
# ifdef __clang__
#  pragma clang diagnostic ignored "-Wdeprecated-register"
#  pragma clang diagnostic ignored "-Wreturn-type-c-linkage"
# endif
#endif
#ifndef _WIN32
# pragma GCC diagnostic pop
#endif

// already included above
// #include "fastcdr/Cdr.h"

namespace ros2_robot_middleware
{

namespace action
{

namespace typesupport_fastrtps_cpp
{

bool
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
cdr_serialize(
  const ros2_robot_middleware::action::MoveToPose_GetResult_Event & ros_message,
  eprosima::fastcdr::Cdr & cdr);

bool
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
cdr_deserialize(
  eprosima::fastcdr::Cdr & cdr,
  ros2_robot_middleware::action::MoveToPose_GetResult_Event & ros_message);

size_t
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
get_serialized_size(
  const ros2_robot_middleware::action::MoveToPose_GetResult_Event & ros_message,
  size_t current_alignment);

size_t
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
max_serialized_size_MoveToPose_GetResult_Event(
  bool & full_bounded,
  bool & is_plain,
  size_t current_alignment);

bool
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
cdr_serialize_key(
  const ros2_robot_middleware::action::MoveToPose_GetResult_Event & ros_message,
  eprosima::fastcdr::Cdr &);

size_t
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
get_serialized_size_key(
  const ros2_robot_middleware::action::MoveToPose_GetResult_Event & ros_message,
  size_t current_alignment);

size_t
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
max_serialized_size_key_MoveToPose_GetResult_Event(
  bool & full_bounded,
  bool & is_plain,
  size_t current_alignment);

}  // namespace typesupport_fastrtps_cpp

}  // namespace action

}  // namespace ros2_robot_middleware

#ifdef __cplusplus
extern "C"
{
#endif

ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
const rosidl_message_type_support_t *
  ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_fastrtps_cpp, ros2_robot_middleware, action, MoveToPose_GetResult_Event)();

#ifdef __cplusplus
}
#endif

// already included above
// #include "rmw/types.h"
// already included above
// #include "rosidl_typesupport_cpp/service_type_support.hpp"
// already included above
// #include "rosidl_typesupport_interface/macros.h"
// already included above
// #include "ros2_robot_middleware/msg/rosidl_typesupport_fastrtps_cpp__visibility_control.h"

#ifdef __cplusplus
extern "C"
{
#endif

ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
const rosidl_service_type_support_t *
  ROSIDL_TYPESUPPORT_INTERFACE__SERVICE_SYMBOL_NAME(rosidl_typesupport_fastrtps_cpp, ros2_robot_middleware, action, MoveToPose_GetResult)();

#ifdef __cplusplus
}
#endif
// already included above
// #include <cstddef>
// already included above
// #include "rosidl_runtime_c/message_type_support_struct.h"
// already included above
// #include "rosidl_typesupport_interface/macros.h"
// already included above
// #include "ros2_robot_middleware/msg/rosidl_typesupport_fastrtps_cpp__visibility_control.h"
// already included above
// #include "ros2_robot_middleware/action/detail/move_to_pose__struct.hpp"

#ifndef _WIN32
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wunused-parameter"
# ifdef __clang__
#  pragma clang diagnostic ignored "-Wdeprecated-register"
#  pragma clang diagnostic ignored "-Wreturn-type-c-linkage"
# endif
#endif
#ifndef _WIN32
# pragma GCC diagnostic pop
#endif

// already included above
// #include "fastcdr/Cdr.h"

namespace ros2_robot_middleware
{

namespace action
{

namespace typesupport_fastrtps_cpp
{

bool
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
cdr_serialize(
  const ros2_robot_middleware::action::MoveToPose_FeedbackMessage & ros_message,
  eprosima::fastcdr::Cdr & cdr);

bool
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
cdr_deserialize(
  eprosima::fastcdr::Cdr & cdr,
  ros2_robot_middleware::action::MoveToPose_FeedbackMessage & ros_message);

size_t
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
get_serialized_size(
  const ros2_robot_middleware::action::MoveToPose_FeedbackMessage & ros_message,
  size_t current_alignment);

size_t
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
max_serialized_size_MoveToPose_FeedbackMessage(
  bool & full_bounded,
  bool & is_plain,
  size_t current_alignment);

bool
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
cdr_serialize_key(
  const ros2_robot_middleware::action::MoveToPose_FeedbackMessage & ros_message,
  eprosima::fastcdr::Cdr &);

size_t
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
get_serialized_size_key(
  const ros2_robot_middleware::action::MoveToPose_FeedbackMessage & ros_message,
  size_t current_alignment);

size_t
ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
max_serialized_size_key_MoveToPose_FeedbackMessage(
  bool & full_bounded,
  bool & is_plain,
  size_t current_alignment);

}  // namespace typesupport_fastrtps_cpp

}  // namespace action

}  // namespace ros2_robot_middleware

#ifdef __cplusplus
extern "C"
{
#endif

ROSIDL_TYPESUPPORT_FASTRTPS_CPP_PUBLIC_ros2_robot_middleware
const rosidl_message_type_support_t *
  ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_fastrtps_cpp, ros2_robot_middleware, action, MoveToPose_FeedbackMessage)();

#ifdef __cplusplus
}
#endif

#endif  // ROS2_ROBOT_MIDDLEWARE__ACTION__DETAIL__MOVE_TO_POSE__ROSIDL_TYPESUPPORT_FASTRTPS_CPP_HPP_
