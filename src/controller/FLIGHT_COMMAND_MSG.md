# UInt8 with enum class
## Custom message with an enum — don't create a `.msg`; use `UInt8` + a C++ enum

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
// flight_controller.h
#include "controller/flight_command.h"
...
rclcpp::Subscription<std_msgs::msg::UInt8>::SharedPtr navigationCommandSubscription;
void onNavigationCommand(const std_msgs::msg::UInt8& msg);
```

```cpp
// flight_controller.cpp
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


# Better, and more correct, custom PX4 message implemation
## Custom message type 
### Option A — `std_msgs/UInt8` + shared C++ enum 

Zero codegen, compile-time safety at call sites, no new package.

```cpp
// src/controller/flight_command.h
#pragma once
#include <cstdint>

namespace cogidrone {
enum class FlightCommand : uint8_t {
    LAND      = 1,
    START     = 2,
    EMERGENCY = 3,
};
} // namespace cogidrone
```

Navigation publishes `std_msgs::msg::UInt8` with `static_cast<uint8_t>(command)`; Pixhawk subscribes to `UInt8` and switches on the enum. Both sides include the same header.

**Trade-off:** `ros2 topic echo` shows `1`, not `LAND`. Acceptable for one byte.

### Option B — proper interface package (when you have 3+ message types)

This is how `px4_msgs` was made. You'd create:

```
cogidrone_msgs/
  package.xml
  CMakeLists.txt
  msg/FlightCommand.msg
```

```cmake
# cogidrone_msgs/CMakeLists.txt
find_package(rosidl_default_generators REQUIRED)
rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/FlightCommand.msg"
)
```

```xml
<!-- package.xml -->
<buildtool_depend>ament_cmake</buildtool_depend>
<buildtool_depend>rosidl_default_generators</buildtool_depend>
<exec_depend>rosidl_default_runtime</exec_depend>
<member_of_group>rosidl_interface_packages</member_of_group>
```

Then build on the Jetson with colcon, and **vendor the generated output** into `vendored/cogidrone_msgs/` exactly like `px4_msgs`:

```bash
cd ~/cogidrone_msgs_ws
colcon build --packages-select cogidrone_msgs
# copy install/cogidrone_msgs/{include,lib,share} → vendored/cogidrone_msgs/
```

Then in `CMakeLists.txt`, add the include dir and link the typesupport libs — same pattern as `px4_msgs`.

**Cost:** every message change requires a Jetson round-trip (edit `.msg` → colcon build → re-vendor). For one enum, that's overkill. For a growing set of custom messages (fused detections, mission commands, telemetry), it's the right investment.

