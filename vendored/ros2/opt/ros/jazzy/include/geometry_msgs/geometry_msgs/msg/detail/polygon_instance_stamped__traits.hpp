// generated from rosidl_generator_cpp/resource/idl__traits.hpp.em
// with input from geometry_msgs:msg/PolygonInstanceStamped.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "geometry_msgs/msg/polygon_instance_stamped.hpp"


#ifndef GEOMETRY_MSGS__MSG__DETAIL__POLYGON_INSTANCE_STAMPED__TRAITS_HPP_
#define GEOMETRY_MSGS__MSG__DETAIL__POLYGON_INSTANCE_STAMPED__TRAITS_HPP_

#include <stdint.h>

#include <sstream>
#include <string>
#include <type_traits>

#include "geometry_msgs/msg/detail/polygon_instance_stamped__struct.hpp"
#include "rosidl_runtime_cpp/traits.hpp"

// Include directives for member types
// Member 'header'
#include "std_msgs/msg/detail/header__traits.hpp"
// Member 'polygon'
#include "geometry_msgs/msg/detail/polygon_instance__traits.hpp"

namespace geometry_msgs
{

namespace msg
{

inline void to_flow_style_yaml(
  const PolygonInstanceStamped & msg,
  std::ostream & out)
{
  out << "{";
  // member: header
  {
    out << "header: ";
    to_flow_style_yaml(msg.header, out);
    out << ", ";
  }

  // member: polygon
  {
    out << "polygon: ";
    to_flow_style_yaml(msg.polygon, out);
  }
  out << "}";
}  // NOLINT(readability/fn_size)

inline void to_block_style_yaml(
  const PolygonInstanceStamped & msg,
  std::ostream & out, size_t indentation = 0)
{
  // member: header
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "header:\n";
    to_block_style_yaml(msg.header, out, indentation + 2);
  }

  // member: polygon
  {
    if (indentation > 0) {
      out << std::string(indentation, ' ');
    }
    out << "polygon:\n";
    to_block_style_yaml(msg.polygon, out, indentation + 2);
  }
}  // NOLINT(readability/fn_size)

inline std::string to_yaml(const PolygonInstanceStamped & msg, bool use_flow_style = false)
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

}  // namespace geometry_msgs

namespace rosidl_generator_traits
{

[[deprecated("use geometry_msgs::msg::to_block_style_yaml() instead")]]
inline void to_yaml(
  const geometry_msgs::msg::PolygonInstanceStamped & msg,
  std::ostream & out, size_t indentation = 0)
{
  geometry_msgs::msg::to_block_style_yaml(msg, out, indentation);
}

[[deprecated("use geometry_msgs::msg::to_yaml() instead")]]
inline std::string to_yaml(const geometry_msgs::msg::PolygonInstanceStamped & msg)
{
  return geometry_msgs::msg::to_yaml(msg);
}

template<>
inline const char * data_type<geometry_msgs::msg::PolygonInstanceStamped>()
{
  return "geometry_msgs::msg::PolygonInstanceStamped";
}

template<>
inline const char * name<geometry_msgs::msg::PolygonInstanceStamped>()
{
  return "geometry_msgs/msg/PolygonInstanceStamped";
}

template<>
struct has_fixed_size<geometry_msgs::msg::PolygonInstanceStamped>
  : std::integral_constant<bool, has_fixed_size<geometry_msgs::msg::PolygonInstance>::value && has_fixed_size<std_msgs::msg::Header>::value> {};

template<>
struct has_bounded_size<geometry_msgs::msg::PolygonInstanceStamped>
  : std::integral_constant<bool, has_bounded_size<geometry_msgs::msg::PolygonInstance>::value && has_bounded_size<std_msgs::msg::Header>::value> {};

template<>
struct is_message<geometry_msgs::msg::PolygonInstanceStamped>
  : std::true_type {};

}  // namespace rosidl_generator_traits

#endif  // GEOMETRY_MSGS__MSG__DETAIL__POLYGON_INSTANCE_STAMPED__TRAITS_HPP_
