// generated from rosidl_generator_c/resource/idl__description.c.em
// with input from ros2_robot_middleware:msg/HealthStatus.idl
// generated code does not contain a copyright notice

#include "ros2_robot_middleware/msg/detail/health_status__functions.h"

ROSIDL_GENERATOR_C_PUBLIC_ros2_robot_middleware
const rosidl_type_hash_t *
ros2_robot_middleware__msg__HealthStatus__get_type_hash(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_type_hash_t hash = {1, {
      0x7b, 0xc1, 0xd6, 0xc6, 0xf0, 0x37, 0x7d, 0x99,
      0xbd, 0x29, 0x5b, 0xee, 0xc8, 0x4a, 0x8b, 0xa2,
      0x0f, 0x34, 0x53, 0x5d, 0x4e, 0xfe, 0x75, 0x90,
      0x14, 0x11, 0x96, 0xf0, 0xbc, 0x21, 0x35, 0x2b,
    }};
  return &hash;
}

#include <assert.h>
#include <string.h>

// Include directives for referenced types

// Hashes for external referenced types
#ifndef NDEBUG
#endif

static char ros2_robot_middleware__msg__HealthStatus__TYPE_NAME[] = "ros2_robot_middleware/msg/HealthStatus";

// Define type names, field names, and default values
static char ros2_robot_middleware__msg__HealthStatus__FIELD_NAME__node_name[] = "node_name";
static char ros2_robot_middleware__msg__HealthStatus__FIELD_NAME__status[] = "status";
static char ros2_robot_middleware__msg__HealthStatus__FIELD_NAME__last_seen_s[] = "last_seen_s";
static char ros2_robot_middleware__msg__HealthStatus__FIELD_NAME__timeout_s[] = "timeout_s";

static rosidl_runtime_c__type_description__Field ros2_robot_middleware__msg__HealthStatus__FIELDS[] = {
  {
    {ros2_robot_middleware__msg__HealthStatus__FIELD_NAME__node_name, 9, 9},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_STRING,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {ros2_robot_middleware__msg__HealthStatus__FIELD_NAME__status, 6, 6},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_STRING,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {ros2_robot_middleware__msg__HealthStatus__FIELD_NAME__last_seen_s, 11, 11},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_DOUBLE,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {ros2_robot_middleware__msg__HealthStatus__FIELD_NAME__timeout_s, 9, 9},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_DOUBLE,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
};

const rosidl_runtime_c__type_description__TypeDescription *
ros2_robot_middleware__msg__HealthStatus__get_type_description(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static bool constructed = false;
  static const rosidl_runtime_c__type_description__TypeDescription description = {
    {
      {ros2_robot_middleware__msg__HealthStatus__TYPE_NAME, 38, 38},
      {ros2_robot_middleware__msg__HealthStatus__FIELDS, 4, 4},
    },
    {NULL, 0, 0},
  };
  if (!constructed) {
    constructed = true;
  }
  return &description;
}

static char toplevel_type_raw_source[] =
  "# Per-node health snapshot\n"
  "string node_name\n"
  "string status        # OK | WARN | ERROR | STALE\n"
  "float64 last_seen_s  # seconds since last activity\n"
  "float64 timeout_s    # configured timeout for this node";

static char msg_encoding[] = "msg";

// Define all individual source functions

const rosidl_runtime_c__type_description__TypeSource *
ros2_robot_middleware__msg__HealthStatus__get_individual_type_description_source(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static const rosidl_runtime_c__type_description__TypeSource source = {
    {ros2_robot_middleware__msg__HealthStatus__TYPE_NAME, 38, 38},
    {msg_encoding, 3, 3},
    {toplevel_type_raw_source, 200, 200},
  };
  return &source;
}

const rosidl_runtime_c__type_description__TypeSource__Sequence *
ros2_robot_middleware__msg__HealthStatus__get_type_description_sources(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_runtime_c__type_description__TypeSource sources[1];
  static const rosidl_runtime_c__type_description__TypeSource__Sequence source_sequence = {sources, 1, 1};
  static bool constructed = false;
  if (!constructed) {
    sources[0] = *ros2_robot_middleware__msg__HealthStatus__get_individual_type_description_source(NULL),
    constructed = true;
  }
  return &source_sequence;
}
