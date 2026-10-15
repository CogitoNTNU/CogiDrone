#pragma once

#include <expected>
#include <atomic>
#include <memory>
#include <mutex>

#include "rclcpp/rclcpp.hpp"
#include "px4_msgs/msg/offboard_control_mode.hpp"
#include "px4_msgs/msg/trajectory_setpoint.hpp"
#include "px4_msgs/msg/vehicle_command.hpp"
#include "px4_msgs/msg/vehicle_local_position.hpp"
#include "px4_msgs/msg/vehicle_status.hpp"

#include "logging.h"


// * Flight Controller class
/**
 * @brief The Pixhawk class represents the Pixhawk flight controller.
 * This class is responsible for communicating with the Pixhawk flight controller 
 * and managing its state; e.g.: Pixhawk.arm();
 */
class Pixhawk {
private:
    // * State machine
    enum class State {
        INIT,                                                                           // ? Created, not streaming yet
        WAITING_EKF,                                                                    // ? Subscribed, waiting for valid local position
        STREAMING,                                                                      // ? Publishing setpoints, not yet in offboard
        OFFBOARD_REQUEST,                                                               // ? Mode switch sent, awaiting ack
        FLYING,                                                                         // ? Armed + offboard active
        LANDING,                                                                        // ? Land commanded
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

        // Control input (from Navigation)
        rclcpp::Subscription<px4_msgs::msg::TrajectorySetpoint>::SharedPtr setpointInputSub;

        // Heartbeat timer - the offboard stream
        rclcpp::TimerBase::SharedPtr streamTimer;
    } m;

    // * Runtime state (written by callbacks/timer on different executor threads)
    // ? These members are categorically different from the assembly struct `M`:
    // ? `M` holds the wiring - handles created once during construction that
    // ? define what the object IS MADE OF (node, publishers, subscriptions,
    // ? timer). These members instead hold what the object EXPERIENCES - the
    // ? evolving condition of the drone:
    // ?     - `state`            changes on every streamTick()
    // ?     - `currentSetpoint`  changes every time Navigation publishes
    // ?     - `lastLocalPos`     changes at PX4's feedback rate
    // ?     - `offboardActive` / `armed` flip as PX4 reports status
    // ? 
    // ? None of these have a meaningful "assembled" value - they are
    // ? initialized to safe defaults (INIT, false, zeroed setpoint) and then
    // ? mutated continuously for the object's whole life by different
    // ? executor threads. Keeping them out of `M` matters because:
    // ?     1. Readability: aggregate-init of `M` in create() reads as "here
    // ?        are the parts"; mixing in runtime defaults would be noise.
    // ?     2. Synchronization: each mutex is grouped with the state it guards,
    // ?        making the thread-safety story visible at a glance instead of
    // ?        scattered between publishers.
    // ?     3. The factory guarantee: create() exists to ensure no partially
    // ?        constructed object escapes - a guarantee about the PARTS (models
    // ?        loaded, node created, wiring done). Runtime state cannot be
    // ?        "partially constructed" in that sense; it starts at safe
    // ?        defaults by definition, so it has no place in the assembly.
    // ?     4. Change isolation: re-wiring topics or swapping models touches
    // ?        `M`; adding a new piece of mutable state (e.g. a battery flag)
    // ?        touches this block. Two different change reasons -> two sections.
    State state = State::INIT;
    std::mutex stateMutex;

    px4_msgs::msg::TrajectorySetpoint currentSetpoint;                                  // guarded by m_setpointMutex
    std::mutex setpointMutex;
    
    px4_msgs::msg::VehicleLocalPosition lastLocalPos;                                   // guarded by m_posMutex
    std::mutex posMutex;
    
    bool offboardActive = false;                                                        // from vehicle_status
    bool armed = false;

    uint8_t streamTicks = 0;                                                            // ticks spent in STREAMING (mode-switch delay)


    // * Ctors & dtor
    explicit Pixhawk(M&& m) : m(std::move(m)) {}
    void wire() noexcept;                                                               // Wire pubs/subs/timer

    // * Callbacks
    void onLocalPosition(const px4_msgs::msg::VehicleLocalPosition& msg);
    void onStatus(const px4_msgs::msg::VehicleStatus& msg);
    void onSetpointInput(const px4_msgs::msg::TrajectorySetpoint& msg);                 // From Navigation
    void streamTick();                                                                  // 10 Hz: the offboard heartbeat + state machine step

    // * Helpers
    void publishOffboardMode();
    void publishSetpoint();
    void sendCommand(uint16_t command, float param1 = 0.f, float param2 = 0.f);
    [[nodiscard]] bool positionValid() const;

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