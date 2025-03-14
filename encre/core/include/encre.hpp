#pragma once

#include "encre_export.hpp"

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <vector>
#include <span>
#include <map>
#include <optional>

namespace encre {
    struct XYZ : glm::vec3 {
        using glm::vec3::vec3;

        constexpr explicit XYZ(const glm::vec3& v) : glm::vec3(v) {
        }
    };

    struct CIEXYZ : XYZ {
        using XYZ::XYZ;

        constexpr explicit CIEXYZ(const glm::vec3& v) : XYZ(v) {
        }
    };

    struct Lab : glm::vec3 {
        using glm::vec3::vec3;

        constexpr explicit Lab(const glm::vec3& v) : glm::vec3(v) {
        }
    };

    struct CIELab : Lab {
        using Lab::Lab;

        constexpr explicit CIELab(const glm::vec3& v) : Lab(v) {
        }
    };

    struct Oklab : Lab {
        using Lab::Lab;

        constexpr explicit Oklab(const glm::vec3& v) : Lab(v) {
        }
    };

    struct Plane : glm::vec4 {
        using glm::vec4::vec4;
    };

    struct Edge : glm::mat2x3 {
        using glm::mat2x3::mat2x3;
    };

    struct Range : glm::vec2 {
        using glm::vec2::vec2;
    };

    struct Palette {
        static constexpr float default_target_lightness = 80;

        std::vector<Oklab> points;
        std::vector<Oklab> gamut_vertices;
        std::vector<Edge> gamut_edges;
        std::vector<Plane> gamut_planes;
        Range gray_range{};
        float lightness_range{};
        float max_chroma{};
    };

    enum class Rotation : uint8_t {
        automatic,
        landscape,
        portrait,
        landscape_upside_down,
        portrait_upside_down
    };

    struct Options {
        static constexpr Rotation default_rotation = Rotation::automatic;
        static constexpr float default_dynamic_range = 0.85f;
        static constexpr float default_contrast = 0.6f;
        static constexpr std::nullopt_t automatic_exposure = std::nullopt;
        static constexpr std::nullopt_t automatic_brightness = std::nullopt;
        static constexpr float no_exposure_change = 1;
        static constexpr float no_brightness_change = 0;
        static constexpr float default_sharpening = 3;
        static constexpr float default_gray_chroma_tolerance = 2.5f;
        static constexpr bool default_hue_dependent_chroma_clamping = true;
        static constexpr float default_clipped_chroma_recovery = 10;
        static constexpr float default_error_attenuation = 0.1f;

        Rotation rotation = default_rotation;
        float dynamic_range = default_dynamic_range;
        std::optional<float> exposure = automatic_exposure;
        std::optional<float> brightness = automatic_brightness;
        float contrast = default_contrast;
        float sharpening = default_sharpening;
        float gray_chroma_tolerance = default_gray_chroma_tolerance;
        bool hue_dependent_chroma_clamping = default_hue_dependent_chroma_clamping;
        float clipped_chroma_recovery = default_clipped_chroma_recovery;
        float error_attenuation = default_error_attenuation;
    };

    // Using std::map to keep the name ordering consistent
    ENCRE_EXPORT extern const std::map<std::string, encre::Rotation> rotation_by_name;

    // E-Ink Gallery "Palette" palette, measured as best I could from:
    // https://shop.pimoroni.com/products/inky-impression-7-3.
    // The chroma from the black and white colours was removed to
    // provide better contrast, at the cost of a bit of grayscale accuracy.
    ENCRE_EXPORT extern const Palette eink_gallery_palette_palette;

    // A strictly accurate version of eink_gallery_palette_palette with correct
    // blacks and whites, but with less contrast.
    ENCRE_EXPORT extern const Palette eink_gallery_palette_accurate_palette;

    // E-Ink Spectra 6 palette, measured as best I could from:
    // https://buyepaper.com/products/gdep073e01.
    // The chroma from the black and white colours was removed to
    // provide better contrast, at the cost of a bit of grayscale accuracy.
    ENCRE_EXPORT extern const Palette eink_spectra_6_palette;

    // A strictly accurate version of eink_spectra_6_palette with correct
    // blacks and whites, but with less contrast.
    ENCRE_EXPORT extern const Palette eink_spectra_6_accurate_palette;

    ENCRE_EXPORT extern const std::map<std::string, Palette> palette_by_name;

    ENCRE_EXPORT void initialize(const char* executable_path);

    ENCRE_EXPORT void uninitalize();

    ENCRE_EXPORT Palette make_palette(std::span<const CIEXYZ> colors, float target_lightness = Palette::default_target_lightness);

    ENCRE_EXPORT Palette make_palette(std::span<const CIELab> colors, float target_lightness = Palette::default_target_lightness);

    ENCRE_EXPORT bool convert(const char* image_path, uint32_t width, const Palette& palette, const Options& options,
            std::span<uint8_t> output, Rotation* output_rotation = nullptr);

    ENCRE_EXPORT bool write_preview(std::span<const uint8_t> converted, uint32_t width, std::span<const Oklab> palette_points,
            const Rotation& output_rotation, const char* image_path);

    ENCRE_EXPORT bool write_encre_file(std::span<const uint8_t> converted, uint32_t width, std::span<const Oklab> palette_points,
            const Rotation& output_rotation, const char* image_path);

    ENCRE_EXPORT bool read_encre_file(const char* image_path, std::vector<uint8_t>& output, uint32_t& width,
            std::vector<Oklab>& palette_points, Rotation& output_rotation);

    ENCRE_EXPORT bool read_compatible_encre_file(const char* image_path, uint32_t width, size_t palette_size,
            std::span<uint8_t> output, Rotation* output_rotation = nullptr);
}
