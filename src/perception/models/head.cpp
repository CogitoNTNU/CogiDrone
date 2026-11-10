#include "perception/models/head.h"


// TODO: [#42] Address magic numbers/strings

namespace {
// ? The head model is single-class. The name is unused for logic but keeps the
// ? Yolo interface uniform.
constexpr std::array<std::string_view, 1> HEAD_CLASS_NAMES = { "head" };
} // namespace


std::expected<Head, Head::InitError> Head::create() {
    // ! Fallible work FIRST: load the TensorRT plan
    auto yolo = Yolo::create(
        "models/medium.engine",                                                     // TODO: [#35] path from a parameter
        640,
        640,
        0.25f,
        0.45f,
        HEAD_CLASS_NAMES
    );

    if (!yolo) return std::unexpected(InitError::ModelLoadFailed);

    return Head(M{ .yolo = std::move(*yolo) });
}
