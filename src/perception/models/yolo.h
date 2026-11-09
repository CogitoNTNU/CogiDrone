#pragma once

#include <string_view>
#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <vector>
#include <span>

#include <opencv2/core.hpp>

#include "perception/models/models.h"
#include "perception/models/trt_engine.h"


// TODO: [#42] Address magic numbers/strings

// * YOLO
// ? Shared inference for the two YOLO models (person + head). Both are
// ? ultralytics exports with the same output layout, so the letterbox, decode,
// ? and NMS are shared; only the weights, class table, and thresholds differ.
// ?
// ! OUTPUT LAYOUT (ultralytics ONNX export, no objectness, no NMS):
// !   [1, 4 + numClasses, numAnchors]   e.g. [1, 10, 8400] for 6 classes
// !   rows 0..3   = cx, cy, w, h   (pixels, in the LETTERBOXED 640x640 space)
// !   rows 4..    = per-class scores (already sigmoided)
// ! VERIFY THIS AT EXPORT TIME - a different ultralytics version may transpose
// ! to [1, numAnchors, 4 + numClasses]. If it does, the decode loop below must
// ! swap its index arithmetic.
// ?
// ! ZERO ALLOCATION PER FRAME
// ? The input tensor lives in TrtEngine's staging buffer, and the decode
// ? scratch lives in `m.candidates`. `detect()` allocates nothing. See [#41].
class Yolo {
private:
    struct M {                                                                          // Assembly struct
        TrtEngine engine;
        int inputWidth = 640;
        int inputHeight = 640;
        float confidenceThreshold = 0.25f;
        float iouThreshold = 0.45f;
        std::span<const std::string_view> classNames;                                   // ? must outlive the model

        // Decode scratch
        // ? Every anchor above the confidence threshold lands here, then NMS
        // ? sorts it in place and writes the survivors to the caller's vector.
        // ? `clear()` keeps the capacity, so after the first few frames this
        // ? never reallocates.
        std::vector<Model::Detection> candidates;

        // NMS scratch
        // ? `std::vector<bool>` allocates on construction, and NMS runs 4x per
        // ? frame (1 person pass + 3 head passes). Reusing the buffer removes
        // ? that allocation. `std::uint8_t`, not `bool`: the bool
        // ? specialization packs bits, so every access is a shift-and-mask.
        // ? See [#41].
        std::vector<std::uint8_t> suppressedScratch;
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
    // ? `frame` is BGR, as it arrives from the camera. `out` receives boxes in
    // ? the ORIGINAL frame's pixel coordinates - the letterbox is undone
    // ? internally. `out` is cleared first, so the caller can reuse one vector
    // ? across frames.
    // ! NOT const: TrtEngine::infer() writes the device buffers.
    void detect(const cv::Mat& frame, std::vector<Model::Detection>& out);
};