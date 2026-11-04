#pragma once
#include <cmath>
#include <concepts>
#include <ostream>
namespace secret::math {
template<std::floating_point T> struct Vector3 final {
  T x{}; T y{}; T z{};
  constexpr Vector3() noexcept = default;
  constexpr explicit Vector3(T v) noexcept : x(v),y(v),z(v) {}
  constexpr Vector3(T x_,T y_,T z_) noexcept : x(x_),y(y_),z(z_) {}
  [[nodiscard]] constexpr T& operator[](unsigned i) noexcept { return i==0U?x:(i==1U?y:z); }
  [[nodiscard]] constexpr const T& operator[](unsigned i) const noexcept { return i==0U?x:(i==1U?y:z); }
  [[nodiscard]] constexpr Vector3 operator-() const noexcept { return {-x,-y,-z}; }
  [[nodiscard]] constexpr Vector3& operator+=(Vector3 r) noexcept { x+=r.x;y+=r.y;z+=r.z;return *this; }
  [[nodiscard]] constexpr Vector3& operator-=(Vector3 r) noexcept { x-=r.x;y-=r.y;z-=r.z;return *this; }
  [[nodiscard]] constexpr Vector3& operator*=(T s) noexcept { x*=s;y*=s;z*=s;return *this; }
  [[nodiscard]] constexpr Vector3& operator/=(T s) noexcept { x/=s;y/=s;z/=s;return *this; }
  [[nodiscard]] constexpr T length_squared() const noexcept { return x*x+y*y+z*z; }
  [[nodiscard]] T length() const noexcept { return std::sqrt(length_squared()); }
  [[nodiscard]] constexpr T dot(Vector3 r) const noexcept { return x*r.x+y*r.y+z*r.z; }
  [[nodiscard]] constexpr Vector3 cross(Vector3 r) const noexcept { return {y*r.z-z*r.y,z*r.x-x*r.z,x*r.y-y*r.x}; }
  [[nodiscard]] constexpr Vector3 hadamard(Vector3 r) const noexcept { return {x*r.x,y*r.y,z*r.z}; }
  [[nodiscard]] constexpr T min_component() const noexcept { return x<y?(x<z?x:z):(y<z?y:z); }
  [[nodiscard]] constexpr T max_component() const noexcept { return x>y?(x>z?x:z):(y>z?y:z); }
  [[nodiscard]] bool is_finite() const noexcept { return std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z); }
  [[nodiscard]] bool try_normalize(Vector3& out,T min_len=static_cast<T>(1.0e-12)) const noexcept {
    const T l2=length_squared(); if (!std::isfinite(l2)||l2<=min_len*min_len) { out={};return false; }
    out=*this*(static_cast<T>(1)/std::sqrt(l2));return true;
  }
};
template<std::floating_point T> [[nodiscard]] constexpr Vector3<T> operator+(Vector3<T> a,Vector3<T> b) noexcept { return a+=b; }
template<std::floating_point T> [[nodiscard]] constexpr Vector3<T> operator-(Vector3<T> a,Vector3<T> b) noexcept { return a-=b; }
template<std::floating_point T> [[nodiscard]] constexpr Vector3<T> operator*(Vector3<T> a,T s) noexcept { return a*=s; }
template<std::floating_point T> [[nodiscard]] constexpr Vector3<T> operator*(T s,Vector3<T> a) noexcept { return a*=s; }
template<std::floating_point T> [[nodiscard]] constexpr Vector3<T> operator/(Vector3<T> a,T s) noexcept { return a/=s; }
template<std::floating_point T> [[nodiscard]] constexpr bool operator==(Vector3<T> a,Vector3<T> b) noexcept { return a.x==b.x&&a.y==b.y&&a.z==b.z; }
template<std::floating_point T> std::ostream& operator<<(std::ostream& os,Vector3<T> v) { return os<<'('<<v.x<<", "<<v.y<<", "<<v.z<<')'; }
using Vector3f=Vector3<float>; using Vector3d=Vector3<double>;
}
