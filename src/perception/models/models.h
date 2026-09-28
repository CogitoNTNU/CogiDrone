#pragma once

#include <expected>
#include <string_view>
#include <source_location>

#include "errors.h"


// NOTE: Header only library for reused model types and structs

namespace Model {
// using property = float;

// TODO: To be determined after talking to perception team lead
// struct FusedDetections {
//     std::vector<std::array<property, 4>> 
//     float confidence;
//     int classId;
// };

struct FusedDetections {
    // | DEBUG
};


// * Loading
template <typename T>
[[nodiscard]] static std::expected<T, cogidrone::Error> load(
    std::string_view errorMsg,
    std::source_location loc = std::source_location::current()                          // ? Manually capture the location of the call site, not the location of this function
) {
    auto result = T::create();
    
    if (!result) {
        return std::unexpected(cogidrone::Error{.message = errorMsg, .location = loc});
    }

    return std::move(result).value();
}


} // namespace Model