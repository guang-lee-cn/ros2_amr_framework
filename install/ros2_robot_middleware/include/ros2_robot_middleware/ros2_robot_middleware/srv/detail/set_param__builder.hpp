// generated from rosidl_generator_cpp/resource/idl__builder.hpp.em
// with input from ros2_robot_middleware:srv/SetParam.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "ros2_robot_middleware/srv/set_param.hpp"


#ifndef ROS2_ROBOT_MIDDLEWARE__SRV__DETAIL__SET_PARAM__BUILDER_HPP_
#define ROS2_ROBOT_MIDDLEWARE__SRV__DETAIL__SET_PARAM__BUILDER_HPP_

#include <algorithm>
#include <utility>

#include "ros2_robot_middleware/srv/detail/set_param__struct.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"


namespace ros2_robot_middleware
{

namespace srv
{

namespace builder
{

class Init_SetParam_Request_value
{
public:
  explicit Init_SetParam_Request_value(::ros2_robot_middleware::srv::SetParam_Request & msg)
  : msg_(msg)
  {}
  ::ros2_robot_middleware::srv::SetParam_Request value(::ros2_robot_middleware::srv::SetParam_Request::_value_type arg)
  {
    msg_.value = std::move(arg);
    return std::move(msg_);
  }

private:
  ::ros2_robot_middleware::srv::SetParam_Request msg_;
};

class Init_SetParam_Request_param_name
{
public:
  Init_SetParam_Request_param_name()
  : msg_(::rosidl_runtime_cpp::MessageInitialization::SKIP)
  {}
  Init_SetParam_Request_value param_name(::ros2_robot_middleware::srv::SetParam_Request::_param_name_type arg)
  {
    msg_.param_name = std::move(arg);
    return Init_SetParam_Request_value(msg_);
  }

private:
  ::ros2_robot_middleware::srv::SetParam_Request msg_;
};

}  // namespace builder

}  // namespace srv

template<typename MessageType>
auto build();

template<>
inline
auto build<::ros2_robot_middleware::srv::SetParam_Request>()
{
  return ros2_robot_middleware::srv::builder::Init_SetParam_Request_param_name();
}

}  // namespace ros2_robot_middleware


namespace ros2_robot_middleware
{

namespace srv
{

namespace builder
{

class Init_SetParam_Response_message
{
public:
  explicit Init_SetParam_Response_message(::ros2_robot_middleware::srv::SetParam_Response & msg)
  : msg_(msg)
  {}
  ::ros2_robot_middleware::srv::SetParam_Response message(::ros2_robot_middleware::srv::SetParam_Response::_message_type arg)
  {
    msg_.message = std::move(arg);
    return std::move(msg_);
  }

private:
  ::ros2_robot_middleware::srv::SetParam_Response msg_;
};

class Init_SetParam_Response_success
{
public:
  Init_SetParam_Response_success()
  : msg_(::rosidl_runtime_cpp::MessageInitialization::SKIP)
  {}
  Init_SetParam_Response_message success(::ros2_robot_middleware::srv::SetParam_Response::_success_type arg)
  {
    msg_.success = std::move(arg);
    return Init_SetParam_Response_message(msg_);
  }

private:
  ::ros2_robot_middleware::srv::SetParam_Response msg_;
};

}  // namespace builder

}  // namespace srv

template<typename MessageType>
auto build();

template<>
inline
auto build<::ros2_robot_middleware::srv::SetParam_Response>()
{
  return ros2_robot_middleware::srv::builder::Init_SetParam_Response_success();
}

}  // namespace ros2_robot_middleware


namespace ros2_robot_middleware
{

namespace srv
{

namespace builder
{

class Init_SetParam_Event_response
{
public:
  explicit Init_SetParam_Event_response(::ros2_robot_middleware::srv::SetParam_Event & msg)
  : msg_(msg)
  {}
  ::ros2_robot_middleware::srv::SetParam_Event response(::ros2_robot_middleware::srv::SetParam_Event::_response_type arg)
  {
    msg_.response = std::move(arg);
    return std::move(msg_);
  }

private:
  ::ros2_robot_middleware::srv::SetParam_Event msg_;
};

class Init_SetParam_Event_request
{
public:
  explicit Init_SetParam_Event_request(::ros2_robot_middleware::srv::SetParam_Event & msg)
  : msg_(msg)
  {}
  Init_SetParam_Event_response request(::ros2_robot_middleware::srv::SetParam_Event::_request_type arg)
  {
    msg_.request = std::move(arg);
    return Init_SetParam_Event_response(msg_);
  }

private:
  ::ros2_robot_middleware::srv::SetParam_Event msg_;
};

class Init_SetParam_Event_info
{
public:
  Init_SetParam_Event_info()
  : msg_(::rosidl_runtime_cpp::MessageInitialization::SKIP)
  {}
  Init_SetParam_Event_request info(::ros2_robot_middleware::srv::SetParam_Event::_info_type arg)
  {
    msg_.info = std::move(arg);
    return Init_SetParam_Event_request(msg_);
  }

private:
  ::ros2_robot_middleware::srv::SetParam_Event msg_;
};

}  // namespace builder

}  // namespace srv

template<typename MessageType>
auto build();

template<>
inline
auto build<::ros2_robot_middleware::srv::SetParam_Event>()
{
  return ros2_robot_middleware::srv::builder::Init_SetParam_Event_info();
}

}  // namespace ros2_robot_middleware

#endif  // ROS2_ROBOT_MIDDLEWARE__SRV__DETAIL__SET_PARAM__BUILDER_HPP_
