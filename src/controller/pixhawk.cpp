#include "pixhawk.h"



Pixhawk::~Pixhawk() = default;

std::expected<std::unique_ptr<Pixhawk>, cogidrone::Error> Pixhawk::create() {
    // * ROS2
    auto node = std::make_shared<rclcpp::Node>(
        "pixhawk",
        rclcpp::NodeOptions().use_intra_process_comms(true)
    );
    cogidrone::log(LOGGING_LEVEL::INFO, "ROS2 IPC node created successfully");

    // * Partial assembly
    auto p = std::unique_ptr<Pixhawk>(
        new Pixhawk(M{                                                                  // ? Transfer ownership into unique_ptr as ctor is private
            .node = std::move(node),
            .offboardModePublisher = nullptr,                                           // ? will be wired later
            .setpointPublisher = nullptr,                                               // ? will be wired later
            .commandPublisher = nullptr,                                                // ? will be wired later
            .localPositionSubscription = nullptr,                                       // ? will be wired later
            .statusSubscription = nullptr,                                              // ? will be wired later
            .setpointInputSubscription = nullptr,                                       // ? will be wired later
            .streamTimer = nullptr                                                      // ? will be wired later
        })
    );
    cogidrone::log(LOGGING_LEVEL::INFO, "Pixhawk object partially constructed");

    // Wire the node
    p->wire();

    return std::expected<std::unique_ptr<Pixhawk>, cogidrone::Error>(std::move(p));
}

void Pixhawk::wire() noexcept {
    // * Publishers
    // PX4 namespace: the bridge exposes uORB topics under /fmu/in/
    m.offboardModePublisher = m.node->create_publisher<px4_msgs::msg::OffboardControlMode>("/fmu/in/offboard_control_mode", 10);
    m.setpointPublisher = m.node->create_publisher<px4_msgs::msg::TrajectorySetpoint>("/fmu/in/trajectory_setpoint", 10);
    m.commandPublisher = m.node->create_publisher<px4_msgs::msg::VehicleCommand>("/fmu/in/vehicle_command", 10);

    // * Subscriptions
    // PX4 namespace: the bridge exposes uORB topics under /fmu/out/
    m.localPositionSubscription = m.node->create_subscription<px4_msgs::msg::VehicleLocalPosition>(
        "/fmu/out/vehicle_local_position", 
        rclcpp::QoS(10).best_effort(),
        [this](const px4_msgs::msg::VehicleLocalPosition& msg) { 
            this->onLocalPosition(msg); 
        }
    );
    m.statusSubscription = m.node->create_subscription<px4_msgs::msg::VehicleStatus>(
        "/fmu/out/vehicle_status", 
        rclcpp::QoS(10).best_effort(),
        [this](const px4_msgs::msg::VehicleStatus& msg) { 
            this->onStatus(msg); 
        }
    );

    // Control input from Navigation (intra-process, zero-copy)
    m.setpointInputSubscription = m.node->create_subscription<px4_msgs::msg::TrajectorySetpoint>(
        "/navigation/setpoint", 
        rclcpp::QoS(10),
        [this](const px4_msgs::msg::TrajectorySetpoint& msg) { 
            this->onSetpointInput(msg); 
        }
    );


    // 10 Hz heartbeat - drives the stream AND the state machine
    m.streamTimer = m.node->create_wall_timer(
        std::chrono::milliseconds(100), 
        [this] { 
            this->streamTick(); 
        }
    );
}



// * Callbacks
void Pixhawk::onLocalPosition(const px4_msgs::msg::VehicleLocalPosition& msg) {
    std::lock_guard lock(posMutex);
    lastLocalPos = msg;
}

void Pixhawk::onStatus(const px4_msgs::msg::VehicleStatus& msg) {
    // ? Written from the status callback thread, read from the timer thread.
    // ? Plain bools are fine here: each is independently-written telemetry,
    // ? read-only for decisions, and a torn/stale read is harmless (next tick
    // ? re-reads). No compound invariants depend on both being in sync.
    offboardActive = (msg.nav_state == px4_msgs::msg::VehicleStatus::NAVIGATION_STATE_OFFBOARD);
    armed = msg.arming_state == px4_msgs::msg::VehicleStatus::ARMING_STATE_ARMED;
}

void Pixhawk::onSetpointInput(const px4_msgs::msg::TrajectorySetpoint& msg) {
    std::lock_guard lock(setpointMutex);
    currentSetpoint = msg;
}

void Pixhawk::streamTick() {
    std::lock_guard lock(stateMutex);

    switch (state) {
        case State::INIT:
            state = State::WAITING_EKF;
            break;

        case State::WAITING_EKF:
            if (isPositionValid()) {                                                    // EKF has a position reference
                cogidrone::log(LOGGING_LEVEL::INFO, "EKF position valid - starting offboard stream");
                state = State::STREAMING;
            }
            break;

        case State::STREAMING:
            publishOffboardMode();
            publishSetpoint();
            // After ~1s of streaming (count ticks), request offboard mode
            if (++streamTicks > 10) {
                sendCommand(
                    px4_msgs::msg::VehicleCommand::VEHICLE_CMD_DO_SET_MODE,
                    1.f, 
                    /* PX4_CUSTOM_MAIN_MODE_OFFBOARD = */ 6.f
                );
                state = State::OFFBOARD_REQUEST;
            }
            break;

        case State::OFFBOARD_REQUEST:
            publishOffboardMode();                                                      // keep the heartbeat alive!
            publishSetpoint();
            if (offboardActive) {
                sendCommand(
                    px4_msgs::msg::VehicleCommand::VEHICLE_CMD_COMPONENT_ARM_DISARM, 
                    1.f
                );
                state = State::FLYING;
            }
            break;

        case State::FLYING:                                                             // ? fallthrough
        case State::LANDING:
            publishOffboardMode();                                                      // never stop streaming while flying
            publishSetpoint();
            
            if (!armed && offboardActive == false && state == State::LANDING) {         // transition to DISARMED only after PX4 has landed and disarmed itself
                cogidrone::log(LOGGING_LEVEL::INFO, "Landed and disarmed");
                state = State::DISARMED;                                                // terminal safe state; timer idles
            }
            break;

        case State::DISARMED:
            break;                                                                      // terminal safe state
    }
}



// * Helpers
void Pixhawk::publishOffboardMode() {
    px4_msgs::msg::OffboardControlMode mode{};
    
    mode.position = true;                                                               // we stream position setpoints
    mode.velocity = false;
    mode.acceleration = false;
    mode.attitude = false;
    mode.body_rate = false;
    mode.timestamp = m.node->now().nanoseconds() / 1000;                                // µs, PX4 clock via bridge timesync
    
    m.offboardModePublisher->publish(mode);
}

void Pixhawk::publishSetpoint() {
    px4_msgs::msg::TrajectorySetpoint sp; {
        std::lock_guard lock(setpointMutex);
        sp = currentSetpoint;                                                           // snapshot under lock
    }

    sp.timestamp = m.node->now().nanoseconds() / 1000;                                  // stamp AFTER copying: fresh on every stream
    m.setpointPublisher->publish(sp);
}

void Pixhawk::sendCommand(uint16_t command, float param1, float param2) {
    px4_msgs::msg::VehicleCommand cmd{};

    cmd.command = command;
    cmd.param1 = param1;
    cmd.param2 = param2;
    cmd.target_system = 1;                                                              // Pixhawk's MAVLink system id (default)
    cmd.target_component = 1;                                                           // flight controller component
    cmd.source_system = 1;
    cmd.source_component = 1;
    cmd.confirmation = false;

    // ? `from_external = true` is load-bearing. Without it, PX4 treats the command as
    // ? coming from its own internals and may ignore it. This is the single most 
    // ? common "my arm command does nothing" bug in px4_msgs code
    cmd.from_external = true;                                                           // ! required: marks the command as offboard/external. 

    cmd.timestamp = m.node->now().nanoseconds() / 1000;
    
    m.commandPublisher->publish(cmd);
}

[[nodiscard]] bool Pixhawk::isPositionValid() const {
    std::lock_guard lock(posMutex);
    // ? EKF2 has a position reference: xy+z valid (GPS outdoors, VIO later).
    // ? Without this, PX4 refuses offboard/arming - gating here gives a
    // ? clean log message instead of a silent command rejection.
    return lastLocalPos.xy_valid && lastLocalPos.z_valid;
}



// * Commands
void Pixhawk::land() {
    std::lock_guard lock(stateMutex);
    if (state != State::FLYING) return;                                                 // nothing to land

    // ? NAV_LAND hands control to PX4's landing controller - it owns the
    // ? descent, touchdown detection, and disarm. We keep streaming setpoints
    // ? (harmless; PX4 ignores them in auto-land) until DISARMED.
    sendCommand(px4_msgs::msg::VehicleCommand::VEHICLE_CMD_NAV_LAND, 0.f, 0.f);
    state = State::LANDING;
}
