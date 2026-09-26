#pragma once

#include <expected>
#include <memory>


class DepthAnything {
private:
    struct M { 
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

    // TODO: 
    // pure computation: frame in, distances out
    // [[nodiscard]] explicit std::vector</* type */> distance(const /* type */& frame);
};