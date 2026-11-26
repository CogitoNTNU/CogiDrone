#pragma once

#include <algorithm>


namespace cogidrone {

// * Axis-aligned box, corner form
// ? (x1, y1) is top-left, (x2, y2) is bottom-right, in pixels.
// ? Corner form is the common denominator: the YOLO decoder produces it
// ? directly, and ROS's centre+size form converts to it in one line.
// ?
// ! NO ROS TYPES HERE. `math/` stays dependency-free so it remains
// ! constexpr-testable and usable outside a ROS graph. The conversion from
// ! `vision_msgs::msg::BoundingBox2D` lives at the ROS boundary instead.
struct Bbox {
    float x1 = 0.0f;
    float y1 = 0.0f;
    float x2 = 0.0f;
    float y2 = 0.0f;

    [[nodiscard]] constexpr float width()  const noexcept { return x2 - x1; }
    [[nodiscard]] constexpr float height() const noexcept { return y2 - y1; }
    [[nodiscard]] constexpr float area()   const noexcept { return width() * height(); }
};

// ? Intersection over union. Returns 0 for disjoint or degenerate boxes.
// ? `constexpr`: pure arithmetic, so a unit test can assert a known overlap
// ? with static_assert.
[[nodiscard]] constexpr float iou(const Bbox& a, const Bbox& b) noexcept {
    const float IX = std::max(0.0f, std::min(a.x2, b.x2) - std::max(a.x1, b.x1));
    const float IY = std::max(0.0f, std::min(a.y2, b.y2) - std::max(a.y1, b.y1));
    const float INTERSECTION = IX * IY;
    const float UNION = a.area() + b.area() - INTERSECTION;
    return UNION > 0.0f ? INTERSECTION / UNION : 0.0f;
}

} // namespace cogidrone