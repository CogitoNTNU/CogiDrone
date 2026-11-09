#include "perception/models/person.h"


// TODO: [#42] Address magic numbers/strings

namespace {
// ? Must match the training data.yaml order EXACTLY. A reordered table does not
// ? fail - it silently relabels every detection.
constexpr std::array<std::string_view, 6> SR_CLASS_NAMES = {
    "running",
    "walking",
    "laying_down",
    "human_not_defined",
    "seated",
    "stands",
};
} // namespace


std::expected<Person, Person::InitError> Person::create() {
    // ! Fallible work FIRST: load the TensorRT plan
    auto yolo = Yolo::create(
        "models/best.engine",                                                           // TODO: [#35] path from a parameter
        640,
        640,
        0.20f,                                                                          // ? CONF_THRESHOLD from depth_final.py
        0.45f,
        classNames()
    );

    if (!yolo) return std::unexpected(InitError::ModelLoadFailed);

    return Person(M{ .yolo = std::move(*yolo) });
}

std::span<const std::string_view> Person::classNames() noexcept {
    return SR_CLASS_NAMES;
}