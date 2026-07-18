// generated from rosidl_typesupport_introspection_cpp/resource/idl__type_support.cpp.em
// with input from ros2_robot_middleware:msg/HealthStatus.idl
// generated code does not contain a copyright notice

#include "array"
#include "cstddef"
#include "string"
#include "vector"
#include "rosidl_runtime_c/message_type_support_struct.h"
#include "rosidl_typesupport_cpp/message_type_support.hpp"
#include "rosidl_typesupport_interface/macros.h"
#include "ros2_robot_middleware/msg/detail/health_status__functions.h"
#include "ros2_robot_middleware/msg/detail/health_status__struct.hpp"
#include "rosidl_typesupport_introspection_cpp/field_types.hpp"
#include "rosidl_typesupport_introspection_cpp/identifier.hpp"
#include "rosidl_typesupport_introspection_cpp/message_introspection.hpp"
#include "rosidl_typesupport_introspection_cpp/message_type_support_decl.hpp"
#include "rosidl_typesupport_introspection_cpp/visibility_control.h"

namespace ros2_robot_middleware
{

namespace msg
{

namespace rosidl_typesupport_introspection_cpp
{

void HealthStatus_init_function(
  void * message_memory, rosidl_runtime_cpp::MessageInitialization _init)
{
  new (message_memory) ros2_robot_middleware::msg::HealthStatus(_init);
}

void HealthStatus_fini_function(void * message_memory)
{
  auto typed_message = static_cast<ros2_robot_middleware::msg::HealthStatus *>(message_memory);
  typed_message->~HealthStatus();
}

static const ::rosidl_typesupport_introspection_cpp::MessageMember HealthStatus_message_member_array[4] = {
  {
    "node_name",  // name
    ::rosidl_typesupport_introspection_cpp::ROS_TYPE_STRING,  // type
    0,  // upper bound of string
    nullptr,  // members of sub message
    false,  // is key
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(ros2_robot_middleware::msg::HealthStatus, node_name),  // bytes offset in struct
    nullptr,  // default value
    nullptr,  // size() function pointer
    nullptr,  // get_const(index) function pointer
    nullptr,  // get(index) function pointer
    nullptr,  // fetch(index, &value) function pointer
    nullptr,  // assign(index, value) function pointer
    nullptr  // resize(index) function pointer
  },
  {
    "status",  // name
    ::rosidl_typesupport_introspection_cpp::ROS_TYPE_STRING,  // type
    0,  // upper bound of string
    nullptr,  // members of sub message
    false,  // is key
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(ros2_robot_middleware::msg::HealthStatus, status),  // bytes offset in struct
    nullptr,  // default value
    nullptr,  // size() function pointer
    nullptr,  // get_const(index) function pointer
    nullptr,  // get(index) function pointer
    nullptr,  // fetch(index, &value) function pointer
    nullptr,  // assign(index, value) function pointer
    nullptr  // resize(index) function pointer
  },
  {
    "last_seen_s",  // name
    ::rosidl_typesupport_introspection_cpp::ROS_TYPE_DOUBLE,  // type
    0,  // upper bound of string
    nullptr,  // members of sub message
    false,  // is key
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(ros2_robot_middleware::msg::HealthStatus, last_seen_s),  // bytes offset in struct
    nullptr,  // default value
    nullptr,  // size() function pointer
    nullptr,  // get_const(index) function pointer
    nullptr,  // get(index) function pointer
    nullptr,  // fetch(index, &value) function pointer
    nullptr,  // assign(index, value) function pointer
    nullptr  // resize(index) function pointer
  },
  {
    "timeout_s",  // name
    ::rosidl_typesupport_introspection_cpp::ROS_TYPE_DOUBLE,  // type
    0,  // upper bound of string
    nullptr,  // members of sub message
    false,  // is key
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(ros2_robot_middleware::msg::HealthStatus, timeout_s),  // bytes offset in struct
    nullptr,  // default value
    nullptr,  // size() function pointer
    nullptr,  // get_const(index) function pointer
    nullptr,  // get(index) function pointer
    nullptr,  // fetch(index, &value) function pointer
    nullptr,  // assign(index, value) function pointer
    nullptr  // resize(index) function pointer
  }
};

static const ::rosidl_typesupport_introspection_cpp::MessageMembers HealthStatus_message_members = {
  "ros2_robot_middleware::msg",  // message namespace
  "HealthStatus",  // message name
  4,  // number of fields
  sizeof(ros2_robot_middleware::msg::HealthStatus),
  false,  // has_any_key_member_
  HealthStatus_message_member_array,  // message members
  HealthStatus_init_function,  // function to initialize message memory (memory has to be allocated)
  HealthStatus_fini_function  // function to terminate message instance (will not free memory)
};

static const rosidl_message_type_support_t HealthStatus_message_type_support_handle = {
  ::rosidl_typesupport_introspection_cpp::typesupport_identifier,
  &HealthStatus_message_members,
  get_message_typesupport_handle_function,
  &ros2_robot_middleware__msg__HealthStatus__get_type_hash,
  &ros2_robot_middleware__msg__HealthStatus__get_type_description,
  &ros2_robot_middleware__msg__HealthStatus__get_type_description_sources,
};

}  // namespace rosidl_typesupport_introspection_cpp

}  // namespace msg

}  // namespace ros2_robot_middleware


namespace rosidl_typesupport_introspection_cpp
{

template<>
ROSIDL_TYPESUPPORT_INTROSPECTION_CPP_PUBLIC
const rosidl_message_type_support_t *
get_message_type_support_handle<ros2_robot_middleware::msg::HealthStatus>()
{
  return &::ros2_robot_middleware::msg::rosidl_typesupport_introspection_cpp::HealthStatus_message_type_support_handle;
}

}  // namespace rosidl_typesupport_introspection_cpp

#ifdef __cplusplus
extern "C"
{
#endif

ROSIDL_TYPESUPPORT_INTROSPECTION_CPP_PUBLIC
const rosidl_message_type_support_t *
ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_introspection_cpp, ros2_robot_middleware, msg, HealthStatus)() {
  return &::ros2_robot_middleware::msg::rosidl_typesupport_introspection_cpp::HealthStatus_message_type_support_handle;
}

#ifdef __cplusplus
}
#endif
