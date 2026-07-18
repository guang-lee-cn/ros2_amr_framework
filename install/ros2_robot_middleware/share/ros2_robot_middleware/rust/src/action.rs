
#[cfg(feature = "serde")]
use serde::{Deserialize, Serialize};



// Corresponds to ros2_robot_middleware__action__MoveToPose_Goal

// This struct is not documented.
#[allow(missing_docs)]

#[allow(non_camel_case_types)]
#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct MoveToPose_Goal {
    /// Goal
    pub target_x: f32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub target_y: f32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub target_theta: f32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub max_speed: f32,

}



impl Default for MoveToPose_Goal {
  fn default() -> Self {
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::action::rmw::MoveToPose_Goal::default())
  }
}

impl rosidl_runtime_rs::Message for MoveToPose_Goal {
  type RmwMsg = super::action::rmw::MoveToPose_Goal;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        target_x: msg.target_x,
        target_y: msg.target_y,
        target_theta: msg.target_theta,
        max_speed: msg.max_speed,
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
      target_x: msg.target_x,
      target_y: msg.target_y,
      target_theta: msg.target_theta,
      max_speed: msg.max_speed,
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      target_x: msg.target_x,
      target_y: msg.target_y,
      target_theta: msg.target_theta,
      max_speed: msg.max_speed,
    }
  }
}


// Corresponds to ros2_robot_middleware__action__MoveToPose_Result

// This struct is not documented.
#[allow(missing_docs)]

#[allow(non_camel_case_types)]
#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct MoveToPose_Result {

    // This member is not documented.
    #[allow(missing_docs)]
    pub reached: bool,


    // This member is not documented.
    #[allow(missing_docs)]
    pub final_x: f32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub final_y: f32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub elapsed_time: f32,

}



impl Default for MoveToPose_Result {
  fn default() -> Self {
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::action::rmw::MoveToPose_Result::default())
  }
}

impl rosidl_runtime_rs::Message for MoveToPose_Result {
  type RmwMsg = super::action::rmw::MoveToPose_Result;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        reached: msg.reached,
        final_x: msg.final_x,
        final_y: msg.final_y,
        elapsed_time: msg.elapsed_time,
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
      reached: msg.reached,
      final_x: msg.final_x,
      final_y: msg.final_y,
      elapsed_time: msg.elapsed_time,
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      reached: msg.reached,
      final_x: msg.final_x,
      final_y: msg.final_y,
      elapsed_time: msg.elapsed_time,
    }
  }
}


// Corresponds to ros2_robot_middleware__action__MoveToPose_Feedback

// This struct is not documented.
#[allow(missing_docs)]

#[allow(non_camel_case_types)]
#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct MoveToPose_Feedback {

    // This member is not documented.
    #[allow(missing_docs)]
    pub current_x: f32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub current_y: f32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub distance_remaining: f32,


    // This member is not documented.
    #[allow(missing_docs)]
    pub percent_complete: f32,

}



impl Default for MoveToPose_Feedback {
  fn default() -> Self {
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::action::rmw::MoveToPose_Feedback::default())
  }
}

impl rosidl_runtime_rs::Message for MoveToPose_Feedback {
  type RmwMsg = super::action::rmw::MoveToPose_Feedback;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        current_x: msg.current_x,
        current_y: msg.current_y,
        distance_remaining: msg.distance_remaining,
        percent_complete: msg.percent_complete,
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
      current_x: msg.current_x,
      current_y: msg.current_y,
      distance_remaining: msg.distance_remaining,
      percent_complete: msg.percent_complete,
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      current_x: msg.current_x,
      current_y: msg.current_y,
      distance_remaining: msg.distance_remaining,
      percent_complete: msg.percent_complete,
    }
  }
}


// Corresponds to ros2_robot_middleware__action__MoveToPose_FeedbackMessage

// This struct is not documented.
#[allow(missing_docs)]

#[allow(non_camel_case_types)]
#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct MoveToPose_FeedbackMessage {

    // This member is not documented.
    #[allow(missing_docs)]
    pub goal_id: unique_identifier_msgs::msg::UUID,


    // This member is not documented.
    #[allow(missing_docs)]
    pub feedback: super::action::MoveToPose_Feedback,

}



impl Default for MoveToPose_FeedbackMessage {
  fn default() -> Self {
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::action::rmw::MoveToPose_FeedbackMessage::default())
  }
}

impl rosidl_runtime_rs::Message for MoveToPose_FeedbackMessage {
  type RmwMsg = super::action::rmw::MoveToPose_FeedbackMessage;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        goal_id: unique_identifier_msgs::msg::UUID::into_rmw_message(std::borrow::Cow::Owned(msg.goal_id)).into_owned(),
        feedback: super::action::MoveToPose_Feedback::into_rmw_message(std::borrow::Cow::Owned(msg.feedback)).into_owned(),
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        goal_id: unique_identifier_msgs::msg::UUID::into_rmw_message(std::borrow::Cow::Borrowed(&msg.goal_id)).into_owned(),
        feedback: super::action::MoveToPose_Feedback::into_rmw_message(std::borrow::Cow::Borrowed(&msg.feedback)).into_owned(),
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      goal_id: unique_identifier_msgs::msg::UUID::from_rmw_message(msg.goal_id),
      feedback: super::action::MoveToPose_Feedback::from_rmw_message(msg.feedback),
    }
  }
}






// Corresponds to ros2_robot_middleware__action__MoveToPose_SendGoal_Request

// This struct is not documented.
#[allow(missing_docs)]

#[allow(non_camel_case_types)]
#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct MoveToPose_SendGoal_Request {

    // This member is not documented.
    #[allow(missing_docs)]
    pub goal_id: unique_identifier_msgs::msg::UUID,


    // This member is not documented.
    #[allow(missing_docs)]
    pub goal: super::action::MoveToPose_Goal,

}



impl Default for MoveToPose_SendGoal_Request {
  fn default() -> Self {
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::action::rmw::MoveToPose_SendGoal_Request::default())
  }
}

impl rosidl_runtime_rs::Message for MoveToPose_SendGoal_Request {
  type RmwMsg = super::action::rmw::MoveToPose_SendGoal_Request;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        goal_id: unique_identifier_msgs::msg::UUID::into_rmw_message(std::borrow::Cow::Owned(msg.goal_id)).into_owned(),
        goal: super::action::MoveToPose_Goal::into_rmw_message(std::borrow::Cow::Owned(msg.goal)).into_owned(),
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        goal_id: unique_identifier_msgs::msg::UUID::into_rmw_message(std::borrow::Cow::Borrowed(&msg.goal_id)).into_owned(),
        goal: super::action::MoveToPose_Goal::into_rmw_message(std::borrow::Cow::Borrowed(&msg.goal)).into_owned(),
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      goal_id: unique_identifier_msgs::msg::UUID::from_rmw_message(msg.goal_id),
      goal: super::action::MoveToPose_Goal::from_rmw_message(msg.goal),
    }
  }
}


// Corresponds to ros2_robot_middleware__action__MoveToPose_SendGoal_Response

// This struct is not documented.
#[allow(missing_docs)]

#[allow(non_camel_case_types)]
#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct MoveToPose_SendGoal_Response {

    // This member is not documented.
    #[allow(missing_docs)]
    pub accepted: bool,


    // This member is not documented.
    #[allow(missing_docs)]
    pub stamp: builtin_interfaces::msg::Time,

}



impl Default for MoveToPose_SendGoal_Response {
  fn default() -> Self {
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::action::rmw::MoveToPose_SendGoal_Response::default())
  }
}

impl rosidl_runtime_rs::Message for MoveToPose_SendGoal_Response {
  type RmwMsg = super::action::rmw::MoveToPose_SendGoal_Response;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        accepted: msg.accepted,
        stamp: builtin_interfaces::msg::Time::into_rmw_message(std::borrow::Cow::Owned(msg.stamp)).into_owned(),
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
      accepted: msg.accepted,
        stamp: builtin_interfaces::msg::Time::into_rmw_message(std::borrow::Cow::Borrowed(&msg.stamp)).into_owned(),
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      accepted: msg.accepted,
      stamp: builtin_interfaces::msg::Time::from_rmw_message(msg.stamp),
    }
  }
}


// Corresponds to ros2_robot_middleware__action__MoveToPose_GetResult_Request

// This struct is not documented.
#[allow(missing_docs)]

#[allow(non_camel_case_types)]
#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct MoveToPose_GetResult_Request {

    // This member is not documented.
    #[allow(missing_docs)]
    pub goal_id: unique_identifier_msgs::msg::UUID,

}



impl Default for MoveToPose_GetResult_Request {
  fn default() -> Self {
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::action::rmw::MoveToPose_GetResult_Request::default())
  }
}

impl rosidl_runtime_rs::Message for MoveToPose_GetResult_Request {
  type RmwMsg = super::action::rmw::MoveToPose_GetResult_Request;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        goal_id: unique_identifier_msgs::msg::UUID::into_rmw_message(std::borrow::Cow::Owned(msg.goal_id)).into_owned(),
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        goal_id: unique_identifier_msgs::msg::UUID::into_rmw_message(std::borrow::Cow::Borrowed(&msg.goal_id)).into_owned(),
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      goal_id: unique_identifier_msgs::msg::UUID::from_rmw_message(msg.goal_id),
    }
  }
}


// Corresponds to ros2_robot_middleware__action__MoveToPose_GetResult_Response

// This struct is not documented.
#[allow(missing_docs)]

#[allow(non_camel_case_types)]
#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct MoveToPose_GetResult_Response {

    // This member is not documented.
    #[allow(missing_docs)]
    pub status: i8,


    // This member is not documented.
    #[allow(missing_docs)]
    pub result: super::action::MoveToPose_Result,

}



impl Default for MoveToPose_GetResult_Response {
  fn default() -> Self {
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::action::rmw::MoveToPose_GetResult_Response::default())
  }
}

impl rosidl_runtime_rs::Message for MoveToPose_GetResult_Response {
  type RmwMsg = super::action::rmw::MoveToPose_GetResult_Response;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        status: msg.status,
        result: super::action::MoveToPose_Result::into_rmw_message(std::borrow::Cow::Owned(msg.result)).into_owned(),
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
      status: msg.status,
        result: super::action::MoveToPose_Result::into_rmw_message(std::borrow::Cow::Borrowed(&msg.result)).into_owned(),
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      status: msg.status,
      result: super::action::MoveToPose_Result::from_rmw_message(msg.result),
    }
  }
}






#[link(name = "ros2_robot_middleware__rosidl_typesupport_c")]
extern "C" {
    fn rosidl_typesupport_c__get_service_type_support_handle__ros2_robot_middleware__action__MoveToPose_SendGoal() -> *const std::ffi::c_void;
}

// Corresponds to ros2_robot_middleware__action__MoveToPose_SendGoal
#[allow(missing_docs, non_camel_case_types)]
pub struct MoveToPose_SendGoal;

impl rosidl_runtime_rs::Service for MoveToPose_SendGoal {
    type Request = MoveToPose_SendGoal_Request;
    type Response = MoveToPose_SendGoal_Response;

    fn get_type_support() -> *const std::ffi::c_void {
        // SAFETY: No preconditions for this function.
        unsafe { rosidl_typesupport_c__get_service_type_support_handle__ros2_robot_middleware__action__MoveToPose_SendGoal() }
    }
}




#[link(name = "ros2_robot_middleware__rosidl_typesupport_c")]
extern "C" {
    fn rosidl_typesupport_c__get_service_type_support_handle__ros2_robot_middleware__action__MoveToPose_GetResult() -> *const std::ffi::c_void;
}

// Corresponds to ros2_robot_middleware__action__MoveToPose_GetResult
#[allow(missing_docs, non_camel_case_types)]
pub struct MoveToPose_GetResult;

impl rosidl_runtime_rs::Service for MoveToPose_GetResult {
    type Request = MoveToPose_GetResult_Request;
    type Response = MoveToPose_GetResult_Response;

    fn get_type_support() -> *const std::ffi::c_void {
        // SAFETY: No preconditions for this function.
        unsafe { rosidl_typesupport_c__get_service_type_support_handle__ros2_robot_middleware__action__MoveToPose_GetResult() }
    }
}






#[link(name = "ros2_robot_middleware__rosidl_typesupport_c")]
extern "C" {
    fn rosidl_typesupport_c__get_action_type_support_handle__ros2_robot_middleware__action__MoveToPose() -> *const std::ffi::c_void;
}

// Corresponds to ros2_robot_middleware__action__MoveToPose
#[allow(missing_docs, non_camel_case_types)]
pub struct MoveToPose;

impl rosidl_runtime_rs::Action for MoveToPose {
  // --- Associated types for client library users ---
  /// The goal message defined in the action definition.
  type Goal = MoveToPose_Goal;

  /// The result message defined in the action definition.
  type Result = MoveToPose_Result;

  /// The feedback message defined in the action definition.
  type Feedback = MoveToPose_Feedback;

  // --- Associated types for client library implementation ---
  /// The feedback message with generic fields which wraps the feedback message.
  type FeedbackMessage = super::action::MoveToPose_FeedbackMessage;

  /// The send_goal service using a wrapped version of the goal message as a request.
  type SendGoalService = super::action::MoveToPose_SendGoal;

  /// The generic service to cancel a goal.
  type CancelGoalService = action_msgs::srv::rmw::CancelGoal;

  /// The get_result service using a wrapped version of the result message as a response.
  type GetResultService = super::action::MoveToPose_GetResult;

  // --- Methods for client library implementation ---
  fn get_type_support() -> *const std::ffi::c_void {
    // SAFETY: No preconditions for this function.
    unsafe { rosidl_typesupport_c__get_action_type_support_handle__ros2_robot_middleware__action__MoveToPose() }
  }

  fn create_goal_request(
    goal_id: &[u8; 16],
    goal: super::action::rmw::MoveToPose_Goal,
  ) -> super::action::rmw::MoveToPose_SendGoal_Request {
   super::action::rmw::MoveToPose_SendGoal_Request {
      goal_id: unique_identifier_msgs::msg::rmw::UUID { uuid: *goal_id },
      goal,
    }
  }

  fn split_goal_request(
    request: super::action::rmw::MoveToPose_SendGoal_Request,
  ) -> (
    [u8; 16],
   super::action::rmw::MoveToPose_Goal,
  ) {
    (request.goal_id.uuid, request.goal)
  }

  fn create_goal_response(
    accepted: bool,
    stamp: (i32, u32),
  ) -> super::action::rmw::MoveToPose_SendGoal_Response {
   super::action::rmw::MoveToPose_SendGoal_Response {
      accepted,
      stamp: builtin_interfaces::msg::rmw::Time {
        sec: stamp.0,
        nanosec: stamp.1,
      },
    }
  }

  fn get_goal_response_accepted(
    response: &super::action::rmw::MoveToPose_SendGoal_Response,
  ) -> bool {
    response.accepted
  }

  fn get_goal_response_stamp(
    response: &super::action::rmw::MoveToPose_SendGoal_Response,
  ) -> (i32, u32) {
    (response.stamp.sec, response.stamp.nanosec)
  }

  fn create_feedback_message(
    goal_id: &[u8; 16],
    feedback: super::action::rmw::MoveToPose_Feedback,
  ) -> super::action::rmw::MoveToPose_FeedbackMessage {
    let mut message = super::action::rmw::MoveToPose_FeedbackMessage::default();
    message.goal_id.uuid = *goal_id;
    message.feedback = feedback;
    message
  }

  fn split_feedback_message(
    feedback: super::action::rmw::MoveToPose_FeedbackMessage,
  ) -> (
    [u8; 16],
   super::action::rmw::MoveToPose_Feedback,
  ) {
    (feedback.goal_id.uuid, feedback.feedback)
  }

  fn create_result_request(
    goal_id: &[u8; 16],
  ) -> super::action::rmw::MoveToPose_GetResult_Request {
   super::action::rmw::MoveToPose_GetResult_Request {
      goal_id: unique_identifier_msgs::msg::rmw::UUID { uuid: *goal_id },
    }
  }

  fn get_result_request_uuid(
    request: &super::action::rmw::MoveToPose_GetResult_Request,
  ) -> &[u8; 16] {
    &request.goal_id.uuid
  }

  fn create_result_response(
    status: i8,
    result: super::action::rmw::MoveToPose_Result,
  ) -> super::action::rmw::MoveToPose_GetResult_Response {
   super::action::rmw::MoveToPose_GetResult_Response {
      status,
      result,
    }
  }

  fn split_result_response(
    response: super::action::rmw::MoveToPose_GetResult_Response
  ) -> (
    i8,
   super::action::rmw::MoveToPose_Result,
  ) {
    (response.status, response.result)
  }
}


