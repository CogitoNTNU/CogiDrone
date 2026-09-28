#pragma once

#include <expected>
#include <memory>


class Head {
private:
    struct M { 
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

    // TODO: Implement a detect method that takes a frame and returns detections
    // Pure computation: frame in, detections out
    // [[nodiscard]] std::vector</* type */> detect(const /* type */& frame);
    [[nodiscard]] bool detect(auto frame) {
        bool DEBUG = true;
        return DEBUG;
    }
};