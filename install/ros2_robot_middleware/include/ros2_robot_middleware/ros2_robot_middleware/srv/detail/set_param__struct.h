// generated from rosidl_generator_c/resource/idl__struct.h.em
// with input from ros2_robot_middleware:srv/SetParam.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "ros2_robot_middleware/srv/set_param.h"


#ifndef ROS2_ROBOT_MIDDLEWARE__SRV__DETAIL__SET_PARAM__STRUCT_H_
#define ROS2_ROBOT_MIDDLEWARE__SRV__DETAIL__SET_PARAM__STRUCT_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


// Constants defined in the message

// Include directives for member types
// Member 'param_name'
#include "rosidl_runtime_c/string.h"

/// Struct defined in srv/SetParam in the package ros2_robot_middleware.
typedef struct ros2_robot_middleware__srv__SetParam_Request
{
  /// parameter key
  rosidl_runtime_c__String param_name;
  /// new value
  double value;
} ros2_robot_middleware__srv__SetParam_Request;

// Struct for a sequence of ros2_robot_middleware__srv__SetParam_Request.
typedef struct ros2_robot_middleware__srv__SetParam_Request__Sequence
{
  ros2_robot_middleware__srv__SetParam_Request * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} ros2_robot_middleware__srv__SetParam_Request__Sequence;

// Constants defined in the message

// Include directives for member types
// Member 'message'
// already included above
// #include "rosidl_runtime_c/string.h"

/// Struct defined in srv/SetParam in the package ros2_robot_middleware.
typedef struct ros2_robot_middleware__srv__SetParam_Response
{
  /// true if applied
  bool success;
  /// human-readable result
  rosidl_runtime_c__String message;
} ros2_robot_middleware__srv__SetParam_Response;

// Struct for a sequence of ros2_robot_middleware__srv__SetParam_Response.
typedef struct ros2_robot_middleware__srv__SetParam_Response__Sequence
{
  ros2_robot_middleware__srv__SetParam_Response * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} ros2_robot_middleware__srv__SetParam_Response__Sequence;

// Constants defined in the message

// Include directives for member types
// Member 'info'
#include "service_msgs/msg/detail/service_event_info__struct.h"

// constants for array fields with an upper bound
// request
enum
{
  ros2_robot_middleware__srv__SetParam_Event__request__MAX_SIZE = 1
};
// response
enum
{
  ros2_robot_middleware__srv__SetParam_Event__response__MAX_SIZE = 1
};

/// Struct defined in srv/SetParam in the package ros2_robot_middleware.
typedef struct ros2_robot_middleware__srv__SetParam_Event
{
  service_msgs__msg__ServiceEventInfo info;
  ros2_robot_middleware__srv__SetParam_Request__Sequence request;
  ros2_robot_middleware__srv__SetParam_Response__Sequence response;
} ros2_robot_middleware__srv__SetParam_Event;

// Struct for a sequence of ros2_robot_middleware__srv__SetParam_Event.
typedef struct ros2_robot_middleware__srv__SetParam_Event__Sequence
{
  ros2_robot_middleware__srv__SetParam_Event * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} ros2_robot_middleware__srv__SetParam_Event__Sequence;

#ifdef __cplusplus
}
#endif

#endif  // ROS2_ROBOT_MIDDLEWARE__SRV__DETAIL__SET_PARAM__STRUCT_H_
