#include "dither.hpp"

#include <vips/vips8>

#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <glm/gtx/compatibility.hpp>

namespace {
    constexpr float epsilon = 1e-5f;
    constexpr auto lightness_relative_scale = 0.5f;

    bool is_inside_palette_gamut(const encre::Palette& palette, const glm::vec3& lab) {
        for (const auto& plane : palette.gamut_planes) {
            const auto distance = glm::dot(glm::vec4(lab, 1), plane);
            if (distance >= epsilon) {
                return false;
            }
        }

        return true;
    }

    glm::mat3 find_hue_extrema(const encre::Palette& palette, const glm::vec3& lab, float gray_chroma_tolerance) {
        const auto hue_normal = glm::normalize(glm::vec3(0, -lab.z, lab.y));

        auto lc_at_extrema = glm::mat3x2(glm::vec2(0, 0), glm::vec2(1e10, 0), glm::vec2(0, 0));
        auto lab_at_extrema = glm::mat3(glm::vec3(glm::mix(palette.gray_range.x, palette.gray_range.y, 0.5), 0, 0),
                glm::vec3(0), glm::vec3(0));

        for (const auto& edge : palette.gamut_edges) {
            const auto edge_start = edge[0];
            const auto edge_delta = edge[1] - edge_start;
            const auto edge_length = glm::length(edge_delta);
            const auto edge_direction = edge_delta / edge_length;
            const auto d = glm::dot(edge_direction, hue_normal);
            if (abs(d) < epsilon) {
                continue;
            }

            const auto t = glm::dot(-edge_start, hue_normal) / d;
            if (t < 0 || t > edge_length) {
                continue;
            }

            const auto intersection = edge_start + t * edge_direction;
            if (glm::dot(glm::yz(lab), glm::yz(intersection)) < epsilon * epsilon) {
                continue;
            }

            const auto intersection_chroma = glm::length(glm::yz(intersection));
            if (intersection_chroma > lc_at_extrema[0].y) {
                lc_at_extrema[0] = glm::vec2(intersection.x, intersection_chroma);
                lab_at_extrema[0] = intersection;
            }
            if (intersection.x < lc_at_extrema[1].x) {
                lc_at_extrema[1] = glm::vec2(intersection.x, intersection_chroma);
                lab_at_extrema[1] = intersection;
            }
            if (intersection.x > lc_at_extrema[2].x) {
                lc_at_extrema[2] = glm::vec2(intersection.x, intersection_chroma);
                lab_at_extrema[2] = intersection;
            }
        }

        if (palette.gray_range.x < lc_at_extrema[1].x) {
            lab_at_extrema[1] = glm::vec3(palette.gray_range.x, 0, 0);
        } else if (lc_at_extrema[1].y > gray_chroma_tolerance) {
            lab_at_extrema[1] = mix(glm::vec3(palette.gray_range.x, 0, 0),
                lab_at_extrema[1], gray_chroma_tolerance / lc_at_extrema[1].y);
        }

        if (palette.gray_range.y > lc_at_extrema[2].x) {
            lab_at_extrema[2] = glm::vec3(palette.gray_range.y, 0, 0);
        } else if (lc_at_extrema[2].y > gray_chroma_tolerance) {
            lab_at_extrema[2] = glm::mix(glm::vec3(palette.gray_range.y, 0, 0),
                lab_at_extrema[2], gray_chroma_tolerance / lc_at_extrema[2].y);
        }

        return lab_at_extrema;
    }

    // From https://bottosson.github.io/posts/gamutclipping/#adaptive-%2C-hue-independent
    glm::vec3 compute_gamut_clamp_target(const encre::Palette& palette, float alpha,
            const glm::vec3& lab, float chroma, const glm::mat3& extrema) {
        float range = extrema[2].x - extrema[1].x;
        float l_start = (lab.x - extrema[1].x) / range;
        if (alpha < epsilon) {
            return glm::mix(extrema[1], extrema[2], glm::clamp(l_start, 0.0f, 1.0f));
        }

        const auto l_diff = l_start - 0.5f;
        const auto e_1 = 0.5f + glm::abs(l_diff) + alpha * chroma * 0.01f;
        const auto l_target = (1 + glm::sign(l_diff) * (e_1 - glm::sqrt(glm::max(0.f, e_1 * e_1 - 2 * glm::abs(l_diff))))) * 0.5f;

        return glm::mix(extrema[1], extrema[2], l_target);
    }

    // From https://bottosson.github.io/posts/gamutclipping/#adaptive-%2C-hue-dependent
    glm::vec3 compute_gamut_clamp_target_hue_dependent(const encre::Palette& palette, float alpha,
            const glm::vec3& lab, float chroma, const glm::mat3& extrema) {
        float range = extrema[2].x - extrema[1].x;
        float l_start = (lab.x - extrema[1].x) / range;
        if (alpha < epsilon) {
            return glm::mix(extrema[1], extrema[2], glm::clamp(l_start, 0.0f, 1.0f));
        }

        const auto l_peak = (extrema[0].x - extrema[1].x) / range;
        const auto l_diff = l_start - l_peak;
        const auto k = 2 * (l_diff >= 0 ? (1 - l_peak) : l_peak);
        const auto e_1 = k / 2 + abs(l_diff) + alpha * chroma * 0.01f / k;
        const auto l_target = l_peak + (glm::sign(l_diff) *
                (e_1 - glm::sqrt(glm::max(0.f, e_1 * e_1 - 2 * k * glm::abs(l_diff))))) * 0.5f;

        return glm::mix(extrema[1], extrema[2], l_target);
    }

    glm::vec3 clamp_to_palette_gamut(const encre::Palette& palette, float gray_chroma_tolerance, bool hue_dependent_chroma_clamping,
            float clipped_chroma_recovery, const glm::vec3& lab) {
        const auto chroma = glm::length(glm::yz(lab));
        if (chroma < epsilon) {
            return {glm::clamp(lab.x, palette.gray_range.x, palette.gray_range.y), 0, 0};
        }

        const auto alpha = clipped_chroma_recovery;
        const auto extrema = find_hue_extrema(palette, lab, gray_chroma_tolerance);
        const auto target = hue_dependent_chroma_clamping ?
                compute_gamut_clamp_target_hue_dependent(palette, alpha, lab, chroma, extrema) :
                compute_gamut_clamp_target(palette, alpha, lab, chroma, extrema);
        const auto clamp_direction = glm::normalize(target - lab);
        const auto hue_chroma = glm::normalize(glm::yz(lab));

        glm::vec3 clamped_lab{};
        auto closest_distance_to_target = std::numeric_limits<float>::max();
        for (const auto& plane : palette.gamut_planes) {
            float angle_cosine = glm::dot(clamp_direction, glm::xyz(plane));
            if (angle_cosine > -epsilon) {
                continue;
            }

            const auto t = -glm::dot(glm::vec4(lab, 1), plane) / angle_cosine;
            const auto lab_projected = lab + t * clamp_direction;

            if (glm::dot(hue_chroma, glm::yz(lab_projected)) < -epsilon) {
                continue;
            }

            const auto distance_to_target = glm::distance(target, lab_projected);
            if (distance_to_target >= closest_distance_to_target) {
                continue;
            }

            clamped_lab = lab_projected;
            closest_distance_to_target = distance_to_target;
        }

        return clamped_lab;
    }

    void clamp_gamut_batch(const encre::Palette& palette, float gray_chroma_tolerance, bool hue_dependent_chroma_clamping,
            float clipped_chroma_recovery, const vips::VRegion& in_region) {
        const auto in_rectangle = in_region.valid();

        for (int y = 0; y < in_rectangle.height; y++) {
            const auto p = reinterpret_cast<float*>(in_region.addr(in_rectangle.left, in_rectangle.top + y));
            for (int x = 0; x < in_rectangle.width; x++) {
                const int ix = x * 3;

                const auto lab = glm::make_vec3(p + ix);
                if (is_inside_palette_gamut(palette, lab)) {
                    continue;
                }

                const auto clamped_lab = clamp_to_palette_gamut(palette, gray_chroma_tolerance, hue_dependent_chroma_clamping,
                    clipped_chroma_recovery, lab);

                #ifndef NDEBUG
                if (!glm::all(glm::isfinite(clamped_lab)) || !is_inside_palette_gamut(palette, clamped_lab)) {
                    __builtin_debugtrap();
                }
                #endif

                std::memcpy(p + ix, glm::value_ptr(clamped_lab), sizeof(clamped_lab));
            }
        }
    }

    std::pair<uint8_t, float> closest_palette_color(const encre::Palette& palette, const glm::vec3& lab) {
        const auto normalization_scale = 1.f / glm::vec3{palette.lightness_range * lightness_relative_scale,
                palette.max_chroma, palette.max_chroma};

        auto closest_distance_square = std::numeric_limits<float>::max();
        int closest_index = -1;
        for (int i = 0; i < palette.points.size(); i++) {
            const auto delta = lab - palette.points[i];
            const auto distance_square = glm::length2(delta * normalization_scale);
            if (distance_square < closest_distance_square) {
                closest_distance_square = distance_square;
                closest_index = i;
            }
        }

        return {static_cast<uint8_t>(closest_index), glm::sqrt(closest_distance_square)};
    }

    void diffuse_dither_error_fs(const VipsRect& rectangle, const glm::ivec3& position, const glm::vec3& delta,
            float* p, float* p_down) {
        if (position.x + 1 < rectangle.width) {
            const auto ix_right = position.z + 3;
            const auto right_pixel = glm::make_vec3(p + ix_right) + delta * (7.f / 16);
            std::memcpy(p + ix_right, glm::value_ptr(right_pixel), sizeof(right_pixel));
        }

        if (position.y + 1 < rectangle.height) {
            if (position.x >= 1) {
                const auto ix_left = position.z - 3;
                const auto down_left_pixel = glm::make_vec3(p_down + ix_left) + delta * (3.f / 16);
                std::memcpy(p_down + ix_left, glm::value_ptr(down_left_pixel), sizeof(down_left_pixel));
            }

            {
                const auto down_pixel = glm::make_vec3(p_down + position.z) + delta * (5.f / 16);
                std::memcpy(p_down + position.z, glm::value_ptr(down_pixel), sizeof(down_pixel));
            }

            if (position.x + 1 < rectangle.width) {
                const auto ix_right = position.z + 3;
                const auto down_right_pixel = glm::make_vec3(p_down + ix_right) + delta * (1.f / 16);
                std::memcpy(p_down + ix_right, glm::value_ptr(down_right_pixel), sizeof(down_right_pixel));
            }
        }
    }

    void dither_batch(const encre::Palette& palette, float error_attenuation,
            const vips::VRegion& in_region, const vips::VRegion& out_region) {
        const auto in_rectangle = in_region.valid();
        const auto out_rectangle = out_region.valid();

        for (int y = 0; y < out_rectangle.height; y++) {
            const auto p = reinterpret_cast<float*>(in_region.addr(out_rectangle.left, out_rectangle.top + y));
            const auto p_down = reinterpret_cast<float*>(in_region.addr(out_rectangle.left, out_rectangle.top + y + 1));
            const auto q = reinterpret_cast<uint8_t*>(out_region.addr(out_rectangle.left, out_rectangle.top + y));
            for (int x = 0; x < out_rectangle.width; x++) {
                const int ix = x * 3;

                const auto old_pixel = glm::make_vec3(p + ix);
                const auto [new_index, error] = closest_palette_color(palette, old_pixel);
                #ifndef NDEBUG
                if (new_index >= palette.points.size()) {
                    __builtin_debugtrap();
                }
                #endif
                const auto new_pixel = static_cast<glm::vec3>(palette.points[new_index]);

                q[x] = new_index;
                std::memcpy(p + ix, glm::value_ptr(new_pixel), sizeof(new_pixel));

                const auto scale = glm::exp(-error_attenuation * error);
                const auto delta = (old_pixel - new_pixel) * scale;
                diffuse_dither_error_fs(in_rectangle, {x, y, ix}, delta, p, p_down);
            }
        }
    }
}

namespace encre {
    void dither(vips::VImage& in, const Palette& palette, float gray_chroma_tolerance, bool hue_dependent_chroma_clamping,
            float clipped_chroma_recovery, float error_attenuation, std::span<uint8_t> result) {
        if (vips_check_uncoded("dither", in.get_image()) ||
                vips_check_bands("dither", in.get_image(), 3) ||
                vips_check_format("dither", in.get_image(), VIPS_FORMAT_FLOAT)) {
            throw vips::VError("Invalid dither image format. Expected uncoded with 3 float bands.");
        }

        in = in.copy_memory();
        const auto in_region = vips::VRegion::new_from_image(in);

        const auto width = in.width();
        const auto height = in.height();

        auto out = vips::VImage::new_from_memory(result.data(), result.size_bytes(),
            width, height, 1, VIPS_FORMAT_UCHAR);
        const auto out_region = vips::VRegion::new_from_image(out);

        for (int y = 0; y < height; y++) {
            in_region.prepare(0, y, width, 1);
            clamp_gamut_batch(palette, gray_chroma_tolerance, hue_dependent_chroma_clamping, clipped_chroma_recovery, in_region);
        }

        for (int y = 0; y < height; y++) {
            in_region.prepare(0, y, width, 2);
            out_region.prepare(0, y, width, 1);
            dither_batch(palette, error_attenuation, in_region, out_region);
        }
    }
}
