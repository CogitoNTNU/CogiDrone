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
            .offboardModePub = nullptr,                                                 // ? will be wired later
            .setpointPub = nullptr,                                                     // ? will be wired later
            .commandPub = nullptr,                                                      // ? will be wired later
            .localPosSub = nullptr,                                                     // ? will be wired later
            .statusSub = nullptr,                                                       // ? will be wired later
            .setpointInputSub = nullptr,                                                // ? will be wired later
            .streamTimer = nullptr                                                      // ? will be wired later
        })
    );
    cogidrone::log(LOGGING_LEVEL::INFO, "Pixhawk object partially constructed");

    // Wire the node
    p->wire();

    return std::expected<std::unique_ptr<Pixhawk>, cogidrone::Error>(std::move(p));
}

void Pixhawk::wire() noexcept {
    // PX4 namespace: the bridge exposes uORB topics under /fmu/{in,out}
    m.offboardModePub = m.node->create_publisher<px4_msgs::msg::OffboardControlMode>("/fmu/in/offboard_control_mode", 10);
    m.setpointPub = m.node->create_publisher<px4_msgs::msg::TrajectorySetpoint>("/fmu/in/trajectory_setpoint", 10);
    m.commandPub = m.node->create_publisher<px4_msgs::msg::VehicleCommand>("/fmu/in/vehicle_command", 10);

    m.localPosSub = m.node->create_subscription<px4_msgs::msg::VehicleLocalPosition>(
        "/fmu/out/vehicle_local_position", 
        rclcpp::QoS(10).best_effort(),
        [this](const px4_msgs::msg::VehicleLocalPosition& msg) { 
            this->onLocalPosition(msg); 
        }
    );
    m.statusSub = m.node->create_subscription<px4_msgs::msg::VehicleStatus>(
        "/fmu/out/vehicle_status", 
        rclcpp::QoS(10).best_effort(),
        [this](const px4_msgs::msg::VehicleStatus& msg) { 
            this->onStatus(msg); 
        }
    );

    // Control input from Navigation (intra-process, zero-copy)
    m.setpointInputSub = m.node->create_subscription<px4_msgs::msg::TrajectorySetpoint>(
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

void Pixhawk::streamTick() {
    std::lock_guard lock(m_stateMutex);

    switch (m_state) {
        case State::INIT:
            m_state = State::WAITING_EKF;
            break;

        case State::WAITING_EKF:
            if (positionValid()) {                                                      // EKF has a position reference
                cogidrone::log(LOGGING_LEVEL::INFO, "EKF position valid — starting offboard stream");
                m_state = State::STREAMING;
            }
            break;

        case State::STREAMING:
            publishOffboardMode();
            publishSetpoint();
            // After ~1s of streaming (count ticks), request offboard mode
            if (++m_streamTicks > 10) {
                sendCommand(
                    px4_msgs::msg::VehicleCommand::VEHICLE_CMD_DO_SET_MODE,
                    1.f, 
                    /* PX4_CUSTOM_MAIN_MODE_OFFBOARD = */ 6.f
                );
                m_state = State::OFFBOARD_REQUEST;
            }
            break;

        case State::OFFBOARD_REQUEST:
            publishOffboardMode();                                                      // keep the heartbeat alive!
            publishSetpoint();
            if (m_offboardActive) {
                sendCommand(
                    px4_msgs::msg::VehicleCommand::VEHICLE_CMD_COMPONENT_ARM_DISARM, 
                    1.f
                );
                m_state = State::FLYING;
            }
            break;

        case State::FLYING:                                                             // ? Fallthrough
        case State::LANDING:
            publishOffboardMode();                                                      // never stop streaming while flying
            publishSetpoint();
            break;

        case State::DISARMED:
            break;                                                                      // terminal safe state
    }
}

void Pixhawk::onSetpointInput(const px4_msgs::msg::TrajectorySetpoint& msg) {
    std::lock_guard lock(m_setpointMutex);
    m_currentSetpoint = msg;
}
