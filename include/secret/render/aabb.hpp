#pragma once

#include "secret/core/ray.hpp"
#include "secret/math/vec3.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace secret::render {

class Aabb final {
public:
    constexpr Aabb() noexcept
        : minimum_(std::numeric_limits<float>::infinity()),
          maximum_(-std::numeric_limits<float>::infinity()) {}
    constexpr Aabb(const math::Point3& minimum, const math::Point3& maximum) noexcept
        : minimum_(minimum), maximum_(maximum) {}

    [[nodiscard]] constexpr const math::Point3& minimum() const noexcept { return minimum_; }
    [[nodiscard]] constexpr const math::Point3& maximum() const noexcept { return maximum_; }
    [[nodiscard]] constexpr math::Vec3 extent() const noexcept { return maximum_ - minimum_; }
    [[nodiscard]] constexpr float surface_area() const noexcept {
        const auto d = extent();
        return 2.0F * (d.x() * d.y() + d.x() * d.z() + d.y() * d.z());
    }
    [[nodiscard]] constexpr math::Point3 centroid() const noexcept {
        return 0.5F * (minimum_ + maximum_);
    }
    [[nodiscard]] constexpr bool valid() const noexcept {
        return minimum_.x() <= maximum_.x() && minimum_.y() <= maximum_.y() &&
               minimum_.z() <= maximum_.z();
    }

    [[nodiscard]] bool hit(const core::Ray& ray, float t_min, float t_max) const noexcept {
        for (int axis = 0; axis < 3; ++axis) {
            const float origin = component(ray.origin(), axis);
            const float direction = component(ray.direction(), axis);
            const float low = component(minimum_, axis);
            const float high = component(maximum_, axis);
            if (std::abs(direction) < 1.0e-20F) {
                if (origin < low || origin > high) return false;
                continue;
            }
            const float inverse = 1.0F / direction;
            float near_t = (low - origin) * inverse;
            float far_t = (high - origin) * inverse;
            if (near_t > far_t) std::swap(near_t, far_t);
            t_min = std::max(t_min, near_t);
            t_max = std::min(t_max, far_t);
            if (t_max < t_min) return false;
        }
        return true;
    }

    [[nodiscard]] static constexpr float component(const math::Vec3& v, int axis) noexcept {
        return axis == 0 ? v.x() : (axis == 1 ? v.y() : v.z());
    }

private:
    math::Point3 minimum_;
    math::Point3 maximum_;
};

[[nodiscard]] inline Aabb surrounding_box(const Aabb& a, const Aabb& b) noexcept {
    return Aabb(
        math::Point3(std::min(a.minimum().x(), b.minimum().x()),
                     std::min(a.minimum().y(), b.minimum().y()),
                     std::min(a.minimum().z(), b.minimum().z())),
        math::Point3(std::max(a.maximum().x(), b.maximum().x()),
                     std::max(a.maximum().y(), b.maximum().y()),
                     std::max(a.maximum().z(), b.maximum().z())));
}

} // namespace secret::render
