#pragma once

#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "bbb/dmx/fixture_mapper.hpp"
#include "bbb/dmx/json.hpp"

namespace bbb::dmx {

inline bool json_vec3(const json_value &object, const std::string &key, vec3 &value, bool required, std::string &error) {
    const json_value *child{object.find(key)};
    if(!child) {
        if(required) {
            error = "missing vec3: " + key;
            return false;
        }
        return true;
    }
    if(child->type != json_type::array || child->array_value.size() < 3) {
        error = "expected 3-number array: " + key;
        return false;
    }
    for(std::size_t index = 0; index < 3; index++) {
        if(child->array_value[index].type != json_type::number) {
            error = "expected numeric vec3 element: " + key;
            return false;
        }
    }
    value = vec3{child->array_value[0].number_value, child->array_value[1].number_value, child->array_value[2].number_value};
    return true;
}

inline bool json_optional_double_present(const json_value &object, const std::string &key, double &value, bool &present, std::string &error) {
    const json_value *child{object.find(key)};
    if(!child) {
        present = false;
        return true;
    }
    if(child->type != json_type::number) {
        error = "expected number: " + key;
        return false;
    }
    value = child->number_value;
    present = true;
    return true;
}

inline bool json_string_or_integer_id(const json_value &object, const std::string &key, std::string &value, bool required, std::string &error) {
    const json_value *child{object.find(key)};
    if(!child) {
        if(required) {
            error = "missing string/integer id: " + key;
            return false;
        }
        return true;
    }
    if(child->type == json_type::string) {
        if(child->string_value.empty()) {
            error = "empty string id: " + key;
            return false;
        }
        value = child->string_value;
        return true;
    }
    if(child->type == json_type::number) {
        if(!std::isfinite(child->number_value) || std::floor(child->number_value) != child->number_value) {
            error = "expected string or integer id: " + key;
            return false;
        }
        std::ostringstream stream{};
        stream << std::fixed << std::setprecision(0) << child->number_value;
        value = stream.str();
        return true;
    }
    error = "expected string or integer id: " + key;
    return false;
}

inline bool parse_byte_order_string(const std::string &text, byte_order &order) {
    return byte_order_from_string(text, order);
}

inline bool parse_parameter_type(const std::string &text, fixture_parameter_type &type) {
    if(text == "u8") {
        type = fixture_parameter_type::u8;
        return true;
    }
    if(text == "u16") {
        type = fixture_parameter_type::u16;
        return true;
    }
    if(text == "u24") {
        type = fixture_parameter_type::u24;
        return true;
    }
    if(text == "enum") {
        type = fixture_parameter_type::enum_u8;
        return true;
    }
    return false;
}

inline mapper_result fixture_profile_from_json(const json_value &root, fixture_profile &profile) {
    if(root.type != json_type::object) {
        return mapper_result::failure("profile root must be object");
    }
    profile = fixture_profile{};
    std::string error{};
    if(!json_string(root, "key", profile.key, true, error)) {
        return mapper_result::failure(error);
    }
    json_string(root, "manufacturer", profile.manufacturer, false, error);
    json_string(root, "model", profile.model, false, error);

    const json_value *photometry{root.find("photometry")};
    if(photometry) {
        if(photometry->type != json_type::object) {
            return mapper_result::failure("profile photometry must be object");
        }
        if(!json_optional_double_present(*photometry, "beam_angle_degrees", profile.photometry.beam_angle_degrees, profile.photometry.has_beam_angle_degrees, error)) {
            return mapper_result::failure(error);
        }
        if(!json_optional_double_present(*photometry, "field_angle_degrees", profile.photometry.field_angle_degrees, profile.photometry.has_field_angle_degrees, error)) {
            return mapper_result::failure(error);
        }
        if(!json_optional_double_present(*photometry, "beam_radius", profile.photometry.beam_radius, profile.photometry.has_beam_radius, error)) {
            return mapper_result::failure(error);
        }
        if(!json_optional_double_present(*photometry, "luminous_flux", profile.photometry.luminous_flux, profile.photometry.has_luminous_flux, error)) {
            return mapper_result::failure(error);
        }
        if(!json_optional_double_present(*photometry, "color_temperature", profile.photometry.color_temperature, profile.photometry.has_color_temperature, error)) {
            return mapper_result::failure(error);
        }
    }

    const json_value *modes{root.find("modes")};
    if(!modes || modes->type != json_type::object) {
        return mapper_result::failure("profile modes must be object");
    }

    profile.modes.clear();
    for(const auto &mode_pair : modes->object_value) {
        if(mode_pair.second.type != json_type::object) {
            return mapper_result::failure("mode must be object: " + mode_pair.first);
        }
        fixture_mode mode{};
        mode.key = mode_pair.first;
        json_string(mode_pair.second, "label", mode.label, false, error);
        if(!json_int(mode_pair.second, "footprint", mode.footprint, true, error)) {
            return mapper_result::failure(error);
        }

        const json_value *channels{mode_pair.second.find("channels")};
        if(!channels || channels->type != json_type::array) {
            return mapper_result::failure("mode channels must be array: " + mode.key);
        }
        for(const auto &channel_value : channels->array_value) {
            if(channel_value.type != json_type::object) {
                return mapper_result::failure("channel must be object in mode: " + mode.key);
            }
            fixture_channel channel{};
            if(!json_int(channel_value, "offset", channel.offset, true, error)) {
                return mapper_result::failure(error);
            }
            if(!json_string(channel_value, "key", channel.key, true, error)) {
                return mapper_result::failure(error);
            }
            json_int(channel_value, "default", channel.default_value, false, error);
            json_string(channel_value, "label", channel.label, false, error);
            json_bool(channel_value, "hold", channel.hold, false, error);
            mode.channels.push_back(channel);
        }

        const json_value *parameters{mode_pair.second.find("parameters")};
        if(parameters && parameters->type == json_type::object) {
            for(const auto &parameter_pair : parameters->object_value) {
                if(parameter_pair.second.type != json_type::object) {
                    return mapper_result::failure("parameter must be object: " + parameter_pair.first);
                }
                fixture_parameter parameter{};
                parameter.key = parameter_pair.first;
                std::string type_text{"u8"};
                if(!json_string(parameter_pair.second, "type", type_text, true, error)) {
                    return mapper_result::failure(error);
                }
                if(!parse_parameter_type(type_text, parameter.type)) {
                    return mapper_result::failure("unknown parameter type: " + type_text);
                }
                json_int(parameter_pair.second, "default", parameter.default_value, false, error);
                json_double(parameter_pair.second, "range_degrees", parameter.range_degrees, false, error);
                std::string order_text{"coarsefine"};
                json_string(parameter_pair.second, "byte_order", order_text, false, error);
                if(!parse_byte_order_string(order_text, parameter.order)) {
                    return mapper_result::failure("unknown byte_order: " + order_text);
                }

                const json_value *ranges{parameter_pair.second.find("ranges")};
                if(ranges) {
                    if(ranges->type != json_type::array) {
                        return mapper_result::failure("parameter ranges must be array: " + parameter.key);
                    }
                    for(const auto &range_value : ranges->array_value) {
                        if(range_value.type != json_type::object) {
                            return mapper_result::failure("parameter range must be object: " + parameter.key);
                        }
                        fixture_parameter_range range{};
                        if(!json_int(range_value, "from", range.from, true, error)) {
                            return mapper_result::failure(error);
                        }
                        if(!json_int(range_value, "to", range.to, true, error)) {
                            return mapper_result::failure(error);
                        }
                        if(!json_string(range_value, "function", range.function, true, error)) {
                            return mapper_result::failure(error);
                        }
                        json_string(range_value, "label", range.label, false, error);
                        if(!json_optional_double_present(range_value, "physical_from", range.physical_from, range.has_physical_from, error)) {
                            return mapper_result::failure(error);
                        }
                        if(!json_optional_double_present(range_value, "physical_to", range.physical_to, range.has_physical_to, error)) {
                            return mapper_result::failure(error);
                        }
                        parameter.ranges.push_back(range);
                    }
                }

                const json_value *channel{parameter_pair.second.find("channel")};
                if(channel) {
                    if(channel->type != json_type::string) {
                        return mapper_result::failure("parameter channel must be string: " + parameter.key);
                    }
                    parameter.channels.push_back(channel->string_value);
                }
                const json_value *parameter_channels{parameter_pair.second.find("channels")};
                if(parameter_channels) {
                    if(parameter_channels->type != json_type::array) {
                        return mapper_result::failure("parameter channels must be array: " + parameter.key);
                    }
                    for(const auto &entry : parameter_channels->array_value) {
                        if(entry.type != json_type::string) {
                            return mapper_result::failure("parameter channel entry must be string: " + parameter.key);
                        }
                        parameter.channels.push_back(entry.string_value);
                    }
                }
                mode.parameters.push_back(parameter);
            }
        }
        profile.modes.push_back(mode);
    }
    return mapper_result::success();
}

inline mapper_result fixture_patch_from_json(const json_value &root, fixture_patch &patch) {
    if(root.type != json_type::object) {
        return mapper_result::failure("patch root must be object");
    }
    patch = fixture_patch{};

    std::string error{};
    if(!json_string(root, "schema", patch.schema, true, error)) {
        return mapper_result::failure(error);
    }
    if(patch.schema != "bbb.dmx.patch.v2") {
        return mapper_result::failure("unsupported patch schema: " + patch.schema);
    }
    if(!json_string(root, "coordinates", patch.coordinates, true, error)) {
        return mapper_result::failure(error);
    }
    if(patch.coordinates != "gdtf") {
        return mapper_result::failure("unsupported patch coordinates: " + patch.coordinates);
    }

    const json_value *profiles{root.find("profiles")};
    if(profiles) {
        if(profiles->type != json_type::array) {
            return mapper_result::failure("patch profiles must be array");
        }
        for(const auto &profile_path : profiles->array_value) {
            if(profile_path.type != json_type::string) {
                return mapper_result::failure("profile path must be string");
            }
            patch.profile_paths.push_back(profile_path.string_value);
        }
    }

    const json_value *fixtures{root.find("fixtures")};
    if(!fixtures || fixtures->type != json_type::array) {
        return mapper_result::failure("patch fixtures must be array");
    }

    for(const auto &fixture_value : fixtures->array_value) {
        if(fixture_value.type != json_type::object) {
            return mapper_result::failure("fixture must be object");
        }
        fixture_instance fixture{};
        if(!json_string_or_integer_id(fixture_value, "id", fixture.id, true, error)) {
            return mapper_result::failure(error);
        }
        if(!json_string(fixture_value, "profile", fixture.profile, true, error)) {
            return mapper_result::failure(error);
        }
        if(!json_string(fixture_value, "mode", fixture.mode, true, error)) {
            return mapper_result::failure(error);
        }
        if(!json_int(fixture_value, "universe", fixture.universe, true, error)) {
            return mapper_result::failure(error);
        }
        if(!json_int(fixture_value, "address", fixture.address, true, error)) {
            return mapper_result::failure(error);
        }
        json_vec3(fixture_value, "position", fixture.position, false, error);
        json_vec3(fixture_value, "rotation", fixture.rotation, false, error);
        const json_value *calibration{fixture_value.find("calibration")};
        if(calibration) {
            if(calibration->type != json_type::object) {
                return mapper_result::failure("calibration must be object: " + fixture.id);
            }
            json_double(*calibration, "pan_offset", fixture.calibration.pan_offset, false, error);
            json_double(*calibration, "tilt_offset", fixture.calibration.tilt_offset, false, error);
            json_bool(*calibration, "pan_invert", fixture.calibration.pan_invert, false, error);
            json_bool(*calibration, "tilt_invert", fixture.calibration.tilt_invert, false, error);
        }
        patch.fixtures.push_back(fixture);
    }
    return mapper_result::success();
}

inline mapper_result read_text_file(const std::string &path, std::string &text) {
    std::ifstream stream(path.c_str(), std::ios::in | std::ios::binary);
    if(!stream) {
        return mapper_result::failure("cannot open file: " + path);
    }
    std::ostringstream buffer{};
    buffer << stream.rdbuf();
    text = buffer.str();
    return mapper_result::success();
}

inline mapper_result parse_fixture_profile_text(const std::string &text, fixture_profile &profile) {
    const json_parse_result parsed{parse_json_text(text)};
    if(!parsed.ok) {
        return mapper_result::failure(parsed.message);
    }
    return fixture_profile_from_json(parsed.value, profile);
}

inline mapper_result parse_fixture_patch_text(const std::string &text, fixture_patch &patch) {
    const json_parse_result parsed{parse_json_text(text)};
    if(!parsed.ok) {
        return mapper_result::failure(parsed.message);
    }
    return fixture_patch_from_json(parsed.value, patch);
}

inline mapper_result read_fixture_profile_file(const std::string &path, fixture_profile &profile) {
    std::string text{};
    mapper_result result{read_text_file(path, text)};
    if(!result.ok) {
        return result;
    }
    return parse_fixture_profile_text(text, profile);
}

inline mapper_result read_fixture_patch_file(const std::string &path, fixture_patch &patch) {
    std::string text{};
    mapper_result result{read_text_file(path, text)};
    if(!result.ok) {
        return result;
    }
    return parse_fixture_patch_text(text, patch);
}

inline std::string parent_directory(const std::string &path) {
    const std::size_t slash_position{path.find_last_of("/\\")};
    if(slash_position == std::string::npos) {
        return "";
    }
    return path.substr(0, slash_position + 1);
}

inline bool path_is_absolute(const std::string &path) {
    if(path.empty()) {
        return false;
    }
    if(path[0] == '/' || path[0] == '\\') {
        return true;
    }
    return 1 < path.size() && path[1] == ':';
}

inline std::string join_relative_path(const std::string &base_directory, const std::string &path) {
    if(path_is_absolute(path) || base_directory.empty()) {
        return path;
    }
    return base_directory + path;
}

inline mapper_result load_fixture_mapper_from_patch_file(const std::string &patch_path, fixture_mapper &mapper) {
    fixture_patch patch{};
    mapper_result result{read_fixture_patch_file(patch_path, patch)};
    if(!result.ok) {
        return result;
    }

    fixture_mapper loaded_mapper{};
    const std::string base_directory{parent_directory(patch_path)};
    for(const auto &profile_path : patch.profile_paths) {
        fixture_profile profile{};
        result = read_fixture_profile_file(join_relative_path(base_directory, profile_path), profile);
        if(!result.ok) {
            return result;
        }
        result = loaded_mapper.add_profile(profile);
        if(!result.ok) {
            return result;
        }
    }
    result = loaded_mapper.set_patch(patch);
    if(!result.ok) {
        return result;
    }
    mapper = loaded_mapper;
    return mapper_result::success();
}

} // namespace bbb::dmx
