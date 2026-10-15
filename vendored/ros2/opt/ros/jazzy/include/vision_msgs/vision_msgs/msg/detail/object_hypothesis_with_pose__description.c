// generated from rosidl_generator_c/resource/idl__description.c.em
// with input from vision_msgs:msg/ObjectHypothesisWithPose.idl
// generated code does not contain a copyright notice

#include "vision_msgs/msg/detail/object_hypothesis_with_pose__functions.h"

ROSIDL_GENERATOR_C_PUBLIC_vision_msgs
const rosidl_type_hash_t *
vision_msgs__msg__ObjectHypothesisWithPose__get_type_hash(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_type_hash_t hash = {1, {
      0x41, 0xc1, 0x23, 0x44, 0xc0, 0x39, 0x6c, 0x48,
      0xb8, 0x1d, 0xf1, 0x24, 0x67, 0xc1, 0x92, 0xd9,
      0x9b, 0x2a, 0x0e, 0xb1, 0x46, 0x3c, 0x5c, 0x37,
      0x19, 0x4e, 0x42, 0x23, 0x25, 0x1c, 0x80, 0xbc,
    }};
  return &hash;
}

#include <assert.h>
#include <string.h>

// Include directives for referenced types
#include "geometry_msgs/msg/detail/pose_with_covariance__functions.h"
#include "geometry_msgs/msg/detail/pose__functions.h"
#include "vision_msgs/msg/detail/object_hypothesis__functions.h"
#include "geometry_msgs/msg/detail/point__functions.h"
#include "geometry_msgs/msg/detail/quaternion__functions.h"

// Hashes for external referenced types
#ifndef NDEBUG
static const rosidl_type_hash_t geometry_msgs__msg__Point__EXPECTED_HASH = {1, {
    0x69, 0x63, 0x08, 0x48, 0x42, 0xa9, 0xb0, 0x44,
    0x94, 0xd6, 0xb2, 0x94, 0x1d, 0x11, 0x44, 0x47,
    0x08, 0xd8, 0x92, 0xda, 0x2f, 0x4b, 0x09, 0x84,
    0x3b, 0x9c, 0x43, 0xf4, 0x2a, 0x7f, 0x68, 0x81,
  }};
static const rosidl_type_hash_t geometry_msgs__msg__Pose__EXPECTED_HASH = {1, {
    0xd5, 0x01, 0x95, 0x4e, 0x94, 0x76, 0xce, 0xa2,
    0x99, 0x69, 0x84, 0xe8, 0x12, 0x05, 0x4b, 0x68,
    0x02, 0x6a, 0xe0, 0xbf, 0xae, 0x78, 0x9d, 0x9a,
    0x10, 0xb2, 0x3d, 0xaf, 0x35, 0xcc, 0x90, 0xfa,
  }};
static const rosidl_type_hash_t geometry_msgs__msg__PoseWithCovariance__EXPECTED_HASH = {1, {
    0x9a, 0x7c, 0x0f, 0xd2, 0x34, 0xb7, 0xf4, 0x5c,
    0x60, 0x98, 0x74, 0x5e, 0xcc, 0xcd, 0x77, 0x3c,
    0xa1, 0x08, 0x56, 0x70, 0xe6, 0x41, 0x07, 0x13,
    0x53, 0x97, 0xae, 0xe3, 0x1c, 0x02, 0xe1, 0xbb,
  }};
static const rosidl_type_hash_t geometry_msgs__msg__Quaternion__EXPECTED_HASH = {1, {
    0x8a, 0x76, 0x5f, 0x66, 0x77, 0x8c, 0x8f, 0xf7,
    0xc8, 0xab, 0x94, 0xaf, 0xcc, 0x59, 0x0a, 0x2e,
    0xd5, 0x32, 0x5a, 0x1d, 0x9a, 0x07, 0x6f, 0xff,
    0xf3, 0x8f, 0xbc, 0xe3, 0x6f, 0x45, 0x86, 0x84,
  }};
static const rosidl_type_hash_t vision_msgs__msg__ObjectHypothesis__EXPECTED_HASH = {1, {
    0x51, 0xda, 0x43, 0xce, 0xc1, 0x1c, 0x01, 0x95,
    0x7a, 0x64, 0xd1, 0xe0, 0x5a, 0x24, 0xac, 0xd1,
    0x8a, 0xae, 0x77, 0x45, 0xec, 0x30, 0xb7, 0x43,
    0x48, 0xad, 0x5f, 0x6d, 0x63, 0x4f, 0x7f, 0x90,
  }};
#endif

static char vision_msgs__msg__ObjectHypothesisWithPose__TYPE_NAME[] = "vision_msgs/msg/ObjectHypothesisWithPose";
static char geometry_msgs__msg__Point__TYPE_NAME[] = "geometry_msgs/msg/Point";
static char geometry_msgs__msg__Pose__TYPE_NAME[] = "geometry_msgs/msg/Pose";
static char geometry_msgs__msg__PoseWithCovariance__TYPE_NAME[] = "geometry_msgs/msg/PoseWithCovariance";
static char geometry_msgs__msg__Quaternion__TYPE_NAME[] = "geometry_msgs/msg/Quaternion";
static char vision_msgs__msg__ObjectHypothesis__TYPE_NAME[] = "vision_msgs/msg/ObjectHypothesis";

// Define type names, field names, and default values
static char vision_msgs__msg__ObjectHypothesisWithPose__FIELD_NAME__hypothesis[] = "hypothesis";
static char vision_msgs__msg__ObjectHypothesisWithPose__FIELD_NAME__pose[] = "pose";

static rosidl_runtime_c__type_description__Field vision_msgs__msg__ObjectHypothesisWithPose__FIELDS[] = {
  {
    {vision_msgs__msg__ObjectHypothesisWithPose__FIELD_NAME__hypothesis, 10, 10},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_NESTED_TYPE,
      0,
      0,
      {vision_msgs__msg__ObjectHypothesis__TYPE_NAME, 32, 32},
    },
    {NULL, 0, 0},
  },
  {
    {vision_msgs__msg__ObjectHypothesisWithPose__FIELD_NAME__pose, 4, 4},
    {
      rosidl_runtime_c__type_description__FieldType__FIELD_TYPE_NESTED_TYPE,
      0,
      0,
      {geometry_msgs__msg__PoseWithCovariance__TYPE_NAME, 36, 36},
    },
    {NULL, 0, 0},
  },
};

static rosidl_runtime_c__type_description__IndividualTypeDescription vision_msgs__msg__ObjectHypothesisWithPose__REFERENCED_TYPE_DESCRIPTIONS[] = {
  {
    {geometry_msgs__msg__Point__TYPE_NAME, 23, 23},
    {NULL, 0, 0},
  },
  {
    {geometry_msgs__msg__Pose__TYPE_NAME, 22, 22},
    {NULL, 0, 0},
  },
  {
    {geometry_msgs__msg__PoseWithCovariance__TYPE_NAME, 36, 36},
    {NULL, 0, 0},
  },
  {
    {geometry_msgs__msg__Quaternion__TYPE_NAME, 28, 28},
    {NULL, 0, 0},
  },
  {
    {vision_msgs__msg__ObjectHypothesis__TYPE_NAME, 32, 32},
    {NULL, 0, 0},
  },
};

const rosidl_runtime_c__type_description__TypeDescription *
vision_msgs__msg__ObjectHypothesisWithPose__get_type_description(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static bool constructed = false;
  static const rosidl_runtime_c__type_description__TypeDescription description = {
    {
      {vision_msgs__msg__ObjectHypothesisWithPose__TYPE_NAME, 40, 40},
      {vision_msgs__msg__ObjectHypothesisWithPose__FIELDS, 2, 2},
    },
    {vision_msgs__msg__ObjectHypothesisWithPose__REFERENCED_TYPE_DESCRIPTIONS, 5, 5},
  };
  if (!constructed) {
    assert(0 == memcmp(&geometry_msgs__msg__Point__EXPECTED_HASH, geometry_msgs__msg__Point__get_type_hash(NULL), sizeof(rosidl_type_hash_t)));
    description.referenced_type_descriptions.data[0].fields = geometry_msgs__msg__Point__get_type_description(NULL)->type_description.fields;
    assert(0 == memcmp(&geometry_msgs__msg__Pose__EXPECTED_HASH, geometry_msgs__msg__Pose__get_type_hash(NULL), sizeof(rosidl_type_hash_t)));
    description.referenced_type_descriptions.data[1].fields = geometry_msgs__msg__Pose__get_type_description(NULL)->type_description.fields;
    assert(0 == memcmp(&geometry_msgs__msg__PoseWithCovariance__EXPECTED_HASH, geometry_msgs__msg__PoseWithCovariance__get_type_hash(NULL), sizeof(rosidl_type_hash_t)));
    description.referenced_type_descriptions.data[2].fields = geometry_msgs__msg__PoseWithCovariance__get_type_description(NULL)->type_description.fields;
    assert(0 == memcmp(&geometry_msgs__msg__Quaternion__EXPECTED_HASH, geometry_msgs__msg__Quaternion__get_type_hash(NULL), sizeof(rosidl_type_hash_t)));
    description.referenced_type_descriptions.data[3].fields = geometry_msgs__msg__Quaternion__get_type_description(NULL)->type_description.fields;
    assert(0 == memcmp(&vision_msgs__msg__ObjectHypothesis__EXPECTED_HASH, vision_msgs__msg__ObjectHypothesis__get_type_hash(NULL), sizeof(rosidl_type_hash_t)));
    description.referenced_type_descriptions.data[4].fields = vision_msgs__msg__ObjectHypothesis__get_type_description(NULL)->type_description.fields;
    constructed = true;
  }
  return &description;
}

static char toplevel_type_raw_source[] =
  "# An object hypothesis that contains pose information.\n"
  "# If you would like to define an array of ObjectHypothesisWithPose messages,\n"
  "#   please see the Detection2D or Detection3D message types.\n"
  "\n"
  "# The object hypothesis (ID and score).\n"
  "ObjectHypothesis hypothesis\n"
  "\n"
  "# The 6D pose of the object hypothesis. This pose should be\n"
  "#   defined as the pose of some fixed reference point on the object, such as\n"
  "#   the geometric center of the bounding box, the center of mass of the\n"
  "#   object or the origin of a reference mesh of the object.\n"
  "# Note that this pose is not stamped; frame information can be defined by\n"
  "#   parent messages.\n"
  "# Also note that different classes predicted for the same input data may have\n"
  "#   different predicted 6D poses.\n"
  "geometry_msgs/PoseWithCovariance pose";

static char msg_encoding[] = "msg";

// Define all individual source functions

const rosidl_runtime_c__type_description__TypeSource *
vision_msgs__msg__ObjectHypothesisWithPose__get_individual_type_description_source(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static const rosidl_runtime_c__type_description__TypeSource source = {
    {vision_msgs__msg__ObjectHypothesisWithPose__TYPE_NAME, 40, 40},
    {msg_encoding, 3, 3},
    {toplevel_type_raw_source, 777, 777},
  };
  return &source;
}

const rosidl_runtime_c__type_description__TypeSource__Sequence *
vision_msgs__msg__ObjectHypothesisWithPose__get_type_description_sources(
  const rosidl_message_type_support_t * type_support)
{
  (void)type_support;
  static rosidl_runtime_c__type_description__TypeSource sources[6];
  static const rosidl_runtime_c__type_description__TypeSource__Sequence source_sequence = {sources, 6, 6};
  static bool constructed = false;
  if (!constructed) {
    sources[0] = *vision_msgs__msg__ObjectHypothesisWithPose__get_individual_type_description_source(NULL),
    sources[1] = *geometry_msgs__msg__Point__get_individual_type_description_source(NULL);
    sources[2] = *geometry_msgs__msg__Pose__get_individual_type_description_source(NULL);
    sources[3] = *geometry_msgs__msg__PoseWithCovariance__get_individual_type_description_source(NULL);
    sources[4] = *geometry_msgs__msg__Quaternion__get_individual_type_description_source(NULL);
    sources[5] = *vision_msgs__msg__ObjectHypothesis__get_individual_type_description_source(NULL);
    constructed = true;
  }
  return &source_sequence;
}
