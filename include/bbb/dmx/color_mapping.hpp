#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "bbb/dmx/fixture_profile.hpp"
#include "bbb/dmx/semantic_overrides.hpp"

namespace bbb::dmx {

struct semantic_color_request {
public:
    double red{0.0};
    double green{0.0};
    double blue{0.0};
};

struct semantic_color_options {
public:
    bool use_white{true};
    bool color_wheel_fallback{false};
};

struct semantic_color_mapping {
public:
    bool ok{false};
    std::string message{};
    std::vector<std::pair<std::string, double>> parameters{};

    static semantic_color_mapping success(std::vector<std::pair<std::string, double>> mapped_parameters) {
        return semantic_color_mapping{true, "", std::move(mapped_parameters)};
    }

    static semantic_color_mapping failure(const std::string &message) {
        return semantic_color_mapping{false, message, {}};
    }
};

inline double clamp_normalized_color(double value) {
    if(!std::isfinite(value)) {
        return 0.0;
    }
    return std::max(0.0, std::min(1.0, value));
}

inline semantic_color_request make_semantic_color_request(double red, double green, double blue) {
    return semantic_color_request{
        clamp_normalized_color(red),
        clamp_normalized_color(green),
        clamp_normalized_color(blue)
    };
}

inline std::string normalized_color_key(const std::string &text) {
    std::string result;
    result.reserve(text.size());
    for(const char character : text) {
        const unsigned char unsigned_character{(unsigned char)character};
        if(std::isalnum(unsigned_character)) {
            result.push_back((char)std::tolower(unsigned_character));
        }
    }
    return result;
}

inline bool mode_has_semantic_parameter(const fixture_mode &mode, const char *parameter) {
    return mode.find_parameter(parameter) != nullptr;
}

inline int color_parameter_first_offset(const fixture_mode &mode, const fixture_parameter &parameter) {
    if(parameter.channels.empty()) {
        return 0;
    }
    const fixture_channel *channel{mode.find_channel(parameter.channels.front())};
    if(!channel) {
        return 0;
    }
    return channel->offset;
}

inline bool color_parameter_is_dimmer_like(const fixture_parameter &parameter) {
    std::string normalized;
    normalized.reserve(parameter.key.size());
    for(const char character : parameter.key) {
        const unsigned char unsigned_character{(unsigned char)character};
        if(std::isalnum(unsigned_character)) {
            normalized.push_back((char)std::tolower(unsigned_character));
        }
    }
    return normalized == "dimmer" ||
        normalized.find("dimmer") == 0 ||
        normalized.find("intensity") != std::string::npos;
}

inline bool color_parameter_ranges_indicate_intensity(const fixture_parameter &parameter) {
    if(parameter.ranges.empty()) {
        return true;
    }
    for(const auto &range : parameter.ranges) {
        const std::string descriptor{normalized_color_key(range.function + " " + range.label)};
        if(
            descriptor == "dimmer" ||
            descriptor == "intensity" ||
            descriptor == "min" ||
            descriptor == "max" ||
            descriptor == "open" ||
            descriptor == "closed" ||
            descriptor.find("dimmer") != std::string::npos ||
            descriptor.find("intensity") != std::string::npos
        ) {
            return true;
        }
    }
    return false;
}

inline bool color_parameter_is_intensity_dimmer_like(const fixture_parameter &parameter) {
    return color_parameter_is_dimmer_like(parameter) && color_parameter_ranges_indicate_intensity(parameter);
}

inline const fixture_parameter *associated_color_dimmer_parameter(
    const fixture_mode &mode,
    const std::vector<std::string> &color_parameter_keys
) {
    int first_color_offset{0};
    for(const auto &key : color_parameter_keys) {
        const fixture_parameter *parameter{mode.find_parameter(key)};
        if(!parameter) {
            continue;
        }
        const int offset{color_parameter_first_offset(mode, *parameter)};
        if(offset <= 0) {
            continue;
        }
        if(first_color_offset == 0 || offset < first_color_offset) {
            first_color_offset = offset;
        }
    }
    if(first_color_offset <= 0) {
        return nullptr;
    }

    const fixture_parameter *best_parameter{nullptr};
    int best_offset{-1};
    for(const auto &parameter : mode.parameters) {
        if(!color_parameter_is_intensity_dimmer_like(parameter)) {
            continue;
        }
        const int offset{color_parameter_first_offset(mode, parameter)};
        if(offset <= 0 || first_color_offset <= offset) {
            continue;
        }
        if(best_offset < offset) {
            best_parameter = &parameter;
            best_offset = offset;
        }
    }
    return best_parameter;
}

inline double color_request_intensity(const semantic_color_request &color) {
    return std::max(color.red, std::max(color.green, color.blue));
}

inline double color_component_for_associated_dimmer(double component, double intensity) {
    if(intensity <= 0.0) {
        return 0.0;
    }
    return std::max(0.0, std::min(1.0, component / intensity));
}

inline const fixture_parameter *first_intensity_dimmer_parameter(const fixture_mode &mode) {
    if(const fixture_parameter *parameter = mode.find_parameter("dimmer")) {
        if(color_parameter_is_intensity_dimmer_like(*parameter)) {
            return parameter;
        }
    }
    for(const auto &parameter : mode.parameters) {
        if(color_parameter_is_intensity_dimmer_like(parameter)) {
            return &parameter;
        }
    }
    return nullptr;
}

inline void append_semantic_color_parameter(
    std::vector<std::pair<std::string, double>> &parameters,
    const std::string &parameter_key,
    double value
) {
    for(auto &parameter : parameters) {
        if(parameter.first == parameter_key) {
            parameter.second = value;
            return;
        }
    }
    parameters.push_back({parameter_key, value});
}

inline semantic_color_mapping semantic_intensity_parameters_from_override(
    const fixture_semantic_mode_override &mode_override,
    double intensity
) {
    if(mode_override.intensity_parameters.empty()) {
        return semantic_color_mapping::failure("semantic override has no intensity parameters");
    }

    const double clamped_intensity{clamp_normalized_color(intensity)};
    std::vector<std::pair<std::string, double>> parameters{};
    for(const auto &parameter_key : mode_override.intensity_parameters) {
        append_semantic_color_parameter(parameters, parameter_key, clamped_intensity);
    }
    return semantic_color_mapping::success(parameters);
}

inline bool color_parameter_key_has_component_suffix(
    const std::string &parameter_key,
    const std::string &component,
    std::string &suffix
) {
    if(parameter_key == component) {
        suffix = "";
        return true;
    }
    const std::string prefix{component + "_"};
    if(parameter_key.find(prefix) == 0) {
        suffix = parameter_key.substr(component.size());
        return true;
    }
    return false;
}

struct rgb_color_block {
public:
    std::string red{};
    std::string green{};
    std::string blue{};
    std::string white{};
};

inline std::vector<rgb_color_block> rgb_color_blocks_for_mode(const fixture_mode &mode) {
    std::vector<rgb_color_block> blocks{};
    for(const auto &parameter : mode.parameters) {
        std::string suffix{};
        if(!color_parameter_key_has_component_suffix(parameter.key, "red", suffix)) {
            continue;
        }
        const std::string green_key{"green" + suffix};
        const std::string blue_key{"blue" + suffix};
        if(!mode.find_parameter(green_key) || !mode.find_parameter(blue_key)) {
            continue;
        }
        const std::string white_key{"white" + suffix};
        blocks.push_back(rgb_color_block{
            parameter.key,
            green_key,
            blue_key,
            mode.find_parameter(white_key) ? white_key : ""
        });
    }
    return blocks;
}

struct cmy_color_block {
public:
    std::string cyan{};
    std::string magenta{};
    std::string yellow{};
};

inline std::vector<cmy_color_block> cmy_color_blocks_for_mode(const fixture_mode &mode) {
    std::vector<cmy_color_block> blocks{};
    for(const auto &parameter : mode.parameters) {
        std::string suffix{};
        if(!color_parameter_key_has_component_suffix(parameter.key, "cyan", suffix)) {
            continue;
        }
        const std::string magenta_key{"magenta" + suffix};
        const std::string yellow_key{"yellow" + suffix};
        if(!mode.find_parameter(magenta_key) || !mode.find_parameter(yellow_key)) {
            continue;
        }
        blocks.push_back(cmy_color_block{parameter.key, magenta_key, yellow_key});
    }
    return blocks;
}

inline semantic_color_mapping semantic_intensity_parameters_for_mode(
    const fixture_mode &mode,
    double intensity,
    const fixture_semantic_mode_override *mode_override = nullptr
) {
    if(mode_override && !mode_override->intensity_parameters.empty()) {
        return semantic_intensity_parameters_from_override(*mode_override, intensity);
    }

    const double clamped_intensity{clamp_normalized_color(intensity)};
    std::vector<std::pair<std::string, double>> parameters{};
    for(const auto &parameter : mode.parameters) {
        if(!color_parameter_is_intensity_dimmer_like(parameter)) {
            continue;
        }
        append_semantic_color_parameter(parameters, parameter.key, clamped_intensity);
    }
    if(!parameters.empty()) {
        return semantic_color_mapping::success(parameters);
    }

    return semantic_color_mapping::failure("fixture has no semantic intensity parameter");
}

inline semantic_color_mapping semantic_primary_intensity_parameter_for_mode(
    const fixture_mode &mode,
    double intensity,
    const fixture_semantic_mode_override *mode_override = nullptr
) {
    const double clamped_intensity{clamp_normalized_color(intensity)};

    if(mode_override) {
        if(!mode_override->primary_intensity_parameter.empty()) {
            return semantic_color_mapping::success({{mode_override->primary_intensity_parameter, clamped_intensity}});
        }
        if(!mode_override->intensity_parameters.empty()) {
            return semantic_color_mapping::success({{mode_override->intensity_parameters.front(), clamped_intensity}});
        }
    }

    if(
        mode_has_semantic_parameter(mode, "red") &&
        mode_has_semantic_parameter(mode, "green") &&
        mode_has_semantic_parameter(mode, "blue")
    ) {
        if(const fixture_parameter *parameter = associated_color_dimmer_parameter(mode, {"red", "green", "blue", "white"})) {
            return semantic_color_mapping::success({{parameter->key, clamped_intensity}});
        }
    }

    if(
        mode_has_semantic_parameter(mode, "cyan") &&
        mode_has_semantic_parameter(mode, "magenta") &&
        mode_has_semantic_parameter(mode, "yellow")
    ) {
        if(const fixture_parameter *parameter = associated_color_dimmer_parameter(mode, {"cyan", "magenta", "yellow"})) {
            return semantic_color_mapping::success({{parameter->key, clamped_intensity}});
        }
    }

    if(const fixture_parameter *parameter = first_intensity_dimmer_parameter(mode)) {
        return semantic_color_mapping::success({{parameter->key, clamped_intensity}});
    }

    return semantic_color_mapping::failure("fixture has no semantic intensity parameter");
}

inline int color_parameter_max_value(const fixture_parameter &parameter) {
    if(parameter.type == fixture_parameter_type::u24) {
        return 16777215;
    }
    if(parameter.type == fixture_parameter_type::u16) {
        return 65535;
    }
    return 255;
}

inline int color_range_low_value(const fixture_parameter_range &range) {
    return std::min(range.from, range.to);
}

inline int color_range_center_value(const fixture_parameter_range &range) {
    const int minimum{color_range_low_value(range)};
    const int maximum{std::max(range.from, range.to)};
    return minimum + (maximum - minimum) / 2;
}

inline bool color_mix_priority_range_matches(const fixture_parameter &parameter, const fixture_parameter_range &range) {
    const std::string parameter_key{normalized_color_key(parameter.key)};
    const std::string descriptor{normalized_color_key(parameter.key + " " + range.function + " " + range.label)};
    if(
        parameter_key.find("colormixmode") == std::string::npos &&
        parameter_key.find("colourmixmode") == std::string::npos &&
        descriptor.find("colourchannels") == std::string::npos &&
        descriptor.find("colorchannels") == std::string::npos
    ) {
        return false;
    }
    return descriptor.find("colourmixhaspriority") != std::string::npos ||
        descriptor.find("colormixhaspriority") != std::string::npos ||
        descriptor.find("colourmixinghaspriority") != std::string::npos ||
        descriptor.find("colormixinghaspriority") != std::string::npos;
}

inline const fixture_parameter_range *best_color_mix_priority_range(const fixture_parameter &parameter) {
    const fixture_parameter_range *best_range{nullptr};
    int best_width{std::numeric_limits<int>::max()};
    for(const auto &range : parameter.ranges) {
        if(!color_mix_priority_range_matches(parameter, range)) {
            continue;
        }
        const int width{std::abs(range.to - range.from)};
        if(width < best_width) {
            best_range = &range;
            best_width = width;
        }
    }
    return best_range;
}

inline const fixture_parameter *color_mix_priority_parameter_for_mode(const fixture_mode &mode, const fixture_parameter_range *&range) {
    for(const auto &parameter : mode.parameters) {
        const fixture_parameter_range *candidate_range{best_color_mix_priority_range(parameter)};
        if(!candidate_range) {
            continue;
        }
        range = candidate_range;
        return &parameter;
    }
    range = nullptr;
    return nullptr;
}

inline void append_color_mix_priority_parameter(const fixture_mode &mode, std::vector<std::pair<std::string, double>> &parameters) {
    const fixture_parameter_range *range{nullptr};
    const fixture_parameter *parameter{color_mix_priority_parameter_for_mode(mode, range)};
    if(!parameter || !range) {
        return;
    }
    parameters.push_back({
        parameter->key,
        (double)color_range_center_value(*range) / (double)color_parameter_max_value(*parameter)
    });
}

inline void append_semantic_rgb_block_parameters(
    std::vector<std::pair<std::string, double>> &parameters,
    const std::string &red_parameter,
    const std::string &green_parameter,
    const std::string &blue_parameter,
    const std::string &white_parameter,
    const std::string &dimmer_parameter,
    const semantic_color_request &color,
    const semantic_color_options &options
) {
    const double intensity{!dimmer_parameter.empty() ? color_request_intensity(color) : 1.0};
    if(!white_parameter.empty() && options.use_white) {
        const double white{std::min(color.red, std::min(color.green, color.blue))};
        append_semantic_color_parameter(parameters, red_parameter, color_component_for_associated_dimmer(color.red - white, intensity));
        append_semantic_color_parameter(parameters, green_parameter, color_component_for_associated_dimmer(color.green - white, intensity));
        append_semantic_color_parameter(parameters, blue_parameter, color_component_for_associated_dimmer(color.blue - white, intensity));
        append_semantic_color_parameter(parameters, white_parameter, color_component_for_associated_dimmer(white, intensity));
    } else {
        append_semantic_color_parameter(parameters, red_parameter, color_component_for_associated_dimmer(color.red, intensity));
        append_semantic_color_parameter(parameters, green_parameter, color_component_for_associated_dimmer(color.green, intensity));
        append_semantic_color_parameter(parameters, blue_parameter, color_component_for_associated_dimmer(color.blue, intensity));
    }
    if(!dimmer_parameter.empty()) {
        append_semantic_color_parameter(parameters, dimmer_parameter, intensity);
    }
}

inline void append_semantic_cmy_block_parameters(
    std::vector<std::pair<std::string, double>> &parameters,
    const std::string &cyan_parameter,
    const std::string &magenta_parameter,
    const std::string &yellow_parameter,
    const std::string &dimmer_parameter,
    const semantic_color_request &color
) {
    const double intensity{!dimmer_parameter.empty() ? color_request_intensity(color) : 1.0};
    append_semantic_color_parameter(parameters, cyan_parameter, 1.0 - color_component_for_associated_dimmer(color.red, intensity));
    append_semantic_color_parameter(parameters, magenta_parameter, 1.0 - color_component_for_associated_dimmer(color.green, intensity));
    append_semantic_color_parameter(parameters, yellow_parameter, 1.0 - color_component_for_associated_dimmer(color.blue, intensity));
    if(!dimmer_parameter.empty()) {
        append_semantic_color_parameter(parameters, dimmer_parameter, intensity);
    }
}

inline semantic_color_mapping semantic_color_parameters_from_override(
    const fixture_semantic_mode_override &mode_override,
    const semantic_color_request &color,
    const semantic_color_options &options
) {
    std::vector<std::pair<std::string, double>> parameters{};
    for(const auto &block : mode_override.rgb_blocks) {
        append_semantic_rgb_block_parameters(
            parameters,
            block.red,
            block.green,
            block.blue,
            block.white,
            block.dimmer,
            color,
            options
        );
    }
    for(const auto &block : mode_override.cmy_blocks) {
        append_semantic_cmy_block_parameters(
            parameters,
            block.cyan,
            block.magenta,
            block.yellow,
            block.dimmer,
            color
        );
    }
    if(parameters.empty()) {
        return semantic_color_mapping::failure("semantic override has no color blocks");
    }
    return semantic_color_mapping::success(parameters);
}

inline double color_distance_squared(const semantic_color_request &left, const semantic_color_request &right) {
    const double red_delta{left.red - right.red};
    const double green_delta{left.green - right.green};
    const double blue_delta{left.blue - right.blue};
    return red_delta * red_delta + green_delta * green_delta + blue_delta * blue_delta;
}

inline bool semantic_color_is_nearly_white(const semantic_color_request &color) {
    constexpr double distance_epsilon{1.0e-12};
    return color_distance_squared(color, make_semantic_color_request(1.0, 1.0, 1.0)) <= distance_epsilon;
}

inline int current_parameter_distance(
    const std::vector<std::pair<std::string, int>> &current_parameter_values,
    const std::string &parameter_key,
    int range_center
) {
    for(const auto &current_parameter : current_parameter_values) {
        if(current_parameter.first == parameter_key) {
            return std::abs(current_parameter.second - range_center);
        }
    }
    return std::numeric_limits<int>::max();
}

inline bool known_color_name_to_rgb(const std::string &name, semantic_color_request &color) {
    const std::string normalized{normalized_color_key(name)};
    if(normalized.empty()) {
        return false;
    }

    if(normalized == "open" || normalized == "white" || normalized == "ctoopen" || normalized.find("white") != std::string::npos) {
        color = make_semantic_color_request(1.0, 1.0, 1.0);
        return true;
    }
    if(normalized.find("red") != std::string::npos) {
        color = make_semantic_color_request(1.0, 0.0, 0.0);
        return true;
    }
    if(normalized.find("green") != std::string::npos) {
        color = make_semantic_color_request(0.0, 1.0, 0.0);
        return true;
    }
    if(normalized.find("blue") != std::string::npos) {
        color = make_semantic_color_request(0.0, 0.0, 1.0);
        return true;
    }
    if(normalized.find("cyan") != std::string::npos) {
        color = make_semantic_color_request(0.0, 1.0, 1.0);
        return true;
    }
    if(normalized.find("magenta") != std::string::npos || normalized.find("pink") != std::string::npos) {
        color = make_semantic_color_request(1.0, 0.0, 1.0);
        return true;
    }
    if(normalized.find("yellow") != std::string::npos) {
        color = make_semantic_color_request(1.0, 1.0, 0.0);
        return true;
    }
    if(normalized.find("amber") != std::string::npos || normalized.find("orange") != std::string::npos) {
        color = make_semantic_color_request(1.0, 0.45, 0.0);
        return true;
    }
    if(normalized.find("lime") != std::string::npos) {
        color = make_semantic_color_request(0.45, 1.0, 0.0);
        return true;
    }
    if(normalized.find("purple") != std::string::npos || normalized.find("violet") != std::string::npos || normalized == "uv") {
        color = make_semantic_color_request(0.45, 0.0, 1.0);
        return true;
    }
    return false;
}

inline double gamma_encode_srgb(double value) {
    const double clamped{std::max(0.0, std::min(1.0, value))};
    if(clamped <= 0.0031308) {
        return 12.92 * clamped;
    }
    return 1.055 * std::pow(clamped, 1.0 / 2.4) - 0.055;
}

inline bool cie_xyY_to_rgb(const fixture_wheel_slot_color &slot_color, semantic_color_request &color) {
    if(!slot_color.has_cie_xyY || slot_color.cie_y <= 0.0) {
        return false;
    }

    const double luminance{slot_color.cie_Y > 1.0 ? slot_color.cie_Y / 100.0 : slot_color.cie_Y};
    const double Y{0.0 < luminance ? luminance : 1.0};
    const double X{(slot_color.cie_x * Y) / slot_color.cie_y};
    const double Z{((1.0 - slot_color.cie_x - slot_color.cie_y) * Y) / slot_color.cie_y};

    double linear_red{(3.2406 * X) + (-1.5372 * Y) + (-0.4986 * Z)};
    double linear_green{(-0.9689 * X) + (1.8758 * Y) + (0.0415 * Z)};
    double linear_blue{(0.0557 * X) + (-0.2040 * Y) + (1.0570 * Z)};

    const double maximum{std::max(linear_red, std::max(linear_green, linear_blue))};
    if(0.0 < maximum) {
        linear_red /= maximum;
        linear_green /= maximum;
        linear_blue /= maximum;
    }

    color = make_semantic_color_request(
        gamma_encode_srgb(linear_red),
        gamma_encode_srgb(linear_green),
        gamma_encode_srgb(linear_blue)
    );
    return true;
}

inline bool wheel_slot_to_rgb(const fixture_wheel_slot &slot, semantic_color_request &color) {
    if(slot.color.has_rgb) {
        color = make_semantic_color_request((double)slot.color.red / 255.0, (double)slot.color.green / 255.0, (double)slot.color.blue / 255.0);
        return true;
    }
    if(cie_xyY_to_rgb(slot.color, color)) {
        return true;
    }
    if(known_color_name_to_rgb(slot.id, color)) {
        return true;
    }
    if(known_color_name_to_rgb(slot.label, color)) {
        return true;
    }
    if(known_color_name_to_rgb(slot.kind, color)) {
        return true;
    }
    return false;
}

inline bool color_wheel_id_is_likely(const std::string &text) {
    const std::string normalized{normalized_color_key(text)};
    return normalized.find("colorwheel") != std::string::npos ||
        normalized.find("colourwheel") != std::string::npos;
}

inline bool parameter_is_likely_color_wheel(const fixture_parameter &parameter) {
    if(color_wheel_id_is_likely(parameter.key) || color_wheel_id_is_likely(parameter.wheel)) {
        return true;
    }
    for(const auto &range : parameter.ranges) {
        if(color_wheel_id_is_likely(range.wheel)) {
            return true;
        }
    }
    return false;
}

inline bool color_from_range_metadata(
    const fixture_profile *profile,
    const fixture_parameter &parameter,
    const fixture_parameter_range &range,
    semantic_color_request &slot_color
) {
    const std::string wheel_id{!range.wheel.empty() ? range.wheel : parameter.wheel};
    if(profile && !wheel_id.empty() && range.has_wheel_slot) {
        const fixture_wheel *wheel{profile->find_wheel(wheel_id)};
        if(wheel) {
            const fixture_wheel_slot *slot{wheel->find_slot(range.wheel_slot)};
            if(slot && wheel_slot_to_rgb(*slot, slot_color)) {
                return true;
            }
        }
    }
    if(known_color_name_to_rgb(range.function, slot_color)) {
        return true;
    }
    return known_color_name_to_rgb(range.label, slot_color);
}

inline semantic_color_mapping semantic_color_wheel_fallback_parameters_for_mode(
    const fixture_profile *profile,
    const fixture_mode &mode,
    const semantic_color_request &color,
    const std::vector<std::pair<std::string, int>> &current_parameter_values = {}
) {
    const double intensity{std::max(color.red, std::max(color.green, color.blue))};
    const bool request_is_black{intensity <= 1.0e-12};
    const semantic_color_request match_color{
        request_is_black ?
            make_semantic_color_request(1.0, 1.0, 1.0) :
            make_semantic_color_request(color.red / intensity, color.green / intensity, color.blue / intensity)
    };
    const bool request_uses_open_slot{semantic_color_is_nearly_white(match_color)};
    const fixture_parameter *best_parameter{nullptr};
    const fixture_parameter_range *best_range{nullptr};
    double best_distance{std::numeric_limits<double>::infinity()};
    int best_bias{0};
    int best_current_distance{std::numeric_limits<int>::max()};
    const fixture_parameter *best_open_parameter{nullptr};
    const fixture_parameter_range *best_open_range{nullptr};
    int best_open_value{std::numeric_limits<int>::max()};
    int best_open_bias{0};
    constexpr double distance_epsilon{1.0e-12};

    for(const auto &parameter : mode.parameters) {
        if(!parameter_is_likely_color_wheel(parameter)) {
            continue;
        }
        for(const auto &range : parameter.ranges) {
            semantic_color_request slot_color{};
            if(!color_from_range_metadata(profile, parameter, range, slot_color)) {
                continue;
            }
            int bias{0};
            if(range.has_wheel_slot) {
                bias += 10;
            }
            if(!range.wheel.empty() || !parameter.wheel.empty()) {
                bias += 10;
            }

            const int range_low{color_range_low_value(range)};
            if(request_uses_open_slot && semantic_color_is_nearly_white(slot_color)) {
                const bool better_open_value{!best_open_range || range_low < best_open_value};
                const bool same_open_value{best_open_range && range_low == best_open_value};
                const bool better_open_bias{same_open_value && best_open_bias < bias};
                if(better_open_value || better_open_bias) {
                    best_open_parameter = &parameter;
                    best_open_range = &range;
                    best_open_value = range_low;
                    best_open_bias = bias;
                }
            }

            const double distance{color_distance_squared(match_color, slot_color)};
            const int range_center{color_range_center_value(range)};
            const int current_distance{current_parameter_distance(current_parameter_values, parameter.key, range_center)};
            const bool better_distance{distance < best_distance - distance_epsilon};
            const bool same_distance{std::abs(distance - best_distance) <= distance_epsilon};
            const bool better_bias{same_distance && best_bias < bias};
            const bool same_bias{same_distance && best_bias == bias};
            const bool better_current_distance{same_bias && current_distance < best_current_distance};
            if(better_distance || better_bias || better_current_distance) {
                best_parameter = &parameter;
                best_range = &range;
                best_distance = distance;
                best_bias = bias;
                best_current_distance = current_distance;
            }
        }
    }

    const bool use_open_candidate{request_uses_open_slot && best_open_parameter && best_open_range};
    const fixture_parameter *selected_parameter{use_open_candidate ? best_open_parameter : best_parameter};
    const fixture_parameter_range *selected_range{use_open_candidate ? best_open_range : best_range};
    if(!selected_parameter || !selected_range) {
        return semantic_color_mapping::failure("fixture has no semantic color model");
    }

    const int selected_value{use_open_candidate ? best_open_value : color_range_center_value(*selected_range)};
    std::vector<std::pair<std::string, double>> parameters{
        {selected_parameter->key, (double)selected_value / (double)color_parameter_max_value(*selected_parameter)}
    };
    if(mode_has_semantic_parameter(mode, "dimmer")) {
        parameters.push_back({"dimmer", intensity});
    }
    return semantic_color_mapping::success(parameters);
}

inline semantic_color_mapping semantic_color_parameters_for_mode(
    const fixture_profile *profile,
    const fixture_mode &mode,
    const semantic_color_request &color,
    const semantic_color_options &options = {},
    const std::vector<std::pair<std::string, int>> &current_parameter_values = {},
    const fixture_semantic_mode_override *mode_override = nullptr
) {
    std::vector<std::pair<std::string, double>> parameters;
    const semantic_color_request clamped_color{make_semantic_color_request(color.red, color.green, color.blue)};

    if(mode_override && (!mode_override->rgb_blocks.empty() || !mode_override->cmy_blocks.empty())) {
        return semantic_color_parameters_from_override(*mode_override, clamped_color, options);
    }

    const std::vector<rgb_color_block> rgb_blocks{rgb_color_blocks_for_mode(mode)};
    if(!rgb_blocks.empty()) {
        for(const auto &block : rgb_blocks) {
            std::vector<std::string> block_parameters{block.red, block.green, block.blue};
            if(!block.white.empty()) {
                block_parameters.push_back(block.white);
            }
            const fixture_parameter *associated_dimmer{associated_color_dimmer_parameter(mode, block_parameters)};
            append_semantic_rgb_block_parameters(
                parameters,
                block.red,
                block.green,
                block.blue,
                block.white,
                associated_dimmer ? associated_dimmer->key : "",
                clamped_color,
                options
            );
        }
        append_color_mix_priority_parameter(mode, parameters);
        return semantic_color_mapping::success(parameters);
    }

    const std::vector<cmy_color_block> cmy_blocks{cmy_color_blocks_for_mode(mode)};
    if(!cmy_blocks.empty()) {
        for(const auto &block : cmy_blocks) {
            const std::vector<std::string> block_parameters{block.cyan, block.magenta, block.yellow};
            const fixture_parameter *associated_dimmer{associated_color_dimmer_parameter(mode, block_parameters)};
            append_semantic_cmy_block_parameters(
                parameters,
                block.cyan,
                block.magenta,
                block.yellow,
                associated_dimmer ? associated_dimmer->key : "",
                clamped_color
            );
        }
        append_color_mix_priority_parameter(mode, parameters);
        return semantic_color_mapping::success(parameters);
    }

    if(options.color_wheel_fallback) {
        return semantic_color_wheel_fallback_parameters_for_mode(profile, mode, clamped_color, current_parameter_values);
    }

    return semantic_color_mapping::failure("fixture has no semantic color model");
}

inline semantic_color_mapping semantic_color_parameters_for_mode(
    const fixture_mode &mode,
    const semantic_color_request &color,
    const semantic_color_options &options = {}
) {
    return semantic_color_parameters_for_mode(nullptr, mode, color, options);
}

} // namespace bbb::dmx
