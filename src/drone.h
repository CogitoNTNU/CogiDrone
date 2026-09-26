#pragma once

#include "perception/perception.h"
#include "link/vehicle_link.h"

#include <rclcpp/rclcpp.hpp>

#include <expected>
#include <memory>


class Drone {
private:
    // Members
    struct M {                                                                          // Assembly struct
        Perception perception;
        std::unique_ptr<IVehicleLink> link;                                              // the vehicle: real over DDS, or a fake

        // ? The node to spin, kept separately from `link` so that IVehicleLink can
        // ? stay free of rclcpp (the whole point of link/vehicle_link.h). Null when
        // ? a fake link was injected - there is then nothing to spin.
        rclcpp::Node::SharedPtr node;
    } m;

    // * Ctors & dtor
    // Ctor
    explicit Drone(M&& m);                                                              // No default ctor exposed

public:
    // Initialization error codes for the Drone class
    enum class InitError {                                                              // ! NOTE: This becomes redundant if we're using std::optional
        ConfigInvalid,
        CameraNotFound,
        ModelLoadFailed,
        ROSContextFailed,
        LinkFailed,                                                                     // could not reach/create the PX4 link
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
    [[nodiscard]] static std::expected<std::unique_ptr<Drone>, InitError>               // ! NOTE: We need to use std::optional if we can't compile with C++23
    initiate(int argc, const char* argv[]);                                             // Factory method

    // ? The injection seam. `struct M` is private, so an overload is the only way
    // ? to hand the Drone a pre-built link - which is what makes FakeLink usable.
    // ! Deliberately does NOT call rclcpp::init(): a fake-injected Drone must be
    // ! constructible in a process where ROS was never initialised at all, or the
    // ! fake buys nothing (third-party/ros2 is arm64-only, so a test host may have
    // ! no working ROS runtime whatsoever).
    [[nodiscard]] static std::expected<std::unique_ptr<Drone>, InitError>
    initiate(std::unique_ptr<IVehicleLink> link);                                       // Factory method, for tests

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

