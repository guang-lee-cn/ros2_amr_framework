// generated from rosidl_generator_c/resource/idl__functions.c.em
// with input from ros2_robot_middleware:msg/HealthReport.idl
// generated code does not contain a copyright notice
#include "ros2_robot_middleware/msg/detail/health_report__functions.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "rcutils/allocator.h"


// Include directives for member types
// Member `header`
#include "std_msgs/msg/detail/header__functions.h"
// Member `nodes`
#include "ros2_robot_middleware/msg/detail/health_status__functions.h"

bool
ros2_robot_middleware__msg__HealthReport__init(ros2_robot_middleware__msg__HealthReport * msg)
{
  if (!msg) {
    return false;
  }
  // header
  if (!std_msgs__msg__Header__init(&msg->header)) {
    ros2_robot_middleware__msg__HealthReport__fini(msg);
    return false;
  }
  // nodes
  if (!ros2_robot_middleware__msg__HealthStatus__Sequence__init(&msg->nodes, 0)) {
    ros2_robot_middleware__msg__HealthReport__fini(msg);
    return false;
  }
  return true;
}

void
ros2_robot_middleware__msg__HealthReport__fini(ros2_robot_middleware__msg__HealthReport * msg)
{
  if (!msg) {
    return;
  }
  // header
  std_msgs__msg__Header__fini(&msg->header);
  // nodes
  ros2_robot_middleware__msg__HealthStatus__Sequence__fini(&msg->nodes);
}

bool
ros2_robot_middleware__msg__HealthReport__are_equal(const ros2_robot_middleware__msg__HealthReport * lhs, const ros2_robot_middleware__msg__HealthReport * rhs)
{
  if (!lhs || !rhs) {
    return false;
  }
  // header
  if (!std_msgs__msg__Header__are_equal(
      &(lhs->header), &(rhs->header)))
  {
    return false;
  }
  // nodes
  if (!ros2_robot_middleware__msg__HealthStatus__Sequence__are_equal(
      &(lhs->nodes), &(rhs->nodes)))
  {
    return false;
  }
  return true;
}

bool
ros2_robot_middleware__msg__HealthReport__copy(
  const ros2_robot_middleware__msg__HealthReport * input,
  ros2_robot_middleware__msg__HealthReport * output)
{
  if (!input || !output) {
    return false;
  }
  // header
  if (!std_msgs__msg__Header__copy(
      &(input->header), &(output->header)))
  {
    return false;
  }
  // nodes
  if (!ros2_robot_middleware__msg__HealthStatus__Sequence__copy(
      &(input->nodes), &(output->nodes)))
  {
    return false;
  }
  return true;
}

ros2_robot_middleware__msg__HealthReport *
ros2_robot_middleware__msg__HealthReport__create(void)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  ros2_robot_middleware__msg__HealthReport * msg = (ros2_robot_middleware__msg__HealthReport *)allocator.allocate(sizeof(ros2_robot_middleware__msg__HealthReport), allocator.state);
  if (!msg) {
    return NULL;
  }
  memset(msg, 0, sizeof(ros2_robot_middleware__msg__HealthReport));
  bool success = ros2_robot_middleware__msg__HealthReport__init(msg);
  if (!success) {
    allocator.deallocate(msg, allocator.state);
    return NULL;
  }
  return msg;
}

void
ros2_robot_middleware__msg__HealthReport__destroy(ros2_robot_middleware__msg__HealthReport * msg)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  if (msg) {
    ros2_robot_middleware__msg__HealthReport__fini(msg);
  }
  allocator.deallocate(msg, allocator.state);
}


bool
ros2_robot_middleware__msg__HealthReport__Sequence__init(ros2_robot_middleware__msg__HealthReport__Sequence * array, size_t size)
{
  if (!array) {
    return false;
  }
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  ros2_robot_middleware__msg__HealthReport * data = NULL;

  if (size) {
    data = (ros2_robot_middleware__msg__HealthReport *)allocator.zero_allocate(size, sizeof(ros2_robot_middleware__msg__HealthReport), allocator.state);
    if (!data) {
      return false;
    }
    // initialize all array elements
    size_t i;
    for (i = 0; i < size; ++i) {
      bool success = ros2_robot_middleware__msg__HealthReport__init(&data[i]);
      if (!success) {
        break;
      }
    }
    if (i < size) {
      // if initialization failed finalize the already initialized array elements
      for (; i > 0; --i) {
        ros2_robot_middleware__msg__HealthReport__fini(&data[i - 1]);
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
ros2_robot_middleware__msg__HealthReport__Sequence__fini(ros2_robot_middleware__msg__HealthReport__Sequence * array)
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
      ros2_robot_middleware__msg__HealthReport__fini(&array->data[i]);
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

ros2_robot_middleware__msg__HealthReport__Sequence *
ros2_robot_middleware__msg__HealthReport__Sequence__create(size_t size)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  ros2_robot_middleware__msg__HealthReport__Sequence * array = (ros2_robot_middleware__msg__HealthReport__Sequence *)allocator.allocate(sizeof(ros2_robot_middleware__msg__HealthReport__Sequence), allocator.state);
  if (!array) {
    return NULL;
  }
  bool success = ros2_robot_middleware__msg__HealthReport__Sequence__init(array, size);
  if (!success) {
    allocator.deallocate(array, allocator.state);
    return NULL;
  }
  return array;
}

void
ros2_robot_middleware__msg__HealthReport__Sequence__destroy(ros2_robot_middleware__msg__HealthReport__Sequence * array)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  if (array) {
    ros2_robot_middleware__msg__HealthReport__Sequence__fini(array);
  }
  allocator.deallocate(array, allocator.state);
}

bool
ros2_robot_middleware__msg__HealthReport__Sequence__are_equal(const ros2_robot_middleware__msg__HealthReport__Sequence * lhs, const ros2_robot_middleware__msg__HealthReport__Sequence * rhs)
{
  if (!lhs || !rhs) {
    return false;
  }
  if (lhs->size != rhs->size) {
    return false;
  }
  for (size_t i = 0; i < lhs->size; ++i) {
    if (!ros2_robot_middleware__msg__HealthReport__are_equal(&(lhs->data[i]), &(rhs->data[i]))) {
      return false;
    }
  }
  return true;
}

bool
ros2_robot_middleware__msg__HealthReport__Sequence__copy(
  const ros2_robot_middleware__msg__HealthReport__Sequence * input,
  ros2_robot_middleware__msg__HealthReport__Sequence * output)
{
  if (!input || !output) {
    return false;
  }
  if (output->capacity < input->size) {
    const size_t allocation_size =
      input->size * sizeof(ros2_robot_middleware__msg__HealthReport);
    rcutils_allocator_t allocator = rcutils_get_default_allocator();
    ros2_robot_middleware__msg__HealthReport * data =
      (ros2_robot_middleware__msg__HealthReport *)allocator.reallocate(
      output->data, allocation_size, allocator.state);
    if (!data) {
      return false;
    }
    // If reallocation succeeded, memory may or may not have been moved
    // to fulfill the allocation request, invalidating output->data.
    output->data = data;
    for (size_t i = output->capacity; i < input->size; ++i) {
      if (!ros2_robot_middleware__msg__HealthReport__init(&output->data[i])) {
        // If initialization of any new item fails, roll back
        // all previously initialized items. Existing items
        // in output are to be left unmodified.
        for (; i-- > output->capacity; ) {
          ros2_robot_middleware__msg__HealthReport__fini(&output->data[i]);
        }
        return false;
      }
    }
    output->capacity = input->size;
  }
  output->size = input->size;
  for (size_t i = 0; i < input->size; ++i) {
    if (!ros2_robot_middleware__msg__HealthReport__copy(
        &(input->data[i]), &(output->data[i])))
    {
      return false;
    }
  }
  return true;
}
