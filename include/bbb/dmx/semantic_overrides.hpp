#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "bbb/dmx/fixture_json.hpp"
#include "bbb/dmx/fixture_profile.hpp"
#include "bbb/dmx/json.hpp"

namespace bbb::dmx {

struct semantic_rgb_override {
public:
    std::string red{};
    std::string green{};
    std::string blue{};
    std::string white{};
    std::string dimmer{};
};

struct semantic_cmy_override {
public:
    std::string cyan{};
    std::string magenta{};
    std::string yellow{};
    std::string dimmer{};
};

struct fixture_semantic_mode_override {
public:
    std::map<std::string, std::string> aliases{};
    std::vector<std::string> intensity_parameters{};
    std::string primary_intensity_parameter{};
    std::vector<semantic_rgb_override> rgb_blocks{};
    std::vector<semantic_cmy_override> cmy_blocks{};

    std::string resolve_alias(const std::string &parameter_key) const {
        const auto found = aliases.find(parameter_key);
        if(found == aliases.end()) {
            return parameter_key;
        }
        return found->second;
    }
};

struct fixture_semantic_profile_override {
public:
    std::map<std::string, fixture_semantic_mode_override> modes{};
};

struct fixture_semantic_overrides {
public:
    std::string schema{"bbb.dmx.semantic_overrides.v1"};
    std::map<std::string, fixture_semantic_profile_override> profiles{};

    const fixture_semantic_mode_override *find_mode_override(const std::string &profile_key, const std::string &mode_key) const {
        const auto profile_found = profiles.find(profile_key);
        if(profile_found == profiles.end()) {
            return nullptr;
        }
        const auto mode_found = profile_found->second.modes.find(mode_key);
        if(mode_found == profile_found->second.modes.end()) {
            return nullptr;
        }
        return &mode_found->second;
    }
};

inline mapper_result semantic_overrides_string_array(
    const json_value &value,
    std::vector<std::string> &strings,
    const std::string &context
) {
    if(value.type != json_type::array) {
        return mapper_result::failure(context + " must be an array");
    }
    for(const auto &entry : value.array_value) {
        if(entry.type != json_type::string || entry.string_value.empty()) {
            return mapper_result::failure(context + " entries must be non-empty strings");
        }
        strings.push_back(entry.string_value);
    }
    return mapper_result::success();
}

inline mapper_result semantic_overrides_parse_aliases(
    const json_value &value,
    fixture_semantic_mode_override &mode_override,
    const std::string &context
) {
    if(value.type != json_type::object) {
        return mapper_result::failure(context + " aliases must be an object");
    }
    for(const auto &entry : value.object_value) {
        if(entry.first.empty()) {
            return mapper_result::failure(context + " alias key is empty");
        }
        if(entry.second.type != json_type::string || entry.second.string_value.empty()) {
            return mapper_result::failure(context + " alias target must be a non-empty string: " + entry.first);
        }
        mode_override.aliases[entry.first] = entry.second.string_value;
    }
    return mapper_result::success();
}

inline mapper_result semantic_overrides_parse_rgb_block(
    const json_value &value,
    semantic_rgb_override &block,
    const std::string &context
) {
    if(value.type != json_type::object) {
        return mapper_result::failure(context + " RGB block must be an object");
    }
    std::string error{};
    if(!json_string(value, "red", block.red, true, error) ||
       !json_string(value, "green", block.green, true, error) ||
       !json_string(value, "blue", block.blue, true, error) ||
       !json_string(value, "white", block.white, false, error) ||
       !json_string(value, "dimmer", block.dimmer, false, error)) {
        return mapper_result::failure(context + " " + error);
    }
    return mapper_result::success();
}

inline mapper_result semantic_overrides_parse_cmy_block(
    const json_value &value,
    semantic_cmy_override &block,
    const std::string &context
) {
    if(value.type != json_type::object) {
        return mapper_result::failure(context + " CMY block must be an object");
    }
    std::string error{};
    if(!json_string(value, "cyan", block.cyan, true, error) ||
       !json_string(value, "magenta", block.magenta, true, error) ||
       !json_string(value, "yellow", block.yellow, true, error) ||
       !json_string(value, "dimmer", block.dimmer, false, error)) {
        return mapper_result::failure(context + " " + error);
    }
    return mapper_result::success();
}

inline mapper_result semantic_overrides_parse_rgb_blocks(
    const json_value &value,
    fixture_semantic_mode_override &mode_override,
    const std::string &context
) {
    if(value.type != json_type::array) {
        return mapper_result::failure(context + " rgb must be an array");
    }
    for(const auto &entry : value.array_value) {
        semantic_rgb_override block{};
        mapper_result result{semantic_overrides_parse_rgb_block(entry, block, context)};
        if(!result.ok) {
            return result;
        }
        mode_override.rgb_blocks.push_back(block);
    }
    return mapper_result::success();
}

inline mapper_result semantic_overrides_parse_cmy_blocks(
    const json_value &value,
    fixture_semantic_mode_override &mode_override,
    const std::string &context
) {
    if(value.type != json_type::array) {
        return mapper_result::failure(context + " cmy must be an array");
    }
    for(const auto &entry : value.array_value) {
        semantic_cmy_override block{};
        mapper_result result{semantic_overrides_parse_cmy_block(entry, block, context)};
        if(!result.ok) {
            return result;
        }
        mode_override.cmy_blocks.push_back(block);
    }
    return mapper_result::success();
}

inline mapper_result semantic_overrides_parse_intensity(
    const json_value &value,
    fixture_semantic_mode_override &mode_override,
    const std::string &context
) {
    if(value.type == json_type::array) {
        return semantic_overrides_string_array(value, mode_override.intensity_parameters, context + " intensity");
    }
    if(value.type != json_type::object) {
        return mapper_result::failure(context + " intensity must be an array or object");
    }
    if(const json_value *parameters_value = value.find("parameters")) {
        mapper_result result{semantic_overrides_string_array(*parameters_value, mode_override.intensity_parameters, context + " intensity.parameters")};
        if(!result.ok) {
            return result;
        }
    }
    std::string error{};
    if(!json_string(value, "primary", mode_override.primary_intensity_parameter, false, error)) {
        return mapper_result::failure(context + " intensity " + error);
    }
    return mapper_result::success();
}

inline mapper_result semantic_overrides_parse_color(
    const json_value &value,
    fixture_semantic_mode_override &mode_override,
    const std::string &context
) {
    if(value.type != json_type::object) {
        return mapper_result::failure(context + " color must be an object");
    }
    if(const json_value *rgb_value = value.find("rgb")) {
        mapper_result result{semantic_overrides_parse_rgb_blocks(*rgb_value, mode_override, context + " color")};
        if(!result.ok) {
            return result;
        }
    }
    if(const json_value *cmy_value = value.find("cmy")) {
        mapper_result result{semantic_overrides_parse_cmy_blocks(*cmy_value, mode_override, context + " color")};
        if(!result.ok) {
            return result;
        }
    }
    return mapper_result::success();
}

inline mapper_result semantic_overrides_parse_mode(
    const json_value &value,
    fixture_semantic_mode_override &mode_override,
    const std::string &context
) {
    if(value.type != json_type::object) {
        return mapper_result::failure(context + " mode override must be an object");
    }
    if(const json_value *aliases_value = value.find("aliases")) {
        mapper_result result{semantic_overrides_parse_aliases(*aliases_value, mode_override, context)};
        if(!result.ok) {
            return result;
        }
    }
    if(const json_value *intensity_value = value.find("intensity")) {
        mapper_result result{semantic_overrides_parse_intensity(*intensity_value, mode_override, context)};
        if(!result.ok) {
            return result;
        }
    }
    if(const json_value *color_value = value.find("color")) {
        mapper_result result{semantic_overrides_parse_color(*color_value, mode_override, context)};
        if(!result.ok) {
            return result;
        }
    }
    if(const json_value *rgb_value = value.find("rgb")) {
        mapper_result result{semantic_overrides_parse_rgb_blocks(*rgb_value, mode_override, context)};
        if(!result.ok) {
            return result;
        }
    }
    if(const json_value *cmy_value = value.find("cmy")) {
        mapper_result result{semantic_overrides_parse_cmy_blocks(*cmy_value, mode_override, context)};
        if(!result.ok) {
            return result;
        }
    }
    return mapper_result::success();
}

inline mapper_result fixture_semantic_overrides_from_json(
    const json_value &root,
    fixture_semantic_overrides &overrides
) {
    if(root.type != json_type::object) {
        return mapper_result::failure("semantic_overrides root must be object");
    }

    fixture_semantic_overrides loaded{};
    std::string error{};
    if(!json_string(root, "schema", loaded.schema, true, error)) {
        return mapper_result::failure(error);
    }
    if(loaded.schema != "bbb.dmx.semantic_overrides.v1") {
        return mapper_result::failure("unsupported semantic_overrides schema: " + loaded.schema);
    }

    const json_value *profiles_value{root.find("profiles")};
    if(!profiles_value || profiles_value->type != json_type::object) {
        return mapper_result::failure("semantic_overrides profiles must be object");
    }

    for(const auto &profile_entry : profiles_value->object_value) {
        if(profile_entry.first.empty()) {
            return mapper_result::failure("semantic_overrides profile key is empty");
        }
        if(profile_entry.second.type != json_type::object) {
            return mapper_result::failure("semantic_overrides profile must be object: " + profile_entry.first);
        }
        const json_value *modes_value{profile_entry.second.find("modes")};
        if(!modes_value || modes_value->type != json_type::object) {
            return mapper_result::failure("semantic_overrides profile modes must be object: " + profile_entry.first);
        }
        fixture_semantic_profile_override profile_override{};
        for(const auto &mode_entry : modes_value->object_value) {
            if(mode_entry.first.empty()) {
                return mapper_result::failure("semantic_overrides mode key is empty: " + profile_entry.first);
            }
            fixture_semantic_mode_override mode_override{};
            mapper_result result{semantic_overrides_parse_mode(
                mode_entry.second,
                mode_override,
                "semantic_overrides " + profile_entry.first + ":" + mode_entry.first
            )};
            if(!result.ok) {
                return result;
            }
            profile_override.modes[mode_entry.first] = mode_override;
        }
        loaded.profiles[profile_entry.first] = profile_override;
    }

    overrides = loaded;
    return mapper_result::success();
}

inline mapper_result parse_fixture_semantic_overrides_text(
    const std::string &text,
    fixture_semantic_overrides &overrides
) {
    const json_parse_result parsed{parse_json_text(text)};
    if(!parsed.ok) {
        return mapper_result::failure(parsed.message);
    }
    return fixture_semantic_overrides_from_json(parsed.value, overrides);
}

inline mapper_result read_fixture_semantic_overrides_file(
    const std::string &path,
    fixture_semantic_overrides &overrides
) {
    std::string text{};
    mapper_result result{read_text_file(path, text)};
    if(!result.ok) {
        return result;
    }
    return parse_fixture_semantic_overrides_text(text, overrides);
}

} // namespace bbb::dmx
