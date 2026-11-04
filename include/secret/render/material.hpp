#pragma once

#include "secret/core/ray.hpp"
#include "secret/render/detail/ltc_sheen_volume_table.hpp"
#include "secret/render/random.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>

namespace secret::render {

struct HitRecord;

class Material {
public:
    virtual ~Material() = default;

    // attenuation is the already divided BSDF * cos(theta) / PDF throughput.
    [[nodiscard]] virtual bool scatter(const core::Ray& incoming,
                                       const HitRecord& hit,
                                       math::Color& attenuation,
                                       core::Ray& scattered,
                                       Random& random) const = 0;
};

struct HitRecord {
    math::Point3 point{};
    math::Vec3 normal{};
    float t = 0.0F;
    bool front_face = false;
    std::shared_ptr<const Material> material{};

    void set_face_normal(const core::Ray& ray, const math::Vec3& outward_normal) noexcept {
        front_face = math::dot(ray.direction(), outward_normal) < 0.0F;
        normal = front_face ? outward_normal : -outward_normal;
    }
};

// Disney principled parameters used by the referenced rlDisney shader.
// All scalar controls are normalized to [0, 1]. base_color is linear RGB.
struct DisneyParameters final {
    math::Color base_color{0.8F, 0.8F, 0.8F};
    float subsurface = 0.0F;
    float metallic = 0.0F;
    float specular = 0.5F;
    float specular_tint = 0.0F;
    float roughness = 0.5F;
    float anisotropic = 0.0F;
    float sheen = 0.0F;
    float sheen_tint = 0.0F;
    float sheen_roughness = 0.5F;
    float opacity = 1.0F;
};

namespace detail {

inline constexpr float pi = std::numbers::pi_v<float>;
inline constexpr float inv_pi = 1.0F / pi;
inline constexpr float epsilon = 1.0e-6F;

[[nodiscard]] inline float saturate(const float value) noexcept {
    return std::clamp(value, 0.0F, 1.0F);
}

[[nodiscard]] inline float lerp(const float a, const float b, const float t) noexcept {
    return a + (b - a) * t;
}

[[nodiscard]] inline math::Color lerp(const math::Color& a,
                                      const math::Color& b,
                                      const float t) noexcept {
    return a + (b - a) * t;
}

[[nodiscard]] inline float pow5(const float value) noexcept {
    const float value2 = value * value;
    return value2 * value2 * value;
}

[[nodiscard]] inline float luminance(const math::Color& color) noexcept {
    return 0.2126F * color.x() + 0.7152F * color.y() + 0.0722F * color.z();
}

struct Frame final {
    math::Vec3 tangent{};
    math::Vec3 bitangent{};
    math::Vec3 normal{};

    explicit Frame(const math::Vec3& n) noexcept : normal(math::unit_vector(n)) {
        const math::Vec3 helper = std::abs(normal.z()) < 0.999F
            ? math::Vec3(0.0F, 0.0F, 1.0F)
            : math::Vec3(0.0F, 1.0F, 0.0F);
        tangent = math::unit_vector(math::cross(helper, normal));
        bitangent = math::cross(normal, tangent);
    }

    [[nodiscard]] math::Vec3 to_local(const math::Vec3& value) const noexcept {
        return {math::dot(value, tangent), math::dot(value, bitangent), math::dot(value, normal)};
    }

    [[nodiscard]] math::Vec3 to_world(const math::Vec3& value) const noexcept {
        return value.x() * tangent + value.y() * bitangent + value.z() * normal;
    }
};

} // namespace detail

// A single Disney principled BSDF implementation:
// - Burley diffuse / Hanrahan-Krueger subsurface approximation
// - anisotropic GGX microfacet reflection with Schlick Fresnel and Smith masking-shadowing
// - multiple-scattering LTC sheen fit to the SGGX volume reference (Zeltner et al., 2022)
// - cosine-weighted diffuse, Heitz VNDF GGX, and LTC sheen sampling with a matching mixture PDF
//
// Formula lineage: Shih-Chin Weng's rlDisney.cpp (MIT), based on Disney's published
// principled BRDF notes and Walter et al.'s microfacet model. See CREDITS.md.
class DisneyMaterial final : public Material {
public:
    explicit DisneyMaterial(DisneyParameters parameters) noexcept : parameters_(parameters) {
        parameters_.base_color = {
            std::clamp(parameters_.base_color.x(), 0.0F, 1.0F),
            std::clamp(parameters_.base_color.y(), 0.0F, 1.0F),
            std::clamp(parameters_.base_color.z(), 0.0F, 1.0F)
        };
        parameters_.subsurface = detail::saturate(parameters_.subsurface);
        parameters_.metallic = detail::saturate(parameters_.metallic);
        parameters_.specular = detail::saturate(parameters_.specular);
        parameters_.specular_tint = detail::saturate(parameters_.specular_tint);
        parameters_.roughness = std::clamp(parameters_.roughness, 0.001F, 1.0F);
        parameters_.anisotropic = detail::saturate(parameters_.anisotropic);
        parameters_.sheen = detail::saturate(parameters_.sheen);
        parameters_.sheen_tint = detail::saturate(parameters_.sheen_tint);
        parameters_.sheen_roughness = detail::saturate(parameters_.sheen_roughness);
        parameters_.opacity = detail::saturate(parameters_.opacity);
    }

    [[nodiscard]] const DisneyParameters& parameters() const noexcept {
        return parameters_;
    }

    // Evaluate the full non-delta Disney BRDF in inverse-square-meter radiometric units.
    // Both directions must point away from the surface and lie in the normal hemisphere.
    [[nodiscard]] math::Color evaluate(const core::Ray& incoming,
                                       const HitRecord& hit,
                                       const math::Vec3& outgoing) const noexcept {
        const detail::Frame frame(hit.normal);
        const math::Vec3 view = math::unit_vector(-incoming.direction());
        const math::Vec3 light = math::unit_vector(outgoing);
        const float n_dot_v = detail::saturate(math::dot(frame.normal, view));
        const float n_dot_l = detail::saturate(math::dot(frame.normal, light));
        if (n_dot_v <= detail::epsilon || n_dot_l <= detail::epsilon) {
            return math::Color(0.0F);
        }

        const math::Vec3 local_v = frame.to_local(view);
        const math::Vec3 local_l = frame.to_local(light);
        const detail::SheenLtcCoefficients sheen_coefficients =
            detail::fetch_sheen_ltc_coefficients(parameters_.sheen_roughness, n_dot_v);

        // The LTC lobe is cosine-weighted by construction. Its fitted PDF is
        // therefore used directly in f_sheen = Csheen * R * p_LTC / cos(theta_i).
        // This keeps evaluation, importance sampling, and the mixture PDF consistent.
        const float sheen_pdf = sheen_ltc_pdf(local_v, local_l, sheen_coefficients);
        math::Color result(0.0F);
        if (sheen_pdf > 0.0F && sheen_coefficients.reflectance > 0.0F) {
            result += sheen_color() *
                (sheen_coefficients.reflectance * sheen_pdf / n_dot_l);
        }

        const math::Vec3 half_sum = view + light;
        if (half_sum.near_zero()) {
            return result;
        }
        const math::Vec3 half_vector = math::unit_vector(half_sum);
        const float l_dot_h = detail::saturate(math::dot(light, half_vector));
        const float v_dot_h = detail::saturate(math::dot(view, half_vector));
        if (l_dot_h <= detail::epsilon || v_dot_h <= detail::epsilon) {
            return result;
        }

        const math::Vec3 local_h = frame.to_local(half_vector);
        const float alpha_x = alpha_x_();
        const float alpha_y = alpha_y_();

        const float distribution = ggx_distribution(local_h, alpha_x, alpha_y);
        const float geometry = smith_g1(local_v, alpha_x, alpha_y) *
                               smith_g1(local_l, alpha_x, alpha_y);
        const math::Color fresnel = schlick_fresnel(specular_f0(), v_dot_h);
        const math::Color specular = fresnel *
            (distribution * geometry / std::max(4.0F * n_dot_v * n_dot_l, detail::epsilon));

        const float diffuse_factor = burley_diffuse(n_dot_l, n_dot_v, l_dot_h);
        const float subsurface_factor = hanrahan_krueger_subsurface(
            n_dot_l, n_dot_v, l_dot_h);
        const float diffuse_scale = detail::lerp(
            diffuse_factor, subsurface_factor, parameters_.subsurface);
        const math::Color diffuse = parameters_.base_color *
            (detail::inv_pi * diffuse_scale * (1.0F - parameters_.metallic));

        return result + diffuse + specular;
    }

    [[nodiscard]] float pdf(const core::Ray& incoming,
                            const HitRecord& hit,
                            const math::Vec3& outgoing) const noexcept {
        const detail::Frame frame(hit.normal);
        const math::Vec3 view = math::unit_vector(-incoming.direction());
        const math::Vec3 light = math::unit_vector(outgoing);
        const float n_dot_v = detail::saturate(math::dot(frame.normal, view));
        const float n_dot_l = detail::saturate(math::dot(frame.normal, light));
        if (n_dot_v <= detail::epsilon || n_dot_l <= detail::epsilon) {
            return 0.0F;
        }

        const math::Vec3 local_v = frame.to_local(view);
        const math::Vec3 local_l = frame.to_local(light);
        const detail::SheenLtcCoefficients sheen_coefficients =
            detail::fetch_sheen_ltc_coefficients(parameters_.sheen_roughness, n_dot_v);
        const LobeProbabilities probabilities =
            lobe_probabilities(sheen_coefficients);

        float result = probabilities.diffuse * n_dot_l * detail::inv_pi;
        result += probabilities.sheen *
            sheen_ltc_pdf(local_v, local_l, sheen_coefficients);

        const math::Vec3 half_sum = view + light;
        if (half_sum.near_zero()) {
            return result;
        }
        const math::Vec3 half_vector = math::unit_vector(half_sum);
        const float v_dot_h = std::abs(math::dot(view, half_vector));
        const float n_dot_h = detail::saturate(math::dot(frame.normal, half_vector));
        if (v_dot_h <= detail::epsilon || n_dot_h <= detail::epsilon) {
            return result;
        }

        const math::Vec3 local_h = frame.to_local(half_vector);
        const float distribution = ggx_distribution(local_h, alpha_x_(), alpha_y_());
        const float visible_normal_pdf = distribution *
            smith_g1(frame.to_local(view), alpha_x_(), alpha_y_()) /
            (4.0F * n_dot_v);
        result += probabilities.specular * visible_normal_pdf;
        return std::isfinite(result) ? result : 0.0F;
    }

    [[nodiscard]] bool scatter(const core::Ray& incoming,
                               const HitRecord& hit,
                               math::Color& attenuation,
                               core::Ray& scattered,
                               Random& random) const override {
        // Opacity is stochastic surface coverage. A rejected coverage sample continues
        // the ray without changing its direction or throughput.
        if (random.uniform() >= parameters_.opacity) {
            scattered = core::Ray(hit.point, incoming.direction());
            attenuation = math::Color(1.0F);
            return true;
        }

        const detail::Frame frame(hit.normal);
        const math::Vec3 view = math::unit_vector(-incoming.direction());
        const float n_dot_v = detail::saturate(math::dot(frame.normal, view));
        if (n_dot_v <= detail::epsilon) {
            return false;
        }
        const math::Vec3 local_v = frame.to_local(view);
        const detail::SheenLtcCoefficients sheen_coefficients =
            detail::fetch_sheen_ltc_coefficients(parameters_.sheen_roughness, n_dot_v);
        const LobeProbabilities probabilities =
            lobe_probabilities(sheen_coefficients);
        math::Vec3 outgoing{};

        const float lobe_sample = random.uniform();
        if (lobe_sample < probabilities.sheen) {
            const math::Vec3 local_outgoing = sample_ltc_sheen(
                random, local_v, sheen_coefficients);
            outgoing = frame.to_world(local_outgoing);
        } else if (lobe_sample < probabilities.sheen + probabilities.specular) {
            const math::Vec3 local_half = sample_ggx_visible_normal(
                random, local_v, alpha_x_(), alpha_y_());
            const math::Vec3 half_vector = math::unit_vector(frame.to_world(local_half));
            if (math::dot(view, half_vector) <= detail::epsilon) {
                return false;
            }
            outgoing = math::reflect(incoming.direction(), half_vector);
        } else {
            outgoing = sample_cosine_hemisphere(random, frame);
        }

        const float n_dot_l = math::dot(frame.normal, outgoing);
        if (n_dot_l <= detail::epsilon || outgoing.near_zero()) {
            return false;
        }

        const float sample_pdf = pdf(incoming, hit, outgoing);
        if (!(sample_pdf > detail::epsilon) || !std::isfinite(sample_pdf)) {
            return false;
        }

        const math::Color value = evaluate(incoming, hit, outgoing);
        attenuation = value * (n_dot_l / sample_pdf);
        if (!std::isfinite(attenuation.x()) ||
            !std::isfinite(attenuation.y()) ||
            !std::isfinite(attenuation.z())) {
            return false;
        }

        scattered = core::Ray(hit.point, math::unit_vector(outgoing));
        return true;
    }

private:
    [[nodiscard]] float alpha_x_() const noexcept {
        const float aspect = std::sqrt(std::max(0.1F, 1.0F - 0.9F * parameters_.anisotropic));
        return std::max(0.001F, parameters_.roughness * parameters_.roughness / aspect);
    }

    [[nodiscard]] float alpha_y_() const noexcept {
        const float aspect = std::sqrt(std::max(0.1F, 1.0F - 0.9F * parameters_.anisotropic));
        return std::max(0.001F, parameters_.roughness * parameters_.roughness * aspect);
    }

    [[nodiscard]] math::Color tint_color() const noexcept {
        const float luma = detail::luminance(parameters_.base_color);
        return luma > detail::epsilon ? parameters_.base_color / luma : math::Color(1.0F);
    }

    [[nodiscard]] math::Color specular_f0() const noexcept {
        const math::Color tinted_specular = detail::lerp(
            math::Color(1.0F), tint_color(), parameters_.specular_tint) *
            (parameters_.specular * 0.08F);
        return detail::lerp(tinted_specular, parameters_.base_color, parameters_.metallic);
    }

    [[nodiscard]] math::Color sheen_color() const noexcept {
        return detail::lerp(math::Color(1.0F), tint_color(), parameters_.sheen_tint) *
               (parameters_.sheen * (1.0F - parameters_.metallic));
    }

    struct LobeProbabilities final {
        float diffuse = 1.0F;
        float specular = 0.0F;
        float sheen = 0.0F;
    };

    [[nodiscard]] LobeProbabilities lobe_probabilities(
        const detail::SheenLtcCoefficients& sheen_coefficients) const noexcept {
        const float diffuse_weight = detail::luminance(parameters_.base_color) *
                                     (1.0F - parameters_.metallic);
        float specular_weight = std::max(0.0F, detail::luminance(specular_f0()));
        const float sheen_weight = std::max(0.0F,
            detail::luminance(sheen_color()) * sheen_coefficients.reflectance);

        // Schlick Fresnel is non-zero at grazing angles even when F0 is zero.
        // Keep a small but non-zero GGX sampling probability so that this part of
        // the Disney specular lobe is not omitted by the mixture sampler.
        if (diffuse_weight > detail::epsilon && specular_weight <= detail::epsilon) {
            specular_weight = 0.05F * diffuse_weight;
        }

        const float total = diffuse_weight + specular_weight + sheen_weight;
        if (total <= detail::epsilon) {
            return {};
        }
        return {
            diffuse_weight / total,
            specular_weight / total,
            sheen_weight / total
        };
    }

    [[nodiscard]] static float sheen_ltc_pdf(
        const math::Vec3& local_view,
        const math::Vec3& local_light,
        const detail::SheenLtcCoefficients& coefficients) noexcept {
        if (local_view.z() <= detail::epsilon || local_light.z() <= detail::epsilon ||
            coefficients.a_inv <= detail::epsilon || coefficients.reflectance <= 0.0F) {
            return 0.0F;
        }

        const float phi = std::atan2(local_view.y(), local_view.x());
        const float cosine = std::cos(phi);
        const float sine = std::sin(phi);
        // Rotate the light direction by -phi(view) so that the LTC's principal
        // axis is aligned with the outgoing/view direction.
        const math::Vec3 aligned_light(
            cosine * local_light.x() + sine * local_light.y(),
            -sine * local_light.x() + cosine * local_light.y(),
            local_light.z());

        const math::Vec3 transformed(
            coefficients.a_inv * aligned_light.x() +
                coefficients.b_inv * aligned_light.z(),
            coefficients.a_inv * aligned_light.y(),
            aligned_light.z());
        const float length = transformed.length();
        if (length <= detail::epsilon || !std::isfinite(length)) {
            return 0.0F;
        }

        const float cosine_transformed = std::max(0.0F, transformed.z() / length);
        const float jacobian = coefficients.a_inv * coefficients.a_inv /
                               (length * length * length);
        const float value = cosine_transformed * detail::inv_pi * jacobian;
        return std::isfinite(value) ? value : 0.0F;
    }

    [[nodiscard]] static math::Vec3 sample_ltc_sheen(
        Random& random,
        const math::Vec3& local_view,
        const detail::SheenLtcCoefficients& coefficients) {
        // The inverse LTC transform maps a cosine-hemisphere sample to the fitted
        // multiple-scattering sheen distribution (Zeltner et al., SIGGRAPH 2022).
        const float radius = std::sqrt(random.uniform());
        const float phi_sample = 2.0F * detail::pi * random.uniform();
        const math::Vec3 cosine_sample(
            radius * std::cos(phi_sample),
            radius * std::sin(phi_sample),
            std::sqrt(std::max(0.0F, 1.0F - radius * radius)));

        if (coefficients.a_inv <= detail::epsilon) {
            return math::Vec3(0.0F, 0.0F, 1.0F);
        }

        const float inverse_a = 1.0F / coefficients.a_inv;
        const math::Vec3 unaligned(
            (cosine_sample.x() - coefficients.b_inv * cosine_sample.z()) * inverse_a,
            cosine_sample.y() * inverse_a,
            cosine_sample.z());
        const math::Vec3 normalized = math::unit_vector(unaligned);

        const float phi_view = std::atan2(local_view.y(), local_view.x());
        const float cosine = std::cos(phi_view);
        const float sine = std::sin(phi_view);
        return math::unit_vector(math::Vec3(
            cosine * normalized.x() - sine * normalized.y(),
            sine * normalized.x() + cosine * normalized.y(),
            normalized.z()));
    }

    [[nodiscard]] static float ggx_distribution(const math::Vec3& h,
                                                const float alpha_x,
                                                const float alpha_y) noexcept {
        if (h.z() <= 0.0F) {
            return 0.0F;
        }
        const float x = h.x() / alpha_x;
        const float y = h.y() / alpha_y;
        const float denominator_term = x * x + y * y + h.z() * h.z();
        const float denominator = detail::pi * alpha_x * alpha_y *
                                  denominator_term * denominator_term;
        return denominator > detail::epsilon ? 1.0F / denominator : 0.0F;
    }

    [[nodiscard]] static float smith_g1(const math::Vec3& w,
                                        const float alpha_x,
                                        const float alpha_y) noexcept {
        if (w.z() <= 0.0F) {
            return 0.0F;
        }
        const float sin2 = w.x() * w.x() + w.y() * w.y();
        if (sin2 <= detail::epsilon) {
            return 1.0F;
        }
        const float alpha2 = (alpha_x * alpha_x * w.x() * w.x() +
                              alpha_y * alpha_y * w.y() * w.y()) / sin2;
        const float tan2 = sin2 / (w.z() * w.z());
        const float root = std::sqrt(1.0F + alpha2 * tan2);
        return 2.0F / (1.0F + root);
    }

    [[nodiscard]] static math::Color schlick_fresnel(const math::Color& f0,
                                                     const float cosine) noexcept {
        const float factor = detail::pow5(1.0F - detail::saturate(cosine));
        return f0 + (math::Color(1.0F) - f0) * factor;
    }

    [[nodiscard]] float burley_diffuse(const float n_dot_l,
                                       const float n_dot_v,
                                       const float l_dot_h) const noexcept {
        const float l_term = detail::pow5(1.0F - n_dot_l);
        const float v_term = detail::pow5(1.0F - n_dot_v);
        const float f90 = 0.5F + 2.0F * parameters_.roughness * l_dot_h * l_dot_h;
        const float light_scatter = detail::lerp(1.0F, f90, l_term);
        const float view_scatter = detail::lerp(1.0F, f90, v_term);
        return light_scatter * view_scatter;
    }

    [[nodiscard]] float hanrahan_krueger_subsurface(const float n_dot_l,
                                                   const float n_dot_v,
                                                   const float l_dot_h) const noexcept {
        const float l_term = detail::pow5(1.0F - n_dot_l);
        const float v_term = detail::pow5(1.0F - n_dot_v);
        const float fss90 = parameters_.roughness * l_dot_h * l_dot_h;
        const float fss = detail::lerp(1.0F, fss90, l_term) *
                          detail::lerp(1.0F, fss90, v_term);
        const float reciprocal_cosines = 1.0F /
            std::max(n_dot_l + n_dot_v, 1.0e-4F);
        // Disney's 1.25 factor approximately preserves albedo for the diffuse SSS lobe.
        return 1.25F * (fss * (reciprocal_cosines - 0.5F) + 0.5F);
    }

    [[nodiscard]] static math::Vec3 sample_cosine_hemisphere(Random& random,
                                                             const detail::Frame& frame) {
        const float r = std::sqrt(random.uniform());
        const float phi = 2.0F * detail::pi * random.uniform();
        const float x = r * std::cos(phi);
        const float y = r * std::sin(phi);
        const float z = std::sqrt(std::max(0.0F, 1.0F - x * x - y * y));
        return math::unit_vector(frame.to_world(math::Vec3(x, y, z)));
    }

    [[nodiscard]] static math::Vec3 sample_ggx_visible_normal(Random& random,
                                                              const math::Vec3& view,
                                                              const float alpha_x,
                                                              const float alpha_y) {
        // Heitz visible-normal sampling for anisotropic GGX.
        const math::Vec3 stretched_view = math::unit_vector(
            math::Vec3(alpha_x * view.x(), alpha_y * view.y(), view.z()));
        const float lensq = stretched_view.x() * stretched_view.x() +
                            stretched_view.y() * stretched_view.y();
        const math::Vec3 tangent1 = lensq > detail::epsilon
            ? math::Vec3(-stretched_view.y(), stretched_view.x(), 0.0F) / std::sqrt(lensq)
            : math::Vec3(1.0F, 0.0F, 0.0F);
        const math::Vec3 tangent2 = math::cross(stretched_view, tangent1);

        const float radius = std::sqrt(random.uniform());
        const float phi = 2.0F * detail::pi * random.uniform();
        const float disk_x = radius * std::cos(phi);
        float disk_y = radius * std::sin(phi);
        const float blend = 0.5F * (1.0F + stretched_view.z());
        disk_y = (1.0F - blend) * std::sqrt(std::max(0.0F, 1.0F - disk_x * disk_x)) +
                 blend * disk_y;

        const float normal_z = std::sqrt(std::max(
            0.0F, 1.0F - disk_x * disk_x - disk_y * disk_y));
        const math::Vec3 visible_normal = disk_x * tangent1 + disk_y * tangent2 +
                                          normal_z * stretched_view;
        return math::unit_vector(math::Vec3(
            alpha_x * visible_normal.x(),
            alpha_y * visible_normal.y(),
            std::max(0.0F, visible_normal.z())));
    }

    DisneyParameters parameters_;
};

} // namespace secret::render
