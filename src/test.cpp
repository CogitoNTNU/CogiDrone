// #include "rclcpp/rclcpp.hpp"
// #include "sensor_msgs/msg/image.hpp"
// #include "std_msgs/msg/string.hpp"
// #include <format>
// #include <vector>

// class TestNode : public rclcpp::Node {
// public:
//     TestNode() : Node("test") {
//         auto sub = create_subscription<sensor_msgs::msg::Image>(
//             "/camera/image",
//             rclcpp::SensorDataQoS(),
//             [this](const sensor_msgs::msg::Image& msg) { (void)msg; });
//         (void)sub;
//         RCLCPP_INFO(get_logger(), "ok");
//     }
// };


// #include <iostream>


// [[nodiscrard]] constexpr double sqrt(const double x) {
//     if (x < 0) [[unlikely]] {
//         return -1;
//     }
//     if (x == 0)  [[unlikely]] {
//         return 0;
//     }


//     double guess = x / 2;

//     for (int i = 0; i < 10; ++i) {
//         guess = (guess + x / guess) / 2;
//     }

//     return guess;
// }


// int main() {
//     constexpr double x = 2.0;

//     double result = 0;

//     for (int i = 0; i < 10000000000000; ++i) {
//         // std::cout << "The square root of " << x << " is approximately " << sqrt(x) << "\n";
//         result *= sqrt(x);
//     }

//     std::cout << "The square root of " << x << " is approximately " << result << "\n";

//     return 0;

// }




#include <iostream>



int main() {
    const double result = 238794.324;
    std::cout << "The result is: " << result << "\n";
}