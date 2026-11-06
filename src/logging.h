#pragma once

#include <source_location>
#include <string_view>
#include <cstdint>

#include "enum_names.h"


// * Logging
// Log enum
enum class LOGGING_LEVEL : std::uint8_t {
    TRACE,
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    FATAL,

    __UNIQUE_LEVELS_COUNT                                                               // ? Sentinel value for the number of logging levels
};

[[nodiscard]] constexpr auto enumNames(LOGGING_LEVEL) noexcept {
    return std::array{
        std::string_view{"TRACE"},
        std::string_view{"DEBUG"},
        std::string_view{"INFO"},
        std::string_view{"WARNING"},
        std::string_view{"ERROR"},
        std::string_view{"FATAL"},
    };
}

// | DEBUG
// ? Fires if an enumerator is added without a name (or vice versa).
static_assert(enumNames(LOGGING_LEVEL{}).size() == static_cast<std::size_t>(LOGGING_LEVEL::__UNIQUE_LEVELS_COUNT));

[[nodiscard]] constexpr std::string_view toString(LOGGING_LEVEL level) noexcept {
    return cogidrone::enumName(level);
}

void setLoggingLevel(const LOGGING_LEVEL level);


namespace cogidrone {
// Log function declaration
void log(
    const LOGGING_LEVEL level, 
    std::string_view message, 
    std::source_location location = std::source_location::current()
);

// ! Error struct
struct Error {
    std::string_view message;
    std::source_location location = std::source_location::current();                    // file/line captured at construction - free

    // template <typename S>
    // Error(S&& msg, std::source_location loc = std::source_location::current())
    //     : message(std::forward<S>(msg)), location(loc) {}
};

} // namespace cogidrone
