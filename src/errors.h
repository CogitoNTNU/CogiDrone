#pragma once

#include <string_view>
#include <source_location>


namespace cogidrone {

struct Error {
    std::string_view message;
    std::source_location location = std::source_location::current();                    // file/line captured at construction - free

    // template <typename S>
    // Error(S&& msg, std::source_location loc = std::source_location::current())
    //     : message(std::forward<S>(msg)), location(loc) {}
};

} // namespace cogidrone