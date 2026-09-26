#include "drone.h"

#include <rclcpp/rclcpp.hpp>


// * Ctor & dtor
Drone::Drone(M&& m) : m(std::move(m)) {} 
Drone::~Drone() {
    // Shutdown ROS context if it was initialized
    if (rclcpp::ok()) {
        rclcpp::shutdown();                                                             
    }
}

std::expected<std::unique_ptr<Drone>, Drone::InitError>
Drone::initiate(int argc, const char* argv[]) {
    // 1. Initialize ROS global state, so subsystems can create nodes
    if (!rclcpp::ok()) {
        rclcpp::init(argc, argv);
    }

    // 2. Subsystems (each may create rclcpp::Node objects)
    // auto perception = Perception::create();
    // if (!perception) return std::unexpected(InitError::CameraNotFound);
    Perception perception;                                                              // | Debug as Perception is missing implementation

    // 3. Assemble
    // ? We wrap the raw pointer and let a unique_ptr take ownership to prevent leaks 
    // ? caused by not deleting the Drone object. Note that the wrapping is simply  
    // ? here because make_unique<Drone> is unavailable as the constructor is private
    return std::unique_ptr<Drone>(
        new Drone(M{                                                                    // ? We must return a Drone pointer, as the Drone 
            .perception = std::move(perception),                                        // ? object shouldn't be copyable nor movable, so 
                                                                                        // ? we can't return a Drone object by value                            
        })                                                                                  
    );
}



// * Public interface
void Drone::start() {
    // TODO: Ensure that this is the correct place and design choice for starting the Drone object
    // Start the ROS event loop, which will run until rclcpp::shutdown() is called
    // rclcpp::spin(std::make_shared<rclcpp::Node>("drone_node"));
}

void Drone::stop() {
    rclcpp::shutdown();
}
