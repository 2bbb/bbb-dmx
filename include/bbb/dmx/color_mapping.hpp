#pragma once

#include <algorithm>
#include <cmath>
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

inline semantic_color_mapping semantic_color_parameters_for_mode(
    const fixture_mode &mode,
    const semantic_color_request &color,
    const semantic_color_options &options = {}
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

    return semantic_color_mapping::failure("fixture has no semantic color model");
}

} // namespace bbb::dmx
