#[cfg(feature = "serde")]
use serde::{Deserialize, Serialize};


#[link(name = "ros2_robot_middleware__rosidl_typesupport_c")]
extern "C" {
    fn rosidl_typesupport_c__get_message_type_support_handle__ros2_robot_middleware__msg__Object() -> *const std::ffi::c_void;
}

#[link(name = "ros2_robot_middleware__rosidl_generator_c")]
extern "C" {
    fn ros2_robot_middleware__msg__Object__init(msg: *mut Object) -> bool;
    fn ros2_robot_middleware__msg__Object__Sequence__init(seq: *mut rosidl_runtime_rs::Sequence<Object>, size: usize) -> bool;
    fn ros2_robot_middleware__msg__Object__Sequence__fini(seq: *mut rosidl_runtime_rs::Sequence<Object>);
    fn ros2_robot_middleware__msg__Object__Sequence__copy(in_seq: &rosidl_runtime_rs::Sequence<Object>, out_seq: *mut rosidl_runtime_rs::Sequence<Object>) -> bool;
}

// Corresponds to ros2_robot_middleware__msg__Object
#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]


// This struct is not documented.
#[allow(missing_docs)]

#[repr(C)]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct Object {
    /// 物体唯一标识
    pub id: rosidl_runtime_rs::String,

    /// base_link 坐标系, 前方 (m)
    pub x: f32,

    /// base_link 坐标系, 左方 (m)
    pub y: f32,

    /// base_link 坐标系, 上方 (m)
    pub z: f32,

}



impl Default for Object {
  fn default() -> Self {
    unsafe {
      let mut msg = std::mem::zeroed();
      if !ros2_robot_middleware__msg__Object__init(&mut msg as *mut _) {
        panic!("Call to ros2_robot_middleware__msg__Object__init() failed");
      }
      msg
    }
  }
}

impl rosidl_runtime_rs::SequenceAlloc for Object {
  fn sequence_init(seq: &mut rosidl_runtime_rs::Sequence<Self>, size: usize) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { ros2_robot_middleware__msg__Object__Sequence__init(seq as *mut _, size) }
  }
  fn sequence_fini(seq: &mut rosidl_runtime_rs::Sequence<Self>) {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { ros2_robot_middleware__msg__Object__Sequence__fini(seq as *mut _) }
  }
  fn sequence_copy(in_seq: &rosidl_runtime_rs::Sequence<Self>, out_seq: &mut rosidl_runtime_rs::Sequence<Self>) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { ros2_robot_middleware__msg__Object__Sequence__copy(in_seq, out_seq as *mut _) }
  }
}

impl rosidl_runtime_rs::Message for Object {
  type RmwMsg = Self;
  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> { msg_cow }
  fn from_rmw_message(msg: Self::RmwMsg) -> Self { msg }
}

impl rosidl_runtime_rs::RmwMessage for Object where Self: Sized {
  const TYPE_NAME: &'static str = "ros2_robot_middleware/msg/Object";
  fn get_type_support() -> *const std::ffi::c_void {
    // SAFETY: No preconditions for this function.
    unsafe { rosidl_typesupport_c__get_message_type_support_handle__ros2_robot_middleware__msg__Object() }
  }
}


#[link(name = "ros2_robot_middleware__rosidl_typesupport_c")]
extern "C" {
    fn rosidl_typesupport_c__get_message_type_support_handle__ros2_robot_middleware__msg__PerceptionObjects() -> *const std::ffi::c_void;
}

#[link(name = "ros2_robot_middleware__rosidl_generator_c")]
extern "C" {
    fn ros2_robot_middleware__msg__PerceptionObjects__init(msg: *mut PerceptionObjects) -> bool;
    fn ros2_robot_middleware__msg__PerceptionObjects__Sequence__init(seq: *mut rosidl_runtime_rs::Sequence<PerceptionObjects>, size: usize) -> bool;
    fn ros2_robot_middleware__msg__PerceptionObjects__Sequence__fini(seq: *mut rosidl_runtime_rs::Sequence<PerceptionObjects>);
    fn ros2_robot_middleware__msg__PerceptionObjects__Sequence__copy(in_seq: &rosidl_runtime_rs::Sequence<PerceptionObjects>, out_seq: *mut rosidl_runtime_rs::Sequence<PerceptionObjects>) -> bool;
}

// Corresponds to ros2_robot_middleware__msg__PerceptionObjects
#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]

/// Fused perception output — detected objects with positions

#[repr(C)]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct PerceptionObjects {

    // This member is not documented.
    #[allow(missing_docs)]
    pub header: std_msgs::msg::rmw::Header,


    // This member is not documented.
    #[allow(missing_docs)]
    pub objects: rosidl_runtime_rs::Sequence<super::super::msg::rmw::Object>,

}



impl Default for PerceptionObjects {
  fn default() -> Self {
    unsafe {
      let mut msg = std::mem::zeroed();
      if !ros2_robot_middleware__msg__PerceptionObjects__init(&mut msg as *mut _) {
        panic!("Call to ros2_robot_middleware__msg__PerceptionObjects__init() failed");
      }
      msg
    }
  }
}

impl rosidl_runtime_rs::SequenceAlloc for PerceptionObjects {
  fn sequence_init(seq: &mut rosidl_runtime_rs::Sequence<Self>, size: usize) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { ros2_robot_middleware__msg__PerceptionObjects__Sequence__init(seq as *mut _, size) }
  }
  fn sequence_fini(seq: &mut rosidl_runtime_rs::Sequence<Self>) {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { ros2_robot_middleware__msg__PerceptionObjects__Sequence__fini(seq as *mut _) }
  }
  fn sequence_copy(in_seq: &rosidl_runtime_rs::Sequence<Self>, out_seq: &mut rosidl_runtime_rs::Sequence<Self>) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { ros2_robot_middleware__msg__PerceptionObjects__Sequence__copy(in_seq, out_seq as *mut _) }
  }
}

impl rosidl_runtime_rs::Message for PerceptionObjects {
  type RmwMsg = Self;
  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> { msg_cow }
  fn from_rmw_message(msg: Self::RmwMsg) -> Self { msg }
}

impl rosidl_runtime_rs::RmwMessage for PerceptionObjects where Self: Sized {
  const TYPE_NAME: &'static str = "ros2_robot_middleware/msg/PerceptionObjects";
  fn get_type_support() -> *const std::ffi::c_void {
    // SAFETY: No preconditions for this function.
    unsafe { rosidl_typesupport_c__get_message_type_support_handle__ros2_robot_middleware__msg__PerceptionObjects() }
  }
}


#[link(name = "ros2_robot_middleware__rosidl_typesupport_c")]
extern "C" {
    fn rosidl_typesupport_c__get_message_type_support_handle__ros2_robot_middleware__msg__HealthStatus() -> *const std::ffi::c_void;
}

#[link(name = "ros2_robot_middleware__rosidl_generator_c")]
extern "C" {
    fn ros2_robot_middleware__msg__HealthStatus__init(msg: *mut HealthStatus) -> bool;
    fn ros2_robot_middleware__msg__HealthStatus__Sequence__init(seq: *mut rosidl_runtime_rs::Sequence<HealthStatus>, size: usize) -> bool;
    fn ros2_robot_middleware__msg__HealthStatus__Sequence__fini(seq: *mut rosidl_runtime_rs::Sequence<HealthStatus>);
    fn ros2_robot_middleware__msg__HealthStatus__Sequence__copy(in_seq: &rosidl_runtime_rs::Sequence<HealthStatus>, out_seq: *mut rosidl_runtime_rs::Sequence<HealthStatus>) -> bool;
}

// Corresponds to ros2_robot_middleware__msg__HealthStatus
#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]

/// Per-node health snapshot

#[repr(C)]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct HealthStatus {

    // This member is not documented.
    #[allow(missing_docs)]
    pub node_name: rosidl_runtime_rs::String,

    /// OK | WARN | ERROR | STALE
    pub status: rosidl_runtime_rs::String,

    /// seconds since last activity
    pub last_seen_s: f64,

    /// configured timeout for this node
    pub timeout_s: f64,

}



impl Default for HealthStatus {
  fn default() -> Self {
    unsafe {
      let mut msg = std::mem::zeroed();
      if !ros2_robot_middleware__msg__HealthStatus__init(&mut msg as *mut _) {
        panic!("Call to ros2_robot_middleware__msg__HealthStatus__init() failed");
      }
      msg
    }
  }
}

impl rosidl_runtime_rs::SequenceAlloc for HealthStatus {
  fn sequence_init(seq: &mut rosidl_runtime_rs::Sequence<Self>, size: usize) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { ros2_robot_middleware__msg__HealthStatus__Sequence__init(seq as *mut _, size) }
  }
  fn sequence_fini(seq: &mut rosidl_runtime_rs::Sequence<Self>) {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { ros2_robot_middleware__msg__HealthStatus__Sequence__fini(seq as *mut _) }
  }
  fn sequence_copy(in_seq: &rosidl_runtime_rs::Sequence<Self>, out_seq: &mut rosidl_runtime_rs::Sequence<Self>) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { ros2_robot_middleware__msg__HealthStatus__Sequence__copy(in_seq, out_seq as *mut _) }
  }
}

impl rosidl_runtime_rs::Message for HealthStatus {
  type RmwMsg = Self;
  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> { msg_cow }
  fn from_rmw_message(msg: Self::RmwMsg) -> Self { msg }
}

impl rosidl_runtime_rs::RmwMessage for HealthStatus where Self: Sized {
  const TYPE_NAME: &'static str = "ros2_robot_middleware/msg/HealthStatus";
  fn get_type_support() -> *const std::ffi::c_void {
    // SAFETY: No preconditions for this function.
    unsafe { rosidl_typesupport_c__get_message_type_support_handle__ros2_robot_middleware__msg__HealthStatus() }
  }
}


#[link(name = "ros2_robot_middleware__rosidl_typesupport_c")]
extern "C" {
    fn rosidl_typesupport_c__get_message_type_support_handle__ros2_robot_middleware__msg__HealthReport() -> *const std::ffi::c_void;
}

#[link(name = "ros2_robot_middleware__rosidl_generator_c")]
extern "C" {
    fn ros2_robot_middleware__msg__HealthReport__init(msg: *mut HealthReport) -> bool;
    fn ros2_robot_middleware__msg__HealthReport__Sequence__init(seq: *mut rosidl_runtime_rs::Sequence<HealthReport>, size: usize) -> bool;
    fn ros2_robot_middleware__msg__HealthReport__Sequence__fini(seq: *mut rosidl_runtime_rs::Sequence<HealthReport>);
    fn ros2_robot_middleware__msg__HealthReport__Sequence__copy(in_seq: &rosidl_runtime_rs::Sequence<HealthReport>, out_seq: *mut rosidl_runtime_rs::Sequence<HealthReport>) -> bool;
}

// Corresponds to ros2_robot_middleware__msg__HealthReport
#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]

/// Aggregate health report for all monitored nodes

#[repr(C)]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct HealthReport {

    // This member is not documented.
    #[allow(missing_docs)]
    pub header: std_msgs::msg::rmw::Header,


    // This member is not documented.
    #[allow(missing_docs)]
    pub nodes: rosidl_runtime_rs::Sequence<super::super::msg::rmw::HealthStatus>,

}



impl Default for HealthReport {
  fn default() -> Self {
    unsafe {
      let mut msg = std::mem::zeroed();
      if !ros2_robot_middleware__msg__HealthReport__init(&mut msg as *mut _) {
        panic!("Call to ros2_robot_middleware__msg__HealthReport__init() failed");
      }
      msg
    }
  }
}

impl rosidl_runtime_rs::SequenceAlloc for HealthReport {
  fn sequence_init(seq: &mut rosidl_runtime_rs::Sequence<Self>, size: usize) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { ros2_robot_middleware__msg__HealthReport__Sequence__init(seq as *mut _, size) }
  }
  fn sequence_fini(seq: &mut rosidl_runtime_rs::Sequence<Self>) {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { ros2_robot_middleware__msg__HealthReport__Sequence__fini(seq as *mut _) }
  }
  fn sequence_copy(in_seq: &rosidl_runtime_rs::Sequence<Self>, out_seq: &mut rosidl_runtime_rs::Sequence<Self>) -> bool {
    // SAFETY: This is safe since the pointer is guaranteed to be valid/initialized.
    unsafe { ros2_robot_middleware__msg__HealthReport__Sequence__copy(in_seq, out_seq as *mut _) }
  }
}

impl rosidl_runtime_rs::Message for HealthReport {
  type RmwMsg = Self;
  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> { msg_cow }
  fn from_rmw_message(msg: Self::RmwMsg) -> Self { msg }
}

impl rosidl_runtime_rs::RmwMessage for HealthReport where Self: Sized {
  const TYPE_NAME: &'static str = "ros2_robot_middleware/msg/HealthReport";
  fn get_type_support() -> *const std::ffi::c_void {
    // SAFETY: No preconditions for this function.
    unsafe { rosidl_typesupport_c__get_message_type_support_handle__ros2_robot_middleware__msg__HealthReport() }
  }
}


