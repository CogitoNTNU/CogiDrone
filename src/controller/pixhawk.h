#pragma once

#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/twist_stamped.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "std_msgs/msg/string.hpp"
#include "px4_msgs/msg/offboard_control_mode.hpp"
#include "px4_msgs/msg/trajectory_setpoint.hpp"
#include "px4_msgs/msg/vehicle_command.hpp"
#include "px4_msgs/msg/vehicle_local_position.hpp"
#include "px4_msgs/msg/vehicle_status.hpp"

#include "logging.h"


// ? Frames: PX4 works in local NED (x north, y east, z down). The rest of the drone (Navigation,
// ? Estimation) works in ENU (x east, y north, z up), like every other ROS package. The Pixhawk
// ? class is the ONLY place that knows about NED - it converts in both directions.


// ? These are runtime-tunable ROS parameters, loaded in create() (same pattern as NavigationParams)
struct PixhawkParams {
    // Topics
    // ? PX4 >= 1.16 appends `_v<MESSAGE_VERSION>` to versioned topics. Both are v1 in our px4_msgs.
    // ? A wrong name fails silently (no data, no error), so they are parameters.
    std::string statusTopic         = "/fmu/out/vehicle_status_v1";
    std::string localPositionTopic  = "/fmu/out/vehicle_local_position_v1";

    // Flight
    float targetHeight              = 2.5f;                                             // meters above takeoff point (same name as Navigation's)
    float takeoffTolerance          = 0.3f;                                             // meters: "at altitude" when this close
    float commandTimeout            = 0.5f;                                             // seconds: hold position if Navigation goes quiet
    bool autoStart                  = true;                                             // send START_MISSION to Navigation after takeoff
};


// * Flight Controller class
/**
 * @brief The Pixhawk class represents the Pixhawk flight controller (PX4, real or SITL).
 * It is the only link between the drone and PX4 (over uXRCE-DDS), and does three things:
 *   1. Feedback:  PX4 local position (NED)    -> /estimation/vio_odometry (nav_msgs/Odometry, ENU)
 *   2. Control:   /navigation/velocity_setpoint (TwistStamped, ENU) -> PX4 offboard velocity (NED)
 *   3. Lifecycle: offboard + arm, takeoff to `target_height`, hand over to Navigation, land
 *
 * @note Navigation does not take off or land by itself, so the Pixhawk does it:
 *   WAITING_EKF -> STREAMING -> TAKEOFF -> FLYING -> LANDING -> DISARMED
 */
class Pixhawk {
private:
    // * State machine
    enum class State {
        WAITING_EKF,                                                                    // ? Subscribed, waiting for a valid local position
        STREAMING,                                                                      // ? Streaming a hold setpoint; requesting offboard + arm until PX4 accepts
        TAKEOFF,                                                                        // ? Armed + offboard, climbing to `targetHeight`
        FLYING,                                                                         // ? Following Navigation's velocity (or holding if it is quiet)
        LANDING,                                                                        // ? Land commanded, PX4 lands on its own
        DISARMED                                                                        // ? Terminal safe state
    };

    // * Assembly members
    struct M {                                                                          // Assembly struct
        // ROS node
        rclcpp::Node::SharedPtr node;

        // Publishers (to PX4 via uXRCE-DDS bridge)
        rclcpp::Publisher<px4_msgs::msg::OffboardControlMode>::SharedPtr offboardModePub;
        rclcpp::Publisher<px4_msgs::msg::TrajectorySetpoint>::SharedPtr setpointPub;
        rclcpp::Publisher<px4_msgs::msg::VehicleCommand>::SharedPtr commandPub;

        // Subscriptions (PX4 feedback)
        rclcpp::Subscription<px4_msgs::msg::VehicleLocalPosition>::SharedPtr localPosSub;
        rclcpp::Subscription<px4_msgs::msg::VehicleStatus>::SharedPtr statusSub;

        // Control input (from Navigation, intra-process)
        rclcpp::Subscription<geometry_msgs::msg::TwistStamped>::SharedPtr velocityInputSub;
        rclcpp::Subscription<std_msgs::msg::String>::SharedPtr missionStatusSub;

        // Outputs to the rest of the drone
        rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr odometryPub;              // ? Until Estimation exists, PX4's EKF is our state estimate
        rclcpp::Publisher<std_msgs::msg::String>::SharedPtr manualOverridePub;          // ? START_MISSION after takeoff

        // Heartbeat timer - the offboard stream
        rclcpp::TimerBase::SharedPtr streamTimer;

        // Parameters
        PixhawkParams params;                                                           // ? Not const: loaded from the parameter server
    } m;

    // * Runtime state (written by callbacks/timer, read by streamTick)
    // ? These members are categorically different from the assembly struct `M`:
    // ? `M` holds the wiring - handles created once during construction that
    // ? define what the object IS MADE OF (node, publishers, subscriptions,
    // ? timer). These members instead hold what the object EXPERIENCES - the
    // ? evolving condition of the drone. They start at safe defaults and are
    // ? mutated for the object's whole life, so they have no place in the assembly.
    // ?
    // ? Every callback and the timer belong to this node's default callback group,
    // ? which is MutuallyExclusive: even on the MultiThreadedExecutor they never run
    // ? at the same time, so this state needs no lock between THEM. The one exception
    // ? is `state`: land() is public and may be called from another node's thread,
    // ? so `state` is guarded by `stateMutex`. If a callback is ever moved to another
    // ? (Reentrant) callback group, the rest needs a mutex too.
    State state = State::WAITING_EKF;                                                   // guarded by stateMutex
    std::mutex stateMutex;
    int stateTicks = 0;                                                                 // streamTick()s since entering `state`

    std::optional<px4_msgs::msg::VehicleLocalPosition> lastLocalPos;                    // ? nullopt until PX4 sends the first one
    std::optional<px4_msgs::msg::VehicleStatus> lastStatus;

    std::optional<geometry_msgs::msg::TwistStamped> lastVelocity;                       // From Navigation (ENU)
    rclcpp::Time lastVelocityTime;
    std::string missionStatus;                                                          // From Navigation, empty until the first status

    struct Hold { float x, y, z, yaw; };                                                // NED position + heading to hold
    std::optional<Hold> hold;                                                           // ? nullopt while following Navigation's velocity

    // * Ctors & dtor
    explicit Pixhawk(M&& m) : m(std::move(m)) {}
    void wire() noexcept;                                                               // Wire pubs/subs/timer

    // * Callbacks
    void onLocalPosition(const px4_msgs::msg::VehicleLocalPosition& msg);
    void onStatus(const px4_msgs::msg::VehicleStatus& msg);
    void onVelocityInput(const geometry_msgs::msg::TwistStamped& msg);                  // From Navigation
    void onMissionStatus(const std_msgs::msg::String& msg);                             // From Navigation
    void streamTick();                                                                  // 10 Hz: the offboard heartbeat + state machine step

    // * State machine (callers hold stateMutex)
    void transitionTo(State next);
    void beginLanding();                                                                // Send NAV_LAND and enter LANDING
    [[nodiscard]] static const char* toString(State state) noexcept;

    // * Helpers
    void holdHere(std::optional<float> z = std::nullopt);                               // Hold the current position (optionally at another height)
    void streamHold();                                                                  // Offboard heartbeat + position setpoint
    void streamVelocity();                                                              // Offboard heartbeat + Navigation's velocity
    void publishOffboardMode(bool velocity);
    void publishOdometry(const px4_msgs::msg::VehicleLocalPosition& pos);
    void sendCommand(uint32_t command, float param1 = 0.f, float param2 = 0.f);
    [[nodiscard]] uint64_t timestampUs() const;                                         // PX4 timestamps are microseconds
    [[nodiscard]] bool positionValid() const;
    [[nodiscard]] bool armed() const;
    [[nodiscard]] bool armedInOffboard() const;
    [[nodiscard]] bool navigationIsFresh() const;

public:
    // * Ctors & dtor
    [[nodiscard]] static std::expected<std::unique_ptr<Pixhawk>, cogidrone::Error> create();

    ~Pixhawk();

    Pixhawk(Pixhawk&&) noexcept = delete;                                               // ? non-movable: `this` captured in callbacks
    Pixhawk& operator=(Pixhawk&&) noexcept = delete;                                    // ? (same rationale as Perception)
    Pixhawk(const Pixhawk&) = delete;
    Pixhawk& operator=(const Pixhawk&) = delete;

    // * Accessors
    [[nodiscard]] inline rclcpp::Node::SharedPtr node() const noexcept { return m.node; }

    // * Commands (thread-safe; called from any executor thread)
    void land();                                                                        // Command PX4 to land; state machine follows
};
