# * Dependency roots
# ? Two build environments, two sources for the SAME dependencies.
# ?
# ?   linux-arm64 preset   -> toolchain file -> CMAKE_CROSSCOMPILING = TRUE
# ?                           dev laptop: IntelliSense + cross-compile
# ?                           deps from vendored/
# ?
# ?   jetson-native preset -> no toolchain   -> CMAKE_CROSSCOMPILING = FALSE
# ?                           build.sh on the drone
# ?                           deps from the system (JetPack)
if(CMAKE_CROSSCOMPILING)
    set(_trt_include_dir    ${CMAKE_SOURCE_DIR}/vendored/tensorrt/include)
    set(_trt_lib_dir        ${CMAKE_SOURCE_DIR}/vendored/tensorrt/lib)
    set(_cuda_include_dir   ${CMAKE_SOURCE_DIR}/vendored/cuda/include)
    set(_cuda_lib_dir       ${CMAKE_SOURCE_DIR}/vendored/cuda/lib)
    set(_opencv_include_dir ${CMAKE_SOURCE_DIR}/vendored/opencv/include)
    set(_opencv_lib_dir     ${CMAKE_SOURCE_DIR}/vendored/opencv/lib)
else()
    set(_trt_include_dir    /usr/include/aarch64-linux-gnu)
    set(_trt_lib_dir        /usr/lib/aarch64-linux-gnu)
    set(_cuda_include_dir   /usr/local/cuda/include)
    set(_cuda_lib_dir       /usr/local/cuda/lib64)
    set(_opencv_include_dir /usr/include/opencv4)
    set(_opencv_lib_dir     /usr/lib/aarch64-linux-gnu)
endif()

# * Dependency presence
# ? Cross-build: WARNING, because configure must succeed for CMake Tools to
# ? hand include paths to IntelliSense. The compile then fails with
# ? "NvInfer.h: No such file or directory", naming the same problem.
# ? Jetson: FATAL, because a missing system path means JetPack is broken.
set(_missing_dirs "")
foreach(_dir IN ITEMS ${_trt_include_dir} ${_cuda_include_dir} ${_opencv_include_dir})
    if(NOT IS_DIRECTORY "${_dir}")
        string(APPEND _missing_dirs "\n    ${_dir}")
    endif()
endforeach()
if(NOT _missing_dirs STREQUAL "")
    if(CMAKE_CROSSCOMPILING)
        message(WARNING
            "Missing vendored header directories:${_missing_dirs}\n"
            "  Configure continues so IntelliSense works. Sources that include\n"
            "  these headers will fail to compile until they are populated.")
    else()
        message(FATAL_ERROR
            "Missing dependency directories:${_missing_dirs}\n"
            "  Check the JetPack install.")
    endif()
endif()

message(STATUS "TensorRT: ${_trt_include_dir} | ${_trt_lib_dir}")
message(STATUS "CUDA:     ${_cuda_include_dir} | ${_cuda_lib_dir}")
message(STATUS "OpenCV:   ${_opencv_include_dir} | ${_opencv_lib_dir}")


# * ROS 2 + px4_msgs
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
# ! HEADERS ARE LISTED ON PURPOSE.
# ? CMake does not compile them - it records them in the target's source list.
# ? CMake Tools reads that list through the CMake File API and uses it to
# ? associate each header with this target's compile flags. A header that is
# ? in no target gets no include paths, so every #include in it fails when the
# ? header is opened on its own. That was the yolo.h error.
add_executable(cogidrone
    # Application
    main.cpp
    drone.cpp
    drone.h
    enum_names.h
    logging.cpp
    logging.h

    # Perception
    perception/perception.cpp
    perception/perception.h
    perception/models/trt_engine.cpp
    perception/models/trt_engine.h
    perception/models/yolo.cpp
    perception/models/yolo.h
    perception/models/person.cpp
    perception/models/person.h
    perception/models/head.cpp
    perception/models/head.h
    perception/models/depth_anything.cpp
    perception/models/depth_anything.h
    perception/models/models.h

    # Representation
    # TODO: [#18] Representation
    representation/representation.h

    # Navigation
    navigation/navigation.cpp
    navigation/navigation.h

    # Controllers
    controller/flight_controller.cpp
    controller/flight_controller.h
    controller/flight_commands.h

    # Math
    math/vec3.h
    math/transform.h
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

# * Runtime library resolution
# ? CMAKE_BUILD_RPATH_USE_ORIGIN makes CMake emit $ORIGIN-relative rpaths for
# ? build-tree targets automatically - no manual escaping needed.
set(CMAKE_BUILD_RPATH_USE_ORIGIN TRUE)
set_target_properties(cogidrone PROPERTIES
    BUILD_RPATH "${_px4_lib_dir};${_ros_lib_dirs}"
    INSTALL_RPATH "${_px4_lib_dir};${_ros_lib_dirs}"
)


# * Cross-link resolution
# ? `-rpath-link` resolves the DT_NEEDED entries of the libraries above. `-L`
# ? (from target_link_directories) only finds libraries named on the command
# ? line; it does not find their dependencies.
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
        # ? on symbols that will resolve at runtime on the Jetson.
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