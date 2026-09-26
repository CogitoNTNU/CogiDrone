#include "link/px4_dds_link.h"

#include <cmath>
#include <limits>


namespace {

// ! PX4 reads NaN as "this field is not controlled". Leaving an unset Setpoint
// ! component as 0.0 instead would command a hard move to the origin.
constexpr float kUnset = std::numeric_limits<float>::quiet_NaN();

float or_unset(const std::optional<double>& value) {
    return value ? static_cast<float>(*value) : kUnset;
}

// PX4 publishes some fields as NaN when the estimator has no answer yet.
bool is_valid(float value) {
    return !std::isnan(value);
}

}  // namespace


// * Ctor & factory
Px4DdsLink::Px4DdsLink(rclcpp::Node::SharedPtr node) : node_(std::move(node)) {}

std::expected<std::unique_ptr<Px4DdsLink>, Px4DdsLink::LinkError>
Px4DdsLink::create() {
    // rclcpp::init() is the caller's job (Drone::initiate does it). Creating a
    // node without it throws, so fail cleanly instead.
    if (!rclcpp::ok()) {
        return std::unexpected(LinkError::RosNotInitialized);
    }

    rclcpp::Node::SharedPtr node;
    try {
        node = std::make_shared<rclcpp::Node>("cogidrone_link");
    } catch (const rclcpp::exceptions::RCLError&) {
        return std::unexpected(LinkError::NodeCreationFailed);
    }

    // ? Same reason Drone uses a private ctor + factory: the link is only handed
    // ? out once it is fully wired, so no caller can observe a half-built link
    // ? that would silently publish nothing.
    auto link = std::unique_ptr<Px4DdsLink>(new Px4DdsLink(std::move(node)));
    link->wire_up();
    return link;
}

void Px4DdsLink::wire_up() {
    // ! PX4's uXRCE-DDS client publishes /fmu/out/* as BEST_EFFORT, KEEP_LAST(5),
    // ! VOLATILE. rclcpp's default subscription QoS is RELIABLE, which does not
    // ! match - the subscription then simply never connects, with no error and no
    // ! warning. Every /fmu/out/* subscription must therefore say this explicitly.
    rclcpp::QoS px4_qos(rclcpp::KeepLast(5));
    px4_qos.best_effort().durability_volatile();

    odometry_sub_ = node_->create_subscription<px4_msgs::msg::VehicleOdometry>(
        "/fmu/out/vehicle_odometry",
        px4_qos,
        [this](const px4_msgs::msg::VehicleOdometry& msg) {
            // ! Only NED is handled. PX4 can publish FRD (a world-fixed frame with
            // ! an arbitrary heading reference), and silently treating that as NED
            // ! would rotate every position by an unknown yaw. Drop it instead.
            if (msg.pose_frame != px4_msgs::msg::VehicleOdometry::POSE_FRAME_NED) {
                return;
            }

            Odometry odometry{};
            odometry.timestamp_us = msg.timestamp;

            if (is_valid(msg.position[0])) {
                odometry.north = msg.position[0];
                odometry.east  = msg.position[1];
                odometry.down  = msg.position[2];
            }

            if (msg.velocity_frame == px4_msgs::msg::VehicleOdometry::VELOCITY_FRAME_NED
                && is_valid(msg.velocity[0])) {
                odometry.velocity_north = msg.velocity[0];
                odometry.velocity_east  = msg.velocity[1];
                odometry.velocity_down  = msg.velocity[2];
            }

            // msg.q is the rotation from the FRD body frame to the reference
            // frame, in (w, x, y, z) order - exactly QuaternionNedFrd's meaning.
            if (is_valid(msg.q[0])) {
                odometry.orientation = {
                    .w = msg.q[0], .x = msg.q[1], .y = msg.q[2], .z = msg.q[3],
                };
            }

            const std::lock_guard<std::mutex> lock(telemetry_mutex_);
            latest_odometry_ = odometry;
        });

    status_sub_ = node_->create_subscription<px4_msgs::msg::VehicleStatus>(
        "/fmu/out/vehicle_status",
        px4_qos,
        [this](const px4_msgs::msg::VehicleStatus& msg) {
            const std::lock_guard<std::mutex> lock(telemetry_mutex_);
            latest_status_ = msg;
        });

    // /fmu/in/* is inbound to PX4, where the default reliable QoS is correct.
    offboard_pub_ = node_->create_publisher<px4_msgs::msg::OffboardControlMode>(
        "/fmu/in/offboard_control_mode", 10);
    setpoint_pub_ = node_->create_publisher<px4_msgs::msg::TrajectorySetpoint>(
        "/fmu/in/trajectory_setpoint", 10);
    command_pub_ = node_->create_publisher<px4_msgs::msg::VehicleCommand>(
        "/fmu/in/vehicle_command", 10);
}


// * Telemetry (RX)
std::optional<Odometry> Px4DdsLink::odometry() const {
    const std::lock_guard<std::mutex> lock(telemetry_mutex_);
    return latest_odometry_;
}

FlightState Px4DdsLink::state() const {
    const std::lock_guard<std::mutex> lock(telemetry_mutex_);

    // No status has ever arrived: either the agent is not running, the PX4 client
    // is not configured, or the QoS does not match. All three look identical here.
    if (!latest_status_) {
        return FlightState::Disconnected;
    }
    const auto& status = *latest_status_;

    // Failsafe outranks everything else - PX4 has taken the vehicle back.
    if (status.failsafe) {
        return FlightState::Failsafe;
    }

    if (status.arming_state == px4_msgs::msg::VehicleStatus::ARMING_STATE_ARMED) {
        switch (status.nav_state) {
            case px4_msgs::msg::VehicleStatus::NAVIGATION_STATE_AUTO_TAKEOFF:
                return FlightState::Takeoff;
            case px4_msgs::msg::VehicleStatus::NAVIGATION_STATE_AUTO_LAND:
                return FlightState::Landing;
            case px4_msgs::msg::VehicleStatus::NAVIGATION_STATE_OFFBOARD:
                return FlightState::Autonomous;
            default:
                return FlightState::Armed;
        }
    }

    // Disarmed: is it willing to arm?
    return status.pre_flight_checks_pass ? FlightState::Ready : FlightState::NotReady;
}


// * Commands (TX)
void Px4DdsLink::send(const Setpoint& setpoint) {
    px4_msgs::msg::TrajectorySetpoint msg{};
    msg.timestamp = now_us();

    msg.position = {or_unset(setpoint.north), or_unset(setpoint.east), or_unset(setpoint.down)};
    msg.velocity = {or_unset(setpoint.velocity_north),
                    or_unset(setpoint.velocity_east),
                    or_unset(setpoint.velocity_down)};
    msg.acceleration = {kUnset, kUnset, kUnset};
    msg.jerk = {kUnset, kUnset, kUnset};

    msg.yaw = or_unset(setpoint.yaw_rad);
    msg.yawspeed = kUnset;

    setpoint_pub_->publish(msg);
}

void Px4DdsLink::arm() {
    send_vehicle_command(px4_msgs::msg::VehicleCommand::VEHICLE_CMD_COMPONENT_ARM_DISARM, 1.0F);
}

void Px4DdsLink::disarm() {
    send_vehicle_command(px4_msgs::msg::VehicleCommand::VEHICLE_CMD_COMPONENT_ARM_DISARM, 0.0F);
}

void Px4DdsLink::tick() {
    // ! The offboard liveness heartbeat. See the contract on IVehicleLink::tick:
    // ! PX4 needs this at >2 Hz or it leaves offboard mode mid-flight.
    px4_msgs::msg::OffboardControlMode msg{};
    msg.timestamp = now_us();

    // Position control only, for now: the vehicle follows TrajectorySetpoint
    // positions. Flipping these flags changes which setpoint fields PX4 honours,
    // so they must agree with what send() actually populates.
    msg.position = true;
    msg.velocity = false;
    msg.acceleration = false;
    msg.attitude = false;
    msg.body_rate = false;
    msg.thrust_and_torque = false;
    msg.direct_actuator = false;

    offboard_pub_->publish(msg);
}


// * Helpers
uint64_t Px4DdsLink::now_us() const {
    // ? PX4 stamps are microseconds since ITS boot, not ROS time. Using the ROS
    // ? clock works because the uXRCE-DDS agent time-synchronises the two clock
    // ? domains; without an agent the stamps are meaningless - but then nothing
    // ? is listening anyway.
    return static_cast<uint64_t>(node_->get_clock()->now().nanoseconds() / 1000);
}

void Px4DdsLink::send_vehicle_command(uint32_t command, float param1) {
    px4_msgs::msg::VehicleCommand msg{};
    msg.timestamp = now_us();
    msg.command = command;
    msg.param1 = param1;
    msg.target_system = 1;
    msg.target_component = 1;
    msg.source_system = 1;
    msg.source_component = 1;
    msg.from_external = true;

    command_pub_->publish(msg);
}
