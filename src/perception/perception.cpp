#include "perception.h"

#include <string_view>


Perception::~Perception() = default;

std::expected<std::unique_ptr<Perception>, cogidrone::Error> Perception::create() {
    using namespace std::literals::string_view_literals;                                // ? for "sv" suffix
    
    // * Failable work: Model loads
    // Person model
    auto personModel = Model::load<Person>("Person model load failed!"sv);
    if (!personModel) return std::unexpected(std::move(personModel.error()));

    // Head model
    auto headModel = Model::load<Head>("Head model load failed!"sv);
    if (!headModel) return std::unexpected(std::move(headModel.error()));

    // DepthAnything model
    auto depthAnythingModel = Model::load<DepthAnything>("DepthAnything model load failed!"sv);
    if (!depthAnythingModel) return std::unexpected(std::move(depthAnythingModel.error()));

    // * ROS2
    // Node
    auto node = std::make_shared<rclcpp::Node>(
        "perception",
        rclcpp::NodeOptions().use_intra_process_comms(true)
    );

    // ! NOTE:
    // ! As much as this pains me, I have to initialize a perception object here to avoid insane workarounds regarding 
    // ! the lambda capture of the subscription callback. This is because the subscription callback needs to capture the 
    // ! perception object in order to invoke it's member methods without copying. Therefore, since i have to use a two-
    // ! step construction process anyway, it is better to initialize the object here and call a `wire()` method to connect 
    // ! the subscribers and publishers, rather than relying on the caller to do it (the Drone) manually.
    // * Partial assembly
    auto p = std::unique_ptr<Perception>(
        new Perception(M{                                                                   // ? Transfer ownership into unique_ptr as ctor is private
            // ROS2
            .node = std::move(node),
            .imageSubscription = nullptr,                                                   // ? will be wired later
            .targetPublisher = nullptr,                                                     // ? will be wired later
            
            // Models
            .personModel = std::move(*personModel),
            .headModel = std::move(*headModel),
            .depthAnythingModel = std::move(*depthAnythingModel)
        })
    );

    // Wire the node
    p->wire();

    // * Return the fully constructed Perception object
    return p;
}

void Perception::wire() noexcept {
    m.imageSubscription = m.node->create_subscription<sensor_msgs::msg::Image>(
        "/camera/image", 
        rclcpp::SensorDataQoS(),
        [this](const sensor_msgs::msg::Image& msg) {                                    // ! This doesn't work. Capturing `this` here causes a dangling pointer 
            this->onFrame(msg);                                                         // ! due to the fact that perception is movable, and get's moved twice 
        }                                                                               // ! in `drone.cpp`. Fix: make perception non-movable and owned via unique_ptr
    );

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

    auto fusedDetections = /*Model::*/ fuse(/* people, heads, distances */);

    // TODO: This publish call SHOULD work seamlessly once the above mentioned TODOs are implemented
    /* m.targetPublisher->publish(std::move(fusedDetections)); */
}


[[nodiscard]] std::unique_ptr<Model::FusedDetections> Perception::fuse(/* const ref something */) {
    return std::make_unique<Model::FusedDetections>();
}
