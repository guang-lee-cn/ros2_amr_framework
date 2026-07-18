// generated from rosidl_generator_cpp/resource/idl__traits.hpp.em
// with input from ros2_robot_middleware:msg/Object.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "ros2_robot_middleware/msg/object.hpp"


#ifndef ROS2_ROBOT_MIDDLEWARE__MSG__DETAIL__OBJECT__TRAITS_HPP_
#define ROS2_ROBOT_MIDDLEWARE__MSG__DETAIL__OBJECT__TRAITS_HPP_

#include <stdint.h>

#include <sstream>
#include <string>
#include <type_traits>

#include "ros2_robot_middleware/msg/detail/object__struct.hpp"
#include "rosidl_runtime_cpp/traits.hpp"

namespace ros2_robot_middleware
{

namespace msg
{

inline void to_flow_style_yaml(
  const Object & msg,
  std::ostream & out)
{
  out << "{";
  // member: id
  {
    out << "id: ";
    rosidl_generator_traits::value_to_yaml(msg.id, out);
    out << ", ";
  }

  // member: x
  {
    out << "x: ";
    rosidl_generator_traits::value_to_yaml(msg.x, out);
    out << ", ";
  }

  // member: y
  {
    out << "y: ";
    rosidl_generator_traits::value_to_yaml(msg.y, out);
    out << ", ";
  }

  // member: z
  {
    out << "z: ";
    rosidl_generator_traits::value_to_yaml(msg.z, out);
  }
  out << "}";
}  // NOLINT(readability/fn_size)

inline void to_block_style_yaml(
  const Object & msg,
  std::ostream & out, size_t indentation = 0)
{
  // member: id
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "id: ";
    rosidl_generator_traits::value_to_yaml(msg.id, out);
    out << "\n";
  }

  // member: x
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "x: ";
    rosidl_generator_traits::value_to_yaml(msg.x, out);
    out << "\n";
  }

  // member: y
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "y: ";
    rosidl_generator_traits::value_to_yaml(msg.y, out);
    out << "\n";
  }

  // member: z
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "z: ";
    rosidl_generator_traits::value_to_yaml(msg.z, out);
    out << "\n";
  }
}  // NOLINT(readability/fn_size)

inline std::string to_yaml(const Object & msg, bool use_flow_style = false)
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
  const ros2_robot_middleware::msg::Object & msg,
  std::ostream & out, size_t indentation = 0)
{
  ros2_robot_middleware::msg::to_block_style_yaml(msg, out, indentation);
}

[[deprecated("use ros2_robot_middleware::msg::to_yaml() instead")]]
inline std::string to_yaml(const ros2_robot_middleware::msg::Object & msg)
{
  return ros2_robot_middleware::msg::to_yaml(msg);
}

template<>
inline const char * data_type<ros2_robot_middleware::msg::Object>()
{
  return "ros2_robot_middleware::msg::Object";
}

template<>
inline const char * name<ros2_robot_middleware::msg::Object>()
{
  return "ros2_robot_middleware/msg/Object";
}

template<>
struct has_fixed_size<ros2_robot_middleware::msg::Object>
  : std::integral_constant<bool, false> {};

template<>
struct has_bounded_size<ros2_robot_middleware::msg::Object>
  : std::integral_constant<bool, false> {};

template<>
struct is_message<ros2_robot_middleware::msg::Object>
  : std::true_type {};

}  // namespace rosidl_generator_traits

#endif  // ROS2_ROBOT_MIDDLEWARE__MSG__DETAIL__OBJECT__TRAITS_HPP_
