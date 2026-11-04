#pragma once

#include <expected>
#include <optional>
#include <memory>
#include <string>
#include <mutex>

#include "rclcpp/rclcpp.hpp"
#include "vision_msgs/msg/detection2_d_array.hpp"
#include "std_msgs/msg/string.hpp"
#include "px4_msgs/msg/trajectory_setpoint.hpp"
#include "std_msgs/msg/u_int8.hpp"

#include "logging.h"
#include "controller/flight_commands.h"
#include "math/vec3.h"


// TODO: WE NEED TO REFACTOR THIS ENTIRE FILE, ALONG WITH `navigation.cpp`!
//     : This current implementation was converted by zai/GLM-5.3 from the original Claude generated
//     : code. The code is in the same style as the same project, but it is not idiomatic C++20, and 
//     : it is not well-structured. The code needs to be refactored to be more readable, maintainable, 
//     : and efficient. This includes using modern C++ features, such as smart pointers, ranges, and 
//     : concepts, as well as improving the overall architecture of the navigation system such that
//     : it doesn't do the tasks of the perception and estimation systems. We also have to incorporate
//     : the flightcontroller - as much of this code is what the flightcontroller does. We also have
//     : to split the monolithic navigation class into smaller classes that are more curated to handle
//     : specific subtasks tasks. 

// * Design choice - Why we don't have a fixed-rate control loop
// ? The only arguments for a fixed-rate control loop are:
// ?  - Deterministic `dt` for a controller that integrates - PID integral 
// ?    term, velocity/acceleration feedforward, jerk-limited trajectories. 
// ?    We have none of these; you publish position setpoints.
// ?  - Output rate independent of input rate - matters when the input is slow
// ?    and the output must be fast. But Pixhawk already re-streams at 10 Hz,
// ?    so the wire rate is decoupled from Navigation's rate regardless.
// ?  - Deterministic state-machine cadence - matters if transitions must happen 
// ?    at a known rate. They don't; they're time-based comparisons.
// ? 
// ? None apply. The tick is pure overhead: at 30 Hz with a 30 Hz camera, we're 
// ? running the state machine twice per frame (once from the timer, once from 
// ? the callback) for no benefit.


// * Mission states
// ? The full mission lifecycle. States whose behaviors depend on subsystems
// ? that don't exist yet (drone state source, obstacle map, fused detections)
// ? are declared but their behaviors are stubs - see onControlTick().
enum class MissionState {
    IDLE,                                                                               // Waiting for mission start
    SEARCHING,                                                                          // Looking for person
    PERSON_DETECTED,                                                                    // Person found, confirming track
    FOLLOWING,                                                                          // Actively following person
    PERSON_LOST,                                                                        // Lost sight of person, hovering/searching
    OBSTACLE_AVOIDANCE,                                                                 // TODO: requires Representation
    RETURNING,                                                                          // TODO: requires drone state + home reference
    LANDING,                                                                            // Handed to Pixhawk::land()
    EMERGENCY                                                                           // Hold position, await operator
};


// * Data types
struct PersonDetection {
    cogidrone::Vec3 position;                                                           // TODO: world-frame position (NED) - requires fused detections (depth + intrinsics) from Perception
    float confidence = 0.0f;                                                            // Detection confidence [0-1]
    rclcpp::Time timestamp{};
    bool valid = false;
};


// ? Runtime-tunable ROS parameters, not compile-time constants. `create()`
// ? overwrites them from the parameter server (YAML / `ros2 param set`), so
// ? they must stay plain, mutable, per-instance members.
struct NavigationParams {
    // Following
    float targetFollowDistance      = 4.0f;                                             // meters
    float targetHeight              = 2.5f;                                             // meters above ground
    float maxFollowSpeed            = 3.0f;                                             // m/s
    float minFollowDistance         = 2.0f;                                             // safety minimum
    float maxFollowDistance         = 10.0f;                                            // lose tracking beyond this

    // Detection
    float detectionTimeout          = 2.0f;                                             // seconds
    float minDetectionConfidence    = 0.7f;
    float trackConfirmTime          = 1.0f;                                             // seconds of stable track before committing to FOLLOWING

    // Control
    float controlFrequency          = 30.0f;                                            // Hz
};


// ! APPEARENTLY FALSE COMMENT:
// ? If we ever need to reference the default nagivation parameters, we can use this
// ? constant - which is a copy constructed at compile time. This could be useful for
// ? resetting the parameters to their defaults, or for comparing the current parameters
// Compile-time default parameter set - usable in constant expressions, e.g. validation
// inline constexpr NavigationParams DEFAULT_NAVIGATION_PARAMS{};


class Navigation {
private:
    struct M {                                                                          // Assembly struct
        // ROS node
        rclcpp::Node::SharedPtr node;

        // Subscriptions
        rclcpp::Subscription<vision_msgs::msg::Detection2DArray>::SharedPtr detectionSubscription;   // Fused detections from Perception

        // Publishers
        // ? Contract with Pixhawk: it subscribes to /navigation/setpoint and
        // ? expects px4_msgs::msg::TrajectorySetpoint (NED frame, µs timestamp).
        // ? This publisher IS the navigation->control interface.
        rclcpp::Publisher<px4_msgs::msg::TrajectorySetpoint>::SharedPtr setpointPublisher;
        rclcpp::Publisher<std_msgs::msg::UInt8>::SharedPtr flightCommandPublisher;      // Flight commands

        rclcpp::Publisher<std_msgs::msg::String>::SharedPtr missionStatusPublisher;     // Mission status (introspection only - not consumed by any subsystem)

        // Timer
        // ? Not a control clock. The detection callback drives control. This
        // ? timer exists for one job: detect that NO frames are arriving, so
        // ? the crash protocol can run if Perception dies.
        rclcpp::TimerBase::SharedPtr watchdogTimer;
        
        // Parameters
        // ? Loaded once in create() from the parameter server (defaults from
        // ? NavigationParams). No YAML file; override at runtime with
        // ? `ros2 param set /navigation <name> <value>`.
        NavigationParams params;                                                        // ? Not const: loaded from the parameter server, may be retuned live
    
        // Watchdog
        // const chrono::milliseconds watchdogPeriod;
    } m;

    // * Runtime state (written by callbacks/timer on different executor threads)
    // ? Outside `M` for the same reason as in Pixhawk: `M` is what the object is
    // ? MADE OF (wiring, created once in create()/wire()); this block is what the
    // ? object EXPERIENCES - mutable state initialized to safe defaults and
    // ? mutated for the object's whole life by different executor threads.
    
    // ? One mutex guards ALL of it: the detection callback and the watchdog
    // ? timer both run the state machine, so both touch `person`, `lastHeartbeatTime`,
    // ? `state`, and `stateEntryTime`.
    PersonDetection person;                                                             // guarded by controlMutex
    rclcpp::Time lastHeartbeatTime{};                                                   // guarded by controlMutex - updated on EVERY frame
    
    MissionState state = MissionState::IDLE;                                            // guarded by controlMutex
    rclcpp::Time stateEntryTime{};                                                      // guarded by controlMutex
    
    // ? Helpers assume the caller holds it and do NOT lock internally since 
    // ? `std::mutex` is not recursive.
    std::mutex controlMutex;                                                            // ? Not mutable: locked from non-const methods which invoke const helpers

    // * Default ctor
    explicit Navigation(M&& m) : m(std::move(m)) {}                                     // No default ctor exposed
    void wire() noexcept;                                                               // Wire the node's subscriptions and publishers to the member methods

    // * Callbacks
    void onDetections(const vision_msgs::msg::Detection2DArray& msg);                   // Drives control on every frame
    void onWatchdogTick();                                                              // Fires only when Perception goes silent

    // * Control
    [[nodiscard]] std::optional<MissionState> evaluateTransitions();                    // Returns the next state if a transition is due
    void transitionTo(MissionState next);
    void onEnter(MissionState entered);   
    void onPerceptionCrashed();                                                         // Crash protocol: Perception is gone
    [[nodiscard]] static std::string toString(MissionState state) noexcept;

    // * Behaviors
    // ? A behavior's job: decide the next position setpoint and hand it to
    // ? publishSetpoint(). PX4 owns velocity/attitude control - Navigation
    // ? never commands velocities or motor-level anything.
    void executeBehavior();                                                             // Dispatch on `state`
    void hoverBehavior();                                                               // Implemented: hold position setpoint
    // TODO: followBehavior() - target selection behind the person. Blocked on:
    //     1. Person world position (needs fused detections w/ depth from Perception)
    //     2. Drone state source (estimation scrapped; PX4 local position vs Pixhawk republish - undecided)
    //// void followBehavior();
    
    // TODO: searchBehavior() - search pattern is a design choice (orbit? lawn-mower? hold+rotate?)
    //// void searchBehavior();
    // TODO: obstacle avoidance - blocked on Representation producing an obstacle map

    // * Navigation math
    // // [[nodiscard]] Vec3 followTarget() const;                                            // Target position behind the person
    // // [[nodiscard]] Vec3 velocityTowards(const Vec3& target) const;                       // P-controller velocity command
    // // [[nodiscard]] float desiredYawTowards(const Vec3& target) const;                    // Yaw to face the target
    // // [[nodiscard]] Vec3 cameraToWorld(const Vec3& camera) const;                         // Camera frame -> world frame

    // * Publishing
    void publishSetpoint(const cogidrone::Vec3& positionNed, float yaw);                // The ONLY path to the flight controller
    void publishFlightCommand(cogidrone::FlightCommand command);
    void publishStatus();

    // * Helpers
    [[nodiscard]] bool detectionIsValid() const;
    [[nodiscard]] bool personIsLost() const;
    [[nodiscard]] bool perceptionIsDead() const;


public:
    // * Ctors & dtor
    [[nodiscard]] static std::expected<std::unique_ptr<Navigation>, cogidrone::Error> create();

    ~Navigation();

    Navigation(Navigation&&) noexcept = delete;                                         // ? non-movable, otherwise it causes dangling
    Navigation& operator=(Navigation&&) noexcept = delete;                              // ? [this] pointers in subscription callbacks
    Navigation(const Navigation&) = delete;                                             // non-copyable (one node, one drone)
    Navigation& operator=(const Navigation&) = delete;

    // * Accessors
    [[nodiscard]] inline rclcpp::Node::SharedPtr node() const noexcept { return m.node; }
};