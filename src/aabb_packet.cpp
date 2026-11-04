#include "secret/render/aabb_packet.hpp"
#include "secret/platform/cpu_features.hpp"

namespace secret::render {

#if defined(_MSC_VER) || defined(__GNUC__) || defined(__clang__)
std::uint8_t avx2_kernel(const Aabb&, const RayPacket8&, float, float, std::uint8_t) noexcept;
#endif

namespace {

using Kernel = std::uint8_t (*)(const Aabb&, const RayPacket8&, float, float, std::uint8_t) noexcept;

std::uint8_t scalar_kernel(const Aabb& bounds, const RayPacket8& rays,
                           const float t_min, const float t_max,
                           const std::uint8_t active_mask) noexcept {
    std::uint8_t result = 0;
    for (unsigned int lane = 0; lane < 8; ++lane) {
        const auto bit = static_cast<std::uint8_t>(1U << lane);
        if ((active_mask & bit) == 0U) continue;
        const core::Ray ray(
            math::Point3(rays.origin_x[lane], rays.origin_y[lane], rays.origin_z[lane]),
            math::Vec3(rays.direction_x[lane], rays.direction_y[lane], rays.direction_z[lane]));
        if (bounds.hit(ray, t_min, t_max)) result = static_cast<std::uint8_t>(result | bit);
    }
    return result;
}

struct Dispatch final {
    Kernel kernel = scalar_kernel;
    bool avx2 = false;

    Dispatch() noexcept {
#if defined(_MSC_VER) || defined(__GNUC__) || defined(__clang__)
        if (platform::detect_cpu_features().avx2) {
            kernel = avx2_kernel;
            avx2 = true;
        }
#endif
    }
};

const Dispatch& dispatch() noexcept {
    static const Dispatch value;
    return value;
}

} // namespace

std::uint8_t intersect_aabb_packet8(const Aabb& bounds, const RayPacket8& rays,
                                    const float t_min, const float t_max,
                                    const std::uint8_t active_mask) noexcept {
    if (!bounds.valid() || !(t_min <= t_max) || active_mask == 0U) return 0U;
    return dispatch().kernel(bounds, rays, t_min, t_max, active_mask);
}

bool aabb_packet8_uses_avx2() noexcept { return dispatch().avx2; }

} // namespace secret::render
