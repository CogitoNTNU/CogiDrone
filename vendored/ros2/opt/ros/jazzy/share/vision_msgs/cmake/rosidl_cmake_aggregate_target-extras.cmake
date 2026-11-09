# generated from rosidl_cmake/cmake/rosidl_cmake_aggregate_target-extras.cmake.in

# Create a convenience aggregate target vision_msgs::vision_msgs
# that links all generated interface targets, so downstream packages can use
# a single modern CMake target name instead of ${vision_msgs_TARGETS}.
if(vision_msgs_TARGETS AND NOT TARGET vision_msgs::vision_msgs)
  add_library(vision_msgs::vision_msgs INTERFACE IMPORTED)
  set_target_properties(vision_msgs::vision_msgs PROPERTIES
    INTERFACE_LINK_LIBRARIES "${vision_msgs_TARGETS}")
endif()
