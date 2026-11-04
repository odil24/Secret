#include "secret/render/sphere_packet.hpp"
#include "secret/platform/cpu_features.hpp"

#include <algorithm>
#include <cmath>

namespace secret::render {

std::uint8_t sphere_packet8_avx2(const math::Point3&, float, const RayPacket8&,
                                 float, const std::array<float, 8>&,
                                 std::array<float, 8>&, std::uint8_t) noexcept;

namespace {
using Kernel = std::uint8_t (*)(const math::Point3&, float, const RayPacket8&, float,
                                const std::array<float, 8>&, std::array<float, 8>&,
                                std::uint8_t) noexcept;

std::uint8_t scalar_kernel(const math::Point3& center, const float radius,
                           const RayPacket8& rays, const float t_min,
                           const std::array<float, 8>& t_max,
                           std::array<float, 8>& roots,
                           const std::uint8_t active_mask) noexcept {
    std::uint8_t mask = 0U;
    const float radius_squared = radius * radius;
    for (unsigned lane = 0; lane < 8; ++lane) {
        const auto bit = static_cast<std::uint8_t>(1U << lane);
        if ((active_mask & bit) == 0U) continue;
        const float ox = rays.origin_x[lane] - center.x();
        const float oy = rays.origin_y[lane] - center.y();
        const float oz = rays.origin_z[lane] - center.z();
        const float dx = rays.direction_x[lane];
        const float dy = rays.direction_y[lane];
        const float dz = rays.direction_z[lane];
        const float a = dx * dx + dy * dy + dz * dz;
        if (!(a > 0.0F) || !std::isfinite(a) || !(t_min <= t_max[lane])) continue;
        const float half_b = ox * dx + oy * dy + oz * dz;
        const float c = ox * ox + oy * oy + oz * oz - radius_squared;
        const float discriminant = half_b * half_b - a * c;
        if (!(discriminant >= 0.0F)) continue;
        const float root_delta = std::sqrt(discriminant);
        float root = (-half_b - root_delta) / a;
        if (root < t_min || root > t_max[lane]) root = (-half_b + root_delta) / a;
        if (root < t_min || root > t_max[lane] || !std::isfinite(root)) continue;
        roots[lane] = root;
        mask = static_cast<std::uint8_t>(mask | bit);
    }
    return mask;
}

struct Dispatch final {
    Kernel kernel = scalar_kernel;
    bool avx2 = false;
    Dispatch() noexcept {
        if (platform::detect_cpu_features().avx2) {
            kernel = sphere_packet8_avx2;
            avx2 = true;
        }
    }
};
const Dispatch& dispatch() noexcept { static const Dispatch value; return value; }
} // namespace

std::uint8_t intersect_sphere_packet8(const math::Point3& center, const float radius,
                                      const RayPacket8& rays, const float t_min,
                                      const std::array<float, 8>& t_max,
                                      std::array<float, 8>& roots,
                                      const std::uint8_t active_mask) noexcept {
    if (!(radius > 0.0F) || !std::isfinite(radius) || active_mask == 0U) return 0U;
    return dispatch().kernel(center, radius, rays, t_min, t_max, roots, active_mask);
}

bool sphere_packet8_uses_avx2() noexcept { return dispatch().avx2; }

} // namespace secret::render
