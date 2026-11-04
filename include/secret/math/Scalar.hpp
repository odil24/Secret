#pragma once
#include <algorithm>
#include <cmath>
#include <concepts>
namespace secret::math {
template<std::floating_point T>
[[nodiscard]] constexpr T clamp(T value, T lower, T upper) noexcept { return std::clamp(value, lower, upper); }
template<std::floating_point T>
[[nodiscard]] constexpr T lerp(T a, T b, T t) noexcept { return a + (b - a) * t; }
template<std::floating_point T>
[[nodiscard]] inline bool is_finite(T value) noexcept { return std::isfinite(value); }
template<std::floating_point T>
[[nodiscard]] inline bool almost_equal(T a, T b, T abs_eps = static_cast<T>(1.0e-6), T rel_eps = static_cast<T>(1.0e-5)) noexcept {
  if (a == b) return true;
  if (!std::isfinite(a) || !std::isfinite(b)) return false;
  const T difference = std::abs(a - b);
  const T scale = std::max(std::abs(a), std::abs(b));
  return difference <= std::max(abs_eps, rel_eps * scale);
}
}
