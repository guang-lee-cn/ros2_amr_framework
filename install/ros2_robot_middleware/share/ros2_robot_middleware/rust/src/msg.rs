#[cfg(feature = "serde")]
use serde::{Deserialize, Serialize};



// Corresponds to ros2_robot_middleware__msg__Object

// This struct is not documented.
#[allow(missing_docs)]

#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct Object {
    /// 物体唯一标识
    pub id: std::string::String,

    /// base_link 坐标系, 前方 (m)
    pub x: f32,

    /// base_link 坐标系, 左方 (m)
    pub y: f32,

    /// base_link 坐标系, 上方 (m)
    pub z: f32,

}



impl Default for Object {
  fn default() -> Self {
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::msg::rmw::Object::default())
  }
}

impl rosidl_runtime_rs::Message for Object {
  type RmwMsg = super::msg::rmw::Object;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        id: msg.id.as_str().into(),
        x: msg.x,
        y: msg.y,
        z: msg.z,
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        id: msg.id.as_str().into(),
      x: msg.x,
      y: msg.y,
      z: msg.z,
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      id: msg.id.to_string(),
      x: msg.x,
      y: msg.y,
      z: msg.z,
    }
  }
}


// Corresponds to ros2_robot_middleware__msg__PerceptionObjects
/// Fused perception output — detected objects with positions

#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct PerceptionObjects {

    // This member is not documented.
    #[allow(missing_docs)]
    pub header: std_msgs::msg::Header,


    // This member is not documented.
    #[allow(missing_docs)]
    pub objects: Vec<super::msg::Object>,

}



impl Default for PerceptionObjects {
  fn default() -> Self {
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::msg::rmw::PerceptionObjects::default())
  }
}

impl rosidl_runtime_rs::Message for PerceptionObjects {
  type RmwMsg = super::msg::rmw::PerceptionObjects;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Owned(msg.header)).into_owned(),
        objects: msg.objects
          .into_iter()
          .map(|elem| super::msg::Object::into_rmw_message(std::borrow::Cow::Owned(elem)).into_owned())
          .collect(),
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Borrowed(&msg.header)).into_owned(),
        objects: msg.objects
          .iter()
          .map(|elem| super::msg::Object::into_rmw_message(std::borrow::Cow::Borrowed(elem)).into_owned())
          .collect(),
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      header: std_msgs::msg::Header::from_rmw_message(msg.header),
      objects: msg.objects
          .into_iter()
          .map(super::msg::Object::from_rmw_message)
          .collect(),
    }
  }
}


// Corresponds to ros2_robot_middleware__msg__HealthStatus
/// Per-node health snapshot

#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct HealthStatus {

    // This member is not documented.
    #[allow(missing_docs)]
    pub node_name: std::string::String,

    /// OK | WARN | ERROR | STALE
    pub status: std::string::String,

    /// seconds since last activity
    pub last_seen_s: f64,

    /// configured timeout for this node
    pub timeout_s: f64,

}



impl Default for HealthStatus {
  fn default() -> Self {
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::msg::rmw::HealthStatus::default())
  }
}

impl rosidl_runtime_rs::Message for HealthStatus {
  type RmwMsg = super::msg::rmw::HealthStatus;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        node_name: msg.node_name.as_str().into(),
        status: msg.status.as_str().into(),
        last_seen_s: msg.last_seen_s,
        timeout_s: msg.timeout_s,
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        node_name: msg.node_name.as_str().into(),
        status: msg.status.as_str().into(),
      last_seen_s: msg.last_seen_s,
      timeout_s: msg.timeout_s,
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      node_name: msg.node_name.to_string(),
      status: msg.status.to_string(),
      last_seen_s: msg.last_seen_s,
      timeout_s: msg.timeout_s,
    }
  }
}


// Corresponds to ros2_robot_middleware__msg__HealthReport
/// Aggregate health report for all monitored nodes

#[cfg_attr(feature = "serde", derive(Deserialize, Serialize))]
#[derive(Clone, Debug, PartialEq, PartialOrd)]
pub struct HealthReport {

    // This member is not documented.
    #[allow(missing_docs)]
    pub header: std_msgs::msg::Header,


    // This member is not documented.
    #[allow(missing_docs)]
    pub nodes: Vec<super::msg::HealthStatus>,

}



impl Default for HealthReport {
  fn default() -> Self {
    <Self as rosidl_runtime_rs::Message>::from_rmw_message(super::msg::rmw::HealthReport::default())
  }
}

impl rosidl_runtime_rs::Message for HealthReport {
  type RmwMsg = super::msg::rmw::HealthReport;

  fn into_rmw_message(msg_cow: std::borrow::Cow<'_, Self>) -> std::borrow::Cow<'_, Self::RmwMsg> {
    match msg_cow {
      std::borrow::Cow::Owned(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Owned(msg.header)).into_owned(),
        nodes: msg.nodes
          .into_iter()
          .map(|elem| super::msg::HealthStatus::into_rmw_message(std::borrow::Cow::Owned(elem)).into_owned())
          .collect(),
      }),
      std::borrow::Cow::Borrowed(msg) => std::borrow::Cow::Owned(Self::RmwMsg {
        header: std_msgs::msg::Header::into_rmw_message(std::borrow::Cow::Borrowed(&msg.header)).into_owned(),
        nodes: msg.nodes
          .iter()
          .map(|elem| super::msg::HealthStatus::into_rmw_message(std::borrow::Cow::Borrowed(elem)).into_owned())
          .collect(),
      })
    }
  }

  fn from_rmw_message(msg: Self::RmwMsg) -> Self {
    Self {
      header: std_msgs::msg::Header::from_rmw_message(msg.header),
      nodes: msg.nodes
          .into_iter()
          .map(super::msg::HealthStatus::from_rmw_message)
          .collect(),
    }
  }
}


