#pragma once

#include <cmath>


namespace cogidrone {
inline constexpr float kPi = 3.14159265358979323846f;


struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    [[nodiscard]] inline constexpr Vec3 operator+(const Vec3& o) const noexcept { return {x + o.x, y + o.y, z + o.z}; }
    [[nodiscard]] inline constexpr Vec3 operator-(const Vec3& o) const noexcept { return {x - o.x, y - o.y, z - o.z}; }
    [[nodiscard]] inline constexpr Vec3 operator*(float s) const noexcept { return {x*s, y*s, z*s}; }
    
    [[nodiscard]] inline constexpr float norm() const noexcept { return std::sqrt(x*x + y*y + z*z); }
    [[nodiscard]] inline constexpr Vec3 normalized() const noexcept {                   // ? zero-safe
        const float N = norm();
        return N > 0.0f ? Vec3{x / N, y / N, z / N} : Vec3{};
    };
};

struct Quat {
    float w = 1.0f;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};



[[nodiscard]] inline constexpr Vec3 rotate(const Quat& q, const Vec3& v) noexcept {     // v' = q * v * q^-1
    // ? Rodrigues' rotation formula: v' = v + 2w(u x v) + 2(u x (u x v)), u = q.xyz
    const Vec3 u{q.x, q.y, q.z};
    const Vec3 t{
        2.0f * (u.y * v.z - u.z * v.y),
        2.0f * (u.z * v.x - u.x * v.z),
        2.0f * (u.x * v.y - u.y * v.x)
    };
    
    return v + t * q.w + Vec3{
        u.y * t.z - u.z * t.y,
        u.z * t.x - u.x * t.z,
        u.x * t.y - u.y * t.x
    };
}

[[nodiscard]] inline constexpr float yawOf(const Quat& q) noexcept {                    // ZYX yaw extraction
    return std::atan2(2.0f * (q.w * q.z + q.x * q.y),
                      1.0f - 2.0f * (q.y * q.y + q.z * q.z));
};

// Normalize an angle to [-pi, pi]
[[nodiscard]] inline constexpr float normalizeAngle(float a) noexcept {
    while (a > kPi)  a -= 2.0f * kPi;
    while (a < -kPi) a += 2.0f * kPi;
    return a;
};

} // namespace cogidrone

