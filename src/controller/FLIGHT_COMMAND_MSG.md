## 3. Custom message with an enum — don't create a `.msg`; use `UInt8` + a C++ enum

Here's the catch: **a real `.msg` file requires rosidl codegen**, which requires colcon/ament — the exact machinery you just spent a day escaping. You'd have to create an interface package, build it on the Jetson, then vendor the generated output into `vendored` the same way you did `px4_msgs`. That's a lot of ceremony for one byte.

The pragmatic equivalent: **carry a `std_msgs/msg/UInt8` on the wire, and define the enum in a shared C++ header.** You get compile-time safety at every call site, zero codegen, and no new package.

### Shared header

Put it somewhere both subsystems can include. Since Pixhawk is the consumer and defines which actions exist, `src/controller/flight_command.h` is defensible; a neutral `src/interfaces/flight_command.h` is cleaner if you expect more shared vocabulary later.

```cpp
// src/controller/flight_command.h
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
```

### Navigation side

```cpp
// navigation.h
#include "controller/flight_command.h"
...
rclcpp::Publisher<std_msgs::msg::UInt8>::SharedPtr flightCommandPublisher;
void publishFlightCommand(cogidrone::FlightCommand command);
```

```cpp
// navigation.cpp
void Navigation::publishFlightCommand(cogidrone::FlightCommand command) {
    auto msg = std::make_unique<std_msgs::msg::UInt8>();
    msg->data = static_cast<uint8_t>(command);
    m.flightCommandPublisher->publish(std::move(msg));
}

void Navigation::onEnter(MissionState entered) {
    switch (entered) {
        case MissionState::LANDING:
            publishFlightCommand(cogidrone::FlightCommand::LAND);
            break;
        default:
            break;
    }
}
```

### Pixhawk side

```cpp
// pixhawk.h
#include "controller/flight_command.h"
...
rclcpp::Subscription<std_msgs::msg::UInt8>::SharedPtr navigationCommandSubscription;
void onNavigationCommand(const std_msgs::msg::UInt8& msg);
```

```cpp
// pixhawk.cpp
void Pixhawk::onNavigationCommand(const std_msgs::msg::UInt8& msg) {
    const auto command = static_cast<cogidrone::FlightCommand>(msg.data);

    switch (command) {
        case cogidrone::FlightCommand::LAND:
            cogidrone::log(LOGGING_LEVEL::INFO, "Flight command: LAND");
            land();
            break;

        case cogidrone::FlightCommand::START:
        case cogidrone::FlightCommand::EMERGENCY:
            // TODO: each needs a corresponding Pixhawk action (design decision)
            break;

        default:
            cogidrone::log(LOGGING_LEVEL::WARNING, "Unknown flight command received");
            break;
    }
}
```

### Testing from the terminal

```bash
ros2 topic pub --once /navigation/flight_command std_msgs/msg/UInt8 "{data: 1}"   # LAND
```

The downside vs a real `.msg`: the terminal shows `1`, not `LAND`, and the wire type isn't self-describing. If that becomes painful, the upgrade path is a proper interface package — but do it when you have three or four message types worth packaging, not for one byte.

### What you gain over `std_msgs/String`

| | `String` | `UInt8` + enum |
|---|---|---|
| Typo fails silently | ✅ yes (`"land"`) | ❌ no — compile error |
| Allocation | SSO, none in practice | none |
| Terminal readability | `"LAND"` | `1` |
| Codegen required | no | no |

The typo protection is the real win: `publishFlightCommand("LAND")` compiles even if you meant `"LANDING"`, and the failure is a drone that doesn't land. `FlightCommand::LAND` cannot be misspelled.