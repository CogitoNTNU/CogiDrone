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
        "/camera/image", 
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

void Perception::onFrame(const sensor_msgs::msg::Image& msg) {
    // TODO: Implement a toFrame function to convert the image to the correct frame for each model
    auto people = m.personModel.detect(/* toFrame */(msg));
    auto heads = m.headModel.detect(/* toFrame */(msg));
    auto distances = m.depthAnythingModel.distance(/* toFrame */(msg));

    // ? These fused detections are NOT ment to be published yet. FusedDetections should just bundle every
    // ? models' output together for each frame (each fusedDetection contains all people, all heads, and
    // ? the depth heatmap image). We then have to calculate the 3D position of each detected person and head
    // ? using the depth heatmap image, AND THEN publish the final fused detections. 
    auto fusedDetections = /*Model::*/ fuse(/* people, heads, distances */);

    // TODO: This publish call SHOULD work seamlessly once the above mentioned TODOs are implemented
    /* m.targetPublisher->publish(std::move(fusedDetections)); */
}


[[nodiscard]] std::unique_ptr<Model::FusedDetections> Perception::fuse(/* const ref something */) {
    return std::make_unique<Model::FusedDetections>();
}
