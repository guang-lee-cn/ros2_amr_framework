// generated from rosidl_generator_cpp/resource/idl__builder.hpp.em
// with input from ros2_robot_middleware:msg/HealthReport.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "ros2_robot_middleware/msg/health_report.hpp"


#ifndef ROS2_ROBOT_MIDDLEWARE__MSG__DETAIL__HEALTH_REPORT__BUILDER_HPP_
#define ROS2_ROBOT_MIDDLEWARE__MSG__DETAIL__HEALTH_REPORT__BUILDER_HPP_

#include <algorithm>
#include <utility>

#include "ros2_robot_middleware/msg/detail/health_report__struct.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"


namespace ros2_robot_middleware
{

namespace msg
{

namespace builder
{

class Init_HealthReport_nodes
{
public:
  explicit Init_HealthReport_nodes(::ros2_robot_middleware::msg::HealthReport & msg)
  : msg_(msg)
  {}
  ::ros2_robot_middleware::msg::HealthReport nodes(::ros2_robot_middleware::msg::HealthReport::_nodes_type arg)
  {
    msg_.nodes = std::move(arg);
    return std::move(msg_);
  }

private:
  ::ros2_robot_middleware::msg::HealthReport msg_;
};

class Init_HealthReport_header
{
public:
  Init_HealthReport_header()
  : msg_(::rosidl_runtime_cpp::MessageInitialization::SKIP)
  {}
  Init_HealthReport_nodes header(::ros2_robot_middleware::msg::HealthReport::_header_type arg)
  {
    msg_.header = std::move(arg);
    return Init_HealthReport_nodes(msg_);
  }

private:
  ::ros2_robot_middleware::msg::HealthReport msg_;
};

}  // namespace builder

}  // namespace msg

template<typename MessageType>
auto build();

template<>
inline
auto build<::ros2_robot_middleware::msg::HealthReport>()
{
  return ros2_robot_middleware::msg::builder::Init_HealthReport_header();
}

}  // namespace ros2_robot_middleware

#endif  // ROS2_ROBOT_MIDDLEWARE__MSG__DETAIL__HEALTH_REPORT__BUILDER_HPP_
