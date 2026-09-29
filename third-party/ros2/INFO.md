# Filestructure
```sh
third-party/ros2/
├── opt/    ← the ROS 2 Jazzy install tree (from packages.ros.org)
├── usr/    ← a minimal Ubuntu arm64 "sysroot" (from ports.ubuntu.com)
└── debs/   ← the downloaded .deb cache
```

| Folder | Contents | Why it exists |
|---|---|---|
| `opt/ros/jazzy/` | `include/` - all ROS 2 headers, **including the generated message types** (`rclcpp/`, `sensor_msgs/`, `std_msgs/`...); `lib/` - arm64 ELF binaries (dead weight on Windows); `share/` - package metadata | The actual ROS 2 API you'll code against |
| `usr/` | `include/c++/13/` - libstdc++ headers (`std::vector`, `std::format`...); `include/aarch64-linux-gnu/c++/13/` - `bits/c++config.h` etc.; `lib/gcc/aarch64-linux-gnu/13/include/` - GCC internals (`stddef.h`, `stdarg.h`); `include/` - libc + Linux kernel headers | Without these, every `std::` and even `#include <cstddef>` squiggles red, because MSYS2's own headers are Windows-targeted |
| `debs/` | The raw `.deb` files | Just a cache - lets re-runs skip downloads. Safe to delete (`rm -rf debs`) |

Housekeeping: make sure `.gitignore` has `ros2` so none of this (~1 GB) gets committed.

## IntelliSense setup

Create `.vscode/c_cpp_properties.json` in the workspace root (requires the **C/C++ extension**, `ms-vscode.cpptools`):

```json
{
  "configurations": [
    {
      "name": "Jazzy arm64",
      "includePath": [
        "${workspaceFolder}/third-party/ros2/usr/include/c++/13",
        "${workspaceFolder}/third-party/ros2/usr/include/aarch64-linux-gnu/c++/13",
        "${workspaceFolder}/third-party/ros2/usr/lib/gcc/aarch64-linux-gnu/13/include",
        "${workspaceFolder}/third-party/ros2/usr/include",
        "${workspaceFolder}/third-party/ros2/usr/include/aarch64-linux-gnu",
        "${workspaceFolder}/third-party/ros2/opt/ros/jazzy/include/**",
        "${workspaceFolder}/src"
      ],
      "intelliSenseMode": "linux-gcc-arm64",
      "cppStandard": "c++20",
      "cStandard": "c17"
    }
  ],
  "version": 4
}
```

The order matters - it mirrors real GCC search order: libstdc++ → arch-specific C++ → GCC internals → libc → ROS. `${workspaceFolder}/src` at the end resolves your own headers (`perception/perception.h` etc.).

## Verify it works

Drop this into a scratch file and check for squiggles:

```cpp
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/image.hpp"
#include "std_msgs/msg/string.hpp"
#include <format>
#include <vector>

class TestNode : public rclcpp::Node {
public:
    TestNode() : Node("test") {
        auto sub = create_subscription<sensor_msgs::msg::Image>(
            "/camera/image", rclcpp::SensorDataQoS(),
            [this](const sensor_msgs::msg::Image& msg) { (void)msg; });
        (void)sub;
        RCLCPP_INFO(get_logger(), "{}", std::format("ok"));
    }
};
```

If everything resolves - hover over `rclcpp::Node`, `sensor_msgs::msg::Image` - you're done. Try **Ctrl+Space** inside `create_subscription<` to confirm autocomplete offers the message types.

## Notes

- **Use cpptools, not clangd, for this setup.** clangd only works from a `compile_commands.json`, which you can't generate on Windows (there's no real build here - that happens on the Jetson). cpptools' `includePath` approach is exactly right for a headers-only tree.
- **If `std::` types still squiggle**: check that `usr/include/aarch64-linux-gnu/c++/13/bits/c++config.h` exists - that file is the linchpin of the whole sysroot.
- **When you add ROS packages later** (e.g., `cv_bridge`, `image_transport`): add them to `ROS_PKGS` in `get.sh`, re-run (cached debs skip instantly), and IntelliSense picks them up automatically via the `/**` glob.
- **`px4_msgs` won't come from this script** - it's generated inside your colcon workspace on the Jetson. When you reach the uXRCE-DDS stage, copy `install/px4_msgs/include/` off the Jetson into `include`.
- Remember: this tree is **IntelliSense-only**. Compilation and linking happen on the Jetson (`colcon build`); the arm64 `.so` files in `opt/ros/jazzy/lib/` are inert on Windows and can be deleted if disk space matters.