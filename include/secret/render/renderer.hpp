#pragma once

#include "secret/math/vec3.hpp"
#include "secret/render/camera.hpp"
#include "secret/render/hittable.hpp"

#include <cstddef>
#include <filesystem>
#include <vector>

namespace secret::render {

struct RenderConfig final {
    int width = 640;
    int height = 360;
    int samples_per_pixel = 16;
    int max_depth = 10;
    unsigned int thread_count = 0;
    int tile_size = 16;
};

class Renderer final {
public:
    explicit Renderer(RenderConfig config);

    [[nodiscard]] std::vector<math::Color> render(const Hittable& world, const Camera& camera) const;
    void write_png(const std::filesystem::path& path,
                   const std::vector<math::Color>& pixels) const;
    [[nodiscard]] const RenderConfig& config() const noexcept { return config_; }

private:
    RenderConfig config_;
};

[[nodiscard]] HittableList make_demo_scene();
[[nodiscard]] Camera make_demo_camera(float aspect_ratio);

} // namespace secret::render
