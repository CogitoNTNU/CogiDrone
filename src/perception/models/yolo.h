#pragma once

#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <span>

#include <opencv2/core.hpp>

#include "perception/models/models.h"
#include "perception/models/trt_engine.h"


// * YOLO
// ? Shared inference for the two YOLO models (person + head). Both are
// ? ultralytics exports with the same output layout, so the letterbox, decode,
// ? and NMS are shared; only the weights, class table, and thresholds differ.
// !
// ! OUTPUT LAYOUT (ultralytics ONNX export, no objectness, no NMS):
// !   [1, 4 + numClasses, numAnchors]   e.g. [1, 10, 8400] for 6 classes
// !   rows 0..3   = cx, cy, w, h   (pixels, in the LETTERBOXED 640x640 space)
// !   rows 4..    = per-class scores (already sigmoided)
// ! VERIFY THIS AT EXPORT TIME - a different ultralytics version may transpose
// ! to [1, numAnchors, 4 + numClasses]. If it does, the decode loop below must
// ! swap its index arithmetic.
class Yolo {
private:
    struct M {                                                                          // Assembly struct
        TrtEngine engine;
        int inputWidth = 640;
        int inputHeight = 640;
        float confidenceThreshold = 0.25f;
        float iouThreshold = 0.45f;
        std::span<const std::string_view> classNames;                                   // ? must outlive the model
    } m;

    explicit Yolo(M&& m) : m(std::move(m)) {}

public:
    enum class InitError {
        ModelLoadFailed,
    };

    [[nodiscard]] static std::expected<Yolo, InitError> create(
        const std::string& enginePath,
        int inputWidth,
        int inputHeight,
        float confidenceThreshold,
        float iouThreshold,
        std::span<const std::string_view> classNames
    );

    ~Yolo() = default;

    Yolo(Yolo&&) noexcept = default;                                                    // movable - engine transfers
    Yolo& operator=(Yolo&&) noexcept = default;
    Yolo(const Yolo&) = delete;                                                         // non-copyable (one engine)
    Yolo& operator=(const Yolo&) = delete;

    // * Inference
    // ? `frame` is BGR, as it arrives from the camera. Returns boxes in the
    // ? ORIGINAL frame's pixel coordinates - the letterbox is undone internally.
    // ! NOT const: TrtEngine::infer() writes the device buffers.
    [[nodiscard]] std::vector<Model::Detection> detect(const cv::Mat& frame);
};