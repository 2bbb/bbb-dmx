#pragma once

#include <cmath>

#include "bbb/dmx/math.hpp"

namespace bbb::dmx {

struct pan_tilt_degrees {
public:
    double pan{0.0};
    double tilt{0.0};
};

inline pan_tilt_degrees vector_to_pan_tilt(const vec3 &local_vector) {
    const double length{local_vector.length()};
    if(length < vector_epsilon) {
        return pan_tilt_degrees{};
    }

    const double horizontal_distance{std::sqrt(local_vector.x * local_vector.x + local_vector.y * local_vector.y)};
    const double pan_radians{horizontal_distance < vector_epsilon ? 0.0 : std::atan2(-local_vector.x, local_vector.y)};
    const double z_normalized{clamp(local_vector.z / length, -1.0, 1.0)};
    const double tilt_radians{std::acos(clamp(-z_normalized, -1.0, 1.0))};
    return pan_tilt_degrees{
        radians_to_degrees(pan_radians),
        radians_to_degrees(tilt_radians),
    };
}

} // namespace bbb::dmx
