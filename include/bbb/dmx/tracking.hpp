#pragma once

#include <cmath>
#include <string>

namespace bbb::dmx {

enum class tracking_mode {
    off,
    pan,
    smart,
};

inline double choose_shortest_pan(double base_pan_degrees, double previous_pan_degrees) {
    const double turn_count{std::round((previous_pan_degrees - base_pan_degrees) / 360.0)};
    return base_pan_degrees + 360.0 * turn_count;
}

inline bool tracking_mode_from_string(const std::string &text, tracking_mode &mode) {
    if(text == "off") {
        mode = tracking_mode::off;
        return true;
    }
    if(text == "pan") {
        mode = tracking_mode::pan;
        return true;
    }
    if(text == "smart") {
        mode = tracking_mode::smart;
        return true;
    }
    return false;
}

inline const char *tracking_mode_to_string(tracking_mode mode) {
    if(mode == tracking_mode::off) {
        return "off";
    }
    if(mode == tracking_mode::pan) {
        return "pan";
    }
    return "smart";
}

} // namespace bbb::dmx
