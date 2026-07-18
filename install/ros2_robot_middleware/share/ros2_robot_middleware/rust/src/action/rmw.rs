
#[cfg(feature = "serde")]
use serde::{Deserialize, Serialize};


#[link(name = "ros2_robot_middleware__rosidl_typesupport_c")]
extern "C" {
    fn rosidl_typesupport_c__get_message_type_support_handle__ros2_robot_middleware__action__MoveToPose_Goal() -> *const std::ffi::c_void;
}

#[link(name = "ros2_robot_middleware__rosidl_generator_c")]
extern "C" {
    fn ros2_robot_middleware__action__MoveToPose_Goal__init(msg: *mut MoveToPose_Goal) -> bool;
    fn ros2_robot_middleware__action__MoveToPose_Goal__Sequence__init(seq: *mut rosidl_runtime_rs::Sequence<MoveToPose_Goal>, size: usize) -> bool;
    fn ros2_robot_middleware__action__MoveToPose_Goal__Sequence__fini(seq: *mut rosidl_runtime_rs::Sequence<MoveToPose_Goal>);
    fn ros2_robot_middleware__action__MoveToPose_Goal__Sequence__copy(in_seq: &rosidl_runtime_rs::Sequence<MoveToPose_Goal>, out_seq: *mut rosidl_runtime_rs::Sequence<MoveToPose_Goal>) -> bool;
}

// Corresponds to ros2_robot_middleware__action__MoveToPose_Goal
#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]


// This struct is not documented.
#[allow(missing_docs)]

#[allow(non_camel_case_types)]
#[repr(C)]
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
    unsafe {
      let mut msg = std::mem::zeroed();
      if !ros2_robot_middleware__action__MoveToPose_Goal__init(&mut msg as *mut _) {
        panic!("Call to ros2_robot_middleware__action__MoveToPose_Goal__init() failed");
      }
      msg
    }
  }
}

impl rosidl_runtime_rs::SequenceAlloc for MoveToPose_Goal {
  fn sequence_init(seq: &mut rosidl_runtime_rs::Sequence<Self>, size: usize) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { ros2_robot_middleware__action__MoveToPose_Goal__Sequence__init(seq as *mut _, size) }
  }
  fn sequence_fini(seq: &mut rosidl_runtime_rs::Sequence<Self>) {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { ros2_robot_middleware__action__MoveToPose_Goal__Sequence__fini(seq as *mut _) }
  }
  fn sequence_copy(in_seq: &rosidl_runtime_rs::Sequence<Self>, out_seq: &mut rosidl_runtime_rs::Sequence<Self>) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { ros2_robot_middleware__action__MoveToPose_Goal__Sequence__copy(in_seq, out_seq as *mut _) }
  }
}

impl rosidl_runtime_rs::Message for MoveToPose_Goal {
  type RmwMsg = Self;
  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> { msg_cow }
  fn from_rmw_message(msg: Self::RmwMsg) -> Self { msg }
}

impl rosidl_runtime_rs::RmwMessage for MoveToPose_Goal where Self: Sized {
  const TYPE_NAME: &'static str = "ros2_robot_middleware/action/MoveToPose_Goal";
  fn get_type_support() -> *const std::ffi::c_void {
    // SAFETY: No preconditions for this function.
    unsafe { rosidl_typesupport_c__get_message_type_support_handle__ros2_robot_middleware__action__MoveToPose_Goal() }
  }
}


#[link(name = "ros2_robot_middleware__rosidl_typesupport_c")]
extern "C" {
    fn rosidl_typesupport_c__get_message_type_support_handle__ros2_robot_middleware__action__MoveToPose_Result() -> *const std::ffi::c_void;
}

#[link(name = "ros2_robot_middleware__rosidl_generator_c")]
extern "C" {
    fn ros2_robot_middleware__action__MoveToPose_Result__init(msg: *mut MoveToPose_Result) -> bool;
    fn ros2_robot_middleware__action__MoveToPose_Result__Sequence__init(seq: *mut rosidl_runtime_rs::Sequence<MoveToPose_Result>, size: usize) -> bool;
    fn ros2_robot_middleware__action__MoveToPose_Result__Sequence__fini(seq: *mut rosidl_runtime_rs::Sequence<MoveToPose_Result>);
    fn ros2_robot_middleware__action__MoveToPose_Result__Sequence__copy(in_seq: &rosidl_runtime_rs::Sequence<MoveToPose_Result>, out_seq: *mut rosidl_runtime_rs::Sequence<MoveToPose_Result>) -> bool;
}

// Corresponds to ros2_robot_middleware__action__MoveToPose_Result
#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]


// This struct is not documented.
#[allow(missing_docs)]

#[allow(non_camel_case_types)]
#[repr(C)]
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
    unsafe {
      let mut msg = std::mem::zeroed();
      if !ros2_robot_middleware__action__MoveToPose_Result__init(&mut msg as *mut _) {
        panic!("Call to ros2_robot_middleware__action__MoveToPose_Result__init() failed");
      }
      msg
    }
  }
}

impl rosidl_runtime_rs::SequenceAlloc for MoveToPose_Result {
  fn sequence_init(seq: &mut rosidl_runtime_rs::Sequence<Self>, size: usize) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { ros2_robot_middleware__action__MoveToPose_Result__Sequence__init(seq as *mut _, size) }
  }
  fn sequence_fini(seq: &mut rosidl_runtime_rs::Sequence<Self>) {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { ros2_robot_middleware__action__MoveToPose_Result__Sequence__fini(seq as *mut _) }
  }
  fn sequence_copy(in_seq: &rosidl_runtime_rs::Sequence<Self>, out_seq: &mut rosidl_runtime_rs::Sequence<Self>) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { ros2_robot_middleware__action__MoveToPose_Result__Sequence__copy(in_seq, out_seq as *mut _) }
  }
}

impl rosidl_runtime_rs::Message for MoveToPose_Result {
  type RmwMsg = Self;
  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> { msg_cow }
  fn from_rmw_message(msg: Self::RmwMsg) -> Self { msg }
}

impl rosidl_runtime_rs::RmwMessage for MoveToPose_Result where Self: Sized {
  const TYPE_NAME: &'static str = "ros2_robot_middleware/action/MoveToPose_Result";
  fn get_type_support() -> *const std::ffi::c_void {
    // SAFETY: No preconditions for this function.
    unsafe { rosidl_typesupport_c__get_message_type_support_handle__ros2_robot_middleware__action__MoveToPose_Result() }
  }
}


#[link(name = "ros2_robot_middleware__rosidl_typesupport_c")]
extern "C" {
    fn rosidl_typesupport_c__get_message_type_support_handle__ros2_robot_middleware__action__MoveToPose_Feedback() -> *const std::ffi::c_void;
}

#[link(name = "ros2_robot_middleware__rosidl_generator_c")]
extern "C" {
    fn ros2_robot_middleware__action__MoveToPose_Feedback__init(msg: *mut MoveToPose_Feedback) -> bool;
    fn ros2_robot_middleware__action__MoveToPose_Feedback__Sequence__init(seq: *mut rosidl_runtime_rs::Sequence<MoveToPose_Feedback>, size: usize) -> bool;
    fn ros2_robot_middleware__action__MoveToPose_Feedback__Sequence__fini(seq: *mut rosidl_runtime_rs::Sequence<MoveToPose_Feedback>);
    fn ros2_robot_middleware__action__MoveToPose_Feedback__Sequence__copy(in_seq: &rosidl_runtime_rs::Sequence<MoveToPose_Feedback>, out_seq: *mut rosidl_runtime_rs::Sequence<MoveToPose_Feedback>) -> bool;
}

// Corresponds to ros2_robot_middleware__action__MoveToPose_Feedback
#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]


// This struct is not documented.
#[allow(missing_docs)]

#[allow(non_camel_case_types)]
#[repr(C)]
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
    unsafe {
      let mut msg = std::mem::zeroed();
      if !ros2_robot_middleware__action__MoveToPose_Feedback__init(&mut msg as *mut _) {
        panic!("Call to ros2_robot_middleware__action__MoveToPose_Feedback__init() failed");
      }
      msg
    }
  }
}

impl rosidl_runtime_rs::SequenceAlloc for MoveToPose_Feedback {
  fn sequence_init(seq: &mut rosidl_runtime_rs::Sequence<Self>, size: usize) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { ros2_robot_middleware__action__MoveToPose_Feedback__Sequence__init(seq as *mut _, size) }
  }
  fn sequence_fini(seq: &mut rosidl_runtime_rs::Sequence<Self>) {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { ros2_robot_middleware__action__MoveToPose_Feedback__Sequence__fini(seq as *mut _) }
  }
  fn sequence_copy(in_seq: &rosidl_runtime_rs::Sequence<Self>, out_seq: &mut rosidl_runtime_rs::Sequence<Self>) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { ros2_robot_middleware__action__MoveToPose_Feedback__Sequence__copy(in_seq, out_seq as *mut _) }
  }
}

impl rosidl_runtime_rs::Message for MoveToPose_Feedback {
  type RmwMsg = Self;
  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> { msg_cow }
  fn from_rmw_message(msg: Self::RmwMsg) -> Self { msg }
}

impl rosidl_runtime_rs::RmwMessage for MoveToPose_Feedback where Self: Sized {
  const TYPE_NAME: &'static str = "ros2_robot_middleware/action/MoveToPose_Feedback";
  fn get_type_support() -> *const std::ffi::c_void {
    // SAFETY: No preconditions for this function.
    unsafe { rosidl_typesupport_c__get_message_type_support_handle__ros2_robot_middleware__action__MoveToPose_Feedback() }
  }
}


#[link(name = "ros2_robot_middleware__rosidl_typesupport_c")]
extern "C" {
    fn rosidl_typesupport_c__get_message_type_support_handle__ros2_robot_middleware__action__MoveToPose_FeedbackMessage() -> *const std::ffi::c_void;
}

#[link(name = "ros2_robot_middleware__rosidl_generator_c")]
extern "C" {
    fn ros2_robot_middleware__action__MoveToPose_FeedbackMessage__init(msg: *mut MoveToPose_FeedbackMessage) -> bool;
    fn ros2_robot_middleware__action__MoveToPose_FeedbackMessage__Sequence__init(seq: *mut rosidl_runtime_rs::Sequence<MoveToPose_FeedbackMessage>, size: usize) -> bool;
    fn ros2_robot_middleware__action__MoveToPose_FeedbackMessage__Sequence__fini(seq: *mut rosidl_runtime_rs::Sequence<MoveToPose_FeedbackMessage>);
    fn ros2_robot_middleware__action__MoveToPose_FeedbackMessage__Sequence__copy(in_seq: &rosidl_runtime_rs::Sequence<MoveToPose_FeedbackMessage>, out_seq: *mut rosidl_runtime_rs::Sequence<MoveToPose_FeedbackMessage>) -> bool;
}

// Corresponds to ros2_robot_middleware__action__MoveToPose_FeedbackMessage
#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]


// This struct is not documented.
#[allow(missing_docs)]

#[allow(non_camel_case_types)]
#[repr(C)]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct MoveToPose_FeedbackMessage {

    // This member is not documented.
    #[allow(missing_docs)]
    pub goal_id: unique_identifier_msgs::msg::rmw::UUID,


    // This member is not documented.
    #[allow(missing_docs)]
    pub feedback: super::super::action::rmw::MoveToPose_Feedback,

}



impl Default for MoveToPose_FeedbackMessage {
  fn default() -> Self {
    unsafe {
      let mut msg = std::mem::zeroed();
      if !ros2_robot_middleware__action__MoveToPose_FeedbackMessage__init(&mut msg as *mut _) {
        panic!("Call to ros2_robot_middleware__action__MoveToPose_FeedbackMessage__init() failed");
      }
      msg
    }
  }
}

impl rosidl_runtime_rs::SequenceAlloc for MoveToPose_FeedbackMessage {
  fn sequence_init(seq: &mut rosidl_runtime_rs::Sequence<Self>, size: usize) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { ros2_robot_middleware__action__MoveToPose_FeedbackMessage__Sequence__init(seq as *mut _, size) }
  }
  fn sequence_fini(seq: &mut rosidl_runtime_rs::Sequence<Self>) {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { ros2_robot_middleware__action__MoveToPose_FeedbackMessage__Sequence__fini(seq as *mut _) }
  }
  fn sequence_copy(in_seq: &rosidl_runtime_rs::Sequence<Self>, out_seq: &mut rosidl_runtime_rs::Sequence<Self>) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { ros2_robot_middleware__action__MoveToPose_FeedbackMessage__Sequence__copy(in_seq, out_seq as *mut _) }
  }
}

impl rosidl_runtime_rs::Message for MoveToPose_FeedbackMessage {
  type RmwMsg = Self;
  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> { msg_cow }
  fn from_rmw_message(msg: Self::RmwMsg) -> Self { msg }
}

impl rosidl_runtime_rs::RmwMessage for MoveToPose_FeedbackMessage where Self: Sized {
  const TYPE_NAME: &'static str = "ros2_robot_middleware/action/MoveToPose_FeedbackMessage";
  fn get_type_support() -> *const std::ffi::c_void {
    // SAFETY: No preconditions for this function.
    unsafe { rosidl_typesupport_c__get_message_type_support_handle__ros2_robot_middleware__action__MoveToPose_FeedbackMessage() }
  }
}




#[link(name = "ros2_robot_middleware__rosidl_typesupport_c")]
extern "C" {
    fn rosidl_typesupport_c__get_message_type_support_handle__ros2_robot_middleware__action__MoveToPose_SendGoal_Request() -> *const std::ffi::c_void;
}

#[link(name = "ros2_robot_middleware__rosidl_generator_c")]
extern "C" {
    fn ros2_robot_middleware__action__MoveToPose_SendGoal_Request__init(msg: *mut MoveToPose_SendGoal_Request) -> bool;
    fn ros2_robot_middleware__action__MoveToPose_SendGoal_Request__Sequence__init(seq: *mut rosidl_runtime_rs::Sequence<MoveToPose_SendGoal_Request>, size: usize) -> bool;
    fn ros2_robot_middleware__action__MoveToPose_SendGoal_Request__Sequence__fini(seq: *mut rosidl_runtime_rs::Sequence<MoveToPose_SendGoal_Request>);
    fn ros2_robot_middleware__action__MoveToPose_SendGoal_Request__Sequence__copy(in_seq: &rosidl_runtime_rs::Sequence<MoveToPose_SendGoal_Request>, out_seq: *mut rosidl_runtime_rs::Sequence<MoveToPose_SendGoal_Request>) -> bool;
}

// Corresponds to ros2_robot_middleware__action__MoveToPose_SendGoal_Request
#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]


// This struct is not documented.
#[allow(missing_docs)]

#[allow(non_camel_case_types)]
#[repr(C)]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct MoveToPose_SendGoal_Request {

    // This member is not documented.
    #[allow(missing_docs)]
    pub goal_id: unique_identifier_msgs::msg::rmw::UUID,


    // This member is not documented.
    #[allow(missing_docs)]
    pub goal: super::super::action::rmw::MoveToPose_Goal,

}



impl Default for MoveToPose_SendGoal_Request {
  fn default() -> Self {
    unsafe {
      let mut msg = std::mem::zeroed();
      if !ros2_robot_middleware__action__MoveToPose_SendGoal_Request__init(&mut msg as *mut _) {
        panic!("Call to ros2_robot_middleware__action__MoveToPose_SendGoal_Request__init() failed");
      }
      msg
    }
  }
}

impl rosidl_runtime_rs::SequenceAlloc for MoveToPose_SendGoal_Request {
  fn sequence_init(seq: &mut rosidl_runtime_rs::Sequence<Self>, size: usize) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { ros2_robot_middleware__action__MoveToPose_SendGoal_Request__Sequence__init(seq as *mut _, size) }
  }
  fn sequence_fini(seq: &mut rosidl_runtime_rs::Sequence<Self>) {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { ros2_robot_middleware__action__MoveToPose_SendGoal_Request__Sequence__fini(seq as *mut _) }
  }
  fn sequence_copy(in_seq: &rosidl_runtime_rs::Sequence<Self>, out_seq: &mut rosidl_runtime_rs::Sequence<Self>) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { ros2_robot_middleware__action__MoveToPose_SendGoal_Request__Sequence__copy(in_seq, out_seq as *mut _) }
  }
}

impl rosidl_runtime_rs::Message for MoveToPose_SendGoal_Request {
  type RmwMsg = Self;
  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> { msg_cow }
  fn from_rmw_message(msg: Self::RmwMsg) -> Self { msg }
}

impl rosidl_runtime_rs::RmwMessage for MoveToPose_SendGoal_Request where Self: Sized {
  const TYPE_NAME: &'static str = "ros2_robot_middleware/action/MoveToPose_SendGoal_Request";
  fn get_type_support() -> *const std::ffi::c_void {
    // SAFETY: No preconditions for this function.
    unsafe { rosidl_typesupport_c__get_message_type_support_handle__ros2_robot_middleware__action__MoveToPose_SendGoal_Request() }
  }
}


#[link(name = "ros2_robot_middleware__rosidl_typesupport_c")]
extern "C" {
    fn rosidl_typesupport_c__get_message_type_support_handle__ros2_robot_middleware__action__MoveToPose_SendGoal_Response() -> *const std::ffi::c_void;
}

#[link(name = "ros2_robot_middleware__rosidl_generator_c")]
extern "C" {
    fn ros2_robot_middleware__action__MoveToPose_SendGoal_Response__init(msg: *mut MoveToPose_SendGoal_Response) -> bool;
    fn ros2_robot_middleware__action__MoveToPose_SendGoal_Response__Sequence__init(seq: *mut rosidl_runtime_rs::Sequence<MoveToPose_SendGoal_Response>, size: usize) -> bool;
    fn ros2_robot_middleware__action__MoveToPose_SendGoal_Response__Sequence__fini(seq: *mut rosidl_runtime_rs::Sequence<MoveToPose_SendGoal_Response>);
    fn ros2_robot_middleware__action__MoveToPose_SendGoal_Response__Sequence__copy(in_seq: &rosidl_runtime_rs::Sequence<MoveToPose_SendGoal_Response>, out_seq: *mut rosidl_runtime_rs::Sequence<MoveToPose_SendGoal_Response>) -> bool;
}

// Corresponds to ros2_robot_middleware__action__MoveToPose_SendGoal_Response
#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]


// This struct is not documented.
#[allow(missing_docs)]

#[allow(non_camel_case_types)]
#[repr(C)]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct MoveToPose_SendGoal_Response {

    // This member is not documented.
    #[allow(missing_docs)]
    pub accepted: bool,


    // This member is not documented.
    #[allow(missing_docs)]
    pub stamp: builtin_interfaces::msg::rmw::Time,

}



impl Default for MoveToPose_SendGoal_Response {
  fn default() -> Self {
    unsafe {
      let mut msg = std::mem::zeroed();
      if !ros2_robot_middleware__action__MoveToPose_SendGoal_Response__init(&mut msg as *mut _) {
        panic!("Call to ros2_robot_middleware__action__MoveToPose_SendGoal_Response__init() failed");
      }
      msg
    }
  }
}

impl rosidl_runtime_rs::SequenceAlloc for MoveToPose_SendGoal_Response {
  fn sequence_init(seq: &mut rosidl_runtime_rs::Sequence<Self>, size: usize) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { ros2_robot_middleware__action__MoveToPose_SendGoal_Response__Sequence__init(seq as *mut _, size) }
  }
  fn sequence_fini(seq: &mut rosidl_runtime_rs::Sequence<Self>) {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { ros2_robot_middleware__action__MoveToPose_SendGoal_Response__Sequence__fini(seq as *mut _) }
  }
  fn sequence_copy(in_seq: &rosidl_runtime_rs::Sequence<Self>, out_seq: &mut rosidl_runtime_rs::Sequence<Self>) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { ros2_robot_middleware__action__MoveToPose_SendGoal_Response__Sequence__copy(in_seq, out_seq as *mut _) }
  }
}

impl rosidl_runtime_rs::Message for MoveToPose_SendGoal_Response {
  type RmwMsg = Self;
  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> { msg_cow }
  fn from_rmw_message(msg: Self::RmwMsg) -> Self { msg }
}

impl rosidl_runtime_rs::RmwMessage for MoveToPose_SendGoal_Response where Self: Sized {
  const TYPE_NAME: &'static str = "ros2_robot_middleware/action/MoveToPose_SendGoal_Response";
  fn get_type_support() -> *const std::ffi::c_void {
    // SAFETY: No preconditions for this function.
    unsafe { rosidl_typesupport_c__get_message_type_support_handle__ros2_robot_middleware__action__MoveToPose_SendGoal_Response() }
  }
}


#[link(name = "ros2_robot_middleware__rosidl_typesupport_c")]
extern "C" {
    fn rosidl_typesupport_c__get_message_type_support_handle__ros2_robot_middleware__action__MoveToPose_GetResult_Request() -> *const std::ffi::c_void;
}

#[link(name = "ros2_robot_middleware__rosidl_generator_c")]
extern "C" {
    fn ros2_robot_middleware__action__MoveToPose_GetResult_Request__init(msg: *mut MoveToPose_GetResult_Request) -> bool;
    fn ros2_robot_middleware__action__MoveToPose_GetResult_Request__Sequence__init(seq: *mut rosidl_runtime_rs::Sequence<MoveToPose_GetResult_Request>, size: usize) -> bool;
    fn ros2_robot_middleware__action__MoveToPose_GetResult_Request__Sequence__fini(seq: *mut rosidl_runtime_rs::Sequence<MoveToPose_GetResult_Request>);
    fn ros2_robot_middleware__action__MoveToPose_GetResult_Request__Sequence__copy(in_seq: &rosidl_runtime_rs::Sequence<MoveToPose_GetResult_Request>, out_seq: *mut rosidl_runtime_rs::Sequence<MoveToPose_GetResult_Request>) -> bool;
}

// Corresponds to ros2_robot_middleware__action__MoveToPose_GetResult_Request
#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]


// This struct is not documented.
#[allow(missing_docs)]

#[allow(non_camel_case_types)]
#[repr(C)]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct MoveToPose_GetResult_Request {

    // This member is not documented.
    #[allow(missing_docs)]
    pub goal_id: unique_identifier_msgs::msg::rmw::UUID,

}



impl Default for MoveToPose_GetResult_Request {
  fn default() -> Self {
    unsafe {
      let mut msg = std::mem::zeroed();
      if !ros2_robot_middleware__action__MoveToPose_GetResult_Request__init(&mut msg as *mut _) {
        panic!("Call to ros2_robot_middleware__action__MoveToPose_GetResult_Request__init() failed");
      }
      msg
    }
  }
}

impl rosidl_runtime_rs::SequenceAlloc for MoveToPose_GetResult_Request {
  fn sequence_init(seq: &mut rosidl_runtime_rs::Sequence<Self>, size: usize) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { ros2_robot_middleware__action__MoveToPose_GetResult_Request__Sequence__init(seq as *mut _, size) }
  }
  fn sequence_fini(seq: &mut rosidl_runtime_rs::Sequence<Self>) {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { ros2_robot_middleware__action__MoveToPose_GetResult_Request__Sequence__fini(seq as *mut _) }
  }
  fn sequence_copy(in_seq: &rosidl_runtime_rs::Sequence<Self>, out_seq: &mut rosidl_runtime_rs::Sequence<Self>) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { ros2_robot_middleware__action__MoveToPose_GetResult_Request__Sequence__copy(in_seq, out_seq as *mut _) }
  }
}

impl rosidl_runtime_rs::Message for MoveToPose_GetResult_Request {
  type RmwMsg = Self;
  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> { msg_cow }
  fn from_rmw_message(msg: Self::RmwMsg) -> Self { msg }
}

impl rosidl_runtime_rs::RmwMessage for MoveToPose_GetResult_Request where Self: Sized {
  const TYPE_NAME: &'static str = "ros2_robot_middleware/action/MoveToPose_GetResult_Request";
  fn get_type_support() -> *const std::ffi::c_void {
    // SAFETY: No preconditions for this function.
    unsafe { rosidl_typesupport_c__get_message_type_support_handle__ros2_robot_middleware__action__MoveToPose_GetResult_Request() }
  }
}


#[link(name = "ros2_robot_middleware__rosidl_typesupport_c")]
extern "C" {
    fn rosidl_typesupport_c__get_message_type_support_handle__ros2_robot_middleware__action__MoveToPose_GetResult_Response() -> *const std::ffi::c_void;
}

#[link(name = "ros2_robot_middleware__rosidl_generator_c")]
extern "C" {
    fn ros2_robot_middleware__action__MoveToPose_GetResult_Response__init(msg: *mut MoveToPose_GetResult_Response) -> bool;
    fn ros2_robot_middleware__action__MoveToPose_GetResult_Response__Sequence__init(seq: *mut rosidl_runtime_rs::Sequence<MoveToPose_GetResult_Response>, size: usize) -> bool;
    fn ros2_robot_middleware__action__MoveToPose_GetResult_Response__Sequence__fini(seq: *mut rosidl_runtime_rs::Sequence<MoveToPose_GetResult_Response>);
    fn ros2_robot_middleware__action__MoveToPose_GetResult_Response__Sequence__copy(in_seq: &rosidl_runtime_rs::Sequence<MoveToPose_GetResult_Response>, out_seq: *mut rosidl_runtime_rs::Sequence<MoveToPose_GetResult_Response>) -> bool;
}

// Corresponds to ros2_robot_middleware__action__MoveToPose_GetResult_Response
#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]


// This struct is not documented.
#[allow(missing_docs)]

#[allow(non_camel_case_types)]
#[repr(C)]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct MoveToPose_GetResult_Response {

    // This member is not documented.
    #[allow(missing_docs)]
    pub status: i8,


    // This member is not documented.
    #[allow(missing_docs)]
    pub result: super::super::action::rmw::MoveToPose_Result,

}



impl Default for MoveToPose_GetResult_Response {
  fn default() -> Self {
    unsafe {
      let mut msg = std::mem::zeroed();
      if !ros2_robot_middleware__action__MoveToPose_GetResult_Response__init(&mut msg as *mut _) {
        panic!("Call to ros2_robot_middleware__action__MoveToPose_GetResult_Response__init() failed");
      }
      msg
    }
  }
}

impl rosidl_runtime_rs::SequenceAlloc for MoveToPose_GetResult_Response {
  fn sequence_init(seq: &mut rosidl_runtime_rs::Sequence<Self>, size: usize) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { ros2_robot_middleware__action__MoveToPose_GetResult_Response__Sequence__init(seq as *mut _, size) }
  }
  fn sequence_fini(seq: &mut rosidl_runtime_rs::Sequence<Self>) {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { ros2_robot_middleware__action__MoveToPose_GetResult_Response__Sequence__fini(seq as *mut _) }
  }
  fn sequence_copy(in_seq: &rosidl_runtime_rs::Sequence<Self>, out_seq: &mut rosidl_runtime_rs::Sequence<Self>) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { ros2_robot_middleware__action__MoveToPose_GetResult_Response__Sequence__copy(in_seq, out_seq as *mut _) }
  }
}

impl rosidl_runtime_rs::Message for MoveToPose_GetResult_Response {
  type RmwMsg = Self;
  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> { msg_cow }
  fn from_rmw_message(msg: Self::RmwMsg) -> Self { msg }
}

impl rosidl_runtime_rs::RmwMessage for MoveToPose_GetResult_Response where Self: Sized {
  const TYPE_NAME: &'static str = "ros2_robot_middleware/action/MoveToPose_GetResult_Response";
  fn get_type_support() -> *const std::ffi::c_void {
    // SAFETY: No preconditions for this function.
    unsafe { rosidl_typesupport_c__get_message_type_support_handle__ros2_robot_middleware__action__MoveToPose_GetResult_Response() }
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


