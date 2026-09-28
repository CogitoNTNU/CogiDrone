#pragma once

#include <expected>
#include <memory>

#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/image.hpp"
#include "std_msgs/msg/string.hpp"
#include "vision_msgs/msg/detection2_d_array.hpp"

#include "perception/models/models.h"
#include "perception/models/person.h"
#include "perception/models/head.h"
#include "perception/models/depth_anything.h"

#include "errors.h"



class Perception {
private:   
    struct M {                                                                          // Assembly struct
        // ROS node
        rclcpp::Node::SharedPtr node;

        // Subscriptions and publishers
        rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr imageSubscription;     // Subscription for camera images
        // TODO: Update the publisher to publish fused detections - for this, I'll have to do some research
        // rclcpp::Publisher<cogidrone::msg::FusedDetections>::SharedPtr targetPublisher;
        rclcpp::Publisher<vision_msgs::msg::Detection2DArray>::SharedPtr targetPublisher;
                
        // Models
        Person         personModel;
        Head           headModel;
        DepthAnything  depthAnythingModel;
    } m;

    // * Default ctor
    explicit Perception(M&& m) : m(std::move(m)) {}                                     // No default ctor exposed;  
    void wire() noexcept;                                                               // Wire the node's subscriptions and publishers to the member methods

    // * Callbacks
    void onFrame(const sensor_msgs::msg::Image& msg);                                   // Callback for camera images

    // * Helpers
    [[nodiscard]] std::unique_ptr<Model::FusedDetections>
    fuse(/* const ref something */);

public:
    // * Ctors & dtor
    [[nodiscard]] static std::expected<Perception, cogidrone::Error> create();

    ~Perception();

    Perception(Perception&&) noexcept = default;                                        // movable
    Perception& operator=(Perception&&) noexcept = default;

    Perception(const Perception&) = delete;                                             // non-copyable (one node, one camera)
    Perception& operator=(const Perception&) = delete;

    // * Accessors
    [[nodiscard]] inline rclcpp::Node::SharedPtr node() const noexcept { return m.node; }
    
    
};
