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
#include "px4_msgs/msg/vehicle_local_position.hpp"
// #include "px4_msgs/msg/vehicle_attitude.hpp"
#include "vision_msgs/msg/detection2_d_array.hpp"

#include "logging.h"
#include "controller/flight_commands.h"
#include "math/vec3.h"
#include "math/transform.h"
#include "enum_names.h"


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

// TODO: [45] Upgrade to full quaternion for camera->world conversion
// * Frame chain - why Navigation converts, and why yaw-only
// ? Three frames are in play. Mixing them up is the easiest way to fly the
// ? drone into something:
// ?
// ?   CAMERA   x right, y DOWN,  z forward     OpenCV convention
// ?   BODY     x forward, y right, z down      PX4 FRD
// ?   WORLD    x north, y east,  z down        PX4 NED
// ?
// ? Perception reports in CAMERA frame on purpose. A sensor node should not
// ? need the vehicle's orientation, and keeping the conversion here means the
// ? Perception pipeline stays testable without a flight controller.
// ?
// ?   CAMERA -> BODY   fixed. The camera is bolted to the airframe, so this is
// ?                    a pure axis permutation: body = {cam.z, cam.x, cam.y}.
// ?                    See cogidrone::cameraToBody() in math/transform.h.
// ?   BODY   -> WORLD  NOT fixed. This is the vehicle's attitude, which changes
// ?                    every control cycle.
// ?
// ? THE APPROXIMATION - tracked as [#45]
// ? We build the body->world rotation from YAW ALONE, using
// ? `VehicleLocalPosition.heading` - a field we already subscribe to. Roll and
// ? pitch are treated as zero.
// ?
// ? Why that is sufficient TODAY:
// ?   - Every consumer of `person.position` reads only .x and .y.
// ?     followBehavior() and detectionIsValid() never touch .z.
// ?   - The horizontal projection of a camera-frame point depends on yaw alone.
// ?     Roll and pitch only leak into the horizontal plane through the VERTICAL
// ?     component of the detection.
// ?
// ? The error is bounded and small:
// ?
// ?     horizontal_error  ~=  |camera.y| * sqrt(pitch^2 + roll^2)
// ?
// ?   |camera.y| is the person's offset below the camera axis. For a drone at
// ?   2.5 m looking at a 1.7 m person, that is ~0.8 m. Against a 4 m standoff:
// ?
// ?     tilt  5 deg  ->  0.07 m  (1.7%)
// ?     tilt 10 deg  ->  0.14 m  (3.5%)
// ?     tilt 20 deg  ->  0.28 m  (7.0%)
// ?     tilt 30 deg  ->  0.42 m  (10.5%)
// ?
// ? A position-hold drone sits under ~10 deg in calm air. Even a 30 deg transit
// ? leaves the standoff within half a metre.
// ?
// ? WHY NOT SUBSCRIBE TO vehicle_attitude
// ? It publishes at ~250 Hz (EKF2 rate) versus the camera's 30 Hz, and it would
// ? join the same MutuallyExclusive callback group - so it would serialize
// ? against onDetections(), which runs the full YOLO pipeline for 20-30 ms.
// ? The CPU cost is negligible (~0.025%), but the conceptual cost is not: a
// ? second high-rate topic, a second callback, a second state member, and a
// ? second entry in the thread-safety invariant below. All to compute a .z that
// ? nothing reads.
// ?
// ! WHAT THIS COSTS
// ! `person.position.z` is a camera-frame value rotated by yaw only. It is NOT
// ! a trustworthy world altitude. Nothing reads it today. If something starts
// ! to, that is the trigger to upgrade.
// !
// ! UPGRADE TRIGGERS - switch to the full quaternion when ANY of these is true:
// !   1. A gimbal or a non-forward camera mount lands. Then camera->body is no
// !      longer a fixed permutation, which is exactly [#32].
// !   2. `person.position.z` is needed for anything (terrain-relative altitude,
// !      a 3D follow point, a drop-release height).
// !   3. Flight testing shows the tilt error matters against the standoff.
// !
// ! See [#45] for the tracking issue.
// !
// ! The upgrade is mechanical: subscribe to /fmu/out/vehicle_attitude
// ! (best_effort, same as local position), store the quaternion, and pass it to
// ! cogidrone::cameraToWorld() instead of the yaw-only Quat built in
// ! onDetections(). cameraToWorld() already takes a full Quat - no signature
// ! change is needed.

// * Control state
// ? The relationship between Navigation and PX4, derived from the two booleans
// ? FlightController publishes (/pixhawk/armed and /pixhawk/offboard_active).
// ? This is the "green light" in explicit form: three named states instead of
// ? two booleans whose combination the reader has to derive.
// ?
// ?   WAITING_FOR_ARM        !armed                  - pilot has not armed yet
// ?   PILOT_HAS_CONTROL      armed && !offboard      - pilot is flying
// ?   NAVIGATION_HAS_CONTROL armed &&  offboard      - we are flying
#define COGIDRONE_CONTROL_STATE_LIST(X)                                                 \
    X(WAITING_FOR_ARM)                                                                  \
    X(PILOT_HAS_CONTROL)                                                                \
    X(NAVIGATION_HAS_CONTROL)

enum class ControlState {
#define COGIDRONE_ENUM_MEMBER(name) name,
    COGIDRONE_CONTROL_STATE_LIST(COGIDRONE_ENUM_MEMBER)
#undef COGIDRONE_ENUM_MEMBER
};

[[nodiscard]] constexpr auto enumNames(ControlState) noexcept {
    return std::array{
#define COGIDRONE_ENUM_NAME(name) std::string_view{#name},
        COGIDRONE_CONTROL_STATE_LIST(COGIDRONE_ENUM_NAME)
#undef COGIDRONE_ENUM_NAME
    };
}



// * Mission states
// ? The mission this drone is built for, in order:
// ?
// ?   1. TAKEOFF         - implicit, see below
// ?   2. SEARCHING       - rotate in place until a person is found
// ?   3. PERSON_DETECTED - confirm the track before committing
// ?   4. FOLLOWING       - stand off behind the person
// ?   5. DROPPING        - release the payload            [MISSING: hardware]
// ?   6. RETURNING       - fly back to the launch point   [MISSING: home ref]
// ?   7. LANDING         - hand the descent to PX4
// ?
// ?   After 5 the mission branches: items remain -> back to 2, else -> 6.
// ?
// ? WHAT IS IMPLEMENTED TODAY
// ?   IDLE, SEARCHING, PERSON_DETECTED, FOLLOWING, PERSON_LOST.
// ?   TAKEOFF has no state because Navigation never commands the climb: the
// ?   first setpoint from searchBehavior() is (current XY, -targetHeight), so
// ?   PX4's position controller does the climb. A separate state would only
// ?   duplicate what the setpoint already says.
// !   Side effect worth knowing: the drone climbs AND rotates at the same
// !   time, because searchBehavior() runs from the first frame. Harmless, but
// !   it is not a clean vertical takeoff.
// ?
// ? WHY THE MISSION CANNOT END TODAY
// ?   DROPPING is blocked on HARDWARE - there is no release mechanism on the
// ?   airframe, so there is no command to send and no feedback to confirm it.
// ?   RETURNING is blocked on a HOME REFERENCE decision - PX4's local frame is
// ?   home-relative, so (0, 0) IS the launch point, but "return" also needs a
// ?   policy for what to do if the path is blocked, which needs Representation.
// ?
// ?   Consequence, stated plainly: FOLLOWING is the TERMINAL state of the
// ?   current implementation. The drone follows the person until the pilot
// ?   takes over or disarms. That is the intended behaviour today, not a bug.
// !   Do NOT add a timeout that lands the drone. Landing is not the mission's
// !   end - returning is - and returning does not exist yet. A timer that
// !   lands turns "we cannot finish" into "we give up", which is a different
// !   mission with different consequences for the payload.
// ?
// ? WHAT TO DO WHEN THE HARDWARE ARRIVES
// ?   1. Add DROPPING to the list below, between FOLLOWING and LANDING.
// ?   2. Add dropBehavior(): command the release, then WAIT for confirmation.
// ?      It must not publish a setpoint while the release fires - hold
// ?      position, or the item lands somewhere else.
// ?   3. Add RETURNING and returnBehavior(): command (0, 0, -targetHeight).
// ?      The origin is the launch point in PX4's local frame, so the simple
// ?      case needs no extra state.
// ?   4. Add the item counter. It is the only thing that decides between
// ?      "search again" and "return", and it is mission policy, not flight
// ?      logic - consider a MissionPlanner that owns it and drives Navigation.
// ?   5. Only then wire LANDING: RETURNING -> LANDING on arrival, and
// ?      DROPPING -> SEARCHING when items remain.
// ?   6. When LANDING becomes reachable, fix deriveControlState() FIRST - see
// ?      the warning there. PX4 leaves offboard by design when it lands.
// ?
// ?   Until step 5, LANDING stays unreachable. It is kept in the enum because
// ?   it is the abort path, and deleting it would mean re-adding it later.
// | OBSTACLE_AVOIDANCE     - TODO: [#27] requires Representation
// | EMERGENCY              - TODO: [#28] hold position, await operator

#define COGIDRONE_MISSION_STATE_LIST(X)                                                 \
    X(IDLE)                                                                             \
    X(SEARCHING)                                                                        \
    X(PERSON_DETECTED)                                                                  \
    X(FOLLOWING)                                                                        \
    X(PERSON_LOST)                                                                      \
    X(LANDING)

enum class MissionState {
#define COGIDRONE_ENUM_MEMBER(name) name,
    COGIDRONE_MISSION_STATE_LIST(COGIDRONE_ENUM_MEMBER)
#undef COGIDRONE_ENUM_MEMBER
};

[[nodiscard]] constexpr auto enumNames(MissionState) noexcept {
    return std::array{
#define COGIDRONE_ENUM_NAME(name) std::string_view{#name},
        COGIDRONE_MISSION_STATE_LIST(COGIDRONE_ENUM_NAME)
#undef COGIDRONE_ENUM_NAME
    };
}


// * Data types
struct PersonDetection {
    // ! `.z` is NOT a trustworthy world altitude.
    // ? See the "Frame chain" block above for the yaw-only approximation this uses. 
    cogidrone::Vec3 position;                                                           // World-frame NED, converted from Perception's camera-frame XYZ in onDetections(). 
    
    float confidence = 0.0f;                                                            // Detection confidence [0-1]
    
    // ! The clock type is load-bearing. `rclcpp::Time{}` defaults to
    // ! RCL_SYSTEM_TIME, but `node->now()` returns RCL_ROS_TIME, and
    // ! `rclcpp::Time::operator-` THROWS std::runtime_error when the two
    // ! differ. Value 0 with RCL_ROS_TIME means "no detection has ever
    // ! arrived", which is exactly what personIsLost() needs.
    // ! Three arguments, not two: `Time{0, RCL_ROS_TIME}` is ambiguous between
    // ! Time(int32_t, uint32_t, rcl_clock_type_t) and Time(int64_t, rcl_clock_type_t),
    // ! because an unscoped enum converts to uint32_t. The 3-arg form is
    // ! unambiguous by arity.
    rclcpp::Time timestamp{0, 0, RCL_ROS_TIME};
    
    bool valid = false;
};


// ? Loaded ONCE in create() from the parameter server (defaults from
// ? NavigationParams). No YAML file; override at startup with
// ? `ros2 param set /navigation <name> <value>` BEFORE the node spins.
// ? NOT retuned live: there is no add_on_set_parameters_callback, so a
// ? `ros2 param set` after startup changes the parameter server but NOT
// ? `m.params`. Add the callback if live retuning is ever needed.
struct NavigationParams {
    // Searching
    // TODO : [#23] Rework searchBehavior() to use Representation's map of the environment.
    //      : The current searchBehavior is a simple rotate-in-place. This is a design
    //      : choice, we could also have a mix of: orbit, lawn-mower, and hold+rotate.
    float searchYawRate             = 0.3f;                                             // rad/s

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
        
        // ! DOUBLE CHECK AFTER REFACTOR: (might be better to have FlightController 
        // ! publish a reference or pointer of it's localPosition member instead 
        // ! of having Navigation subscribe to PX4's local position directly)
        // ? Navigation reads PX4's local position itself rather than going
        // ? through FlightController. FlightController owns the COMMAND link
        // ? (the offboard handshake); telemetry is a separate concern, and
        // ? routing it through FlightController would add a topic and a
        // ? publisher/subscriber pair for no benefit.
        // ! QoS MUST be best_effort: PX4 publishes vehicle_local_position as
        // ! best_effort, and a reliable subscriber will not match it.
        rclcpp::Subscription<px4_msgs::msg::VehicleLocalPosition>::SharedPtr localPositionSubscription;

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
    // ?      armed, offboardActive, dronePosition, or controlState, it MUST be
    // ?      created without an explicit callback group (i.e. join the default
    // ?      MutuallyExclusive group).
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

    // ! DOUBLE CHECK AFTER REFACTOR: (might be better to have FlightController 
    // ! publish a reference or pointer of it's localPosition member instead 
    // ! of having Navigation subscribe to PX4's local position directly)
    // ? Latest PX4 local position (NED, home-relative). Written by
    // ? onLocalPosition(), read by the behaviors. Same serialization argument
    // ? as the rest of this block: the executor runs one callback at a time.
    px4_msgs::msg::VehicleLocalPosition dronePosition;

    // TODO: [#23] Rework searchBehavior() to use Representation's map of the environment.
    //     : The current searchBehavior is a simple rotate-in-place. This is a design
    //     : choice, we could also have a mix of: orbit, lawn-mower, and hold+rotate.
    // ?
    // ? Heading the search sweep started from. Captured on entry to SEARCHING so
    // ? the sweep never snaps the drone round to a fixed heading.
    float searchStartYaw = 0.0f;

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
    void onLocalPosition(const px4_msgs::msg::VehicleLocalPosition& msg);               // From PX4 via the uXRCE-DDS bridge
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
    void hoverBehavior();                                                               // Hold current XY at targetHeight
    // TODO: [#23] Rework searchBehavior() to use Representation's map of the environment.
    //     : The current searchBehavior is a simple rotate-in-place. This is a design
    //     : choice, we could also have a mix of: orbit, lawn-mower, and hold+rotate.
    void searchBehavior();
    void followBehavior();
    // TODO: [#27] Obstacle avoidance - blocked on Representation producing an obstacle map
    
    // * Publishing
    void publishSetpoint(const cogidrone::Vec3& positionNed, float yaw);                // The ONLY path to the flight controller
    void publishFlightCommand(cogidrone::FlightCommand command);
    void publishStatus();

    // * State queries 
    [[nodiscard]] bool detectionIsValid() const;
    [[nodiscard]] bool personIsLost() const;
    [[nodiscard]] bool perceptionIsDead() const;
    [[nodiscard]] bool dronePositionValid() const;                                      // EKF has a usable position reference


    // * Helpers
    // NOTE: 
    // I wish Ubuntu 24.04 had GCC16 support as enum class reflection, which was
    // introduced in C++26, generalizes this metaprogramming bullshit, along with
    // all the other boilerplate crap into this piece of beautiful code:
    /*
        template <typename E> requires std::is_enum_v<E>
        constexpr std::string_view enumName(E value) {
            template for (constexpr auto e : std::meta::enumerators_of(^^E)) {
                if (value == [:e:]) return std::meta::identifier_of(e);
            }
            return "UNKNOWN";
        }
    */
    // but ofc it's out of fucking reach as of today.. so you'll have to do with
    // this monstrocity below :) (Good fucking luck figuring out how the 
    // `cogidrone::NamedEnum` concept + X-macro method works!)     
    // 
    // Thanks for coming to my TED talk
    //   - Elias Alexander Wiklund Ottersbo
    template <cogidrone::NamedEnum E>
    [[nodiscard]] static constexpr std::string_view toString(E state) noexcept {
        return cogidrone::enumName(state);
    }


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