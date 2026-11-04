#include "navigation.h"

#include <chrono>
#include <string>



// * Ctors & dtor
Navigation::~Navigation() = default;

std::expected<std::unique_ptr<Navigation>, cogidrone::Error> Navigation::create() {
    using namespace std::literals::string_view_literals;                                // ? for "sv" suffix

    // * ROS2
    auto node = std::make_shared<rclcpp::Node>(
        "navigation",
        rclcpp::NodeOptions().use_intra_process_comms(true)
    );
    cogidrone::log(LOGGING_LEVEL::INFO, "ROS2 IPC node created successfully"sv);
    
    // * Parameters (non-failable: defaults live in NavigationParams)
    NavigationParams params;
    params.targetFollowDistance   = node->declare_parameter("target_follow_distance", params.targetFollowDistance);
    params.targetHeight           = node->declare_parameter("target_height", params.targetHeight);
    params.maxFollowSpeed         = node->declare_parameter("max_follow_speed", params.maxFollowSpeed);
    params.minFollowDistance      = node->declare_parameter("min_follow_distance", params.minFollowDistance);
    params.maxFollowDistance      = node->declare_parameter("max_follow_distance", params.maxFollowDistance);
    params.detectionTimeout       = node->declare_parameter("detection_timeout", params.detectionTimeout);
    params.minDetectionConfidence = node->declare_parameter("min_detection_confidence", params.minDetectionConfidence);
    params.trackConfirmTime       = node->declare_parameter("track_confirm_time", params.trackConfirmTime);
    params.controlFrequency       = node->declare_parameter("control_frequency", params.controlFrequency);
    
    // Ensure controlFrequency is positive to avoid division by zero when calculating the timer period
    if (params.controlFrequency <= 0.0f) {
        return std::unexpected(cogidrone::Error{
            .message = "Navigation parameter `controlFrequency` must be positive!"sv
        });
    }

    cogidrone::log(LOGGING_LEVEL::INFO, "Navigation parameters initialized successfully"sv);

    // * Partial assembly
    auto n = std::unique_ptr<Navigation>(
        new Navigation(M{                                                               // ? Transfer ownership into unique_ptr as ctor is private
            .node = std::move(node),
            .detectionSubscription = nullptr,                                           // ? will be wired later
            .manualOverrideSubscription = nullptr,                                      // ? will be wired later
            .setpointPublisher = nullptr,                                               // ? will be wired later
            .flightCommandPublisher = nullptr,                                          // ? will be wired later
            .missionStatusPublisher = nullptr,                                          // ? will be wired later
            .watchdogTimer = nullptr,                                                   // ? will be wired later
            .params = std::move(params)
        })
    );
    cogidrone::log(LOGGING_LEVEL::INFO, "Navigation object partially constructed"sv);

    // Wire the node
    n->wire();
    cogidrone::log(LOGGING_LEVEL::INFO, "Navigation object wired successfully"sv);

    // | DEBUG: Publish status
    n->publishStatus();                                                                 // ! NOTE: Unsure if this works

    // * Return the fully constructed Navigation object
    return n;
}

void Navigation::wire() noexcept {
    // * Subscriptions
    // ! DO NOT pass an explicit callback group to ANY create_* call in this class.
    // ! The node's default group is MutuallyExclusive, and every callback that
    // ! omits the group joins it. That is what serializes `onDetections()` and
    // ! `onWatchdogTick()` - see the invariant comment in navigation.h.
    // ! Passing a ReentrantCallbackGroup here silently removes the guarantee.
    m.detectionSubscription = m.node->create_subscription<vision_msgs::msg::Detection2DArray>(
        "/perception/detections",
        rclcpp::QoS(10),
        [this](const vision_msgs::msg::Detection2DArray& msg) {
            this->onDetections(msg);
        }
    );


    // * Manual override
    // ? `Bool` is a LEVEL: true = pilot has control, false = Navigation has
    // ? control. The publisher is the RC bridge (not yet implemented) that
    // ? mirrors the pilot's mode switch onto ROS.
    // ? QoS: volatile + depth 1 is enough IF the bridge publishes periodically.
    // ? If the bridge latches the switch instead, both sides must use
    // ? transient_local so a late-joining Navigation learns the current state.
    // ! No explicit callback group - same invariant as the other callbacks.
    // ? The override callback must only post a request - e.g. an
    // ? `std::atomic_bool overrideRequested` or an atomic enum - and the control
    // ? step (`onDetections` / `onWatchdogTick`) consumes it. Reasons:
    // ?   1. If the callback is in the default group, it is serialized, so it
    // ?      COULD mutate state directly - but it would become a second control
    // ?      step that may transition state without a fresh frame, or run
    // ?      `executeBehavior()` without updating `person`. That breaks the
    // ?      "one control step per frame" invariant.
    // ?   2. If the callback is ever moved to a Reentrant group (e.g. for
    // ?      responsiveness), direct mutation is a data race. The flag approach
    // ?      stays correct in both configurations.
    // ?   3. A direct Drone-level call runs on whatever thread Drone calls from -
    // ?      not an executor thread at all. Atomic is mandatory there.
    m.manualOverrideSubscription = m.node->create_subscription<std_msgs::msg::Bool>(
        "/navigation/manual_override",
        rclcpp::QoS(1).transient_local(),                                               // ? Latch the last state for late-joining Navigation
        [this](const std_msgs::msg::Bool& msg) {
            this->onManualOverride(msg);
        }
    );

    // TODO: Drone state (position/velocity/orientation) - the source is undecided.
    //     : Estimation is scrapped; candidates are subscribing to
    //     : /fmu/out/vehicle_local_position directly, or Pixhawk republishing its
    //     : lastLocalPos on a navigation-owned topic. Until decided, Navigation
    //     : has NO drone state and only issues absolute home-relative setpoints.

    // * Publishers
    m.setpointPublisher = m.node->create_publisher<px4_msgs::msg::TrajectorySetpoint>(
        "/navigation/setpoint",
        rclcpp::QoS(10)
    );

    m.flightCommandPublisher = m.node->create_publisher<std_msgs::msg::UInt8>(
        "/navigation/flight_command",
        rclcpp::QoS(10)
    );

    m.missionStatusPublisher = m.node->create_publisher<std_msgs::msg::String>(
        "/navigation/mission_status",
        rclcpp::QoS(10)
    );

    // * Watchdog timer
    // ? Not a control clock - the detection callback drives control. This timer
    // ? exists for one job: detect that NO frames are arriving, so the crash
    // ? protocol runs if Perception dies.
    // ! Same rule as the subscription above: no explicit callback group.
    const auto WATCHDOG_PERIOD = std::chrono::milliseconds(
        static_cast<int>(m.params.detectionTimeout * 1000.0f / 4.0f)                    // 4 checks per timeout window
    );
    m.watchdogTimer = m.node->create_wall_timer(
        WATCHDOG_PERIOD,
        [this]() { this->onWatchdogTick(); }
    );


    // * Timestamps
    stateEntryTime = m.node->now();
    lastHeartbeatTime = m.node->now();                                                  // ? give Perception detectionTimeout to send the first frame
    // ? `person.timestamp` is deliberately NOT initialized: time 0 means
    // ? "no detection has ever arrived", which the watchdog treats as stale.
}


// * Callbacks
// ? No lock: the executor serializes this callback with onWatchdogTick() and
// ? onManualOverride() via the node's default MutuallyExclusive group.
// ? If controlMutex is reinstated, uncomment the lock_guard below.
void Navigation::onDetections(const vision_msgs::msg::Detection2DArray& msg) {
    // | std::lock_guard lock(controlMutex);

    // ? Apply any pending override FIRST - it must take effect even if the rest
    // ? of the control step is skipped.
    consumeManualOverride();

    // ? Heartbeat: Perception is alive, regardless of whether a person was found.
    lastHeartbeatTime = m.node->now();

    // * Pick the most confident detection in the array
    // ! [4.3]: `onDetections()` ignores `hypothesis.class_id`
    // !    It picks the highest score regardless of class. If Perception ever publishes
    // !    heads and persons in the same array, Navigation may follow a head. Either:
    // !      * filter by `hypothesis.class_id` (requires a shared class-id convention), or
    // !      * make Perception publish only person detections on `/perception/detections`.
    // ! 
    // !    This is a cross-subsystem contract - decide it explicitly rather than relying 
    // !    on "Perception only sends persons".S
    const vision_msgs::msg::Detection2D* best = nullptr;
    for (const auto& detection : msg.detections) {
        if (detection.results.empty()) continue;                                        // ? Malformed entry
        if (!best || detection.results.front().hypothesis.score
                   > best->results.front().hypothesis.score) {
            best = &detection;
        }
    }

    // * Policy A: an empty array means "no person in this frame"
    if (!best) {
        person.valid = false;
    } else {
        person.confidence = best->results.front().hypothesis.score;
        person.timestamp = m.node->now();
        person.valid = true;

        // TODO: person.position = ... (world-frame, from fused detections)
    }
    
    // ? While overridden, keep `person` fresh (so the mission resumes with
    // ? current data) but do not run the state machine or behaviors.
    if (manualOverrideActive) return;
    
    // * Drive the state machine and behavior on every frame
    if (auto next = evaluateTransitions()) {
        transitionTo(*next);
        publishStatus();
    }

    executeBehavior();
}

// ? No lock: same reason as onDetections().
// ? If controlMutex is reinstated, uncomment the lock_guard below.
void Navigation::onWatchdogTick() {
    // | std::lock_guard lock(controlMutex);

    // ? Consume the override request even when Perception is silent: otherwise a
    // ? pilot who engages override while Perception is dead could never release
    // ? it (no frames -> no consumption).
    consumeManualOverride();

    // ? While overridden, Perception silence is not our problem - the pilot is
    // ? flying. Do not run the crash protocol.
    if (manualOverrideActive) return;

    // ? The detection callback normally drives the state machine. This timer
    // ? only acts when NO frames arrive at all - i.e. Perception is dead, not
    // ? just "person not in frame".
    if (!perceptionIsDead()) return;

    onPerceptionCrashed();
}

void Navigation::onManualOverride(const std_msgs::msg::Bool& msg) {
    manualOverrideRequested.store(msg.data, std::memory_order_relaxed);
}


// * State machine
std::optional<MissionState> Navigation::evaluateTransitions() {
    // ? No lock: called only from onDetections(), which is serialized by the
    // ? executor. `person`/`person.timestamp` are read here without a mutex for
    // ? the same reason. If controlMutex is reinstated, the caller must hold it.

    // ? Manual override freezes the mission: the pilot is flying, so no
    // ? transition should fire. Timeouts are meaningless while we are not the
    // ? pilot.
    if (manualOverrideActive) return std::nullopt;

    switch (state) {
        case MissionState::IDLE:
            // TODO: mission start trigger - tied to the manual override design
            //     : decision. Until then Navigation stays in IDLE (hovering).
            break;

        case MissionState::SEARCHING:
            if (detectionIsValid()) 
                return MissionState::PERSON_DETECTED;
                
            // TODO: add lawnmower search pattern (or other) to avoid staying in one place forever
            break;

        case MissionState::PERSON_DETECTED:
            // ? Require stable tracking for trackConfirmTime before committing to follow
            if ((m.node->now() - stateEntryTime).seconds() > m.params.trackConfirmTime) {
                return detectionIsValid() ? MissionState::FOLLOWING
                                          : MissionState::SEARCHING;
            }
            break;

        case MissionState::FOLLOWING:
            if (personIsLost()) 
                return MissionState::PERSON_LOST;
            // TODO: OBSTACLE_AVOIDANCE transition - blocked on Representation
            break;

        case MissionState::PERSON_LOST:
            if (detectionIsValid()) 
                return MissionState::PERSON_DETECTED;
            // TODO: timeout -> RETURNING (return-home strategy is a design choice)
            break;

        default:
            break;                                                                      // LANDING/EMERGENCY are exited via explicit commands only
    }

    return std::nullopt;
}

void Navigation::transitionTo(MissionState next) {
    RCLCPP_INFO(m.node->get_logger(), "State transition: %s -> %s",
        toString(state).c_str(), toString(next).c_str());

    state = next;
    stateEntryTime = m.node->now();

    onEnter(next);                                                                      // entry actions live here
}

void Navigation::onEnter(MissionState entered) {
    // ? Edge-triggered outputs: exactly once per transition, never per tick.
    switch (entered) {
        case MissionState::LANDING:
            publishFlightCommand(cogidrone::FlightCommand::LAND);
            break;
        default:
            break;                                                                      // No entry action
    }
}


void Navigation::onPerceptionCrashed() {
    // ! Caller must hold controlMutex if mutex is reinstated - do NOT lock here!
    // ? Called only from onWatchdogTick(), which is serialized by the executor.
    
    // ? Protocol: Perception is gone. We cannot see the person and cannot know
    // ? whether they are still there. Stop following and hold.
    if (state == MissionState::PERSON_LOST) return;                                     // Already handled

    RCLCPP_WARN(m.node->get_logger(),
                "Perception silent for %.1f s - entering PERSON_LOST",
                m.params.detectionTimeout);

    transitionTo(MissionState::PERSON_LOST);
    publishStatus();

    // ? Deliberately NO setpoint publish: Pixhawk re-streams the last one, so
    // ? the drone holds its last commanded position. Calling hoverBehavior()
    // ? here would command the ORIGIN (see its comment), which is not "hold".
    // 
    // TODO: If PERSON_LOST persists for N seconds, publish FlightCommand::LAND.
    //     : That is the decisive safe action once you decide the timeout.
}


void Navigation::consumeManualOverride() {
    // ? Control step only. Level comparison, NOT exchange(false): the override
    // ? is a state ("pilot has control"), not an event. exchange() would lose
    // ? repeated identical values.
    const bool REQUESTED = manualOverrideRequested.load(std::memory_order_relaxed);
    if (REQUESTED == manualOverrideActive) return;

    manualOverrideActive = REQUESTED;

    if (manualOverrideActive) {
        RCLCPP_WARN(m.node->get_logger(),
                    "Manual override ENGAGED - suspending mission (state: %s)",
                    toString(state).c_str());
    } else {
        // ? Restart the dwell timer: evidence gathered before the override is
        // ? stale. Without this, a long override would instantly satisfy
        // ? trackConfirmTime on the first frame after release.
        // ? NOTE: `person.timestamp` is deliberately NOT reset. If the person
        // ? was absent during the override, personIsLost() is immediately true
        // ? and FOLLOWING -> PERSON_LOST fires on the first frame. That is
        // ? correct since: the person really is lost.
        stateEntryTime = m.node->now();
        RCLCPP_INFO(m.node->get_logger(),
                    "Manual override RELEASED - resuming mission (state: %s)",
                    toString(state).c_str());
    }
}

// TODO: REFACTOR SUCH THAT WE CAN REMOVE THIS
std::string Navigation::toString(MissionState state) noexcept {
    switch (state) {
        case MissionState::IDLE:                return "IDLE";
        case MissionState::SEARCHING:           return "SEARCHING";
        case MissionState::PERSON_DETECTED:     return "PERSON_DETECTED";
        case MissionState::FOLLOWING:           return "FOLLOWING";
        case MissionState::PERSON_LOST:         return "PERSON_LOST";
        case MissionState::OBSTACLE_AVOIDANCE:  return "OBSTACLE_AVOIDANCE";
        case MissionState::RETURNING:           return "RETURNING";
        case MissionState::LANDING:             return "LANDING";
        case MissionState::EMERGENCY:           return "EMERGENCY";
        default:                                return "UNKNOWN";
    }
}


// * Behaviors
void Navigation::executeBehavior() {
    // ! Caller must hold controlMutex if mutex is reinstated - do NOT lock here!
    // ? No lock: called only from onDetections(), which is serialized by the executor. 

    // ? Manual override: publish NOTHING. PX4 is not in offboard mode and would
    // ? ignore us, but publishing would still overwrite Pixhawk's stored
    // ? `currentSetpoint` - so on release the drone would jump to a stale
    // ? mission target. Silence is correct.
    if (manualOverrideActive) return;

    switch (state) {
        case MissionState::IDLE:
        case MissionState::SEARCHING:
        case MissionState::PERSON_DETECTED:
        case MissionState::OBSTACLE_AVOIDANCE:                                          // TODO: blocked on Representation
        case MissionState::RETURNING:                                                   // TODO: blocked on drone state source
        case MissionState::EMERGENCY:
            hoverBehavior();
            break;

        case MissionState::PERSON_LOST:
            // ? Hold: do NOT publish a new setpoint. Pixhawk re-streams the last
            // ? one, so the drone holds its last commanded position. True "hold
            // ? current position" needs a drone state source - see wire() TODO.
            break;

        case MissionState::FOLLOWING:
            // TODO: followBehavior() - blocked on fused detections + drone state.
            hoverBehavior();
            break;

        case MissionState::LANDING:
            // ? LANDING is owned by Pixhawk. No new commands here.
            break;
    }
}

void Navigation::hoverBehavior() {
    // ? Hold a fixed home-relative hover setpoint.
    // ? LIMITATION: without a drone state source we cannot "hold current position" - 
    // ?           : we command the origin at targetHeight and rely on PX4's position
    // ?           : controller to fly there and stay. Revisit once the drone state 
    // ?           : source is decided.
    publishSetpoint(cogidrone::Vec3{0.0f, 0.0f, -m.params.targetHeight}, 0.0f);         // NED: negative z = up
}

//// void Navigation::followBehavior() {
////     if (!detectionIsValid()) return;
//// 
////     // * Target: a point behind the person at the follow distance
////     const Vec3 target = followTarget();
//// 
////     // * Velocity towards the target (P-controller, clamped)
////     Vec3 velocity = velocityTowards(target);
//// 
////     // * Yaw to face the person
////     const float yawError = desiredYawTowards(m.personWorldPosition) - m.drone.yaw;
////     float yawRate = m.params.yawPGain * normalizeAngle(yawError);
//// 
////     // * Safety
////     if (!commandIsSafe(velocity)) {
////         velocity = Vec3{};
////         yawRate = 0.0f;
////     }
//// 
////     publishVelocity(velocity, yawRate);
//// }
//// 
//// void Navigation::searchBehavior() {
////     // ? Simple search: hover and rotate slowly
////     publishVelocity(Vec3{}, 0.3f);
//// }



// * Navigation math
//// Vec3 Navigation::followTarget() const {
////     // * Direction of motion - fall back to the current drone-to-person line if the person is stationary
////     Vec3 personDirection = m.personWorldVelocity.normalized();
////     if (m.personWorldVelocity.norm() < 0.1f) {
////         personDirection = (m.drone.position - m.personWorldPosition).normalized();
////     }
//// 
////     // * A point behind the person at the follow distance, at the target height
////     Vec3 target = m.personWorldPosition - personDirection * m.params.targetFollowDistance;
////     target.z = m.params.targetHeight;
////     return target;
//// }
//// 
//// Vec3 Navigation::velocityTowards(const Vec3& target) const {
////     // * P-controller
////     Vec3 velocity = (target - m.drone.position) * m.params.positionPGain;
//// 
////     // * Clamp to max follow speed
////     const float speed = velocity.norm();
////     if (speed > m.params.maxFollowSpeed) {
////         velocity = velocity * (m.params.maxFollowSpeed / speed);
////     }
////     return velocity;
//// }
//// 
//// float Navigation::desiredYawTowards(const Vec3& target) const {
////     const Vec3 toTarget = target - m.drone.position;
////     return std::atan2(toTarget.y, toTarget.x);
//// }
//// 
//// Vec3 Navigation::cameraToWorld(const Vec3& camera) const {
////     // TODO: Apply the static camera mount extrinsic (camera -> body) once the rig is fixed;
////     // for now the camera is assumed coincident with the body frame
////     const Vec3 body = camera;
//// 
////     // * Body -> world: rotate by the drone orientation, then translate by the drone position
////     return m.drone.position + rotate(m.drone.orientation, body);
//// }


// * Publishing
void Navigation::publishSetpoint(const cogidrone::Vec3& positionNed, float yaw) {
    auto sp = std::make_unique<px4_msgs::msg::TrajectorySetpoint>();                    // ? unique_ptr publish: zero-copy intra-process
    sp->position = {positionNed.x, positionNed.y, positionNed.z};
    sp->yaw = yaw;
    sp->timestamp = m.node->now().nanoseconds() / 1000;                                 // µs, stamped at publish: always fresh for Pixhawk's stream

    m.setpointPublisher->publish(std::move(sp));
}

void Navigation::publishFlightCommand(cogidrone::FlightCommand command) {
    auto msg = std::make_unique<std_msgs::msg::UInt8>();
    msg->data = static_cast<uint8_t>(command);
    m.flightCommandPublisher->publish(std::move(msg));
}

void Navigation::publishStatus() {
    auto msg = std::make_unique<std_msgs::msg::String>();                               // ? unique_ptr publish: zero-copy intra-process
    msg->data = manualOverrideActive ? "MANUAL_OVERRIDE" : toString(state);
    m.missionStatusPublisher->publish(std::move(msg));
}


// * Helpers
bool Navigation::detectionIsValid() const {
    // ! Caller must hold controlMutex if mutex is reinstated - do NOT lock here!
    // ? No lock: caller (onDetections/onWatchdogTick) is serialized by the executor. 

    if (!person.valid) return false;
    if (person.confidence < m.params.minDetectionConfidence) return false;

    const double sinceDetection = (m.node->now() - person.timestamp).seconds();
    if (sinceDetection > m.params.detectionTimeout) return false;

    // TODO: Distance bounds check - requires world position + drone state.
    return true;
}

bool Navigation::personIsLost() const {
    // ! Caller must hold controlMutex if mutex is reinstated - do NOT lock here!
    // ? No lock: caller (onDetections/onWatchdogTick) is serialized by the executor. 

    // ? Deliberate hysteresis: "lost" means "no detection for detectionTimeout",
    // ? NOT "the latest frame was empty". A single dropped detection should not
    // ? abort FOLLOWING.
    return (m.node->now() - person.timestamp).seconds() > m.params.detectionTimeout;
}

bool Navigation::perceptionIsDead() const {
    // ! Caller must hold controlMutex if mutex is reinstated - do NOT lock here!
    // ? No lock: caller (onDetections/onWatchdogTick) is serialized by the executor. 
    
    // ? `lastHeartbeatTime` is updated on EVERY frame (including empty ones), so
    // ? this distinguishes "Perception stopped talking" from "person not in frame".
    return (m.node->now() - lastHeartbeatTime).seconds() > m.params.detectionTimeout;
}