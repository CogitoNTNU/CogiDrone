cmake_minimum_required(VERSION 3.20)
project(CogiDrone LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(src)                                                                   # Directory containing the drone source code (C++ files)
