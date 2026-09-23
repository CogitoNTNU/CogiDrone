#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/image.hpp"
#include "std_msgs/msg/string.hpp"
#include <format>
#include <vector>

class TestNode : public rclcpp::Node {
public:
    TestNode() : Node("test") {
        auto sub = create_subscription<sensor_msgs::msg::Image>(
            "/camera/image",
            rclcpp::SensorDataQoS(),
            [this](const sensor_msgs::msg::Image& msg) { (void)msg; });
        (void)sub;
        RCLCPP_INFO(get_logger(), "ok");
    }
};