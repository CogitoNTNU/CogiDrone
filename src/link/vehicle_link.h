#pragma once

#include <cstdint>
#include <optional>


// ! This header must NEVER include <rclcpp/...> or <px4_msgs/...>, and neither
// ! must anything it pulls in. That is the entire purpose of the file.
// ?
// ? third-party/ros2 contains arm64-only .so files, so any code that touches
// ? rclcpp can only ever run on the Jetson (or an arm64 container). Keeping this
// ? port ROS-free means flight logic written against it - and FakeLink - compile
// ? and run anywhere, with no middleware present at all. src/C++ STANDARD.md
// ? makes the same argument: "Keep C++23 only for code that never touches ROS 2".
// ?
// ? Px4DdsLink is the only place px4_msgs is allowed to appear.


// * Coordinate frames
// ! PX4 is NED (North-East-Down) with FRD body axes. ROS 2 is ENU with FLU.
// ! Mixing them is the single most common source of silently inverted axes in
// ! PX4 integrations, so every field below names its frame. Conversion happens
// ! ONLY inside Px4DdsLink, never implicitly, and there is deliberately no
// ! generic Vector3 type here - an untyped triple is exactly how a frame gets
// ! lost. If you find yourself wanting one, you want a named struct instead.

// Orientation of the body (FRD) relative to the local NED frame.
struct QuaternionNedFrd {
    double w{1.0};
    double x{0.0};
    double y{0.0};
    double z{0.0};
};

// Vehicle pose and velocity, as PX4 reports it: metres and metres/second, NED.
struct Odometry {
    double north{0.0};
    double east{0.0};
    double down{0.0};                                                                   // ! positive is DOWN: altitude 5 m above home is down = -5.0

    double velocity_north{0.0};
    double velocity_east{0.0};
    double velocity_down{0.0};

    QuaternionNedFrd orientation{};

    uint64_t timestamp_us{0};                                                           // PX4 hrt microseconds since boot, not ROS time
};

// What we ask the vehicle to do. Position and velocity are independently
// optional: PX4 reads NaN as "unset" per field, and leaving both unset makes the
// setpoint a pure hold.
struct Setpoint {
    std::optional<double> north;
    std::optional<double> east;
    std::optional<double> down;

    std::optional<double> velocity_north;
    std::optional<double> velocity_east;
    std::optional<double> velocity_down;

    std::optional<double> yaw_rad;                                                      // NED, clockwise from north
};

// ? These are the states src/NOTES.md specifies for the future `flight_manager`:
// ? "Create one flight_manager component that owns flight commands and enforces
// ? states such as..." - reproduced here so the machine has somewhere to live.
// ? This enum reports what the vehicle IS; it does not drive the transitions.
enum class FlightState {
    Disconnected,                                                                       // no telemetry has ever arrived
    Connected,                                                                          // telemetry flowing, vehicle not evaluated yet
    NotReady,                                                                           // vehicle refuses to arm (preflight failure)
    Ready,                                                                              // armable
    Armed,
    Takeoff,
    Autonomous,
    Landing,
    Failsafe,
};


// * The port
// ? One process, one link, one owner. src/NOTES.md: "Avoid having multiple
// ? independent components simultaneously command the vehicle." Ownership of an
// ? IVehicleLink is therefore what makes something THE commander.
class IVehicleLink {
public:
    virtual ~IVehicleLink() = default;

    IVehicleLink() = default;
    IVehicleLink(const IVehicleLink&) = delete;                                         // a link is a hardware channel: not copyable,
    IVehicleLink& operator=(const IVehicleLink&) = delete;                              // and held by pointer, so not movable either
    IVehicleLink(IVehicleLink&&) = delete;
    IVehicleLink& operator=(IVehicleLink&&) = delete;

    // * Telemetry (RX)
    // Latest odometry, or nullopt if none has arrived yet. Never blocks.
    [[nodiscard]] virtual std::optional<Odometry> odometry() const = 0;
    [[nodiscard]] virtual FlightState state() const = 0;

    // * Commands (TX)
    virtual void send(const Setpoint& setpoint) = 0;
    virtual void arm() = 0;
    virtual void disarm() = 0;

    // ! MUST be called at more than 2 Hz, both BEFORE entering offboard mode and
    // ! continuously while in it. PX4 requires a steady OffboardControlMode
    // ! stream as a liveness signal and will drop out of offboard the moment it
    // ! lapses - mid-flight, silently, with the vehicle reverting to its failsafe.
    // ?
    // ? This is exposed rather than hidden behind an internal timer on purpose:
    // ? the obligation is real and belongs to whoever owns the link, and a fake
    // ? can then record the call rate so a test can assert the contract is met.
    virtual void tick() = 0;
};
