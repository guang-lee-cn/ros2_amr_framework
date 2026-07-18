// generated from rosidl_generator_c/resource/idl__struct.h.em
// with input from ros2_robot_middleware:action/MoveToPose.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "ros2_robot_middleware/action/move_to_pose.h"


#ifndef ROS2_ROBOT_MIDDLEWARE__ACTION__DETAIL__MOVE_TO_POSE__STRUCT_H_
#define ROS2_ROBOT_MIDDLEWARE__ACTION__DETAIL__MOVE_TO_POSE__STRUCT_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


// Constants defined in the message

/// Struct defined in action/MoveToPose in the package ros2_robot_middleware.
typedef struct ros2_robot_middleware__action__MoveToPose_Goal
{
  /// Goal
  float target_x;
  float target_y;
  float target_theta;
  float max_speed;
} ros2_robot_middleware__action__MoveToPose_Goal;

// Struct for a sequence of ros2_robot_middleware__action__MoveToPose_Goal.
typedef struct ros2_robot_middleware__action__MoveToPose_Goal__Sequence
{
  ros2_robot_middleware__action__MoveToPose_Goal * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} ros2_robot_middleware__action__MoveToPose_Goal__Sequence;

// Constants defined in the message

/// Struct defined in action/MoveToPose in the package ros2_robot_middleware.
typedef struct ros2_robot_middleware__action__MoveToPose_Result
{
  bool reached;
  float final_x;
  float final_y;
  float elapsed_time;
} ros2_robot_middleware__action__MoveToPose_Result;

// Struct for a sequence of ros2_robot_middleware__action__MoveToPose_Result.
typedef struct ros2_robot_middleware__action__MoveToPose_Result__Sequence
{
  ros2_robot_middleware__action__MoveToPose_Result * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} ros2_robot_middleware__action__MoveToPose_Result__Sequence;

// Constants defined in the message

/// Struct defined in action/MoveToPose in the package ros2_robot_middleware.
typedef struct ros2_robot_middleware__action__MoveToPose_Feedback
{
  float current_x;
  float current_y;
  float distance_remaining;
  float percent_complete;
} ros2_robot_middleware__action__MoveToPose_Feedback;

// Struct for a sequence of ros2_robot_middleware__action__MoveToPose_Feedback.
typedef struct ros2_robot_middleware__action__MoveToPose_Feedback__Sequence
{
  ros2_robot_middleware__action__MoveToPose_Feedback * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} ros2_robot_middleware__action__MoveToPose_Feedback__Sequence;

// Constants defined in the message

// Include directives for member types
// Member 'goal_id'
#include "unique_identifier_msgs/msg/detail/uuid__struct.h"
// Member 'goal'
#include "ros2_robot_middleware/action/detail/move_to_pose__struct.h"

/// Struct defined in action/MoveToPose in the package ros2_robot_middleware.
typedef struct ros2_robot_middleware__action__MoveToPose_SendGoal_Request
{
  unique_identifier_msgs__msg__UUID goal_id;
  ros2_robot_middleware__action__MoveToPose_Goal goal;
} ros2_robot_middleware__action__MoveToPose_SendGoal_Request;

// Struct for a sequence of ros2_robot_middleware__action__MoveToPose_SendGoal_Request.
typedef struct ros2_robot_middleware__action__MoveToPose_SendGoal_Request__Sequence
{
  ros2_robot_middleware__action__MoveToPose_SendGoal_Request * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} ros2_robot_middleware__action__MoveToPose_SendGoal_Request__Sequence;

// Constants defined in the message

// Include directives for member types
// Member 'stamp'
#include "builtin_interfaces/msg/detail/time__struct.h"

/// Struct defined in action/MoveToPose in the package ros2_robot_middleware.
typedef struct ros2_robot_middleware__action__MoveToPose_SendGoal_Response
{
  bool accepted;
  builtin_interfaces__msg__Time stamp;
} ros2_robot_middleware__action__MoveToPose_SendGoal_Response;

// Struct for a sequence of ros2_robot_middleware__action__MoveToPose_SendGoal_Response.
typedef struct ros2_robot_middleware__action__MoveToPose_SendGoal_Response__Sequence
{
  ros2_robot_middleware__action__MoveToPose_SendGoal_Response * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} ros2_robot_middleware__action__MoveToPose_SendGoal_Response__Sequence;

// Constants defined in the message

// Include directives for member types
// Member 'info'
#include "service_msgs/msg/detail/service_event_info__struct.h"

// constants for array fields with an upper bound
// request
enum
{
  ros2_robot_middleware__action__MoveToPose_SendGoal_Event__request__MAX_SIZE = 1
};
// response
enum
{
  ros2_robot_middleware__action__MoveToPose_SendGoal_Event__response__MAX_SIZE = 1
};

/// Struct defined in action/MoveToPose in the package ros2_robot_middleware.
typedef struct ros2_robot_middleware__action__MoveToPose_SendGoal_Event
{
  service_msgs__msg__ServiceEventInfo info;
  ros2_robot_middleware__action__MoveToPose_SendGoal_Request__Sequence request;
  ros2_robot_middleware__action__MoveToPose_SendGoal_Response__Sequence response;
} ros2_robot_middleware__action__MoveToPose_SendGoal_Event;

// Struct for a sequence of ros2_robot_middleware__action__MoveToPose_SendGoal_Event.
typedef struct ros2_robot_middleware__action__MoveToPose_SendGoal_Event__Sequence
{
  ros2_robot_middleware__action__MoveToPose_SendGoal_Event * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} ros2_robot_middleware__action__MoveToPose_SendGoal_Event__Sequence;

// Constants defined in the message

// Include directives for member types
// Member 'goal_id'
// already included above
// #include "unique_identifier_msgs/msg/detail/uuid__struct.h"

/// Struct defined in action/MoveToPose in the package ros2_robot_middleware.
typedef struct ros2_robot_middleware__action__MoveToPose_GetResult_Request
{
  unique_identifier_msgs__msg__UUID goal_id;
} ros2_robot_middleware__action__MoveToPose_GetResult_Request;

// Struct for a sequence of ros2_robot_middleware__action__MoveToPose_GetResult_Request.
typedef struct ros2_robot_middleware__action__MoveToPose_GetResult_Request__Sequence
{
  ros2_robot_middleware__action__MoveToPose_GetResult_Request * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} ros2_robot_middleware__action__MoveToPose_GetResult_Request__Sequence;

// Constants defined in the message

// Include directives for member types
// Member 'result'
// already included above
// #include "ros2_robot_middleware/action/detail/move_to_pose__struct.h"

/// Struct defined in action/MoveToPose in the package ros2_robot_middleware.
typedef struct ros2_robot_middleware__action__MoveToPose_GetResult_Response
{
  int8_t status;
  ros2_robot_middleware__action__MoveToPose_Result result;
} ros2_robot_middleware__action__MoveToPose_GetResult_Response;

// Struct for a sequence of ros2_robot_middleware__action__MoveToPose_GetResult_Response.
typedef struct ros2_robot_middleware__action__MoveToPose_GetResult_Response__Sequence
{
  ros2_robot_middleware__action__MoveToPose_GetResult_Response * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} ros2_robot_middleware__action__MoveToPose_GetResult_Response__Sequence;

// Constants defined in the message

// Include directives for member types
// Member 'info'
// already included above
// #include "service_msgs/msg/detail/service_event_info__struct.h"

// constants for array fields with an upper bound
// request
enum
{
  ros2_robot_middleware__action__MoveToPose_GetResult_Event__request__MAX_SIZE = 1
};
// response
enum
{
  ros2_robot_middleware__action__MoveToPose_GetResult_Event__response__MAX_SIZE = 1
};

/// Struct defined in action/MoveToPose in the package ros2_robot_middleware.
typedef struct ros2_robot_middleware__action__MoveToPose_GetResult_Event
{
  service_msgs__msg__ServiceEventInfo info;
  ros2_robot_middleware__action__MoveToPose_GetResult_Request__Sequence request;
  ros2_robot_middleware__action__MoveToPose_GetResult_Response__Sequence response;
} ros2_robot_middleware__action__MoveToPose_GetResult_Event;

// Struct for a sequence of ros2_robot_middleware__action__MoveToPose_GetResult_Event.
typedef struct ros2_robot_middleware__action__MoveToPose_GetResult_Event__Sequence
{
  ros2_robot_middleware__action__MoveToPose_GetResult_Event * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} ros2_robot_middleware__action__MoveToPose_GetResult_Event__Sequence;

// Constants defined in the message

// Include directives for member types
// Member 'goal_id'
// already included above
// #include "unique_identifier_msgs/msg/detail/uuid__struct.h"
// Member 'feedback'
// already included above
// #include "ros2_robot_middleware/action/detail/move_to_pose__struct.h"

/// Struct defined in action/MoveToPose in the package ros2_robot_middleware.
typedef struct ros2_robot_middleware__action__MoveToPose_FeedbackMessage
{
  unique_identifier_msgs__msg__UUID goal_id;
  ros2_robot_middleware__action__MoveToPose_Feedback feedback;
} ros2_robot_middleware__action__MoveToPose_FeedbackMessage;

// Struct for a sequence of ros2_robot_middleware__action__MoveToPose_FeedbackMessage.
typedef struct ros2_robot_middleware__action__MoveToPose_FeedbackMessage__Sequence
{
  ros2_robot_middleware__action__MoveToPose_FeedbackMessage * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} ros2_robot_middleware__action__MoveToPose_FeedbackMessage__Sequence;

#ifdef __cplusplus
}
#endif

#endif  // ROS2_ROBOT_MIDDLEWARE__ACTION__DETAIL__MOVE_TO_POSE__STRUCT_H_
