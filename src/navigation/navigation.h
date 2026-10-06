#pragma once

#include <chrono>
#include <cmath>
#include <expected>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/twist_stamped.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "std_msgs/msg/string.hpp"
#include "vision_msgs/msg/detection2_d_array.hpp"

#include "errors.h"


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


// NOTE: Minimal linear-algebra types. Eigen is not vendored in third-party/ros2, so the
// navigation math is kept dependency-free until Eigen is actually pulled in.

// * Math
struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    [[nodiscard]] constexpr Vec3 operator+(const Vec3& o) const noexcept { return {x + o.x, y + o.y, z + o.z}; }
    [[nodiscard]] constexpr Vec3 operator-(const Vec3& o) const noexcept { return {x - o.x, y - o.y, z - o.z}; }
    [[nodiscard]] constexpr Vec3 operator*(float s) const noexcept { return {x * s, y * s, z * s}; }
    [[nodiscard]] float norm() const noexcept { return std::sqrt(x * x + y * y + z * z); }
    [[nodiscard]] Vec3 normalized() const noexcept;                                     // ? zero-safe
};

struct Quat {
    float w = 1.0f;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

[[nodiscard]] Vec3 rotate(const Quat& q, const Vec3& v) noexcept;                       // v' = q * v * q^-1
[[nodiscard]] float yawOf(const Quat& q) noexcept;                                      // ZYX yaw extraction


// * Mission states
enum class MissionState {
    IDLE,                                                                               // Waiting for commands
    SEARCHING,                                                                          // Looking for person
    PERSON_DETECTED,                                                                    // Person found, preparing to follow
    FOLLOWING,                                                                          // Actively following person
    PERSON_LOST,                                                                        // Lost sight of person, hovering/searching
    OBSTACLE_AVOIDANCE,                                                                 // Emergency obstacle avoidance mode
    RETURNING,                                                                          // Returning to starting position
    LANDING,                                                                            // Landing sequence
    EMERGENCY                                                                           // Emergency/failsafe mode
};


// * Data types
struct PersonDetection {
    Vec3 position;                                                                      // Position in camera frame
    float confidence = 0.0f;                                                            // Detection confidence [0-1]
    rclcpp::Time timestamp{};
    bool valid = false;
};

struct DroneState {
    Vec3 position;                                                                      // Position in world frame
    Vec3 velocity;                                                                      // Velocity in world frame
    Quat orientation;                                                                   // Orientation quaternion
    float yaw = 0.0f;                                                                   // Yaw angle in radians
    rclcpp::Time timestamp{};
};


// ? These are runtime-tunable ROS parameters, not compile-time constants. `create()` 
// ? overwrites them from the parameter server (YAML / `ros2 param set`), so therefore
// ? they must stay plain, mutable, per-instance members.
struct NavigationParams {
    // Following
    float targetFollowDistance      = 4.0f;                                             // meters
    float targetHeight              = 2.5f;                                             // meters above ground
    float maxFollowSpeed            = 3.0f;                                             // m/s
    float minFollowDistance         = 2.0f;                                             // safety minimum
    float maxFollowDistance         = 10.0f;                                            // lose tracking beyond this
  
    // Safety limits   
    float minAltitude               = 0.5f;                                             // meters
    float maxAltitude               = 10.0f;                                            // meters
    float safetyDistance            = 1.0f;                                             // meters from obstacles

    // Detection
    float detectionTimeout          = 2.0f;                                             // seconds
    float minDetectionConfidence    = 0.7f;

    // Control gains
    float positionPGain             = 1.0f;
    float yawPGain                  = 2.0f;

    // Update rates
    float controlFrequency          = 30.0f;                                            // Hz
};

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

        // Subscriptions and publishers
        rclcpp::Subscription<vision_msgs::msg::Detection2DArray>::SharedPtr detectionSubscription;
        rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr odometrySubscription;
        rclcpp::Subscription<std_msgs::msg::String>::SharedPtr manualOverrideSubscription;
        rclcpp::Publisher<geometry_msgs::msg::TwistStamped>::SharedPtr velocityPublisher;
        rclcpp::Publisher<std_msgs::msg::String>::SharedPtr missionStatusPublisher;

        // Timer
        rclcpp::TimerBase::SharedPtr controlTimer;

        // State
        NavigationParams params;                                                        // ? Not const: loaded from the parameter server, may be retuned live
        MissionState state = MissionState::IDLE;
        PersonDetection person;
        DroneState drone;
        Vec3 personWorldPosition;                                                       // Estimated person position in world frame
        Vec3 personWorldVelocity;                                                       // Estimated person velocity
        rclcpp::Time lastDetectionTime;
        rclcpp::Time stateEntryTime;
        Vec3 homePosition;
        bool homePositionSet = false;
    } m;

    // * Default ctor
    explicit Navigation(M&& m) : m(std::move(m)) {}                                     // No default ctor exposed
    void wire() noexcept;                                                               // Wire the node's subscriptions and publishers to the member methods

    // * Callbacks
    void onDetections(const vision_msgs::msg::Detection2DArray& msg);                   // Fused detections from perception
    void onOdometry(const nav_msgs::msg::Odometry& msg);                                // VIO odometry from estimation
    void onManualOverride(const std_msgs::msg::String& msg);                            // Manual override commands
    void onControlTick();                                                               // Main control loop, runs at params.controlFrequency

    // * State machine
    void updateStateMachine();
    void transitionTo(MissionState next);
    [[nodiscard]] static std::string toString(MissionState state) noexcept;

    // * Behaviors
    void followBehavior();
    void searchBehavior();
    void hoverBehavior();

    // * Navigation math
    [[nodiscard]] Vec3 followTarget() const;                                            // Target position behind the person
    [[nodiscard]] Vec3 velocityTowards(const Vec3& target) const;                       // P-controller velocity command
    [[nodiscard]] float desiredYawTowards(const Vec3& target) const;                    // Yaw to face the target
    [[nodiscard]] Vec3 cameraToWorld(const Vec3& camera) const;                         // Camera frame -> world frame

    // * Safety
    [[nodiscard]] bool commandIsSafe(const Vec3& velocity) const;
    [[nodiscard]] bool detectionIsValid() const;
    [[nodiscard]] bool personIsLost() const;

    // * Publishing
    void publishVelocity(const Vec3& velocity, float yawRate);
    void publishStatus();

public:
    // * Ctors & dtor
    [[nodiscard]] static std::expected<std::unique_ptr<Navigation>, cogidrone::Error> create();

    ~Navigation();

    Navigation(Navigation&&) noexcept = delete;                                         // non-movable, causes dangling [this] pointers in subscription callbacks
    Navigation& operator=(Navigation&&) noexcept = delete;

    Navigation(const Navigation&) = delete;                                             // non-copyable (one node, one drone)
    Navigation& operator=(const Navigation&) = delete;

    // * Accessors
    [[nodiscard]] inline rclcpp::Node::SharedPtr node() const noexcept { return m.node; }
};