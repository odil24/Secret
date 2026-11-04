#pragma once

#include "secret/core/ray.hpp"

#include <array>
#include <cstddef>

namespace secret::render {

// Structure-of-arrays packet for eight coherent or semi-coherent rays.
struct RayPacket8 final {
    std::array<float, 8> origin_x{};
    std::array<float, 8> origin_y{};
    std::array<float, 8> origin_z{};
    std::array<float, 8> direction_x{};
    std::array<float, 8> direction_y{};
    std::array<float, 8> direction_z{};

    void set(const std::size_t lane, const core::Ray& ray) noexcept {
        origin_x[lane] = ray.origin().x();
        origin_y[lane] = ray.origin().y();
        origin_z[lane] = ray.origin().z();
        direction_x[lane] = ray.direction().x();
        direction_y[lane] = ray.direction().y();
        direction_z[lane] = ray.direction().z();
    }
};

} // namespace secret::render
