// generated from rosidl_generator_c/resource/idl__functions.c.em
// with input from ros2_robot_middleware:msg/Object.idl
// generated code does not contain a copyright notice
#include "ros2_robot_middleware/msg/detail/object__functions.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "rcutils/allocator.h"


// Include directives for member types
// Member `id`
#include "rosidl_runtime_c/string_functions.h"

bool
ros2_robot_middleware__msg__Object__init(ros2_robot_middleware__msg__Object * msg)
{
  if (!msg) {
    return false;
  }
  // id
  if (!rosidl_runtime_c__String__init(&msg->id)) {
    ros2_robot_middleware__msg__Object__fini(msg);
    return false;
  }
  // x
  // y
  // z
  return true;
}

void
ros2_robot_middleware__msg__Object__fini(ros2_robot_middleware__msg__Object * msg)
{
  if (!msg) {
    return;
  }
  // id
  rosidl_runtime_c__String__fini(&msg->id);
  // x
  // y
  // z
}

bool
ros2_robot_middleware__msg__Object__are_equal(const ros2_robot_middleware__msg__Object * lhs, const ros2_robot_middleware__msg__Object * rhs)
{
  if (!lhs || !rhs) {
    return false;
  }
  // id
  if (!rosidl_runtime_c__String__are_equal(
      &(lhs->id), &(rhs->id)))
  {
    return false;
  }
  // x
  if (lhs->x != rhs->x) {
    return false;
  }
  // y
  if (lhs->y != rhs->y) {
    return false;
  }
  // z
  if (lhs->z != rhs->z) {
    return false;
  }
  return true;
}

bool
ros2_robot_middleware__msg__Object__copy(
  const ros2_robot_middleware__msg__Object * input,
  ros2_robot_middleware__msg__Object * output)
{
  if (!input || !output) {
    return false;
  }
  // id
  if (!rosidl_runtime_c__String__copy(
      &(input->id), &(output->id)))
  {
    return false;
  }
  // x
  output->x = input->x;
  // y
  output->y = input->y;
  // z
  output->z = input->z;
  return true;
}

ros2_robot_middleware__msg__Object *
ros2_robot_middleware__msg__Object__create(void)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  ros2_robot_middleware__msg__Object * msg = (ros2_robot_middleware__msg__Object *)allocator.allocate(sizeof(ros2_robot_middleware__msg__Object), allocator.state);
  if (!msg) {
    return NULL;
  }
  memset(msg, 0, sizeof(ros2_robot_middleware__msg__Object));
  bool success = ros2_robot_middleware__msg__Object__init(msg);
  if (!success) {
    allocator.deallocate(msg, allocator.state);
    return NULL;
  }
  return msg;
}

void
ros2_robot_middleware__msg__Object__destroy(ros2_robot_middleware__msg__Object * msg)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  if (msg) {
    ros2_robot_middleware__msg__Object__fini(msg);
  }
  allocator.deallocate(msg, allocator.state);
}


bool
ros2_robot_middleware__msg__Object__Sequence__init(ros2_robot_middleware__msg__Object__Sequence * array, size_t size)
{
  if (!array) {
    return false;
  }
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  ros2_robot_middleware__msg__Object * data = NULL;

  if (size) {
    data = (ros2_robot_middleware__msg__Object *)allocator.zero_allocate(size, sizeof(ros2_robot_middleware__msg__Object), allocator.state);
    if (!data) {
      return false;
    }
    // initialize all array elements
    size_t i;
    for (i = 0; i < size; ++i) {
      bool success = ros2_robot_middleware__msg__Object__init(&data[i]);
      if (!success) {
        break;
      }
    }
    if (i < size) {
      // if initialization failed finalize the already initialized array elements
      for (; i > 0; --i) {
        ros2_robot_middleware__msg__Object__fini(&data[i - 1]);
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
ros2_robot_middleware__msg__Object__Sequence__fini(ros2_robot_middleware__msg__Object__Sequence * array)
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
      ros2_robot_middleware__msg__Object__fini(&array->data[i]);
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

ros2_robot_middleware__msg__Object__Sequence *
ros2_robot_middleware__msg__Object__Sequence__create(size_t size)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  ros2_robot_middleware__msg__Object__Sequence * array = (ros2_robot_middleware__msg__Object__Sequence *)allocator.allocate(sizeof(ros2_robot_middleware__msg__Object__Sequence), allocator.state);
  if (!array) {
    return NULL;
  }
  bool success = ros2_robot_middleware__msg__Object__Sequence__init(array, size);
  if (!success) {
    allocator.deallocate(array, allocator.state);
    return NULL;
  }
  return array;
}

void
ros2_robot_middleware__msg__Object__Sequence__destroy(ros2_robot_middleware__msg__Object__Sequence * array)
{
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  if (array) {
    ros2_robot_middleware__msg__Object__Sequence__fini(array);
  }
  allocator.deallocate(array, allocator.state);
}

bool
ros2_robot_middleware__msg__Object__Sequence__are_equal(const ros2_robot_middleware__msg__Object__Sequence * lhs, const ros2_robot_middleware__msg__Object__Sequence * rhs)
{
  if (!lhs || !rhs) {
    return false;
  }
  if (lhs->size != rhs->size) {
    return false;
  }
  for (size_t i = 0; i < lhs->size; ++i) {
    if (!ros2_robot_middleware__msg__Object__are_equal(&(lhs->data[i]), &(rhs->data[i]))) {
      return false;
    }
  }
  return true;
}

bool
ros2_robot_middleware__msg__Object__Sequence__copy(
  const ros2_robot_middleware__msg__Object__Sequence * input,
  ros2_robot_middleware__msg__Object__Sequence * output)
{
  if (!input || !output) {
    return false;
  }
  if (output->capacity < input->size) {
    const size_t allocation_size =
      input->size * sizeof(ros2_robot_middleware__msg__Object);
    rcutils_allocator_t allocator = rcutils_get_default_allocator();
    ros2_robot_middleware__msg__Object * data =
      (ros2_robot_middleware__msg__Object *)allocator.reallocate(
      output->data, allocation_size, allocator.state);
    if (!data) {
      return false;
    }
    // If reallocation succeeded, memory may or may not have been moved
    // to fulfill the allocation request, invalidating output->data.
    output->data = data;
    for (size_t i = output->capacity; i < input->size; ++i) {
      if (!ros2_robot_middleware__msg__Object__init(&output->data[i])) {
        // If initialization of any new item fails, roll back
        // all previously initialized items. Existing items
        // in output are to be left unmodified.
        for (; i-- > output->capacity; ) {
          ros2_robot_middleware__msg__Object__fini(&output->data[i]);
        }
        return false;
      }
    }
    output->capacity = input->size;
  }
  output->size = input->size;
  for (size_t i = 0; i < input->size; ++i) {
    if (!ros2_robot_middleware__msg__Object__copy(
        &(input->data[i]), &(output->data[i])))
    {
      return false;
    }
  }
  return true;
}
