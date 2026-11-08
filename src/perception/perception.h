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

#include "logging.h"



// ? Loaded ONCE in create() from the parameter server (defaults from
// ? PerceptionParams). Override at startup with
// ? `ros2 param set /perception image_topic <topic>` BEFORE the node spins.
// ? NOT retuned live: there is no add_on_set_parameters_callback.
struct PerceptionParams {
    // ? realsense2_camera's default colour stream. Change this for a bag, a
    // ? second camera, or SITL - the topic name is deployment-specific, which
    // ? is exactly why it is a parameter and not a constant.
    std::string imageTopic = "/camera/camera/color/image_raw";
};


class Perception {
private:   
    struct M {                                                                          // Assembly struct
        // ROS node
        rclcpp::Node::SharedPtr node;

        // Subscriptions
        rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr imageSubscription;     // Subscription for camera images
        
        // Publishers
        // ? CONTRACT: this topic carries PERSON detections only. Navigation takes
        // ? the highest score without inspecting `class_id`, so anything else
        // ? published here will be followed. Heads go on their own topic.
        // ?
        // ? WIRE FORMAT: `vision_msgs/msg/Detection2DArray`. Every value the
        // ? pipeline produces already has a home:
        // ?   bbox                            -> the pixel box
        // ?   results[0].hypothesis.score     -> confidence
        // ?   results[0].hypothesis.class_id  -> always "person"
        // ?   results[0].pose.pose.position   -> camera-frame XYZ, metres
        // ?   id                              -> the pose class ("walking", ...)
        // ? A custom message becomes worth it only when we need something this
        // ? cannot express - a track ID, a covariance, or a typed pose enum.
        rclcpp::Publisher<vision_msgs::msg::Detection2DArray>::SharedPtr targetPublisher;
                
        // Models
        Person         personModel;
        Head           headModel;
        DepthAnything  depthAnythingModel;

        // Parameters
        PerceptionParams params;                                                        // ? Loaded once in create(); not retuned at runtime
    } m;

    // * Default ctor
    explicit Perception(M&& m) : m(std::move(m)) {}                                     // No default ctor exposed;  
    void wire() noexcept;                                                               // Wire the node's subscriptions and publishers to the member methods

    // * Callbacks
    void onFrame(const sensor_msgs::msg::Image& msg);                                   // Callback for camera images

    // * Helpers
    // ? Two stages, deliberately separate:
    // ?   fuse()             - pure, no ROS types. Bundles every model's output
    // ?                        for one frame. Testable without a ROS graph.
    // ?   toDetectionArray() - the ROS boundary. Converts the bundle into the
    // ?                        wire format Navigation consumes.
    [[nodiscard]] Model::FusedDetections fuse(/* const ref something */) const;
    [[nodiscard]] std::unique_ptr<vision_msgs::msg::Detection2DArray> 
    toDetectionArray(const Model::FusedDetections& fused) const;
    
public:
    // * Ctors & dtor
    [[nodiscard]] static std::expected<std::unique_ptr<Perception>, cogidrone::Error> create();

    ~Perception();

    Perception(Perception&&) noexcept = delete;                                         // ? non-movable, otherwise it causes dangling
    Perception& operator=(Perception&&) noexcept = delete;                              // ? [this] pointers in subscription callbacks
    Perception(const Perception&) = delete;                                             // non-copyable (one node, one camera)
    Perception& operator=(const Perception&) = delete;

    // * Accessors
    [[nodiscard]] inline rclcpp::Node::SharedPtr node() const noexcept { return m.node; }
    
    
};
