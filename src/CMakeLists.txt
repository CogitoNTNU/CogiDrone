# * Dependency roots
# ? Two build environments, two sources for the SAME dependencies.
# ?
# ? The predicate is CMAKE_CROSSCOMPILING, which CMake sets to TRUE if and
# ? only if a toolchain file set CMAKE_SYSTEM_NAME. Proof from CMakePresets.json:
# ?
# ?   linux-arm64   -> "toolchainFile": cmake/aarch64-linux-gnu.cmake
# ?                    -> that file sets CMAKE_SYSTEM_NAME Linux (line 1)
# ?                    -> CMAKE_CROSSCOMPILING = TRUE   (workstation)
# ?
# ?   jetson-native -> no toolchainFile
# ?                    -> CMAKE_SYSTEM_NAME = host value
# ?                    -> CMAKE_CROSSCOMPILING = FALSE  (Jetson)
# ?
# ? This is not a capability probe. It does not ask "is TensorRT installed?"
# ? It asks "which machine am I building for?" - which has a stable answer.
if(CMAKE_CROSSCOMPILING)
    # Workstation: everything from vendored/, copied off the Jetson.
    # ? Headers are text - architecture-independent, so IntelliSense and the
    # ? cross-compiler parse them identically to the Jetson's g++.
    # ? The .so files are aarch64 ELF. The linker reads their symbol tables;
    # ? it does not execute them, so a Windows x64 host can link against them.
    set(_trt_include_dir    ${CMAKE_SOURCE_DIR}/vendored/tensorrt/include)
    set(_trt_lib_dir        ${CMAKE_SOURCE_DIR}/vendored/tensorrt/lib)
    set(_cuda_include_dir   ${CMAKE_SOURCE_DIR}/vendored/cuda/include)
    set(_cuda_lib_dir       ${CMAKE_SOURCE_DIR}/vendored/cuda/lib)
    set(_opencv_include_dir ${CMAKE_SOURCE_DIR}/vendored/opencv/include)
    set(_opencv_lib_dir     ${CMAKE_SOURCE_DIR}/vendored/opencv/lib)
else()
    # Jetson: everything from the system, installed by JetPack.
    # ? These are the authoritative copies. If JetPack updates TensorRT, this
    # ? build picks it up with no re-vendoring step.
    set(_trt_include_dir    /usr/include/aarch64-linux-gnu)
    set(_trt_lib_dir        /usr/lib/aarch64-linux-gnu)
    set(_cuda_include_dir   /usr/local/cuda/include)
    set(_cuda_lib_dir       /usr/local/cuda/lib64)
    set(_opencv_include_dir /usr/include/opencv4)
    set(_opencv_lib_dir     /usr/lib/aarch64-linux-gnu)
endif()

# ? Fail at configure time with the exact missing path, not at compile time
# ? with "NvInfer.h: No such file or directory".
foreach(_dir IN ITEMS
        ${_trt_include_dir} ${_trt_lib_dir}
        ${_cuda_include_dir} ${_cuda_lib_dir}
        ${_opencv_include_dir} ${_opencv_lib_dir})
    if(NOT IS_DIRECTORY "${_dir}")
        message(FATAL_ERROR
            "Missing dependency directory: ${_dir}\n"
            "  Cross-build: populate vendored/ (see scripts/vendor-deps.sh)\n"
            "  Jetson:      check the JetPack install")
    endif()
endforeach()

# ? Printed at configure time so the branch taken is visible without reading
# ? CMakeCache.txt.
message(STATUS "TensorRT: ${_trt_include_dir} | ${_trt_lib_dir}")
message(STATUS "CUDA:     ${_cuda_include_dir} | ${_cuda_lib_dir}")
message(STATUS "OpenCV:   ${_opencv_include_dir} | ${_opencv_lib_dir}")


# * ROS 2 + px4_msgs (vendored on BOTH builds - see the note above)
# ? Each Jazzy package dir is its own include root (headers live at <pkg>/<pkg>/...)
file(GLOB _ros_include_dirs
    LIST_DIRECTORIES true
    ${CMAKE_SOURCE_DIR}/vendored/ros2/opt/ros/jazzy/include/*
)
file(GLOB _ros_lib_dirs ${CMAKE_SOURCE_DIR}/vendored/ros2/opt/ros/jazzy/lib)

# ? Headers live at <root>/include/px4_msgs/px4_msgs/msg/..., so the include
# ? root is .../include/px4_msgs (same <pkg>/<pkg>/ nesting as Jazzy packages)
set(_px4_include_dir ${CMAKE_SOURCE_DIR}/vendored/px4_msgs/include/px4_msgs)
set(_px4_lib_dir     ${CMAKE_SOURCE_DIR}/vendored/px4_msgs/lib)


# * Sources
# ? Unconditional. Every file compiles in both environments, because both
# ? environments now have real headers for every dependency. No stub, no
# ? #ifdef, no target_sources split.
add_executable(cogidrone
    # Application
    main.cpp
    drone.cpp

    # Perception
    perception/perception.cpp
    perception/models/trt_engine.cpp
    perception/models/yolo.cpp
    perception/models/person.cpp
    perception/models/head.cpp
    perception/models/depth_anything.cpp

    # Representation
    # TODO: [#18] Representation

    # Navigation
    navigation/navigation.cpp

    # Controllers
    controller/flight_controller.cpp

    # Helpers
    logging.cpp
)


# * Include
target_include_directories(cogidrone PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${_ros_include_dirs}
    ${_px4_include_dir}
    ${_trt_include_dir}
    ${_cuda_include_dir}
    ${_opencv_include_dir}
)


# * Link
target_link_directories(cogidrone PRIVATE
    ${_ros_lib_dirs}
    ${_px4_lib_dir}
    ${_trt_lib_dir}
    ${_cuda_lib_dir}
    ${_opencv_lib_dir}
)
target_link_libraries(cogidrone PRIVATE
    rclcpp rcl rmw rmw_fastrtps_cpp rmw_implementation rmw_dds_common
    rmw_fastrtps_shared_cpp rcutils rcpputils
    rosidl_runtime_c rosidl_typesupport_c rosidl_typesupport_cpp                        # `rosidl_runtime_cpp`: header-only, no .so exists
    rosidl_typesupport_fastrtps_c rosidl_typesupport_fastrtps_cpp
    rosidl_typesupport_introspection_c rosidl_typesupport_introspection_cpp
    rosidl_dynamic_typesupport
    sensor_msgs__rosidl_typesupport_cpp sensor_msgs__rosidl_typesupport_c
    std_msgs__rosidl_typesupport_cpp builtin_interfaces__rosidl_typesupport_cpp
    geometry_msgs__rosidl_typesupport_cpp nav_msgs__rosidl_typesupport_cpp
    vision_msgs__rosidl_typesupport_cpp
    libstatistics_collector statistics_msgs__rosidl_typesupport_cpp                     # // `statistics_msgs`: bare msg lib doesn't exist;
    rosgraph_msgs__rosidl_typesupport_cpp                                               # //                    typesupport variant covers it
    tracetools rcl_yaml_param_parser rcl_interfaces__rosidl_typesupport_cpp
    ament_index_cpp class_loader fastcdr fastrtps                                       # `console_bridge`: not vendored; runtime dep on the Jetson only

    px4_msgs__rosidl_typesupport_cpp px4_msgs__rosidl_typesupport_c
    px4_msgs__rosidl_typesupport_fastrtps_cpp px4_msgs__rosidl_typesupport_fastrtps_c
    px4_msgs__rosidl_typesupport_introspection_cpp px4_msgs__rosidl_typesupport_introspection_c

    nvinfer cudart
    opencv_core opencv_imgproc
)


# * Cross-link resolution
# ? `-rpath-link` is how the linker resolves the DT_NEEDED entries of the
# ? libraries above. `-L` (from target_link_directories) only finds libraries
# ? named on the command line; it does not find their dependencies.
# ?
# ? On the Jetson the default search path already covers all of these, so this
# ? block is cross-only. That is what keeps the Jetson build free of vendored
# ? paths.
if(CMAKE_CROSSCOMPILING)
    target_link_options(cogidrone PRIVATE
        "-Wl,-rpath-link,${CMAKE_SOURCE_DIR}/vendored/ros2/opt/ros/jazzy/lib"
        "-Wl,-rpath-link,${CMAKE_SOURCE_DIR}/vendored/ros2/usr/lib/aarch64-linux-gnu"
        "-Wl,-rpath-link,${_px4_lib_dir}"
        "-Wl,-rpath-link,${_trt_lib_dir}"
        "-Wl,-rpath-link,${_cuda_lib_dir}"
        "-Wl,-rpath-link,${_opencv_lib_dir}"

        # ? libnvinfer.so has DT_NEEDED entries for libraries we do not vendor
        # ? (libcublas, libcudnn, ...). Without this flag the cross-link fails
        # ? on symbols that will resolve at runtime on the Jetson. This is the
        # ? exact case the old comment described.
        "-Wl,--allow-shlib-undefined"
    )
endif()


# * Compile
target_compile_features(cogidrone PRIVATE cxx_std_23)

if(MSVC)
    target_compile_options(cogidrone PRIVATE /W4)
else()
    target_compile_options(cogidrone PRIVATE -Wall -Wextra -Wpedantic)
endif()