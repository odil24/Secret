#include "secret/render/bvh.hpp"
#include "secret/render/random.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

int main() {
    using namespace secret;
    using Clock = std::chrono::steady_clock;

    render::DisneyParameters parameters{};
    const auto material = std::make_shared<render::DisneyMaterial>(parameters);
    render::HittableList linear_scene;
    std::vector<std::shared_ptr<const render::Hittable>> objects;
    objects.reserve(1024);
    for (int z = 0; z < 32; ++z) {
        for (int x = 0; x < 32; ++x) {
            auto sphere = std::make_shared<render::Sphere>(
                math::Point3(static_cast<float>(x) * 0.65F - 10.0F,
                             static_cast<float>((x * 7 + z * 3) % 11) * 0.22F - 1.0F,
                             -3.0F - static_cast<float>(z) * 0.65F), 0.2F, material);
            linear_scene.add(sphere);
            objects.push_back(std::move(sphere));
        }
    }
    const auto build_start = Clock::now();
    const auto bvh = render::BvhNode::build(objects);
    const auto build_end = Clock::now();

    constexpr int ray_count = 30000;
    std::vector<core::Ray> rays;
    rays.reserve(ray_count);
    render::Random random(0x51EC7U);
    for (int i = 0; i < ray_count; ++i) {
        const math::Point3 origin(random.uniform() * 26.0F - 13.0F,
                                  random.uniform() * 9.0F - 4.5F,
                                  2.0F);
        const math::Vec3 direction = math::unit_vector(math::Vec3(
            random.uniform() * 2.0F - 1.0F,
            random.uniform() * 1.2F - 0.6F,
            -random.uniform() - 0.1F));
        rays.emplace_back(origin, direction);
    }

    std::uint64_t linear_hits = 0;
    const auto linear_start = Clock::now();
    for (const auto& ray : rays) {
        render::HitRecord hit{};
        if (linear_scene.hit(ray, 0.001F, 1000.0F, hit)) ++linear_hits;
    }
    const auto linear_end = Clock::now();

    std::uint64_t bvh_hits = 0;
    const auto bvh_start = Clock::now();
    for (const auto& ray : rays) {
        render::HitRecord hit{};
        if (bvh->hit(ray, 0.001F, 1000.0F, hit)) ++bvh_hits;
    }
    const auto bvh_end = Clock::now();

    const auto milliseconds = [](const auto duration) {
        return std::chrono::duration<double, std::milli>(duration).count();
    };
    const double build_ms = milliseconds(build_end - build_start);
    const double linear_ms = milliseconds(linear_end - linear_start);
    const double bvh_ms = milliseconds(bvh_end - bvh_start);
    std::cout << "primitives=" << objects.size() << " rays=" << ray_count << '\n'
              << "bvh_build_ms=" << build_ms << '\n'
              << "linear_ms=" << linear_ms << " hits=" << linear_hits << '\n'
              << "bvh_ms=" << bvh_ms << " hits=" << bvh_hits << '\n'
              << "speedup=" << (linear_ms / bvh_ms) << "x\n";
    return linear_hits == bvh_hits ? 0 : 2;
}
