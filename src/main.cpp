#include "secret/platform/cpu_features.hpp"
#include "secret/render/renderer.hpp"

#include <charconv>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace {

[[nodiscard]] int parse_positive_int(const std::string_view value, const char* option_name) {
    int result = 0;
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), result);
    if (error != std::errc{} || end != value.data() + value.size() || result <= 0) {
        throw std::invalid_argument(std::string("Invalid value for ") + option_name + ".");
    }
    return result;
}

} // namespace

int main(const int argc, char** argv) {
    try {
        secret::render::RenderConfig config{};
        std::filesystem::path output_path = "render.png";

        for (int index = 1; index < argc; ++index) {
            const std::string_view argument(argv[index]);
            if (argument == "--width" && index + 1 < argc) {
                config.width = parse_positive_int(argv[++index], "--width");
            } else if (argument == "--height" && index + 1 < argc) {
                config.height = parse_positive_int(argv[++index], "--height");
            } else if (argument == "--spp" && index + 1 < argc) {
                config.samples_per_pixel = parse_positive_int(argv[++index], "--spp");
            } else if (argument == "--depth" && index + 1 < argc) {
                config.max_depth = parse_positive_int(argv[++index], "--depth");
            } else if (argument == "--tile" && index + 1 < argc) {
                config.tile_size = parse_positive_int(argv[++index], "--tile");
            } else if (argument == "--output" && index + 1 < argc) {
                output_path = argv[++index];
            } else if (argument == "--help") {
                std::cout << "Secret CPU Path Tracer v0.7\n"
                          << "Usage: secret [--width N] [--height N] [--spp N] [--depth N] [--tile N] [--output FILE.png]\n";
                return 0;
            } else {
                throw std::invalid_argument(std::string("Unknown or incomplete argument: ") + std::string(argument));
            }
        }

        const auto cpu = secret::platform::detect_cpu_features();
        std::cout << "Secret CPU Path Tracer v0.7\n"
                  << "Target: Windows 10 x64 / MSVC / C++23\n"
                  << "CPU features: AVX=" << (cpu.avx ? "yes" : "no")
                  << ", AVX2=" << (cpu.avx2 ? "yes" : "no") << "\n"
                  << "Renderer: packet BVH traversal, AVX2 sphere/AABB kernels, tiled CPU path tracer\n"
                  << "Tile size: " << config.tile_size << "x" << config.tile_size << "\n"
                  << "Resolution: " << config.width << 'x' << config.height
                  << ", samples: " << config.samples_per_pixel
                  << ", depth: " << config.max_depth << "\n";

        const auto world = secret::render::make_demo_scene();
        const auto camera = secret::render::make_demo_camera(
            static_cast<float>(config.width) / static_cast<float>(config.height));
        const secret::render::Renderer renderer(config);
        const auto pixels = renderer.render(world, camera);
        renderer.write_png(output_path, pixels);

        std::cout << "Saved image: " << output_path.string() << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Secret failed: " << error.what() << '\n';
        return 1;
    }
}
