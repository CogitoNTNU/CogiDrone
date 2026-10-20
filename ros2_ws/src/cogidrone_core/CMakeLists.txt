# Builds the C++ drone core in the repo's src/ (Elias' code) as a ROS 2 package, so
# `cb` builds it and `ros2 run cogidrone_core cogidrone` / launch files can start it.
# The code itself stays in src/; this only adds it to the colcon workspace.
cmake_minimum_required(VERSION 3.20)
project(cogidrone_core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ament_cmake REQUIRED)

get_filename_component(COGIDRONE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../../../src" ABSOLUTE)
add_subdirectory(${COGIDRONE_SRC} cogidrone)

install(TARGETS cogidrone DESTINATION lib/${PROJECT_NAME})

ament_package()
