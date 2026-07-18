// generated from rosidl_generator_cpp/resource/idl__builder.hpp.em
// with input from ros2_robot_middleware:msg/HealthStatus.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "ros2_robot_middleware/msg/health_status.hpp"


#ifndef ROS2_ROBOT_MIDDLEWARE__MSG__DETAIL__HEALTH_STATUS__BUILDER_HPP_
#define ROS2_ROBOT_MIDDLEWARE__MSG__DETAIL__HEALTH_STATUS__BUILDER_HPP_

#include <algorithm>
#include <utility>

#include "ros2_robot_middleware/msg/detail/health_status__struct.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"


namespace ros2_robot_middleware
{

namespace msg
{

namespace builder
{

class Init_HealthStatus_timeout_s
{
public:
  explicit Init_HealthStatus_timeout_s(::ros2_robot_middleware::msg::HealthStatus & msg)
  : msg_(msg)
  {}
  ::ros2_robot_middleware::msg::HealthStatus timeout_s(::ros2_robot_middleware::msg::HealthStatus::_timeout_s_type arg)
  {
    msg_.timeout_s = std::move(arg);
    return std::move(msg_);
  }

private:
  ::ros2_robot_middleware::msg::HealthStatus msg_;
};

class Init_HealthStatus_last_seen_s
{
public:
  explicit Init_HealthStatus_last_seen_s(::ros2_robot_middleware::msg::HealthStatus & msg)
  : msg_(msg)
  {}
  Init_HealthStatus_timeout_s last_seen_s(::ros2_robot_middleware::msg::HealthStatus::_last_seen_s_type arg)
  {
    msg_.last_seen_s = std::move(arg);
    return Init_HealthStatus_timeout_s(msg_);
  }

private:
  ::ros2_robot_middleware::msg::HealthStatus msg_;
};

class Init_HealthStatus_status
{
public:
  explicit Init_HealthStatus_status(::ros2_robot_middleware::msg::HealthStatus & msg)
  : msg_(msg)
  {}
  Init_HealthStatus_last_seen_s status(::ros2_robot_middleware::msg::HealthStatus::_status_type arg)
  {
    msg_.status = std::move(arg);
    return Init_HealthStatus_last_seen_s(msg_);
  }

private:
  ::ros2_robot_middleware::msg::HealthStatus msg_;
};

class Init_HealthStatus_node_name
{
public:
  Init_HealthStatus_node_name()
  : msg_(::rosidl_runtime_cpp::MessageInitialization::SKIP)
  {}
  Init_HealthStatus_status node_name(::ros2_robot_middleware::msg::HealthStatus::_node_name_type arg)
  {
    msg_.node_name = std::move(arg);
    return Init_HealthStatus_status(msg_);
  }

private:
  ::ros2_robot_middleware::msg::HealthStatus msg_;
};

}  // namespace builder

}  // namespace msg

template<typename MessageType>
auto build();

template<>
inline
auto build<::ros2_robot_middleware::msg::HealthStatus>()
{
  return ros2_robot_middleware::msg::builder::Init_HealthStatus_node_name();
}

}  // namespace ros2_robot_middleware

#endif  // ROS2_ROBOT_MIDDLEWARE__MSG__DETAIL__HEALTH_STATUS__BUILDER_HPP_
