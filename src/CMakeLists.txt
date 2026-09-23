# Each Jazzy package dir is its own include root (headers live at <pkg>/<pkg>/...)
file(GLOB _ros_include_dirs
    LIST_DIRECTORIES true
    ${CMAKE_SOURCE_DIR}/third-party/ros2/opt/ros/jazzy/include/*
)


add_executable(cogidrone
    # Main
    main.cpp
    test.cpp

    # Drone
    drone.cpp

    # Perception
    perception/perception.cpp
    perception/camera/camera.cpp
    perception/vision/yolo.cpp

    # Estimation

    # Representation

    # Navigation

    # Control

    # Act
)


target_include_directories(cogidrone PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${_ros_include_dirs}
)

target_compile_features(cogidrone PRIVATE cxx_std_23)

if(MSVC)
    target_compile_options(cogidrone PRIVATE /W4)
else()
    target_compile_options(cogidrone PRIVATE -Wall -Wextra -Wpedantic)
endif()
