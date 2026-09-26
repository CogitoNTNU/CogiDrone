# Each Jazzy package dir is its own include root (headers live at <pkg>/<pkg>/...)
file(GLOB _ros_include_dirs
    LIST_DIRECTORIES true
    ${CMAKE_SOURCE_DIR}/third-party/ros2/opt/ros/jazzy/include/*
)

# Glob the lib dir (top of file, next to the include glob)
file(GLOB _ros_lib_dirs ${CMAKE_SOURCE_DIR}/third-party/ros2/opt/ros/jazzy/lib)

# * Sources
add_executable(cogidrone
    # Main
    main.cpp
    test.cpp

    # Drone
    drone.cpp

    # Perception
    perception/perception.cpp
    perception/models/person.cpp
    perception/models/head.cpp
    perception/models/depth_anything.cpp

    # Estimation

    # Representation

    # Navigation

    # Control

    # Act
)

# * Include
target_include_directories(cogidrone PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${_ros_include_dirs}
)

# * Link
target_link_directories(cogidrone PRIVATE ${_ros_lib_dirs})
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
    libstatistics_collector rosgraph_msgs__rosidl_typesupport_cpp                       # `statistics_msgs`: bare msg lib doesn't exist; 
    tracetools rcl_yaml_param_parser rcl_interfaces__rosidl_typesupport_cpp             #                    typesupport variant covers it
    ament_index_cpp class_loader fastcdr fastrtps                                       # `console_bridge`: not vendored; runtime dep on the Jetson only    
)

target_link_options(cogidrone PRIVATE
    "-Wl,-rpath-link,${CMAKE_SOURCE_DIR}/third-party/ros2/opt/ros/jazzy/lib"
    "-Wl,-rpath-link,${CMAKE_SOURCE_DIR}/third-party/ros2/usr/lib/aarch64-linux-gnu"

    # ? UNCOMMENT THIS IF WE EVER WANNA BLINDLY LINK TO A LIB WHICH WE KNOW EXISTS ON THE TARGET, BUT WHICH IS NOT PRESENT IN THE CROSS-TOOLCHAIN SYSROOT!
    # "-Wl,--allow-shlib-undefined"            
)    

# * Compile
target_compile_features(cogidrone PRIVATE cxx_std_23)

if(MSVC)
    target_compile_options(cogidrone PRIVATE /W4)
else()
    target_compile_options(cogidrone PRIVATE -Wall -Wextra -Wpedantic)
endif()
