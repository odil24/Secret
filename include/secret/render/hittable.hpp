#pragma once

#include "secret/core/ray.hpp"
#include "secret/render/aabb.hpp"
#include "secret/render/material.hpp"
#include "secret/render/sphere_packet.hpp"

#include <array>
#include <cstdint>

#include <memory>
#include <vector>

namespace secret::render {

class Hittable {
public:
    virtual ~Hittable() = default;
    [[nodiscard]] virtual bool hit(const core::Ray& ray, float t_min, float t_max, HitRecord& record) const = 0;
    [[nodiscard]] virtual bool bounding_box(Aabb& output) const noexcept = 0;

    // Generic packet fallback. Specialized primitives may override with SIMD kernels.
    [[nodiscard]] virtual std::uint8_t hit_packet8(
        const RayPacket8& rays, float t_min, const std::array<float, 8>& t_max,
        std::array<HitRecord, 8>& records, std::uint8_t active_mask = 0xFFU) const {
        std::uint8_t hits = 0U;
        for (unsigned lane = 0; lane < 8; ++lane) {
            const auto bit = static_cast<std::uint8_t>(1U << lane);
            if ((active_mask & bit) == 0U) continue;
            const core::Ray ray(math::Point3(rays.origin_x[lane], rays.origin_y[lane], rays.origin_z[lane]),
                                math::Vec3(rays.direction_x[lane], rays.direction_y[lane], rays.direction_z[lane]));
            if (hit(ray, t_min, t_max[lane], records[lane]))
                hits = static_cast<std::uint8_t>(hits | bit);
        }
        return hits;
    }
};

class Sphere final : public Hittable {
public:
    Sphere(const math::Point3& center, const float radius, std::shared_ptr<const Material> material)
        : center_(center), radius_(radius), material_(std::move(material)) {}

    [[nodiscard]] bool bounding_box(Aabb& output) const noexcept override {
        const math::Vec3 radius(radius_);
        output = Aabb(center_ - radius, center_ + radius);
        return radius_ > 0.0F;
    }

    [[nodiscard]] std::uint8_t hit_packet8(
        const RayPacket8& rays, const float t_min, const std::array<float, 8>& t_max,
        std::array<HitRecord, 8>& records, const std::uint8_t active_mask = 0xFFU) const override {
        std::array<float, 8> roots{};
        const auto mask = intersect_sphere_packet8(center_, radius_, rays, t_min, t_max, roots, active_mask);
        for (unsigned lane = 0; lane < 8; ++lane) {
            const auto bit = static_cast<std::uint8_t>(1U << lane);
            if ((mask & bit) == 0U) continue;
            const core::Ray ray(math::Point3(rays.origin_x[lane], rays.origin_y[lane], rays.origin_z[lane]),
                                math::Vec3(rays.direction_x[lane], rays.direction_y[lane], rays.direction_z[lane]));
            const float root = roots[lane];
            records[lane].t = root;
            records[lane].point = ray.at(root);
            records[lane].set_face_normal(ray, (records[lane].point - center_) / radius_);
            records[lane].material = material_;
        }
        return mask;
    }

    [[nodiscard]] bool hit(const core::Ray& ray,
                           const float t_min,
                           const float t_max,
                           HitRecord& record) const override {
        const math::Vec3 offset = ray.origin() - center_;
        const float a = ray.direction().length_squared();
        const float half_b = math::dot(offset, ray.direction());
        const float c = offset.length_squared() - radius_ * radius_;
        const float discriminant = half_b * half_b - a * c;
        if (discriminant < 0.0F) {
            return false;
        }

        const float square_root = std::sqrt(discriminant);
        float root = (-half_b - square_root) / a;
        if (root < t_min || root > t_max) {
            root = (-half_b + square_root) / a;
            if (root < t_min || root > t_max) {
                return false;
            }
        }

        record.t = root;
        record.point = ray.at(root);
        const math::Vec3 outward_normal = (record.point - center_) / radius_;
        record.set_face_normal(ray, outward_normal);
        record.material = material_;
        return true;
    }

private:
    math::Point3 center_;
    float radius_;
    std::shared_ptr<const Material> material_;
};

class HittableList final : public Hittable {
public:
    void add(std::shared_ptr<const Hittable> object) { objects_.push_back(std::move(object)); }

    [[nodiscard]] bool hit(const core::Ray& ray,
                           const float t_min,
                           const float t_max,
                           HitRecord& record) const override {
        HitRecord temporary{};
        bool hit_anything = false;
        float closest = t_max;
        for (const auto& object : objects_) {
            if (object->hit(ray, t_min, closest, temporary)) {
                hit_anything = true;
                closest = temporary.t;
                record = temporary;
            }
        }
        return hit_anything;
    }

    [[nodiscard]] bool bounding_box(Aabb& output) const noexcept override {
        if (objects_.empty()) return false;
        Aabb current;
        if (!objects_.front()->bounding_box(current)) return false;
        for (std::size_t i = 1; i < objects_.size(); ++i) {
            Aabb next;
            if (!objects_[i]->bounding_box(next)) return false;
            current = surrounding_box(current, next);
        }
        output = current;
        return true;
    }

    [[nodiscard]] const std::vector<std::shared_ptr<const Hittable>>& objects() const noexcept {
        return objects_;
    }

private:
    std::vector<std::shared_ptr<const Hittable>> objects_;
};

} // namespace secret::render
