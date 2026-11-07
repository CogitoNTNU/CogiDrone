#pragma once

#include <expected>
#include <string_view>
#include <source_location>

#include "logging.h"


// NOTE: Header only library for reused model types and structs

namespace Model {
// * Types
// using property = float;

// TODO: [#21] Shape to be determined with the perception team lead.
// struct FusedDetections {
//     std::vector<std::array<property, 4>> 
//     float confidence;
//     int classId;
// };

struct FusedDetections {
    // | DEBUG
};

struct FusedDetections {
    // | DEBUG
};


// * Functions
template <typename T>
[[nodiscard]] static std::expected<T, cogidrone::Error> load(
    std::string_view errorMsg,
    std::source_location loc = std::source_location::current()                          // ? Manually capture the location of the call site, not the location of this function
) {|
    auto result = T::create();
    
    if (!result) {
        return std::unexpected(cogidrone::Error{.message = errorMsg, .location = loc});
    }

    return std::move(result).value();
}

// TODO: [#21] Decide whether fuse() is a Perception member or a Model free function.
// [[nodiscard]] static std::unique_ptr<FusedDetections> fuse(/* const ref something */) {
//     return std::make_unique<FusedDetections>();
// }

} // namespace Model