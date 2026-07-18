// generated from rosidl_typesupport_introspection_cpp/resource/idl__type_support.cpp.em
// with input from ros2_robot_middleware:msg/HealthReport.idl
// generated code does not contain a copyright notice

#include "array"
#include "cstddef"
#include "string"
#include "vector"
#include "rosidl_runtime_c/message_type_support_struct.h"
#include "rosidl_typesupport_cpp/message_type_support.hpp"
#include "rosidl_typesupport_interface/macros.h"
#include "ros2_robot_middleware/msg/detail/health_report__functions.h"
#include "ros2_robot_middleware/msg/detail/health_report__struct.hpp"
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

void HealthReport_init_function(
  void * message_memory, rosidl_runtime_cpp::MessageInitialization _init)
{
  new (message_memory) ros2_robot_middleware::msg::HealthReport(_init);
}

void HealthReport_fini_function(void * message_memory)
{
  auto typed_message = static_cast<ros2_robot_middleware::msg::HealthReport *>(message_memory);
  typed_message->~HealthReport();
}

size_t size_function__HealthReport__nodes(const void * untyped_member)
{
  const auto * member = reinterpret_cast<const std::vector<ros2_robot_middleware::msg::HealthStatus> *>(untyped_member);
  return member->size();
}

const void * get_const_function__HealthReport__nodes(const void * untyped_member, size_t index)
{
  const auto & member =
    *reinterpret_cast<const std::vector<ros2_robot_middleware::msg::HealthStatus> *>(untyped_member);
  return &member[index];
}

void * get_function__HealthReport__nodes(void * untyped_member, size_t index)
{
  auto & member =
    *reinterpret_cast<std::vector<ros2_robot_middleware::msg::HealthStatus> *>(untyped_member);
  return &member[index];
}

void fetch_function__HealthReport__nodes(
  const void * untyped_member, size_t index, void * untyped_value)
{
  const auto & item = *reinterpret_cast<const ros2_robot_middleware::msg::HealthStatus *>(
    get_const_function__HealthReport__nodes(untyped_member, index));
  auto & value = *reinterpret_cast<ros2_robot_middleware::msg::HealthStatus *>(untyped_value);
  value = item;
}

void assign_function__HealthReport__nodes(
  void * untyped_member, size_t index, const void * untyped_value)
{
  auto & item = *reinterpret_cast<ros2_robot_middleware::msg::HealthStatus *>(
    get_function__HealthReport__nodes(untyped_member, index));
  const auto & value = *reinterpret_cast<const ros2_robot_middleware::msg::HealthStatus *>(untyped_value);
  item = value;
}

void resize_function__HealthReport__nodes(void * untyped_member, size_t size)
{
  auto * member =
    reinterpret_cast<std::vector<ros2_robot_middleware::msg::HealthStatus> *>(untyped_member);
  member->resize(size);
}

static const ::rosidl_typesupport_introspection_cpp::MessageMember HealthReport_message_member_array[2] = {
  {
    "header",  // name
    ::rosidl_typesupport_introspection_cpp::ROS_TYPE_MESSAGE,  // type
    0,  // upper bound of string
    ::rosidl_typesupport_introspection_cpp::get_message_type_support_handle<std_msgs::msg::Header>(),  // members of sub message
    false,  // is key
    false,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(ros2_robot_middleware::msg::HealthReport, header),  // bytes offset in struct
    nullptr,  // default value
    nullptr,  // size() function pointer
    nullptr,  // get_const(index) function pointer
    nullptr,  // get(index) function pointer
    nullptr,  // fetch(index, &value) function pointer
    nullptr,  // assign(index, value) function pointer
    nullptr  // resize(index) function pointer
  },
  {
    "nodes",  // name
    ::rosidl_typesupport_introspection_cpp::ROS_TYPE_MESSAGE,  // type
    0,  // upper bound of string
    ::rosidl_typesupport_introspection_cpp::get_message_type_support_handle<ros2_robot_middleware::msg::HealthStatus>(),  // members of sub message
    false,  // is key
    true,  // is array
    0,  // array size
    false,  // is upper bound
    offsetof(ros2_robot_middleware::msg::HealthReport, nodes),  // bytes offset in struct
    nullptr,  // default value
    size_function__HealthReport__nodes,  // size() function pointer
    get_const_function__HealthReport__nodes,  // get_const(index) function pointer
    get_function__HealthReport__nodes,  // get(index) function pointer
    fetch_function__HealthReport__nodes,  // fetch(index, &value) function pointer
    assign_function__HealthReport__nodes,  // assign(index, value) function pointer
    resize_function__HealthReport__nodes  // resize(index) function pointer
  }
};

static const ::rosidl_typesupport_introspection_cpp::MessageMembers HealthReport_message_members = {
  "ros2_robot_middleware::msg",  // message namespace
  "HealthReport",  // message name
  2,  // number of fields
  sizeof(ros2_robot_middleware::msg::HealthReport),
  false,  // has_any_key_member_
  HealthReport_message_member_array,  // message members
  HealthReport_init_function,  // function to initialize message memory (memory has to be allocated)
  HealthReport_fini_function  // function to terminate message instance (will not free memory)
};

static const rosidl_message_type_support_t HealthReport_message_type_support_handle = {
  ::rosidl_typesupport_introspection_cpp::typesupport_identifier,
  &HealthReport_message_members,
  get_message_typesupport_handle_function,
  &ros2_robot_middleware__msg__HealthReport__get_type_hash,
  &ros2_robot_middleware__msg__HealthReport__get_type_description,
  &ros2_robot_middleware__msg__HealthReport__get_type_description_sources,
};

}  // namespace rosidl_typesupport_introspection_cpp

}  // namespace msg

}  // namespace ros2_robot_middleware


namespace rosidl_typesupport_introspection_cpp
{

template<>
ROSIDL_TYPESUPPORT_INTROSPECTION_CPP_PUBLIC
const rosidl_message_type_support_t *
get_message_type_support_handle<ros2_robot_middleware::msg::HealthReport>()
{
  return &::ros2_robot_middleware::msg::rosidl_typesupport_introspection_cpp::HealthReport_message_type_support_handle;
}

}  // namespace rosidl_typesupport_introspection_cpp

#ifdef __cplusplus
extern "C"
{
#endif

ROSIDL_TYPESUPPORT_INTROSPECTION_CPP_PUBLIC
const rosidl_message_type_support_t *
ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME(rosidl_typesupport_introspection_cpp, ros2_robot_middleware, msg, HealthReport)() {
  return &::ros2_robot_middleware::msg::rosidl_typesupport_introspection_cpp::HealthReport_message_type_support_handle;
}

#ifdef __cplusplus
}
#endif
