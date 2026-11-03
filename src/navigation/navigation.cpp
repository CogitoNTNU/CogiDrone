#include "navigation.h"

#include <chrono>
#include <string>


// /* // | UNSURE IF BULLSHIT
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
// */

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
            .setpointPublisher = nullptr,                                               // ? will be wired later
            .flightCommandPublisher = nullptr,                                          // ? will be wired later
            .missionStatusPublisher = nullptr,                                          // ? will be wired later
            .controlTimer = nullptr,                                                    // ? will be wired later
            .params = std::move(params)
        })
    );
    cogidrone::log(LOGGING_LEVEL::INFO, "Navigation object partially constructed"sv);

    // Wire the node
    n->wire();
    cogidrone::log(LOGGING_LEVEL::INFO, "Navigation object wired successfully"sv);

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

    m.flightCommandPublisher = m.node->create_publisher<std_msgs::msg::UInt8>(
        "/navigation/flight_command",
        rclcpp::QoS(10)
    );

    m.missionStatusPublisher = m.node->create_publisher<std_msgs::msg::String>(
        "/navigation/mission_status",
        rclcpp::QoS(10)
    );

    // * Control timer
    // ? Navigation polls because it's a controller, and controllers are clocked by 
    // ? definition - it must publish setpoints when nothing changed, and it must
    // ? notice when nothing arrived. Perception reacts because it's a transformer 
    // ? - no input, no work. The timer isn't a busy-wait; it's the control clock, 
    // ? and the executor sleeps between ticks. Therefore, since the camera/detector 
    // ? runs at whatever rate it runs - 10-60 Hz, which is jittery and sometimes 
    // ? drops frames - the control can't depend on it. Control must run at a steady 
    // ? rate. Decoupling the two is the entire point of a control loop: we sample 
    // ? inputs asynchronously, then act on a fixed schedule. If Navigation ran on 
    // ? detection callbacks, its control rate would inherit the detector's jitter.
    const auto PERIOD = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::duration<double>(1.0 / m.params.controlFrequency)
    );
    m.controlTimer = m.node->create_wall_timer(
        PERIOD,
        [this]() { this->onControlTick(); }
    );

    // * Timestamps
    stateEntryTime = m.node->now();
    // ? `person.timestamp` is deliberately NOT initialized: time 0 means
    // ? "no detection has ever arrived", which the watchdog treats as stale.
    // ? Initializing it to now() would fake a detection for detectionTimeout.
}


// * Callbacks
// ! [4.3]: `onDetections()` ignores `hypothesis.class_id`
// !    It picks the highest score regardless of class. If Perception ever publishes
// !    heads and persons in the same array, Navigation may follow a head. Either:
// !      * filter by `hypothesis.class_id` (requires a shared class-id convention), or
// !      * make Perception publish only person detections on `/perception/detections`.
// ! 
// !    This is a cross-subsystem contract - decide it explicitly rather than relying 
// !    on "Perception only sends persons".S
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
        publishStatus();                                                                // | DEBUG: Update mission status
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
            // ! APPEARENTLY FALSE COMMENT:
            // ? LANDING is owned by Pixhawk (its land() hands control to PX4's
            // ? landing controller). Navigation keeps streaming its last
            // ? setpoint - PX4 ignores it in auto-land. No new commands here.
            break;
    }
}


// * State machine
std::optional<MissionState> Navigation::evaluateTransitions() {
    // ? No lock: `state`/`stateEntryTime` are timer-thread-owned. The only
    // ? cross-thread data read here is `person`/`person.timestamp`, guarded
    // ? inside detectionIsValid()/personIsLost() by personMutex.
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

// ! REMOVE ONCE WE IMPLEMENT OUR OWN MESSAGE TYPE
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

    const double sinceDetection = (m.node->now() - person.timestamp).seconds();
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

    // ? Deliberate hysteresis: "lost" means "no detection for detectionTimeout",
    // ? NOT "the latest frame was empty". A single dropped detection should not
    // ? abort FOLLOWING. `person.valid` is the immediate signal used by
    // ? detectionIsValid(); this is the coasting window.
    return (m.node->now() - person.timestamp).seconds() > m.params.detectionTimeout;
}