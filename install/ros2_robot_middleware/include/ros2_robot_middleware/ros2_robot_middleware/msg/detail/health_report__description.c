// generated from rosidl_generator_c/resource/idl__description.c.em
// with input from ros2_robot_middleware:msg/HealthReport.idl
// generated code does not contain a copyright notice

#include "ros2_robot_middleware/msg/detail/health_report__functions.h"

ROSIDL_GENERATOR_C_PUBLIC_ros2_robot_middleware
const rosidl_type_hash_t *
ros2_robot_middleware__msg__HealthReport__get_type_hash(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_type_hash_t hash = {1, {
      0x84, 0x4d, 0xa7, 0x44, 0xce, 0x34, 0xc2, 0xd9,
      0x0e, 0xd1, 0x76, 0xc7, 0xea, 0xd3, 0xa1, 0xa0,
      0xe7, 0x8a, 0xbb, 0x2b, 0x9c, 0x77, 0x4c, 0x06,
      0xb3, 0xe1, 0x58, 0x53, 0x63, 0xc4, 0x43, 0xae,
    }};
  return &hash;
}

#include <assert.h>
#include <string.h>

// Include directives for referenced types
#include "ros2_robot_middleware/msg/detail/health_status__functions.h"
#include "std_msgs/msg/detail/header__functions.h"
#include "builtin_interfaces/msg/detail/time__functions.h"

// Hashes for external referenced types
#ifndef NDEBUG
static const rosidl_type_hash_t builtin_interfaces__msg__Time__EXPECTED_HASH = {1, {
    0xb1, 0x06, 0x23, 0x5e, 0x25, 0xa4, 0xc5, 0xed,
    0x35, 0x09, 0x8a, 0xa0, 0xa6, 0x1a, 0x3e, 0xe9,
    0xc9, 0xb1, 0x8d, 0x19, 0x7f, 0x39, 0x8b, 0x0e,
    0x42, 0x06, 0xce, 0xa9, 0xac, 0xf9, 0xc1, 0x97,
  }};
static const rosidl_type_hash_t ros2_robot_middleware__msg__HealthStatus__EXPECTED_HASH = {1, {
    0x7b, 0xc1, 0xd6, 0xc6, 0xf0, 0x37, 0x7d, 0x99,
    0xbd, 0x29, 0x5b, 0xee, 0xc8, 0x4a, 0x8b, 0xa2,
    0x0f, 0x34, 0x53, 0x5d, 0x4e, 0xfe, 0x75, 0x90,
    0x14, 0x11, 0x96, 0xf0, 0xbc, 0x21, 0x35, 0x2b,
  }};
static const rosidl_type_hash_t std_msgs__msg__Header__EXPECTED_HASH = {1, {
    0xf4, 0x9f, 0xb3, 0xae, 0x2c, 0xf0, 0x70, 0xf7,
    0x93, 0x64, 0x5f, 0xf7, 0x49, 0x68, 0x3a, 0xc6,
    0xb0, 0x62, 0x03, 0xe4, 0x1c, 0x89, 0x1e, 0x17,
    0x70, 0x1b, 0x1c, 0xb5, 0x97, 0xce, 0x6a, 0x01,
  }};
#endif

static char ros2_robot_middleware__msg__HealthReport__TYPE_NAME[] = "ros2_robot_middleware/msg/HealthReport";
static char builtin_interfaces__msg__Time__TYPE_NAME[] = "builtin_interfaces/msg/Time";
static char ros2_robot_middleware__msg__HealthStatus__TYPE_NAME[] = "ros2_robot_middleware/msg/HealthStatus";
static char std_msgs__msg__Header__TYPE_NAME[] = "std_msgs/msg/Header";

// Define type names, field names, and default values
static char ros2_robot_middleware__msg__HealthReport__FIELD_NAME__header[] = "header";
static char ros2_robot_middleware__msg__HealthReport__FIELD_NAME__nodes[] = "nodes";

static rosidl_runtime_c__type_description__Field ros2_robot_middleware__msg__HealthReport__FIELDS[] = {
  {
    {ros2_robot_middleware__msg__HealthReport__FIELD_NAME__header, 6, 6},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_NESTED_TYPE,
      0,
      0,
      {std_msgs__msg__Header__TYPE_NAME, 19, 19},
    },
    {NULL, 0, 0},
  },
  {
    {ros2_robot_middleware__msg__HealthReport__FIELD_NAME__nodes, 5, 5},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_NESTED_TYPE_UNBOUNDED_SEQUENCE,
      0,
      0,
      {ros2_robot_middleware__msg__HealthStatus__TYPE_NAME, 38, 38},
    },
    {NULL, 0, 0},
  },
};

static rosidl_runtime_c__type_description__IndividualTypeDescription ros2_robot_middleware__msg__HealthReport__REFERENCED_TYPE_DESCRIPTIONS[] = {
  {
    {builtin_interfaces__msg__Time__TYPE_NAME, 27, 27},
    {NULL, 0, 0},
  },
  {
    {ros2_robot_middleware__msg__HealthStatus__TYPE_NAME, 38, 38},
    {NULL, 0, 0},
  },
  {
    {std_msgs__msg__Header__TYPE_NAME, 19, 19},
    {NULL, 0, 0},
  },
};

const rosidl_runtime_c__type_description__TypeDescription *
ros2_robot_middleware__msg__HealthReport__get_type_description(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static bool constructed = false;
  static const rosidl_runtime_c__type_description__TypeDescription description = {
    {
      {ros2_robot_middleware__msg__HealthReport__TYPE_NAME, 38, 38},
      {ros2_robot_middleware__msg__HealthReport__FIELDS, 2, 2},
    },
    {ros2_robot_middleware__msg__HealthReport__REFERENCED_TYPE_DESCRIPTIONS, 3, 3},
  };
  if (!constructed) {
    assert(0 == memcmp(&builtin_interfaces__msg__Time__EXPECTED_HASH, builtin_interfaces__msg__Time__get_type_hash(NULL), sizeof(rosidl_type_hash_t)));
    description.referenced_type_descriptions.data[0].fields = builtin_interfaces__msg__Time__get_type_description(NULL)->type_description.fields;
    assert(0 == memcmp(&ros2_robot_middleware__msg__HealthStatus__EXPECTED_HASH, ros2_robot_middleware__msg__HealthStatus__get_type_hash(NULL), sizeof(rosidl_type_hash_t)));
    description.referenced_type_descriptions.data[1].fields = ros2_robot_middleware__msg__HealthStatus__get_type_description(NULL)->type_description.fields;
    assert(0 == memcmp(&std_msgs__msg__Header__EXPECTED_HASH, std_msgs__msg__Header__get_type_hash(NULL), sizeof(rosidl_type_hash_t)));
    description.referenced_type_descriptions.data[2].fields = std_msgs__msg__Header__get_type_description(NULL)->type_description.fields;
    constructed = true;
  }
  return &description;
}

static char toplevel_type_raw_source[] =
  "# Aggregate health report for all monitored nodes\n"
  "std_msgs/Header header\n"
  "HealthStatus[] nodes";

static char msg_encoding[] = "msg";

// Define all individual source functions

const rosidl_runtime_c__type_description__TypeSource *
ros2_robot_middleware__msg__HealthReport__get_individual_type_description_source(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static const rosidl_runtime_c__type_description__TypeSource source = {
    {ros2_robot_middleware__msg__HealthReport__TYPE_NAME, 38, 38},
    {msg_encoding, 3, 3},
    {toplevel_type_raw_source, 94, 94},
  };
  return &source;
}

const rosidl_runtime_c__type_description__TypeSource__Sequence *
ros2_robot_middleware__msg__HealthReport__get_type_description_sources(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_runtime_c__type_description__TypeSource sources[4];
  static const rosidl_runtime_c__type_description__TypeSource__Sequence source_sequence = {sources, 4, 4};
  static bool constructed = false;
  if (!constructed) {
    sources[0] = *ros2_robot_middleware__msg__HealthReport__get_individual_type_description_source(NULL),
    sources[1] = *builtin_interfaces__msg__Time__get_individual_type_description_source(NULL);
    sources[2] = *ros2_robot_middleware__msg__HealthStatus__get_individual_type_description_source(NULL);
    sources[3] = *std_msgs__msg__Header__get_individual_type_description_source(NULL);
    constructed = true;
  }
  return &source_sequence;
}
