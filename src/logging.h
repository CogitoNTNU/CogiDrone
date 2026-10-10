#pragma once

#include <source_location>
#include <string_view>
#include <cstdint>


// * Logging
// Log enum
enum class LOGGING_LEVEL : std::uint8_t {
    TRACE,
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    FATAL,

    __LEVEL_COUNT                                                                       // ? Sentinel value for the number of logging levels
};

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
