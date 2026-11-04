#include "secret/render/sphere_packet.hpp"

#include <immintrin.h>
#include <cmath>

namespace secret::render {

std::uint8_t sphere_packet8_avx2(const math::Point3& center, const float radius,
                                 const RayPacket8& rays, const float t_min,
                                 const std::array<float, 8>& t_max,
                                 std::array<float, 8>& roots,
                                 const std::uint8_t active_mask) noexcept {
    const __m256 ox = _mm256_sub_ps(_mm256_loadu_ps(rays.origin_x.data()), _mm256_set1_ps(center.x()));
    const __m256 oy = _mm256_sub_ps(_mm256_loadu_ps(rays.origin_y.data()), _mm256_set1_ps(center.y()));
    const __m256 oz = _mm256_sub_ps(_mm256_loadu_ps(rays.origin_z.data()), _mm256_set1_ps(center.z()));
    const __m256 dx = _mm256_loadu_ps(rays.direction_x.data());
    const __m256 dy = _mm256_loadu_ps(rays.direction_y.data());
    const __m256 dz = _mm256_loadu_ps(rays.direction_z.data());
    const __m256 a = _mm256_add_ps(_mm256_mul_ps(dx, dx),
        _mm256_add_ps(_mm256_mul_ps(dy, dy), _mm256_mul_ps(dz, dz)));
    const __m256 half_b = _mm256_add_ps(_mm256_mul_ps(ox, dx),
        _mm256_add_ps(_mm256_mul_ps(oy, dy), _mm256_mul_ps(oz, dz)));
    const __m256 c = _mm256_sub_ps(_mm256_add_ps(_mm256_mul_ps(ox, ox),
        _mm256_add_ps(_mm256_mul_ps(oy, oy), _mm256_mul_ps(oz, oz))),
        _mm256_set1_ps(radius * radius));
    const __m256 discriminant = _mm256_sub_ps(_mm256_mul_ps(half_b, half_b), _mm256_mul_ps(a, c));
    const __m256 valid_a = _mm256_cmp_ps(a, _mm256_setzero_ps(), _CMP_GT_OQ);
    const __m256 valid_disc = _mm256_cmp_ps(discriminant, _mm256_setzero_ps(), _CMP_GE_OQ);
    const __m256 safe_a = _mm256_blendv_ps(_mm256_set1_ps(1.0F), a, valid_a);
    const __m256 delta = _mm256_sqrt_ps(_mm256_max_ps(discriminant, _mm256_setzero_ps()));
    const __m256 first = _mm256_div_ps(_mm256_sub_ps(_mm256_sub_ps(_mm256_setzero_ps(), half_b), delta), safe_a);
    const __m256 second = _mm256_div_ps(_mm256_add_ps(_mm256_sub_ps(_mm256_setzero_ps(), half_b), delta), safe_a);
    const __m256 lo = _mm256_set1_ps(t_min);
    const __m256 hi = _mm256_loadu_ps(t_max.data());
    const __m256 first_ok = _mm256_and_ps(_mm256_cmp_ps(first, lo, _CMP_GE_OQ),
                                          _mm256_cmp_ps(first, hi, _CMP_LE_OQ));
    const __m256 second_ok = _mm256_and_ps(_mm256_cmp_ps(second, lo, _CMP_GE_OQ),
                                           _mm256_cmp_ps(second, hi, _CMP_LE_OQ));
    const __m256 choose_first = first_ok;
    const __m256 selected = _mm256_blendv_ps(second, first, choose_first);
    const __m256 valid = _mm256_and_ps(_mm256_and_ps(valid_a, valid_disc),
        _mm256_or_ps(first_ok, second_ok));
    _mm256_storeu_ps(roots.data(), selected);
    return static_cast<std::uint8_t>(_mm256_movemask_ps(valid) & static_cast<int>(active_mask));
}

} // namespace secret::render
