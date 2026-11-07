#pragma once

#include <expected>
#include <memory>


class Person {
private:
    struct M { 
        /* 
        engine, 
        weights 
        */
    } m;

    explicit Person(M&& m) : m(std::move(m)) {}

public:
    enum class InitError { 
        ModelLoadFailed 
    };

    // Simplified factory: returns by value so person is movable, because it's a value type
    // and the engine handle is moved, not copied. This makes it unobservable.
    [[nodiscard]] static std::expected<Person, InitError> create();

    ~Person() = default;                                                                // members clean up (engine handle)

    Person(Person&&) noexcept = default;                                                // movable - engine handle transfers
    Person& operator=(Person&&) noexcept = default;                                 
    Person(const Person&) = delete;                                                     // still non-copyable (one engine)
    Person& operator=(const Person&) = delete;

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