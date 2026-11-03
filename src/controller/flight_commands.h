#pragma once

#include <cstdint>


namespace cogidrone {
// ? Flight actions Navigation may request from the flight controller.
// ? Carried on the wire as std_msgs/msg/UInt8; the enum lives here so both
// ? sides share one definition and invalid values are unrepresentable in C++.
enum class FlightCommand : uint8_t {
    LAND      = 1,
    START     = 2,
    EMERGENCY = 3,
};

} // namespace cogidrone