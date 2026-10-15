// generated from rosidl_generator_c/resource/idl__description.c.em
// with input from vision_msgs:msg/VisionInfo.idl
// generated code does not contain a copyright notice

#include "vision_msgs/msg/detail/vision_info__functions.h"

ROSIDL_GENERATOR_C_PUBLIC_vision_msgs
const rosidl_type_hash_t *
vision_msgs__msg__VisionInfo__get_type_hash(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_type_hash_t hash = {1, {
      0x95, 0xd3, 0x11, 0x6a, 0x17, 0x48, 0x4a, 0xd9,
      0x1c, 0x9a, 0x26, 0x86, 0xe1, 0xd5, 0x48, 0x90,
      0xbb, 0xfc, 0x3f, 0xf6, 0xc6, 0xa2, 0xde, 0x13,
      0x77, 0x38, 0xc4, 0x95, 0x0d, 0x45, 0xd2, 0x84,
    }};
  return &hash;
}

#include <assert.h>
#include <string.h>

// Include directives for referenced types
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
#endif

static char vision_msgs__msg__VisionInfo__TYPE_NAME[] = "vision_msgs/msg/VisionInfo";
static char builtin_interfaces__msg__Time__TYPE_NAME[] = "builtin_interfaces/msg/Time";
static char std_msgs__msg__Header__TYPE_NAME[] = "std_msgs/msg/Header";

// Define type names, field names, and default values
static char vision_msgs__msg__VisionInfo__FIELD_NAME__header[] = "header";
static char vision_msgs__msg__VisionInfo__FIELD_NAME__method[] = "method";
static char vision_msgs__msg__VisionInfo__FIELD_NAME__database_location[] = "database_location";
static char vision_msgs__msg__VisionInfo__FIELD_NAME__database_version[] = "database_version";

static rosidl_runtime_c__type_description__Field vision_msgs__msg__VisionInfo__FIELDS[] = {
  {
    {vision_msgs__msg__VisionInfo__FIELD_NAME__header, 6, 6},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_NESTED_TYPE,
      0,
      0,
      {std_msgs__msg__Header__TYPE_NAME, 19, 19},
    },
    {NULL, 0, 0},
  },
  {
    {vision_msgs__msg__VisionInfo__FIELD_NAME__method, 6, 6},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_STRING,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {vision_msgs__msg__VisionInfo__FIELD_NAME__database_location, 17, 17},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_STRING,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {vision_msgs__msg__VisionInfo__FIELD_NAME__database_version, 16, 16},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_INT32,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
};

static rosidl_runtime_c__type_description__IndividualTypeDescription vision_msgs__msg__VisionInfo__REFERENCED_TYPE_DESCRIPTIONS[] = {
  {
    {builtin_interfaces__msg__Time__TYPE_NAME, 27, 27},
    {NULL, 0, 0},
  },
  {
    {std_msgs__msg__Header__TYPE_NAME, 19, 19},
    {NULL, 0, 0},
  },
};

const rosidl_runtime_c__type_description__TypeDescription *
vision_msgs__msg__VisionInfo__get_type_description(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static bool constructed = false;
  static const rosidl_runtime_c__type_description__TypeDescription description = {
    {
      {vision_msgs__msg__VisionInfo__TYPE_NAME, 26, 26},
      {vision_msgs__msg__VisionInfo__FIELDS, 4, 4},
    },
    {vision_msgs__msg__VisionInfo__REFERENCED_TYPE_DESCRIPTIONS, 2, 2},
  };
  if (!constructed) {
    assert(0 == memcmp(&builtin_interfaces__msg__Time__EXPECTED_HASH, builtin_interfaces__msg__Time__get_type_hash(NULL), sizeof(rosidl_type_hash_t)));
    description.referenced_type_descriptions.data[0].fields = builtin_interfaces__msg__Time__get_type_description(NULL)->type_description.fields;
    assert(0 == memcmp(&std_msgs__msg__Header__EXPECTED_HASH, std_msgs__msg__Header__get_type_hash(NULL), sizeof(rosidl_type_hash_t)));
    description.referenced_type_descriptions.data[1].fields = std_msgs__msg__Header__get_type_description(NULL)->type_description.fields;
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
  "# Each vision pipeline should publish its VisionInfo messages to its own topic,\n"
  "#   in a manner similar to CameraInfo.\n"
  "\n"
  "# Used for sequencing\n"
  "std_msgs/Header header\n"
  "\n"
  "# Name of the vision pipeline. This should be a value that is meaningful to an\n"
  "#   outside user.\n"
  "string method\n"
  "\n"
  "# Location where the metadata database is stored. The recommended location is\n"
  "#   as an XML string on the ROS parameter server, but the exact implementation\n"
  "#   and information is left up to the user.\n"
  "# The database should store information attached to class ids. Each\n"
  "#   class id should map to an atomic, visually recognizable element. This\n"
  "#   definition is intentionally vague to allow extreme flexibility. The\n"
  "#   elements could be classes in a pixel segmentation algorithm, object classes\n"
  "#   in a detector, different people's faces in a face detection algorithm, etc.\n"
  "#   Vision pipelines report results in terms of numeric IDs, which map into\n"
  "#   this  database.\n"
  "# The information stored in this database is, again, left up to the user. The\n"
  "#   database could be as simple as a map from ID to class name, or it could\n"
  "#   include information such as object meshes or colors to use for\n"
  "#   visualization.\n"
  "string database_location\n"
  "\n"
  "# Metadata database version. This counter is incremented\n"
  "#   each time the pipeline begins using a new version of the database (useful\n"
  "#   in the case of online training or user modifications).\n"
  "#   The counter value can be monitored by listeners to ensure that the pipeline\n"
  "#   and the listener are using the same metadata.\n"
  "int32 database_version";

static char msg_encoding[] = "msg";

// Define all individual source functions

const rosidl_runtime_c__type_description__TypeSource *
vision_msgs__msg__VisionInfo__get_individual_type_description_source(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static const rosidl_runtime_c__type_description__TypeSource source = {
    {vision_msgs__msg__VisionInfo__TYPE_NAME, 26, 26},
    {msg_encoding, 3, 3},
    {toplevel_type_raw_source, 2068, 2068},
  };
  return &source;
}

const rosidl_runtime_c__type_description__TypeSource__Sequence *
vision_msgs__msg__VisionInfo__get_type_description_sources(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_runtime_c__type_description__TypeSource sources[3];
  static const rosidl_runtime_c__type_description__TypeSource__Sequence source_sequence = {sources, 3, 3};
  static bool constructed = false;
  if (!constructed) {
    sources[0] = *vision_msgs__msg__VisionInfo__get_individual_type_description_source(NULL),
    sources[1] = *builtin_interfaces__msg__Time__get_individual_type_description_source(NULL);
    sources[2] = *std_msgs__msg__Header__get_individual_type_description_source(NULL);
    constructed = true;
  }
  return &source_sequence;
}
