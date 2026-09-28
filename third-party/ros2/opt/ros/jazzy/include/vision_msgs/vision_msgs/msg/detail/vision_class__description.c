// generated from rosidl_generator_c/resource/idl__description.c.em
// with input from vision_msgs:msg/VisionClass.idl
// generated code does not contain a copyright notice

#include "vision_msgs/msg/detail/vision_class__functions.h"

ROSIDL_GENERATOR_C_PUBLIC_vision_msgs
const rosidl_type_hash_t *
vision_msgs__msg__VisionClass__get_type_hash(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_type_hash_t hash = {1, {
      0x1e, 0x4a, 0x11, 0xf0, 0xfe, 0xd5, 0x0b, 0x56,
      0x23, 0xcb, 0x8f, 0xf2, 0x6c, 0xa0, 0xe3, 0x79,
      0x52, 0xa4, 0x8a, 0x9c, 0x07, 0x4d, 0x16, 0xf8,
      0x04, 0xb4, 0x4e, 0x91, 0x64, 0xc2, 0x03, 0x7a,
    }};
  return &hash;
}

#include <assert.h>
#include <string.h>

// Include directives for referenced types

// Hashes for external referenced types
#ifndef NDEBUG
#endif

static char vision_msgs__msg__VisionClass__TYPE_NAME[] = "vision_msgs/msg/VisionClass";

// Define type names, field names, and default values
static char vision_msgs__msg__VisionClass__FIELD_NAME__class_id[] = "class_id";
static char vision_msgs__msg__VisionClass__FIELD_NAME__class_name[] = "class_name";

static rosidl_runtime_c__type_description__Field vision_msgs__msg__VisionClass__FIELDS[] = {
  {
    {vision_msgs__msg__VisionClass__FIELD_NAME__class_id, 8, 8},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_UINT16,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {vision_msgs__msg__VisionClass__FIELD_NAME__class_name, 10, 10},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_STRING,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
};

const rosidl_runtime_c__type_description__TypeDescription *
vision_msgs__msg__VisionClass__get_type_description(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static bool constructed = false;
  static const rosidl_runtime_c__type_description__TypeDescription description = {
    {
      {vision_msgs__msg__VisionClass__TYPE_NAME, 27, 27},
      {vision_msgs__msg__VisionClass__FIELDS, 2, 2},
    },
    {NULL, 0, 0},
  };
  if (!constructed) {
    constructed = true;
  }
  return &description;
}

static char toplevel_type_raw_source[] =
  "# A key value pair that maps an integer class_id to a string class label\n"
  "#   in computer vision systems.\n"
  "\n"
  "# The int value that identifies the class.\n"
  "# Elements identified with 65535, the maximum uint16 value are assumed\n"
  "#   to belong to the \"UNLABELED\" class. For vision pipelines using less\n"
  "#   than 255 classes the \"UNLABELED\" is the maximum value in the uint8\n"
  "#   range.\n"
  "uint16 class_id\n"
  "\n"
  "# The name of the class represented by the class_id\n"
  "string class_name";

static char msg_encoding[] = "msg";

// Define all individual source functions

const rosidl_runtime_c__type_description__TypeSource *
vision_msgs__msg__VisionClass__get_individual_type_description_source(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static const rosidl_runtime_c__type_description__TypeSource source = {
    {vision_msgs__msg__VisionClass__TYPE_NAME, 27, 27},
    {msg_encoding, 3, 3},
    {toplevel_type_raw_source, 461, 461},
  };
  return &source;
}

const rosidl_runtime_c__type_description__TypeSource__Sequence *
vision_msgs__msg__VisionClass__get_type_description_sources(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_runtime_c__type_description__TypeSource sources[1];
  static const rosidl_runtime_c__type_description__TypeSource__Sequence source_sequence = {sources, 1, 1};
  static bool constructed = false;
  if (!constructed) {
    sources[0] = *vision_msgs__msg__VisionClass__get_individual_type_description_source(NULL),
    constructed = true;
  }
  return &source_sequence;
}
