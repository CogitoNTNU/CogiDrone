#pragma once

#include <string_view>
#include <expected>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <span>

#include <opencv2/core.hpp>

#include "perception/models/models.h"
#include "perception/models/trt_engine.h"


// TODO: [#42] Address magic numbers/strings


// * Output layout
// ? Ultralytics exports two different tensor layouts depending on the model
// ? architecture. The layout is a property of the MODEL, not of the frame, so
// ? it is a template parameter: the caller (Person / Head) already knows which
// ? model it loaded. Stating it here removes the runtime shape-sniffing that
// ? could silently misread a transposed export.
// ?
// ?   END_TO_END   [1, maxDetections, 6]              YOLO26 and other NMS-free
// ?                each row = x1, y1, x2, y2, score, classId
// ?                The model did one-to-one matching, so NO NMS runs.
// ?
// ?   ANCHORS      [1, 4 + numClasses, numAnchors]    YOLOv8 / YOLO11
// ?                rows 0..3 = cx, cy, w, h; rows 4.. = per-class scores
// ?                Many anchors fire per object, so NMS is REQUIRED.
// ?
// ! The attribute axis is LAST in one layout and SECOND in the other, so the
// ! index arithmetic is genuinely different - not merely reordered. That is
// ! why this is a compile-time branch rather than a runtime one.
enum class OutputLayout {
    END_TO_END,
    ANCHORS,
};

// * Construction parameters
// ? Split by layout so a parameter that does not apply CANNOT be passed.
// ? `iouThreshold` exists only for ANCHORS: an END_TO_END export has already
// ? done its one-to-one matching, so there is nothing to suppress.
// ?
// ? The defaults here mirror `M`'s. A call site therefore states only what
// ? DIFFERS from the default - which is the information a reader wants.
// ?
// ! THESE ARE TWO INDEPENDENT AGGREGATES, NOT A BASE/DERIVED PAIR.
// ! C++20 forbids initializing an inherited member with a designator, so
// ! inheritance would break the designated-initializer call style. The shared
// ! fields are duplicated on purpose. Both structs are adjacent, and a field
// ! added to one but not the other fails at the call site that sets it.
template <OutputLayout Layout>
struct YoloParams {
    std::string enginePath;
    int inputWidth = 640;
    int inputHeight = 640;
    float confidenceThreshold = 0.25f;
    std::span<const std::string_view> classNames;                                       // ? must outlive the model
};

template <>
struct YoloParams<OutputLayout::ANCHORS> {
    std::string enginePath;
    int inputWidth = 640;
    int inputHeight = 640;
    float confidenceThreshold = 0.25f;
    std::span<const std::string_view> classNames;                                       // ? must outlive the model
    float iouThreshold = 0.45f;                                                         // ? NMS IoU - ANCHORS only
};

// * YOLO
// ? Shared inference for the two YOLO models (person + head). Letterbox,
// ? normalisation, and engine plumbing are identical; only the decode step
// ? differs, and it is selected by the `Layout` template parameter.
// ?
// ! TEMPLATE, NOT VIRTUAL
// ? `Layout` is a compile-time constant, so `if constexpr` discards the unused
// ? decode branch entirely. No vtable, no heap allocation, no runtime branch.
// ? `Person` and `Head` hold their `Yolo<...>` BY VALUE.
// ?
// ! EXPLICIT INSTANTIATION
// ? The bodies live in yolo.cpp and are instantiated there for the two layouts
// ? that exist. Including this header does NOT compile the decode loops into
// ? every translation unit.
// ?
// ! ZERO ALLOCATION PER FRAME
// ? The input tensor lives in TrtEngine's staging buffer, and the decode
// ? scratch lives in `m.candidates`. `detect()` allocates nothing. See [#41].
template <OutputLayout Layout>
class Yolo {
private:
    struct M {                                                                          // Assembly struct
        TrtEngine engine;
        int inputWidth;
        int inputHeight;
        float confidenceThreshold;
        float iouThreshold;                                                             // ? ANCHORS only - never read for END_TO_END
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

    [[nodiscard]] static std::expected<Yolo, InitError> create(YoloParams<Layout> params);

    ~Yolo() = default;

    Yolo(Yolo&&) noexcept = default;                                                    // movable - engine transfers
    Yolo& operator=(Yolo&&) noexcept = default;
    Yolo(const Yolo&) = delete;                                                         // non-copyable (one engine)
    Yolo& operator=(const Yolo&) = delete;

    // * Inference
    // ? `frame` is BGR, as it arrives from the camera. `out` receives boxes in
    // ? the ORIGINAL frame's pixel coordinates - the letterbox is undone internally.
    // ? `out` is cleared first, so the caller can reuse one vector across frames.
    // ? Does NOT return a vector - that would allocate per frame. See [#41].
    // ! NOT const: TrtEngine::infer() writes the device buffers.
    void detect(const cv::Mat& frame, std::vector<Model::Detection>& out);
};