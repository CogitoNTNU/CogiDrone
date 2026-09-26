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
        Person          personModel;
        Head            headModel;
        DepthAnything   depthAnythingModel;
    } m;

    // * Default ctor
    explicit Perception(M&& m) : m(std::move(m)) {}                                     // No default ctor exposed;    

    // * Callbacks
    void onFrame(const sensor_msgs::msg::Image& msg);

public:
    enum class InitError {
        CameraNotFound,
        ModelLoadFailed,
    };
    
    
    // * Ctors & dtor
    [[nodiscard]] static std::expected<Perception, InitError> create();

    ~Perception();

    Perception(Perception&&) noexcept = default;                                        // movable
    Perception& operator=(Perception&&) noexcept = default;

    Perception(const Perception&) = delete;                                             // non-copyable (one node, one camera)
    Perception& operator=(const Perception&) = delete;

    // * Accessors
    [[nodiscard]] inline rclcpp::Node::SharedPtr node() const noexcept { return m.node; }
    
    
};
