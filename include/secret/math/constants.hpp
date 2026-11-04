#pragma once
#include <limits>
namespace secret::math::constants {
inline constexpr float pi = 3.14159265358979323846F;
inline constexpr float two_pi = 2.0F * pi;
inline constexpr float half_pi = 0.5F * pi;
inline constexpr float inv_pi = 1.0F / pi;
inline constexpr float inv_two_pi = 1.0F / two_pi;
inline constexpr float infinity = std::numeric_limits<float>::infinity();
inline constexpr float epsilon = 1.0e-6F;
}
