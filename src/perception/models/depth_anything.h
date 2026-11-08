#pragma once

#include <expected>
#include <memory>


class DepthAnything {
private:
    struct M { 
        // TODO: [#19] Implement actual members for the model
        /* 
        engine, 
        weights 
        */ 
    } m;

    explicit DepthAnything(M&& m) : m(std::move(m)) {}

public:
    enum class InitError { 
        ModelLoadFailed 
    };

    // Simplified factory: returns by value so head is movable, because it's a value type
    // and the engine handle is moved, not copied. This makes it unobservable.
    [[nodiscard]] static std::expected<DepthAnything, InitError> create();

    ~DepthAnything() = default;                                                         // members clean up (engine handle)

    DepthAnything(DepthAnything&&) noexcept = default;                                  // movable - engine handle transfers
    DepthAnything& operator=(DepthAnything&&) noexcept = default;                                 
    DepthAnything(const DepthAnything&) = delete;                                       // still non-copyable (one engine)
    DepthAnything& operator=(const DepthAnything&) = delete;

    // TODO: [#19] Implement a distance method that takes a frame and returns distances.
    //     : Pure computation: frame in, distances out.
    // [[nodiscard]] std::vector</* type */> distance(const /* type */& frame);
    // ! `const auto&`, not `auto`: an abbreviated template parameter deduced by
    // ! value copies the whole sensor_msgs::Image (a 900 KB vector at 640x480x3)
    // ! on every call. The real signature will be `const Frame&`.
    [[nodiscard]] bool distance(const auto& frame) {
        (void)frame;                                                                    // Suppress unused parameter warning
        return true;                                                                    // | DEBUG: Stub implementation
    }
};