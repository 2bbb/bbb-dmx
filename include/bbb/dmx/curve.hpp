#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "bbb/dmx/universe.hpp"
#include "bbb/dmx/value.hpp"

namespace bbb::dmx {

enum class dmx_curve_type {
    linear,
    gamma,
    invert,
    threshold,
    points,
};

struct dmx_curve_point {
public:
    double x{0.0};
    double y{0.0};
};

struct dmx_curve_rule {
public:
    int universe{0}; // 0 = all universes
    int start{1};
    int count{universe_channel_count};
    dmx_curve_type type{dmx_curve_type::linear};
    double gamma{1.0};
    double threshold{0.5};
    std::vector<dmx_curve_point> points{};
};

inline bool curve_type_from_string(const std::string &text, dmx_curve_type &type) {
    if(text == "linear") {
        type = dmx_curve_type::linear;
        return true;
    }
    if(text == "gamma") {
        type = dmx_curve_type::gamma;
        return true;
    }
    if(text == "invert") {
        type = dmx_curve_type::invert;
        return true;
    }
    if(text == "threshold") {
        type = dmx_curve_type::threshold;
        return true;
    }
    if(text == "points") {
        type = dmx_curve_type::points;
        return true;
    }
    return false;
}

inline double interpolate_curve_points(std::vector<dmx_curve_point> points, double x) {
    if(points.empty()) {
        return x;
    }
    for(auto &point : points) {
        point.x = clamp_normalized_value(point.x);
        point.y = clamp_normalized_value(point.y);
    }
    std::sort(points.begin(), points.end(), [](const dmx_curve_point &left, const dmx_curve_point &right) {
        return left.x < right.x;
    });
    x = clamp_normalized_value(x);
    if(x <= points.front().x) {
        return points.front().y;
    }
    if(points.back().x <= x) {
        return points.back().y;
    }
    for(std::size_t index = 1; index < points.size(); index++) {
        if(x <= points[index].x) {
            const dmx_curve_point &left = points[index - 1];
            const dmx_curve_point &right = points[index];
            const double width{std::max(1.0e-9, right.x - left.x)};
            const double mix{(x - left.x) / width};
            return left.y + (right.y - left.y) * mix;
        }
    }
    return x;
}

inline int apply_curve_value(int value, const dmx_curve_rule &rule) {
    const double input{clamp_normalized_value((double)std::max(0, std::min(255, value)) / 255.0)};
    double output{input};
    switch(rule.type) {
        case dmx_curve_type::gamma:
            output = std::pow(input, std::max(0.01, rule.gamma));
            break;
        case dmx_curve_type::invert:
            output = 1.0 - input;
            break;
        case dmx_curve_type::threshold:
            output = input < clamp_normalized_value(rule.threshold) ? 0.0 : 1.0;
            break;
        case dmx_curve_type::points:
            output = interpolate_curve_points(rule.points, input);
            break;
        case dmx_curve_type::linear:
        default:
            output = input;
            break;
    }
    return (int)normalized_to_u8(output);
}

inline bool curve_rule_matches(const dmx_curve_rule &rule, int universe_id, int address) {
    if(rule.universe != 0 && rule.universe != universe_id) {
        return false;
    }
    const int start{std::max(1, rule.start)};
    const int count{std::max(0, rule.count)};
    return start <= address && address < start + count;
}

inline dmx_universe apply_curve_rules(const dmx_universe &input, int universe_id, const std::vector<dmx_curve_rule> &rules) {
    dmx_universe output{input};
    for(int address = 1; address <= universe_channel_count; address++) {
        int value{input.channel(address)};
        for(const auto &rule : rules) {
            if(curve_rule_matches(rule, universe_id, address)) {
                value = apply_curve_value(value, rule);
            }
        }
        output.set_channel(address, value);
    }
    return output;
}

} // namespace bbb::dmx
