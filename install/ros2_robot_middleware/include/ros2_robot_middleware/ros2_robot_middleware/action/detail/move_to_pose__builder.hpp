// generated from rosidl_generator_cpp/resource/idl__builder.hpp.em
// with input from ros2_robot_middleware:action/MoveToPose.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "ros2_robot_middleware/action/move_to_pose.hpp"


#ifndef ROS2_ROBOT_MIDDLEWARE__ACTION__DETAIL__MOVE_TO_POSE__BUILDER_HPP_
#define ROS2_ROBOT_MIDDLEWARE__ACTION__DETAIL__MOVE_TO_POSE__BUILDER_HPP_

#include <algorithm>
#include <utility>

#include "ros2_robot_middleware/action/detail/move_to_pose__struct.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"


namespace ros2_robot_middleware
{

namespace action
{

namespace builder
{

class Init_MoveToPose_Goal_max_speed
{
public:
  explicit Init_MoveToPose_Goal_max_speed(::ros2_robot_middleware::action::MoveToPose_Goal & msg)
  : msg_(msg)
  {}
  ::ros2_robot_middleware::action::MoveToPose_Goal max_speed(::ros2_robot_middleware::action::MoveToPose_Goal::_max_speed_type arg)
  {
    msg_.max_speed = std::move(arg);
    return std::move(msg_);
  }

private:
  ::ros2_robot_middleware::action::MoveToPose_Goal msg_;
};

class Init_MoveToPose_Goal_target_theta
{
public:
  explicit Init_MoveToPose_Goal_target_theta(::ros2_robot_middleware::action::MoveToPose_Goal & msg)
  : msg_(msg)
  {}
  Init_MoveToPose_Goal_max_speed target_theta(::ros2_robot_middleware::action::MoveToPose_Goal::_target_theta_type arg)
  {
    msg_.target_theta = std::move(arg);
    return Init_MoveToPose_Goal_max_speed(msg_);
  }

private:
  ::ros2_robot_middleware::action::MoveToPose_Goal msg_;
};

class Init_MoveToPose_Goal_target_y
{
public:
  explicit Init_MoveToPose_Goal_target_y(::ros2_robot_middleware::action::MoveToPose_Goal & msg)
  : msg_(msg)
  {}
  Init_MoveToPose_Goal_target_theta target_y(::ros2_robot_middleware::action::MoveToPose_Goal::_target_y_type arg)
  {
    msg_.target_y = std::move(arg);
    return Init_MoveToPose_Goal_target_theta(msg_);
  }

private:
  ::ros2_robot_middleware::action::MoveToPose_Goal msg_;
};

class Init_MoveToPose_Goal_target_x
{
public:
  Init_MoveToPose_Goal_target_x()
  : msg_(::rosidl_runtime_cpp::MessageInitialization::SKIP)
  {}
  Init_MoveToPose_Goal_target_y target_x(::ros2_robot_middleware::action::MoveToPose_Goal::_target_x_type arg)
  {
    msg_.target_x = std::move(arg);
    return Init_MoveToPose_Goal_target_y(msg_);
  }

private:
  ::ros2_robot_middleware::action::MoveToPose_Goal msg_;
};

}  // namespace builder

}  // namespace action

template<typename MessageType>
auto build();

template<>
inline
auto build<::ros2_robot_middleware::action::MoveToPose_Goal>()
{
  return ros2_robot_middleware::action::builder::Init_MoveToPose_Goal_target_x();
}

}  // namespace ros2_robot_middleware


namespace ros2_robot_middleware
{

namespace action
{

namespace builder
{

class Init_MoveToPose_Result_elapsed_time
{
public:
  explicit Init_MoveToPose_Result_elapsed_time(::ros2_robot_middleware::action::MoveToPose_Result & msg)
  : msg_(msg)
  {}
  ::ros2_robot_middleware::action::MoveToPose_Result elapsed_time(::ros2_robot_middleware::action::MoveToPose_Result::_elapsed_time_type arg)
  {
    msg_.elapsed_time = std::move(arg);
    return std::move(msg_);
  }

private:
  ::ros2_robot_middleware::action::MoveToPose_Result msg_;
};

class Init_MoveToPose_Result_final_y
{
public:
  explicit Init_MoveToPose_Result_final_y(::ros2_robot_middleware::action::MoveToPose_Result & msg)
  : msg_(msg)
  {}
  Init_MoveToPose_Result_elapsed_time final_y(::ros2_robot_middleware::action::MoveToPose_Result::_final_y_type arg)
  {
    msg_.final_y = std::move(arg);
    return Init_MoveToPose_Result_elapsed_time(msg_);
  }

private:
  ::ros2_robot_middleware::action::MoveToPose_Result msg_;
};

class Init_MoveToPose_Result_final_x
{
public:
  explicit Init_MoveToPose_Result_final_x(::ros2_robot_middleware::action::MoveToPose_Result & msg)
  : msg_(msg)
  {}
  Init_MoveToPose_Result_final_y final_x(::ros2_robot_middleware::action::MoveToPose_Result::_final_x_type arg)
  {
    msg_.final_x = std::move(arg);
    return Init_MoveToPose_Result_final_y(msg_);
  }

private:
  ::ros2_robot_middleware::action::MoveToPose_Result msg_;
};

class Init_MoveToPose_Result_reached
{
public:
  Init_MoveToPose_Result_reached()
  : msg_(::rosidl_runtime_cpp::MessageInitialization::SKIP)
  {}
  Init_MoveToPose_Result_final_x reached(::ros2_robot_middleware::action::MoveToPose_Result::_reached_type arg)
  {
    msg_.reached = std::move(arg);
    return Init_MoveToPose_Result_final_x(msg_);
  }

private:
  ::ros2_robot_middleware::action::MoveToPose_Result msg_;
};

}  // namespace builder

}  // namespace action

template<typename MessageType>
auto build();

template<>
inline
auto build<::ros2_robot_middleware::action::MoveToPose_Result>()
{
  return ros2_robot_middleware::action::builder::Init_MoveToPose_Result_reached();
}

}  // namespace ros2_robot_middleware


namespace ros2_robot_middleware
{

namespace action
{

namespace builder
{

class Init_MoveToPose_Feedback_percent_complete
{
public:
  explicit Init_MoveToPose_Feedback_percent_complete(::ros2_robot_middleware::action::MoveToPose_Feedback & msg)
  : msg_(msg)
  {}
  ::ros2_robot_middleware::action::MoveToPose_Feedback percent_complete(::ros2_robot_middleware::action::MoveToPose_Feedback::_percent_complete_type arg)
  {
    msg_.percent_complete = std::move(arg);
    return std::move(msg_);
  }

private:
  ::ros2_robot_middleware::action::MoveToPose_Feedback msg_;
};

class Init_MoveToPose_Feedback_distance_remaining
{
public:
  explicit Init_MoveToPose_Feedback_distance_remaining(::ros2_robot_middleware::action::MoveToPose_Feedback & msg)
  : msg_(msg)
  {}
  Init_MoveToPose_Feedback_percent_complete distance_remaining(::ros2_robot_middleware::action::MoveToPose_Feedback::_distance_remaining_type arg)
  {
    msg_.distance_remaining = std::move(arg);
    return Init_MoveToPose_Feedback_percent_complete(msg_);
  }

private:
  ::ros2_robot_middleware::action::MoveToPose_Feedback msg_;
};

class Init_MoveToPose_Feedback_current_y
{
public:
  explicit Init_MoveToPose_Feedback_current_y(::ros2_robot_middleware::action::MoveToPose_Feedback & msg)
  : msg_(msg)
  {}
  Init_MoveToPose_Feedback_distance_remaining current_y(::ros2_robot_middleware::action::MoveToPose_Feedback::_current_y_type arg)
  {
    msg_.current_y = std::move(arg);
    return Init_MoveToPose_Feedback_distance_remaining(msg_);
  }

private:
  ::ros2_robot_middleware::action::MoveToPose_Feedback msg_;
};

class Init_MoveToPose_Feedback_current_x
{
public:
  Init_MoveToPose_Feedback_current_x()
  : msg_(::rosidl_runtime_cpp::MessageInitialization::SKIP)
  {}
  Init_MoveToPose_Feedback_current_y current_x(::ros2_robot_middleware::action::MoveToPose_Feedback::_current_x_type arg)
  {
    msg_.current_x = std::move(arg);
    return Init_MoveToPose_Feedback_current_y(msg_);
  }

private:
  ::ros2_robot_middleware::action::MoveToPose_Feedback msg_;
};

}  // namespace builder

}  // namespace action

template<typename MessageType>
auto build();

template<>
inline
auto build<::ros2_robot_middleware::action::MoveToPose_Feedback>()
{
  return ros2_robot_middleware::action::builder::Init_MoveToPose_Feedback_current_x();
}

}  // namespace ros2_robot_middleware


namespace ros2_robot_middleware
{

namespace action
{

namespace builder
{

class Init_MoveToPose_SendGoal_Request_goal
{
public:
  explicit Init_MoveToPose_SendGoal_Request_goal(::ros2_robot_middleware::action::MoveToPose_SendGoal_Request & msg)
  : msg_(msg)
  {}
  ::ros2_robot_middleware::action::MoveToPose_SendGoal_Request goal(::ros2_robot_middleware::action::MoveToPose_SendGoal_Request::_goal_type arg)
  {
    msg_.goal = std::move(arg);
    return std::move(msg_);
  }

private:
  ::ros2_robot_middleware::action::MoveToPose_SendGoal_Request msg_;
};

class Init_MoveToPose_SendGoal_Request_goal_id
{
public:
  Init_MoveToPose_SendGoal_Request_goal_id()
  : msg_(::rosidl_runtime_cpp::MessageInitialization::SKIP)
  {}
  Init_MoveToPose_SendGoal_Request_goal goal_id(::ros2_robot_middleware::action::MoveToPose_SendGoal_Request::_goal_id_type arg)
  {
    msg_.goal_id = std::move(arg);
    return Init_MoveToPose_SendGoal_Request_goal(msg_);
  }

private:
  ::ros2_robot_middleware::action::MoveToPose_SendGoal_Request msg_;
};

}  // namespace builder

}  // namespace action

template<typename MessageType>
auto build();

template<>
inline
auto build<::ros2_robot_middleware::action::MoveToPose_SendGoal_Request>()
{
  return ros2_robot_middleware::action::builder::Init_MoveToPose_SendGoal_Request_goal_id();
}

}  // namespace ros2_robot_middleware


namespace ros2_robot_middleware
{

namespace action
{

namespace builder
{

class Init_MoveToPose_SendGoal_Response_stamp
{
public:
  explicit Init_MoveToPose_SendGoal_Response_stamp(::ros2_robot_middleware::action::MoveToPose_SendGoal_Response & msg)
  : msg_(msg)
  {}
  ::ros2_robot_middleware::action::MoveToPose_SendGoal_Response stamp(::ros2_robot_middleware::action::MoveToPose_SendGoal_Response::_stamp_type arg)
  {
    msg_.stamp = std::move(arg);
    return std::move(msg_);
  }

private:
  ::ros2_robot_middleware::action::MoveToPose_SendGoal_Response msg_;
};

class Init_MoveToPose_SendGoal_Response_accepted
{
public:
  Init_MoveToPose_SendGoal_Response_accepted()
  : msg_(::rosidl_runtime_cpp::MessageInitialization::SKIP)
  {}
  Init_MoveToPose_SendGoal_Response_stamp accepted(::ros2_robot_middleware::action::MoveToPose_SendGoal_Response::_accepted_type arg)
  {
    msg_.accepted = std::move(arg);
    return Init_MoveToPose_SendGoal_Response_stamp(msg_);
  }

private:
  ::ros2_robot_middleware::action::MoveToPose_SendGoal_Response msg_;
};

}  // namespace builder

}  // namespace action

template<typename MessageType>
auto build();

template<>
inline
auto build<::ros2_robot_middleware::action::MoveToPose_SendGoal_Response>()
{
  return ros2_robot_middleware::action::builder::Init_MoveToPose_SendGoal_Response_accepted();
}

}  // namespace ros2_robot_middleware


namespace ros2_robot_middleware
{

namespace action
{

namespace builder
{

class Init_MoveToPose_SendGoal_Event_response
{
public:
  explicit Init_MoveToPose_SendGoal_Event_response(::ros2_robot_middleware::action::MoveToPose_SendGoal_Event & msg)
  : msg_(msg)
  {}
  ::ros2_robot_middleware::action::MoveToPose_SendGoal_Event response(::ros2_robot_middleware::action::MoveToPose_SendGoal_Event::_response_type arg)
  {
    msg_.response = std::move(arg);
    return std::move(msg_);
  }

private:
  ::ros2_robot_middleware::action::MoveToPose_SendGoal_Event msg_;
};

class Init_MoveToPose_SendGoal_Event_request
{
public:
  explicit Init_MoveToPose_SendGoal_Event_request(::ros2_robot_middleware::action::MoveToPose_SendGoal_Event & msg)
  : msg_(msg)
  {}
  Init_MoveToPose_SendGoal_Event_response request(::ros2_robot_middleware::action::MoveToPose_SendGoal_Event::_request_type arg)
  {
    msg_.request = std::move(arg);
    return Init_MoveToPose_SendGoal_Event_response(msg_);
  }

private:
  ::ros2_robot_middleware::action::MoveToPose_SendGoal_Event msg_;
};

class Init_MoveToPose_SendGoal_Event_info
{
public:
  Init_MoveToPose_SendGoal_Event_info()
  : msg_(::rosidl_runtime_cpp::MessageInitialization::SKIP)
  {}
  Init_MoveToPose_SendGoal_Event_request info(::ros2_robot_middleware::action::MoveToPose_SendGoal_Event::_info_type arg)
  {
    msg_.info = std::move(arg);
    return Init_MoveToPose_SendGoal_Event_request(msg_);
  }

private:
  ::ros2_robot_middleware::action::MoveToPose_SendGoal_Event msg_;
};

}  // namespace builder

}  // namespace action

template<typename MessageType>
auto build();

template<>
inline
auto build<::ros2_robot_middleware::action::MoveToPose_SendGoal_Event>()
{
  return ros2_robot_middleware::action::builder::Init_MoveToPose_SendGoal_Event_info();
}

}  // namespace ros2_robot_middleware


namespace ros2_robot_middleware
{

namespace action
{

namespace builder
{

class Init_MoveToPose_GetResult_Request_goal_id
{
public:
  Init_MoveToPose_GetResult_Request_goal_id()
  : msg_(::rosidl_runtime_cpp::MessageInitialization::SKIP)
  {}
  ::ros2_robot_middleware::action::MoveToPose_GetResult_Request goal_id(::ros2_robot_middleware::action::MoveToPose_GetResult_Request::_goal_id_type arg)
  {
    msg_.goal_id = std::move(arg);
    return std::move(msg_);
  }

private:
  ::ros2_robot_middleware::action::MoveToPose_GetResult_Request msg_;
};

}  // namespace builder

}  // namespace action

template<typename MessageType>
auto build();

template<>
inline
auto build<::ros2_robot_middleware::action::MoveToPose_GetResult_Request>()
{
  return ros2_robot_middleware::action::builder::Init_MoveToPose_GetResult_Request_goal_id();
}

}  // namespace ros2_robot_middleware


namespace ros2_robot_middleware
{

namespace action
{

namespace builder
{

class Init_MoveToPose_GetResult_Response_result
{
public:
  explicit Init_MoveToPose_GetResult_Response_result(::ros2_robot_middleware::action::MoveToPose_GetResult_Response & msg)
  : msg_(msg)
  {}
  ::ros2_robot_middleware::action::MoveToPose_GetResult_Response result(::ros2_robot_middleware::action::MoveToPose_GetResult_Response::_result_type arg)
  {
    msg_.result = std::move(arg);
    return std::move(msg_);
  }

private:
  ::ros2_robot_middleware::action::MoveToPose_GetResult_Response msg_;
};

class Init_MoveToPose_GetResult_Response_status
{
public:
  Init_MoveToPose_GetResult_Response_status()
  : msg_(::rosidl_runtime_cpp::MessageInitialization::SKIP)
  {}
  Init_MoveToPose_GetResult_Response_result status(::ros2_robot_middleware::action::MoveToPose_GetResult_Response::_status_type arg)
  {
    msg_.status = std::move(arg);
    return Init_MoveToPose_GetResult_Response_result(msg_);
  }

private:
  ::ros2_robot_middleware::action::MoveToPose_GetResult_Response msg_;
};

}  // namespace builder

}  // namespace action

template<typename MessageType>
auto build();

template<>
inline
auto build<::ros2_robot_middleware::action::MoveToPose_GetResult_Response>()
{
  return ros2_robot_middleware::action::builder::Init_MoveToPose_GetResult_Response_status();
}

}  // namespace ros2_robot_middleware


namespace ros2_robot_middleware
{

namespace action
{

namespace builder
{

class Init_MoveToPose_GetResult_Event_response
{
public:
  explicit Init_MoveToPose_GetResult_Event_response(::ros2_robot_middleware::action::MoveToPose_GetResult_Event & msg)
  : msg_(msg)
  {}
  ::ros2_robot_middleware::action::MoveToPose_GetResult_Event response(::ros2_robot_middleware::action::MoveToPose_GetResult_Event::_response_type arg)
  {
    msg_.response = std::move(arg);
    return std::move(msg_);
  }

private:
  ::ros2_robot_middleware::action::MoveToPose_GetResult_Event msg_;
};

class Init_MoveToPose_GetResult_Event_request
{
public:
  explicit Init_MoveToPose_GetResult_Event_request(::ros2_robot_middleware::action::MoveToPose_GetResult_Event & msg)
  : msg_(msg)
  {}
  Init_MoveToPose_GetResult_Event_response request(::ros2_robot_middleware::action::MoveToPose_GetResult_Event::_request_type arg)
  {
    msg_.request = std::move(arg);
    return Init_MoveToPose_GetResult_Event_response(msg_);
  }

private:
  ::ros2_robot_middleware::action::MoveToPose_GetResult_Event msg_;
};

class Init_MoveToPose_GetResult_Event_info
{
public:
  Init_MoveToPose_GetResult_Event_info()
  : msg_(::rosidl_runtime_cpp::MessageInitialization::SKIP)
  {}
  Init_MoveToPose_GetResult_Event_request info(::ros2_robot_middleware::action::MoveToPose_GetResult_Event::_info_type arg)
  {
    msg_.info = std::move(arg);
    return Init_MoveToPose_GetResult_Event_request(msg_);
  }

private:
  ::ros2_robot_middleware::action::MoveToPose_GetResult_Event msg_;
};

}  // namespace builder

}  // namespace action

template<typename MessageType>
auto build();

template<>
inline
auto build<::ros2_robot_middleware::action::MoveToPose_GetResult_Event>()
{
  return ros2_robot_middleware::action::builder::Init_MoveToPose_GetResult_Event_info();
}

}  // namespace ros2_robot_middleware


namespace ros2_robot_middleware
{

namespace action
{

namespace builder
{

class Init_MoveToPose_FeedbackMessage_feedback
{
public:
  explicit Init_MoveToPose_FeedbackMessage_feedback(::ros2_robot_middleware::action::MoveToPose_FeedbackMessage & msg)
  : msg_(msg)
  {}
  ::ros2_robot_middleware::action::MoveToPose_FeedbackMessage feedback(::ros2_robot_middleware::action::MoveToPose_FeedbackMessage::_feedback_type arg)
  {
    msg_.feedback = std::move(arg);
    return std::move(msg_);
  }

private:
  ::ros2_robot_middleware::action::MoveToPose_FeedbackMessage msg_;
};

class Init_MoveToPose_FeedbackMessage_goal_id
{
public:
  Init_MoveToPose_FeedbackMessage_goal_id()
  : msg_(::rosidl_runtime_cpp::MessageInitialization::SKIP)
  {}
  Init_MoveToPose_FeedbackMessage_feedback goal_id(::ros2_robot_middleware::action::MoveToPose_FeedbackMessage::_goal_id_type arg)
  {
    msg_.goal_id = std::move(arg);
    return Init_MoveToPose_FeedbackMessage_feedback(msg_);
  }

private:
  ::ros2_robot_middleware::action::MoveToPose_FeedbackMessage msg_;
};

}  // namespace builder

}  // namespace action

template<typename MessageType>
auto build();

template<>
inline
auto build<::ros2_robot_middleware::action::MoveToPose_FeedbackMessage>()
{
  return ros2_robot_middleware::action::builder::Init_MoveToPose_FeedbackMessage_goal_id();
}

}  // namespace ros2_robot_middleware

#endif  // ROS2_ROBOT_MIDDLEWARE__ACTION__DETAIL__MOVE_TO_POSE__BUILDER_HPP_
