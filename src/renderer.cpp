#include "secret/render/renderer.hpp"

#include "secret/render/material.hpp"
#include "secret/render/bvh.hpp"
#include "secret/render/random.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>
#include <Windows.h>
#include <wincodec.h>
#include <wrl/client.h>

namespace secret::render {
namespace {

[[nodiscard]] math::Color ray_color(const core::Ray& ray, const Hittable& world,
                                    int depth, Random& random);

[[nodiscard]] math::Color ray_color_after_hit(const core::Ray& ray, const Hittable& world,
                                              const int depth, Random& random,
                                              const bool found, const HitRecord& hit) {
    if (depth <= 0) return math::Color(0.0F);
    if (found) {
        core::Ray scattered{};
        math::Color attenuation{};
        if (hit.material && hit.material->scatter(ray, hit, attenuation, scattered, random))
            return attenuation * ray_color(scattered, world, depth - 1, random);
        return math::Color(0.0F);
    }
    const math::Vec3 direction = math::unit_vector(ray.direction());
    const float blend = 0.5F * (direction.y() + 1.0F);
    return (1.0F - blend) * math::Color(1.0F) + blend * math::Color(0.5F, 0.7F, 1.0F);
}

[[nodiscard]] math::Color ray_color(const core::Ray& ray, const Hittable& world,
                                    const int depth, Random& random) {
    HitRecord hit{};
    const bool found = world.hit(ray, 0.001F, std::numeric_limits<float>::infinity(), hit);
    return ray_color_after_hit(ray, world, depth, random, found, hit);
}

[[nodiscard]] std::uint32_t pixel_seed(const int x, const int y) noexcept {
    std::uint32_t value = static_cast<std::uint32_t>(x) * 0x9E3779B9U;
    value ^= static_cast<std::uint32_t>(y) + 0x85EBCA6BU + (value << 6U) + (value >> 2U);
    value ^= value >> 16U;
    value *= 0x7FEB352DU;
    value ^= value >> 15U;
    value *= 0x846CA68BU;
    value ^= value >> 16U;
    return value;
}

[[nodiscard]] float linear_to_srgb(const float value) noexcept {
    const float bounded = std::clamp(value, 0.0F, 1.0F);
    if (bounded <= 0.0031308F) {
        return 12.92F * bounded;
    }
    return 1.055F * std::pow(bounded, 1.0F / 2.4F) - 0.055F;
}

[[nodiscard]] int to_byte(const float linear_value) noexcept {
    // Reinhard tone mapping keeps HDR radiance in display range before sRGB encoding.
    const float non_negative = std::max(0.0F, linear_value);
    const float tone_mapped = non_negative / (1.0F + non_negative);
    const float srgb = std::clamp(linear_to_srgb(tone_mapped), 0.0F, 0.999F);
    return static_cast<int>(256.0F * srgb);
}

} // namespace

Renderer::Renderer(RenderConfig config) : config_(config) {
    if (config_.width <= 0 || config_.height <= 0) {
        throw std::invalid_argument("Render width and height must be positive.");
    }
    if (config_.width > 16384 || config_.height > 16384 ||
        static_cast<std::uint64_t>(config_.width) * static_cast<std::uint64_t>(config_.height) >
            100'000'000ULL) {
        throw std::invalid_argument("Render dimensions exceed the v0.7 safety limits.");
    }
    if (config_.samples_per_pixel <= 0 || config_.max_depth <= 0 || config_.tile_size <= 0) {
        throw std::invalid_argument("Samples per pixel, max depth and tile size must be positive.");
    }
}

std::vector<math::Color> Renderer::render(const Hittable& world, const Camera& camera) const {
    // Build acceleration once per frame for mutable scene lists; callers may also pass
    // a prebuilt BvhNode directly to reuse it across frames.
    std::shared_ptr<const BvhNode> built_bvh;
    const Hittable* accelerated_world = &world;
    if (const auto* list = dynamic_cast<const HittableList*>(&world)) {
        built_bvh = BvhNode::build(list->objects());
        if (built_bvh) accelerated_world = built_bvh.get();
    }
    const auto pixel_count = static_cast<std::size_t>(config_.width) * static_cast<std::size_t>(config_.height);
    std::vector<math::Color> pixels(pixel_count);
    const int tiles_x = (config_.width + config_.tile_size - 1) / config_.tile_size;
    const int tiles_y = (config_.height + config_.tile_size - 1) / config_.tile_size;
    const int tile_count = tiles_x * tiles_y;
    std::atomic<int> next_tile{0};

    unsigned int worker_count = config_.thread_count;
    if (worker_count == 0U) worker_count = std::max(1U, std::thread::hardware_concurrency());
    worker_count = std::min(worker_count, static_cast<unsigned int>(tile_count));
    const float inverse_width = 1.0F / static_cast<float>(std::max(1, config_.width - 1));
    const float inverse_height = 1.0F / static_cast<float>(std::max(1, config_.height - 1));
    const float sample_scale = 1.0F / static_cast<float>(config_.samples_per_pixel);

    const auto worker = [&]() {
        for (;;) {
            const int tile = next_tile.fetch_add(1, std::memory_order_relaxed);
            if (tile >= tile_count) break;
            const int tile_x = tile % tiles_x;
            const int tile_y = tile / tiles_x;
            const int x_begin = tile_x * config_.tile_size;
            const int y_begin = tile_y * config_.tile_size;
            const int x_end = std::min(config_.width, x_begin + config_.tile_size);
            const int y_end = std::min(config_.height, y_begin + config_.tile_size);

            for (int y = y_begin; y < y_end; ++y) {
                for (int x = x_begin; x < x_end; x += 8) {
                    std::array<Random, 8> randoms{
                        Random(0U), Random(0U), Random(0U), Random(0U),
                        Random(0U), Random(0U), Random(0U), Random(0U)};
                    std::array<math::Color, 8> accumulated{};
                    std::array<core::Ray, 8> primary_rays{};
                    std::array<HitRecord, 8> primary_hits{};
                    std::array<float, 8> lane_max{};
                    lane_max.fill(std::numeric_limits<float>::infinity());
                    RayPacket8 packet{};
                    std::uint8_t active_mask = 0U;
                    for (unsigned lane = 0; lane < 8; ++lane) {
                        const int pixel_x = x + static_cast<int>(lane);
                        if (pixel_x >= x_end) continue;
                        active_mask = static_cast<std::uint8_t>(active_mask | (1U << lane));
                        randoms[lane] = Random(pixel_seed(pixel_x, y));
                    }

                    for (int sample = 0; sample < config_.samples_per_pixel; ++sample) {
                        for (unsigned lane = 0; lane < 8; ++lane) {
                            if ((active_mask & static_cast<std::uint8_t>(1U << lane)) == 0U) continue;
                            const int pixel_x = x + static_cast<int>(lane);
                            const float u = (static_cast<float>(pixel_x) + randoms[lane].uniform()) * inverse_width;
                            const float v = (static_cast<float>(config_.height - 1 - y) + randoms[lane].uniform()) * inverse_height;
                            primary_rays[lane] = camera.get_ray(u, v, randoms[lane]);
                            packet.set(lane, primary_rays[lane]);
                        }
                        const std::uint8_t hit_mask = accelerated_world->hit_packet8(
                            packet, 0.001F, lane_max, primary_hits, active_mask);
                        for (unsigned lane = 0; lane < 8; ++lane) {
                            const auto bit = static_cast<std::uint8_t>(1U << lane);
                            if ((active_mask & bit) == 0U) continue;
                            const bool found = (hit_mask & bit) != 0U;
                            accumulated[lane] += ray_color_after_hit(
                                primary_rays[lane], *accelerated_world, config_.max_depth,
                                randoms[lane], found, primary_hits[lane]);
                        }
                    }
                    for (unsigned lane = 0; lane < 8; ++lane) {
                        if ((active_mask & static_cast<std::uint8_t>(1U << lane)) == 0U) continue;
                        const auto pixel_index = static_cast<std::size_t>(y) * static_cast<std::size_t>(config_.width) +
                                                 static_cast<std::size_t>(x + static_cast<int>(lane));
                        pixels[pixel_index] = sample_scale * accumulated[lane];
                    }
                }
            }
        }
    };

    std::vector<std::jthread> workers;
    workers.reserve(worker_count);
    for (unsigned int index = 0; index < worker_count; ++index) {
        (void)index;
        workers.emplace_back(worker);
    }
    workers.clear(); // jthread joins before the pixel buffer is returned.
    return pixels;
}

void Renderer::write_png(const std::filesystem::path& path,
                         const std::vector<math::Color>& pixels) const {
    const auto expected_size = static_cast<std::size_t>(config_.width) * static_cast<std::size_t>(config_.height);
    if (pixels.size() != expected_size) {
        throw std::invalid_argument("Pixel buffer size does not match render dimensions.");
    }

    const HRESULT init_result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool uninitialize = SUCCEEDED(init_result);
    if (FAILED(init_result) && init_result != RPC_E_CHANGED_MODE) {
        throw std::runtime_error("COM initialization failed for PNG encoder.");
    }

    const auto cleanup = [&]() noexcept { if (uninitialize) CoUninitialize(); };
    try {
        Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
        HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                      IID_PPV_ARGS(&factory));
        if (FAILED(hr)) throw std::runtime_error("Could not create Windows Imaging Component factory.");

        Microsoft::WRL::ComPtr<IWICBitmapEncoder> encoder;
        hr = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
        if (FAILED(hr)) throw std::runtime_error("Could not create PNG encoder.");

        Microsoft::WRL::ComPtr<IWICStream> stream;
        hr = factory->CreateStream(&stream);
        if (FAILED(hr)) throw std::runtime_error("Could not create PNG output stream.");
        hr = stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE);
        if (FAILED(hr)) throw std::runtime_error("Could not open PNG output file: " + path.string());
        hr = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
        if (FAILED(hr)) throw std::runtime_error("Could not initialize PNG encoder.");

        Microsoft::WRL::ComPtr<IWICBitmapFrameEncode> frame;
        Microsoft::WRL::ComPtr<IPropertyBag2> properties;
        hr = encoder->CreateNewFrame(&frame, &properties);
        if (FAILED(hr)) throw std::runtime_error("Could not create PNG frame.");
        hr = frame->Initialize(properties.Get());
        if (FAILED(hr)) throw std::runtime_error("Could not initialize PNG frame.");
        hr = frame->SetSize(static_cast<UINT>(config_.width), static_cast<UINT>(config_.height));
        if (FAILED(hr)) throw std::runtime_error("Could not set PNG dimensions.");

        WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
        hr = frame->SetPixelFormat(&format);
        if (FAILED(hr) || format != GUID_WICPixelFormat32bppBGRA) {
            throw std::runtime_error("PNG encoder does not support 32-bit BGRA pixels.");
        }

        std::vector<BYTE> bgra(expected_size * 4U);
        for (std::size_t i = 0; i < expected_size; ++i) {
            const auto& pixel = pixels[i];
            bgra[i * 4U + 0U] = static_cast<BYTE>(to_byte(pixel.z()));
            bgra[i * 4U + 1U] = static_cast<BYTE>(to_byte(pixel.y()));
            bgra[i * 4U + 2U] = static_cast<BYTE>(to_byte(pixel.x()));
            bgra[i * 4U + 3U] = 255U;
        }
        const UINT stride = static_cast<UINT>(config_.width * 4);
        const UINT byte_count = static_cast<UINT>(bgra.size());
        hr = frame->WritePixels(static_cast<UINT>(config_.height), stride, byte_count, bgra.data());
        if (FAILED(hr)) throw std::runtime_error("Failed while writing PNG pixels.");
        hr = frame->Commit();
        if (FAILED(hr)) throw std::runtime_error("Could not finalize PNG frame.");
        hr = encoder->Commit();
        if (FAILED(hr)) throw std::runtime_error("Could not finalize PNG file.");
    } catch (...) {
        cleanup();
        throw;
    }
    cleanup();
}

HittableList make_demo_scene() {
    HittableList world;

    DisneyParameters ground_parameters{};
    ground_parameters.base_color = math::Color(0.52F, 0.54F, 0.58F);
    ground_parameters.roughness = 0.82F;
    const auto ground = std::make_shared<DisneyMaterial>(ground_parameters);

    DisneyParameters subsurface_parameters{};
    subsurface_parameters.base_color = math::Color(0.72F, 0.18F, 0.12F);
    subsurface_parameters.subsurface = 0.9F;
    subsurface_parameters.roughness = 0.42F;
    subsurface_parameters.specular = 0.45F;
    const auto subsurface = std::make_shared<DisneyMaterial>(subsurface_parameters);

    DisneyParameters blue_parameters{};
    blue_parameters.base_color = math::Color(0.08F, 0.22F, 0.72F);
    blue_parameters.roughness = 0.24F;
    blue_parameters.specular = 0.75F;
    blue_parameters.specular_tint = 0.15F;
    blue_parameters.sheen = 0.12F;
    const auto blue = std::make_shared<DisneyMaterial>(blue_parameters);

    DisneyParameters metal_parameters{};
    metal_parameters.base_color = math::Color(0.82F, 0.48F, 0.16F);
    metal_parameters.metallic = 1.0F;
    metal_parameters.roughness = 0.19F;
    metal_parameters.anisotropic = 0.35F;
    const auto metal = std::make_shared<DisneyMaterial>(metal_parameters);

    DisneyParameters fabric_parameters{};
    fabric_parameters.base_color = math::Color(0.22F, 0.58F, 0.32F);
    fabric_parameters.roughness = 0.78F;
    fabric_parameters.sheen = 0.8F;
    fabric_parameters.sheen_tint = 0.7F;
    const auto fabric = std::make_shared<DisneyMaterial>(fabric_parameters);

    world.add(std::make_shared<Sphere>(math::Point3(0.0F, -100.5F, -1.0F), 100.0F, ground));
    world.add(std::make_shared<Sphere>(math::Point3(-1.05F, 0.0F, -1.0F), 0.48F, subsurface));
    world.add(std::make_shared<Sphere>(math::Point3(0.0F, 0.0F, -1.0F), 0.5F, blue));
    world.add(std::make_shared<Sphere>(math::Point3(1.05F, 0.0F, -1.0F), 0.48F, metal));
    world.add(std::make_shared<Sphere>(math::Point3(0.0F, -0.05F, -2.0F), 0.32F, fabric));
    return world;
}

Camera make_demo_camera(const float aspect_ratio) {
    return Camera(math::Point3(3.0F, 1.5F, 2.0F),
                  math::Point3(0.0F, 0.0F, -1.0F),
                  math::Vec3(0.0F, 1.0F, 0.0F),
                  35.0F,
                  aspect_ratio,
                  0.0F,
                  4.0F);
}

} // namespace secret::render
