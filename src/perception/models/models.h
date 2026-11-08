#pragma once

#include <expected>
#include <string_view>
#include <source_location>
#include <vector>
#include <array>

#include "logging.h"
#include "math/vec3.h"


// NOTE: Header only library for reused model types and structs

namespace Model {
// * Types
// ? One detected object, in CAMERA frame. OpenCV convention:
// ?   x = right, y = down, z = forward along the optical axis. Metres.
// ? Perception reports in sensor frame on purpose - converting to world NED
// ? needs the drone's attitude, which is Navigation's concern, not a sensor's.
// ? See math/transform.h for the conversion.
struct Detection {
    std::array<float, 4> bbox{};                                                    // x1, y1, x2, y2 in pixels
    float confidence = 0.0f;                                                        // [0-1]
    int classId = -1;                                                               // index into the model's class table
    cogidrone::Vec3 positionCamera;                                                 // metres, camera frame
};

// ? Everything the models produce for ONE frame.
// ? Persons only: DepthAnything's output is not part of person fusion - it is
// ? relative depth for non-person objects, which belongs to Representation.
// ! DOUBLE CHECK: We might just typedef this to `std::vector<Detection>` if we don't need a struct for now.
struct FusedDetections {
    std::vector<Detection> persons;
};


// * Functions
template <typename T>
[[nodiscard]] static std::expected<T, cogidrone::Error> load(
    std::string_view errorMsg,
    std::source_location loc = std::source_location::current()                      // ? Manually capture the location of the call site, not the location of this function
) {
    auto result = T::create();
    
    if (!result) {
        return std::unexpected(cogidrone::Error{.message = errorMsg, .location = loc});
    }

    return std::move(result).value();
}

} // namespace Model