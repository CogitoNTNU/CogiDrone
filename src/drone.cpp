#include "drone.h"

#include <rclcpp/rclcpp.hpp>


// * Ctor & dtor
Drone::Drone(M&& m) : m(std::move(m)) {} 
Drone::~Drone() {
    if (rclcpp::ok()) {
        rclcpp::shutdown();                                                             // Shutdown ROS context if it was initialized
    }
}


std::expected<std::unique_ptr<Drone>, Drone::InitError>
Drone::initiate(int argc, const char* argv[]) {
    // 1. ROS global state - first thing, so subsystems can create nodes
    if (!rclcpp::ok()) {
        rclcpp::init(argc, argv);
    }

    // 2. Subsystems (each may create rclcpp::Node objects)
    // auto perception = Perception::create();
    // if (!perception) return std::unexpected(InitError::CameraNotFound);

    // 3. Assemble
    return std::unique_ptr<Drone>(
        new Drone(M{
            .perception = std::move(*perception),
        })
    );
}
