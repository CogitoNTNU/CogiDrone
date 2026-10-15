// generated from rosidl_generator_c/resource/idl__description.c.em
// with input from vision_msgs:msg/Point2D.idl
// generated code does not contain a copyright notice

#include "vision_msgs/msg/detail/point2_d__functions.h"

ROSIDL_GENERATOR_C_PUBLIC_vision_msgs
const rosidl_type_hash_t *
vision_msgs__msg__Point2D__get_type_hash(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_type_hash_t hash = {1, {
      0xea, 0xb0, 0xe8, 0x3f, 0x44, 0xab, 0x4d, 0xe9,
      0x2c, 0xea, 0xf7, 0x6d, 0xd7, 0xd0, 0xe7, 0x63,
      0x8f, 0x2b, 0xdd, 0x1d, 0x16, 0xff, 0xbe, 0x16,
      0xc7, 0xa1, 0x36, 0xc4, 0x8e, 0x40, 0x72, 0x47,
    }};
  return &hash;
}

#include <assert.h>
#include <string.h>

// Include directives for referenced types

// Hashes for external referenced types
#ifndef NDEBUG
#endif

static char vision_msgs__msg__Point2D__TYPE_NAME[] = "vision_msgs/msg/Point2D";

// Define type names, field names, and default values
static char vision_msgs__msg__Point2D__FIELD_NAME__x[] = "x";
static char vision_msgs__msg__Point2D__FIELD_NAME__y[] = "y";

static rosidl_runtime_c__type_description__Field vision_msgs__msg__Point2D__FIELDS[] = {
  {
    {vision_msgs__msg__Point2D__FIELD_NAME__x, 1, 1},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_DOUBLE,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {vision_msgs__msg__Point2D__FIELD_NAME__y, 1, 1},
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
vision_msgs__msg__Point2D__get_type_description(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static bool constructed = false;
  static const rosidl_runtime_c__type_description__TypeDescription description = {
    {
      {vision_msgs__msg__Point2D__TYPE_NAME, 23, 23},
      {vision_msgs__msg__Point2D__FIELDS, 2, 2},
    },
    {NULL, 0, 0},
  };
  if (!constructed) {
    constructed = true;
  }
  return &description;
}

static char toplevel_type_raw_source[] =
  "# Represents a 2D point in pixel coordinates.\n"
  "# XY matches the sensor_msgs/Image convention: X is positive right and Y is positive down.\n"
  "\n"
  "float64 x\n"
  "float64 y";

static char msg_encoding[] = "msg";

// Define all individual source functions

const rosidl_runtime_c__type_description__TypeSource *
vision_msgs__msg__Point2D__get_individual_type_description_source(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static const rosidl_runtime_c__type_description__TypeSource source = {
    {vision_msgs__msg__Point2D__TYPE_NAME, 23, 23},
    {msg_encoding, 3, 3},
    {toplevel_type_raw_source, 158, 158},
  };
  return &source;
}

const rosidl_runtime_c__type_description__TypeSource__Sequence *
vision_msgs__msg__Point2D__get_type_description_sources(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_runtime_c__type_description__TypeSource sources[1];
  static const rosidl_runtime_c__type_description__TypeSource__Sequence source_sequence = {sources, 1, 1};
  static bool constructed = false;
  if (!constructed) {
    sources[0] = *vision_msgs__msg__Point2D__get_individual_type_description_source(NULL),
    constructed = true;
  }
  return &source_sequence;
}
