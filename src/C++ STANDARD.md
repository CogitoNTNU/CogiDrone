# C++ standard
## Why C++23 might be bad
### ROS2 binaries
The ROS 2 Humble binaries on the Jetson were built with **GCC 11 in C++17** mode! Mixing the C++23 objects with their C++17-built libraries works only as long as we stay within the ABI - but C++23 headers (e.g., `std::expected`, `std::print`, `std::mdspan`) can pull in symbols that don't exist in the system's libstdc++ usage patterns, and any template-heavy ROS 2 header interaction gets fragile.

### GCC 11
GCC 11 on the Jetson doesn't fully implement C++23. If we ever build natively on the Jetson (TensorRT/CUDA work is painful to cross-compile), C++23 code may simply not compile.

### Ecosystem
Nothing in the ROS 2 ecosystem uses C++23 yet, so we'd be the first to hit every edge case.

## Practical recommendation
- Use C++20 for our own code if you want modern features (`std::span`, `std::ranges`, `concepts`, `std::format` via GCC 11 - note `std::format` needs GCC 13, so use `{fmt}` on the Jetson). GCC 11 handles C++20 well.
- Use C++17 if you want zero friction - that's what every ROS 2 Humble tutorial, vendor driver (realsense-ros), and Isaac ROS package assumes.
- Keep C++23 only for code that never touches ROS 2 - e.g., pure algorithm code you compile with your modern cross-compiler and link as a clean library.
