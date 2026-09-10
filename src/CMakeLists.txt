add_executable(cogidrone
    # Main
    drone.cpp

    # Perception
    perception/camera/camera.cpp
    perception/vision/yolo.cpp

    # Estimation

    # Representation

    # Navigation

    # Control

    # Act
)

target_include_directories(cogidrone
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(cogidrone PRIVATE cxx_std_23)

if(MSVC)
    target_compile_options(cogidrone PRIVATE /W4)
else()
    target_compile_options(cogidrone PRIVATE -Wall -Wextra -Wpedantic)
endif()
