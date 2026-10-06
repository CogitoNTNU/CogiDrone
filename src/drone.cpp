#include "drone.h"

#include <rclcpp/rclcpp.hpp>


// * Ctor & dtor
Drone::~Drone() {
    // Shutdown ROS context if it was initialized
    if (rclcpp::ok()) {
        rclcpp::shutdown();                                                             
    }
}

std::expected<std::unique_ptr<Drone>, cogidrone::Error>
Drone::initiate(int argc, const char* argv[]) {
    // * 1. Initialize ROS global state, so subsystems can create nodes
    if (!rclcpp::ok()) {
        rclcpp::init(argc, argv);
    }

    // * 2. Subsystems (each may create rclcpp::Node objects)
    // 1. Perception
    std::optional<std::unique_ptr<Perception>> perception; {                            // Local
        auto result = Perception::create();
        
        if (!result) {
            return std::unexpected(result.error());                                     // Propagate the error up to the caller
        };

        perception = std::move(*result);                                                // ! THIS CAUSES A MOVE OF THE PERCEPTION OBJECT AFTER WE'VE CAPTURED 
    }                                                                                   // ! `this` IN THE SUBSCRIPTION CALLBACK, LEADING TO A DANGLING POINTER. 

    // 4. Navigation
    std::optional<Navigation> navigation; {
        auto result = Navigation::create();
    
        if (!result) {
            return std::unexpected(result.error());
        }

        navigation = std::move(*result);
    }

    // * 3. Assemble
    // ? We wrap the raw pointer and let a unique_ptr take ownership to prevent leaks 
    // ? caused by not deleting the Drone object. Note that the wrapping is simply  
    // ? here because make_unique<Drone> is unavailable as the constructor is private
    return std::unique_ptr<Drone>(
        new Drone(M{                                                                    // ? We must return a Drone pointer, as the Drone 
            .perception = std::move(*perception),                                       // ? object shouldn't be copyable nor movable, so
            .estimation = nullptr,                                                      // ? we can't return a Drone object by value 
            .representation = nullptr,
            .navigation = std::move(*navigation),                            
            
        })                                                                                  
    );
}



// * Public interface
void Drone::start() {
    // TODO: Ensure that this is the correct place and design choice for starting the Drone object
    rclcpp::executors::MultiThreadedExecutor executor;                                  // Local
    
    executor.add_node(m.perception->node());
    executor.add_node(m.navigation.node());
    
    executor.spin();                                                                    // ! Blocks until shutdown
    // executor destroyed here - releases its node references
}

void Drone::stop() {
    rclcpp::shutdown();
}
