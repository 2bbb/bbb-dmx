#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <vector>

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

struct semantic_shutter_mappings {
public:
    bool ok{false};
    std::string message{};
    std::vector<semantic_shutter_mapping> mappings{};

    static semantic_shutter_mappings success(const std::vector<semantic_shutter_mapping> &parameter_mappings) {
        return semantic_shutter_mappings{true, "", parameter_mappings};
    }

    static semantic_shutter_mappings failure(const std::string &message) {
        return semantic_shutter_mappings{false, message, {}};
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

inline bool normalized_contains_any(const std::string &text, const std::vector<std::string> &needles) {
    for(const auto &needle : needles) {
        if(text.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

inline std::string shutter_parameter_descriptor(const fixture_mode &mode, const fixture_parameter &parameter) {
    std::string descriptor{parameter.key};
    for(const auto &channel_key : parameter.channels) {
        descriptor += " ";
        descriptor += channel_key;
        const fixture_channel *channel{mode.find_channel(channel_key)};
        if(channel) {
            descriptor += " ";
            descriptor += channel->label;
        }
    }
    return normalized_semantic_key(descriptor);
}

inline bool shutter_parameter_key_is_likely(const std::string &key) {
    const std::string normalized{normalized_semantic_key(key)};
    return normalized == "shutter" ||
        normalized == "shutterstrobe" ||
        normalized.find("shutter") != std::string::npos ||
        normalized.find("strobe") != std::string::npos;
}

inline bool shutter_parameter_is_strobe_mode_like(const fixture_mode &mode, const fixture_parameter &parameter) {
    const std::string descriptor{shutter_parameter_descriptor(mode, parameter)};
    return normalized_contains_any(descriptor, {
        "strobemode",
        "shuttermode",
        "strobefx",
        "strobeeffect"
    });
}

inline bool shutter_parameter_is_rate_or_duration_like(const fixture_mode &mode, const fixture_parameter &parameter) {
    const std::string descriptor{shutter_parameter_descriptor(mode, parameter)};
    return normalized_contains_any(descriptor, {
        "strobeduration",
        "strobeperiod",
        "stroberate",
        "strobefrequency",
        "strobespeed"
    });
}

inline bool shutter_open_text_matches(const std::string &text) {
    return text == "open" ||
        text == "shutteropen" ||
        text == "shutteropened";
}

inline bool shutter_closed_text_matches(const std::string &text) {
    return text == "closed" ||
        text == "close" ||
        text == "blackout" ||
        text == "shutterclosed" ||
        text == "shutterclose" ||
        text == "shutterblackout";
}

inline bool shutter_range_matches_state(const fixture_parameter_range &range, bool open) {
    const std::string function{normalized_semantic_key(range.function)};
    const std::string label{normalized_semantic_key(range.label)};
    if(open) {
        return shutter_open_text_matches(function) || shutter_open_text_matches(label);
    }
    return shutter_closed_text_matches(function) || shutter_closed_text_matches(label);
}

inline bool shutter_range_is_no_effect(const fixture_parameter_range &range) {
    const std::string function{normalized_semantic_key(range.function)};
    const std::string label{normalized_semantic_key(range.label)};
    return function == "noeffect" ||
        function == "nofunction" ||
        function == "nofunktion" ||
        function == "none" ||
        label == "noeffect" ||
        label == "nofunction" ||
        label == "nofunktion" ||
        label == "none";
}

inline bool range_contains_value(const fixture_parameter_range &range, int value) {
    const int minimum{std::min(range.from, range.to)};
    const int maximum{std::max(range.from, range.to)};
    return minimum <= value && value <= maximum;
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

inline int parameter_clamped_default_value(const fixture_parameter &parameter) {
    return std::max(0, std::min(parameter_max_value(parameter), parameter.default_value));
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

inline bool dimmer_parameter_is_likely(const fixture_mode &mode, const fixture_parameter &parameter) {
    const std::string descriptor{shutter_parameter_descriptor(mode, parameter)};
    return normalized_contains_any(descriptor, {"dimmer", "intensity"});
}

inline semantic_shutter_mappings semantic_shutter_parameters_for_mode(const fixture_mode &mode, bool open);

inline int shutter_range_score(const fixture_parameter &parameter, const fixture_parameter_range &range) {
    const int minimum{std::min(range.from, range.to)};
    const int maximum{std::max(range.from, range.to)};
    const int width{maximum - minimum};
    int score{1000 - width};
    if(minimum == 0 && maximum == 0) {
        score += 200;
    }
    if(range_contains_value(range, parameter.default_value)) {
        score += 100;
    }
    return score;
}

inline const fixture_parameter_range *best_shutter_range_for_state(const fixture_parameter &parameter, bool open) {
    const fixture_parameter_range *best_range{nullptr};
    int best_score{-1};
    for(const auto &range : parameter.ranges) {
        if(!shutter_range_matches_state(range, open)) {
            continue;
        }
        const int score{shutter_range_score(parameter, range)};
        if(best_score < score) {
            best_range = &range;
            best_score = score;
        }
    }
    return best_range;
}

inline const fixture_parameter_range *best_no_effect_range(const fixture_parameter &parameter) {
    const fixture_parameter_range *best_range{nullptr};
    int best_score{-1};
    for(const auto &range : parameter.ranges) {
        if(!shutter_range_is_no_effect(range)) {
            continue;
        }
        const int score{shutter_range_score(parameter, range)};
        if(best_score < score) {
            best_range = &range;
            best_score = score;
        }
    }
    return best_range;
}

inline bool semantic_shutter_mapping_contains_parameter(
    const std::vector<semantic_shutter_mapping> &mappings,
    const std::string &parameter_key
) {
    for(const auto &mapping : mappings) {
        if(mapping.parameter == parameter_key) {
            return true;
        }
    }
    return false;
}

inline void append_semantic_shutter_mapping(
    std::vector<semantic_shutter_mapping> &mappings,
    const std::string &parameter_key,
    int value
) {
    if(semantic_shutter_mapping_contains_parameter(mappings, parameter_key)) {
        return;
    }
    mappings.push_back(semantic_shutter_mapping::success(parameter_key, value));
}

inline bool shutter_parameter_should_reset_to_no_effect(const fixture_mode &mode, const fixture_parameter &parameter) {
    if(shutter_parameter_is_strobe_mode_like(mode, parameter)) {
        return true;
    }
    return shutter_parameter_key_is_likely(parameter.key) &&
        !shutter_parameter_is_rate_or_duration_like(mode, parameter) &&
        best_no_effect_range(parameter) != nullptr;
}

inline semantic_shutter_mapping semantic_shutter_parameter_for_mode(const fixture_mode &mode, bool open) {
    const semantic_shutter_mappings mappings{semantic_shutter_parameters_for_mode(mode, open)};
    if(!mappings.ok) {
        return semantic_shutter_mapping::failure(mappings.message);
    }
    if(mappings.mappings.empty()) {
        return semantic_shutter_mapping::failure("fixture has no semantic shutter parameter");
    }
    return mappings.mappings.front();
}

inline semantic_shutter_mappings semantic_shutter_parameters_for_mode(const fixture_mode &mode, bool open) {
    const fixture_parameter *best_parameter{nullptr};
    int best_score{-1};
    std::vector<semantic_shutter_mapping> mappings{};
    bool has_explicit_shutter_state_mapping{false};

    for(const auto &parameter : mode.parameters) {
        const int base_score{shutter_parameter_base_score(parameter)};
        if(base_score <= 0) {
            continue;
        }
        const fixture_parameter_range *range{best_shutter_range_for_state(parameter, open)};
        if(range) {
            append_semantic_shutter_mapping(mappings, parameter.key, range_center_value(*range));
            has_explicit_shutter_state_mapping = true;
        }
    }

    for(const auto &parameter : mode.parameters) {
        if(!shutter_parameter_should_reset_to_no_effect(mode, parameter)) {
            continue;
        }
        const fixture_parameter_range *range{best_no_effect_range(parameter)};
        if(range) {
            append_semantic_shutter_mapping(mappings, parameter.key, range_center_value(*range));
        }
    }

    for(const auto &parameter : mode.parameters) {
        if(!shutter_parameter_is_rate_or_duration_like(mode, parameter)) {
            continue;
        }
        append_semantic_shutter_mapping(mappings, parameter.key, parameter_clamped_default_value(parameter));
    }

    if(!open && !has_explicit_shutter_state_mapping) {
        for(const auto &parameter : mode.parameters) {
            if(!dimmer_parameter_is_likely(mode, parameter)) {
                continue;
            }
            const fixture_parameter_range *range{best_shutter_range_for_state(parameter, false)};
            if(range) {
                append_semantic_shutter_mapping(mappings, parameter.key, range_center_value(*range));
            }
        }
    }

    if(!mappings.empty()) {
        return semantic_shutter_mappings::success(mappings);
    }

    for(const auto &parameter : mode.parameters) {
        if(!shutter_parameter_key_is_likely(parameter.key)) {
            continue;
        }
        if(open && shutter_parameter_is_rate_or_duration_like(mode, parameter)) {
            continue;
        }
        const int score{shutter_parameter_base_score(parameter)};
        if(best_score < score) {
            best_parameter = &parameter;
            best_score = score;
        }
    }

    if(best_parameter) {
        return semantic_shutter_mappings::success({
            semantic_shutter_mapping::success(best_parameter->key, open ? parameter_max_value(*best_parameter) : 0)
        });
    }

    return semantic_shutter_mappings::failure("fixture has no semantic shutter parameter");
}

} // namespace bbb::dmx
