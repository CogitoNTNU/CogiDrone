#include "perception.h"

#include <string_view>



Perception::~Perception() = default;

std::expected<std::unique_ptr<Perception>, cogidrone::Error> Perception::create() {
    using namespace std::literals::string_view_literals;                                // ? for "sv" suffix
    
    // * Failable work: Model loads
    // Person model
    auto personModel = Model::load<Person>("Person model load failed!"sv);
    if (!personModel) return std::unexpected(std::move(personModel.error()));
    cogidrone::log(LOGGING_LEVEL::INFO, "Person model loaded successfully");

    // Head model
    auto headModel = Model::load<Head>("Head model load failed!"sv);
    if (!headModel) return std::unexpected(std::move(headModel.error()));
    cogidrone::log(LOGGING_LEVEL::INFO, "Head model loaded successfully");

    // DepthAnything model
    auto depthAnythingModel = Model::load<DepthAnything>("DepthAnything model load failed!"sv);
    if (!depthAnythingModel) return std::unexpected(std::move(depthAnythingModel.error()));
    cogidrone::log(LOGGING_LEVEL::INFO, "DepthAnything model loaded successfully");

    // * ROS2
    // Node
    auto node = std::make_shared<rclcpp::Node>(
        "perception",
        rclcpp::NodeOptions().use_intra_process_comms(true)
    );
    cogidrone::log(LOGGING_LEVEL::INFO, "ROS2 IPC node created successfully");


    // ! NOTE:
    // ! As much as this pains me, I have to initialize a perception object here to avoid insane workarounds regarding 
    // ! the lambda capture of the subscription callback. This is because the subscription callback needs to capture the 
    // ! perception object in order to invoke it's member methods without copying. Therefore, since i have to use a two-
    // ! step construction process anyway, it is better to initialize the object here and call a `wire()` method to connect 
    // ! the subscribers and publishers, rather than relying on the caller to do it (the Drone) manually.
    // * Partial assembly
    auto p = std::unique_ptr<Perception>(
        new Perception(M{                                                               // ? Transfer ownership into unique_ptr as ctor is private
            // ROS2
            .node = std::move(node),
            .imageSubscription = nullptr,                                               // ? will be wired later
            .targetPublisher = nullptr,                                                 // ? will be wired later
            
            // Models
            .personModel = std::move(*personModel),
            .headModel = std::move(*headModel),
            .depthAnythingModel = std::move(*depthAnythingModel)
        })
    );
    cogidrone::log(LOGGING_LEVEL::INFO, "Perception object partially constructed");

    // Wire the node
    p->wire();                                      
    cogidrone::log(LOGGING_LEVEL::INFO, "Perception object wired successfully");

    // * Return the fully constructed Perception object
    return p;
}

void Perception::wire() noexcept {
    m.imageSubscription = m.node->create_subscription<sensor_msgs::msg::Image>(
        "/camera/camera/image", 
        rclcpp::SensorDataQoS(),
        [this](const sensor_msgs::msg::Image& msg) {                                    // ! This ONLY works if the Perception object is non-movable. Since the rvalue
            this->onFrame(msg);                                                         // ! from the Perception::create() function is moved, capturing `this` here
        }                                                                               // ! would therefore cause a dangling pointer as the `this` adress would be 
    );                                                                                  // ! invalid. Fix: make perception non-movable and owned via a unique_ptr

    // TODO: Update the publisher to publish fused detections - for this, I'll have to do some research
    m.targetPublisher = m.node->create_publisher<vision_msgs::msg::Detection2DArray>(
        "/perception/detections",
        rclcpp::QoS(10)
    );
}

// * Callbacks
void Perception::onFrame(const sensor_msgs::msg::Image& msg) {
    // TODO: Implement a toFrame function to convert the image to the correct frame for each model
    const auto PEOPLE    = m.personModel.detect(/* toFrame */(msg));
    const auto HEADS     = m.headModel.detect(/* toFrame */(msg));
    const auto DISTANCES = m.depthAnythingModel.distance(/* toFrame */(msg));

    // ? These fused detections are NOT ment to be published yet. FusedDetections should just bundle every
    // ? models' output together for each frame (each fusedDetection contains all people, all heads, and
    // ? the depth heatmap image). We then have to calculate the 3D position of each detected person and head
    // ? using the depth heatmap image, AND THEN publish the final fused detections. IMPORTANT: PERCEPTION
    // ? SHOULD PUBLISH AN EMPTY DETECTION IF NO PEOPLE ARE DETECTED.
    const auto FUSED = fuse(/* PEOPLE, HEADS, DISTANCES */);

    // ! Publish UNCONDITIONALLY. An empty array is the "no person in this frame"
    // ! signal Navigation's Policy A depends on, AND the heartbeat that keeps
    // ! Navigation's watchdog from declaring Perception dead. Skipping the
    // ! publish when nothing was found is indistinguishable from a crash.
    m.targetPublisher->publish(toDetectionArray(FUSED));
}

// * Helpers
[[nodiscard]] Model::FusedDetections Perception::fuse(/* const ref something */) const {
    // TODO: real fusion once the models return detections instead of `bool`.
    return Model::FusedDetections{};
}

[[nodiscard]] std::unique_ptr<vision_msgs::msg::Detection2DArray>
Perception::toDetectionArray(const Model::FusedDetections& fused) const {
    auto out = std::make_unique<vision_msgs::msg::Detection2DArray>();                  // ? unique_ptr publish: zero-copy intra-process

    // TODO: for each fused person, append a Detection2D with:
    //     :   results[0].hypothesis.class_id = "person"
    //     :   results[0].hypothesis.score    = confidence
    //     :   results[0].pose.pose.position  = camera-frame XYZ (needs depth + intrinsics)
    //     :   bbox                           = pixel box
    // ? Until then this returns an empty array, which is the correct
    // ? "no person detected" message - not a placeholder.
    (void)fused;
    return out;
}