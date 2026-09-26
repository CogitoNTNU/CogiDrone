#include "perception.h"
#include <vision_msgs/msg/detection2_d_array.hpp>


std::expected<Perception, Perception::InitError> Perception::create() {
    // TODO: IMPLEMENT THIS
    // 1. Fallible work FIRST - models load before anything exists
    auto yolo = Yolo::create();
    if (!yolo) return std::unexpected(InitError::ModelLoadFailed);

    // 2. Wire the node - nothing here can fail
    _node = std::make_shared<rclcpp::Node>("perception");

    m.imageSubscription = m.node->create_subscription<sensor_msgs::msg::Image>(
        "/camera/image", rclcpp::SensorDataQoS(),
        [&m](const sensor_msgs::msg::Image& msg) {
            // NOTE: capturing &m by reference is WRONG here - see lifetime note below
        });

    m.targetPublisher = m.node->create_publisher<vision_msgs::msg::Detection2DArray>(
        "/cogidrone/detections", 10);

    // 3. Assemble - all pieces valid, one move
    return Perception(M{
        // ROS 2
        .node = std::move(m.node),
        .imageSubscription = std::move(m.imageSubscription),
        .targetPublisher = std::move(m.targetPublisher),
        .yolo = std::move(m.yolo),
    });
}

Perception::~Perception() = default;

void Perception::onFrame(const sensor_msgs::msg::Image& msg) {
    auto detections = m.yolo.detect(to_frame(msg));
    m.targetPublisher->publish(to_msg(detections));
}