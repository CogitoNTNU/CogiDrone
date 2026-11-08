#pragma once

#include <expected>
#include <memory>
#include <cstddef>

#include <opencv2/core.hpp>


// * Depth Anything V2
// ? Relative monocular depth. NOT used for person distance - the follow mission
// ? uses pinhole geometry from the head/body box height, which is more accurate
// ? and needs no third inference per frame.
// !
// ! WHAT IT IS FOR: relative depth of NON-PERSON objects, which is the raw
// ! material for Representation's obstacle map ([#18]). On a RealSense the
// ! hardware depth stream replaces it entirely.
// !
// ! DEFERRED: [#40]. The framework is in place so the wiring does not have to
// ! change when the implementation lands - same pattern as `representation`
// ! being a nullptr_t in Drone::M.
class DepthAnything {
private:
    struct M { 
        // TODO: [#40] TensorRT engine for Depth-Anything-V2-Small.
        //     : Same TrtEngine as the YOLO models - the plan is built from
        //     : depth-anything-v2-small.onnx with trtexec on the Jetson.
        //     : Input  [1, 3, 518, 518] float32, RGB, normalised with the
        //     :        mean/std from the model card.
        //     : Output [1, 1, 518, 518] float32, RELATIVE inverse depth.
        //     :        Not metres - see the note on distance().
        // TrtEngine engine;
        std::nullptr_t engine = nullptr;
    } m;

    explicit DepthAnything(M&& m) : m(std::move(m)) {}

public:
    enum class InitError { 
        ModelLoadFailed 
    };

    // Simplified factory: returns by value so the model is movable, because it's a value type
    // and the engine handle is moved, not copied. This makes it unobservable.
    [[nodiscard]] static std::expected<DepthAnything, InitError> create();

    ~DepthAnything() = default;                                                     // members clean up (engine handle)

    DepthAnything(DepthAnything&&) noexcept = default;                              // movable - engine handle transfers
    DepthAnything& operator=(DepthAnything&&) noexcept = default;                                 
    DepthAnything(const DepthAnything&) = delete;                                   // still non-copyable (one engine)
    DepthAnything& operator=(const DepthAnything&) = delete;

    // TODO: [#40] Relative depth map, same size as the input frame.
    //     : Pure computation: frame in, depth map out.
    //     : RELATIVE, not metric. Depth Anything V2 predicts inverse depth
    //     : normalised per image, so values are only comparable WITHIN one
    //     : frame. Converting to metres needs a scale reference - the person's
    //     : pinhole distance is the obvious one.
    // [[nodiscard]] cv::Mat distance(const cv::Mat& frame);
    // ! `const auto&`, not `auto`: an abbreviated template parameter deduced by
    // ! value copies the whole frame on every call.
    [[nodiscard]] bool distance(const auto& frame) {
        (void)frame;                                                                    // Suppress unused parameter warning
        return true;                                                                    // | DEBUG: Stub implementation
    }
};