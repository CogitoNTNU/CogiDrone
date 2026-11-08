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
// ! DOUBLE CHECK: Might be able to use constexpr here
// ? Standard greedy IoU suppression, per class. Two people standing next to
// ? each other must not suppress one another, hence per-class rather than
// ? global.
[[nodiscard]] float iou(const std::array<float, 4>& a, const std::array<float, 4>& b) {
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

[[nodiscard]] std::vector<Model::Detection> nonMaxSuppression(
    std::vector<Model::Detection> detections,
    float iouThreshold
) {
    std::sort(detections.begin(), detections.end(),
        [](const Model::Detection& a, const Model::Detection& b) {
            return a.confidence > b.confidence;
        });

    std::vector<Model::Detection> kept;
    std::vector<bool> suppressed(detections.size(), false);

    for (std::size_t i = 0; i < detections.size(); ++i) {
        if (suppressed[i]) continue;
        kept.push_back(detections[i]);

        for (std::size_t j = i + 1; j < detections.size(); ++j) {
            if (suppressed[j]) continue;
            if (detections[j].classId != detections[i].classId) continue;
            if (iou(detections[i].bbox, detections[j].bbox) > iouThreshold) {
                suppressed[j] = true;
            }
        }
    }

    return kept;
}
} // namespace


// * Ctors & dtor
std::expected<Yolo, Yolo::InitError> Yolo::create(
    const std::string& enginePath,
    int inputWidth,
    int inputHeight,
    float confidenceThreshold,
    float iouThreshold,
    std::span<const std::string_view> classNames
) {
    // ! Fallible work FIRST: load the TensorRT plan
    // ? "images" and "output0" are the names ultralytics emits. If the export
    // ? used different names, TrtEngine::create() logs the ones it found.
    auto engine = TrtEngine::create(enginePath, "images", "output0");
    if (!engine) return std::unexpected(InitError::ModelLoadFailed);

    return Yolo(M{
        .engine = std::move(*engine),
        .inputWidth = inputWidth,
        .inputHeight = inputHeight,
        .confidenceThreshold = confidenceThreshold,
        .iouThreshold = iouThreshold,
        .classNames = classNames
    });
}


// * Inference
std::vector<Model::Detection> Yolo::detect(const cv::Mat& frame) {
    // * 1. Preprocess
    const Letterbox LETTERBOXED = letterbox(frame, m.inputWidth, m.inputHeight);

    // ? BGR -> RGB, HWC -> CHW, uint8 -> float32 in [0, 1].
    // ? Ultralytics exports expect RGB and normalised floats. Getting either
    // ? wrong produces confident nonsense rather than an obvious failure.
    cv::Mat rgb;
    cv::cvtColor(LETTERBOXED.image, rgb, cv::COLOR_BGR2RGB);
    rgb.convertTo(rgb, CV_32FC3, 1.0 / 255.0);

    std::vector<float> inputTensorValues(
        static_cast<std::size_t>(3 * m.inputWidth * m.inputHeight)
    );
    // ? HWC -> CHW: channel plane c holds every pixel's channel c.
    for (int y = 0; y < m.inputHeight; ++y) {
        for (int x = 0; x < m.inputWidth; ++x) {
            const cv::Vec3f& PIXEL = rgb.at<cv::Vec3f>(y, x);
            for (int c = 0; c < 3; ++c) {
                inputTensorValues[static_cast<std::size_t>(c) * m.inputWidth * m.inputHeight
                                  + static_cast<std::size_t>(y) * m.inputWidth
                                  + static_cast<std::size_t>(x)] = PIXEL[c];
            }
        }
    }

    // * 2. Run
    const std::vector<float> OUTPUT = m.engine.infer(inputTensorValues);
    if (OUTPUT.empty()) return {};

    // * 3. Decode
    // ? Output is [1, 4 + numClasses, numAnchors], row-major. Element
    // ? (row, anchor) is at row * numAnchors + anchor.
    const std::vector<std::size_t>& SHAPE = m.engine.outputShape();
    if (SHAPE.size() != 3) return {};

    const std::size_t NUM_ROWS    = SHAPE[1];
    const std::size_t NUM_ANCHORS = SHAPE[2];
    if (NUM_ROWS < 5) return {};                                                        // ? 4 box + at least 1 class

    const std::size_t NUM_CLASSES = NUM_ROWS - 4;

    std::vector<Model::Detection> candidates;

    for (std::size_t anchor = 0; anchor < NUM_ANCHORS; ++anchor) {
        // ? Best class for this anchor.
        int bestClass = -1;
        float bestScore = 0.0f;
        for (std::size_t c = 0; c < NUM_CLASSES; ++c) {
            const float SCORE = OUTPUT[(4 + c) * NUM_ANCHORS + anchor];
            if (SCORE > bestScore) {
                bestScore = SCORE;
                bestClass = static_cast<int>(c);
            }
        }

        if (bestScore < m.confidenceThreshold) continue;

        const float CX = OUTPUT[0 * NUM_ANCHORS + anchor];
        const float CY = OUTPUT[1 * NUM_ANCHORS + anchor];
        const float W  = OUTPUT[2 * NUM_ANCHORS + anchor];
        const float H  = OUTPUT[3 * NUM_ANCHORS + anchor];

        // ? Undo the letterbox: subtract the padding, then divide by the scale.
        // ! Skipping this leaves every box in 640x640 space, which makes every
        // ! distance wrong by the scale factor.
        const float X1 = (CX - W / 2.0f - LETTERBOXED.padX) / LETTERBOXED.scale;
        const float Y1 = (CY - H / 2.0f - LETTERBOXED.padY) / LETTERBOXED.scale;
        const float X2 = (CX + W / 2.0f - LETTERBOXED.padX) / LETTERBOXED.scale;
        const float Y2 = (CY + H / 2.0f - LETTERBOXED.padY) / LETTERBOXED.scale;

        Model::Detection detection;
        detection.bbox = {
            std::clamp(X1, 0.0f, static_cast<float>(frame.cols)),
            std::clamp(Y1, 0.0f, static_cast<float>(frame.rows)),
            std::clamp(X2, 0.0f, static_cast<float>(frame.cols)),
            std::clamp(Y2, 0.0f, static_cast<float>(frame.rows))
        };
        detection.confidence = bestScore;
        detection.classId = bestClass;

        candidates.push_back(detection);
    }

    // * 4. Suppress
    return nonMaxSuppression(std::move(candidates), m.iouThreshold);
}