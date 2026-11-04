#pragma once

#include "secret/render/hittable.hpp"
#include "secret/render/aabb_packet.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstddef>
#include <memory>
#include <limits>
#include <cmath>
#include <numeric>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace secret::render {

// Binary BVH with a 12-bin SAH split. Leaves contain at most four primitives.
class BvhNode final : public Hittable {
public:
    using Hittable::hit_packet8;
    static constexpr std::size_t bin_count = 12;
    static constexpr std::size_t max_leaf_size = 4;

    [[nodiscard]] static std::shared_ptr<const BvhNode> build(
        std::span<const std::shared_ptr<const Hittable>> objects) {
        if (objects.empty()) return {};
        std::vector<std::shared_ptr<const Hittable>> owned(objects.begin(), objects.end());
        for (const auto& object : owned) {
            if (!object) throw std::invalid_argument("BVH cannot contain a null primitive.");
            Aabb bounds;
            if (!object->bounding_box(bounds) || !bounds.valid() ||
                !std::isfinite(bounds.minimum().x()) || !std::isfinite(bounds.minimum().y()) ||
                !std::isfinite(bounds.minimum().z()) || !std::isfinite(bounds.maximum().x()) ||
                !std::isfinite(bounds.maximum().y()) || !std::isfinite(bounds.maximum().z()))
                throw std::invalid_argument("Every BVH primitive must provide finite valid bounds.");
        }
        return std::shared_ptr<const BvhNode>(build_recursive(std::move(owned)).release());
    }

    [[nodiscard]] bool bounding_box(Aabb& output) const noexcept override {
        output = bounds_;
        return bounds_.valid();
    }

    [[nodiscard]] bool hit(const core::Ray& ray, const float t_min, const float t_max,
                           HitRecord& record) const override {
        return bounds_.hit(ray, t_min, t_max) && hit_contents(ray, t_min, t_max, record);
    }

    // Packet traversal tests every internal node with the eight-lane AABB kernel.
    // Leaves dispatch primitive-specific packet kernels (AVX2 for spheres).
    [[nodiscard]] std::uint8_t hit_packet8(
        const RayPacket8& rays, const float t_min, const std::array<float, 8>& t_max,
        std::array<HitRecord, 8>& records, const std::uint8_t active_mask = 0xFFU) const override {
        auto lane_max = t_max;
        return hit_packet_recursive(rays, t_min, lane_max, records, active_mask);
    }

    [[nodiscard]] std::uint8_t hit_packet8(
        const RayPacket8& rays, const float t_min, const float t_max,
        std::array<HitRecord, 8>& records, const std::uint8_t active_mask = 0xFFU) const {
        std::array<float, 8> lane_max{};
        lane_max.fill(t_max);
        return hit_packet8(rays, t_min, lane_max, records, active_mask);
    }

private:
    [[nodiscard]] std::uint8_t hit_packet_recursive(
        const RayPacket8& rays, const float t_min, std::array<float, 8>& lane_max,
        std::array<HitRecord, 8>& records, const std::uint8_t active_mask) const {
        const float global_max = *std::max_element(lane_max.begin(), lane_max.end());
        const auto candidates = intersect_aabb_packet8(bounds_, rays, t_min, global_max, active_mask);
        if (candidates == 0U) return 0U;

        if (!left_ && !right_) {
            std::uint8_t hits = 0U;
            for (const auto& primitive : primitives_) {
                const auto primitive_hits = primitive->hit_packet8(
                    rays, t_min, lane_max, records, candidates);
                for (unsigned lane = 0; lane < 8; ++lane) {
                    const auto bit = static_cast<std::uint8_t>(1U << lane);
                    if ((primitive_hits & bit) != 0U) {
                        lane_max[lane] = records[lane].t;
                        hits = static_cast<std::uint8_t>(hits | bit);
                    }
                }
            }
            return hits;
        }

        std::uint8_t hits = 0U;
        if (left_) hits = static_cast<std::uint8_t>(hits | left_->hit_packet_recursive(
            rays, t_min, lane_max, records, candidates));
        if (right_) hits = static_cast<std::uint8_t>(hits | right_->hit_packet_recursive(
            rays, t_min, lane_max, records, candidates));
        return hits;
    }

    struct Bin final {
        Aabb bounds;
        std::size_t count = 0;
    };

    [[nodiscard]] bool hit_contents(const core::Ray& ray, const float t_min, const float t_max,
                                    HitRecord& record) const {
        if (!left_ && !right_) {
            HitRecord candidate{};
            bool found = false;
            float closest = t_max;
            for (const auto& primitive : primitives_) {
                if (primitive->hit(ray, t_min, closest, candidate)) {
                    found = true;
                    closest = candidate.t;
                    record = candidate;
                }
            }
            return found;
        }

        float left_near = t_min;
        float right_near = t_min;
        float left_far = t_max;
        float right_far = t_max;
        const bool hit_left = left_ && left_->bounds_.hit(ray, left_near, left_far);
        const bool hit_right = right_ && right_->bounds_.hit(ray, right_near, right_far);
        if (!hit_left && !hit_right) return false;

        const BvhNode* first = nullptr;
        const BvhNode* second = nullptr;
        if (hit_left && hit_right) {
            if (right_near < left_near) { first = right_.get(); second = left_.get(); }
            else { first = left_.get(); second = right_.get(); }
        } else {
            first = hit_left ? left_.get() : right_.get();
        }

        bool found = false;
        HitRecord candidate{};
        float closest = t_max;
        if (first->hit_contents(ray, t_min, closest, candidate)) {
            found = true;
            closest = candidate.t;
            record = candidate;
        }
        if (second && second->hit_contents(ray, t_min, closest, candidate)) {
            found = true;
            record = candidate;
        }
        return found;
    }

    explicit BvhNode(std::vector<std::shared_ptr<const Hittable>> primitives)
        : primitives_(std::move(primitives)) {
        for (const auto& primitive : primitives_) {
            Aabb primitive_bounds;
            (void)primitive->bounding_box(primitive_bounds);
            bounds_ = bounds_.valid() ? surrounding_box(bounds_, primitive_bounds) : primitive_bounds;
        }
    }

    BvhNode(std::unique_ptr<BvhNode> left, std::unique_ptr<BvhNode> right)
        : left_(std::move(left)), right_(std::move(right)) {
        Aabb left_bounds;
        Aabb right_bounds;
        (void)left_->bounding_box(left_bounds);
        (void)right_->bounding_box(right_bounds);
        bounds_ = surrounding_box(left_bounds, right_bounds);
    }

    [[nodiscard]] static std::unique_ptr<BvhNode> build_recursive(
        std::vector<std::shared_ptr<const Hittable>> objects) {
        if (objects.size() <= max_leaf_size)
            return std::unique_ptr<BvhNode>(new BvhNode(std::move(objects)));

        Aabb centroid_bounds;
        bool first_centroid = true;
        for (const auto& object : objects) {
            Aabb b;
            (void)object->bounding_box(b);
            const Aabb point_bounds(b.centroid(), b.centroid());
            centroid_bounds = first_centroid ? point_bounds : surrounding_box(centroid_bounds, point_bounds);
            first_centroid = false;
        }

        const auto extent = centroid_bounds.extent();
        int axis = extent.y() > extent.x() ? 1 : 0;
        if (Aabb::component(extent, 2) > Aabb::component(extent, axis)) axis = 2;
        const float centroid_min = Aabb::component(centroid_bounds.minimum(), axis);
        const float centroid_extent = Aabb::component(extent, axis);
        const auto get_bin_index = [centroid_min, centroid_extent](const float centroid) {
            const float normalized = std::clamp((centroid - centroid_min) / centroid_extent,
                                                0.0F, 0.99999994F);
            return static_cast<std::size_t>(normalized * static_cast<float>(bin_count));
        };

        std::size_t split_bin = 0;
        bool use_sah = centroid_extent > 1.0e-8F;
        float best_cost = std::numeric_limits<float>::infinity();
        if (use_sah) {
            std::array<Bin, bin_count> bins{};
            for (const auto& object : objects) {
                Aabb b;
                (void)object->bounding_box(b);
                const float c = Aabb::component(b.centroid(), axis);
                const auto index = get_bin_index(c);
                bins[index].count++;
                bins[index].bounds = bins[index].bounds.valid() ?
                    surrounding_box(bins[index].bounds, b) : b;
            }

            std::array<Aabb, bin_count> prefix_bounds{};
            std::array<Aabb, bin_count> suffix_bounds{};
            std::array<std::size_t, bin_count> prefix_count{};
            std::array<std::size_t, bin_count> suffix_count{};
            Aabb running;
            std::size_t count = 0;
            for (std::size_t i = 0; i < bin_count; ++i) {
                if (bins[i].count) running = running.valid() ? surrounding_box(running, bins[i].bounds) : bins[i].bounds;
                count += bins[i].count;
                prefix_bounds[i] = running;
                prefix_count[i] = count;
            }
            running = Aabb{};
            count = 0;
            for (std::size_t i = bin_count; i-- > 0;) {
                if (bins[i].count) running = running.valid() ? surrounding_box(running, bins[i].bounds) : bins[i].bounds;
                count += bins[i].count;
                suffix_bounds[i] = running;
                suffix_count[i] = count;
            }
            const float parent_area = std::max(1.0e-20F, [&] {
                Aabb b;
                for (const auto& object : objects) {
                    Aabb p; (void)object->bounding_box(p);
                    b = b.valid() ? surrounding_box(b, p) : p;
                }
                return b.surface_area();
            }());
            for (std::size_t i = 0; i + 1 < bin_count; ++i) {
                if (!prefix_count[i] || !suffix_count[i + 1]) continue;
                const float cost = 1.0F +
                    (prefix_bounds[i].surface_area() * static_cast<float>(prefix_count[i]) +
                     suffix_bounds[i + 1].surface_area() * static_cast<float>(suffix_count[i + 1])) / parent_area;
                if (cost < best_cost) { best_cost = cost; split_bin = i; }
            }
            use_sah = std::isfinite(best_cost);
        }

        std::vector<std::shared_ptr<const Hittable>> left_objects;
        std::vector<std::shared_ptr<const Hittable>> right_objects;
        if (use_sah) {
            auto middle = std::stable_partition(objects.begin(), objects.end(), [&](const auto& object) {
                Aabb b; (void)object->bounding_box(b);
                const float c = Aabb::component(b.centroid(), axis);
                const auto index = get_bin_index(c);
                return index <= split_bin;
            });
            left_objects.assign(objects.begin(), middle);
            right_objects.assign(middle, objects.end());
        }
        if (left_objects.empty() || right_objects.empty()) {
            std::stable_sort(objects.begin(), objects.end(), [axis](const auto& a, const auto& b) {
                Aabb ba, bb; (void)a->bounding_box(ba); (void)b->bounding_box(bb);
                return Aabb::component(ba.centroid(), axis) < Aabb::component(bb.centroid(), axis);
            });
            const auto middle = objects.begin() + static_cast<std::ptrdiff_t>(objects.size() / 2);
            left_objects.assign(objects.begin(), middle);
            right_objects.assign(middle, objects.end());
        }
        auto left = build_recursive(std::move(left_objects));
        auto right = build_recursive(std::move(right_objects));
        return std::unique_ptr<BvhNode>(new BvhNode(std::move(left), std::move(right)));
    }

    Aabb bounds_;
    std::vector<std::shared_ptr<const Hittable>> primitives_;
    std::unique_ptr<BvhNode> left_;
    std::unique_ptr<BvhNode> right_;
};

} // namespace secret::render
