#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/image.hpp"
#include "std_msgs/msg/string.hpp"
#include "px4_msgs/msg/offboard_control_mode.hpp"
#include "px4_msgs/msg/vehicle_status.hpp"
#include <format>
#include <vector>

// ? This node is never instantiated - it is a build-time smoke test. But the ctor
// ? is deliberately defined OUT OF LINE below, not in the class body: a ctor defined
// ? in-class is implicitly `inline`, so with nothing ever instantiating TestNode it
// ? is never odr-used and the compiler emits no code for it at all. Nothing would
// ? then reference any message symbol, `-Wl,--as-needed` would drop every message
// ? .so, and this file would only prove the headers parse - not that the link line
// ? in CMakeLists.txt is correct. Out of line, the ctor is emitted unconditionally
// ? and a wrong/missing typesupport lib fails the build instead of the flight.
class TestNode : public rclcpp::Node {
public:
    TestNode();
};

TestNode::TestNode() : Node("test") {
    auto sub = create_subscription<sensor_msgs::msg::Image>(
        "/camera/image",
        rclcpp::SensorDataQoS(),
        [this](const sensor_msgs::msg::Image& msg) { (void)msg; });
    (void)sub;

    // ! PX4's uXRCE-DDS client publishes /fmu/out/* as BEST_EFFORT, KEEP_LAST(5),
    // ! VOLATILE. rclcpp's default subscription QoS is RELIABLE, which does not
    // ! match - the subscription then simply never connects, with no error and no
    // ! warning. Every /fmu/out/* subscription must therefore say this explicitly.
    rclcpp::QoS px4_qos(rclcpp::KeepLast(5));
    px4_qos.best_effort().durability_volatile();

    // RX: telemetry out of the flight controller
    auto status_sub = create_subscription<px4_msgs::msg::VehicleStatus>(
        "/fmu/out/vehicle_status",
        px4_qos,
        [this](const px4_msgs::msg::VehicleStatus& msg) { (void)msg; });
    (void)status_sub;

    // TX: /fmu/in/* is inbound to PX4, where the default reliable QoS is correct
    auto offboard_pub = create_publisher<px4_msgs::msg::OffboardControlMode>(
        "/fmu/in/offboard_control_mode", 10);
    (void)offboard_pub;

    RCLCPP_INFO(get_logger(), "ok");
}
