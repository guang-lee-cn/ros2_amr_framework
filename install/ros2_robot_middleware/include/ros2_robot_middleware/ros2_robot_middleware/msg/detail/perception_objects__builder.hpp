// generated from rosidl_generator_cpp/resource/idl__builder.hpp.em
// with input from ros2_robot_middleware:msg/PerceptionObjects.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "ros2_robot_middleware/msg/perception_objects.hpp"


#ifndef ROS2_ROBOT_MIDDLEWARE__MSG__DETAIL__PERCEPTION_OBJECTS__BUILDER_HPP_
#define ROS2_ROBOT_MIDDLEWARE__MSG__DETAIL__PERCEPTION_OBJECTS__BUILDER_HPP_

#include <algorithm>
#include <utility>

#include "ros2_robot_middleware/msg/detail/perception_objects__struct.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"


namespace ros2_robot_middleware
{

namespace msg
{

namespace builder
{

class Init_PerceptionObjects_objects
{
public:
  explicit Init_PerceptionObjects_objects(::ros2_robot_middleware::msg::PerceptionObjects & msg)
  : msg_(msg)
  {}
  ::ros2_robot_middleware::msg::PerceptionObjects objects(::ros2_robot_middleware::msg::PerceptionObjects::_objects_type arg)
  {
    msg_.objects = std::move(arg);
    return std::move(msg_);
  }

private:
  ::ros2_robot_middleware::msg::PerceptionObjects msg_;
};

class Init_PerceptionObjects_header
{
public:
  Init_PerceptionObjects_header()
  : msg_(::rosidl_runtime_cpp::MessageInitialization::SKIP)
  {}
  Init_PerceptionObjects_objects header(::ros2_robot_middleware::msg::PerceptionObjects::_header_type arg)
  {
    msg_.header = std::move(arg);
    return Init_PerceptionObjects_objects(msg_);
  }

private:
  ::ros2_robot_middleware::msg::PerceptionObjects msg_;
};

}  // namespace builder

}  // namespace msg

template<typename MessageType>
auto build();

template<>
inline
auto build<::ros2_robot_middleware::msg::PerceptionObjects>()
{
  return ros2_robot_middleware::msg::builder::Init_PerceptionObjects_header();
}

}  // namespace ros2_robot_middleware

#endif  // ROS2_ROBOT_MIDDLEWARE__MSG__DETAIL__PERCEPTION_OBJECTS__BUILDER_HPP_
