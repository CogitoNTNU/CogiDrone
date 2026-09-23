#pragma once

#include "perception/perception.h"

#include <expected>
#include <memory>


class Drone {
private:
    // Members
    struct M {                                                                          // Assembly struct
        Perception perception;
    } m;

    // * Ctors & dtor
    // Ctor
    explicit Drone(M&& m);                                                              // No default ctor exposed

public:
    // Initialization error codes for the Drone class
    enum class InitError {
        ConfigInvalid,
        CameraNotFound,
        ModelLoadFailed,
        ROSContextFailed,
    };
    
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
    [[nodiscard]] static std::expected<std::unique_ptr<Drone>, InitError> 
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

