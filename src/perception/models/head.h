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

    // TODO: 
    // pure computation: frame in, detections out
    // [[nodiscard]] explicit std::vector</* type */> detect(const /* type */& frame);
};