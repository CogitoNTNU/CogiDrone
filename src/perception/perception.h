#pragma once

#include <expected>
#include <memory>
#include <string>
#include <vector>
#include <span>

#include <opencv2/core.hpp>

#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/image.hpp"
#include "sensor_msgs/msg/camera_info.hpp"
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

    // ? Camera intrinsics. realsense2_camera publishes these alongside the
    // ? image; they are what turns a pixel offset into a bearing.
    // ! The Python prototype used `focal_length = width * 0.8`, which is a
    // ! guess. A wrong focal length is a wrong distance on EVERY detection, so
    // ? the calibrated value from CameraInfo is used instead. See [#38].
    std::string cameraInfoTopic = "/camera/camera/color/camera_info";

    // ? Assumed real-world heights, used by the pinhole distance estimate.
    // ? These are the two numbers the whole distance estimate rests on.
    float averageHumanHeight = 1.70f;                                                   // metres
    float averageHeadHeight  = 0.23f;                                                   // metres
};


class Perception {
private:   
    struct M {                                                                          // Assembly struct
        // ROS node
        rclcpp::Node::SharedPtr node;

        // Subscriptions
        rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr imageSubscription;           // Subscription for camera images
        rclcpp::Subscription<sensor_msgs::msg::CameraInfo>::SharedPtr cameraInfoSubscription; // Intrinsics, sent with every frame
        
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
        DepthAnything  depthAnythingModel;                                          // ? deferred - see [#40]

        // Parameters
        PerceptionParams params;                                                    // ? Loaded once in create(); not retuned at runtime
    } m;

    // * Runtime state
    // ? Written by onCameraInfo() (subscription thread), read by onFrame()
    // ? (subscription thread). Both are in the node's default MutuallyExclusive
    // ? group, so the executor serializes them - no mutex needed.
    // ! Until the first CameraInfo arrives, `focalLength` is 0 and every
    // ! distance would be 0. onFrame() refuses to publish until it is set.
    float focalLength = 0.0f;                                                       // pixels, from CameraInfo.K[0]
    float principalX = 0.0f;                                                        // pixels, from CameraInfo.K[2]
    float principalY = 0.0f;                                                        // pixels, from CameraInfo.K[5]

    // ? One-shot guard so the "no intrinsics" warning is logged once, not at
    // ? camera rate. A silent early return is a debugging trap: the node looks
    // ? alive but publishes nothing.
    bool intrinsicsMissingLogged = false;
    bool intrinsicsReceivedLogged = false;

    // * Frame scratch
    // ? Reused across frames. `clear()` keeps the capacity, so after the first
    // ? few frames none of these allocate. They are members rather than locals
    // ? because a local would be destroyed and reallocated every frame - which
    // ? is exactly the cost [#41] removes.
    // ?
    // ! `personsScratch` and `headsScratch` are separate on purpose: fuse()
    // ! iterates persons while writing heads, so they must not alias.
    std::vector<Model::Detection> personsScratch;
    std::vector<Model::Detection> headsScratch;
    std::vector<Model::Detection> fusedScratch;

    // * Default ctor
    explicit Perception(M&& m) : m(std::move(m)) {}                                     // No default ctor exposed;  
    void wire() noexcept;                                                               // Wire the node's subscriptions and publishers to the member methods

    // * Callbacks
    void onFrame(const sensor_msgs::msg::Image& msg);                                   // Callback for camera images
    void onCameraInfo(const sensor_msgs::msg::CameraInfo& msg);                         // Intrinsics

    // * Helpers
    // ? Three stages, deliberately separate:
    // ?   toFrame()           - ROS -> OpenCV. The only place that knows about
    // ?                        sensor_msgs.
    // ?   fuse()              - pure, no ROS types. Person boxes + head boxes +
    // ?                        pinhole distance -> camera-frame positions.
    // ?                        Testable without a ROS graph.
    // ?   toDetectionArray()  - the ROS boundary. Converts the bundle into the
    // ?                        wire format Navigation consumes.
    // ?
    // ! `fuse()` writes into `out` and clears it first. It does NOT return a
    // ! vector - that would allocate per frame. See [#41].
    // ?
    // ? `toDetectionArray()` takes a `std::span`, not a `const std::vector&`:
    // ? it only reads and iterates, never resizes. The span states that
    // ? contract, and makes the function callable with a `std::array` or a raw
    // ? buffer in a test.
    [[nodiscard]] cv::Mat toFrame(const sensor_msgs::msg::Image& msg) const;
    void fuse(const cv::Mat& frame, std::vector<Model::Detection>& out);
    [[nodiscard]] std::unique_ptr<vision_msgs::msg::Detection2DArray>
    toDetectionArray(std::span<const Model::Detection> fused) const;

    // * Distance
    // ? Pinhole geometry: a known real-world height and its pixel height give
    // ? the distance. This is the ONLY distance source for persons - the depth
    // ? map is not involved.
    // ?   distance = (realHeight * focalLength) / pixelHeight
    [[nodiscard]] float distanceFromPixelHeight(float realHeight, float pixelHeight) const;
    
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