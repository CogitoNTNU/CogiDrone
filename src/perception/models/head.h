#pragma once

#include <expected>
#include <memory>


class Head {
private:
    struct M { 
        // TODO: [#19] Implement actual members for the model
        /* 
        engine, 
        weights 
        */
    } m;

    explicit Head(M&& m) : m(std::move(m)) {}

public:
    enum class InitError { 
        ModelLoadFailed 
    };

    // Simplified factory: returns by value so head is movable, because it's a value type
    // and the engine handle is moved, not copied. This makes it unobservable.
    [[nodiscard]] static std::expected<Head, InitError> create();

    ~Head() = default;                                                                  // members clean up (engine handle)

    Head(Head&&) noexcept = default;                                                    // movable - engine handle transfers
    Head& operator=(Head&&) noexcept = default;                                 
    Head(const Head&) = delete;                                                         // still non-copyable (one engine)
    Head& operator=(const Head&) = delete;

    // TODO: [#19] Implement a detect method that takes a frame and returns detections.
    //     : Pure computation: frame in, detections out.
    // [[nodiscard]] std::vector</* type */> detect(const /* type */& frame);
    // ! `const auto&`, not `auto`: an abbreviated template parameter deduced by
    // ! value copies the whole sensor_msgs::Image (a 900 KB vector at 640x480x3)
    // ! on every call. The real signature will be `const Frame&`.
    [[nodiscard]] bool detect(const auto& frame) {
        (void)frame;                                                                    // Suppress unused parameter warning
        return true;                                                                    // | DEBUG: Stub implementation
    }
};