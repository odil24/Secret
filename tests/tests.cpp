#include "secret/core/ray.hpp"
#include "secret/math/vec3.hpp"
#include "secret/render/hittable.hpp"
#include "secret/render/bvh.hpp"
#include "secret/render/aabb_packet.hpp"
#include "secret/render/material.hpp"

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <span>
#include <vector>

namespace {

void require(const bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

[[nodiscard]] bool finite_color(const secret::math::Color& color) {
    return std::isfinite(color.x()) && std::isfinite(color.y()) && std::isfinite(color.z());
}

} // namespace

int main() {
    using secret::math::Vec3;
    using secret::render::DisneyMaterial;
    using secret::render::DisneyParameters;

    const Vec3 a(1.0F, 2.0F, 3.0F);
    const Vec3 b(2.0F, 1.0F, 0.0F);
    require(std::abs(secret::math::dot(a, b) - 4.0F) < 1.0e-6F, "dot product");
    require(secret::math::cross(a, b).x() == -3.0F, "cross product x");
    require(std::abs(a.length_squared() - 14.0F) < 1.0e-6F, "vector length squared");

    const secret::core::Ray camera_ray(secret::math::Point3(0.0F, 0.0F, 0.0F),
                                       Vec3(0.0F, 0.0F, -1.0F));
    require(camera_ray.at(2.0F).z() == -2.0F, "ray point evaluation");

    DisneyParameters parameters{};
    parameters.base_color = Vec3(0.2F, 0.4F, 0.8F);
    parameters.subsurface = 0.75F;
    parameters.specular = 0.7F;
    parameters.specular_tint = 0.25F;
    parameters.metallic = 0.2F;
    parameters.roughness = 0.35F;
    parameters.anisotropic = 0.4F;
    parameters.sheen = 0.2F;
    parameters.sheen_tint = 0.5F;
    parameters.opacity = 1.0F;

    const auto material = std::make_shared<DisneyMaterial>(parameters);
    const secret::render::Sphere sphere(secret::math::Point3(0.0F, 0.0F, -3.0F),
                                        1.0F, material);
    secret::render::HitRecord hit{};
    require(sphere.hit(camera_ray, 0.001F, 100.0F, hit), "sphere intersection");
    require(std::abs(hit.t - 2.0F) < 1.0e-6F, "sphere intersection distance");

    const Vec3 outgoing(0.25F, 0.0F, 0.9682458F);
    const Vec3 brdf = material->evaluate(camera_ray, hit, outgoing);
    const float brdf_pdf = material->pdf(camera_ray, hit, outgoing);
    require(finite_color(brdf), "Disney BRDF is finite");
    require(brdf.x() > 0.0F && brdf.y() > 0.0F && brdf.z() > 0.0F,
            "Disney BRDF includes diffuse/SSS response");
    require(std::isfinite(brdf_pdf) && brdf_pdf > 0.0F, "Disney mixture PDF is positive");

    secret::render::Random random(0x12345678U);
    int valid_samples = 0;
    for (int sample = 0; sample < 1000; ++sample) {
        secret::core::Ray scattered{};
        Vec3 throughput{};
        if (!material->scatter(camera_ray, hit, throughput, scattered, random)) {
            continue; // Visible-normal microfacets can reflect below the geometric hemisphere.
        }
        ++valid_samples;
        require(finite_color(throughput), "Disney path throughput is finite");
        require(finite_color(scattered.direction()), "Disney scattered direction is finite");
        require(scattered.direction().length_squared() > 0.99F,
                "Disney scattered direction is normalized");
    }
    require(valid_samples > 500, "Disney BSDF generates enough valid path samples");


    DisneyParameters sss_only_parameters{};
    sss_only_parameters.base_color = Vec3(0.65F, 0.28F, 0.12F);
    sss_only_parameters.subsurface = 1.0F;
    sss_only_parameters.specular = 0.0F;
    sss_only_parameters.roughness = 0.45F;
    const DisneyMaterial sss_only(sss_only_parameters);
    const Vec3 sss_response = sss_only.evaluate(camera_ray, hit, outgoing);
    require(finite_color(sss_response) && sss_response.x() > 0.0F,
            "Disney subsurface diffuse response works without a separate Lambert material");

    const Vec3 grazing_outgoing(0.995F, 0.0F, 0.0998749F);
    const float grazing_pdf = sss_only.pdf(camera_ray, hit, grazing_outgoing);
    require(std::isfinite(grazing_pdf) && grazing_pdf > 0.0F,
            "Disney keeps a GGX sampling probability when F0 is zero");

    DisneyParameters transparent_parameters{};
    transparent_parameters.opacity = 0.0F;
    const DisneyMaterial transparent(transparent_parameters);
    secret::core::Ray continued{};
    Vec3 transparent_throughput{};
    require(transparent.scatter(camera_ray, hit, transparent_throughput, continued, random),
            "zero-opacity Disney surface continues the ray");
    require(continued.direction().x() == camera_ray.direction().x() &&
            continued.direction().y() == camera_ray.direction().y() &&
            continued.direction().z() == camera_ray.direction().z(),
            "opacity hole preserves ray direction");
    require(transparent_throughput.x() == 1.0F &&
            transparent_throughput.y() == 1.0F &&
            transparent_throughput.z() == 1.0F,
            "opacity hole preserves throughput");

    DisneyParameters sheen_only_parameters{};
    sheen_only_parameters.base_color = Vec3(0.0F);
    sheen_only_parameters.specular = 0.0F;
    sheen_only_parameters.sheen = 1.0F;
    sheen_only_parameters.sheen_roughness = 0.5F;
    const DisneyMaterial sheen_only(sheen_only_parameters);
    const Vec3 sheen_response = sheen_only.evaluate(camera_ray, hit, outgoing);
    const float sheen_pdf = sheen_only.pdf(camera_ray, hit, outgoing);
    require(finite_color(sheen_response) && sheen_response.x() > 0.0F,
            "LTC multiple-scattering sheen is evaluated inside Disney BSDF");
    require(std::isfinite(sheen_pdf) && sheen_pdf > 0.0F,
            "LTC sheen contributes to the Disney mixture PDF");
    const auto sheen_coefficients = secret::render::detail::fetch_sheen_ltc_coefficients(
        sheen_only_parameters.sheen_roughness, 1.0F);
    const Vec3 sheen_single_lobe_throughput = sheen_response * (outgoing.z() / sheen_pdf);
    require(std::abs(sheen_single_lobe_throughput.x() - sheen_coefficients.reflectance) < 2.0e-4F &&
            std::abs(sheen_single_lobe_throughput.y() - sheen_coefficients.reflectance) < 2.0e-4F &&
            std::abs(sheen_single_lobe_throughput.z() - sheen_coefficients.reflectance) < 2.0e-4F,
            "LTC sheen BSDF and PDF preserve the fitted directional reflectance");
    for (const float alpha : {0.0F, 0.5F, 1.0F}) {
        for (const float cosine : {0.0F, 0.5F, 1.0F}) {
            const auto coefficients =
                secret::render::detail::fetch_sheen_ltc_coefficients(alpha, cosine);
            require(std::isfinite(coefficients.a_inv) &&
                    std::isfinite(coefficients.b_inv) &&
                    std::isfinite(coefficients.reflectance) &&
                    coefficients.reflectance >= 0.0F,
                    "LTC sheen coefficient interpolation is finite at table boundaries");
        }
    }

    int sheen_valid_samples = 0;
    for (int sample = 0; sample < 256; ++sample) {
        secret::core::Ray scattered{};
        Vec3 throughput{};
        if (!sheen_only.scatter(camera_ray, hit, throughput, scattered, random)) {
            continue;
        }
        ++sheen_valid_samples;
        require(finite_color(throughput) && finite_color(scattered.direction()),
                "LTC sheen sampling produces finite path throughput and direction");
        require(scattered.direction().z() > 0.0F,
                "LTC sheen samples the visible hemisphere");
    }
    require(sheen_valid_samples > 240,
            "pure LTC sheen material generates valid importance samples");

    DisneyParameters clamped{};
    clamped.subsurface = 4.0F;
    clamped.metallic = -1.0F;
    clamped.roughness = 0.0F;
    clamped.sheen_roughness = 2.0F;
    clamped.opacity = 2.0F;
    const DisneyMaterial clamped_material(clamped);
    require(clamped_material.parameters().subsurface == 1.0F, "SSS parameter clamp");
    require(clamped_material.parameters().metallic == 0.0F, "metallic parameter clamp");
    require(clamped_material.parameters().roughness >= 0.001F, "roughness lower bound");
    require(clamped_material.parameters().opacity == 1.0F, "opacity parameter clamp");
    require(clamped_material.parameters().sheen_roughness == 1.0F,
            "LTC sheen roughness parameter clamp");


    // BVH must agree with the linear reference over deterministic randomized rays.
    secret::render::HittableList many_spheres;
    std::vector<std::shared_ptr<const secret::render::Hittable>> bvh_objects;
    for (int z = 0; z < 7; ++z) {
        for (int x = 0; x < 9; ++x) {
            auto primitive = std::make_shared<secret::render::Sphere>(
                Vec3(static_cast<float>(x) * 0.73F - 2.92F,
                     static_cast<float>((x * 3 + z) % 5) * 0.31F - 0.62F,
                     -2.0F - static_cast<float>(z) * 0.81F), 0.22F, material);
            many_spheres.add(primitive);
            bvh_objects.push_back(primitive);
        }
    }
    const auto bvh = secret::render::BvhNode::build(bvh_objects);
    require(static_cast<bool>(bvh), "BVH builds for non-empty primitive arrays");
    secret::render::Aabb scene_bounds;
    require(bvh->bounding_box(scene_bounds) && scene_bounds.valid(), "BVH returns valid scene bounds");
    secret::render::Random ray_random(0xBADC0DEU);
    for (int i = 0; i < 5000; ++i) {
        const Vec3 origin(ray_random.uniform() * 9.0F - 4.5F,
                          ray_random.uniform() * 6.0F - 3.0F,
                          ray_random.uniform() * 5.0F + 0.5F);
        const Vec3 direction = secret::math::unit_vector(Vec3(
            ray_random.uniform() * 2.0F - 1.0F,
            ray_random.uniform() * 2.0F - 1.0F,
            -ray_random.uniform() - 0.05F));
        const secret::core::Ray ray(origin, direction);
        secret::render::HitRecord linear_hit{};
        secret::render::HitRecord bvh_hit{};
        const bool linear_found = many_spheres.hit(ray, 0.001F, 1000.0F, linear_hit);
        const bool bvh_found = bvh->hit(ray, 0.001F, 1000.0F, bvh_hit);
        require(linear_found == bvh_found, "BVH hit result matches brute-force traversal");
        if (linear_found) require(std::abs(linear_hit.t - bvh_hit.t) < 1.0e-4F,
                                  "BVH nearest-hit distance matches brute force");
    }
    // AVX2 sphere packet kernel must match the scalar sphere intersection,
    // including per-lane closest distances and inactive lanes.
    secret::render::RayPacket8 sphere_packet{};
    std::array<secret::core::Ray, 8> sphere_rays{};
    std::array<float, 8> sphere_max{};
    std::array<float, 8> sphere_roots{};
    for (std::size_t lane = 0; lane < 8; ++lane) {
        const Vec3 origin(ray_random.uniform() * 5.0F - 2.5F,
                          ray_random.uniform() * 4.0F - 2.0F,
                          ray_random.uniform() * 3.0F + 0.5F);
        const Vec3 direction = secret::math::unit_vector(Vec3(
            ray_random.uniform() * 1.2F - 0.6F,
            ray_random.uniform() * 1.2F - 0.6F, -1.0F));
        sphere_rays[lane] = secret::core::Ray(origin, direction);
        sphere_packet.set(lane, sphere_rays[lane]);
        sphere_max[lane] = (lane % 2U == 0U) ? 100.0F : 0.01F;
    }
    const auto sphere_mask = secret::render::intersect_sphere_packet8(
        Vec3(0.0F, 0.0F, -3.0F), 1.0F, sphere_packet, 0.001F,
        sphere_max, sphere_roots, 0xFFU);
    for (std::size_t lane = 0; lane < 8; ++lane) {
        secret::render::HitRecord scalar_record{};
        const bool expected = sphere.hit(sphere_rays[lane], 0.001F, sphere_max[lane], scalar_record);
        const bool actual = (sphere_mask & static_cast<std::uint8_t>(1U << lane)) != 0U;
        require(actual == expected, "AVX2 sphere packet hit mask matches scalar sphere");
        if (expected) require(std::abs(sphere_roots[lane] - scalar_record.t) < 2.0e-4F,
                              "AVX2 sphere packet root matches scalar sphere");
    }
    sphere_max[0] = 0.01F;
    const auto clipped_sphere_mask = secret::render::intersect_sphere_packet8(
        Vec3(0.0F, 0.0F, -3.0F), 1.0F, sphere_packet, 0.001F,
        sphere_max, sphere_roots, 0x01U);
    require(clipped_sphere_mask == 0U, "sphere packet respects per-lane t_max and active mask");

    const secret::render::Aabb slab(Vec3(-1.0F), Vec3(1.0F));

    // Eight-ray AABB packet kernel must match the scalar reference, including parallel axes.
    secret::render::RayPacket8 packet{};
    std::array<secret::core::Ray, 8> packet_rays{};
    for (std::size_t lane = 0; lane < packet_rays.size(); ++lane) {
        const Vec3 origin(ray_random.uniform() * 6.0F - 3.0F,
                          ray_random.uniform() * 6.0F - 3.0F,
                          ray_random.uniform() * 6.0F - 3.0F);
        Vec3 direction(ray_random.uniform() * 2.0F - 1.0F,
                       ray_random.uniform() * 2.0F - 1.0F,
                       ray_random.uniform() * 2.0F - 1.0F);
        if (lane == 0) direction = Vec3(0.0F, 0.0F, -1.0F);
        if (lane == 1) direction = Vec3(0.0F, 1.0F, 0.0F);
        packet_rays[lane] = secret::core::Ray(origin, direction);
        packet.set(lane, packet_rays[lane]);
    }
    const std::uint8_t packet_mask = secret::render::intersect_aabb_packet8(
        slab, packet, 0.0F, 100.0F, 0xFFU);
    for (std::size_t lane = 0; lane < packet_rays.size(); ++lane) {
        const bool expected = slab.hit(packet_rays[lane], 0.0F, 100.0F);
        const bool actual = (packet_mask & static_cast<std::uint8_t>(1U << lane)) != 0U;
        require(actual == expected, "AVX2/scalar packet AABB agrees with scalar ray test");
    }
    const auto partial_mask = secret::render::intersect_aabb_packet8(slab, packet, 0.0F, 100.0F, 0x05U);
    require((partial_mask & 0xFAU) == 0U, "packet AABB leaves inactive lanes clear");
    require(secret::render::intersect_aabb_packet8(slab, packet, 1.0F, 0.0F) == 0U,
            "packet AABB rejects invalid ray intervals");

    for (int batch = 0; batch < 2000; ++batch) {
        for (std::size_t lane = 0; lane < packet_rays.size(); ++lane) {
            const Vec3 origin(ray_random.uniform() * 10.0F - 5.0F,
                              ray_random.uniform() * 10.0F - 5.0F,
                              ray_random.uniform() * 10.0F - 5.0F);
            const Vec3 direction(ray_random.uniform() * 2.0F - 1.0F,
                                 ray_random.uniform() * 2.0F - 1.0F,
                                 ray_random.uniform() * 2.0F - 1.0F);
            packet_rays[lane] = secret::core::Ray(origin, direction);
            packet.set(lane, packet_rays[lane]);
        }
        const auto mask = secret::render::intersect_aabb_packet8(slab, packet, 0.001F, 50.0F);
        for (std::size_t lane = 0; lane < packet_rays.size(); ++lane) {
            const bool expected = slab.hit(packet_rays[lane], 0.001F, 50.0F);
            const bool actual = (mask & static_cast<std::uint8_t>(1U << lane)) != 0U;
            require(actual == expected, "randomized eight-ray packet matches scalar AABB");
        }
        std::array<secret::render::HitRecord, 8> packet_hits{};
        const auto bvh_mask = bvh->hit_packet8(packet, 0.001F, 1000.0F, packet_hits);
        for (std::size_t lane = 0; lane < packet_rays.size(); ++lane) {
            secret::render::HitRecord scalar_hit{};
            const bool expected = bvh->hit(packet_rays[lane], 0.001F, 1000.0F, scalar_hit);
            const bool actual = (bvh_mask & static_cast<std::uint8_t>(1U << lane)) != 0U;
            require(actual == expected, "packet BVH hit mask matches scalar BVH");
            if (expected) require(std::abs(packet_hits[lane].t - scalar_hit.t) < 5.0e-4F,
                                  "packet BVH nearest hit matches scalar BVH");
        }
    }

    // Verify the renderer-facing virtual packet API reaches BVH packet traversal.
    std::array<float, 8> virtual_tmax{};
    for (std::size_t lane = 0; lane < 8; ++lane)
        virtual_tmax[lane] = (lane % 2U == 0U) ? 1000.0F : 0.01F;
    std::array<secret::render::HitRecord, 8> virtual_hits{};
    const secret::render::Hittable& bvh_interface = *bvh;
    const auto virtual_mask = bvh_interface.hit_packet8(
        packet, 0.001F, virtual_tmax, virtual_hits, 0xFFU);
    for (std::size_t lane = 0; lane < 8; ++lane) {
        secret::render::HitRecord scalar_hit{};
        const bool expected = bvh->hit(packet_rays[lane], 0.001F, virtual_tmax[lane], scalar_hit);
        const bool actual = (virtual_mask & static_cast<std::uint8_t>(1U << lane)) != 0U;
        require(actual == expected, "virtual packet interface dispatches to BVH traversal");
        if (expected) require(std::abs(virtual_hits[lane].t - scalar_hit.t) < 5.0e-4F,
                              "virtual packet BVH returns nearest hit");
    }

    require(!secret::render::BvhNode::build(
                std::span<const std::shared_ptr<const secret::render::Hittable>>{}),
            "empty BVH build returns null");
    require(slab.hit(secret::core::Ray(Vec3(0.0F, 0.0F, 3.0F), Vec3(0.0F, 0.0F, -1.0F)),
                     0.0F, 100.0F), "AABB slab test handles zero direction components");
    require(!slab.hit(secret::core::Ray(Vec3(2.0F, 0.0F, 3.0F), Vec3(0.0F, 0.0F, -1.0F)),
                      0.0F, 100.0F), "AABB rejects parallel ray outside slab");

    std::cout << "All Secret v0.7 tests passed.\n";
    return EXIT_SUCCESS;
}
