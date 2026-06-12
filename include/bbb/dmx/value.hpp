#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <string>

#include "bbb/dmx/math.hpp"

namespace bbb::dmx {

enum class byte_order {
    coarse_fine,
    fine_coarse,
    coarse_mid_fine,
    fine_mid_coarse,
};

struct split_u16 {
public:
    std::uint8_t coarse{0};
    std::uint8_t fine{0};
};

struct split_u24 {
public:
    std::uint8_t coarse{0};
    std::uint8_t middle{0};
    std::uint8_t fine{0};
};

inline double clamp_normalized_value(double value) {
    if(value < 0.0) {
        return 0.0;
    }
    if(1.0 < value) {
        return 1.0;
    }
    return value;
}

inline std::uint8_t normalized_to_u8(double value) {
    return (std::uint8_t)std::round(clamp_normalized_value(value) * 255.0);
}

inline std::uint16_t normalized_to_u16(double value) {
    return (std::uint16_t)std::round(clamp_normalized_value(value) * 65535.0);
}

inline std::uint32_t normalized_to_u24(double value) {
    return (std::uint32_t)std::round(clamp_normalized_value(value) * 16777215.0);
}

inline std::uint16_t angle_to_u16(double degrees, double range_degrees) {
    const double sanitized_range{sanitize_positive_range(range_degrees)};
    const double clamped_degrees{clamp_angle_to_range(degrees, sanitized_range)};
    const double half_range{sanitized_range * 0.5};
    const double normalized{(clamped_degrees + half_range) / sanitized_range};
    int value{(int)std::round(normalized * 65535.0)};
    value = std::max(0, std::min(65535, value));
    return (std::uint16_t)value;
}

inline double u16_to_angle(std::uint16_t value, double range_degrees) {
    const double sanitized_range{sanitize_positive_range(range_degrees)};
    const double half_range{sanitized_range * 0.5};
    const double normalized{(double)value / 65535.0};
    return normalized * sanitized_range - half_range;
}

inline split_u16 split_16(std::uint16_t value) {
    return split_u16{
        (std::uint8_t)(value >> 8),
        (std::uint8_t)(value & 255),
    };
}

inline split_u24 split_24(std::uint32_t value) {
    value = std::min<std::uint32_t>(16777215, value);
    return split_u24{
        (std::uint8_t)((value >> 16) & 255),
        (std::uint8_t)((value >> 8) & 255),
        (std::uint8_t)(value & 255),
    };
}

inline std::uint16_t combine_16(std::uint8_t first, std::uint8_t second, byte_order order) {
    if(order == byte_order::fine_coarse) {
        return (std::uint16_t)(((std::uint16_t)second << 8) | first);
    }
    return (std::uint16_t)(((std::uint16_t)first << 8) | second);
}

inline std::uint32_t combine_24(std::uint8_t first, std::uint8_t second, std::uint8_t third, byte_order order) {
    if(order == byte_order::fine_mid_coarse) {
        return ((std::uint32_t)third << 16) | ((std::uint32_t)second << 8) | first;
    }
    return ((std::uint32_t)first << 16) | ((std::uint32_t)second << 8) | third;
}

inline std::array<int, 4> pan_tilt_to_bytes(std::uint16_t pan_value, std::uint16_t tilt_value, byte_order order) {
    const split_u16 pan{split_16(pan_value)};
    const split_u16 tilt{split_16(tilt_value)};

    if(order == byte_order::fine_coarse) {
        return {pan.fine, pan.coarse, tilt.fine, tilt.coarse};
    }
    return {pan.coarse, pan.fine, tilt.coarse, tilt.fine};
}

inline std::uint16_t neutral_u16() {
    return 32768;
}

inline bool byte_order_from_string(const std::string &text, byte_order &order) {
    if(text == "coarsefine") {
        order = byte_order::coarse_fine;
        return true;
    }
    if(text == "finecoarse") {
        order = byte_order::fine_coarse;
        return true;
    }
    if(text == "coarsemidfine" || text == "coarsemiddlefine" || text == "msbfirst") {
        order = byte_order::coarse_mid_fine;
        return true;
    }
    if(text == "finemidcoarse" || text == "finemiddlecoarse" || text == "lsbfirst") {
        order = byte_order::fine_mid_coarse;
        return true;
    }
    return false;
}

inline const char *byte_order_to_string(byte_order order) {
    if(order == byte_order::fine_coarse) {
        return "finecoarse";
    }
    if(order == byte_order::coarse_mid_fine) {
        return "coarsemidfine";
    }
    if(order == byte_order::fine_mid_coarse) {
        return "finemidcoarse";
    }
    return "coarsefine";
}

} // namespace bbb::dmx
