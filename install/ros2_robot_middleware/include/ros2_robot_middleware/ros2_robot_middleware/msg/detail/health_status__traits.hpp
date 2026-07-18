// generated from rosidl_generator_cpp/resource/idl__traits.hpp.em
// with input from ros2_robot_middleware:msg/HealthStatus.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "ros2_robot_middleware/msg/health_status.hpp"


#ifndef ROS2_ROBOT_MIDDLEWARE__MSG__DETAIL__HEALTH_STATUS__TRAITS_HPP_
#define ROS2_ROBOT_MIDDLEWARE__MSG__DETAIL__HEALTH_STATUS__TRAITS_HPP_

#include <stdint.h>

#include <sstream>
#include <string>
#include <type_traits>

#include "ros2_robot_middleware/msg/detail/health_status__struct.hpp"
#include "rosidl_runtime_cpp/traits.hpp"

namespace ros2_robot_middleware
{

namespace msg
{

inline void to_flow_style_yaml(
  const HealthStatus & msg,
  std::ostream & out)
{
  out << "{";
  // member: node_name
  {
    out << "node_name: ";
    rosidl_generator_traits::value_to_yaml(msg.node_name, out);
    out << ", ";
  }

  // member: status
  {
    out << "status: ";
    rosidl_generator_traits::value_to_yaml(msg.status, out);
    out << ", ";
  }

  // member: last_seen_s
  {
    out << "last_seen_s: ";
    rosidl_generator_traits::value_to_yaml(msg.last_seen_s, out);
    out << ", ";
  }

  // member: timeout_s
  {
    out << "timeout_s: ";
    rosidl_generator_traits::value_to_yaml(msg.timeout_s, out);
  }
  out << "}";
}  // NOLINT(readability/fn_size)

inline void to_block_style_yaml(
  const HealthStatus & msg,
  std::ostream & out, size_t indentation = 0)
{
  // member: node_name
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "node_name: ";
    rosidl_generator_traits::value_to_yaml(msg.node_name, out);
    out << "\n";
  }

  // member: status
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "status: ";
    rosidl_generator_traits::value_to_yaml(msg.status, out);
    out << "\n";
  }

  // member: last_seen_s
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "last_seen_s: ";
    rosidl_generator_traits::value_to_yaml(msg.last_seen_s, out);
    out << "\n";
  }

  // member: timeout_s
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "timeout_s: ";
    rosidl_generator_traits::value_to_yaml(msg.timeout_s, out);
    out << "\n";
  }
}  // NOLINT(readability/fn_size)

inline std::string to_yaml(const HealthStatus & msg, bool use_flow_style = false)
{
  std::ostringstream out;
  if (use_flow_style) {
    to_flow_style_yaml(msg, out);
  } else {
    to_block_style_yaml(msg, out);
  }
  return out.str();
}

}  // namespace msg

}  // namespace ros2_robot_middleware

namespace rosidl_generator_traits
{

[[deprecated("use ros2_robot_middleware::msg::to_block_style_yaml() instead")]]
inline void to_yaml(
  const ros2_robot_middleware::msg::HealthStatus & msg,
  std::ostream & out, size_t indentation = 0)
{
  ros2_robot_middleware::msg::to_block_style_yaml(msg, out, indentation);
}

[[deprecated("use ros2_robot_middleware::msg::to_yaml() instead")]]
inline std::string to_yaml(const ros2_robot_middleware::msg::HealthStatus & msg)
{
  return ros2_robot_middleware::msg::to_yaml(msg);
}

template<>
inline const char * data_type<ros2_robot_middleware::msg::HealthStatus>()
{
  return "ros2_robot_middleware::msg::HealthStatus";
}

template<>
inline const char * name<ros2_robot_middleware::msg::HealthStatus>()
{
  return "ros2_robot_middleware/msg/HealthStatus";
}

template<>
struct has_fixed_size<ros2_robot_middleware::msg::HealthStatus>
  : std::integral_constant<bool, false> {};

template<>
struct has_bounded_size<ros2_robot_middleware::msg::HealthStatus>
  : std::integral_constant<bool, false> {};

template<>
struct is_message<ros2_robot_middleware::msg::HealthStatus>
  : std::true_type {};

}  // namespace rosidl_generator_traits

#endif  // ROS2_ROBOT_MIDDLEWARE__MSG__DETAIL__HEALTH_STATUS__TRAITS_HPP_
