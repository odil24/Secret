#pragma once

#include "secret/core/ray.hpp"
#include "secret/render/random.hpp"

#include <cmath>
#include <numbers>

namespace secret::render {

class Camera final {
public:
    Camera(const math::Point3& look_from,
           const math::Point3& look_at,
           const math::Vec3& view_up,
           const float vertical_fov_degrees,
           const float aspect_ratio,
           const float aperture = 0.0F,
           const float focus_distance = 1.0F) {
        const float theta = vertical_fov_degrees * std::numbers::pi_v<float> / 180.0F;
        const float half_height = std::tan(theta * 0.5F);
        const float viewport_height = 2.0F * half_height;
        const float viewport_width = aspect_ratio * viewport_height;

        w_ = math::unit_vector(look_from - look_at);
        u_ = math::unit_vector(math::cross(view_up, w_));
        v_ = math::cross(w_, u_);

        origin_ = look_from;
        horizontal_ = focus_distance * viewport_width * u_;
        vertical_ = focus_distance * viewport_height * v_;
        lower_left_ = origin_ - horizontal_ * 0.5F - vertical_ * 0.5F - focus_distance * w_;
        lens_radius_ = aperture * 0.5F;
    }

    [[nodiscard]] core::Ray get_ray(const float s, const float t, Random& random) const {
        const math::Vec3 disk = lens_radius_ * random.in_unit_disk();
        const math::Vec3 offset = u_ * disk.x() + v_ * disk.y();
        return core::Ray(origin_ + offset,
                         lower_left_ + s * horizontal_ + t * vertical_ - origin_ - offset);
    }

private:
    math::Point3 origin_{};
    math::Point3 lower_left_{};
    math::Vec3 horizontal_{};
    math::Vec3 vertical_{};
    math::Vec3 u_{};
    math::Vec3 v_{};
    math::Vec3 w_{};
    float lens_radius_ = 0.0F;
};

} // namespace secret::render
