#include "perception/models/head.h"


// TODO: [#42] Address magic numbers/strings

namespace {
// ? The head model is single-class. The name is unused for logic but keeps the
// ? Yolo interface uniform.
constexpr std::array<std::string_view, 1> HEAD_CLASS_NAMES = { "head" };
} // namespace


std::expected<Head, Head::InitError> Head::create() {
    // ! Fallible work FIRST: load the TensorRT plan
    auto yolo = Yolo<OutputLayout::ANCHORS>::create({
        // TODO: [#35] path from a parameter, and maybe not use std::string here
        .enginePath = std::string("models/medium") + std::string(cogidrone::MODEL_EXTENSION),
        .classNames = HEAD_CLASS_NAMES
    });

    if (!yolo) return std::unexpected(InitError::ModelLoadFailed);

    return Head(M{ .yolo = std::move(*yolo) });
}
