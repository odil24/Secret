#pragma once
#include <cmath>
#include <concepts>
#include <ostream>
namespace secret::math {
template<std::floating_point T> struct Vector2 final {
  T x{}; T y{};
  constexpr Vector2() noexcept = default;
  constexpr explicit Vector2(T v) noexcept : x(v), y(v) {}
  constexpr Vector2(T x_, T y_) noexcept : x(x_), y(y_) {}
  [[nodiscard]] constexpr T& operator[](unsigned i) noexcept { return i == 0U ? x : y; }
  [[nodiscard]] constexpr const T& operator[](unsigned i) const noexcept { return i == 0U ? x : y; }
  [[nodiscard]] constexpr Vector2 operator-() const noexcept { return {-x,-y}; }
  [[nodiscard]] constexpr Vector2& operator+=(Vector2 r) noexcept { x+=r.x; y+=r.y; return *this; }
  [[nodiscard]] constexpr Vector2& operator-=(Vector2 r) noexcept { x-=r.x; y-=r.y; return *this; }
  [[nodiscard]] constexpr Vector2& operator*=(T s) noexcept { x*=s; y*=s; return *this; }
  [[nodiscard]] constexpr Vector2& operator/=(T s) noexcept { x/=s; y/=s; return *this; }
  [[nodiscard]] constexpr T length_squared() const noexcept { return x*x+y*y; }
  [[nodiscard]] T length() const noexcept { return std::sqrt(length_squared()); }
  [[nodiscard]] constexpr T dot(Vector2 r) const noexcept { return x*r.x+y*r.y; }
  [[nodiscard]] constexpr Vector2 hadamard(Vector2 r) const noexcept { return {x*r.x,y*r.y}; }
  [[nodiscard]] bool is_finite() const noexcept { return std::isfinite(x)&&std::isfinite(y); }
  [[nodiscard]] bool try_normalize(Vector2& out, T min_len=static_cast<T>(1.0e-12)) const noexcept {
    const T l2=length_squared(); if (!std::isfinite(l2)||l2<=min_len*min_len) { out={}; return false; }
    out=*this*(static_cast<T>(1)/std::sqrt(l2)); return true;
  }
};
template<std::floating_point T> [[nodiscard]] constexpr Vector2<T> operator+(Vector2<T> a,Vector2<T> b) noexcept { return a+=b; }
template<std::floating_point T> [[nodiscard]] constexpr Vector2<T> operator-(Vector2<T> a,Vector2<T> b) noexcept { return a-=b; }
template<std::floating_point T> [[nodiscard]] constexpr Vector2<T> operator*(Vector2<T> a,T s) noexcept { return a*=s; }
template<std::floating_point T> [[nodiscard]] constexpr Vector2<T> operator*(T s,Vector2<T> a) noexcept { return a*=s; }
template<std::floating_point T> [[nodiscard]] constexpr Vector2<T> operator/(Vector2<T> a,T s) noexcept { return a/=s; }
template<std::floating_point T> [[nodiscard]] constexpr bool operator==(Vector2<T> a,Vector2<T> b) noexcept { return a.x==b.x&&a.y==b.y; }
template<std::floating_point T> std::ostream& operator<<(std::ostream& os,Vector2<T> v) { return os<<'('<<v.x<<", "<<v.y<<')'; }
using Vector2f=Vector2<float>; using Vector2d=Vector2<double>;
}
