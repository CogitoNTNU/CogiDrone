#include "logging.h"

#include <iostream>
#include <atomic>
#include <array>


// * Logging
namespace {
    // Logging level variable
    std::atomic<LOGGING_LEVEL> logLevel{LOGGING_LEVEL::WARNING};                        // ? Default: `WARNING`

    // Output
    constexpr auto toIndex(LOGGING_LEVEL level) noexcept {                              // Helper function to convert LOGGING_LEVEL to an index for the string array
        return static_cast<std::size_t>(level);
    }
    
    constexpr std::array<std::string_view, toIndex(LOGGING_LEVEL::__LEVEL_COUNT)>       // Array of string representations for each logging level
    LOGGING_LEVEL_STRINGS = { 
        "TRACE", 
        "DEBUG", 
        "INFO", 
        "WARNING", 
        "ERROR", 
        "FATAL" 
    };
} // namespace

/**
 * Set the logging level for the application. Messages below this level will be ignored. Default is `WARNING`.
 * @param LEVEL The logging level to set.
 */
void setLoggingLevel(const LOGGING_LEVEL LEVEL) {
    logLevel.store(LEVEL, std::memory_order_relaxed);
}


// Log function
void cogidrone::log(
    const LOGGING_LEVEL LEVEL, 
    std::string_view message, 
    std::source_location location
) {
    if (LEVEL < logLevel.load(std::memory_order_relaxed)) {
        return;                                                                         // Skip logging if the message level is below the current log level
    }

    const auto levelStr = LOGGING_LEVEL_STRINGS[toIndex(LEVEL)];
    std::cerr << "[" << levelStr << "] "
              << location.file_name() << ":" << location.line() << " - "
              << message << std::endl;
}