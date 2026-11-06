#include "flight_controller.h"

#include <cmath>


FlightController::~FlightController() = default;

std::expected<std::unique_ptr<FlightController>, cogidrone::Error> FlightController::create() {
    // * ROS2
    auto node = std::make_shared<rclcpp::Node>(
        "flight_controller",
        rclcpp::NodeOptions().use_intra_process_comms(true)
    );
    cogidrone::log(LOGGING_LEVEL::INFO, "ROS2 IPC node created successfully");

    // * Partial assembly
    auto p = std::unique_ptr<FlightController>(
        new FlightController(M{                                                         // ? Transfer ownership into unique_ptr as ctor is private
            .node = std::move(node),
            .offboardModePublisher = nullptr,                                           // ? will be wired in `FlightController::wire()`
            .setpointPublisher = nullptr,                                               // ? will be wired in `FlightController::wire()`
            .vehicleCommandPublisher = nullptr,                                         // ? will be wired in `FlightController::wire()`
            .offboardActivePublisher = nullptr,                                         // ? will be wired in `FlightController::wire()`
            .armedPublisher = nullptr,                                                  // ? will be wired in `FlightController::wire()`
            .localPositionSubscription = nullptr,                                       // ? will be wired in `FlightController::wire()`
            .statusSubscription = nullptr,                                              // ? will be wired in `FlightController::wire()`
            .setpointInputSubscription = nullptr,                                       // ? will be wired in `FlightController::wire()`
            .navigationCommandSubscription  = nullptr,                                  // ? will be wired in `FlightController::wire()`
            .streamTimer = nullptr                                                      // ? will be wired in `FlightController::wire()`
        })
    );
    cogidrone::log(LOGGING_LEVEL::INFO, "FlightController object partially constructed");

    // Wire the node
    p->wire();

    return std::expected<std::unique_ptr<FlightController>, cogidrone::Error>(std::move(p));
}

void FlightController::wire() noexcept {
    // * Publishers
    // PX4 namespace: the bridge exposes uORB topics under /fmu/in/
    m.offboardModePublisher = m.node->create_publisher<px4_msgs::msg::OffboardControlMode>("/fmu/in/offboard_control_mode", 10);
    m.setpointPublisher = m.node->create_publisher<px4_msgs::msg::TrajectorySetpoint>("/fmu/in/trajectory_setpoint", 10);
    m.vehicleCommandPublisher = m.node->create_publisher<px4_msgs::msg::VehicleCommand>("/fmu/in/vehicle_command", 10);

    // Control output to Drone subsystems (intra-process, zero-copy)
    // ? `offboard_active` is the authoritative "who is the pilot" signal. It is
    // ? read straight from vehicle_status.nav_state, so it covers the RC switch,
    // ? QGC mode changes, and failsafes alike - anything that makes PX4 leave
    // ? offboard mode.
    // ! QoS MUST be transient_local to match Navigation's subscriber.
    m.offboardActivePublisher = m.node->create_publisher<std_msgs::msg::Bool>(
        "/pixhawk/offboard_active",
        rclcpp::QoS(1).transient_local()
    );

    // ? Arming state, straight from vehicle_status.arming_state.
    // ? Navigation uses this to distinguish "waiting for the pilot to arm"
    // ? (IDLE) from "pilot is flying" (MANUALLY_OVERRIDDEN).
    // ! Same QoS contract as above.
    m.armedPublisher = m.node->create_publisher<std_msgs::msg::Bool>(
        "/pixhawk/armed",
        rclcpp::QoS(1).transient_local()
    );

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

    m.navigationCommandSubscription = m.node->create_subscription<std_msgs::msg::UInt8>(
        "/navigation/flight_command", 
        rclcpp::QoS(10),
        [this](const std_msgs::msg::UInt8& msg) { 
            this->onNavigationCommand(msg);
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
void FlightController::onLocalPosition(const px4_msgs::msg::VehicleLocalPosition& msg) {
    std::lock_guard lock(positionMutex);
    lastLocalPosition = msg;
}

void FlightController::onStatus(const px4_msgs::msg::VehicleStatus& msg) {
    // ? Written from the status callback thread, read from the timer thread.
    // ? Plain bools are fine here: each is independently-written telemetry,
    // ? read-only for decisions, and a torn/stale read is harmless (next tick
    // ? re-reads). No compound invariants depend on both being in sync.
    offboardActive = (msg.nav_state == px4_msgs::msg::VehicleStatus::NAVIGATION_STATE_OFFBOARD);
    armed = msg.arming_state == px4_msgs::msg::VehicleStatus::ARMING_STATE_ARMED;

    // Offboard active
    if (// ? Publish only on change,
        !lastPublishedOffboardActive.has_value() 
        // ? but ALWAYS on the first message so the `transient_local` 
        // ? latch is populated for late-joining subscribers.
        || *lastPublishedOffboardActive != offboardActive) 
    {
        lastPublishedOffboardActive = offboardActive;
        m.offboardActivePublisher->publish(std_msgs::msg::Bool{}.set__data(offboardActive));
    }

    // Armed state
    if (// ? Publish only on change,
        !lastPublishedArmed.has_value() 
        // ? but ALWAYS on the first message so the `transient_local` 
        // ? latch is populated for late-joining subscribers.
        || *lastPublishedArmed != armed) 
    {
        lastPublishedArmed = armed;
        m.armedPublisher->publish(std_msgs::msg::Bool{}.set__data(armed));
    }
}

void FlightController::onSetpointInput(const px4_msgs::msg::TrajectorySetpoint& msg) {
    std::lock_guard lock(setpointMutex);
    currentSetpoint = msg;
}

void FlightController::onNavigationCommand(const std_msgs::msg::UInt8& msg) {
    const auto command = static_cast<cogidrone::FlightCommand>(msg.data);

    switch (command) {
        case cogidrone::FlightCommand::LAND:
            cogidrone::log(LOGGING_LEVEL::INFO, "Flight command: LAND");
            land();
            break;

        case cogidrone::FlightCommand::START:
        case cogidrone::FlightCommand::EMERGENCY:
            // TODO: each needs a corresponding FlightController action (design decision)
            break;

        default:
            cogidrone::log(LOGGING_LEVEL::WARNING, "Unknown flight command received");
            break;
    }
}

void FlightController::streamTick() {
    std::lock_guard lock(stateMutex);

    switch (state) {
        case State::STARTING:
            state = State::WAITING_FOR_POSITION;
            break;

        case State::WAITING_FOR_POSITION:
            if (isPositionValid()) {                                                    // EKF has a position reference
                // ? Freeze the setpoint at the current position BEFORE streaming.
                // ? `currentSetpoint` is default-initialized to zeros, so without
                // ? this the first streamed setpoint would be the origin - and if
                // ? PX4 enters offboard, the drone would fly there. This matters
                // ? most when FlightController restarts mid-flight.
                px4_msgs::msg::VehicleLocalPosition pos; {
                    std::lock_guard lock(positionMutex);
                    pos = lastLocalPosition;
                }
                {
                    std::lock_guard lock(setpointMutex);
                    currentSetpoint.position = {pos.x, pos.y, pos.z};
                    
                    // ? Why freeze yaw too: if the pilot rotates the drone 180° and 
                    // ? hands back control, the frozen setpoint carries the old yaw. 
                    // ? PX4 would rotate the drone back before Navigation's first 
                    // ? setpoint arrives. `VehicleLocalPosition.heading` is NAN when 
                    // ? unknown, hence the `std::isfinite` guard. 
                    if (std::isfinite(pos.heading)) {
                        currentSetpoint.yaw = pos.heading;
                    }
                }

                cogidrone::log(LOGGING_LEVEL::INFO, "EKF position valid - starting offboard stream");
                state = State::STREAMING_SETPOINTS;
            }
            break;

        case State::STREAMING_SETPOINTS:
            publishOffboardMode();
            publishSetpoint();
            // After ~1s of streaming (count ticks), request offboard mode
            if (++streamTicks > 10) {
                sendCommand(
                    px4_msgs::msg::VehicleCommand::VEHICLE_CMD_DO_SET_MODE,
                    1.f, 
                    /* PX4_CUSTOM_MAIN_MODE_OFFBOARD = */ 6.f
                );
                cogidrone::log(LOGGING_LEVEL::INFO, "Offboard requested - waiting for PX4 (pilot must select Offboard on the RC switch)");
                state = State::REQUESTING_CONTROL;
            }
            break;

        case State::REQUESTING_CONTROL:
            // ? Waits for PX4 to enter offboard mode. No timeout by design:
            // ? the pilot's mode switch is the authority.
            // ? We do NOT arm here. Arming is the pilot's explicit action via
            // ? the RC arm switch (RC_MAP_ARM_SW in QGC). FlightController only
            // ? observes `armed` and forwards it to Navigation.
            publishOffboardMode();                                                      // Keep the heartbeat alive
            publishSetpoint();

            if (offboardActive) {
                cogidrone::log(LOGGING_LEVEL::INFO, "PX4 entered offboard");
                state = State::CONTROL_GRANTED;
            }
            break;

        case State::CONTROL_GRANTED:
            if (!offboardActive) {
                // ? Freeze the setpoint at the current position BEFORE publishing,
                // ? so the very next streamed setpoint is "hold here", not the
                // ? stale pre-override target.
                px4_msgs::msg::VehicleLocalPosition pos; {
                    std::lock_guard lock(positionMutex);
                    pos = lastLocalPosition;
                }
                {
                    std::lock_guard lock(setpointMutex);
                    currentSetpoint.position = {pos.x, pos.y, pos.z};

                    // ? Why freeze yaw too: if the pilot rotates the drone 180° and 
                    // ? hands back control, the frozen setpoint carries the old yaw. 
                    // ? PX4 would rotate the drone back before Navigation's first 
                    // ? setpoint arrives. `VehicleLocalPosition.heading` is NAN when 
                    // ? unknown, hence the `std::isfinite` guard. 
                    if (std::isfinite(pos.heading)) {
                        currentSetpoint.yaw = pos.heading;                              // ? Hold current heading too
                    }
                }

                cogidrone::log(LOGGING_LEVEL::WARNING, "PX4 left offboard - pilot or failsafe has control");
                state = State::CONTROL_REVOKED;
            }

            publishOffboardMode();                                                      // never stop streaming while flying
            publishSetpoint();
            break;

        case State::LANDING:
            publishOffboardMode();                                                      // never stop streaming while flying
            publishSetpoint();

            if (!armed) {                                                               // transition to DISARMED only after PX4 has landed and disarmed itself
                cogidrone::log(LOGGING_LEVEL::INFO, "Landed and disarmed");
                state = State::DISARMED;                                                // terminal safe state; timer idles
            } else if (!offboardActive) {
                // ? Pilot took over during the landing. PX4 owns the descent now,
                // ? and Navigation must stop believing it is the pilot.
                cogidrone::log(LOGGING_LEVEL::WARNING, "PX4 left offboard during landing - pilot has control");
                state = State::CONTROL_REVOKED;
            }
            break;

        case State::CONTROL_REVOKED:
            // ? Keep streaming: PX4 requires an active setpoint stream to ACCEPT
            // ? offboard mode. Do NOT re-request offboard - the pilot's switch owns
            // ? that decision. When they flip back to Offboard, PX4 enters offboard
            // ? itself and `offboardActive` becomes true again.
            publishOffboardMode();
            publishSetpoint();

            // ? Disarm wins over re-entering offboard: if the pilot landed and
            // ? disarmed, the mission is over regardless of switch position.
            if (!armed) {
                cogidrone::log(LOGGING_LEVEL::INFO, "Disarmed while offboard was lost - terminal");
                state = State::DISARMED;
            } 
            else if (offboardActive) {
                cogidrone::log(LOGGING_LEVEL::INFO, "PX4 re-entered offboard - Navigation has control");
                state = State::CONTROL_GRANTED;
            }
            
            break;

        case State::DISARMED:
            break;                                                                      // terminal safe state
    }
}



// * Helpers
void FlightController::publishOffboardMode() {
    px4_msgs::msg::OffboardControlMode mode{};
    
    mode.position = true;                                                               // we stream position setpoints
    mode.velocity = false;
    mode.acceleration = false;
    mode.attitude = false;
    mode.body_rate = false;
    mode.timestamp = m.node->now().nanoseconds() / 1000;                                // µs, PX4 clock via bridge timesync
    
    m.offboardModePublisher->publish(mode);
}

void FlightController::publishSetpoint() {
    px4_msgs::msg::TrajectorySetpoint sp; {
        std::lock_guard lock(setpointMutex);
        sp = currentSetpoint;                                                           // snapshot under lock
    }

    sp.timestamp = m.node->now().nanoseconds() / 1000;                                  // stamp AFTER copying: fresh on every stream
    m.setpointPublisher->publish(sp);
}

void FlightController::sendCommand(uint16_t command, float param1, float param2) {
    px4_msgs::msg::VehicleCommand cmd{};

    cmd.command = command;
    cmd.param1 = param1;
    cmd.param2 = param2;
    cmd.target_system = 1;                                                              // PX4's MAVLink system id (default)
    cmd.target_component = 1;                                                           // flight controller component
    cmd.source_system = 1;
    cmd.source_component = 1;
    cmd.confirmation = false;

    // ? `from_external = true` is load-bearing. Without it, PX4 treats the command as
    // ? coming from its own internals and may ignore it. This is the single most 
    // ? common "my arm command does nothing" bug in px4_msgs code
    cmd.from_external = true;                                                           // ! required: marks the command as offboard/external. 

    cmd.timestamp = m.node->now().nanoseconds() / 1000;
    
    m.vehicleCommandPublisher->publish(cmd);
}

[[nodiscard]] bool FlightController::isPositionValid() const {
    std::lock_guard lock(positionMutex);
    // ? EKF2 has a position reference: xy+z valid (GPS outdoors, VIO later).
    // ? Without this, PX4 refuses offboard/arming - gating here gives a
    // ? clean log message instead of a silent command rejection.
    return lastLocalPosition.xy_valid && lastLocalPosition.z_valid;
}



// * Commands
void FlightController::land() {
    std::lock_guard lock(stateMutex);

    // ? Only meaningful when Navigation has control. In CONTROL_REVOKED the pilot
    // ? is flying, so Navigation must not be able to command a landing. In
    // ? LANDING it is already landing. In DISARMED there is nothing to land.
    // ? If an emergency landing is ever needed while the pilot has control,
    // ? that is a separate command with its own semantics (e.g. a kill switch
    // ? or a PX4 failsafe action), not this one.
    if (state != State::CONTROL_GRANTED) return;                                        // nothing to land

    // ? NAV_LAND hands control to PX4's landing controller - it owns the
    // ? descent, touchdown detection, and disarm. We keep streaming setpoints
    // ? (harmless; PX4 ignores them in auto-land) until DISARMED.
    sendCommand(px4_msgs::msg::VehicleCommand::VEHICLE_CMD_NAV_LAND, 0.f, 0.f);
    state = State::LANDING;
}