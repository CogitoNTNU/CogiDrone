// generated from rosidl_generator_c/resource/idl__description.c.em
// with input from vision_msgs:msg/Pose2D.idl
// generated code does not contain a copyright notice

#include "vision_msgs/msg/detail/pose2_d__functions.h"

ROSIDL_GENERATOR_C_PUBLIC_vision_msgs
const rosidl_type_hash_t *
vision_msgs__msg__Pose2D__get_type_hash(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_type_hash_t hash = {1, {
      0xb0, 0xb5, 0xd1, 0xd7, 0xb4, 0xc2, 0x0d, 0xd4,
      0xfc, 0xde, 0x3e, 0xd1, 0xd4, 0xb2, 0x89, 0xa9,
      0x19, 0x29, 0x05, 0x72, 0x00, 0x7d, 0x67, 0xa9,
      0x70, 0x43, 0x81, 0x4d, 0xb2, 0x5d, 0x36, 0x48,
    }};
  return &hash;
}

#include <assert.h>
#include <string.h>

// Include directives for referenced types
#include "vision_msgs/msg/detail/point2_d__functions.h"

// Hashes for external referenced types
#ifndef NDEBUG
static const rosidl_type_hash_t vision_msgs__msg__Point2D__EXPECTED_HASH = {1, {
    0xea, 0xb0, 0xe8, 0x3f, 0x44, 0xab, 0x4d, 0xe9,
    0x2c, 0xea, 0xf7, 0x6d, 0xd7, 0xd0, 0xe7, 0x63,
    0x8f, 0x2b, 0xdd, 0x1d, 0x16, 0xff, 0xbe, 0x16,
    0xc7, 0xa1, 0x36, 0xc4, 0x8e, 0x40, 0x72, 0x47,
  }};
#endif

static char vision_msgs__msg__Pose2D__TYPE_NAME[] = "vision_msgs/msg/Pose2D";
static char vision_msgs__msg__Point2D__TYPE_NAME[] = "vision_msgs/msg/Point2D";

// Define type names, field names, and default values
static char vision_msgs__msg__Pose2D__FIELD_NAME__position[] = "position";
static char vision_msgs__msg__Pose2D__FIELD_NAME__theta[] = "theta";

static rosidl_runtime_c__type_description__Field vision_msgs__msg__Pose2D__FIELDS[] = {
  {
    {vision_msgs__msg__Pose2D__FIELD_NAME__position, 8, 8},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_NESTED_TYPE,
      0,
      0,
      {vision_msgs__msg__Point2D__TYPE_NAME, 23, 23},
    },
    {NULL, 0, 0},
  },
  {
    {vision_msgs__msg__Pose2D__FIELD_NAME__theta, 5, 5},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_DOUBLE,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
};

static rosidl_runtime_c__type_description__IndividualTypeDescription vision_msgs__msg__Pose2D__REFERENCED_TYPE_DESCRIPTIONS[] = {
  {
    {vision_msgs__msg__Point2D__TYPE_NAME, 23, 23},
    {NULL, 0, 0},
  },
};

const rosidl_runtime_c__type_description__TypeDescription *
vision_msgs__msg__Pose2D__get_type_description(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static bool constructed = false;
  static const rosidl_runtime_c__type_description__TypeDescription description = {
    {
      {vision_msgs__msg__Pose2D__TYPE_NAME, 22, 22},
      {vision_msgs__msg__Pose2D__FIELDS, 2, 2},
    },
    {vision_msgs__msg__Pose2D__REFERENCED_TYPE_DESCRIPTIONS, 1, 1},
  };
  if (!constructed) {
    assert(0 == memcmp(&vision_msgs__msg__Point2D__EXPECTED_HASH, vision_msgs__msg__Point2D__get_type_hash(NULL), sizeof(rosidl_type_hash_t)));
    description.referenced_type_descriptions.data[0].fields = vision_msgs__msg__Point2D__get_type_description(NULL)->type_description.fields;
    constructed = true;
  }
  return &description;
}

static char toplevel_type_raw_source[] =
  "# Represents a 2D pose (coordinates and a radian rotation). Rotation is positive counterclockwise.\n"
  "\n"
  "vision_msgs/Point2D position\n"
  "float64 theta";

static char msg_encoding[] = "msg";

// Define all individual source functions

const rosidl_runtime_c__type_description__TypeSource *
vision_msgs__msg__Pose2D__get_individual_type_description_source(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static const rosidl_runtime_c__type_description__TypeSource source = {
    {vision_msgs__msg__Pose2D__TYPE_NAME, 22, 22},
    {msg_encoding, 3, 3},
    {toplevel_type_raw_source, 143, 143},
  };
  return &source;
}

const rosidl_runtime_c__type_description__TypeSource__Sequence *
vision_msgs__msg__Pose2D__get_type_description_sources(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_runtime_c__type_description__TypeSource sources[2];
  static const rosidl_runtime_c__type_description__TypeSource__Sequence source_sequence = {sources, 2, 2};
  static bool constructed = false;
  if (!constructed) {
    sources[0] = *vision_msgs__msg__Pose2D__get_individual_type_description_source(NULL),
    sources[1] = *vision_msgs__msg__Point2D__get_individual_type_description_source(NULL);
    constructed = true;
  }
  return &source_sequence;
}
