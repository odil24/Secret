#include "secret/render/aabb_packet.hpp"
#include "secret/render/random.hpp"

#include <bit>
#include <chrono>
#include <cstdint>
#include <iostream>

int main() {
    using namespace secret;
    using clock = std::chrono::steady_clock;
    constexpr int batches = 250'000;
    const render::Aabb bounds(math::Point3(-1.0F, -1.0F, -1.0F),
                              math::Point3(1.0F, 1.0F, 1.0F));
    render::Random random(0x51A7U);
    render::RayPacket8 packet{};
    std::uint64_t scalar_checksum = 0;
    std::uint64_t packet_checksum = 0;

    const auto scalar_start = clock::now();
    for (int batch = 0; batch < batches; ++batch) {
        for (std::size_t lane = 0; lane < 8; ++lane) {
            const math::Point3 origin(random.uniform() * 8.0F - 4.0F,
                                      random.uniform() * 8.0F - 4.0F,
                                      random.uniform() * 8.0F - 4.0F);
            const math::Vec3 direction(random.uniform() * 2.0F - 1.0F,
                                       random.uniform() * 2.0F - 1.0F,
                                       random.uniform() * 2.0F - 1.0F);
            packet.set(lane, core::Ray(origin, direction));
        }
        for (std::size_t lane = 0; lane < 8; ++lane) {
            const core::Ray ray(math::Point3(packet.origin_x[lane], packet.origin_y[lane], packet.origin_z[lane]),
                                math::Vec3(packet.direction_x[lane], packet.direction_y[lane], packet.direction_z[lane]));
            scalar_checksum += bounds.hit(ray, 0.001F, 100.0F) ? 1U : 0U;
        }
    }
    const auto scalar_end = clock::now();

    random = render::Random(0x51A7U);
    const auto packet_start = clock::now();
    for (int batch = 0; batch < batches; ++batch) {
        for (std::size_t lane = 0; lane < 8; ++lane) {
            const math::Point3 origin(random.uniform() * 8.0F - 4.0F,
                                      random.uniform() * 8.0F - 4.0F,
                                      random.uniform() * 8.0F - 4.0F);
            const math::Vec3 direction(random.uniform() * 2.0F - 1.0F,
                                       random.uniform() * 2.0F - 1.0F,
                                       random.uniform() * 2.0F - 1.0F);
            packet.set(lane, core::Ray(origin, direction));
        }
        packet_checksum += static_cast<std::uint64_t>(
            std::popcount(render::intersect_aabb_packet8(bounds, packet, 0.001F, 100.0F)));
    }
    const auto packet_end = clock::now();

    const double scalar_ms = std::chrono::duration<double, std::milli>(scalar_end - scalar_start).count();
    const double packet_ms = std::chrono::duration<double, std::milli>(packet_end - packet_start).count();
    std::cout << "AVX2 active: " << (render::aabb_packet8_uses_avx2() ? "yes" : "no") << '\n'
              << "Rays: " << (static_cast<std::uint64_t>(batches) * 8U) << '\n'
              << "Scalar: " << scalar_ms << " ms, checksum=" << scalar_checksum << '\n'
              << "Packet: " << packet_ms << " ms, checksum=" << packet_checksum << '\n'
              << "Speedup: " << (scalar_ms / packet_ms) << "x\n";
    return scalar_checksum == packet_checksum ? 0 : 2;
}
