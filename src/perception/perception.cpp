#include "perception.h"

#include <string_view>
#include <algorithm>

#include <opencv2/imgproc.hpp>


Perception::~Perception() = default;

std::expected<std::unique_ptr<Perception>, cogidrone::Error> Perception::create() {
    using namespace std::literals::string_view_literals;                                // ? for "sv" suffix
    
    // * Failable work: Model loads
    auto personModel = Model::load<Person>("Person model load failed!"sv);
    if (!personModel) return std::unexpected(std::move(personModel.error()));
    cogidrone::log(LOGGING_LEVEL::INFO, "Person model loaded successfully");

    auto headModel = Model::load<Head>("Head model load failed!"sv);
    if (!headModel) return std::unexpected(std::move(headModel.error()));
    cogidrone::log(LOGGING_LEVEL::INFO, "Head model loaded successfully");

    // ? DepthAnything is deferred - see [#40]. It is not used for person
    // ? distance, so the follow mission does not need it.
    auto depthAnythingModel = Model::load<DepthAnything>("DepthAnything model load failed!"sv);
    if (!depthAnythingModel) return std::unexpected(std::move(depthAnythingModel.error()));
    cogidrone::log(LOGGING_LEVEL::INFO, "DepthAnything model loaded successfully");

    // * ROS2
    auto node = std::make_shared<rclcpp::Node>(
        "perception",
        rclcpp::NodeOptions().use_intra_process_comms(true)
    );
    cogidrone::log(LOGGING_LEVEL::INFO, "ROS2 IPC node created successfully");


    // * Parameters (non-failable: defaults live in PerceptionParams)
    PerceptionParams params;
    params.imageTopic         = node->declare_parameter("image_topic", params.imageTopic);
    params.cameraInfoTopic    = node->declare_parameter("camera_info_topic", params.cameraInfoTopic);
    params.averageHumanHeight = node->declare_parameter("average_human_height", params.averageHumanHeight);
    params.averageHeadHeight  = node->declare_parameter("average_head_height", params.averageHeadHeight);

    cogidrone::log(LOGGING_LEVEL::INFO, "Perception parameters initialized successfully");


    // ! NOTE:
    // ! As much as this pains me, I have to initialize a perception object here to avoid insane workarounds regarding 
    // ! the lambda capture of the subscription callback. This is because the subscription callback needs to capture the 
    // ! perception object in order to invoke it's member methods without copying. Therefore, since i have to use a two-
    // ! step construction process anyway, it is better to initialize the object here and call a `wire()` method to connect 
    // ! the subscribers and publishers, rather than relying on the caller (the Drone) to do it manually.
    // * Partial assembly
    auto p = std::unique_ptr<Perception>(
        new Perception(M{                                                               // ? Transfer ownership into unique_ptr as ctor is private
            // ROS2
            .node = std::move(node),
            .imageSubscription = nullptr,                                               // ? will be wired later
            .cameraInfoSubscription = nullptr,                                          // ? will be wired later
            .targetPublisher = nullptr,                                                 // ? will be wired later
            // Models
            .personModel = std::move(*personModel),
            .headModel = std::move(*headModel),
            .depthAnythingModel = std::move(*depthAnythingModel),
            // Parameters
            .params = std::move(params)
        })
    );
    cogidrone::log(LOGGING_LEVEL::INFO, "Perception object partially constructed");

    p->wire();                                      
    cogidrone::log(LOGGING_LEVEL::INFO, "Perception object wired successfully");

    return p;
}

void Perception::wire() noexcept {
    m.imageSubscription = m.node->create_subscription<sensor_msgs::msg::Image>(
        m.params.imageTopic,
        rclcpp::SensorDataQoS(),
        [this](const sensor_msgs::msg::Image& msg) {                                    // ! This ONLY works if the Perception object is non-movable. Since the rvalue
            this->onFrame(msg);                                                         // ! from the Perception::create() function is moved, capturing `this` here
        }                                                                               // ! would therefore cause a dangling pointer as the `this` adress would be 
    );                                                                                  // ! invalid. Fix: make perception non-movable and owned via a unique_ptr

    // ? Intrinsics are latched: they are published once and never change for a
    // ? fixed camera configuration. `transient_local` means a late-joining
    // ? Perception still receives the last one.
    m.cameraInfoSubscription = m.node->create_subscription<sensor_msgs::msg::CameraInfo>(
        m.params.cameraInfoTopic,
        rclcpp::QoS(1).transient_local(),
        [this](const sensor_msgs::msg::CameraInfo& msg) {
            this->onCameraInfo(msg);
        }
    );

    m.targetPublisher = m.node->create_publisher<vision_msgs::msg::Detection2DArray>(
        "/perception/detections",
        rclcpp::QoS(10)
    );
}


// * Callbacks
void Perception::onCameraInfo(const sensor_msgs::msg::CameraInfo& msg) {
    // TODO: [#42] Address magic numbers/strings
    // ? K is row-major 3x3: [fx 0 cx; 0 fy cy; 0 0 1].
    // ? fx and fy are equal for a square-pixel sensor, but read both and use
    // ? fx for horizontal and fy for vertical if they ever diverge.
    focalLength = static_cast<float>(msg.k[0]);
    principalX  = static_cast<float>(msg.k[2]);
    principalY  = static_cast<float>(msg.k[5]);

    cogidrone::log(LOGGING_LEVEL::INFO, "Camera intrinsics received");
}

void Perception::onFrame(const sensor_msgs::msg::Image& msg) {
    // ! No intrinsics yet: every distance would be 0, and Navigation would
    // ! follow a person who is always at the camera. Refuse to publish.
    // ! This is NOT a crash - the watchdog in Navigation will see silence and
    // ! hold position, which is the correct behaviour.
    if (focalLength <= 0.0f) {
        if (!intrinsicsMissingLogged) {
            intrinsicsMissingLogged = true;
            cogidrone::log(LOGGING_LEVEL::WARNING,
                "No camera intrinsics yet - not publishing detections");
        }
        return;
    }

    const cv::Mat FRAME = toFrame(msg);
    if (FRAME.empty()) return;

    // ? Write into the member scratch, not a local. A local would be destroyed
    // ? and reallocated every frame. See [#41].
    fuse(FRAME, fusedScratch);

    // ! Publish UNCONDITIONALLY. An empty array is the "no person in this frame"
    // ! signal Navigation's Policy A depends on, AND the heartbeat that keeps
    // ! Navigation's watchdog from declaring Perception dead. Skipping the
    // ! publish when nothing was found is indistinguishable from a crash.
    m.targetPublisher->publish(toDetectionArray(fusedScratch));
}


// * Helpers
cv::Mat Perception::toFrame(const sensor_msgs::msg::Image& msg) const {
    // ? sensor_msgs::Image is a raw byte buffer with a stride. OpenCV needs a
    // ? Mat that wraps it without copying, then a clone because the message is
    // ? released when the callback returns.
    // ! `step` is NOT always width * channels - the publisher may pad rows.
    // ! Using width * channels directly produces a sheared image.
    const bool IS_RGB = (msg.encoding == "rgb8");
    const bool IS_BGR = (msg.encoding == "bgr8");

    if (!IS_RGB && !IS_BGR) {
        cogidrone::log(LOGGING_LEVEL::WARNING, "Unsupported image encoding");
        return {};
    }

    cv::Mat wrapped(
        static_cast<int>(msg.height),
        static_cast<int>(msg.width),
        CV_8UC3,
        const_cast<uint8_t*>(msg.data.data()),                                      // ? OpenCV's API predates const-correctness
        static_cast<std::size_t>(msg.step)
    );

    cv::Mat frame = wrapped.clone();

    // ? The models were trained on BGR (OpenCV default). realsense2_camera
    // ? publishes rgb8 by default, so convert.
    if (IS_RGB) {
        cv::cvtColor(frame, frame, cv::COLOR_RGB2BGR);
    }

    return frame;
}

void Perception::fuse(const cv::Mat& frame, std::vector<Model::Detection>& out) {
    // TODO: [#41] Reduce function call overhead by passing a preallocated  
    //     : output vector to fill, or a pointer to the output buffer. 
    // TODO: [#42] Address magic numbers/strings
    // TODO: [#43] Use `emplace_back()` instead of `push_back()` to avoid a copy.
    out.clear();

    // * 1. Persons on the full frame
    m.personModel.detect(frame, personsScratch);

    for (const Model::Detection& person : personsScratch) {
        // * 2. Head on the person crop
        // ? Clamp the crop to the frame: a box can extend past the edge when a
        // ? person is partially out of view.
        const int X1 = std::max(0, static_cast<int>(person.bbox[0]));
        const int Y1 = std::max(0, static_cast<int>(person.bbox[1]));
        const int X2 = std::min(frame.cols, static_cast<int>(person.bbox[2]));
        const int Y2 = std::min(frame.rows, static_cast<int>(person.bbox[3]));

        if (X2 <= X1 || Y2 <= Y1) continue;                                         // ? Degenerate box

        const cv::Mat CROP = frame(cv::Rect(X1, Y1, X2 - X1, Y2 - Y1));
        m.headModel.detect(CROP, headsScratch);

        // * 3. Distance: head preferred, body fallback
        // ? This is the whole reason the head model exists. A head is ~23 cm
        // ? wide, so its pixel height gives a much better distance than the
        // ? body's, which assumes a 1.70 m person regardless of who they are.
        // ? See prøblems.txt: body is "whack" at range, head is good to ~15 m.
        float distance = 0.0f;

        if (!headsScratch.empty()) {
            // ? Highest-confidence head, matching the Python prototype.
            const auto BEST_HEAD = std::max_element(
                headsScratch.begin(), headsScratch.end(),
                [](const Model::Detection& a, const Model::Detection& b) {
                    return a.confidence < b.confidence;
                }
            );

            const float HEAD_PIXEL_HEIGHT = BEST_HEAD->bbox[3] - BEST_HEAD->bbox[1];
            distance = distanceFromPixelHeight(m.params.averageHeadHeight, HEAD_PIXEL_HEIGHT);
        }

        if (distance <= 0.0f) {
            const float BODY_PIXEL_HEIGHT = person.bbox[3] - person.bbox[1];
            distance = distanceFromPixelHeight(m.params.averageHumanHeight, BODY_PIXEL_HEIGHT);
        }

        if (distance <= 0.0f) continue;                                             // ? No usable height

        // * 4. Camera-frame position
        // ? The box centre's pixel offset from the principal point, scaled by
        // ? distance / focalLength, gives the lateral offset in metres. This is
        // ? `offset_in_meters()` from depth_head.py.
        const float CENTER_X = (person.bbox[0] + person.bbox[2]) / 2.0f;
        const float CENTER_Y = (person.bbox[1] + person.bbox[3]) / 2.0f;

        // TODO: [#43] Use `emplace_back()` instead of `push_back()` to avoid 
        //     : temporary construction of the hypothesis object. This is a copy, 
        //     : and we can avoid it by constructing the object in place.
        // ? Copy, not move: `person` is a const& into personsScratch, so it
        // ? cannot be mutated and must not be. The copy is the mutable working
        // ? value that gets `positionCamera` filled in before being moved into
        // ? the result.
        Model::Detection fusedPerson = person;
        fusedPerson.positionCamera = cogidrone::Vec3{
            (CENTER_X - principalX) * distance / focalLength,                       // x: right
            (CENTER_Y - principalY) * distance / focalLength,                       // y: down
            distance                                                                // z: forward
        };

        out.push_back(std::move(fusedPerson));
    }
}

std::unique_ptr<vision_msgs::msg::Detection2DArray>
Perception::toDetectionArray(const std::vector<Model::Detection>& fused) const {
    // TODO: [#42] Address magic numbers/strings
    // TODO: [#43] Use `emplace_back()` instead of `push_back()` to avoid a copy.
    auto out = std::make_unique<vision_msgs::msg::Detection2DArray>();              // ? unique_ptr publish: zero-copy intra-process

    // ? One allocation for the message itself is unavoidable: `publish()` takes
    // ? ownership of the unique_ptr. But the inner vector can be sized once
    // ? instead of growing.
    out->detections.reserve(fused.size());

    for (const Model::Detection& person : fused) {
        vision_msgs::msg::Detection2D detection;

        // ? bbox is centre + size, not corners.
        detection.bbox.center.position.x = (person.bbox[0] + person.bbox[2]) / 2.0;
        detection.bbox.center.position.y = (person.bbox[1] + person.bbox[3]) / 2.0;
        detection.bbox.size_x = person.bbox[2] - person.bbox[0];
        detection.bbox.size_y = person.bbox[3] - person.bbox[1];

        // TODO: [#43] Use `emplace_back()` instead of `push_back()` to avoid 
        //     : temporary construction of the hypothesis object. This is a copy, 
        //     : and we can avoid it by constructing the object in place.
        vision_msgs::msg::ObjectHypothesisWithPose hypothesis;
        hypothesis.hypothesis.class_id = "person";                                  // ? CONTRACT: always "person"
        hypothesis.hypothesis.score = person.confidence;

        // ? Camera-frame position. Navigation converts to world NED via
        // ? cogidrone::cameraToWorld() - see math/transform.h and [#32].
        hypothesis.pose.pose.position.x = person.positionCamera.x;
        hypothesis.pose.pose.position.y = person.positionCamera.y;
        hypothesis.pose.pose.position.z = person.positionCamera.z;

        detection.results.push_back(std::move(hypothesis));

        // ? The pose class travels in `id`, not `class_id`, so Navigation can
        // ? use it later without breaking the "person only" contract.
        const auto CLASS_NAMES = Person::classNames();
        if (person.classId >= 0 && static_cast<std::size_t>(person.classId) < CLASS_NAMES.size()) {
            detection.id = std::string(CLASS_NAMES[static_cast<std::size_t>(person.classId)]);
        }

        out->detections.push_back(std::move(detection));
    }

    return out;
}


// * Distance
float Perception::distanceFromPixelHeight(float realHeight, float pixelHeight) const {
    // ? Pinhole: an object of known real height h at distance d projects to
    // ? h * f / d pixels. Inverting gives d = h * f / pixelHeight.
    // ! A zero or negative pixel height means the box is degenerate - return 0
    // ! so the caller falls through to the other estimate.
    if (pixelHeight <= 0.0f) return 0.0f;
    return (realHeight * focalLength) / pixelHeight;
}