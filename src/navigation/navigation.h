#pragma once

#include <expected>
#include <optional>
#include <memory>
#include <string>
// | #include <mutex>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"
#include "std_msgs/msg/bool.hpp"
#include "std_msgs/msg/u_int8.hpp"
#include "px4_msgs/msg/trajectory_setpoint.hpp"
#include "vision_msgs/msg/detection2_d_array.hpp"

#include "logging.h"
#include "controller/flight_commands.h"
#include "math/vec3.h"


// * Design choice - Why we don't have a fixed-rate control loop
// ? The only arguments for a fixed-rate control loop are:
// ?  - Deterministic `dt` for a controller that integrates - PID integral 
// ?    term, velocity/acceleration feedforward, jerk-limited trajectories. 
// ?    We have none of these; you publish position setpoints.
// ?  - Output rate independent of input rate - matters when the input is slow
// ?    and the output must be fast. But FlightController already re-streams at 10 Hz,
// ?    so the wire rate is decoupled from Navigation's rate regardless.
// ?  - Deterministic state-machine cadence - matters if transitions must happen 
// ?    at a known rate. They don't; they're time-based comparisons.
// ? 
// ? None apply. The tick is pure overhead: at 30 Hz with a 30 Hz camera, we're 
// ? running the state machine twice per frame (once from the timer, once from 
// ? the callback) for no benefit.


// * Control state
// ? The relationship between Navigation and PX4, derived from the two booleans
// ? FlightController publishes (/pixhawk/armed and /pixhawk/offboard_active).
// ? This is the "green light" in explicit form: three named states instead of
// ? two booleans whose combination the reader has to derive.
// ?
// ?   WAITING_FOR_ARM        !armed                  - pilot has not armed yet
// ?   PILOT_HAS_CONTROL      armed && !offboard      - pilot is flying
// ?   NAVIGATION_HAS_CONTROL armed &&  offboard      - we are flying
enum class ControlState {
    WAITING_FOR_ARM,
    PILOT_HAS_CONTROL,
    NAVIGATION_HAS_CONTROL,
};


// * Mission states
// ? The full mission lifecycle. States whose behaviors depend on subsystems
// ? that don't exist yet (drone state source, obstacle map, fused detections)
// ? are declared but their behaviors are stubs.
enum class MissionState {
    IDLE,                                                                               // Waiting for control
    SEARCHING,                                                                          // Looking for person
    PERSON_DETECTED,                                                                    // Person found, confirming track
    FOLLOWING,                                                                          // Actively following person
    PERSON_LOST,                                                                        // Lost sight of person, hovering/searching
    // | OBSTACLE_AVOIDANCE,                                                                 // TODO: requires Representation
    // | RETURNING,                                                                          // TODO: requires drone state + home reference
    LANDING,                                                                            // Handed to FlightController::land()
    // | EMERGENCY                                                                           // Hold position, await operator
};


// * Data types
struct PersonDetection {
    cogidrone::Vec3 position;                                                           // TODO: world-frame position (NED) - requires fused detections (depth + intrinsics) from Perception
    float confidence = 0.0f;                                                            // Detection confidence [0-1]
    // ! The clock type is load-bearing. `rclcpp::Time{}` defaults to
    // ! RCL_SYSTEM_TIME, but `node->now()` returns RCL_ROS_TIME, and
    // ! `rclcpp::Time::operator-` THROWS std::runtime_error when the two
    // ! differ. Value 0 with RCL_ROS_TIME means "no detection has ever
    // ! arrived", which is exactly what personIsLost() needs.
    rclcpp::Time timestamp{0, RCL_ROS_TIME};
    bool valid = false;
};


// ? Loaded ONCE in create() from the parameter server (defaults from
// ? NavigationParams). No YAML file; override at startup with
// ? `ros2 param set /navigation <name> <value>` BEFORE the node spins.
// ? NOT retuned live: there is no add_on_set_parameters_callback, so a
// ? `ros2 param set` after startup changes the parameter server but NOT
// ? `m.params`. Add the callback if live retuning is ever needed.
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
};


class Navigation {
private:
    struct M {                                                                          // Assembly struct
        // ROS node
        rclcpp::Node::SharedPtr node;

        // Subscriptions
        rclcpp::Subscription<vision_msgs::msg::Detection2DArray>::SharedPtr detectionSubscription;  // Fused detections from Perception
        rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr offboardActiveSubscription;            // From FlightController
        rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr armedSubscription;                     // From FlightController

        // Publishers
        // ? Contract with FlightController: it subscribes to /navigation/setpoint and
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
        NavigationParams params;                                                        // ? Loaded once in create(); not retuned at runtime
    } m;

    // * Runtime state (written by callbacks/timer on different executor threads)
    // ? Outside M for the same reason as in FlightController: M is what the object is
    // ? MADE OF (wiring, created once in create()/wire()); this block is what the
    // ? object EXPERIENCES - mutable state initialized to safe defaults and
    // ? mutated for the object's whole life by different executor threads.
    // ?
    // ! THREAD-SAFETY INVARIANT - currently upheld by EXECUTOR SERIALIZATION only.
    // ? onDetections() (subscription), onWatchdogTick() (timer), onArmed()
    // ? (subscription), and onOffboardActive() (subscription) are all created in
    // ? wire() WITHOUT an explicit callback group. rclcpp therefore assigns all
    // ? four to the node's DEFAULT callback group, which is
    // ? CallbackGroupType::MutuallyExclusive (rclcpp node_base.cpp). The executor
    // ? refuses to take a second entity from a group whose can_be_taken_from
    // ? atomic is false, and only resets it after the callback returns (rclcpp
    // ? executor.cpp). Therefore no two callbacks can run concurrently - not
    // ? under MultiThreadedExecutor, and not with intra-process comms enabled
    // ? (the intra-process waitable is added to the SAME callback group -
    // ? rclcpp node_topics.cpp).
    // ?
    // ! RULES FOR ANY FUTURE CALLBACK:
    // ?    - If it touches person, lastHeartbeatTime, state, stateEntryTime,
    // ?      armed, offboardActive, or controlState, it MUST be created without
    // ?      an explicit callback group (i.e. join the default MutuallyExclusive
    // ?      group).
    // ?    - PREFER posting a request (std::atomic) and letting the control step
    // ?      consume it. That stays correct even if the callback is later moved
    // ?      to a Reentrant group. See onArmed() / onOffboardActive().
    // ?
    // ! REINSTATING controlMutex (if the invariant is ever broken):
    // ?    1. Uncomment `#include <mutex>` at the top of this file.
    // ?    2. Uncomment `std::mutex controlMutex;` below.
    // ?    3. Uncomment `std::lock_guard lock(controlMutex);` at the top of
    // ?       onDetections(), onWatchdogTick(), onArmed(), and onOffboardActive().
    // ?    4. Do NOT lock inside the helpers (detectionIsValid, personIsLost,
    // ?       perceptionIsDead, deriveControlState) - std::mutex is not recursive.
    // ?       They assume the caller holds it.
    // ?    5. Re-verify: every path that reads/writes the members below must be
    // ?       reachable only from a lock-holding callback.
    PersonDetection person;                                                             // guarded by controlMutex (if reinstated)
    rclcpp::Time lastHeartbeatTime{};                                                   // guarded by controlMutex (if reinstated) - updated on EVERY frame
    
    MissionState state = MissionState::IDLE;                                            // guarded by controlMutex (if reinstated)
    rclcpp::Time stateEntryTime{};                                                      // guarded by controlMutex (if reinstated)
    
    // ? No mutex today: the executor serializes all callbacks (see invariant above).
    // ? To reinstate, uncomment the line below and follow the guide above.
    // | std::mutex controlMutex;

    // ? The two raw inputs from FlightController. Written by onArmed() /
    // ? onOffboardActive(), read by deriveControlState().
    bool armed = false;
    bool offboardActive = false;

    // ? The derived green light. Recomputed by updateControlState() whenever
    // ? either raw input changes. Every control step reads this, never the raw
    // ? booleans, so the three-way decision is made in exactly one place.
    ControlState controlState = ControlState::WAITING_FOR_ARM;

    // * Default ctor
    explicit Navigation(M&& m) : m(std::move(m)) {}                                     // No default ctor exposed
    void wire() noexcept;                                                               // Wire the node's subscriptions and publishers to the member methods

    // * Callbacks
    void onDetections(const vision_msgs::msg::Detection2DArray& msg);                   // Drives control on every frame
    void onArmed(const std_msgs::msg::Bool& msg);                                       // From FlightController
    void onOffboardActive(const std_msgs::msg::Bool& msg);                              // From FlightController
    void onWatchdogTick();                                                              // Fires only when Perception goes silent

    // * Control state
    [[nodiscard]] ControlState deriveControlState() const noexcept;                     // Pure: armed + offboardActive -> ControlState
    void updateControlState();                                                          // Recompute + dispatch to the matching handler
    void onWaitingForArm();                                                             // `!armed`: mission over, return to IDLE
    void onPilotTookControl();                                                          // `armed && !offboard`: freeze, keep state
    void onNavigationTookControl();                                                     // `armed && offboard`: start or resume

    // * Mission state machine
    [[nodiscard]] std::optional<MissionState> evaluateTransitions();                    // Returns the next state if a transition is due
    void transitionTo(MissionState next);
    void onEnter(MissionState entered);   
    void onPerceptionCrashed();                                                         // Crash protocol: Perception is gone

    // * Behaviors
    // ? A behavior's job: decide the next position setpoint and hand it to
    // ? publishSetpoint(). PX4 owns velocity/attitude control - Navigation
    // ? never commands velocities or motor-level anything.
    void executeBehavior();                                                             // Dispatch on `state`
    void hoverBehavior();                                                               // Implemented: hold position setpoint
    // TODO: followBehavior() - target selection behind the person. Blocked on:
    //     1. Person world position (needs fused detections w/ depth from Perception)
    //     2. Drone state source (estimation scrapped; PX4 local position vs FlightController republish - undecided)
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

    // * State queries 
    [[nodiscard]] bool detectionIsValid() const;
    [[nodiscard]] bool personIsLost() const;
    [[nodiscard]] bool perceptionIsDead() const;


    // * Helpers
    [[nodiscard]] static std::string toString(MissionState state) noexcept;
    [[nodiscard]] static std::string toString(ControlState state) noexcept;


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