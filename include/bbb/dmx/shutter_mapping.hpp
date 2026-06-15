#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>

#include "bbb/dmx/fixture_profile.hpp"

namespace bbb::dmx {

struct semantic_shutter_mapping {
public:
    bool ok{false};
    std::string message{};
    std::string parameter{};
    int value{0};

    static semantic_shutter_mapping success(const std::string &parameter_key, int dmx_value) {
        return semantic_shutter_mapping{true, "", parameter_key, dmx_value};
    }

    static semantic_shutter_mapping failure(const std::string &message) {
        return semantic_shutter_mapping{false, message, "", 0};
    }
};

inline std::string normalized_semantic_key(const std::string &text) {
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

inline bool shutter_parameter_key_is_likely(const std::string &key) {
    const std::string normalized{normalized_semantic_key(key)};
    return normalized == "shutter" ||
        normalized == "shutterstrobe" ||
        normalized.find("shutter") != std::string::npos ||
        normalized.find("strobe") != std::string::npos;
}

inline bool shutter_range_matches_state(const fixture_parameter_range &range, bool open) {
    const std::string function{normalized_semantic_key(range.function)};
    const std::string label{normalized_semantic_key(range.label)};
    if(open) {
        return function == "open" || label == "open";
    }
    return function == "closed" ||
        function == "close" ||
        function == "blackout" ||
        label == "closed" ||
        label == "close" ||
        label == "blackout";
}

inline int range_center_value(const fixture_parameter_range &range) {
    const int minimum{std::min(range.from, range.to)};
    const int maximum{std::max(range.from, range.to)};
    return minimum + (maximum - minimum) / 2;
}

inline int parameter_max_value(const fixture_parameter &parameter) {
    if(parameter.type == fixture_parameter_type::u24) {
        return 16777215;
    }
    if(parameter.type == fixture_parameter_type::u16) {
        return 65535;
    }
    return 255;
}

inline int shutter_parameter_base_score(const fixture_parameter &parameter) {
    const std::string normalized{normalized_semantic_key(parameter.key)};
    if(normalized == "shutter") {
        return 100;
    }
    if(normalized == "shutterstrobe") {
        return 90;
    }
    if(normalized.find("shutter") != std::string::npos) {
        return 80;
    }
    if(normalized.find("strobe") != std::string::npos) {
        return 50;
    }
    return 0;
}

inline semantic_shutter_mapping semantic_shutter_parameter_for_mode(const fixture_mode &mode, bool open) {
    const fixture_parameter *best_parameter{nullptr};
    const fixture_parameter_range *best_range{nullptr};
    int best_score{-1};

    for(const auto &parameter : mode.parameters) {
        const int base_score{shutter_parameter_base_score(parameter)};
        if(base_score <= 0) {
            continue;
        }
        for(const auto &range : parameter.ranges) {
            if(!shutter_range_matches_state(range, open)) {
                continue;
            }
            const int score{1000 + base_score};
            if(best_score < score) {
                best_parameter = &parameter;
                best_range = &range;
                best_score = score;
            }
        }
    }

    if(best_parameter && best_range) {
        return semantic_shutter_mapping::success(best_parameter->key, range_center_value(*best_range));
    }

    for(const auto &parameter : mode.parameters) {
        if(!shutter_parameter_key_is_likely(parameter.key)) {
            continue;
        }
        const int score{shutter_parameter_base_score(parameter)};
        if(best_score < score) {
            best_parameter = &parameter;
            best_score = score;
        }
    }

    if(best_parameter) {
        return semantic_shutter_mapping::success(best_parameter->key, open ? parameter_max_value(*best_parameter) : 0);
    }

    return semantic_shutter_mapping::failure("fixture has no semantic shutter parameter");
}

} // namespace bbb::dmx
