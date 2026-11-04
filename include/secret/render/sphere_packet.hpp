#pragma once

#include "secret/math/vec3.hpp"
#include "secret/render/ray_packet.hpp"

#include <array>
#include <cstdint>

namespace secret::render {

// Returns one hit bit per lane and writes the nearest valid root for hit lanes.
[[nodiscard]] std::uint8_t intersect_sphere_packet8(
    const math::Point3& center, float radius, const RayPacket8& rays,
    float t_min, const std::array<float, 8>& t_max,
    std::array<float, 8>& roots, std::uint8_t active_mask = 0xFFU) noexcept;

[[nodiscard]] bool sphere_packet8_uses_avx2() noexcept;

} // namespace secret::render
