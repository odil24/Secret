#include "secret/math/Math.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>

namespace {

int failures = 0;

void check(const bool condition, const char* label) {
    if (!condition) {
        ++failures;
        std::cerr << "[FAIL] " << label << '\n';
    }
}

void test_scalar() {
    using namespace secret::math;
    check(almost_equal(lerp(2.0F, 6.0F, 0.5F), 4.0F), "lerp");
    check(clamp(5.0F, 0.0F, 1.0F) == 1.0F, "clamp upper bound");
    check(clamp(-1.0F, 0.0F, 1.0F) == 0.0F, "clamp lower bound");
    check(almost_equal(constants::pi * constants::inv_pi, 1.0F), "pi reciprocal");
    check(!almost_equal(std::numeric_limits<float>::infinity(), 1.0F), "infinity comparison");
    check(!almost_equal(std::numeric_limits<float>::quiet_NaN(), 1.0F), "NaN comparison");
}

void test_vec2() {
    using namespace secret::math;
    const Vector2f a{3.0F, 4.0F};
    check(a.length() == 5.0F, "Vector2 length");
    check(a.dot({2.0F, 1.0F}) == 10.0F, "Vector2 dot");
    check(a + Vector2f{1.0F, 2.0F} == Vector2f{4.0F, 6.0F}, "Vector2 addition");
    Vector2f normalized{};
    check(a.try_normalize(normalized), "Vector2 normalize");
    check(almost_equal(normalized.length(), 1.0F), "Vector2 unit length");
    check(!Vector2f{}.try_normalize(normalized), "zero Vector2 rejected");
}

void test_vec3() {
    using namespace secret::math;
    const Vector3f x{1.0F, 0.0F, 0.0F};
    const Vector3f y{0.0F, 1.0F, 0.0F};
    check(x.cross(y) == Vector3f{0.0F, 0.0F, 1.0F}, "Vector3 cross");
    check(x.dot(y) == 0.0F, "Vector3 dot");
    check(Vector3f{2.0F, -3.0F, 1.0F}.min_component() == -3.0F, "Vector3 min");
    check(Vector3f{2.0F, -3.0F, 1.0F}.max_component() == 2.0F, "Vector3 max");
    Vector3f normalized{};
    check(!Vector3f{}.try_normalize(normalized), "zero Vector3 rejected");
    check(normalized == Vector3f{}, "failed normalization output");
    check(Vector3f{1.0F, 2.0F, 3.0F}.hadamard({2.0F, 3.0F, 4.0F}) ==
          Vector3f{2.0F, 6.0F, 12.0F}, "Vector3 Hadamard");
}

void test_vec4() {
    using namespace secret::math;
    const Vector4f value{1.0F, 2.0F, 3.0F, 4.0F};
    check(value.dot(value) == 30.0F, "Vector4 dot");
    check(value.hadamard(Vector4f{2.0F}) == Vector4f{2.0F, 4.0F, 6.0F, 8.0F},
          "Vector4 Hadamard");
    check(value.is_finite(), "Vector4 finite");
    check(!Vector4f{1.0F, 2.0F, std::numeric_limits<float>::infinity(), 4.0F}.is_finite(),
          "Vector4 detects infinity");
}

} // namespace

int main() {
    test_scalar();
    test_vec2();
    test_vec3();
    test_vec4();

    if (failures != 0) {
        std::cerr << failures << " math test(s) failed.\n";
        return EXIT_FAILURE;
    }
    std::cout << "All Secret Math tests passed.\n";
    return EXIT_SUCCESS;
}
