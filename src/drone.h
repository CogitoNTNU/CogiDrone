#pragma once

#include <expected>
#include <memory>

#include "perception/perception.h"
#include "navigation/navigation.h"
#include "controller/pixhawk.h"
#include "logging.h"



/**
 * @brief The main class representing the drone. This class is the central point of control for the drone, 
 * managing its various subsystems. The main idea behind this design is ROS2 composition architecture. It 
 * allows us to run multiple nodes inside a single process as shared libraries, reducing CPU overhead and 
 * enabling zero-copy intra-process communication. It also gives us explicit control over the construction, 
 * initialization, and ownership of the program hierarchy, which is crucial for robust system design. 
 */
class Drone {
private:
    // Members
    struct M {                                                                          // Assembly struct
        std::unique_ptr<Perception> perception;
        nullptr_t representation;                                                       // TODO: Implement the Representation subsystem
        std::unique_ptr<Navigation> navigation;
        std::unique_ptr<Pixhawk> flightController;
    } m;

    // * Ctors & dtor
    // Ctor
    explicit Drone(M&& m) : m(std::move(m)) {}                                          // No default ctor exposed

public:
    // * Ctors & dtor
    // ? The reason why we use a factory method paired with aggregate 
    // ? initialization for a private struct containing the members of 
    // ? the Drone object, as opposed to the standard ctor pattern, is 
    // ? to ensure that the Drone object is never partially constructed 
    // ? before it is returned to the caller. This way, because the
    // ? subsystems within the Drone class may fail, we can guarantee
    // ? that the caller always receives a valid Drone object (or an 
    // ? error code indicating why the Drone couldn't be initialized),
    // ? as there is no moment where we can read from a Drone that is 
    // ? not fully constructed, eliminating the risk of a caller, or 
    // ? us - the author, using a partially constructed Drone object, 
    // ? which could lead to undefined behavior or crashes. 
    [[nodiscard]] static std::expected<std::unique_ptr<Drone>, cogidrone::Error>        // ! NOTE: We need to use std::optional if we can't compile with C++23
    initiate(int argc, const char* argv[]);                                             // Factory method

    ~Drone();                                                                           // 1. Dtor: declared, requires cleanup via rclcpp::shutdown()  

    // ? The Drone is the single-owner backbone of the entire project: 
    // ? exactly one instance exists per process, and it exclusively owns 
    // ? the hardware-facing subsystems (camera, ROS nodes, flight control).
    // ? Copying would create two Drone objects sharing one set of physical 
    // ? resources - two drones flying one airframe. Moving would leave the
    // ? source object behind as a hollow, which creates a moved-from husk: 
    // ? a Drone that exists but holds nothing - precisely the partially 
    // ? initialized state our factory design exists to make impossible. 
    // ? Ownership transfer therefore happens at the pointer level via 
    // ? unique_ptr<Drone> (see initiate), which is freely movable while 
    // ? the Drone object itself stays pinned in place for its whole life.
    Drone(const Drone&) = delete;                                                       // 2. Copy ctor: deleted
    Drone& operator=(const Drone&) = delete;                                            // 3. Copy assignment: deleted
    Drone(Drone&&) = delete;                                                            // 4. Move ctor: deleted
    Drone& operator=(Drone&&) = delete;                                                 // 5. Move assignment: deleted                                            

    // * Public interface
    void start();                                                                       // Inlined
    void stop();                                                                        // Inlined
};

