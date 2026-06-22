#pragma once

#include <cmath>
#include <optional>
#include <set>
#include <string>

#include "bbb/dmx/fixture_json.hpp"
#include "bbb/dmx/json.hpp"

namespace bbb::dmx {

struct dmx_setup_values {
public:
    std::optional<std::string> config{};
    std::optional<std::string> patch{};
    std::optional<std::string> map{};
    std::optional<std::string> groups{};
    std::optional<std::string> semantic_overrides{};
    std::optional<int> universe{};
    std::optional<std::string> universe_mode{};
    std::optional<bool> autobang{};
    std::optional<std::string> tracking_mode{};
    std::optional<double> default_pan_range{};
    std::optional<double> default_tilt_range{};
    std::optional<bool> track_strict{};
    std::optional<bool> color_use_white{};
    std::optional<bool> color_wheel_fallback{};
    std::optional<std::string> plane_order{};
    std::optional<double> gamma{};
    std::optional<double> brightness{};
    std::optional<bool> invert_x{};
    std::optional<bool> invert_y{};
};

struct dmx_setup_document {
public:
    std::string schema{};
    dmx_setup_values common{};
    dmx_setup_values fixturemap{};
    dmx_setup_values matrixmap{};
    dmx_setup_values mask{};
};

inline bool setup_json_string_optional(const json_value &object, const std::string &key, std::optional<std::string> &value, std::string &error) {
    const json_value *child{object.find(key)};
    if(!child) {
        return true;
    }
    if(child->type != json_type::string) {
        error = "expected string: " + key;
        return false;
    }
    if(child->string_value.empty()) {
        error = "empty string: " + key;
        return false;
    }
    value = child->string_value;
    return true;
}

inline bool setup_json_integer_optional(const json_value &object, const std::string &key, std::optional<int> &value, std::string &error) {
    const json_value *child{object.find(key)};
    if(!child) {
        return true;
    }
    if(child->type != json_type::number || !std::isfinite(child->number_value) || std::floor(child->number_value) != child->number_value) {
        error = "expected integer: " + key;
        return false;
    }
    value = (int)child->number_value;
    return true;
}

inline bool setup_json_double_optional(const json_value &object, const std::string &key, std::optional<double> &value, std::string &error) {
    const json_value *child{object.find(key)};
    if(!child) {
        return true;
    }
    if(child->type != json_type::number || !std::isfinite(child->number_value)) {
        error = "expected number: " + key;
        return false;
    }
    value = child->number_value;
    return true;
}

inline bool setup_json_bool_optional(const json_value &object, const std::string &key, std::optional<bool> &value, std::string &error) {
    const json_value *child{object.find(key)};
    if(!child) {
        return true;
    }
    if(child->type != json_type::boolean && child->type != json_type::number) {
        error = "expected boolean or 0/1: " + key;
        return false;
    }
    if(child->type == json_type::boolean) {
        value = child->boolean_value;
        return true;
    }
    if(!std::isfinite(child->number_value)) {
        error = "expected finite boolean number: " + key;
        return false;
    }
    value = child->number_value != 0.0;
    return true;
}

inline mapper_result dmx_setup_values_from_json(const json_value &object, const std::string &context, dmx_setup_values &values) {
    if(object.type != json_type::object) {
        return mapper_result::failure("setup section must be object: " + context);
    }

    const std::set<std::string> allowed_keys{
        "config",
        "patch",
        "map",
        "groups",
        "group",
        "semantic_overrides",
        "universe",
        "universe_mode",
        "autobang",
        "tracking_mode",
        "default_pan_range",
        "default_tilt_range",
        "track_strict",
        "color_use_white",
        "color_wheel_fallback",
        "plane_order",
        "gamma",
        "brightness",
        "invert_x",
        "invert_y",
    };
    for(const auto &entry : object.object_value) {
        if(allowed_keys.find(entry.first) == allowed_keys.end()) {
            return mapper_result::failure("unknown setup key in " + context + ": " + entry.first);
        }
    }

    std::string error{};
    if(!setup_json_string_optional(object, "config", values.config, error)) {
        return mapper_result::failure(error + ": " + context);
    }
    if(!setup_json_string_optional(object, "patch", values.patch, error)) {
        return mapper_result::failure(error + ": " + context);
    }
    if(!setup_json_string_optional(object, "map", values.map, error)) {
        return mapper_result::failure(error + ": " + context);
    }
    if(!setup_json_string_optional(object, "groups", values.groups, error)) {
        return mapper_result::failure(error + ": " + context);
    }
    std::optional<std::string> group_alias{};
    if(!setup_json_string_optional(object, "group", group_alias, error)) {
        return mapper_result::failure(error + ": " + context);
    }
    if(group_alias.has_value()) {
        values.groups = group_alias.value();
    }
    if(!setup_json_string_optional(object, "semantic_overrides", values.semantic_overrides, error)) {
        return mapper_result::failure(error + ": " + context);
    }
    if(!setup_json_integer_optional(object, "universe", values.universe, error)) {
        return mapper_result::failure(error + ": " + context);
    }
    if(!setup_json_string_optional(object, "universe_mode", values.universe_mode, error)) {
        return mapper_result::failure(error + ": " + context);
    }
    if(!setup_json_bool_optional(object, "autobang", values.autobang, error)) {
        return mapper_result::failure(error + ": " + context);
    }
    if(!setup_json_string_optional(object, "tracking_mode", values.tracking_mode, error)) {
        return mapper_result::failure(error + ": " + context);
    }
    if(!setup_json_double_optional(object, "default_pan_range", values.default_pan_range, error)) {
        return mapper_result::failure(error + ": " + context);
    }
    if(!setup_json_double_optional(object, "default_tilt_range", values.default_tilt_range, error)) {
        return mapper_result::failure(error + ": " + context);
    }
    if(!setup_json_bool_optional(object, "track_strict", values.track_strict, error)) {
        return mapper_result::failure(error + ": " + context);
    }
    if(!setup_json_bool_optional(object, "color_use_white", values.color_use_white, error)) {
        return mapper_result::failure(error + ": " + context);
    }
    if(!setup_json_bool_optional(object, "color_wheel_fallback", values.color_wheel_fallback, error)) {
        return mapper_result::failure(error + ": " + context);
    }
    if(!setup_json_string_optional(object, "plane_order", values.plane_order, error)) {
        return mapper_result::failure(error + ": " + context);
    }
    if(!setup_json_double_optional(object, "gamma", values.gamma, error)) {
        return mapper_result::failure(error + ": " + context);
    }
    if(!setup_json_double_optional(object, "brightness", values.brightness, error)) {
        return mapper_result::failure(error + ": " + context);
    }
    if(!setup_json_bool_optional(object, "invert_x", values.invert_x, error)) {
        return mapper_result::failure(error + ": " + context);
    }
    if(!setup_json_bool_optional(object, "invert_y", values.invert_y, error)) {
        return mapper_result::failure(error + ": " + context);
    }
    return mapper_result::success();
}

template <typename value_type>
inline void merge_setup_optional(std::optional<value_type> &target, const std::optional<value_type> &source) {
    if(source.has_value()) {
        target = source.value();
    }
}

inline dmx_setup_values merge_setup_values(const dmx_setup_values &common, const dmx_setup_values &section) {
    dmx_setup_values merged{common};
    merge_setup_optional(merged.config, section.config);
    merge_setup_optional(merged.patch, section.patch);
    merge_setup_optional(merged.map, section.map);
    merge_setup_optional(merged.groups, section.groups);
    merge_setup_optional(merged.semantic_overrides, section.semantic_overrides);
    merge_setup_optional(merged.universe, section.universe);
    merge_setup_optional(merged.universe_mode, section.universe_mode);
    merge_setup_optional(merged.autobang, section.autobang);
    merge_setup_optional(merged.tracking_mode, section.tracking_mode);
    merge_setup_optional(merged.default_pan_range, section.default_pan_range);
    merge_setup_optional(merged.default_tilt_range, section.default_tilt_range);
    merge_setup_optional(merged.track_strict, section.track_strict);
    merge_setup_optional(merged.color_use_white, section.color_use_white);
    merge_setup_optional(merged.color_wheel_fallback, section.color_wheel_fallback);
    merge_setup_optional(merged.plane_order, section.plane_order);
    merge_setup_optional(merged.gamma, section.gamma);
    merge_setup_optional(merged.brightness, section.brightness);
    merge_setup_optional(merged.invert_x, section.invert_x);
    merge_setup_optional(merged.invert_y, section.invert_y);
    return merged;
}

inline mapper_result dmx_setup_from_json(const json_value &root, dmx_setup_document &setup) {
    if(root.type != json_type::object) {
        return mapper_result::failure("setup root must be object");
    }

    const std::set<std::string> top_level_keys{
        "schema",
        "config",
        "patch",
        "map",
        "groups",
        "group",
        "semantic_overrides",
        "universe",
        "universe_mode",
        "autobang",
        "tracking_mode",
        "default_pan_range",
        "default_tilt_range",
        "track_strict",
        "color_use_white",
        "color_wheel_fallback",
        "plane_order",
        "gamma",
        "brightness",
        "invert_x",
        "invert_y",
        "fixturemap",
        "matrixmap",
        "mask",
    };
    for(const auto &entry : root.object_value) {
        if(top_level_keys.find(entry.first) == top_level_keys.end()) {
            return mapper_result::failure("unknown setup key: " + entry.first);
        }
    }

    dmx_setup_document loaded{};
    std::string error{};
    if(!json_string(root, "schema", loaded.schema, true, error)) {
        return mapper_result::failure(error);
    }
    if(loaded.schema != "bbb.dmx.setup.v1") {
        return mapper_result::failure("unsupported setup schema: " + loaded.schema);
    }

    json_value common_object{root};
    common_object.object_value.erase("schema");
    common_object.object_value.erase("fixturemap");
    common_object.object_value.erase("matrixmap");
    common_object.object_value.erase("mask");
    mapper_result result{dmx_setup_values_from_json(common_object, "top-level", loaded.common)};
    if(!result.ok) {
        return result;
    }
    const json_value *fixturemap_section{root.find("fixturemap")};
    if(fixturemap_section) {
        result = dmx_setup_values_from_json(*fixturemap_section, "fixturemap", loaded.fixturemap);
        if(!result.ok) {
            return result;
        }
    }
    const json_value *matrixmap_section{root.find("matrixmap")};
    if(matrixmap_section) {
        result = dmx_setup_values_from_json(*matrixmap_section, "matrixmap", loaded.matrixmap);
        if(!result.ok) {
            return result;
        }
    }
    const json_value *mask_section{root.find("mask")};
    if(mask_section) {
        result = dmx_setup_values_from_json(*mask_section, "mask", loaded.mask);
        if(!result.ok) {
            return result;
        }
    }
    setup = loaded;
    return mapper_result::success();
}

inline mapper_result parse_dmx_setup_text(const std::string &text, dmx_setup_document &setup) {
    const json_parse_result parsed{parse_json_text(text)};
    if(!parsed.ok) {
        return mapper_result::failure(parsed.message);
    }
    return dmx_setup_from_json(parsed.value, setup);
}

inline mapper_result read_dmx_setup_file(const std::string &path, dmx_setup_document &setup) {
    std::string text{};
    mapper_result result{read_text_file(path, text)};
    if(!result.ok) {
        return result;
    }
    return parse_dmx_setup_text(text, setup);
}

} // namespace bbb::dmx
