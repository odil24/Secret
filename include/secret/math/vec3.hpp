#pragma once

#include <algorithm>
#include <cmath>
#include <ostream>

namespace secret::math {

class Vec3 final {
public:
    constexpr Vec3() noexcept = default;
    constexpr explicit Vec3(const float value) noexcept : x_(value), y_(value), z_(value) {}
    constexpr Vec3(const float x, const float y, const float z) noexcept : x_(x), y_(y), z_(z) {}

    [[nodiscard]] constexpr float x() const noexcept { return x_; }
    [[nodiscard]] constexpr float y() const noexcept { return y_; }
    [[nodiscard]] constexpr float z() const noexcept { return z_; }

    [[nodiscard]] constexpr Vec3 operator-() const noexcept { return {-x_, -y_, -z_}; }
    constexpr Vec3& operator+=(const Vec3& rhs) noexcept {
        x_ += rhs.x_; y_ += rhs.y_; z_ += rhs.z_; return *this;
    }
    constexpr Vec3& operator-=(const Vec3& rhs) noexcept {
        x_ -= rhs.x_; y_ -= rhs.y_; z_ -= rhs.z_; return *this;
    }
    constexpr Vec3& operator*=(const float scalar) noexcept {
        x_ *= scalar; y_ *= scalar; z_ *= scalar; return *this;
    }
    constexpr Vec3& operator/=(const float scalar) noexcept {
        const float inverse = 1.0F / scalar;
        return *this *= inverse;
    }

    [[nodiscard]] constexpr float length_squared() const noexcept { return x_ * x_ + y_ * y_ + z_ * z_; }
    [[nodiscard]] float length() const noexcept { return std::sqrt(length_squared()); }
    [[nodiscard]] constexpr bool near_zero() const noexcept {
        constexpr float epsilon = 1.0e-8F;
        return std::abs(x_) < epsilon && std::abs(y_) < epsilon && std::abs(z_) < epsilon;
    }

private:
    float x_ = 0.0F;
    float y_ = 0.0F;
    float z_ = 0.0F;
};

[[nodiscard]] constexpr Vec3 operator+(Vec3 lhs, const Vec3& rhs) noexcept { return lhs += rhs; }
[[nodiscard]] constexpr Vec3 operator-(Vec3 lhs, const Vec3& rhs) noexcept { return lhs -= rhs; }
[[nodiscard]] constexpr Vec3 operator*(Vec3 lhs, const float rhs) noexcept { return lhs *= rhs; }
[[nodiscard]] constexpr Vec3 operator*(const float lhs, Vec3 rhs) noexcept { return rhs *= lhs; }
[[nodiscard]] constexpr Vec3 operator/(Vec3 lhs, const float rhs) noexcept { return lhs /= rhs; }
[[nodiscard]] constexpr Vec3 operator*(const Vec3& lhs, const Vec3& rhs) noexcept {
    return {lhs.x() * rhs.x(), lhs.y() * rhs.y(), lhs.z() * rhs.z()};
}
[[nodiscard]] constexpr float dot(const Vec3& lhs, const Vec3& rhs) noexcept {
    return lhs.x() * rhs.x() + lhs.y() * rhs.y() + lhs.z() * rhs.z();
}
[[nodiscard]] constexpr Vec3 cross(const Vec3& lhs, const Vec3& rhs) noexcept {
    return {lhs.y() * rhs.z() - lhs.z() * rhs.y(),
            lhs.z() * rhs.x() - lhs.x() * rhs.z(),
            lhs.x() * rhs.y() - lhs.y() * rhs.x()};
}
[[nodiscard]] inline Vec3 unit_vector(const Vec3& value) noexcept { return value / value.length(); }
[[nodiscard]] inline Vec3 reflect(const Vec3& value, const Vec3& normal) noexcept {
    return value - 2.0F * dot(value, normal) * normal;
}
[[nodiscard]] inline Vec3 refract(const Vec3& value, const Vec3& normal, const float eta_ratio) noexcept {
    const float cos_theta = std::min(dot(-value, normal), 1.0F);
    const Vec3 perpendicular = eta_ratio * (value + cos_theta * normal);
    const Vec3 parallel = -std::sqrt(std::abs(1.0F - perpendicular.length_squared())) * normal;
    return perpendicular + parallel;
}
[[nodiscard]] inline Vec3 clamp(const Vec3& value, const float low, const float high) noexcept {
    return {std::clamp(value.x(), low, high),
            std::clamp(value.y(), low, high),
            std::clamp(value.z(), low, high)};
}

inline std::ostream& operator<<(std::ostream& stream, const Vec3& value) {
    return stream << value.x() << ' ' << value.y() << ' ' << value.z();
}

using Color = Vec3;
using Point3 = Vec3;

} // namespace secret::math
