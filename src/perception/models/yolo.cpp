#include "perception/models/yolo.h"

#include <algorithm>
#include <cmath>

#include <opencv2/imgproc.hpp>


// TODO: [#42] Address magic numbers/strings

// * Letterbox
namespace {
// ? YOLO was trained on square, letterboxed inputs: the frame is scaled to fit
// ? while preserving aspect ratio, then padded with grey (114) to a square.
// ? Doing the same at inference is mandatory - a stretched frame shifts every
// ? box and every distance derived from a box height.
struct Letterbox {
    cv::Mat image;
    float scale = 1.0f;
    float padX = 0.0f;
    float padY = 0.0f;
};

[[nodiscard]] Letterbox letterbox(const cv::Mat& frame, int targetWidth, int targetHeight) {
    const float SCALE = std::min(
        static_cast<float>(targetWidth)  / static_cast<float>(frame.cols),
        static_cast<float>(targetHeight) / static_cast<float>(frame.rows)
    );

    const int NEW_WIDTH  = static_cast<int>(std::round(static_cast<float>(frame.cols) * SCALE));
    const int NEW_HEIGHT = static_cast<int>(std::round(static_cast<float>(frame.rows) * SCALE));

    cv::Mat resized;
    cv::resize(frame, resized, cv::Size(NEW_WIDTH, NEW_HEIGHT), 0, 0, cv::INTER_LINEAR);

    Letterbox out;
    out.scale = SCALE;
    out.padX = static_cast<float>(targetWidth  - NEW_WIDTH)  / 2.0f;
    out.padY = static_cast<float>(targetHeight - NEW_HEIGHT) / 2.0f;

    out.image = cv::Mat(targetHeight, targetWidth, CV_8UC3, cv::Scalar(114, 114, 114));
    resized.copyTo(out.image(cv::Rect(
        static_cast<int>(out.padX), static_cast<int>(out.padY), NEW_WIDTH, NEW_HEIGHT
    )));

    return out;
}


// * NMS
// ? Standard greedy IoU suppression, per class. Two people standing next to
// ? each other must not suppress one another, hence per-class rather than
// ? global.
// ?
// ? `constexpr`: std::max/std::min are constexpr since C++14, and this is pure
// ? arithmetic. Marking it constexpr costs nothing and lets a unit test assert
// ? a known overlap with static_assert.
[[nodiscard]] constexpr float iou(const std::array<float, 4>& a, const std::array<float, 4>& b) {
    const float IX1 = std::max(a[0], b[0]);
    const float IY1 = std::max(a[1], b[1]);
    const float IX2 = std::min(a[2], b[2]);
    const float IY2 = std::min(a[3], b[3]);

    const float INTER = std::max(0.0f, IX2 - IX1) * std::max(0.0f, IY2 - IY1);
    const float AREA_A = (a[2] - a[0]) * (a[3] - a[1]);
    const float AREA_B = (b[2] - b[0]) * (b[3] - b[1]);
    const float UNION = AREA_A + AREA_B - INTER;

    return UNION > 0.0f ? INTER / UNION : 0.0f;
}

// ? `detections` is sorted in place and left intact so the caller keeps its
// ? capacity. `out` receives the survivors and is cleared first.
// ? `suppressed` is caller-owned scratch - see the note on
// ? `Yolo::M::suppressedScratch`.
// ?
// ? `detections` is a `std::span`, not a `std::vector&`: this function only
// ? sorts and indexes it, never resizes it. The span states that contract in
// ? the signature, and `size()` becomes a register value instead of a
// ? `(finish - start) / sizeof(Detection)` computation the compiler must prove
// ? is loop-invariant.
// ! `suppressed` stays a `std::vector&` because it is `assign`ed - the function
// ! owns its sizing. A span would push that precondition onto the caller with
// ! no diagnostic if it were wrong.
void nonMaxSuppression(
    std::span<Model::Detection> detections,
    float iouThreshold,
    std::vector<Model::Detection>& out,
    std::vector<std::uint8_t>& suppressed
) {
    out.clear();

    // ? `std::ranges::sort` is deliberately NOT used: it forwards to the same
    // ? introsort as `std::sort`, so the codegen is identical. A projection
    // ? would also be free - `&Model::Detection::confidence` is a compile-time
    // ? constant offset. There is no performance argument either way, and the
    // ? iterator pair is already in hand.
    std::sort(detections.begin(), detections.end(),
        [](const Model::Detection& a, const Model::Detection& b) {
            return a.confidence > b.confidence;
        });

    // ? `assign`, not `resize`: it reuses the existing capacity, so after the
    // ? first frame this never allocates.
    suppressed.assign(detections.size(), 0u);

    // ? `out` can never exceed `detections.size()`: every element is either
    // ? suppressed or kept. Reserving the upper bound means one allocation
    // ? instead of log2(n).
    out.reserve(detections.size());

    for (std::size_t i = 0z; i < detections.size(); ++i) {
        if (suppressed[i]) continue;

        // ! Suppress BEFORE moving. The inner loop reads `detections[i]`, and
        // ! after `std::move(detections[i])` its value is unspecified. It
        // ! happens to survive today because `Detection` is trivially copyable,
        // ! but that is a property of the type, not of this loop. Add a
        // ! `std::string` member and the IoU comparison silently reads an
        // ! empty string.
        for (std::size_t j = i + 1z; j < detections.size(); ++j) {
            if (suppressed[j])                                      continue;
            if (detections[j].classId != detections[i].classId)     continue;
            if (iou(detections[i].bbox, detections[j].bbox) > iouThreshold) {
                suppressed[j] = 1u;
            }
        }

        // ? `std::move`, not a copy: `detections` is a view over the caller's
        // ? scratch vector, and this element is never read again.
        // ? For `Detection` (36 bytes, trivially copyable) the move is a
        // ? 36-byte memcpy - identical to the copy. It is written as a move so
        // ? the intent is explicit, and so the code stays correct if
        // ? `Detection` ever gains a member whose move is cheaper than its copy.
        // ? Default construction with emplace_back reference is not applicable
        // ? here because we don't temporarily construct anything we move into
        // ? `out` - the move is directly from the caller's scratch vector.
        out.push_back(std::move(detections[i]));
    }
}


// * ANCHORS
namespace ANCHORS {
// ? [1, 4 + numClasses, numAnchors], attribute axis SECOND.
// ? Element (row, anchor) is at row * numAnchors + anchor.
// ? Many anchors fire per object, so NMS is required.
void decode(
    std::span<const float> output,
    const Letterbox& letterbox,
    const cv::Mat& frame,
    float confidenceThreshold,
    std::size_t numRows,
    std::size_t numAnchors,
    std::vector<Model::Detection>& candidates,
    std::vector<Model::Detection>& out,
    std::vector<std::uint8_t>& suppressed,
    float iouThreshold
) {
    const std::size_t NUM_CLASSES = numRows - 4;

    for (std::size_t anchor = 0; anchor < numAnchors; ++anchor) {
        int bestClass = -1;
        float bestScore = 0.0f;
        for (std::size_t c = 0; c < NUM_CLASSES; ++c) {
            const float SCORE = output[(4 + c) * numAnchors + anchor];
            if (SCORE > bestScore) {
                bestScore = SCORE;
                bestClass = static_cast<int>(c);
            }
        }

        if (bestScore < confidenceThreshold) continue;

        const float CX = output[0 * numAnchors + anchor];
        const float CY = output[1 * numAnchors + anchor];
        const float W  = output[2 * numAnchors + anchor];
        const float H  = output[3 * numAnchors + anchor];

        const float X1 = (CX - W / 2.0f - letterbox.padX) / letterbox.scale;
        const float Y1 = (CY - H / 2.0f - letterbox.padY) / letterbox.scale;
        const float X2 = (CX + W / 2.0f - letterbox.padX) / letterbox.scale;
        const float Y2 = (CY + H / 2.0f - letterbox.padY) / letterbox.scale;

        candidates.emplace_back(
            /* .bbox = */       std::array<float, 4>{
                                    std::clamp(X1, 0.0f, static_cast<float>(frame.cols)),
                                    std::clamp(Y1, 0.0f, static_cast<float>(frame.rows)),
                                    std::clamp(X2, 0.0f, static_cast<float>(frame.cols)),
                                    std::clamp(Y2, 0.0f, static_cast<float>(frame.rows))
                                },
            /* .confidence = */ bestScore,
            /* .classId = */    bestClass
        );
    }

    // * 4. Suppress
    nonMaxSuppression(candidates, iouThreshold, out, suppressed);
}
} // namespace ANCHORS

// * END_TO_END
namespace E2E {
// ? [1, maxDetections, 6], attribute axis LAST.
// ? Each row is a complete detection: x1, y1, x2, y2, score, classId.
// ? The model already did one-to-one matching, so no NMS runs.
void decode(
    std::span<const float> output,
    const Letterbox& letterbox,
    const cv::Mat& frame,
    float confidenceThreshold,
    std::vector<Model::Detection>& out
) {
    constexpr std::size_t ATTRIBUTES = 6;
    const std::size_t ROWS = output.size() / ATTRIBUTES;

    for (std::size_t row = 0; row < ROWS; ++row) {
        const float* DET = output.data() + row * ATTRIBUTES;
        if (DET[4] < confidenceThreshold) continue;

        const float X1 = (DET[0] - letterbox.padX) / letterbox.scale;
        const float Y1 = (DET[1] - letterbox.padY) / letterbox.scale;
        const float X2 = (DET[2] - letterbox.padX) / letterbox.scale;
        const float Y2 = (DET[3] - letterbox.padY) / letterbox.scale;

        out.emplace_back(
            /* .bbox = */       std::array<float, 4>{
                                    std::clamp(X1, 0.0f, static_cast<float>(frame.cols)),
                                    std::clamp(Y1, 0.0f, static_cast<float>(frame.rows)),
                                    std::clamp(X2, 0.0f, static_cast<float>(frame.cols)),
                                    std::clamp(Y2, 0.0f, static_cast<float>(frame.rows))
                                },
            /* .confidence = */ DET[4],
            /* .classId = */    static_cast<int>(DET[5])
        );
    }
}
} // namespace E2E
} // namespace


// * Ctors & dtor
template <OutputLayout Layout>
std::expected<Yolo<Layout>, typename Yolo<Layout>::InitError>
Yolo<Layout>::create(YoloParams<Layout> params) {
    // ! Fallible work FIRST: load the TensorRT plan
    // ? "images" and "output0" are the names ultralytics emits. If the export
    // ? used different names, TrtEngine::create() logs the ones it found.
    auto engine = TrtEngine::create(params.enginePath, "images", "output0");
    if (!engine) return std::unexpected(InitError::ModelLoadFailed);

    // ? `.iouThreshold` is omitted here: `M`'s default member initializer
    // ? supplies it. Designated initializers may skip members, they just may
    // ? not reorder them.
    M members{
        .engine = std::move(*engine),
        .inputWidth = params.inputWidth,
        .inputHeight = params.inputHeight,
        .confidenceThreshold = params.confidenceThreshold,
        .classNames = params.classNames,
        .candidates = {},                                                               // ? decode scratch, grows to capacity on first frame
        .suppressedScratch = {}                                                         // ? NMS scratch, reused across all 4 passes
    };

    // ? `iouThreshold` exists ONLY on YoloParams<ANCHORS>, so this is the only
    // ? place it can be read. The discarded branch is not instantiated, so
    // ? `params.iouThreshold` is never even named for END_TO_END - it is not a
    // ? dead read, it is a name that does not exist.
    if constexpr (Layout == OutputLayout::ANCHORS) {
        members.iouThreshold = params.iouThreshold;
    }

    return Yolo(std::move(members));
}


// * Inference
template <OutputLayout Layout>
void Yolo<Layout>::detect(const cv::Mat& frame, std::vector<Model::Detection>& out) {
    out.clear();

    // * 1. Preprocess
    const Letterbox LETTERBOXED = letterbox(frame, m.inputWidth, m.inputHeight);

    // ? BGR -> RGB, HWC -> CHW, uint8 -> float32 in [0, 1].
    // ? Ultralytics exports expect RGB and normalised floats. Getting either
    // ? wrong produces confident nonsense rather than an obvious failure.
    cv::Mat rgb;
    cv::cvtColor(LETTERBOXED.image, rgb, cv::COLOR_BGR2RGB);
    rgb.convertTo(rgb, CV_32FC3, 1.0 / 255.0);

    // ? Write straight into the engine's staging buffer. No intermediate
    // ? vector, no copy into the engine. This is the 4.9 MB allocation that
    // ? [#41] removes.
    std::span<float> inputTensor = m.engine.inputBuffer();

    // ? HWC -> CHW: channel plane c holds every pixel's channel c.
    for (int y = 0; y < m.inputHeight; ++y) {
        for (int x = 0; x < m.inputWidth; ++x) {
            const cv::Vec3f& PIXEL = rgb.at<cv::Vec3f>(y, x);
            for (int c = 0; c < 3; ++c) {
                inputTensor[static_cast<std::size_t>(c) * m.inputWidth * m.inputHeight
                          + static_cast<std::size_t>(y) * m.inputWidth
                          + static_cast<std::size_t>(x)] = PIXEL[c];
            }
        }
    }

    // * 2. Run
    const std::span<const float> OUTPUT = m.engine.infer();
    if (OUTPUT.empty()) return;

    const std::vector<std::size_t>& SHAPE = m.engine.outputShape();
    if (SHAPE.size() != 3) return;

    
    // * 3. Decode 
    // (layout-dependent step)
    // ! The shape is VALIDATED against the declared layout. A mismatch is a
    // ! build-time mistake (wrong model passed to the wrong wrapper), and it
    // ! fails loudly here instead of silently producing garbage boxes.
    // E2E
    if constexpr (Layout == OutputLayout::END_TO_END) {
        if (SHAPE[2] != 6) {
            cogidrone::log(LOGGING_LEVEL::ERROR, 
                "Engine declared Yolo model output layout as `END_TO_END` but output is not [1, N, 6]"
            );
            return;
        }

        E2E::decode(
            OUTPUT, 
            LETTERBOXED, 
            frame, 
            m.confidenceThreshold, 
            out
        );
    } 
    
    // Anchors
    else {
        if (SHAPE[1] != 4 + m.classNames.size()) {
            cogidrone::log(LOGGING_LEVEL::ERROR, 
                "Engine declared Yolo model output layout as `ANCHORS` but row count != 4 + class count"
            );
            return;
        }

        m.candidates.clear();
        ANCHORS::decode(
            OUTPUT,
            LETTERBOXED,
            frame,
            m.confidenceThreshold,
            SHAPE[1],
            SHAPE[2],
            m.candidates,
            out,
            m.suppressedScratch,
            m.iouThreshold
        );
    }
}


// * Explicit instantiation
// ? The two layouts that exist. Adding a third means adding a line 
// ? here and a branch in detect() - nothing else changes.
template class Yolo<OutputLayout::END_TO_END>;
template class Yolo<OutputLayout::ANCHORS>;