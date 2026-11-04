#include "secret/render/hittable.hpp"
#include "secret/render/random.hpp"
#include <bit>

#include <array>
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
    const render::Sphere sphere(math::Point3(0.0F, 0.0F, -3.0F), 1.0F, material);
    constexpr int batches = 250'000;
    std::uint64_t scalar_hits = 0;
    std::uint64_t packet_hits = 0;
    render::Random random(0x70B7U);
    std::vector<render::RayPacket8> packets(static_cast<std::size_t>(batches));
    std::array<float, 8> tmax{};
    std::array<float, 8> roots{};
    tmax.fill(1000.0F);
    for (auto& packet : packets) {
        for (std::size_t lane = 0; lane < 8; ++lane) {
            const math::Point3 origin(random.uniform() * 8.0F - 4.0F,
                                      random.uniform() * 6.0F - 3.0F,
                                      random.uniform() * 4.0F + 0.5F);
            const math::Vec3 direction = math::unit_vector(math::Vec3(
                random.uniform() * 1.4F - 0.7F,
                random.uniform() * 1.4F - 0.7F, -1.0F));
            packet.set(lane, core::Ray(origin, direction));
        }
    }

    const auto scalar_start = Clock::now();
    for (const auto& packet : packets) {
        for (std::size_t lane = 0; lane < 8; ++lane) {
            const core::Ray ray(math::Point3(packet.origin_x[lane], packet.origin_y[lane], packet.origin_z[lane]),
                                math::Vec3(packet.direction_x[lane], packet.direction_y[lane], packet.direction_z[lane]));
            render::HitRecord hit{};
            if (sphere.hit(ray, 0.001F, 1000.0F, hit)) ++scalar_hits;
        }
    }
    const auto scalar_end = Clock::now();

    const auto packet_start = Clock::now();
    for (const auto& packet : packets) {
        packet_hits += static_cast<std::uint64_t>(std::popcount(
            render::intersect_sphere_packet8(math::Point3(0.0F, 0.0F, -3.0F), 1.0F,
                                             packet, 0.001F, tmax, roots)));
    }
    const auto packet_end = Clock::now();
    const double scalar_ms = std::chrono::duration<double, std::milli>(scalar_end - scalar_start).count();
    const double packet_ms = std::chrono::duration<double, std::milli>(packet_end - packet_start).count();
    std::cout << "AVX2 active: " << (render::sphere_packet8_uses_avx2() ? "yes" : "no") << '\n'
              << "Rays: " << (static_cast<std::uint64_t>(batches) * 8U) << '\n'
              << "Scalar: " << scalar_ms << " ms, hits=" << scalar_hits << '\n'
              << "Packet: " << packet_ms << " ms, hits=" << packet_hits << '\n'
              << "Speedup: " << (scalar_ms / packet_ms) << "x\n";
    return scalar_hits == packet_hits ? 0 : 2;
}
