add_executable(cogidrone
    drone.cpp

    
    

    perception/camera/camera_node.cpp
    perception/vision/yolo_detector.cpp
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