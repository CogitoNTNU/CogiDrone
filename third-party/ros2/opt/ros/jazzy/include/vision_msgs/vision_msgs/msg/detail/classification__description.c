// generated from rosidl_generator_c/resource/idl__description.c.em
// with input from vision_msgs:msg/Classification.idl
// generated code does not contain a copyright notice

#include "vision_msgs/msg/detail/classification__functions.h"

ROSIDL_GENERATOR_C_PUBLIC_vision_msgs
const rosidl_type_hash_t *
vision_msgs__msg__Classification__get_type_hash(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_type_hash_t hash = {1, {
      0xf7, 0x4d, 0xb2, 0x8d, 0x0e, 0xae, 0xf3, 0xaf,
      0xb7, 0x8f, 0x48, 0xbf, 0x21, 0x61, 0xcd, 0x70,
      0x9a, 0x13, 0x83, 0xa0, 0x30, 0x9d, 0x34, 0x54,
      0x38, 0x3b, 0x25, 0xbe, 0x11, 0x34, 0x51, 0x43,
    }};
  return &hash;
}

#include <assert.h>
#include <string.h>

// Include directives for referenced types
#include "vision_msgs/msg/detail/object_hypothesis__functions.h"
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
static const rosidl_type_hash_t vision_msgs__msg__ObjectHypothesis__EXPECTED_HASH = {1, {
    0x51, 0xda, 0x43, 0xce, 0xc1, 0x1c, 0x01, 0x95,
    0x7a, 0x64, 0xd1, 0xe0, 0x5a, 0x24, 0xac, 0xd1,
    0x8a, 0xae, 0x77, 0x45, 0xec, 0x30, 0xb7, 0x43,
    0x48, 0xad, 0x5f, 0x6d, 0x63, 0x4f, 0x7f, 0x90,
  }};
#endif

static char vision_msgs__msg__Classification__TYPE_NAME[] = "vision_msgs/msg/Classification";
static char builtin_interfaces__msg__Time__TYPE_NAME[] = "builtin_interfaces/msg/Time";
static char std_msgs__msg__Header__TYPE_NAME[] = "std_msgs/msg/Header";
static char vision_msgs__msg__ObjectHypothesis__TYPE_NAME[] = "vision_msgs/msg/ObjectHypothesis";

// Define type names, field names, and default values
static char vision_msgs__msg__Classification__FIELD_NAME__header[] = "header";
static char vision_msgs__msg__Classification__FIELD_NAME__results[] = "results";

static rosidl_runtime_c__type_description__Field vision_msgs__msg__Classification__FIELDS[] = {
  {
    {vision_msgs__msg__Classification__FIELD_NAME__header, 6, 6},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_NESTED_TYPE,
      0,
      0,
      {std_msgs__msg__Header__TYPE_NAME, 19, 19},
    },
    {NULL, 0, 0},
  },
  {
    {vision_msgs__msg__Classification__FIELD_NAME__results, 7, 7},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_NESTED_TYPE_UNBOUNDED_SEQUENCE,
      0,
      0,
      {vision_msgs__msg__ObjectHypothesis__TYPE_NAME, 32, 32},
    },
    {NULL, 0, 0},
  },
};

static rosidl_runtime_c__type_description__IndividualTypeDescription vision_msgs__msg__Classification__REFERENCED_TYPE_DESCRIPTIONS[] = {
  {
    {builtin_interfaces__msg__Time__TYPE_NAME, 27, 27},
    {NULL, 0, 0},
  },
  {
    {std_msgs__msg__Header__TYPE_NAME, 19, 19},
    {NULL, 0, 0},
  },
  {
    {vision_msgs__msg__ObjectHypothesis__TYPE_NAME, 32, 32},
    {NULL, 0, 0},
  },
};

const rosidl_runtime_c__type_description__TypeDescription *
vision_msgs__msg__Classification__get_type_description(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static bool constructed = false;
  static const rosidl_runtime_c__type_description__TypeDescription description = {
    {
      {vision_msgs__msg__Classification__TYPE_NAME, 30, 30},
      {vision_msgs__msg__Classification__FIELDS, 2, 2},
    },
    {vision_msgs__msg__Classification__REFERENCED_TYPE_DESCRIPTIONS, 3, 3},
  };
  if (!constructed) {
    assert(0 == memcmp(&builtin_interfaces__msg__Time__EXPECTED_HASH, builtin_interfaces__msg__Time__get_type_hash(NULL), sizeof(rosidl_type_hash_t)));
    description.referenced_type_descriptions.data[0].fields = builtin_interfaces__msg__Time__get_type_description(NULL)->type_description.fields;
    assert(0 == memcmp(&std_msgs__msg__Header__EXPECTED_HASH, std_msgs__msg__Header__get_type_hash(NULL), sizeof(rosidl_type_hash_t)));
    description.referenced_type_descriptions.data[1].fields = std_msgs__msg__Header__get_type_description(NULL)->type_description.fields;
    assert(0 == memcmp(&vision_msgs__msg__ObjectHypothesis__EXPECTED_HASH, vision_msgs__msg__ObjectHypothesis__get_type_hash(NULL), sizeof(rosidl_type_hash_t)));
    description.referenced_type_descriptions.data[2].fields = vision_msgs__msg__ObjectHypothesis__get_type_description(NULL)->type_description.fields;
    constructed = true;
  }
  return &description;
}

static char toplevel_type_raw_source[] =
  "# Defines a classification result.\n"
  "#\n"
  "# This result does not contain any position information. It is designed for\n"
  "#   classifiers, which simply provide class probabilities given an instance of\n"
  "#   source data (e.g., an image or a point cloud).\n"
  "\n"
  "std_msgs/Header header\n"
  "\n"
  "# A list of class probabilities. This list need not provide a probability for\n"
  "#   every possible class, just ones that are nonzero, or above some\n"
  "#   user-defined threshold.\n"
  "ObjectHypothesis[] results\n"
  "\n"
  "# Source data that generated this classification are not a part of the message.\n"
  "# If you need to access them, use an exact or approximate time synchronizer in\n"
  "# your code, as this message's header should match the header of the source\n"
  "# data.";

static char msg_encoding[] = "msg";

// Define all individual source functions

const rosidl_runtime_c__type_description__TypeSource *
vision_msgs__msg__Classification__get_individual_type_description_source(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static const rosidl_runtime_c__type_description__TypeSource source = {
    {vision_msgs__msg__Classification__TYPE_NAME, 30, 30},
    {msg_encoding, 3, 3},
    {toplevel_type_raw_source, 713, 713},
  };
  return &source;
}

const rosidl_runtime_c__type_description__TypeSource__Sequence *
vision_msgs__msg__Classification__get_type_description_sources(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_runtime_c__type_description__TypeSource sources[4];
  static const rosidl_runtime_c__type_description__TypeSource__Sequence source_sequence = {sources, 4, 4};
  static bool constructed = false;
  if (!constructed) {
    sources[0] = *vision_msgs__msg__Classification__get_individual_type_description_source(NULL),
    sources[1] = *builtin_interfaces__msg__Time__get_individual_type_description_source(NULL);
    sources[2] = *std_msgs__msg__Header__get_individual_type_description_source(NULL);
    sources[3] = *vision_msgs__msg__ObjectHypothesis__get_individual_type_description_source(NULL);
    constructed = true;
  }
  return &source_sequence;
}
