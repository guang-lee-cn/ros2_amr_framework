#[cfg(feature = "serde")]
use serde::{Deserialize, Serialize};




// Corresponds to ros2_robot_middleware__srv__SetParam_Request

// This struct is not documented.
#[allow(missing_docs)]

#[allow(non_camel_case_types)]
#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct SetParam_Request {
    /// parameter key
    pub param_name: std::string::String,

    /// new value
    pub value: f64,

}



impl Default for SetParam_Request {
  fn default() -> Self {
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::srv::rmw::SetParam_Request::default())
  }
}

impl rosidl_runtime_rs::Message for SetParam_Request {
  type RmwMsg = super::srv::rmw::SetParam_Request;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        param_name: msg.param_name.as_str().into(),
        value: msg.value,
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        param_name: msg.param_name.as_str().into(),
      value: msg.value,
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      param_name: msg.param_name.to_string(),
      value: msg.value,
    }
  }
}


// Corresponds to ros2_robot_middleware__srv__SetParam_Response

// This struct is not documented.
#[allow(missing_docs)]

#[allow(non_camel_case_types)]
#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct SetParam_Response {
    /// true if applied
    pub success: bool,

    /// human-readable result
    pub message: std::string::String,

}



impl Default for SetParam_Response {
  fn default() -> Self {
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::srv::rmw::SetParam_Response::default())
  }
}

impl rosidl_runtime_rs::Message for SetParam_Response {
  type RmwMsg = super::srv::rmw::SetParam_Response;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        success: msg.success,
        message: msg.message.as_str().into(),
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
      success: msg.success,
        message: msg.message.as_str().into(),
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      success: msg.success,
      message: msg.message.to_string(),
    }
  }
}






#[link(name = "ros2_robot_middleware__rosidl_typesupport_c")]
extern "C" {
    fn rosidl_typesupport_c__get_service_type_support_handle__ros2_robot_middleware__srv__SetParam() -> *const std::ffi::c_void;
}

// Corresponds to ros2_robot_middleware__srv__SetParam
#[allow(missing_docs, non_camel_case_types)]
pub struct SetParam;

impl rosidl_runtime_rs::Service for SetParam {
    type Request = SetParam_Request;
    type Response = SetParam_Response;

    fn get_type_support() -> *const std::ffi::c_void {
        // SAFETY: No preconditions for this function.
        unsafe { rosidl_typesupport_c__get_service_type_support_handle__ros2_robot_middleware__srv__SetParam() }
    }
}


