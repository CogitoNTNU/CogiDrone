#pragma once

#include "link/vehicle_link.h"

#include <vector>


// An IVehicleLink that talks to nothing.
//
// ? Deliberately dumb: no threads, no timers, no DDS, no ROS. You push telemetry
// ? in, the code under test reads it and sends commands, and you inspect what it
// ? sent. Because vehicle_link.h is ROS-free, this whole class compiles and runs
// ? without any of third-party/ros2 - which matters, because those binaries are
// ? arm64-only and cannot run on a dev machine or an x86 CI runner.
//
// ! Not used by anything yet. The test framework is a deliberately open decision
// ! (see the plan), so this ships as the seam for tests rather than with them.
class FakeLink final : public IVehicleLink {
public:
    // What the code under test asked the vehicle to do, in order.
    enum class Command {
        Arm,
        Disarm,
    };

    FakeLink() = default;
    ~FakeLink() override = default;

    // * IVehicleLink - the production surface
    [[nodiscard]] std::optional<Odometry> odometry() const override;
    [[nodiscard]] FlightState state() const override;
    void send(const Setpoint& setpoint) override;
    void arm() override;
    void disarm() override;
    void tick() override;

    // * Test surface - drive the fake
    void push_odometry(const Odometry& odometry);                                        // becomes the value odometry() returns
    void set_state(FlightState state);
    void clear();                                                                        // forget recorded commands, keep telemetry

    // * Test surface - inspect what was sent
    [[nodiscard]] const std::vector<Setpoint>& sent_setpoints() const { return sent_setpoints_; }
    [[nodiscard]] const std::vector<Command>& commands() const { return commands_; }

    // ! Counts calls, not rate. Asserting the >2 Hz offboard contract from
    // ! vehicle_link.h needs a clock the link's owner injects, which does not
    // ! exist yet - so a test can currently check that ticking happens, not that
    // ! it happens often enough. Do not mistake a nonzero count for compliance.
    [[nodiscard]] std::size_t tick_count() const { return tick_count_; }

private:
    std::optional<Odometry> odometry_{};
    FlightState state_{FlightState::Disconnected};

    std::vector<Setpoint> sent_setpoints_{};
    std::vector<Command> commands_{};
    std::size_t tick_count_{0};
};
