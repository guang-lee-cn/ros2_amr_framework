// generated from rosidl_generator_cpp/resource/idl__traits.hpp.em
// with input from ros2_robot_middleware:msg/HealthReport.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "ros2_robot_middleware/msg/health_report.hpp"


#ifndef ROS2_ROBOT_MIDDLEWARE__MSG__DETAIL__HEALTH_REPORT__TRAITS_HPP_
#define ROS2_ROBOT_MIDDLEWARE__MSG__DETAIL__HEALTH_REPORT__TRAITS_HPP_

#include <stdint.h>

#include <sstream>
#include <string>
#include <type_traits>

#include "ros2_robot_middleware/msg/detail/health_report__struct.hpp"
#include "rosidl_runtime_cpp/traits.hpp"

// Include directives for member types
// Member 'header'
#include "std_msgs/msg/detail/header__traits.hpp"
// Member 'nodes'
#include "ros2_robot_middleware/msg/detail/health_status__traits.hpp"

namespace ros2_robot_middleware
{

namespace msg
{

inline void to_flow_style_yaml(
  const HealthReport & msg,
  std::ostream & out)
{
  out << "{";
  // member: header
  {
    out << "header: ";
    to_flow_style_yaml(msg.header, out);
    out << ", ";
  }

  // member: nodes
  {
    if (msg.nodes.size() == 0) {
      out << "nodes: []";
    } else {
      out << "nodes: [";
      size_t pending_items = msg.nodes.size();
      for (auto item : msg.nodes) {
        to_flow_style_yaml(item, out);
        if (--pending_items > 0) {
          out << ", ";
        }
      }
      out << "]";
    }
  }
  out << "}";
}  // NOLINT(readability/fn_size)

inline void to_block_style_yaml(
  const HealthReport & msg,
  std::ostream & out, size_t indentation = 0)
{
  // member: header
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "header:\n";
    to_block_style_yaml(msg.header, out, indentation + 2);
  }

  // member: nodes
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    if (msg.nodes.size() == 0) {
      out << "nodes: []\n";
    } else {
      out << "nodes:\n";
      for (auto item : msg.nodes) {
        if (indentation > 0) {
          out << std::string(indentation, ' ');
        }
        out << "-\n";
        to_block_style_yaml(item, out, indentation + 2);
      }
    }
  }
}  // NOLINT(readability/fn_size)

inline std::string to_yaml(const HealthReport & msg, bool use_flow_style = false)
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
  const ros2_robot_middleware::msg::HealthReport & msg,
  std::ostream & out, size_t indentation = 0)
{
  ros2_robot_middleware::msg::to_block_style_yaml(msg, out, indentation);
}

[[deprecated("use ros2_robot_middleware::msg::to_yaml() instead")]]
inline std::string to_yaml(const ros2_robot_middleware::msg::HealthReport & msg)
{
  return ros2_robot_middleware::msg::to_yaml(msg);
}

template<>
inline const char * data_type<ros2_robot_middleware::msg::HealthReport>()
{
  return "ros2_robot_middleware::msg::HealthReport";
}

template<>
inline const char * name<ros2_robot_middleware::msg::HealthReport>()
{
  return "ros2_robot_middleware/msg/HealthReport";
}

template<>
struct has_fixed_size<ros2_robot_middleware::msg::HealthReport>
  : std::integral_constant<bool, false> {};

template<>
struct has_bounded_size<ros2_robot_middleware::msg::HealthReport>
  : std::integral_constant<bool, false> {};

template<>
struct is_message<ros2_robot_middleware::msg::HealthReport>
  : std::true_type {};

}  // namespace rosidl_generator_traits

#endif  // ROS2_ROBOT_MIDDLEWARE__MSG__DETAIL__HEALTH_REPORT__TRAITS_HPP_
