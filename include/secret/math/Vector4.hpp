#pragma once
#include <cmath>
#include <concepts>
#include <ostream>
namespace secret::math {
template<std::floating_point T> struct Vector4 final {
  T x{}; T y{}; T z{}; T w{};
  constexpr Vector4() noexcept = default;
  constexpr explicit Vector4(T v) noexcept : x(v),y(v),z(v),w(v) {}
  constexpr Vector4(T x_,T y_,T z_,T w_) noexcept : x(x_),y(y_),z(z_),w(w_) {}
  [[nodiscard]] constexpr T& operator[](unsigned i) noexcept { return i==0U?x:(i==1U?y:(i==2U?z:w)); }
  [[nodiscard]] constexpr const T& operator[](unsigned i) const noexcept { return i==0U?x:(i==1U?y:(i==2U?z:w)); }
  [[nodiscard]] constexpr Vector4 operator-() const noexcept { return {-x,-y,-z,-w}; }
  [[nodiscard]] constexpr Vector4& operator+=(Vector4 r) noexcept { x+=r.x;y+=r.y;z+=r.z;w+=r.w;return *this; }
  [[nodiscard]] constexpr Vector4& operator-=(Vector4 r) noexcept { x-=r.x;y-=r.y;z-=r.z;w-=r.w;return *this; }
  [[nodiscard]] constexpr Vector4& operator*=(T s) noexcept { x*=s;y*=s;z*=s;w*=s;return *this; }
  [[nodiscard]] constexpr Vector4& operator/=(T s) noexcept { x/=s;y/=s;z/=s;w/=s;return *this; }
  [[nodiscard]] constexpr T length_squared() const noexcept { return x*x+y*y+z*z+w*w; }
  [[nodiscard]] T length() const noexcept { return std::sqrt(length_squared()); }
  [[nodiscard]] constexpr T dot(Vector4 r) const noexcept { return x*r.x+y*r.y+z*r.z+w*r.w; }
  [[nodiscard]] constexpr Vector4 hadamard(Vector4 r) const noexcept { return {x*r.x,y*r.y,z*r.z,w*r.w}; }
  [[nodiscard]] bool is_finite() const noexcept { return std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z)&&std::isfinite(w); }
  [[nodiscard]] bool try_normalize(Vector4& out,T min_len=static_cast<T>(1.0e-12)) const noexcept {
    const T l2=length_squared();if(!std::isfinite(l2)||l2<=min_len*min_len){out={};return false;}
    out=*this*(static_cast<T>(1)/std::sqrt(l2));return true;
  }
};
template<std::floating_point T> [[nodiscard]] constexpr Vector4<T> operator+(Vector4<T> a,Vector4<T> b) noexcept { return a+=b; }
template<std::floating_point T> [[nodiscard]] constexpr Vector4<T> operator-(Vector4<T> a,Vector4<T> b) noexcept { return a-=b; }
template<std::floating_point T> [[nodiscard]] constexpr Vector4<T> operator*(Vector4<T> a,T s) noexcept { return a*=s; }
template<std::floating_point T> [[nodiscard]] constexpr Vector4<T> operator*(T s,Vector4<T> a) noexcept { return a*=s; }
template<std::floating_point T> [[nodiscard]] constexpr Vector4<T> operator/(Vector4<T> a,T s) noexcept { return a/=s; }
template<std::floating_point T> [[nodiscard]] constexpr bool operator==(Vector4<T> a,Vector4<T> b) noexcept { return a.x==b.x&&a.y==b.y&&a.z==b.z&&a.w==b.w; }
template<std::floating_point T> std::ostream& operator<<(std::ostream& os,Vector4<T> v) { return os<<'('<<v.x<<", "<<v.y<<", "<<v.z<<", "<<v.w<<')'; }
using Vector4f=Vector4<float>; using Vector4d=Vector4<double>;
}
