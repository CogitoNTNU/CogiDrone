#include "pixhawk.h"

#include <cmath>
#include <limits>


namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();                          // ? PX4: NaN = "don't control this field"

constexpr int kRateHz = 10;                                                             // streamTick() rate. PX4 needs >= 2 Hz or it leaves offboard
constexpr int kRetryTicks = 2 * kRateHz;                                                // Re-send mode/arm/START_MISSION every 2 s until it sticks

// PX4 publishes best-effort; a reliable subscription would never match and receive nothing
// ? Keep durability VOLATILE: a transient_local QoS on an intra-process node makes rclcpp throw
const auto kPx4Qos = rclcpp::QoS(rclcpp::KeepLast(1)).best_effort();
} // namespace


// * Ctors & dtor
Pixhawk::~Pixhawk() = default;

std::expected<std::unique_ptr<Pixhawk>, cogidrone::Error> Pixhawk::create() {
    // * ROS2
    auto node = std::make_shared<rclcpp::Node>(
        "pixhawk",
        rclcpp::NodeOptions().use_intra_process_comms(true)
    );
    cogidrone::log(LOGGING_LEVEL::INFO, "ROS2 IPC node created successfully");

    // * Parameters (non-failable: defaults live in PixhawkParams)
    PixhawkParams params;
    params.statusTopic        = node->declare_parameter("status_topic", params.statusTopic);
    params.localPositionTopic = node->declare_parameter("local_position_topic", params.localPositionTopic);
    params.targetHeight       = static_cast<float>(node->declare_parameter("target_height", double{params.targetHeight}));
    params.commandTimeout     = static_cast<float>(node->declare_parameter("command_timeout", double{params.commandTimeout}));
    params.autoStart          = node->declare_parameter("auto_start", params.autoStart);
    cogidrone::log(LOGGING_LEVEL::INFO, "Pixhawk parameters initialized successfully");

    // * Partial assembly
    auto p = std::unique_ptr<Pixhawk>(
        new Pixhawk(M{                                                                  // ? Transfer ownership into unique_ptr as ctor is private
            .node = std::move(node),
            .offboardModePub = nullptr,                                                 // ? will be wired later
            .setpointPub = nullptr,                                                     // ? will be wired later
            .commandPub = nullptr,                                                      // ? will be wired later
            .localPosSub = nullptr,                                                     // ? will be wired later
            .statusSub = nullptr,                                                       // ? will be wired later
            .velocityInputSub = nullptr,                                                // ? will be wired later
            .missionStatusSub = nullptr,                                                // ? will be wired later
            .odometryPub = nullptr,                                                     // ? will be wired later
            .manualOverridePub = nullptr,                                               // ? will be wired later
            .streamTimer = nullptr,                                                     // ? will be wired later
            .params = std::move(params)
        })
    );
    cogidrone::log(LOGGING_LEVEL::INFO, "Pixhawk object partially constructed");

    // Wire the node
    p->wire();
    cogidrone::log(LOGGING_LEVEL::INFO, "Pixhawk object wired successfully");

    return p;
}

void Pixhawk::wire() noexcept {
    // * PX4: the uXRCE-DDS bridge exposes uORB topics under /fmu/{in,out}
    m.offboardModePub = m.node->create_publisher<px4_msgs::msg::OffboardControlMode>("/fmu/in/offboard_control_mode", 10);
    m.setpointPub = m.node->create_publisher<px4_msgs::msg::TrajectorySetpoint>("/fmu/in/trajectory_setpoint", 10);
    m.commandPub = m.node->create_publisher<px4_msgs::msg::VehicleCommand>("/fmu/in/vehicle_command", 10);

    m.localPosSub = m.node->create_subscription<px4_msgs::msg::VehicleLocalPosition>(
        m.params.localPositionTopic,
        kPx4Qos,
        [this](const px4_msgs::msg::VehicleLocalPosition& msg) {
            this->onLocalPosition(msg);
        }
    );
    m.statusSub = m.node->create_subscription<px4_msgs::msg::VehicleStatus>(
        m.params.statusTopic,
        kPx4Qos,
        [this](const px4_msgs::msg::VehicleStatus& msg) {
            this->onStatus(msg);
        }
    );

    // * Navigation (intra-process, zero-copy)
    m.velocityInputSub = m.node->create_subscription<geometry_msgs::msg::TwistStamped>(
        "/navigation/velocity_setpoint",
        rclcpp::QoS(10),
        [this](const geometry_msgs::msg::TwistStamped& msg) {
            this->onVelocityInput(msg);
        }
    );
    m.missionStatusSub = m.node->create_subscription<std_msgs::msg::String>(
        "/navigation/mission_status",
        rclcpp::QoS(10),
        [this](const std_msgs::msg::String& msg) {
            this->onMissionStatus(msg);
        }
    );

    m.odometryPub = m.node->create_publisher<nav_msgs::msg::Odometry>("/estimation/vio_odometry", 10);
    m.manualOverridePub = m.node->create_publisher<std_msgs::msg::String>("/manual_override", 10);

    // * 10 Hz heartbeat - drives the stream AND the state machine
    m.streamTimer = m.node->create_wall_timer(
        std::chrono::milliseconds(1000 / kRateHz),
        [this] {
            this->streamTick();
        }
    );
}


// * Callbacks
void Pixhawk::onLocalPosition(const px4_msgs::msg::VehicleLocalPosition& msg) {
    lastLocalPos = msg;
    publishOdometry(msg);
}

void Pixhawk::onStatus(const px4_msgs::msg::VehicleStatus& msg) {
    lastStatus = msg;
}

void Pixhawk::onVelocityInput(const geometry_msgs::msg::TwistStamped& msg) {
    lastVelocity = msg;
    lastVelocityTime = m.node->now();
}

void Pixhawk::onMissionStatus(const std_msgs::msg::String& msg) {
    std::lock_guard lock(stateMutex);

    if (msg.data != missionStatus) {
        RCLCPP_INFO(m.node->get_logger(), "Navigation: %s", msg.data.c_str());
    }
    missionStatus = msg.data;

    // ? Navigation's LANDING state publishes no velocity - landing is our job
    if (missionStatus == "LANDING" && (state == State::TAKEOFF || state == State::FLYING)) {
        RCLCPP_INFO(m.node->get_logger(), "Navigation asked to land");
        beginLanding();
    }
}

void Pixhawk::streamTick() {
    std::lock_guard lock(stateMutex);
    ++stateTicks;

    // * PX4 disarmed us (landed, failsafe, kill switch) - stop streaming for good
    if ((state == State::TAKEOFF || state == State::FLYING || state == State::LANDING) && !armed()) {
        RCLCPP_WARN(m.node->get_logger(), "PX4 reports disarmed");
        transitionTo(State::DISARMED);
    }

    switch (state) {
        case State::WAITING_EKF:
            if (positionValid() && lastStatus) {                                        // EKF has a position reference, PX4 is talking
                RCLCPP_INFO(m.node->get_logger(), "EKF position valid - streaming setpoints before offboard");
                holdHere(lastLocalPos->z - m.params.targetHeight);                      // ? NED: up is negative z
                transitionTo(State::STREAMING);
            }
            break;

        case State::STREAMING:
            streamHold();                                                               // ! PX4 only accepts offboard once a setpoint stream exists

            // After 1 s of streaming, (re)request offboard + arm every 2 s until PX4 accepts both
            // ? SITL often rejects the first request while its preflight checks settle
            if (stateTicks >= kRateHz && stateTicks % kRetryTicks == kRateHz) {
                RCLCPP_INFO(m.node->get_logger(), "Requesting offboard mode + arm");
                sendCommand(
                    px4_msgs::msg::VehicleCommand::VEHICLE_CMD_DO_SET_MODE,
                    1.f,
                    /* PX4_CUSTOM_MAIN_MODE_OFFBOARD = */ 6.f
                );
                sendCommand(
                    px4_msgs::msg::VehicleCommand::VEHICLE_CMD_COMPONENT_ARM_DISARM,
                    1.f
                );
            }
            if (armedInOffboard()) {
                RCLCPP_INFO(m.node->get_logger(), "Armed in offboard, taking off to %.1f m", m.params.targetHeight);
                transitionTo(State::TAKEOFF);
            }
            break;

        case State::TAKEOFF:
            streamHold();
            if (std::abs(lastLocalPos->z - hold->z) < m.params.takeoffTolerance) {
                RCLCPP_INFO(m.node->get_logger(), "At altitude, handing over to Navigation");
                transitionTo(State::FLYING);
            }
            break;

        case State::FLYING:
            // Follow Navigation while it talks to us, otherwise hold position instead of drifting
            if (navigationIsFresh()) {
                hold.reset();
                streamVelocity();
            } else {
                if (!hold) holdHere();
                streamHold();
            }

            // ? Navigation starts in IDLE and waits for START_MISSION; repeat it until it leaves IDLE
            if (m.params.autoStart
                && (missionStatus.empty() || missionStatus == "IDLE")
                && stateTicks % kRetryTicks == 1) {
                auto msg = std::make_unique<std_msgs::msg::String>();
                msg->data = "START_MISSION";
                m.manualOverridePub->publish(std::move(msg));
                RCLCPP_INFO(m.node->get_logger(), "Sent START_MISSION");
            }
            break;

        case State::LANDING:
            break;                                                                      // ? PX4 lands on its own; we stop streaming

        case State::DISARMED:
            break;                                                                      // terminal safe state
    }
}


// * State machine
void Pixhawk::transitionTo(State next) {
    RCLCPP_INFO(m.node->get_logger(), "State transition: %s -> %s", toString(state), toString(next));
    state = next;
    stateTicks = 0;
}

void Pixhawk::beginLanding() {
    // ? NAV_LAND hands control to PX4's landing controller - it owns the
    // ? descent, touchdown detection, and disarm. We stop streaming setpoints,
    // ? and streamTick() moves to DISARMED once PX4 reports disarmed.
    sendCommand(px4_msgs::msg::VehicleCommand::VEHICLE_CMD_NAV_LAND);
    transitionTo(State::LANDING);
}

const char* Pixhawk::toString(State state) noexcept {
    switch (state) {
        case State::WAITING_EKF:    return "WAITING_EKF";
        case State::STREAMING:      return "STREAMING";
        case State::TAKEOFF:        return "TAKEOFF";
        case State::FLYING:         return "FLYING";
        case State::LANDING:        return "LANDING";
        case State::DISARMED:       return "DISARMED";
        default:                    return "UNKNOWN";
    }
}


// * Helpers
void Pixhawk::holdHere(std::optional<float> z) {
    const auto& pos = *lastLocalPos;
    hold = Hold{pos.x, pos.y, z.value_or(pos.z), pos.heading};
}

void Pixhawk::streamHold() {
    publishOffboardMode(/* velocity = */ false);

    auto msg = std::make_unique<px4_msgs::msg::TrajectorySetpoint>();
    msg->timestamp = timestampUs();
    msg->position = {hold->x, hold->y, hold->z};
    msg->velocity = {kNaN, kNaN, kNaN};
    msg->acceleration = {kNaN, kNaN, kNaN};
    msg->yaw = hold->yaw;
    msg->yawspeed = kNaN;
    m.setpointPub->publish(std::move(msg));
}

void Pixhawk::streamVelocity() {
    publishOffboardMode(/* velocity = */ true);

    // * ENU -> NED: swap x/y, flip z. Yaw rate: ENU is counter-clockwise, NED is clockwise.
    // ? Navigation fills `linear` with a WORLD-frame velocity, even though frame_id says base_link
    const auto& t = lastVelocity->twist;
    auto msg = std::make_unique<px4_msgs::msg::TrajectorySetpoint>();
    msg->timestamp = timestampUs();
    msg->position = {kNaN, kNaN, kNaN};
    msg->velocity = {
        static_cast<float>(t.linear.y),
        static_cast<float>(t.linear.x),
        static_cast<float>(-t.linear.z)
    };
    msg->acceleration = {kNaN, kNaN, kNaN};
    msg->yaw = kNaN;
    msg->yawspeed = static_cast<float>(-t.angular.z);
    m.setpointPub->publish(std::move(msg));
}

void Pixhawk::publishOffboardMode(bool velocity) {
    auto msg = std::make_unique<px4_msgs::msg::OffboardControlMode>();
    msg->timestamp = timestampUs();
    msg->position = !velocity;
    msg->velocity = velocity;
    m.offboardModePub->publish(std::move(msg));
}

void Pixhawk::publishOdometry(const px4_msgs::msg::VehicleLocalPosition& pos) {
    if (!(pos.xy_valid && pos.z_valid)) return;

    // * NED -> ENU: swap x/y, flip z. Heading is from north, clockwise; ENU yaw is from east, counter-clockwise.
    auto odom = std::make_unique<nav_msgs::msg::Odometry>();
    odom->header.stamp = m.node->now();
    odom->header.frame_id = "odom";
    odom->child_frame_id = "base_link";

    odom->pose.pose.position.x = pos.y;
    odom->pose.pose.position.y = pos.x;
    odom->pose.pose.position.z = -pos.z;

    const float yaw = kPi / 2.f - pos.heading;
    odom->pose.pose.orientation.w = std::cos(yaw / 2.f);
    odom->pose.pose.orientation.z = std::sin(yaw / 2.f);

    odom->twist.twist.linear.x = pos.vy;                                                // ? World-frame velocity, which is what Navigation::onOdometry expects
    odom->twist.twist.linear.y = pos.vx;
    odom->twist.twist.linear.z = -pos.vz;

    m.odometryPub->publish(std::move(odom));
}

void Pixhawk::sendCommand(uint32_t command, float param1, float param2) {
    auto msg = std::make_unique<px4_msgs::msg::VehicleCommand>();
    msg->timestamp = timestampUs();
    msg->command = command;
    msg->param1 = param1;
    msg->param2 = param2;
    msg->target_system = 1;                                                             // ! Pixhawk's MAVLink system id (default); PX4 ignores commands not addressed to it
    msg->target_component = 1;                                                          // flight controller component
    msg->source_system = 1;
    msg->source_component = 1;

    // ? `from_external = true` is load-bearing. Without it, PX4 treats the command as
    // ? coming from its own internals and may ignore it. This is the single most
    // ? common "my arm command does nothing" bug in px4_msgs code
    msg->from_external = true;                                                          // ! required: marks the command as offboard/external
    m.commandPub->publish(std::move(msg));
}

uint64_t Pixhawk::timestampUs() const {
    return static_cast<uint64_t>(m.node->now().nanoseconds() / 1000);
}

bool Pixhawk::positionValid() const {
    // ? EKF2 has a position reference: xy+z valid (GPS outdoors, VIO later).
    // ? Without this, PX4 refuses offboard/arming - gating here gives a
    // ? clean log message instead of a silent command rejection.
    return lastLocalPos && lastLocalPos->xy_valid && lastLocalPos->z_valid;
}

bool Pixhawk::armed() const {
    return lastStatus && lastStatus->arming_state == px4_msgs::msg::VehicleStatus::ARMING_STATE_ARMED;
}

bool Pixhawk::armedInOffboard() const {
    return armed() && lastStatus->nav_state == px4_msgs::msg::VehicleStatus::NAVIGATION_STATE_OFFBOARD;
}

bool Pixhawk::navigationIsFresh() const {
    return lastVelocity && (m.node->now() - lastVelocityTime).seconds() < m.params.commandTimeout;
}


// * Commands
void Pixhawk::land() {
    std::lock_guard lock(stateMutex);
    if (state != State::TAKEOFF && state != State::FLYING) return;                      // nothing to land

    beginLanding();
}
