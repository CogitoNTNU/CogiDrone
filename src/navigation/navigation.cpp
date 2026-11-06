#include "navigation.h"

#include <chrono>
#include <string>
#include <cmath>



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

    cogidrone::log(LOGGING_LEVEL::INFO, "Navigation parameters initialized successfully"sv);

    // * Partial assembly
    auto n = std::unique_ptr<Navigation>(
        new Navigation(M{                                                               // ? Transfer ownership into unique_ptr as ctor is private
            .node = std::move(node),
            .detectionSubscription = nullptr,                                           // ? will be wired later
            .offboardActiveSubscription = nullptr,                                      // ? will be wired later
            .armedSubscription = nullptr,                                               // ? will be wired later
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
    n->publishStatus();                                                                 // ! Works only if `missionStatusPublisher` is late-joining,
                                                                                        // ! transient_local; otherwise it silently fails.
    
    // * Return the fully constructed Navigation object
    return n;
}

void Navigation::wire() noexcept {
    // * Subscriptions
    // ! DO NOT pass an explicit callback group to ANY create_* call in this class.
    // ! The node's default group is MutuallyExclusive, and every callback that
    // ! omits the group joins it. That is what serializes onDetections(),
    // ! onWatchdogTick(), onArmed(), and onOffboardActive() - see the invariant
    // ! comment in navigation.h.
    // ! Passing a ReentrantCallbackGroup here silently removes the guarantee.
    m.detectionSubscription = m.node->create_subscription<vision_msgs::msg::Detection2DArray>(
        "/perception/detections",
        rclcpp::QoS(10),
        [this](const vision_msgs::msg::Detection2DArray& msg) {
            this->onDetections(msg);
        }
    );


    // * Control inputs from FlightController
    // ? `Bool` is a LEVEL, not an event: true = the condition holds right now.
    // ? Both topics are published by FlightController from vehicle_status, so
    // ? they are authoritative - they cover the RC switch, QGC mode changes,
    // ? and failsafes alike.
    // ! QoS CONTRACT: these subscribers use TRANSIENT_LOCAL so a late-joining
    // ! Navigation learns the current state immediately. FlightController MUST
    // ! publish with `rclcpp::QoS(1).transient_local()` too - a VOLATILE
    // ! publisher will NOT match these subscriptions (DDS durability is
    // ! requested <= offered), and the state would silently never arrive.
    // ! TRANSIENT_LOCAL also requires RELIABLE reliability (rclcpp::QoS default).
    // !
    // ! No explicit callback group - same invariant as the other callbacks.
    m.offboardActiveSubscription = m.node->create_subscription<std_msgs::msg::Bool>(
        "/pixhawk/offboard_active",
        rclcpp::QoS(1).transient_local(),                                               // ? Latch the last state for late-joining Navigation
        [this](const std_msgs::msg::Bool& msg) { 
            this->onOffboardActive(msg); 
        }
    );

    m.armedSubscription = m.node->create_subscription<std_msgs::msg::Bool>(
        "/pixhawk/armed",
        rclcpp::QoS(1).transient_local(),                                               // ? Latch the last state for late-joining Navigation
        [this](const std_msgs::msg::Bool& msg) { 
            this->onArmed(msg); 
        }
    );

    // ! DOUBLE CHECK AFTER REFACTOR: (might be better to have FlightController 
    // ! publish a reference or pointer of it's localPosition member instead 
    // ! of having Navigation subscribe to PX4's local position directly)
    // ? Read straight from PX4. `best_effort` is mandatory: PX4 publishes this
    // ? topic as best_effort, and a reliable subscriber would never match.
    // ! No explicit callback group - same invariant as the other callbacks.
    m.localPositionSubscription = m.node->create_subscription<px4_msgs::msg::VehicleLocalPosition>(
        "/fmu/out/vehicle_local_position",
        rclcpp::QoS(10).best_effort(),
        [this](const px4_msgs::msg::VehicleLocalPosition& msg) {
            this->onLocalPosition(msg);
        }
    );

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
        rclcpp::QoS(1).transient_local()                                                // ? Latch the last status so the debug line in `create()`
    );                                                                                  // ? works even if the publisher is late-joining

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
// ? No lock: the executor serializes this callback with onWatchdogTick(),
// ? onArmed(), and onOffboardActive() via the node's default MutuallyExclusive
// ? group. If controlMutex is reinstated, uncomment the lock_guard below.
void Navigation::onDetections(const vision_msgs::msg::Detection2DArray& msg) {
    // | std::lock_guard lock(controlMutex);

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
    // !    on "Perception only sends persons".
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
    
    // ? Only run the mission when we are the pilot. In every other control
    // ? state we keep `person` fresh (above) so the mission resumes with
    // ? current data, but we do not transition or publish setpoints.
    if (controlState != ControlState::NAVIGATION_HAS_CONTROL) return;
    
    // * Drive the state machine and behavior on every frame
    if (auto next = evaluateTransitions()) {
        transitionTo(*next);
        publishStatus();
    }

    executeBehavior();
}

// ? No lock: same reason as onDetections().
// ? If controlMutex is reinstated, uncomment the lock_guard below.
void Navigation::onArmed(const std_msgs::msg::Bool& msg) {
    // | std::lock_guard lock(controlMutex);

    if (msg.data == armed) return;                                                      // ? Level, not event: ignore repeats
    armed = msg.data;
    updateControlState();
}

// ? No lock: same reason as onDetections().
// ? If controlMutex is reinstated, uncomment the lock_guard below.
void Navigation::onOffboardActive(const std_msgs::msg::Bool& msg) {
    // | std::lock_guard lock(controlMutex);

    if (msg.data == offboardActive) return;                                             // ? Level, not event: ignore repeats
    offboardActive = msg.data;
    updateControlState();
}

// ! DOUBLE CHECK AFTER REFACTOR: (might be better to have FlightController 
// ! publish a reference or pointer of it's localPosition member instead 
// ! of having Navigation subscribe to PX4's local position directly)
// ? No lock: same reason as onDetections().
// ? If controlMutex is reinstated, uncomment the lock_guard below.
void Navigation::onLocalPosition(const px4_msgs::msg::VehicleLocalPosition& msg) {
    // | std::lock_guard lock(controlMutex);

    dronePosition = msg;
}

// ? No lock: same reason as onDetections().
// ? If controlMutex is reinstated, uncomment the lock_guard below.
void Navigation::onWatchdogTick() {
    // | std::lock_guard lock(controlMutex);

    // ? No control: Perception silence is not our problem.
    if (controlState != ControlState::NAVIGATION_HAS_CONTROL) return;

    // ? The detection callback normally drives the state machine. This timer
    // ? only acts when NO frames arrive at all - i.e. Perception is dead, not
    // ? just "person not in frame".
    if (!perceptionIsDead()) return;

    onPerceptionCrashed();
}


// * Control state
ControlState Navigation::deriveControlState() const noexcept {
    // ? Pure function of the two raw inputs. No side effects, no logging.
    if (!armed)         return ControlState::WAITING_FOR_ARM;
    if (!offboardActive) return ControlState::PILOT_HAS_CONTROL;
    return ControlState::NAVIGATION_HAS_CONTROL;
}

void Navigation::updateControlState() {
    // ? Called from onArmed() and onOffboardActive(). The executor serializes
    // ? them, so no lock.
    const ControlState next = deriveControlState();
    if (next == controlState) return;                                                   // ? Nothing changed

    const ControlState previous = controlState;
    controlState = next;

    RCLCPP_INFO(m.node->get_logger(), "Control state: %s -> %s",
        toString(previous).c_str(), toString(next).c_str());

    // ? Dispatch to the handler for the NEW state. Each handler is responsible
    // ? for exactly one transition, so the policy is visible at the call site
    // ? instead of buried in an if-chain.
    switch (next) {
        case ControlState::WAITING_FOR_ARM:        onWaitingForArm();        break;
        case ControlState::PILOT_HAS_CONTROL:      onPilotTookControl();     break;
        case ControlState::NAVIGATION_HAS_CONTROL: onNavigationTookControl(); break;
    }

    publishStatus();
}

void Navigation::onWaitingForArm() {
    // ? Disarmed: the mission is over (or never started). Return to IDLE so
    // ? the next arm starts a fresh mission from SEARCHING.
    if (state != MissionState::IDLE) {
        RCLCPP_INFO(m.node->get_logger(), "Disarmed - mission over, returning to IDLE");
        transitionTo(MissionState::IDLE);
    }
}

void Navigation::onPilotTookControl() {
    // ? Pilot is flying. Freeze the mission: keep `state` so we resume where
    // ? we left off when control returns.
    // ? Restart the dwell timer: evidence gathered before the override is
    // ? stale. Without this, a long override would instantly satisfy
    // ? trackConfirmTime on the first frame after release.
    // ? NOTE: `person.timestamp` is deliberately NOT reset. If the person
    // ? was absent during the override, personIsLost() is immediately true
    // ? and FOLLOWING -> PERSON_LOST fires on the first frame. That is
    // ? correct since: the person really is lost.
    stateEntryTime = m.node->now();

    if (state == MissionState::IDLE) {
        RCLCPP_INFO(m.node->get_logger(), "Pilot has control - mission not started");
    } 
    
    else {
        RCLCPP_WARN(m.node->get_logger(), "Pilot has control - mission suspended (state: %s)",
            toString(state).c_str());
    }
}

void Navigation::onNavigationTookControl() {
    // ? We are flying. If the mission has not started, start it. Otherwise
    // ? resume from the preserved state.
    // ? Restart the dwell timer for the same reason as onPilotTookControl().
    stateEntryTime = m.node->now();

    if (state == MissionState::IDLE) {
        RCLCPP_INFO(m.node->get_logger(), "Armed + offboard - mission starting");
        transitionTo(MissionState::SEARCHING);
    } else {
        RCLCPP_INFO(m.node->get_logger(), "Navigation has control - resuming mission (state: %s)",
            toString(state).c_str());
    }
}


// * State machine
std::optional<MissionState> Navigation::evaluateTransitions() {
    // ? No lock: called only from onDetections(), which is serialized by the
    // ? executor. `person`/`person.timestamp` are read here without a mutex for
    // ? the same reason. If controlMutex is reinstated, the caller must hold it.

    switch (state) {
        case MissionState::IDLE:
            // ? Unreachable: this function is only called from onDetections()
            // ? after the NAVIGATION_HAS_CONTROL gate, and IDLE only exists
            // ? when we do NOT have control. The IDLE -> SEARCHING transition
            // ? happens in onNavigationTookControl().
            // ! If this fires, the control-state gate is broken.
            RCLCPP_ERROR(m.node->get_logger(),
                "evaluateTransitions() called in IDLE - control-state gate is broken");
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

        case MissionState::LANDING:
            break;                                                                      // Terminal until FlightController reports disarmed
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

    // ? Deliberately NO setpoint publish: FlightController re-streams the last one, so
    // ? the drone holds its last commanded position. Calling hoverBehavior()
    // ? here would command the ORIGIN (see its comment), which is not "hold".
    // 
    // TODO: If PERSON_LOST persists for N seconds, publish FlightCommand::LAND.
    //     : That is the decisive safe action once you decide the timeout.
}



// * Behaviors
void Navigation::executeBehavior() {
    // ! Caller must hold controlMutex if mutex is reinstated - do NOT lock here!
    // ? No lock: called only from onDetections(), which is serialized by the executor. 

    switch (state) {
        case MissionState::IDLE:
            // ? Unreachable for the same reason as in evaluateTransitions().
            // ! If this fires, the control-state gate is broken.
            RCLCPP_ERROR(m.node->get_logger(),
                "executeBehavior() called in IDLE - control-state gate is broken");
            break;

        case MissionState::SEARCHING:
        case MissionState::PERSON_DETECTED:
        // | case MissionState::OBSTACLE_AVOIDANCE:                                          // TODO: blocked on Representation
        // | case MissionState::RETURNING:                                                   // TODO: blocked on drone state source
        // | case MissionState::EMERGENCY:
            hoverBehavior();
            break;

        case MissionState::PERSON_LOST:
            // ? Hold: do NOT publish a new setpoint. FlightController re-streams the last
            // ? one, so the drone holds its last commanded position. True "hold
            // ? current position" needs a drone state source - see wire() TODO.
            break;

        case MissionState::FOLLOWING:
            // TODO: followBehavior() - blocked on fused detections + drone state.
            hoverBehavior();
            break;

        case MissionState::LANDING:
            // ? LANDING is owned by FlightController. No new commands here.
            break;
    }
}

// ! DOUBLE CHECK AFTER REFACTOR: (might be better to have FlightController 
// ! publish a reference or pointer of it's localPosition member instead 
// ! of having Navigation subscribe to PX4's local position directly)
void Navigation::hoverBehavior() {
    // ? Hold the CURRENT horizontal position at the target height. This is the
    // ? only behavior that is safe to run with no person in frame: it never
    // ? moves the drone laterally, so a stale or missing detection cannot cause
    // ? a transit.
    // ! Without a valid EKF position we do not know where "here" is. Publishing
    // ! anything would command an absolute point in a frame we do not trust, so
    // ! we publish nothing and let FlightController re-stream its frozen
    // ! setpoint - which is "hold where you are".
    if (!dronePositionValid()) return;

    const cogidrone::Vec3 HOLD_POSITION_NED = {
        dronePosition.x,
        dronePosition.y,
        -m.params.targetHeight                                                          // NED: negative z = up
    };

    // ? Preserve the current heading. `VehicleLocalPosition.heading` is NAN when
    // ? unknown, hence the guard - a NAN yaw would be rejected by PX4.
    const float YAW = std::isfinite(dronePosition.heading) ? dronePosition.heading : 0.0f;

    publishSetpoint(HOLD_POSITION_NED, YAW);
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
    sp->timestamp = m.node->now().nanoseconds() / 1000;                                 // µs, stamped at publish: always fresh for FlightController's stream

    m.setpointPublisher->publish(std::move(sp));
}

void Navigation::publishFlightCommand(cogidrone::FlightCommand command) {
    auto msg = std::make_unique<std_msgs::msg::UInt8>();
    msg->data = static_cast<uint8_t>(command);
    m.flightCommandPublisher->publish(std::move(msg));
}

void Navigation::publishStatus() {
    // ? The status string is a function of the control state, not the mission
    // ? state alone. Three cases, one per ControlState:
    // ?   WAITING_FOR_ARM        -> "IDLE"                (not flying yet)
    // ?   PILOT_HAS_CONTROL      -> "MANUALLY_OVERRIDDEN" (pilot is flying)
    // ?   NAVIGATION_HAS_CONTROL -> toString(state)       (we are flying)
    std::string status;
    switch (controlState) {
        case ControlState::WAITING_FOR_ARM:
            status = "IDLE";
            break;
        case ControlState::PILOT_HAS_CONTROL:
            status = "MANUALLY_OVERRIDDEN";
            break;
        case ControlState::NAVIGATION_HAS_CONTROL:
            status = toString(state);
            break;
    }

    auto msg = std::make_unique<std_msgs::msg::String>();                               // ? unique_ptr publish: zero-copy intra-process
    msg->data = std::move(status);
    m.missionStatusPublisher->publish(std::move(msg));
}


// * State queries
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

// ! DOUBLE CHECK AFTER REFACTOR: (might be better to have FlightController 
// ! publish a reference or pointer of it's localPosition member instead 
// ! of having Navigation subscribe to PX4's local position directly)
bool Navigation::dronePositionValid() const {
    // ! Caller must hold controlMutex if mutex is reinstated - do NOT lock here!
    // ? No lock: caller is serialized by the executor.

    // ? Same predicate FlightController uses before it will stream: EKF2 has a
    // ? position reference (GPS outdoors, VIO later). Without it, `x`/`y`/`z`
    // ? are meaningless and any setpoint built from them is a guess.
    return dronePosition.xy_valid && dronePosition.z_valid;
}


// * Helpers
std::string Navigation::toString(MissionState state) noexcept {
    switch (state) {
        case MissionState::IDLE:                return "IDLE";
        case MissionState::SEARCHING:           return "SEARCHING";
        case MissionState::PERSON_DETECTED:     return "PERSON_DETECTED";
        case MissionState::FOLLOWING:           return "FOLLOWING";
        case MissionState::PERSON_LOST:         return "PERSON_LOST";
        case MissionState::LANDING:             return "LANDING";
        default:                                return "UNKNOWN";
    }
}

std::string Navigation::toString(ControlState state) noexcept {
    switch (state) {
        case ControlState::WAITING_FOR_ARM:        return "WAITING_FOR_ARM";
        case ControlState::PILOT_HAS_CONTROL:      return "PILOT_HAS_CONTROL";
        case ControlState::NAVIGATION_HAS_CONTROL: return "NAVIGATION_HAS_CONTROL";
        default:                                   return "UNKNOWN";
    }
}