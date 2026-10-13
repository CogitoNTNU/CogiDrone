// generated from rosidl_generator_c/resource/idl__description.c.em
// with input from vision_msgs:msg/ObjectHypothesis.idl
// generated code does not contain a copyright notice

#include "vision_msgs/msg/detail/object_hypothesis__functions.h"

ROSIDL_GENERATOR_C_PUBLIC_vision_msgs
const rosidl_type_hash_t *
vision_msgs__msg__ObjectHypothesis__get_type_hash(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_type_hash_t hash = {1, {
      0x51, 0xda, 0x43, 0xce, 0xc1, 0x1c, 0x01, 0x95,
      0x7a, 0x64, 0xd1, 0xe0, 0x5a, 0x24, 0xac, 0xd1,
      0x8a, 0xae, 0x77, 0x45, 0xec, 0x30, 0xb7, 0x43,
      0x48, 0xad, 0x5f, 0x6d, 0x63, 0x4f, 0x7f, 0x90,
    }};
  return &hash;
}

#include <assert.h>
#include <string.h>

// Include directives for referenced types

// Hashes for external referenced types
#ifndef NDEBUG
#endif

static char vision_msgs__msg__ObjectHypothesis__TYPE_NAME[] = "vision_msgs/msg/ObjectHypothesis";

// Define type names, field names, and default values
static char vision_msgs__msg__ObjectHypothesis__FIELD_NAME__class_id[] = "class_id";
static char vision_msgs__msg__ObjectHypothesis__FIELD_NAME__score[] = "score";

static rosidl_runtime_c__type_description__Field vision_msgs__msg__ObjectHypothesis__FIELDS[] = {
  {
    {vision_msgs__msg__ObjectHypothesis__FIELD_NAME__class_id, 8, 8},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_STRING,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {vision_msgs__msg__ObjectHypothesis__FIELD_NAME__score, 5, 5},
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
vision_msgs__msg__ObjectHypothesis__get_type_description(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static bool constructed = false;
  static const rosidl_runtime_c__type_description__TypeDescription description = {
    {
      {vision_msgs__msg__ObjectHypothesis__TYPE_NAME, 32, 32},
      {vision_msgs__msg__ObjectHypothesis__FIELDS, 2, 2},
    },
    {NULL, 0, 0},
  };
  if (!constructed) {
    constructed = true;
  }
  return &description;
}

static char toplevel_type_raw_source[] =
  "# An object hypothesis that contains no pose information.\n"
  "# If you would like to define an array of ObjectHypothesis messages,\n"
  "#   please see the Classification message type.\n"
  "\n"
  "# The unique ID of the object class. To get additional information about\n"
  "#   this ID, such as its human-readable class name, listeners should perform a\n"
  "#   lookup in a metadata database. See vision_msgs/VisionInfo.msg for more detail.\n"
  "string class_id\n"
  "\n"
  "# The probability or confidence value of the detected object. By convention,\n"
  "#   this value should lie in the range [0-1].\n"
  "float64 score";

static char msg_encoding[] = "msg";

// Define all individual source functions

const rosidl_runtime_c__type_description__TypeSource *
vision_msgs__msg__ObjectHypothesis__get_individual_type_description_source(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static const rosidl_runtime_c__type_description__TypeSource source = {
    {vision_msgs__msg__ObjectHypothesis__TYPE_NAME, 32, 32},
    {msg_encoding, 3, 3},
    {toplevel_type_raw_source, 565, 565},
  };
  return &source;
}

const rosidl_runtime_c__type_description__TypeSource__Sequence *
vision_msgs__msg__ObjectHypothesis__get_type_description_sources(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_runtime_c__type_description__TypeSource sources[1];
  static const rosidl_runtime_c__type_description__TypeSource__Sequence source_sequence = {sources, 1, 1};
  static bool constructed = false;
  if (!constructed) {
    sources[0] = *vision_msgs__msg__ObjectHypothesis__get_individual_type_description_source(NULL),
    constructed = true;
  }
  return &source_sequence;
}
