#include "secret/render/aabb_packet.hpp"

#include <immintrin.h>
#include <limits>

namespace secret::render {

std::uint8_t avx2_kernel(const Aabb& bounds, const RayPacket8& rays,
                         const float t_min, const float t_max,
                         const std::uint8_t active_mask) noexcept {
    const __m256 one = _mm256_set1_ps(1.0F);
    const __m256 negative_infinity = _mm256_set1_ps(-std::numeric_limits<float>::infinity());
    const __m256 positive_infinity = _mm256_set1_ps(std::numeric_limits<float>::infinity());
    __m256 near_value = _mm256_set1_ps(t_min);
    __m256 far_value = _mm256_set1_ps(t_max);
    __m256 valid = _mm256_castsi256_ps(_mm256_set1_epi32(-1));

    const auto process_axis = [&](const std::array<float, 8>& origins,
                                  const std::array<float, 8>& directions,
                                  const float slab_min, const float slab_max) {
        const __m256 origin = _mm256_loadu_ps(origins.data());
        const __m256 direction = _mm256_loadu_ps(directions.data());
        const __m256 parallel = _mm256_cmp_ps(_mm256_andnot_ps(
            _mm256_set1_ps(-0.0F), direction), _mm256_set1_ps(1.0e-20F), _CMP_LT_OQ);
        const __m256 outside = _mm256_or_ps(
            _mm256_cmp_ps(origin, _mm256_set1_ps(slab_min), _CMP_LT_OQ),
            _mm256_cmp_ps(origin, _mm256_set1_ps(slab_max), _CMP_GT_OQ));
        valid = _mm256_andnot_ps(_mm256_and_ps(parallel, outside), valid);

        const __m256 safe_direction = _mm256_blendv_ps(direction, one, parallel);
        const __m256 t0 = _mm256_div_ps(_mm256_sub_ps(_mm256_set1_ps(slab_min), origin), safe_direction);
        const __m256 t1 = _mm256_div_ps(_mm256_sub_ps(_mm256_set1_ps(slab_max), origin), safe_direction);
        const __m256 axis_near = _mm256_blendv_ps(_mm256_min_ps(t0, t1), negative_infinity, parallel);
        const __m256 axis_far = _mm256_blendv_ps(_mm256_max_ps(t0, t1), positive_infinity, parallel);
        near_value = _mm256_max_ps(near_value, axis_near);
        far_value = _mm256_min_ps(far_value, axis_far);
        valid = _mm256_and_ps(valid, _mm256_cmp_ps(far_value, near_value, _CMP_GE_OQ));
    };

    process_axis(rays.origin_x, rays.direction_x, bounds.minimum().x(), bounds.maximum().x());
    process_axis(rays.origin_y, rays.direction_y, bounds.minimum().y(), bounds.maximum().y());
    process_axis(rays.origin_z, rays.direction_z, bounds.minimum().z(), bounds.maximum().z());

    const int hit_bits = _mm256_movemask_ps(valid);
    return static_cast<std::uint8_t>(hit_bits & static_cast<int>(active_mask));
}

} // namespace secret::render
