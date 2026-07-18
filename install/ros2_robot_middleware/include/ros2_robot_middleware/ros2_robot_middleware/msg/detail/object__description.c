// generated from rosidl_generator_c/resource/idl__description.c.em
// with input from ros2_robot_middleware:msg/Object.idl
// generated code does not contain a copyright notice

#include "ros2_robot_middleware/msg/detail/object__functions.h"

ROSIDL_GENERATOR_C_PUBLIC_ros2_robot_middleware
const rosidl_type_hash_t *
ros2_robot_middleware__msg__Object__get_type_hash(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_type_hash_t hash = {1, {
      0xdb, 0xbf, 0x07, 0x2f, 0x59, 0x1c, 0xfc, 0x54,
      0xf4, 0x04, 0x11, 0xa3, 0x4a, 0x1b, 0x75, 0xc6,
      0x47, 0xf7, 0x05, 0x74, 0x50, 0xc3, 0x8d, 0x24,
      0xaf, 0xb9, 0xfa, 0xb3, 0x30, 0x2a, 0x9a, 0xd1,
    }};
  return &hash;
}

#include <assert.h>
#include <string.h>

// Include directives for referenced types

// Hashes for external referenced types
#ifndef NDEBUG
#endif

static char ros2_robot_middleware__msg__Object__TYPE_NAME[] = "ros2_robot_middleware/msg/Object";

// Define type names, field names, and default values
static char ros2_robot_middleware__msg__Object__FIELD_NAME__id[] = "id";
static char ros2_robot_middleware__msg__Object__FIELD_NAME__x[] = "x";
static char ros2_robot_middleware__msg__Object__FIELD_NAME__y[] = "y";
static char ros2_robot_middleware__msg__Object__FIELD_NAME__z[] = "z";

static rosidl_runtime_c__type_description__Field ros2_robot_middleware__msg__Object__FIELDS[] = {
  {
    {ros2_robot_middleware__msg__Object__FIELD_NAME__id, 2, 2},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_STRING,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {ros2_robot_middleware__msg__Object__FIELD_NAME__x, 1, 1},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_FLOAT,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {ros2_robot_middleware__msg__Object__FIELD_NAME__y, 1, 1},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_FLOAT,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {ros2_robot_middleware__msg__Object__FIELD_NAME__z, 1, 1},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_FLOAT,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
};

const rosidl_runtime_c__type_description__TypeDescription *
ros2_robot_middleware__msg__Object__get_type_description(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static bool constructed = false;
  static const rosidl_runtime_c__type_description__TypeDescription description = {
    {
      {ros2_robot_middleware__msg__Object__TYPE_NAME, 32, 32},
      {ros2_robot_middleware__msg__Object__FIELDS, 4, 4},
    },
    {NULL, 0, 0},
  };
  if (!constructed) {
    constructed = true;
  }
  return &description;
}

static char toplevel_type_raw_source[] =
  "string  id        # \\xe7\\x89\\xa9\\xe4\\xbd\\x93\\xe5\\x94\\xaf\\xe4\\xb8\\x80\\xe6\\xa0\\x87\\xe8\\xaf\\x86\n"
  "float32 x         # base_link \\xe5\\x9d\\x90\\xe6\\xa0\\x87\\xe7\\xb3\\xbb, \\xe5\\x89\\x8d\\xe6\\x96\\xb9 (m)\n"
  "float32 y         # base_link \\xe5\\x9d\\x90\\xe6\\xa0\\x87\\xe7\\xb3\\xbb, \\xe5\\xb7\\xa6\\xe6\\x96\\xb9 (m)\n"
  "float32 z         # base_link \\xe5\\x9d\\x90\\xe6\\xa0\\x87\\xe7\\xb3\\xbb, \\xe4\\xb8\\x8a\\xe6\\x96\\xb9 (m)";

static char msg_encoding[] = "msg";

// Define all individual source functions

const rosidl_runtime_c__type_description__TypeSource *
ros2_robot_middleware__msg__Object__get_individual_type_description_source(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static const rosidl_runtime_c__type_description__TypeSource source = {
    {ros2_robot_middleware__msg__Object__TYPE_NAME, 32, 32},
    {msg_encoding, 3, 3},
    {toplevel_type_raw_source, 153, 153},
  };
  return &source;
}

const rosidl_runtime_c__type_description__TypeSource__Sequence *
ros2_robot_middleware__msg__Object__get_type_description_sources(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_runtime_c__type_description__TypeSource sources[1];
  static const rosidl_runtime_c__type_description__TypeSource__Sequence source_sequence = {sources, 1, 1};
  static bool constructed = false;
  if (!constructed) {
    sources[0] = *ros2_robot_middleware__msg__Object__get_individual_type_description_source(NULL),
    constructed = true;
  }
  return &source_sequence;
}
