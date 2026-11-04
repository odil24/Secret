#pragma once

#include "secret/math/vec3.hpp"

namespace secret::core {

class Ray final {
public:
    constexpr Ray() noexcept = default;
    constexpr Ray(const math::Point3& origin, const math::Vec3& direction) noexcept
        : origin_(origin), direction_(direction) {}

    [[nodiscard]] constexpr const math::Point3& origin() const noexcept { return origin_; }
    [[nodiscard]] constexpr const math::Vec3& direction() const noexcept { return direction_; }
    [[nodiscard]] constexpr math::Point3 at(const float t) const noexcept {
        return origin_ + t * direction_;
    }

private:
    math::Point3 origin_{};
    math::Vec3 direction_{};
};

} // namespace secret::core
