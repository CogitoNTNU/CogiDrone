// generated from rosidl_generator_c/resource/idl__description.c.em
// with input from vision_msgs:msg/LabelInfo.idl
// generated code does not contain a copyright notice

#include "vision_msgs/msg/detail/label_info__functions.h"

ROSIDL_GENERATOR_C_PUBLIC_vision_msgs
const rosidl_type_hash_t *
vision_msgs__msg__LabelInfo__get_type_hash(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_type_hash_t hash = {1, {
      0x3d, 0x37, 0xdf, 0x40, 0x15, 0xcd, 0xf6, 0x60,
      0xcc, 0x85, 0x1c, 0x51, 0x89, 0x7f, 0x2a, 0xcd,
      0x95, 0x7e, 0x1c, 0xa6, 0x20, 0x5b, 0xa0, 0xa9,
      0x9a, 0xeb, 0xff, 0xb9, 0x40, 0x6b, 0x07, 0x83,
    }};
  return &hash;
}

#include <assert.h>
#include <string.h>

// Include directives for referenced types
#include "vision_msgs/msg/detail/vision_class__functions.h"
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
static const rosidl_type_hash_t std_msgs__msg__Header__EXPECTED_HASH = {1, {
    0xf4, 0x9f, 0xb3, 0xae, 0x2c, 0xf0, 0x70, 0xf7,
    0x93, 0x64, 0x5f, 0xf7, 0x49, 0x68, 0x3a, 0xc6,
    0xb0, 0x62, 0x03, 0xe4, 0x1c, 0x89, 0x1e, 0x17,
    0x70, 0x1b, 0x1c, 0xb5, 0x97, 0xce, 0x6a, 0x01,
  }};
static const rosidl_type_hash_t vision_msgs__msg__VisionClass__EXPECTED_HASH = {1, {
    0x1e, 0x4a, 0x11, 0xf0, 0xfe, 0xd5, 0x0b, 0x56,
    0x23, 0xcb, 0x8f, 0xf2, 0x6c, 0xa0, 0xe3, 0x79,
    0x52, 0xa4, 0x8a, 0x9c, 0x07, 0x4d, 0x16, 0xf8,
    0x04, 0xb4, 0x4e, 0x91, 0x64, 0xc2, 0x03, 0x7a,
  }};
#endif

static char vision_msgs__msg__LabelInfo__TYPE_NAME[] = "vision_msgs/msg/LabelInfo";
static char builtin_interfaces__msg__Time__TYPE_NAME[] = "builtin_interfaces/msg/Time";
static char std_msgs__msg__Header__TYPE_NAME[] = "std_msgs/msg/Header";
static char vision_msgs__msg__VisionClass__TYPE_NAME[] = "vision_msgs/msg/VisionClass";

// Define type names, field names, and default values
static char vision_msgs__msg__LabelInfo__FIELD_NAME__header[] = "header";
static char vision_msgs__msg__LabelInfo__FIELD_NAME__class_map[] = "class_map";
static char vision_msgs__msg__LabelInfo__FIELD_NAME__threshold[] = "threshold";

static rosidl_runtime_c__type_description__Field vision_msgs__msg__LabelInfo__FIELDS[] = {
  {
    {vision_msgs__msg__LabelInfo__FIELD_NAME__header, 6, 6},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_NESTED_TYPE,
      0,
      0,
      {std_msgs__msg__Header__TYPE_NAME, 19, 19},
    },
    {NULL, 0, 0},
  },
  {
    {vision_msgs__msg__LabelInfo__FIELD_NAME__class_map, 9, 9},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_NESTED_TYPE_UNBOUNDED_SEQUENCE,
      0,
      0,
      {vision_msgs__msg__VisionClass__TYPE_NAME, 27, 27},
    },
    {NULL, 0, 0},
  },
  {
    {vision_msgs__msg__LabelInfo__FIELD_NAME__threshold, 9, 9},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_FLOAT,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
};

static rosidl_runtime_c__type_description__IndividualTypeDescription vision_msgs__msg__LabelInfo__REFERENCED_TYPE_DESCRIPTIONS[] = {
  {
    {builtin_interfaces__msg__Time__TYPE_NAME, 27, 27},
    {NULL, 0, 0},
  },
  {
    {std_msgs__msg__Header__TYPE_NAME, 19, 19},
    {NULL, 0, 0},
  },
  {
    {vision_msgs__msg__VisionClass__TYPE_NAME, 27, 27},
    {NULL, 0, 0},
  },
};

const rosidl_runtime_c__type_description__TypeDescription *
vision_msgs__msg__LabelInfo__get_type_description(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static bool constructed = false;
  static const rosidl_runtime_c__type_description__TypeDescription description = {
    {
      {vision_msgs__msg__LabelInfo__TYPE_NAME, 25, 25},
      {vision_msgs__msg__LabelInfo__FIELDS, 3, 3},
    },
    {vision_msgs__msg__LabelInfo__REFERENCED_TYPE_DESCRIPTIONS, 3, 3},
  };
  if (!constructed) {
    assert(0 == memcmp(&builtin_interfaces__msg__Time__EXPECTED_HASH, builtin_interfaces__msg__Time__get_type_hash(NULL), sizeof(rosidl_type_hash_t)));
    description.referenced_type_descriptions.data[0].fields = builtin_interfaces__msg__Time__get_type_description(NULL)->type_description.fields;
    assert(0 == memcmp(&std_msgs__msg__Header__EXPECTED_HASH, std_msgs__msg__Header__get_type_hash(NULL), sizeof(rosidl_type_hash_t)));
    description.referenced_type_descriptions.data[1].fields = std_msgs__msg__Header__get_type_description(NULL)->type_description.fields;
    assert(0 == memcmp(&vision_msgs__msg__VisionClass__EXPECTED_HASH, vision_msgs__msg__VisionClass__get_type_hash(NULL), sizeof(rosidl_type_hash_t)));
    description.referenced_type_descriptions.data[2].fields = vision_msgs__msg__VisionClass__get_type_description(NULL)->type_description.fields;
    constructed = true;
  }
  return &description;
}

static char toplevel_type_raw_source[] =
  "# Provides meta-information about a visual pipeline.\n"
  "#\n"
  "# This message serves a similar purpose to sensor_msgs/CameraInfo, but instead\n"
  "#   of being tied to hardware, it represents information about a specific\n"
  "#   computer vision pipeline. This information stays constant (or relatively\n"
  "#   constant) over time, and so it is wasteful to send it with each individual\n"
  "#   result. By listening to these messages, subscribers will receive\n"
  "#   the context in which published vision messages are to be interpreted.\n"
  "# Each vision pipeline should publish its LabelInfo messages to its own topic,\n"
  "#   in a manner similar to CameraInfo.\n"
  "# This message is meant to allow converting data from vision pipelines that\n"
  "#   return id based classifications back to human readable string class names.\n"
  "\n"
  "# Used for sequencing\n"
  "std_msgs/Header header\n"
  "\n"
  "# An array of uint16 keys and string values containing the association\n"
  "#   between class identifiers and their names. According to the amount\n"
  "#   of classes and the datatype used to store their ids internally, the\n"
  "#   maxiumum class id allowed (65535 for uint16 and 255 for uint8) belongs to\n"
  "#   the \"UNLABELED\" class.\n"
  "vision_msgs/VisionClass[] class_map \n"
  "\n"
  "# The value between 0-1 used as confidence threshold for the inference.\n"
  "float32 threshold";

static char msg_encoding[] = "msg";

// Define all individual source functions

const rosidl_runtime_c__type_description__TypeSource *
vision_msgs__msg__LabelInfo__get_individual_type_description_source(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static const rosidl_runtime_c__type_description__TypeSource source = {
    {vision_msgs__msg__LabelInfo__TYPE_NAME, 25, 25},
    {msg_encoding, 3, 3},
    {toplevel_type_raw_source, 1274, 1274},
  };
  return &source;
}

const rosidl_runtime_c__type_description__TypeSource__Sequence *
vision_msgs__msg__LabelInfo__get_type_description_sources(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_runtime_c__type_description__TypeSource sources[4];
  static const rosidl_runtime_c__type_description__TypeSource__Sequence source_sequence = {sources, 4, 4};
  static bool constructed = false;
  if (!constructed) {
    sources[0] = *vision_msgs__msg__LabelInfo__get_individual_type_description_source(NULL),
    sources[1] = *builtin_interfaces__msg__Time__get_individual_type_description_source(NULL);
    sources[2] = *std_msgs__msg__Header__get_individual_type_description_source(NULL);
    sources[3] = *vision_msgs__msg__VisionClass__get_individual_type_description_source(NULL);
    constructed = true;
  }
  return &source_sequence;
}
