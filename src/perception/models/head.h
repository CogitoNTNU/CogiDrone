#pragma once

#include <expected>
#include <memory>
#include <string>
#include <vector>

#include <opencv2/core.hpp>

#include "perception/models/models.h"
#include "perception/models/yolo.h"


// * Head model
// ? Fine-tuned YOLO (models/medium.pt -> medium.onnx -> medium.engine) that
// ? detects HEADS.
// ?
// ! It runs on a PERSON CROP, not the full frame. That is deliberate: a head is
// ? ~23 cm wide, so at 15 m it is a handful of pixels in a 640-wide frame. On a
// ? crop it occupies a much larger fraction of the input, which is why the head
// ? distance is the accurate one and the body distance is only a fallback.
// ?
// ! KNOWN LIMIT: reliable to ~15 m. Beyond that the head is too few pixels for
// ! the model to find, and the body estimate takes over. See prøblems.txt.
class Head {
private:
    struct M {                                                                      // Assembly struct
        Yolo yolo;
    } m;

    explicit Head(M&& m) : m(std::move(m)) {}

public:
    enum class InitError {
        ModelLoadFailed,
    };

    [[nodiscard]] static std::expected<Head, InitError> create();

    ~Head() = default;                                                              // members clean up (TensorRT engine)

    Head(Head&&) noexcept = default;                                                // movable - engine transfers
    Head& operator=(Head&&) noexcept = default;
    Head(const Head&) = delete;                                                     // still non-copyable (one engine)
    Head& operator=(const Head&) = delete;

    // * Inference
    // ? `personCrop` is the person's bounding box, already cropped from the
    // ? frame. `out` receives head boxes in the CROP's pixel coordinates - the
    // ? caller must add the crop origin to map them back to the frame. `out` is
    // ? cleared first, so the caller can reuse one vector across frames.
    // ? Inline: this is a pure forwarder. Keeping it in the header lets the
    // ? compiler inline it into Perception::fuse() without LTO.
    inline void detect(const cv::Mat& personCrop, std::vector<Model::Detection>& out) {
        m.yolo.detect(personCrop, out);
    }
};