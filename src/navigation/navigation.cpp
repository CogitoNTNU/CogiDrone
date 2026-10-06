#include "navigation.h"


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
    // Node
    auto node = std::make_shared<rclcpp::Node>(
        "navigation",
        rclcpp::NodeOptions().use_intra_process_comms(true)
    );

    // * Parameters (non-failable: defaults live in NavigationParams)
    NavigationParams params;
    node->declare_parameter("target_follow_distance", params.targetFollowDistance);
    node->declare_parameter("target_height", params.targetHeight);
    node->declare_parameter("max_follow_speed", params.maxFollowSpeed);
    node->declare_parameter("control_frequency", params.controlFrequency);
    node->declare_parameter("detection_timeout", params.detectionTimeout);
    node->declare_parameter("min_detection_confidence", params.minDetectionConfidence);
    node->get_parameter("target_follow_distance", params.targetFollowDistance);
    node->get_parameter("target_height", params.targetHeight);
    node->get_parameter("max_follow_speed", params.maxFollowSpeed);
    node->get_parameter("control_frequency", params.controlFrequency);
    node->get_parameter("detection_timeout", params.detectionTimeout);
    node->get_parameter("min_detection_confidence", params.minDetectionConfidence);

    // * Partial assembly
    auto n = std::unique_ptr<Navigation>(
        new Navigation(M{                                                               // ? Transfer ownership into unique_ptr as ctor is private
            // ROS2
            .node = std::move(node),
            .detectionSubscription = nullptr,                                           // ? will be wired later
            .odometrySubscription = nullptr,                                            // ? will be wired later
            .manualOverrideSubscription = nullptr,                                      // ? will be wired later
            .velocityPublisher = nullptr,                                               // ? will be wired later
            .missionStatusPublisher = nullptr,                                          // ? will be wired later
            .controlTimer = nullptr,                                                    // ? will be wired later

            // State
            .params = std::move(params)
            // ? remaining members default-initialize; timestamps are set in wire()
        })
    );

    // Wire the node
    n->wire();

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

    m.odometrySubscription = m.node->create_subscription<nav_msgs::msg::Odometry>(
        "/estimation/vio_odometry",
        rclcpp::QoS(10),
        [this](const nav_msgs::msg::Odometry& msg) {
            this->onOdometry(msg);
        }
    );

    m.manualOverrideSubscription = m.node->create_subscription<std_msgs::msg::String>(
        "/manual_override",
        rclcpp::QoS(10),
        [this](const std_msgs::msg::String& msg) {
            this->onManualOverride(msg);
        }
    );

    // * Publishers
    m.velocityPublisher = m.node->create_publisher<geometry_msgs::msg::TwistStamped>(
        "/navigation/velocity_setpoint",
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
    m.lastDetectionTime = m.node->now();
    m.stateEntryTime = m.node->now();
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

    if (!best) {                                                                        // ? no usable detection in this frame
        m.person.valid = false;
        return;
    }

    m.person.confidence = best->results.front().hypothesis.score;
    m.person.timestamp = m.node->now();
    m.person.valid = true;

    // TODO: Extract the 3D camera-frame position from the fused detection. This requires
    // the depth estimate (DepthAnything) and the camera intrinsics, which perception will
    // provide once the fused detection message is finalized. Until then, position stays zero.
    // m.person.position = ...

    // * Transform to world frame
    m.personWorldPosition = cameraToWorld(m.person.position);
    m.lastDetectionTime = m.node->now();
}

void Navigation::onOdometry(const nav_msgs::msg::Odometry& msg) {
    // * Position
    m.drone.position = Vec3{
        static_cast<float>(msg.pose.pose.position.x),
        static_cast<float>(msg.pose.pose.position.y),
        static_cast<float>(msg.pose.pose.position.z)
    };

    // * Velocity
    m.drone.velocity = Vec3{
        static_cast<float>(msg.twist.twist.linear.x),
        static_cast<float>(msg.twist.twist.linear.y),
        static_cast<float>(msg.twist.twist.linear.z)
    };

    // * Orientation
    m.drone.orientation = Quat{
        static_cast<float>(msg.pose.pose.orientation.w),
        static_cast<float>(msg.pose.pose.orientation.x),
        static_cast<float>(msg.pose.pose.orientation.y),
        static_cast<float>(msg.pose.pose.orientation.z)
    };
    m.drone.yaw = yawOf(m.drone.orientation);
    m.drone.timestamp = m.node->now();

    // * Home position is wherever we first get a position fix
    if (!m.homePositionSet) {
        m.homePosition = m.drone.position;
        m.homePositionSet = true;
    }
}

void Navigation::onManualOverride(const std_msgs::msg::String& msg) {
    RCLCPP_WARN(m.node->get_logger(), "Manual override received: %s", msg.data.c_str());

    // ? String commands keep the override channel usable from a terminal (`ros2 topic pub`)
    if (msg.data == "EMERGENCY_STOP")      transitionTo(MissionState::EMERGENCY);
    else if (msg.data == "START_MISSION")  transitionTo(MissionState::SEARCHING);
    else if (msg.data == "RETURN_HOME")    transitionTo(MissionState::RETURNING);
    else if (msg.data == "LAND")           transitionTo(MissionState::LANDING);
}

void Navigation::onControlTick() {
    // * Update the state machine, then execute the behavior of the current state
    updateStateMachine();

    switch (m.state) {
        case MissionState::IDLE:
            break;                                                                      // Waiting for mission start

        case MissionState::SEARCHING:
        case MissionState::PERSON_LOST:
            searchBehavior();
            break;

        case MissionState::PERSON_DETECTED:
            // Transition to FOLLOWING happens in the state machine
            hoverBehavior();
            break;

        case MissionState::FOLLOWING:
            followBehavior();
            break;

        case MissionState::OBSTACLE_AVOIDANCE:
            // TODO: Implement obstacle avoidance
            hoverBehavior();
            break;

        case MissionState::RETURNING:
            // TODO: Implement return to home
            hoverBehavior();
            break;

        case MissionState::LANDING:
            // TODO: Implement landing
            break;

        case MissionState::EMERGENCY:
            publishVelocity(Vec3{}, 0.0f);                                              // Emergency stop
            break;
    }

    // * Publish mission status
    publishStatus();
}


// * State machine
void Navigation::updateStateMachine() {
    MissionState next = m.state;

    switch (m.state) {
        case MissionState::IDLE:
            break;                                                                      // ? start command arrives via onManualOverride

        case MissionState::SEARCHING:
            if (detectionIsValid()) next = MissionState::PERSON_DETECTED;
            break;

        case MissionState::PERSON_DETECTED:
            // ? Require stable tracking for 1s before committing to follow
            if ((m.node->now() - m.stateEntryTime).seconds() > 1.0) {
                next = detectionIsValid() ? MissionState::FOLLOWING
                                         : MissionState::SEARCHING;
            }
            break;

        case MissionState::FOLLOWING:
            if (personIsLost()) next = MissionState::PERSON_LOST;
            // TODO: Transition to OBSTACLE_AVOIDANCE based on representation data
            break;

        case MissionState::PERSON_LOST:
            if (detectionIsValid()) next = MissionState::PERSON_DETECTED;
            // TODO: Timeout -> RETURNING
            break;

        default:
            break;
    }

    if (next != m.state) transitionTo(next);
}

void Navigation::transitionTo(MissionState next) {
    RCLCPP_INFO(m.node->get_logger(), "State transition: %s -> %s",
        toString(m.state).c_str(), toString(next).c_str());

    m.state = next;
    m.stateEntryTime = m.node->now();
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
void Navigation::followBehavior() {
    if (!detectionIsValid()) return;

    // * Target: a point behind the person at the follow distance
    const Vec3 target = followTarget();

    // * Velocity towards the target (P-controller, clamped)
    Vec3 velocity = velocityTowards(target);

    // * Yaw to face the person
    const float yawError = desiredYawTowards(m.personWorldPosition) - m.drone.yaw;
    float yawRate = m.params.yawPGain * normalizeAngle(yawError);

    // * Safety
    if (!commandIsSafe(velocity)) {
        velocity = Vec3{};
        yawRate = 0.0f;
    }

    publishVelocity(velocity, yawRate);
}

void Navigation::searchBehavior() {
    // ? Simple search: hover and rotate slowly
    publishVelocity(Vec3{}, 0.3f);
}

void Navigation::hoverBehavior() {
    publishVelocity(Vec3{}, 0.0f);
}


// * Navigation math
Vec3 Navigation::followTarget() const {
    // * Direction of motion - fall back to the current drone-to-person line if the person is stationary
    Vec3 personDirection = m.personWorldVelocity.normalized();
    if (m.personWorldVelocity.norm() < 0.1f) {
        personDirection = (m.drone.position - m.personWorldPosition).normalized();
    }

    // * A point behind the person at the follow distance, at the target height
    Vec3 target = m.personWorldPosition - personDirection * m.params.targetFollowDistance;
    target.z = m.params.targetHeight;
    return target;
}

Vec3 Navigation::velocityTowards(const Vec3& target) const {
    // * P-controller
    Vec3 velocity = (target - m.drone.position) * m.params.positionPGain;

    // * Clamp to max follow speed
    const float speed = velocity.norm();
    if (speed > m.params.maxFollowSpeed) {
        velocity = velocity * (m.params.maxFollowSpeed / speed);
    }
    return velocity;
}

float Navigation::desiredYawTowards(const Vec3& target) const {
    const Vec3 toTarget = target - m.drone.position;
    return std::atan2(toTarget.y, toTarget.x);
}

Vec3 Navigation::cameraToWorld(const Vec3& camera) const {
    // TODO: Apply the static camera mount extrinsic (camera -> body) once the rig is fixed;
    // for now the camera is assumed coincident with the body frame
    const Vec3 body = camera;

    // * Body -> world: rotate by the drone orientation, then translate by the drone position
    return m.drone.position + rotate(m.drone.orientation, body);
}


// * Safety
bool Navigation::commandIsSafe(const Vec3& velocity) const {
    if (m.drone.position.z < m.params.minAltitude)  return false;                       // ? too low
    if (m.drone.position.z > m.params.maxAltitude)  return false;                       // ? too high
    if (velocity.norm() > m.params.maxFollowSpeed)  return false;                       // ? too fast
    // TODO: Obstacle distance checks against the representation layer
    return true;
}

bool Navigation::detectionIsValid() const {
    if (!m.person.valid) return false;
    if (m.person.confidence < m.params.minDetectionConfidence) return false;

    const double sinceDetection = (m.node->now() - m.lastDetectionTime).seconds();
    if (sinceDetection > m.params.detectionTimeout) return false;

    const float distance = (m.personWorldPosition - m.drone.position).norm();
    if (distance < m.params.minFollowDistance || distance > m.params.maxFollowDistance) return false;

    return true;
}

bool Navigation::personIsLost() const {
    return (m.node->now() - m.lastDetectionTime).seconds() > m.params.detectionTimeout;
}


// * Publishing
void Navigation::publishVelocity(const Vec3& velocity, float yawRate) {
    auto msg = std::make_unique<geometry_msgs::msg::TwistStamped>();                    // ? unique_ptr publish: zero-copy intra-process
    msg->header.stamp = m.node->now();
    msg->header.frame_id = "base_link";
    msg->twist.linear.x = velocity.x;
    msg->twist.linear.y = velocity.y;
    msg->twist.linear.z = velocity.z;
    msg->twist.angular.z = yawRate;

    m.velocityPublisher->publish(std::move(msg));
}

void Navigation::publishStatus() {
    auto msg = std::make_unique<std_msgs::msg::String>();
    msg->data = toString(m.state);

    m.missionStatusPublisher->publish(std::move(msg));
}