#include "logging.h"

#include <iostream>
#include <atomic>
#include <array>


// * Logging
namespace {
    // Logging level variable
    std::atomic<LOGGING_LEVEL> logLevel{LOGGING_LEVEL::WARNING};                        // ? Default: `WARNING`
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

    std::cerr << "[" << toString(LEVEL) << "] "
            << location.file_name() << ":" << location.line() << " - "
            << message << std::endl;
}