#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>

namespace secret::math {

namespace constants {
inline constexpr float pi = std::numbers::pi_v<float>;
inline constexpr float inv_pi = 1.0F / pi;
inline constexpr float two_pi = 2.0F * pi;
inline constexpr float half_pi = 0.5F * pi;
} // namespace constants

[[nodiscard]] constexpr float lerp(const float a, const float b, const float t) noexcept {
    return a + (b - a) * t;
}

[[nodiscard]] constexpr float clamp(const float value,
                                    const float low,
                                    const float high) noexcept {
    return std::clamp(value, low, high);
}

[[nodiscard]] inline bool almost_equal(const float a,
                                       const float b,
                                       const float absolute_tolerance = 1.0e-6F,
                                       const float relative_tolerance = 1.0e-5F) noexcept {
    if (a == b) {
        return true;
    }
    if (!std::isfinite(a) || !std::isfinite(b)) {
        return false;
    }
    const float difference = std::abs(a - b);
    const float scale = std::max(std::abs(a), std::abs(b));
    return difference <= absolute_tolerance + relative_tolerance * scale;
}

class Vector2f final {
public:
    constexpr Vector2f() noexcept = default;
    constexpr explicit Vector2f(const float value) noexcept : x_(value), y_(value) {}
    constexpr Vector2f(const float x, const float y) noexcept : x_(x), y_(y) {}

    [[nodiscard]] constexpr float x() const noexcept { return x_; }
    [[nodiscard]] constexpr float y() const noexcept { return y_; }
    [[nodiscard]] constexpr float length_squared() const noexcept { return x_ * x_ + y_ * y_; }
    [[nodiscard]] float length() const noexcept { return std::sqrt(length_squared()); }
    [[nodiscard]] constexpr float dot(const Vector2f& rhs) const noexcept {
        return x_ * rhs.x_ + y_ * rhs.y_;
    }
    [[nodiscard]] bool try_normalize(Vector2f& output) const noexcept {
        const float squared = length_squared();
        if (!std::isfinite(squared) || squared <= 1.0e-20F) {
            output = {};
            return false;
        }
        const float inverse_length = 1.0F / std::sqrt(squared);
        output = {x_ * inverse_length, y_ * inverse_length};
        return true;
    }

    constexpr Vector2f& operator+=(const Vector2f& rhs) noexcept {
        x_ += rhs.x_; y_ += rhs.y_; return *this;
    }
    constexpr Vector2f& operator-=(const Vector2f& rhs) noexcept {
        x_ -= rhs.x_; y_ -= rhs.y_; return *this;
    }
    constexpr Vector2f& operator*=(const float scalar) noexcept {
        x_ *= scalar; y_ *= scalar; return *this;
    }
    [[nodiscard]] constexpr bool operator==(const Vector2f&) const noexcept = default;

private:
    float x_ = 0.0F;
    float y_ = 0.0F;
};

[[nodiscard]] constexpr Vector2f operator+(Vector2f lhs, const Vector2f& rhs) noexcept { return lhs += rhs; }
[[nodiscard]] constexpr Vector2f operator-(Vector2f lhs, const Vector2f& rhs) noexcept { return lhs -= rhs; }
[[nodiscard]] constexpr Vector2f operator*(Vector2f value, const float scalar) noexcept { return value *= scalar; }
[[nodiscard]] constexpr Vector2f operator*(const float scalar, Vector2f value) noexcept { return value *= scalar; }

class Vector3f final {
public:
    constexpr Vector3f() noexcept = default;
    constexpr explicit Vector3f(const float value) noexcept : x_(value), y_(value), z_(value) {}
    constexpr Vector3f(const float x, const float y, const float z) noexcept : x_(x), y_(y), z_(z) {}

    [[nodiscard]] constexpr float x() const noexcept { return x_; }
    [[nodiscard]] constexpr float y() const noexcept { return y_; }
    [[nodiscard]] constexpr float z() const noexcept { return z_; }
    [[nodiscard]] constexpr float length_squared() const noexcept { return x_ * x_ + y_ * y_ + z_ * z_; }
    [[nodiscard]] float length() const noexcept { return std::sqrt(length_squared()); }
    [[nodiscard]] constexpr float dot(const Vector3f& rhs) const noexcept {
        return x_ * rhs.x_ + y_ * rhs.y_ + z_ * rhs.z_;
    }
    [[nodiscard]] constexpr Vector3f cross(const Vector3f& rhs) const noexcept {
        return {y_ * rhs.z_ - z_ * rhs.y_, z_ * rhs.x_ - x_ * rhs.z_, x_ * rhs.y_ - y_ * rhs.x_};
    }
    [[nodiscard]] constexpr float min_component() const noexcept { return std::min({x_, y_, z_}); }
    [[nodiscard]] constexpr float max_component() const noexcept { return std::max({x_, y_, z_}); }
    [[nodiscard]] constexpr Vector3f hadamard(const Vector3f& rhs) const noexcept {
        return {x_ * rhs.x_, y_ * rhs.y_, z_ * rhs.z_};
    }
    [[nodiscard]] bool is_finite() const noexcept {
        return std::isfinite(x_) && std::isfinite(y_) && std::isfinite(z_);
    }
    [[nodiscard]] bool try_normalize(Vector3f& output) const noexcept {
        const float squared = length_squared();
        if (!std::isfinite(squared) || squared <= 1.0e-20F) {
            output = {};
            return false;
        }
        const float inverse_length = 1.0F / std::sqrt(squared);
        output = {x_ * inverse_length, y_ * inverse_length, z_ * inverse_length};
        return true;
    }

    constexpr Vector3f& operator+=(const Vector3f& rhs) noexcept {
        x_ += rhs.x_; y_ += rhs.y_; z_ += rhs.z_; return *this;
    }
    constexpr Vector3f& operator-=(const Vector3f& rhs) noexcept {
        x_ -= rhs.x_; y_ -= rhs.y_; z_ -= rhs.z_; return *this;
    }
    constexpr Vector3f& operator*=(const float scalar) noexcept {
        x_ *= scalar; y_ *= scalar; z_ *= scalar; return *this;
    }
    [[nodiscard]] constexpr bool operator==(const Vector3f&) const noexcept = default;

private:
    float x_ = 0.0F;
    float y_ = 0.0F;
    float z_ = 0.0F;
};

[[nodiscard]] constexpr Vector3f operator+(Vector3f lhs, const Vector3f& rhs) noexcept { return lhs += rhs; }
[[nodiscard]] constexpr Vector3f operator-(Vector3f lhs, const Vector3f& rhs) noexcept { return lhs -= rhs; }
[[nodiscard]] constexpr Vector3f operator*(Vector3f value, const float scalar) noexcept { return value *= scalar; }
[[nodiscard]] constexpr Vector3f operator*(const float scalar, Vector3f value) noexcept { return value *= scalar; }

class Vector4f final {
public:
    constexpr Vector4f() noexcept = default;
    constexpr explicit Vector4f(const float value) noexcept : x_(value), y_(value), z_(value), w_(value) {}
    constexpr Vector4f(const float x, const float y, const float z, const float w) noexcept
        : x_(x), y_(y), z_(z), w_(w) {}

    [[nodiscard]] constexpr float x() const noexcept { return x_; }
    [[nodiscard]] constexpr float y() const noexcept { return y_; }
    [[nodiscard]] constexpr float z() const noexcept { return z_; }
    [[nodiscard]] constexpr float w() const noexcept { return w_; }
    [[nodiscard]] constexpr float dot(const Vector4f& rhs) const noexcept {
        return x_ * rhs.x_ + y_ * rhs.y_ + z_ * rhs.z_ + w_ * rhs.w_;
    }
    [[nodiscard]] constexpr Vector4f hadamard(const Vector4f& rhs) const noexcept {
        return {x_ * rhs.x_, y_ * rhs.y_, z_ * rhs.z_, w_ * rhs.w_};
    }
    [[nodiscard]] bool is_finite() const noexcept {
        return std::isfinite(x_) && std::isfinite(y_) && std::isfinite(z_) && std::isfinite(w_);
    }
    [[nodiscard]] constexpr bool operator==(const Vector4f&) const noexcept = default;

private:
    float x_ = 0.0F;
    float y_ = 0.0F;
    float z_ = 0.0F;
    float w_ = 0.0F;
};

} // namespace secret::math
