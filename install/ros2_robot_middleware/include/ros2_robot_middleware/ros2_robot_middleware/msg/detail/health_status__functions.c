// generated from rosidl_generator_c/resource/idl__functions.c.em
// with input from ros2_robot_middleware:msg/HealthStatus.idl
// generated code does not contain a copyright notice
#include "ros2_robot_middleware/msg/detail/health_status__functions.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "rcutils/allocator.h"


// Include directives for member types
// Member `node_name`
// Member `status`
#include "rosidl_runtime_c/string_functions.h"

bool
ros2_robot_middleware__msg__HealthStatus__init(ros2_robot_middleware__msg__HealthStatus * msg)
{
  if (!msg) {
    return false;
  }
  // node_name
  if (!rosidl_runtime_c__String__init(&msg->node_name)) {
    ros2_robot_middleware__msg__HealthStatus__fini(msg);
    return false;
  }
  // status
  if (!rosidl_runtime_c__String__init(&msg->status)) {
    ros2_robot_middleware__msg__HealthStatus__fini(msg);
    return false;
  }
  // last_seen_s
  // timeout_s
  return true;
}

void
ros2_robot_middleware__msg__HealthStatus__fini(ros2_robot_middleware__msg__HealthStatus * msg)
{
  if (!msg) {
    return;
  }
  // node_name
  rosidl_runtime_c__String__fini(&msg->node_name);
  // status
  rosidl_runtime_c__String__fini(&msg->status);
  // last_seen_s
  // timeout_s
}

bool
ros2_robot_middleware__msg__HealthStatus__are_equal(const ros2_robot_middleware__msg__HealthStatus * lhs, const ros2_robot_middleware__msg__HealthStatus * rhs)
{
  if (!lhs || !rhs) {
    return false;
  }
  // node_name
  if (!rosidl_runtime_c__String__are_equal(
      &(lhs->node_name), &(rhs->node_name)))
  {
    return false;
  }
  // status
  if (!rosidl_runtime_c__String__are_equal(
      &(lhs->status), &(rhs->status)))
  {
    return false;
  }
  // last_seen_s
  if (lhs->last_seen_s != rhs->last_seen_s) {
    return false;
  }
  // timeout_s
  if (lhs->timeout_s != rhs->timeout_s) {
    return false;
  }
  return true;
}

bool
ros2_robot_middleware__msg__HealthStatus__copy(
  const ros2_robot_middleware__msg__HealthStatus * input,
  ros2_robot_middleware__msg__HealthStatus * output)
{
  if (!input || !output) {
    return false;
  }
  // node_name
  if (!rosidl_runtime_c__String__copy(
      &(input->node_name), &(output->node_name)))
  {
    return false;
  }
  // status
  if (!rosidl_runtime_c__String__copy(
      &(input->status), &(output->status)))
  {
    return false;
  }
  // last_seen_s
  output->last_seen_s = input->last_seen_s;
  // timeout_s
  output->timeout_s = input->timeout_s;
  return true;
}

ros2_robot_middleware__msg__HealthStatus *
ros2_robot_middleware__msg__HealthStatus__create(void)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  ros2_robot_middleware__msg__HealthStatus * msg = (ros2_robot_middleware__msg__HealthStatus *)allocator.allocate(sizeof(ros2_robot_middleware__msg__HealthStatus), allocator.state);
  if (!msg) {
    return NULL;
  }
  memset(msg, 0, sizeof(ros2_robot_middleware__msg__HealthStatus));
  bool success = ros2_robot_middleware__msg__HealthStatus__init(msg);
  if (!success) {
    allocator.deallocate(msg, allocator.state);
    return NULL;
  }
  return msg;
}

void
ros2_robot_middleware__msg__HealthStatus__destroy(ros2_robot_middleware__msg__HealthStatus * msg)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  if (msg) {
    ros2_robot_middleware__msg__HealthStatus__fini(msg);
  }
  allocator.deallocate(msg, allocator.state);
}


bool
ros2_robot_middleware__msg__HealthStatus__Sequence__init(ros2_robot_middleware__msg__HealthStatus__Sequence * array, size_t size)
{
  if (!array) {
    return false;
  }
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  ros2_robot_middleware__msg__HealthStatus * data = NULL;

  if (size) {
    data = (ros2_robot_middleware__msg__HealthStatus *)allocator.zero_allocate(size, sizeof(ros2_robot_middleware__msg__HealthStatus), allocator.state);
    if (!data) {
      return false;
    }
    // initialize all array elements
    size_t i;
    for (i = 0; i < size; ++i) {
      bool success = ros2_robot_middleware__msg__HealthStatus__init(&data[i]);
      if (!success) {
        break;
      }
    }
    if (i < size) {
      // if initialization failed finalize the already initialized array elements
      for (; i > 0; --i) {
        ros2_robot_middleware__msg__HealthStatus__fini(&data[i - 1]);
      }
      allocator.deallocate(data, allocator.state);
      return false;
    }
  }
  array->data = data;
  array->size = size;
  array->capacity = size;
  return true;
}

void
ros2_robot_middleware__msg__HealthStatus__Sequence__fini(ros2_robot_middleware__msg__HealthStatus__Sequence * array)
{
  if (!array) {
    return;
  }
  rcutils_allocator_t allocator = rcutils_get_default_allocator();

  if (array->data) {
    // ensure that data and capacity values are consistent
    assert(array->capacity > 0);
    // finalize all array elements
    for (size_t i = 0; i < array->capacity; ++i) {
      ros2_robot_middleware__msg__HealthStatus__fini(&array->data[i]);
    }
    allocator.deallocate(array->data, allocator.state);
    array->data = NULL;
    array->size = 0;
    array->capacity = 0;
  } else {
    // ensure that data, size, and capacity values are consistent
    assert(0 == array->size);
    assert(0 == array->capacity);
  }
}

ros2_robot_middleware__msg__HealthStatus__Sequence *
ros2_robot_middleware__msg__HealthStatus__Sequence__create(size_t size)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  ros2_robot_middleware__msg__HealthStatus__Sequence * array = (ros2_robot_middleware__msg__HealthStatus__Sequence *)allocator.allocate(sizeof(ros2_robot_middleware__msg__HealthStatus__Sequence), allocator.state);
  if (!array) {
    return NULL;
  }
  bool success = ros2_robot_middleware__msg__HealthStatus__Sequence__init(array, size);
  if (!success) {
    allocator.deallocate(array, allocator.state);
    return NULL;
  }
  return array;
}

void
ros2_robot_middleware__msg__HealthStatus__Sequence__destroy(ros2_robot_middleware__msg__HealthStatus__Sequence * array)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  if (array) {
    ros2_robot_middleware__msg__HealthStatus__Sequence__fini(array);
  }
  allocator.deallocate(array, allocator.state);
}

bool
ros2_robot_middleware__msg__HealthStatus__Sequence__are_equal(const ros2_robot_middleware__msg__HealthStatus__Sequence * lhs, const ros2_robot_middleware__msg__HealthStatus__Sequence * rhs)
{
  if (!lhs || !rhs) {
    return false;
  }
  if (lhs->size != rhs->size) {
    return false;
  }
  for (size_t i = 0; i < lhs->size; ++i) {
    if (!ros2_robot_middleware__msg__HealthStatus__are_equal(&(lhs->data[i]), &(rhs->data[i]))) {
      return false;
    }
  }
  return true;
}

bool
ros2_robot_middleware__msg__HealthStatus__Sequence__copy(
  const ros2_robot_middleware__msg__HealthStatus__Sequence * input,
  ros2_robot_middleware__msg__HealthStatus__Sequence * output)
{
  if (!input || !output) {
    return false;
  }
  if (output->capacity < input->size) {
    const size_t allocation_size =
      input->size * sizeof(ros2_robot_middleware__msg__HealthStatus);
    rcutils_allocator_t allocator = rcutils_get_default_allocator();
    ros2_robot_middleware__msg__HealthStatus * data =
      (ros2_robot_middleware__msg__HealthStatus *)allocator.reallocate(
      output->data, allocation_size, allocator.state);
    if (!data) {
      return false;
    }
    // If reallocation succeeded, memory may or may not have been moved
    // to fulfill the allocation request, invalidating output->data.
    output->data = data;
    for (size_t i = output->capacity; i < input->size; ++i) {
      if (!ros2_robot_middleware__msg__HealthStatus__init(&output->data[i])) {
        // If initialization of any new item fails, roll back
        // all previously initialized items. Existing items
        // in output are to be left unmodified.
        for (; i-- > output->capacity; ) {
          ros2_robot_middleware__msg__HealthStatus__fini(&output->data[i]);
        }
        return false;
      }
    }
    output->capacity = input->size;
  }
  output->size = input->size;
  for (size_t i = 0; i < input->size; ++i) {
    if (!ros2_robot_middleware__msg__HealthStatus__copy(
        &(input->data[i]), &(output->data[i])))
    {
      return false;
    }
  }
  return true;
}
