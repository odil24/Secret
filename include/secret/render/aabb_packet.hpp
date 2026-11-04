#pragma once

#include "secret/render/aabb.hpp"
#include "secret/render/ray_packet.hpp"

#include <cstdint>

namespace secret::render {

// Returns one bit per ray lane. Inactive lanes are always zero.
[[nodiscard]] std::uint8_t intersect_aabb_packet8(
    const Aabb& bounds, const RayPacket8& rays,
    float t_min, float t_max, std::uint8_t active_mask = 0xFFU) noexcept;

[[nodiscard]] bool aabb_packet8_uses_avx2() noexcept;

} // namespace secret::render
