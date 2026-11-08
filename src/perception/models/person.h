#pragma once

#include <expected>
#include <memory>
#include <string>
#include <vector>
#include <span>

#include <opencv2/core.hpp>

#include "perception/models/models.h"
#include "perception/models/yolo.h"


// * Person model
// ? Fine-tuned YOLO (models/best.pt -> best.onnx -> best.engine) on the 6 S&R
// ? classes:
// ?   running, walking, laying_down, human_not_defined, seated, stands
// ?
// ! EVERY CLASS IS A PERSON. The class id carries POSE, not "person vs
// ! not-person". Navigation's contract ("this topic carries PERSON detections
// ! only") therefore still holds, and the pose is available for later policy
// ! (e.g. `laying_down` -> injured -> priority target).
class Person {
private:
    struct M {                                                                          // Assembly struct
        Yolo yolo;
    } m;

    explicit Person(M&& m) : m(std::move(m)) {}

public:
    enum class InitError {
        ModelLoadFailed,
    };

    // Simplified factory: returns by value so person is movable, because it's a value type
    // and the engine handle is moved, not copied. This makes it unobservable.
    [[nodiscard]] static std::expected<Person, InitError> create();

    ~Person() = default;                                                                // members clean up (TensorRT engine)

    Person(Person&&) noexcept = default;                                                // movable - engine transfers
    Person& operator=(Person&&) noexcept = default;
    Person(const Person&) = delete;                                                     // still non-copyable (one engine)
    Person& operator=(const Person&) = delete;

    // * Inference
    // ! DOUBLE CHECK: return type
    // TODO: [#41] Reduce copy overhead by passing a preallocated
    //     : output vector to fill, or a pointer to the output buffer.
    // ? Returns person boxes in the ORIGINAL frame's pixel coordinates.
    // ? `positionCamera` is NOT filled here - distance needs the head pass,
    // ? which is Perception's job (see fuse()).
    [[nodiscard]] std::vector<Model::Detection> detect(const cv::Mat& frame);

    // * Class table
    [[nodiscard]] static std::span<const std::string_view> classNames() noexcept;
};