// generated from rosidl_generator_cpp/resource/idl__builder.hpp.em
// with input from ros2_robot_middleware:msg/Object.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "ros2_robot_middleware/msg/object.hpp"


#ifndef ROS2_ROBOT_MIDDLEWARE__MSG__DETAIL__OBJECT__BUILDER_HPP_
#define ROS2_ROBOT_MIDDLEWARE__MSG__DETAIL__OBJECT__BUILDER_HPP_

#include <algorithm>
#include <utility>

#include "ros2_robot_middleware/msg/detail/object__struct.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"


namespace ros2_robot_middleware
{

namespace msg
{

namespace builder
{

class Init_Object_z
{
public:
  explicit Init_Object_z(::ros2_robot_middleware::msg::Object & msg)
  : msg_(msg)
  {}
  ::ros2_robot_middleware::msg::Object z(::ros2_robot_middleware::msg::Object::_z_type arg)
  {
    msg_.z = std::move(arg);
    return std::move(msg_);
  }

private:
  ::ros2_robot_middleware::msg::Object msg_;
};

class Init_Object_y
{
public:
  explicit Init_Object_y(::ros2_robot_middleware::msg::Object & msg)
  : msg_(msg)
  {}
  Init_Object_z y(::ros2_robot_middleware::msg::Object::_y_type arg)
  {
    msg_.y = std::move(arg);
    return Init_Object_z(msg_);
  }

private:
  ::ros2_robot_middleware::msg::Object msg_;
};

class Init_Object_x
{
public:
  explicit Init_Object_x(::ros2_robot_middleware::msg::Object & msg)
  : msg_(msg)
  {}
  Init_Object_y x(::ros2_robot_middleware::msg::Object::_x_type arg)
  {
    msg_.x = std::move(arg);
    return Init_Object_y(msg_);
  }

private:
  ::ros2_robot_middleware::msg::Object msg_;
};

class Init_Object_id
{
public:
  Init_Object_id()
  : msg_(::rosidl_runtime_cpp::MessageInitialization::SKIP)
  {}
  Init_Object_x id(::ros2_robot_middleware::msg::Object::_id_type arg)
  {
    msg_.id = std::move(arg);
    return Init_Object_x(msg_);
  }

private:
  ::ros2_robot_middleware::msg::Object msg_;
};

}  // namespace builder

}  // namespace msg

template<typename MessageType>
auto build();

template<>
inline
auto build<::ros2_robot_middleware::msg::Object>()
{
  return ros2_robot_middleware::msg::builder::Init_Object_id();
}

}  // namespace ros2_robot_middleware

#endif  // ROS2_ROBOT_MIDDLEWARE__MSG__DETAIL__OBJECT__BUILDER_HPP_
