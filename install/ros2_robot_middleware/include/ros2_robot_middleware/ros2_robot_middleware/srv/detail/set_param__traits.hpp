// generated from rosidl_generator_cpp/resource/idl__traits.hpp.em
// with input from ros2_robot_middleware:srv/SetParam.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "ros2_robot_middleware/srv/set_param.hpp"


#ifndef ROS2_ROBOT_MIDDLEWARE__SRV__DETAIL__SET_PARAM__TRAITS_HPP_
#define ROS2_ROBOT_MIDDLEWARE__SRV__DETAIL__SET_PARAM__TRAITS_HPP_

#include <stdint.h>

#include <sstream>
#include <string>
#include <type_traits>

#include "ros2_robot_middleware/srv/detail/set_param__struct.hpp"
#include "rosidl_runtime_cpp/traits.hpp"

namespace ros2_robot_middleware
{

namespace srv
{

inline void to_flow_style_yaml(
  const SetParam_Request & msg,
  std::ostream & out)
{
  out << "{";
  // member: param_name
  {
    out << "param_name: ";
    rosidl_generator_traits::value_to_yaml(msg.param_name, out);
    out << ", ";
  }

  // member: value
  {
    out << "value: ";
    rosidl_generator_traits::value_to_yaml(msg.value, out);
  }
  out << "}";
}  // NOLINT(readability/fn_size)

inline void to_block_style_yaml(
  const SetParam_Request & msg,
  std::ostream & out, size_t indentation = 0)
{
  // member: param_name
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "param_name: ";
    rosidl_generator_traits::value_to_yaml(msg.param_name, out);
    out << "\n";
  }

  // member: value
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "value: ";
    rosidl_generator_traits::value_to_yaml(msg.value, out);
    out << "\n";
  }
}  // NOLINT(readability/fn_size)

inline std::string to_yaml(const SetParam_Request & msg, bool use_flow_style = false)
{
  std::ostringstream out;
  if (use_flow_style) {
    to_flow_style_yaml(msg, out);
  } else {
    to_block_style_yaml(msg, out);
  }
  return out.str();
}

}  // namespace srv

}  // namespace ros2_robot_middleware

namespace rosidl_generator_traits
{

[[deprecated("use ros2_robot_middleware::srv::to_block_style_yaml() instead")]]
inline void to_yaml(
  const ros2_robot_middleware::srv::SetParam_Request & msg,
  std::ostream & out, size_t indentation = 0)
{
  ros2_robot_middleware::srv::to_block_style_yaml(msg, out, indentation);
}

[[deprecated("use ros2_robot_middleware::srv::to_yaml() instead")]]
inline std::string to_yaml(const ros2_robot_middleware::srv::SetParam_Request & msg)
{
  return ros2_robot_middleware::srv::to_yaml(msg);
}

template<>
inline const char * data_type<ros2_robot_middleware::srv::SetParam_Request>()
{
  return "ros2_robot_middleware::srv::SetParam_Request";
}

template<>
inline const char * name<ros2_robot_middleware::srv::SetParam_Request>()
{
  return "ros2_robot_middleware/srv/SetParam_Request";
}

template<>
struct has_fixed_size<ros2_robot_middleware::srv::SetParam_Request>
  : std::integral_constant<bool, false> {};

template<>
struct has_bounded_size<ros2_robot_middleware::srv::SetParam_Request>
  : std::integral_constant<bool, false> {};

template<>
struct is_message<ros2_robot_middleware::srv::SetParam_Request>
  : std::true_type {};

}  // namespace rosidl_generator_traits

namespace ros2_robot_middleware
{

namespace srv
{

inline void to_flow_style_yaml(
  const SetParam_Response & msg,
  std::ostream & out)
{
  out << "{";
  // member: success
  {
    out << "success: ";
    rosidl_generator_traits::value_to_yaml(msg.success, out);
    out << ", ";
  }

  // member: message
  {
    out << "message: ";
    rosidl_generator_traits::value_to_yaml(msg.message, out);
  }
  out << "}";
}  // NOLINT(readability/fn_size)

inline void to_block_style_yaml(
  const SetParam_Response & msg,
  std::ostream & out, size_t indentation = 0)
{
  // member: success
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "success: ";
    rosidl_generator_traits::value_to_yaml(msg.success, out);
    out << "\n";
  }

  // member: message
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "message: ";
    rosidl_generator_traits::value_to_yaml(msg.message, out);
    out << "\n";
  }
}  // NOLINT(readability/fn_size)

inline std::string to_yaml(const SetParam_Response & msg, bool use_flow_style = false)
{
  std::ostringstream out;
  if (use_flow_style) {
    to_flow_style_yaml(msg, out);
  } else {
    to_block_style_yaml(msg, out);
  }
  return out.str();
}

}  // namespace srv

}  // namespace ros2_robot_middleware

namespace rosidl_generator_traits
{

[[deprecated("use ros2_robot_middleware::srv::to_block_style_yaml() instead")]]
inline void to_yaml(
  const ros2_robot_middleware::srv::SetParam_Response & msg,
  std::ostream & out, size_t indentation = 0)
{
  ros2_robot_middleware::srv::to_block_style_yaml(msg, out, indentation);
}

[[deprecated("use ros2_robot_middleware::srv::to_yaml() instead")]]
inline std::string to_yaml(const ros2_robot_middleware::srv::SetParam_Response & msg)
{
  return ros2_robot_middleware::srv::to_yaml(msg);
}

template<>
inline const char * data_type<ros2_robot_middleware::srv::SetParam_Response>()
{
  return "ros2_robot_middleware::srv::SetParam_Response";
}

template<>
inline const char * name<ros2_robot_middleware::srv::SetParam_Response>()
{
  return "ros2_robot_middleware/srv/SetParam_Response";
}

template<>
struct has_fixed_size<ros2_robot_middleware::srv::SetParam_Response>
  : std::integral_constant<bool, false> {};

template<>
struct has_bounded_size<ros2_robot_middleware::srv::SetParam_Response>
  : std::integral_constant<bool, false> {};

template<>
struct is_message<ros2_robot_middleware::srv::SetParam_Response>
  : std::true_type {};

}  // namespace rosidl_generator_traits

// Include directives for member types
// Member 'info'
#include "service_msgs/msg/detail/service_event_info__traits.hpp"

namespace ros2_robot_middleware
{

namespace srv
{

inline void to_flow_style_yaml(
  const SetParam_Event & msg,
  std::ostream & out)
{
  out << "{";
  // member: info
  {
    out << "info: ";
    to_flow_style_yaml(msg.info, out);
    out << ", ";
  }

  // member: request
  {
    if (msg.request.size() == 0) {
      out << "request: []";
    } else {
      out << "request: [";
      size_t pending_items = msg.request.size();
      for (auto item : msg.request) {
        to_flow_style_yaml(item, out);
        if (--pending_items > 0) {
          out << ", ";
        }
      }
      out << "]";
    }
    out << ", ";
  }

  // member: response
  {
    if (msg.response.size() == 0) {
      out << "response: []";
    } else {
      out << "response: [";
      size_t pending_items = msg.response.size();
      for (auto item : msg.response) {
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
  const SetParam_Event & msg,
  std::ostream & out, size_t indentation = 0)
{
  // member: info
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "info:\n";
    to_block_style_yaml(msg.info, out, indentation + 2);
  }

  // member: request
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    if (msg.request.size() == 0) {
      out << "request: []\n";
    } else {
      out << "request:\n";
      for (auto item : msg.request) {
        if (indentation > 0) {
          out << std::string(indentation, ' ');
        }
        out << "-\n";
        to_block_style_yaml(item, out, indentation + 2);
      }
    }
  }

  // member: response
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    if (msg.response.size() == 0) {
      out << "response: []\n";
    } else {
      out << "response:\n";
      for (auto item : msg.response) {
        if (indentation > 0) {
          out << std::string(indentation, ' ');
        }
        out << "-\n";
        to_block_style_yaml(item, out, indentation + 2);
      }
    }
  }
}  // NOLINT(readability/fn_size)

inline std::string to_yaml(const SetParam_Event & msg, bool use_flow_style = false)
{
  std::ostringstream out;
  if (use_flow_style) {
    to_flow_style_yaml(msg, out);
  } else {
    to_block_style_yaml(msg, out);
  }
  return out.str();
}

}  // namespace srv

}  // namespace ros2_robot_middleware

namespace rosidl_generator_traits
{

[[deprecated("use ros2_robot_middleware::srv::to_block_style_yaml() instead")]]
inline void to_yaml(
  const ros2_robot_middleware::srv::SetParam_Event & msg,
  std::ostream & out, size_t indentation = 0)
{
  ros2_robot_middleware::srv::to_block_style_yaml(msg, out, indentation);
}

[[deprecated("use ros2_robot_middleware::srv::to_yaml() instead")]]
inline std::string to_yaml(const ros2_robot_middleware::srv::SetParam_Event & msg)
{
  return ros2_robot_middleware::srv::to_yaml(msg);
}

template<>
inline const char * data_type<ros2_robot_middleware::srv::SetParam_Event>()
{
  return "ros2_robot_middleware::srv::SetParam_Event";
}

template<>
inline const char * name<ros2_robot_middleware::srv::SetParam_Event>()
{
  return "ros2_robot_middleware/srv/SetParam_Event";
}

template<>
struct has_fixed_size<ros2_robot_middleware::srv::SetParam_Event>
  : std::integral_constant<bool, false> {};

template<>
struct has_bounded_size<ros2_robot_middleware::srv::SetParam_Event>
  : std::integral_constant<bool, has_bounded_size<ros2_robot_middleware::srv::SetParam_Request>::value && has_bounded_size<ros2_robot_middleware::srv::SetParam_Response>::value && has_bounded_size<service_msgs::msg::ServiceEventInfo>::value> {};

template<>
struct is_message<ros2_robot_middleware::srv::SetParam_Event>
  : std::true_type {};

}  // namespace rosidl_generator_traits

namespace rosidl_generator_traits
{

template<>
inline const char * data_type<ros2_robot_middleware::srv::SetParam>()
{
  return "ros2_robot_middleware::srv::SetParam";
}

template<>
inline const char * name<ros2_robot_middleware::srv::SetParam>()
{
  return "ros2_robot_middleware/srv/SetParam";
}

template<>
struct has_fixed_size<ros2_robot_middleware::srv::SetParam>
  : std::integral_constant<
    bool,
    has_fixed_size<ros2_robot_middleware::srv::SetParam_Request>::value &&
    has_fixed_size<ros2_robot_middleware::srv::SetParam_Response>::value
  >
{
};

template<>
struct has_bounded_size<ros2_robot_middleware::srv::SetParam>
  : std::integral_constant<
    bool,
    has_bounded_size<ros2_robot_middleware::srv::SetParam_Request>::value &&
    has_bounded_size<ros2_robot_middleware::srv::SetParam_Response>::value
  >
{
};

template<>
struct is_service<ros2_robot_middleware::srv::SetParam>
  : std::true_type
{
};

template<>
struct is_service_request<ros2_robot_middleware::srv::SetParam_Request>
  : std::true_type
{
};

template<>
struct is_service_response<ros2_robot_middleware::srv::SetParam_Response>
  : std::true_type
{
};

}  // namespace rosidl_generator_traits

#endif  // ROS2_ROBOT_MIDDLEWARE__SRV__DETAIL__SET_PARAM__TRAITS_HPP_
