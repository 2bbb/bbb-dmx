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

inline bool mode_has_semantic_parameter(const fixture_mode &mode, const char *parameter) {
    return mode.find_parameter(parameter) != nullptr;
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

inline int color_parameter_max_value(const fixture_parameter &parameter) {
    if(parameter.type == fixture_parameter_type::u24) {
        return 16777215;
    }
    if(parameter.type == fixture_parameter_type::u16) {
        return 65535;
    }
    return 255;
}

inline int color_range_center_value(const fixture_parameter_range &range) {
    const int minimum{std::min(range.from, range.to)};
    const int maximum{std::max(range.from, range.to)};
    return minimum + (maximum - minimum) / 2;
}

inline double color_distance_squared(const semantic_color_request &left, const semantic_color_request &right) {
    const double red_delta{left.red - right.red};
    const double green_delta{left.green - right.green};
    const double blue_delta{left.blue - right.blue};
    return red_delta * red_delta + green_delta * green_delta + blue_delta * blue_delta;
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

inline bool parameter_is_likely_color_wheel(const fixture_parameter &parameter) {
    const std::string normalized{normalized_color_key(parameter.key)};
    if(normalized.find("colorwheel") != std::string::npos || normalized.find("colourwheel") != std::string::npos) {
        return true;
    }
    if(!parameter.wheel.empty()) {
        return true;
    }
    for(const auto &range : parameter.ranges) {
        if(range.has_wheel_slot || !range.wheel.empty()) {
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
    const fixture_parameter *best_parameter{nullptr};
    const fixture_parameter_range *best_range{nullptr};
    double best_distance{std::numeric_limits<double>::infinity()};
    int best_bias{0};
    int best_current_distance{std::numeric_limits<int>::max()};
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

    if(!best_parameter || !best_range) {
        return semantic_color_mapping::failure("fixture has no semantic color model");
    }

    std::vector<std::pair<std::string, double>> parameters{
        {best_parameter->key, (double)color_range_center_value(*best_range) / (double)color_parameter_max_value(*best_parameter)}
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
    const std::vector<std::pair<std::string, int>> &current_parameter_values = {}
) {
    std::vector<std::pair<std::string, double>> parameters;
    const semantic_color_request clamped_color{make_semantic_color_request(color.red, color.green, color.blue)};

    if(
        mode_has_semantic_parameter(mode, "red") &&
        mode_has_semantic_parameter(mode, "green") &&
        mode_has_semantic_parameter(mode, "blue")
    ) {
        const bool has_white{mode_has_semantic_parameter(mode, "white")};
        if(has_white && options.use_white) {
            const double white{std::min(clamped_color.red, std::min(clamped_color.green, clamped_color.blue))};
            parameters.push_back({"red", clamped_color.red - white});
            parameters.push_back({"green", clamped_color.green - white});
            parameters.push_back({"blue", clamped_color.blue - white});
            parameters.push_back({"white", white});
        } else {
            parameters.push_back({"red", clamped_color.red});
            parameters.push_back({"green", clamped_color.green});
            parameters.push_back({"blue", clamped_color.blue});
        }
        return semantic_color_mapping::success(parameters);
    }

    if(
        mode_has_semantic_parameter(mode, "cyan") &&
        mode_has_semantic_parameter(mode, "magenta") &&
        mode_has_semantic_parameter(mode, "yellow")
    ) {
        parameters.push_back({"cyan", 1.0 - clamped_color.red});
        parameters.push_back({"magenta", 1.0 - clamped_color.green});
        parameters.push_back({"yellow", 1.0 - clamped_color.blue});
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
