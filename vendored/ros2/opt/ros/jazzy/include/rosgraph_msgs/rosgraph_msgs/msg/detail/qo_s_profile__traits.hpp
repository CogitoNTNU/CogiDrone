// generated from rosidl_generator_cpp/resource/idl__traits.hpp.em
// with input from rosgraph_msgs:msg/QoSProfile.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "rosgraph_msgs/msg/qo_s_profile.hpp"


#ifndef ROSGRAPH_MSGS__MSG__DETAIL__QO_S_PROFILE__TRAITS_HPP_
#define ROSGRAPH_MSGS__MSG__DETAIL__QO_S_PROFILE__TRAITS_HPP_

#include <stdint.h>

#include <sstream>
#include <string>
#include <type_traits>

#include "rosgraph_msgs/msg/detail/qo_s_profile__struct.hpp"
#include "rosidl_runtime_cpp/traits.hpp"

// Include directives for member types
// Member 'deadline'
// Member 'lifespan'
// Member 'liveliness_lease_duration'
#include "builtin_interfaces/msg/detail/duration__traits.hpp"

namespace rosgraph_msgs
{

namespace msg
{

inline void to_flow_style_yaml(
  const QoSProfile & msg,
  std::ostream & out)
{
  out << "{";
  // member: depth
  {
    out << "depth: ";
    rosidl_generator_traits::value_to_yaml(msg.depth, out);
    out << ", ";
  }

  // member: deadline
  {
    out << "deadline: ";
    to_flow_style_yaml(msg.deadline, out);
    out << ", ";
  }

  // member: lifespan
  {
    out << "lifespan: ";
    to_flow_style_yaml(msg.lifespan, out);
    out << ", ";
  }

  // member: history
  {
    out << "history: ";
    rosidl_generator_traits::value_to_yaml(msg.history, out);
    out << ", ";
  }

  // member: reliability
  {
    out << "reliability: ";
    rosidl_generator_traits::value_to_yaml(msg.reliability, out);
    out << ", ";
  }

  // member: durability
  {
    out << "durability: ";
    rosidl_generator_traits::value_to_yaml(msg.durability, out);
    out << ", ";
  }

  // member: liveliness
  {
    out << "liveliness: ";
    rosidl_generator_traits::value_to_yaml(msg.liveliness, out);
    out << ", ";
  }

  // member: liveliness_lease_duration
  {
    out << "liveliness_lease_duration: ";
    to_flow_style_yaml(msg.liveliness_lease_duration, out);
  }
  out << "}";
}  // NOLINT(readability/fn_size)

inline void to_block_style_yaml(
  const QoSProfile & msg,
  std::ostream & out, size_t indentation = 0)
{
  // member: depth
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "depth: ";
    rosidl_generator_traits::value_to_yaml(msg.depth, out);
    out << "\n";
  }

  // member: deadline
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "deadline:\n";
    to_block_style_yaml(msg.deadline, out, indentation + 2);
  }

  // member: lifespan
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "lifespan:\n";
    to_block_style_yaml(msg.lifespan, out, indentation + 2);
  }

  // member: history
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "history: ";
    rosidl_generator_traits::value_to_yaml(msg.history, out);
    out << "\n";
  }

  // member: reliability
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "reliability: ";
    rosidl_generator_traits::value_to_yaml(msg.reliability, out);
    out << "\n";
  }

  // member: durability
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "durability: ";
    rosidl_generator_traits::value_to_yaml(msg.durability, out);
    out << "\n";
  }

  // member: liveliness
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "liveliness: ";
    rosidl_generator_traits::value_to_yaml(msg.liveliness, out);
    out << "\n";
  }

  // member: liveliness_lease_duration
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "liveliness_lease_duration:\n";
    to_block_style_yaml(msg.liveliness_lease_duration, out, indentation + 2);
  }
}  // NOLINT(readability/fn_size)

inline std::string to_yaml(const QoSProfile & msg, bool use_flow_style = false)
{
  std::ostringstream out;
  if (use_flow_style) {
    to_flow_style_yaml(msg, out);
  } else {
    to_block_style_yaml(msg, out);
  }
  return out.str();
}

}  // namespace msg

}  // namespace rosgraph_msgs

namespace rosidl_generator_traits
{

[[deprecated("use rosgraph_msgs::msg::to_block_style_yaml() instead")]]
inline void to_yaml(
  const rosgraph_msgs::msg::QoSProfile & msg,
  std::ostream & out, size_t indentation = 0)
{
  rosgraph_msgs::msg::to_block_style_yaml(msg, out, indentation);
}

[[deprecated("use rosgraph_msgs::msg::to_yaml() instead")]]
inline std::string to_yaml(const rosgraph_msgs::msg::QoSProfile & msg)
{
  return rosgraph_msgs::msg::to_yaml(msg);
}

template<>
inline const char * data_type<rosgraph_msgs::msg::QoSProfile>()
{
  return "rosgraph_msgs::msg::QoSProfile";
}

template<>
inline const char * name<rosgraph_msgs::msg::QoSProfile>()
{
  return "rosgraph_msgs/msg/QoSProfile";
}

template<>
struct has_fixed_size<rosgraph_msgs::msg::QoSProfile>
  : std::integral_constant<bool, has_fixed_size<builtin_interfaces::msg::Duration>::value> {};

template<>
struct has_bounded_size<rosgraph_msgs::msg::QoSProfile>
  : std::integral_constant<bool, has_bounded_size<builtin_interfaces::msg::Duration>::value> {};

template<>
struct is_message<rosgraph_msgs::msg::QoSProfile>
  : std::true_type {};

}  // namespace rosidl_generator_traits

#endif  // ROSGRAPH_MSGS__MSG__DETAIL__QO_S_PROFILE__TRAITS_HPP_
