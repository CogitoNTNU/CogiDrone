#include "navigation.h"

#include <optional>
#include <string>

namespace {
constexpr float kPi = 3.14159265358979323846f;

// Normalize an angle to [-pi, pi]
float normalizeAngle(float a) noexcept {
    while (a > kPi)  a -= 2.0f * kPi;
    while (a < -kPi) a += 2.0f * kPi;
    return a;
}
} // namespace


// * Math
Vec3 Vec3::normalized() const noexcept {
    const float n = norm();
    return n > 0.0f ? Vec3{x / n, y / n, z / n} : Vec3{};
}

Vec3 rotate(const Quat& q, const Vec3& v) noexcept {
    // ? Rodrigues' rotation formula: v' = v + 2w(u x v) + 2(u x (u x v)), u = q.xyz
    const Vec3 u{q.x, q.y, q.z};
    const Vec3 t{
        2.0f * (u.y * v.z - u.z * v.y),
        2.0f * (u.z * v.x - u.x * v.z),
        2.0f * (u.x * v.y - u.y * v.x)
    };
    return v + t * q.w + Vec3{
        u.y * t.z - u.z * t.y,
        u.z * t.x - u.x * t.z,
        u.x * t.y - u.y * t.x
    };
}

float yawOf(const Quat& q) noexcept {
    return std::atan2(2.0f * (q.w * q.z + q.x * q.y),
                      1.0f - 2.0f * (q.y * q.y + q.z * q.z));
}


// * Ctors & dtor
Navigation::~Navigation() = default;

std::expected<std::unique_ptr<Navigation>, cogidrone::Error> Navigation::create() {
    // * ROS2
    auto node = std::make_shared<rclcpp::Node>(
        "navigation",
        rclcpp::NodeOptions().use_intra_process_comms(true)
    );
    cogidrone::log(LOGGING_LEVEL::INFO, "ROS2 IPC node created successfully");

    // * Parameters (non-failable: defaults live in NavigationParams)
    NavigationParams params;
    node->declare_parameter("target_follow_distance", params.targetFollowDistance);
    node->declare_parameter("target_height", params.targetHeight);
    node->declare_parameter("max_follow_speed", params.maxFollowSpeed);
    node->declare_parameter("min_follow_distance", params.minFollowDistance);
    node->declare_parameter("max_follow_distance", params.maxFollowDistance);
    node->declare_parameter("detection_timeout", params.detectionTimeout);
    node->declare_parameter("min_detection_confidence", params.minDetectionConfidence);
    node->declare_parameter("track_confirm_time", params.trackConfirmTime);
    node->declare_parameter("control_frequency", params.controlFrequency);
    node->get_parameter("target_follow_distance", params.targetFollowDistance);
    node->get_parameter("target_height", params.targetHeight);
    node->get_parameter("max_follow_speed", params.maxFollowSpeed);
    node->get_parameter("min_follow_distance", params.minFollowDistance);
    node->get_parameter("max_follow_distance", params.maxFollowDistance);
    node->get_parameter("detection_timeout", params.detectionTimeout);
    node->get_parameter("min_detection_confidence", params.minDetectionConfidence);
    node->get_parameter("track_confirm_time", params.trackConfirmTime);
    node->get_parameter("control_frequency", params.controlFrequency);
    cogidrone::log(LOGGING_LEVEL::INFO, "Navigation parameters initialized successfully");

    // * Partial assembly
    auto n = std::unique_ptr<Navigation>(
        new Navigation(M{                                                               // ? Transfer ownership into unique_ptr as ctor is private
            .node = std::move(node),
            .detectionSubscription = nullptr,                                           // ? will be wired later
            .setpointPublisher = nullptr,                                               // ? will be wired later
            .flightCommandPublisher = nullptr,                                          // ? will be wired later
            .missionStatusPublisher = nullptr,                                          // ? will be wired later
            .controlTimer = nullptr,                                                    // ? will be wired later
            .params = std::move(params)
        })
    );
    cogidrone::log(LOGGING_LEVEL::INFO, "Navigation object partially constructed");

    // Wire the node
    n->wire();
    cogidrone::log(LOGGING_LEVEL::INFO, "Navigation object wired successfully");

    // * Return the fully constructed Navigation object
    return n;
}

void Navigation::wire() noexcept {
    // * Subscriptions
    m.detectionSubscription = m.node->create_subscription<vision_msgs::msg::Detection2DArray>(
        "/perception/detections",
        rclcpp::QoS(10),
        [this](const vision_msgs::msg::Detection2DArray& msg) {
            this->onDetections(msg);
        }
    );

    // TODO: Manual override channel - the mechanism (string commands? joy topic?
    //     : direct Drone-level call?) is a design decision, not yet made.

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

    m.flightCommandPublisher = m.node->create_publisher<std_msgs::msg::String>(
        "/navigation/flight_command",
        rclcpp::QoS(10)
    );

    m.missionStatusPublisher = m.node->create_publisher<std_msgs::msg::String>(
        "/navigation/mission_status",
        rclcpp::QoS(10)
    );

    // * Control timer
    const auto period = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::duration<double>(1.0 / m.params.controlFrequency)
    );
    m.controlTimer = m.node->create_wall_timer(
        period,
        [this]() { this->onControlTick(); }
    );

    // * Timestamps
    stateEntryTime = m.node->now();
    lastDetectionTime = m.node->now();
}


// * Callbacks
void Navigation::onDetections(const vision_msgs::msg::Detection2DArray& msg) {
    // * Pick the most confident detection in the array
    const vision_msgs::msg::Detection2D* best = nullptr;
    for (const auto& detection : msg.detections) {
        if (detection.results.empty()) continue;                                        // ? malformed entry
        if (!best || detection.results.front().hypothesis.score
                   > best->results.front().hypothesis.score) {
            best = &detection;
        }
    }

    std::lock_guard lock(personMutex);

    if (!best) {                                                                        // ? no usable detection in this frame
        person.valid = false;
        return;
    }

    person.confidence = best->results.front().hypothesis.score;
    person.timestamp = m.node->now();
    person.valid = true;
    lastDetectionTime = m.node->now();

    // TODO: Extract the 3D camera-frame position from the fused detection. This
    //     : requires the depth estimate (DepthAnything) and camera intrinsics,
    //     : which Perception will provide once Model::FusedDetections is
    //     : finalized. Until then, person.position stays zero and is NOT used
    //     : by any behavior.
    // person.position = ...
}

void Navigation::onControlTick() {
    // * Evaluate the state machine, and transition if requested
    if (auto next = evaluateTransitions()) {
        transitionTo(*next);
    }

    // ? Behaviors that don't exist yet fall back to hover - the safe default.
    // ? Every state ends in exactly one publishSetpoint() call per tick.
    switch (state) {
        case MissionState::IDLE:
        case MissionState::SEARCHING:
        case MissionState::PERSON_DETECTED:
        case MissionState::PERSON_LOST:
        case MissionState::OBSTACLE_AVOIDANCE:                                          // TODO: behavior blocked on Representation
        case MissionState::RETURNING:                                                   // TODO: behavior blocked on drone state source
        case MissionState::EMERGENCY:
            hoverBehavior();
            break;

        case MissionState::FOLLOWING:
            // TODO: followBehavior() - blocked on fused detections (person world
            //     : position) and drone state. Hover until then.
            hoverBehavior();
            break;

        case MissionState::LANDING:
            // ? LANDING is owned by Pixhawk (its land() hands control to PX4's
            // ? landing controller). Navigation keeps streaming its last
            // ? setpoint - PX4 ignores it in auto-land. No new commands here.
            break;
    }

    // * Publish mission status
    publishStatus();
}


// * State machine
// TODO: I feel like we can do this without allocating a next-state, and then comparing. Instead
//     : we could just return early on a transition, and return nullopt if no transition is taken. 
[[nodiscard]] std::optional<MissionState> Navigation::evaluateTransitions() {
    // ? No lock: `state`/`stateEntryTime` are timer-thread-owned. The only
    // ? cross-thread data read here is `person`/`lastDetectionTime`, guarded
    // ? inside detectionIsValid()/personIsLost() by personMutex.
    MissionState next = state;

    switch (state) {
        case MissionState::IDLE:
            // TODO: mission start trigger - tied to the manual override design
            //     : decision. Until then Navigation stays in IDLE (hovering).
            break;

        case MissionState::SEARCHING:
            if (detectionIsValid()) 
                next = MissionState::PERSON_DETECTED;
            break;

        case MissionState::PERSON_DETECTED:
            // ? Require stable tracking for trackConfirmTime before committing to follow
            if ((m.node->now() - stateEntryTime).seconds() > m.params.trackConfirmTime) {
                next = detectionIsValid() ? MissionState::FOLLOWING
                                          : MissionState::SEARCHING;
            }
            break;

        case MissionState::FOLLOWING:
            if (personIsLost()) 
                next = MissionState::PERSON_LOST;
            // TODO: OBSTACLE_AVOIDANCE transition - blocked on Representation
            break;

        case MissionState::PERSON_LOST:
            if (detectionIsValid()) 
                next = MissionState::PERSON_DETECTED;
            // TODO: timeout -> RETURNING (return-home strategy is a design choice)
            break;

        default:
            break;                                                                      // LANDING/EMERGENCY are exited via explicit commands only
    }

    //// if (next != state) transitionTo(next);
    if (next == state) return std::nullopt;
    return next;
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
            publishFlightCommand("LAND");                                               // TODO: Use custom message enum type to ensure safety. See "src/controller/FLIGHT_COMMAND_MSG.md" for implementation details.
            break;
        default:
            break;                                                                      // no entry action
    }
}

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
void Navigation::hoverBehavior() {
    // ? Hold a fixed home-relative hover setpoint.
    // ? LIMITATION: without a drone state source we cannot "hold current
    //     : position" - we command the origin at targetHeight and rely on PX4's
    //     : position controller to fly there and stay. Revisit once the drone
    //     : state source is decided.
    publishSetpoint(Vec3{0.0f, 0.0f, -m.params.targetHeight}, 0.0f);                    // NED: negative z = up
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
void Navigation::publishSetpoint(const Vec3& positionNed, float yaw) {
    auto sp = std::make_unique<px4_msgs::msg::TrajectorySetpoint>();                    // ? unique_ptr publish: zero-copy intra-process
    sp->position = {positionNed.x, positionNed.y, positionNed.z};
    sp->yaw = yaw;
    sp->timestamp = m.node->now().nanoseconds() / 1000;                                  // µs, stamped at publish: always fresh for Pixhawk's stream

    m.setpointPublisher->publish(std::move(sp));
}

// navigation.cpp
void Navigation::publishFlightCommand(const std::string& command) {
    auto msg = std::make_unique<std_msgs::msg::String>();
    msg->data = command;
    m.flightCommandPublisher->publish(std::move(msg));
}

void Navigation::publishStatus() {
    auto msg = std::make_unique<std_msgs::msg::String>();                               // ? unique_ptr publish: zero-copy intra-process
    msg->data = toString(state);

    m.missionStatusPublisher->publish(std::move(msg));
}


// * Helpers
//// bool Navigation::commandIsSafe(const Vec3& velocity) const {
////     if (m.drone.position.z < m.params.minAltitude)  return false;                       // ? too low
////     if (m.drone.position.z > m.params.maxAltitude)  return false;                       // ? too high
////     if (velocity.norm() > m.params.maxFollowSpeed)  return false;                       // ? too fast
////     // TODO: Obstacle distance checks against the representation layer
////     return true;
//// }

bool Navigation::detectionIsValid() const {
    std::lock_guard lock(personMutex);

    if (!person.valid) return false;
    if (person.confidence < m.params.minDetectionConfidence) return false;

    const double sinceDetection = (m.node->now() - lastDetectionTime).seconds();
    if (sinceDetection > m.params.detectionTimeout) return false;

    // TODO: Distance bounds check (min/maxFollowDistance) requires the person's
    //     : world position, which requires drone state + fused detections.
    // TODO: Obstacle proximity check - blocked on Representation.
    //// const float distance = (m.personWorldPosition - m.drone.position).norm();
    //// if (distance < m.params.minFollowDistance || distance > m.params.maxFollowDistance) return false;

    return true;
}

bool Navigation::personIsLost() const {
    std::lock_guard lock(personMutex);
    return (m.node->now() - lastDetectionTime).seconds() > m.params.detectionTimeout;
}