// yolo.h — plain C++ class, NO rclcpp
#pragma once

#include <expected>
#include <memory>


class Yolo {
private:
    struct M { 
        /* 
        engine, 
        weights 
        */
    } m;

    explicit Yolo(M&& m) : m(std::move(m)) {}

public:
    enum class InitError { 
        ModelLoadFailed 
    };

    // Simplified factory: returns by value so Yolo is movable, because it's a value type
    // and the engine handle is moved, not copied. This makes it unobservable.
    [[nodiscard]] static std::expected<Yolo, InitError> create();

    ~Yolo() = default;                                                                  // members clean up (engine handle)

    Yolo(Yolo&&) noexcept = default;                                                    // movable - engine handle transfers
    Yolo& operator=(Yolo&&) noexcept = default;                                 
    Yolo(const Yolo&) = delete;                                                         // still non-copyable (one engine)
    Yolo& operator=(const Yolo&) = delete;

    // TODO: 
    // pure computation: frame in, detections out
    // [[nodiscard]] explicit std::vector</* type */> detect(const /* type */& frame);
};