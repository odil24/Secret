#include "secret/math/Math.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
namespace {
int failures=0;
void check(bool condition,const char* label){if(!condition){++failures;std::cerr<<"[FAIL] "<<label<<'\n';}}
void test_scalar(){using namespace secret::math;check(almost_equal(lerp(2.0F,6.0F,0.5F),4.0F),"lerp");check(clamp(5.0F,0.0F,1.0F)==1.0F,"clamp");check(almost_equal(constants::pi*constants::inv_pi,1.0F),"pi reciprocal");check(!almost_equal(std::numeric_limits<float>::infinity(),1.0F),"infinity comparison");}
void test_vec2(){using namespace secret::math;const Vector2f a{3,4};check(a.length()==5,"Vector2 length");check(a.dot({2,1})==10,"Vector2 dot");check(a+Vector2f{1,2}==Vector2f{4,6},"Vector2 addition");Vector2f n{};check(a.try_normalize(n),"Vector2 normalize");check(almost_equal(n.length(),1.0F),"Vector2 unit length");}
void test_vec3(){using namespace secret::math;const Vector3f x{1,0,0},y{0,1,0};check(x.cross(y)==Vector3f{0,0,1},"Vector3 cross");check(x.dot(y)==0,"Vector3 dot");check(Vector3f{2,-3,1}.min_component()==-3,"Vector3 min");check(Vector3f{2,-3,1}.max_component()==2,"Vector3 max");Vector3f n{};check(!Vector3f{}.try_normalize(n),"zero Vector3 rejected");check(n==Vector3f{},"failed normalization output");}
void test_vec4(){using namespace secret::math;const Vector4f v{1,2,3,4};check(v.dot(v)==30,"Vector4 dot");check(v.hadamard(Vector4f{2})==Vector4f{2,4,6,8},"Vector4 Hadamard");check(v.is_finite(),"Vector4 finite");}
}
int main(){test_scalar();test_vec2();test_vec3();test_vec4();if(failures){std::cerr<<failures<<" test(s) failed.\n";return EXIT_FAILURE;}std::cout<<"All Secret Math tests passed.\n";return EXIT_SUCCESS;}
