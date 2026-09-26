#pragma once

#include <expected>
#include <memory>

#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/image.hpp"
#include "std_msgs/msg/string.hpp"

#include "perception/models/person.h"
#include "perception/models/head.h"
#include "perception/models/depth_anything.h"



namespace perception {
    struct Detection {
        float x, y, w, h;
        float confidence;
        int class_id;
    };
};

class Perception {
private:   
    struct M {                                                                          // Assembly struct
        // ROS node
        rclcpp::Node::SharedPtr node;

        // Subscriptions and publishers
        rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr imageSubscription;     // Subscription for camera images
        rclcpp::Publisher<cogidrone::msg::FusedDetections>::SharedPtr targetPublisher;  // Publisher for fused target detections
                
        // Models
        std::unique_ptr<Person> personModel;
        // std::unique_ptr<Head> headModel;
        // std::unique_ptr<DepthAnything> depthAnythingModel;
    } m;

    explicit Perception(M&& m);    

public:
    enum class InitError {
        CameraNotFound,
        ModelLoadFailed,
    };
    
    
    // * Ctors & dtor
    [[nodiscard]] static std::expected<std::unique_ptr<Perception>, InitError> create();

    ~Perception();
    Perception(const Perception&) = delete;
    Perception& operator=(const Perception&) = delete;
    Perception(Perception&&) = delete;
    Perception& operator=(Perception&&) = delete;

    // * Accessors
    [[nodiscard]] inline rclcpp::Node::SharedPtr node() const noexcept { return m.node; }
    
    
};
