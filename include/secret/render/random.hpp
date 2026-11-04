#pragma once

#include "secret/math/vec3.hpp"

#include <cstdint>
#include <random>

namespace secret::render {

class Random final {
public:
    explicit Random(const std::uint32_t seed) : engine_(seed) {}

    [[nodiscard]] float uniform() {
        return distribution_(engine_);
    }

    [[nodiscard]] float uniform(const float min_value, const float max_value) {
        return min_value + (max_value - min_value) * uniform();
    }

    [[nodiscard]] math::Vec3 in_unit_sphere() {
        for (;;) {
            const math::Vec3 point{
                uniform(-1.0F, 1.0F),
                uniform(-1.0F, 1.0F),
                uniform(-1.0F, 1.0F)
            };
            if (point.length_squared() < 1.0F) {
                return point;
            }
        }
    }

    [[nodiscard]] math::Vec3 unit_vector() {
        return math::unit_vector(in_unit_sphere());
    }

    [[nodiscard]] math::Vec3 in_unit_disk() {
        for (;;) {
            const math::Vec3 point{uniform(-1.0F, 1.0F), uniform(-1.0F, 1.0F), 0.0F};
            if (point.length_squared() < 1.0F) {
                return point;
            }
        }
    }

private:
    std::mt19937 engine_;
    std::uniform_real_distribution<float> distribution_{0.0F, 1.0F};
};

} // namespace secret::render
