#include <encre.hpp>

#include <argparse/argparse.hpp>

#include <iostream>
#include <filesystem>

namespace {
    template<typename T, typename S = T, bool optional = false>
    const auto read_option(const argparse::ArgumentParser& arguments, std::string_view name, T& storage) {
        if constexpr (optional) {
            if (const auto value = arguments.present<S>(name)) {
                storage = static_cast<T>(*value);
            }
        } else {
            storage = static_cast<T>(arguments.get<S>(name));
        }
    }

    template<typename T>
    const auto read_option(const argparse::ArgumentParser& arguments, std::string_view name, std::optional<T>& storage) {
        read_option<std::optional<T>, T, true>(arguments, name, storage);
    }
}

int main(int arg_count, char** arg_values) {
    argparse::ArgumentParser arguments("encre-cli", "0.0.1");
    arguments.add_description("Command line interface for Encre");
    arguments.add_argument("input_image");
    arguments.add_argument("-w", "--width").scan<'u', uint32_t>().metavar("width").help("output width").
            default_value(800u);
    arguments.add_argument("-h", "--height").scan<'u', uint32_t>().metavar("height").help("output height").
            default_value(480u);
    arguments.add_argument("-o", "--out").metavar("output_binary").help("output binary").
            default_value("-");
    arguments.add_argument("-p", "--preview").metavar("output_preview_image").help("output preview image").
            default_value("-");
    arguments.add_argument("-v", "--dynamic-range").scan<'g', float>().metavar("percentage").help("dynamic range").
            default_value(encre::Options::default_dynamic_range);
    arguments.add_argument("-e", "--exposure").scan<'g', float>().metavar("scale").help("manual exposure adjustment");
    arguments.add_argument("-b", "--brightness").scan<'g', float>().metavar("bias").help("manual brightness adjustment");
    arguments.add_argument("-c", "--contrast").scan<'g', float>().metavar("factor").help("contrast").
            default_value(encre::Options::default_contrast);
    arguments.add_argument("-s", "--sharpening").scan<'g', float>().metavar("factor").help("sharpening").
            default_value(encre::Options::default_sharpening);
    arguments.add_argument("-t", "--gray-chroma-tolerance").scan<'g', float>().metavar("factor").help("gray chroma tolerance").
            default_value(encre::Options::default_gray_chroma_tolerance);
    arguments.add_argument("-d", "--hue-dependent-chroma-clamping").scan<'u', uint32_t>().help("hue dependent chroma clamping").
            default_value(uint32_t{encre::Options::default_hue_dependent_chroma_clamping});
    arguments.add_argument("-g", "--clipped-chroma-recovery").scan<'g', float>().metavar("factor").help("clipped chroma recovery").
            default_value(encre::Options::default_clipped_chroma_recovery);
    arguments.add_argument("-a", "--error-attenuation").scan<'g', float>().metavar("factor").help("dither error attenuation").
            default_value(encre::Options::default_error_attenuation);

    auto& rotation_argument = arguments.add_argument("-r", "--rotation").metavar("orientation").help("image rotation").
            default_value("automatic");
    for (const auto& [name, _] : encre::rotation_by_name) {
        rotation_argument.add_choice(name);
    }

    auto& palette_argument = arguments.add_argument("-l", "--palette").metavar("name").help("display palette").
            default_value("eink_spectra_6");
    for (const auto& [name, _] : encre::palette_by_name) {
        palette_argument.add_choice(name);
    }

    try {
        arguments.parse_args(arg_count, arg_values);
    } catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << arguments;
        return 1;
    }

    const auto width = arguments.get<uint32_t>("-w");
    const auto height = arguments.get<uint32_t>("-h");

    const std::filesystem::path input_image_path = arguments.get("input_image");

    std::filesystem::path output_image_path = arguments.get("-o");
    if (output_image_path == "-") {
        output_image_path = std::filesystem::path(input_image_path).replace_extension(".bin");
    }

    std::filesystem::path preview_image_path = arguments.is_used("-p") ? arguments.get("-p") : "";
    if (preview_image_path == "-") {
        preview_image_path = (output_image_path.parent_path() / (output_image_path.stem() += "_preview.png"));
    }

    auto palette = &encre::palette_by_name.at(arguments.get("-l"));

    encre::Options options{};
    options.rotation = encre::rotation_by_name.at(arguments.get("-r"));
    read_option(arguments, "-v", options.dynamic_range);
    read_option(arguments, "-e", options.exposure);
    read_option(arguments, "-b", options.brightness);
    read_option(arguments, "-c", options.contrast);
    read_option(arguments, "-s", options.sharpening);
    read_option(arguments, "-t", options.gray_chroma_tolerance);
    read_option<bool, uint32_t>(arguments, "-d", options.hue_dependent_chroma_clamping);
    read_option(arguments, "-g", options.clipped_chroma_recovery);
    read_option(arguments, "-a", options.error_attenuation);

    encre::initialize(arg_values[0]);

    std::vector<uint8_t> output;
    output.resize(width * height);
    encre::Rotation output_rotation{};

    int result_code = 0;
    if (!encre::read_compatible_encre_file(input_image_path.c_str(), width, palette->points.size(),
            output, &output_rotation) && !(encre::convert(input_image_path.c_str(),
            width, *palette, options, output, &output_rotation) && encre::write_encre_file(
            output, width, palette->points, output_rotation, output_image_path.c_str()))) {
        result_code = 1;
        std::cerr << "Failed to convert\n";
    }

    if (result_code == 0 && !preview_image_path.empty() &&
            !encre::write_preview(output, width, palette->points, output_rotation,
            preview_image_path.c_str())) {
        result_code = 1;
        std::cerr << "Failed to write preview\n";
    }

    encre::uninitalize();

    return result_code;
}
