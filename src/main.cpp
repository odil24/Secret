#include "secret/math/Math.hpp"
#if defined(SECRET_WITH_EMBREE)
#include "secret/acceleration/embree_device.hpp"
#endif
#include <exception>
#include <iostream>
int main() {
  try {
#if defined(SECRET_WITH_EMBREE)
    const secret::acceleration::EmbreeDevice embree_device;
    (void)embree_device;
#endif
    using secret::math::Vector3f;
    const Vector3f input{1.0F,2.0F,3.0F};
    Vector3f unit{};
    const bool ok=input.try_normalize(unit);
    std::cout << "Secret Renderer 0.2.0\nC++23 math foundation initialized.\n"
              << "Vector3: " << input << "\nNormalized: " << unit << '\n';
#if defined(SECRET_WITH_EMBREE)
    std::cout << "Embree device initialized.\n";
#else
    std::cout << "Embree backend not enabled in this build.\n";
#endif
    return ok ? 0 : 1;
  } catch (const std::exception& error) {
    std::cerr << "Secret initialization failed: " << error.what() << '\n';
    return 1;
  }
}
