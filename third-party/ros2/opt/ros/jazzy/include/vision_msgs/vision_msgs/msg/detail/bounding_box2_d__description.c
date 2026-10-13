// generated from rosidl_generator_c/resource/idl__description.c.em
// with input from vision_msgs:msg/BoundingBox2D.idl
// generated code does not contain a copyright notice

#include "vision_msgs/msg/detail/bounding_box2_d__functions.h"

ROSIDL_GENERATOR_C_PUBLIC_vision_msgs
const rosidl_type_hash_t *
vision_msgs__msg__BoundingBox2D__get_type_hash(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_type_hash_t hash = {1, {
      0x00, 0x8e, 0xac, 0xbb, 0x0c, 0xf8, 0xf2, 0x6e,
      0x83, 0x79, 0x55, 0x47, 0xbe, 0xd0, 0x13, 0xee,
      0xc3, 0x67, 0x54, 0x85, 0xef, 0x38, 0x6e, 0xc0,
      0x56, 0xd8, 0x0e, 0xaa, 0xf1, 0xc1, 0xce, 0x2f,
    }};
  return &hash;
}

#include <assert.h>
#include <string.h>

// Include directives for referenced types
#include "vision_msgs/msg/detail/point2_d__functions.h"
#include "vision_msgs/msg/detail/pose2_d__functions.h"

// Hashes for external referenced types
#ifndef NDEBUG
static const rosidl_type_hash_t vision_msgs__msg__Point2D__EXPECTED_HASH = {1, {
    0xea, 0xb0, 0xe8, 0x3f, 0x44, 0xab, 0x4d, 0xe9,
    0x2c, 0xea, 0xf7, 0x6d, 0xd7, 0xd0, 0xe7, 0x63,
    0x8f, 0x2b, 0xdd, 0x1d, 0x16, 0xff, 0xbe, 0x16,
    0xc7, 0xa1, 0x36, 0xc4, 0x8e, 0x40, 0x72, 0x47,
  }};
static const rosidl_type_hash_t vision_msgs__msg__Pose2D__EXPECTED_HASH = {1, {
    0xb0, 0xb5, 0xd1, 0xd7, 0xb4, 0xc2, 0x0d, 0xd4,
    0xfc, 0xde, 0x3e, 0xd1, 0xd4, 0xb2, 0x89, 0xa9,
    0x19, 0x29, 0x05, 0x72, 0x00, 0x7d, 0x67, 0xa9,
    0x70, 0x43, 0x81, 0x4d, 0xb2, 0x5d, 0x36, 0x48,
  }};
#endif

static char vision_msgs__msg__BoundingBox2D__TYPE_NAME[] = "vision_msgs/msg/BoundingBox2D";
static char vision_msgs__msg__Point2D__TYPE_NAME[] = "vision_msgs/msg/Point2D";
static char vision_msgs__msg__Pose2D__TYPE_NAME[] = "vision_msgs/msg/Pose2D";

// Define type names, field names, and default values
static char vision_msgs__msg__BoundingBox2D__FIELD_NAME__center[] = "center";
static char vision_msgs__msg__BoundingBox2D__FIELD_NAME__size_x[] = "size_x";
static char vision_msgs__msg__BoundingBox2D__FIELD_NAME__size_y[] = "size_y";

static rosidl_runtime_c__type_description__Field vision_msgs__msg__BoundingBox2D__FIELDS[] = {
  {
    {vision_msgs__msg__BoundingBox2D__FIELD_NAME__center, 6, 6},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_NESTED_TYPE,
      0,
      0,
      {vision_msgs__msg__Pose2D__TYPE_NAME, 22, 22},
    },
    {NULL, 0, 0},
  },
  {
    {vision_msgs__msg__BoundingBox2D__FIELD_NAME__size_x, 6, 6},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_DOUBLE,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
  {
    {vision_msgs__msg__BoundingBox2D__FIELD_NAME__size_y, 6, 6},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_DOUBLE,
      0,
      0,
      {NULL, 0, 0},
    },
    {NULL, 0, 0},
  },
};

static rosidl_runtime_c__type_description__IndividualTypeDescription vision_msgs__msg__BoundingBox2D__REFERENCED_TYPE_DESCRIPTIONS[] = {
  {
    {vision_msgs__msg__Point2D__TYPE_NAME, 23, 23},
    {NULL, 0, 0},
  },
  {
    {vision_msgs__msg__Pose2D__TYPE_NAME, 22, 22},
    {NULL, 0, 0},
  },
};

const rosidl_runtime_c__type_description__TypeDescription *
vision_msgs__msg__BoundingBox2D__get_type_description(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static bool constructed = false;
  static const rosidl_runtime_c__type_description__TypeDescription description = {
    {
      {vision_msgs__msg__BoundingBox2D__TYPE_NAME, 29, 29},
      {vision_msgs__msg__BoundingBox2D__FIELDS, 3, 3},
    },
    {vision_msgs__msg__BoundingBox2D__REFERENCED_TYPE_DESCRIPTIONS, 2, 2},
  };
  if (!constructed) {
    assert(0 == memcmp(&vision_msgs__msg__Point2D__EXPECTED_HASH, vision_msgs__msg__Point2D__get_type_hash(NULL), sizeof(rosidl_type_hash_t)));
    description.referenced_type_descriptions.data[0].fields = vision_msgs__msg__Point2D__get_type_description(NULL)->type_description.fields;
    assert(0 == memcmp(&vision_msgs__msg__Pose2D__EXPECTED_HASH, vision_msgs__msg__Pose2D__get_type_hash(NULL), sizeof(rosidl_type_hash_t)));
    description.referenced_type_descriptions.data[1].fields = vision_msgs__msg__Pose2D__get_type_description(NULL)->type_description.fields;
    constructed = true;
  }
  return &description;
}

static char toplevel_type_raw_source[] =
  "# A 2D bounding box that can be rotated about its center.\n"
  "# All dimensions are in pixels, but represented using floating-point\n"
  "#   values to allow sub-pixel precision. If an exact pixel crop is required\n"
  "#   for a rotated bounding box, it can be calculated using Bresenham's line\n"
  "#   algorithm.\n"
  "\n"
  "# The 2D position (in pixels) and orientation of the bounding box center.\n"
  "vision_msgs/Pose2D center\n"
  "\n"
  "# The total size (in pixels) of the bounding box surrounding the object relative\n"
  "#   to the pose of its center.\n"
  "float64 size_x\n"
  "float64 size_y";

static char msg_encoding[] = "msg";

// Define all individual source functions

const rosidl_runtime_c__type_description__TypeSource *
vision_msgs__msg__BoundingBox2D__get_individual_type_description_source(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static const rosidl_runtime_c__type_description__TypeSource source = {
    {vision_msgs__msg__BoundingBox2D__TYPE_NAME, 29, 29},
    {msg_encoding, 3, 3},
    {toplevel_type_raw_source, 538, 538},
  };
  return &source;
}

const rosidl_runtime_c__type_description__TypeSource__Sequence *
vision_msgs__msg__BoundingBox2D__get_type_description_sources(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_runtime_c__type_description__TypeSource sources[3];
  static const rosidl_runtime_c__type_description__TypeSource__Sequence source_sequence = {sources, 3, 3};
  static bool constructed = false;
  if (!constructed) {
    sources[0] = *vision_msgs__msg__BoundingBox2D__get_individual_type_description_source(NULL),
    sources[1] = *vision_msgs__msg__Point2D__get_individual_type_description_source(NULL);
    sources[2] = *vision_msgs__msg__Pose2D__get_individual_type_description_source(NULL);
    constructed = true;
  }
  return &source_sequence;
}
