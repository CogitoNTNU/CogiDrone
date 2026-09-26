#include "link/fake_link.h"


// * IVehicleLink
std::optional<Odometry> FakeLink::odometry() const {
    return odometry_;
}

FlightState FakeLink::state() const {
    return state_;
}

void FakeLink::send(const Setpoint& setpoint) {
    sent_setpoints_.push_back(setpoint);
}

void FakeLink::arm() {
    commands_.push_back(Command::Arm);
}

void FakeLink::disarm() {
    commands_.push_back(Command::Disarm);
}

void FakeLink::tick() {
    ++tick_count_;
}


// * Test surface
void FakeLink::push_odometry(const Odometry& odometry) {
    odometry_ = odometry;
}

void FakeLink::set_state(FlightState state) {
    state_ = state;
}

void FakeLink::clear() {
    sent_setpoints_.clear();
    commands_.clear();
    tick_count_ = 0;
}
