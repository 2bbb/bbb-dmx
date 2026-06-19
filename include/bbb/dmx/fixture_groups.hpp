#pragma once

#include <cctype>
#include <cmath>
#include <iomanip>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "bbb/dmx/fixture_json.hpp"
#include "bbb/dmx/fixture_patch.hpp"
#include "bbb/dmx/json.hpp"

namespace bbb::dmx {

enum class fixture_group_entry_type {
    fixture,
    group,
};

struct fixture_group_entry {
public:
    fixture_group_entry_type type{fixture_group_entry_type::fixture};
    std::string id{};
};

struct fixture_group {
public:
    std::string id{};
    std::vector<fixture_group_entry> entries{};
};

struct fixture_group_set {
public:
    std::string schema{"bbb.dmx.groups.v1"};
    std::vector<fixture_group> groups{};

    const fixture_group *find_group(const std::string &group_id) const {
        for(const auto &group : groups) {
            if(group.id == group_id) {
                return &group;
            }
        }
        return nullptr;
    }
};

inline const fixture_instance *find_patch_fixture(const fixture_patch &patch, const std::string &fixture_id) {
    for(const auto &fixture : patch.fixtures) {
        if(fixture.id == fixture_id) {
            return &fixture;
        }
    }
    return nullptr;
}

inline bool fixture_group_json_integer_id(const json_value &value, std::string &id) {
    if(value.type != json_type::number || !std::isfinite(value.number_value) || std::floor(value.number_value) != value.number_value) {
        return false;
    }
    std::ostringstream stream{};
    stream << std::fixed << std::setprecision(0) << value.number_value;
    id = stream.str();
    return true;
}

inline bool fixture_group_json_required_integer(const json_value &object, const std::string &key, int &value, std::string &error) {
    const json_value *child{object.find(key)};
    if(!child) {
        error = "missing integer: " + key;
        return false;
    }
    if(child->type != json_type::number || !std::isfinite(child->number_value) || std::floor(child->number_value) != child->number_value) {
        error = "expected integer: " + key;
        return false;
    }
    value = (int)child->number_value;
    return true;
}

inline mapper_result fixture_group_format_pattern_id(const std::string &pattern, int value, std::string &fixture_id) {
    fixture_id.clear();
    int conversion_count{0};
    for(std::size_t index{0}; index < pattern.size(); index++) {
        const char character{pattern[index]};
        if(character != '%') {
            fixture_id.push_back(character);
            continue;
        }

        if(index + 1 >= pattern.size()) {
            return mapper_result::failure("fixture_pattern has unterminated % conversion");
        }
        index++;
        if(pattern[index] == '%') {
            fixture_id.push_back('%');
            continue;
        }

        conversion_count++;
        if(1 < conversion_count) {
            return mapper_result::failure("fixture_pattern must contain exactly one integer conversion");
        }

        bool pad_zero{false};
        if(pattern[index] == '0') {
            pad_zero = true;
            index++;
        }

        int width{0};
        while(index < pattern.size() && std::isdigit((unsigned char)pattern[index])) {
            width = width * 10 + (pattern[index] - '0');
            index++;
        }

        if(index >= pattern.size() || (pattern[index] != 'd' && pattern[index] != 'i')) {
            return mapper_result::failure("fixture_pattern supports only %d integer conversion");
        }

        const std::string number_text{std::to_string(value)};
        const bool has_sign{!number_text.empty() && number_text[0] == '-'};
        const std::size_t sign_count{has_sign ? 1U : 0U};
        const std::size_t total_count{number_text.size()};
        const std::size_t pad_count{(std::size_t)width > total_count ? (std::size_t)width - total_count : 0U};
        if(!pad_zero) {
            fixture_id.append(pad_count, ' ');
        }
        if(has_sign) {
            fixture_id.push_back('-');
        }
        if(pad_zero) {
            fixture_id.append(pad_count, '0');
        }
        fixture_id += number_text.substr(sign_count);
    }

    if(conversion_count != 1) {
        return mapper_result::failure("fixture_pattern must contain exactly one integer conversion");
    }
    if(fixture_id.empty()) {
        return mapper_result::failure("fixture_pattern generated empty fixture id");
    }
    return mapper_result::success();
}

inline mapper_result fixture_group_append_pattern_entries_from_json(
    const json_value &value,
    const std::string &group_id,
    std::vector<fixture_group_entry> &entries
) {
    if(value.object_value.size() != 3 || !value.find("fixture_pattern") || !value.find("start_index") || !value.find("num")) {
        return mapper_result::failure("fixture_pattern entry must contain only fixture_pattern, start_index, and num: " + group_id);
    }

    std::string error{};
    std::string fixture_pattern{};
    if(!json_string(value, "fixture_pattern", fixture_pattern, true, error)) {
        return mapper_result::failure(error + ": " + group_id);
    }
    if(fixture_pattern.empty()) {
        return mapper_result::failure("fixture_pattern is empty: " + group_id);
    }

    int start_index{0};
    if(!fixture_group_json_required_integer(value, "start_index", start_index, error)) {
        return mapper_result::failure(error + ": " + group_id);
    }

    int fixture_count{0};
    if(!fixture_group_json_required_integer(value, "num", fixture_count, error)) {
        return mapper_result::failure(error + ": " + group_id);
    }
    if(fixture_count < 0) {
        return mapper_result::failure("fixture_pattern num must be non-negative: " + group_id);
    }

    for(int offset{0}; offset < fixture_count; offset++) {
        std::string fixture_id{};
        mapper_result result{fixture_group_format_pattern_id(fixture_pattern, start_index + offset, fixture_id)};
        if(!result.ok) {
            return mapper_result::failure(result.message + ": " + group_id);
        }
        entries.push_back(fixture_group_entry{fixture_group_entry_type::fixture, fixture_id});
    }
    return mapper_result::success();
}

inline mapper_result fixture_group_append_entries_from_json(
    const json_value &value,
    const std::string &group_id,
    std::vector<fixture_group_entry> &entries
) {
    if(value.type == json_type::string) {
        if(value.string_value.empty()) {
            return mapper_result::failure("group fixture id is empty: " + group_id);
        }
        entries.push_back(fixture_group_entry{fixture_group_entry_type::fixture, value.string_value});
        return mapper_result::success();
    }

    std::string fixture_id{};
    if(fixture_group_json_integer_id(value, fixture_id)) {
        if(fixture_id.empty()) {
            return mapper_result::failure("group fixture id is empty: " + group_id);
        }
        entries.push_back(fixture_group_entry{fixture_group_entry_type::fixture, fixture_id});
        return mapper_result::success();
    }

    if(value.type == json_type::object) {
        if(value.find("fixture_pattern")) {
            return fixture_group_append_pattern_entries_from_json(value, group_id, entries);
        }
        if(value.object_value.size() != 1 || !value.find("group")) {
            return mapper_result::failure("group object entry must be either {\"group\":\"...\"} or {\"fixture_pattern\":\"...\",\"start_index\":N,\"num\":N}: " + group_id);
        }
        const json_value *referenced_group{value.find("group")};
        if(!referenced_group || referenced_group->type != json_type::string) {
            return mapper_result::failure("group reference id must be string: " + group_id);
        }
        if(referenced_group->string_value.empty()) {
            return mapper_result::failure("group reference id is empty: " + group_id);
        }
        entries.push_back(fixture_group_entry{fixture_group_entry_type::group, referenced_group->string_value});
        return mapper_result::success();
    }

    return mapper_result::failure("group entry must be fixture id string/integer, group reference object, or fixture_pattern object: " + group_id);
}

inline mapper_result fixture_groups_from_json(const json_value &root, fixture_group_set &group_set) {
    if(root.type != json_type::object) {
        return mapper_result::failure("groups root must be object");
    }

    fixture_group_set loaded{};
    std::string error{};
    if(!json_string(root, "schema", loaded.schema, true, error)) {
        return mapper_result::failure(error);
    }
    if(loaded.schema != "bbb.dmx.groups.v1") {
        return mapper_result::failure("unsupported groups schema: " + loaded.schema);
    }

    const json_value *groups_value{root.find("groups")};
    if(!groups_value || groups_value->type != json_type::object) {
        return mapper_result::failure("groups must be object");
    }

    for(const auto &entry : groups_value->object_value) {
        if(entry.first.empty()) {
            return mapper_result::failure("group id is empty");
        }
        if(entry.second.type != json_type::array) {
            return mapper_result::failure("group must be fixture id array: " + entry.first);
        }
        fixture_group group{};
        group.id = entry.first;
        for(const auto &fixture_value : entry.second.array_value) {
            mapper_result result{fixture_group_append_entries_from_json(fixture_value, entry.first, group.entries)};
            if(!result.ok) {
                return result;
            }
        }
        loaded.groups.push_back(group);
    }

    group_set = loaded;
    return mapper_result::success();
}

inline mapper_result parse_fixture_groups_text(const std::string &text, fixture_group_set &group_set) {
    const json_parse_result parsed{parse_json_text(text)};
    if(!parsed.ok) {
        return mapper_result::failure(parsed.message);
    }
    return fixture_groups_from_json(parsed.value, group_set);
}

inline mapper_result read_fixture_groups_file(const std::string &path, fixture_group_set &group_set) {
    std::string text{};
    mapper_result result{read_text_file(path, text)};
    if(!result.ok) {
        return result;
    }
    return parse_fixture_groups_text(text, group_set);
}

inline std::string fixture_group_stack_description(const std::vector<std::string> &group_stack, const std::string &next_group_id) {
    std::string text{};
    for(std::size_t index{0}; index < group_stack.size(); index++) {
        if(0 < index) {
            text += " -> ";
        }
        text += group_stack[index];
    }
    if(!text.empty()) {
        text += " -> ";
    }
    text += next_group_id;
    return text;
}

inline mapper_result append_resolved_fixture_group_entries(
    const fixture_group_set &group_set,
    const fixture_patch &patch,
    const std::string &group_id,
    std::set<std::string> &emitted_fixture_ids,
    std::vector<std::string> &fixture_ids,
    std::vector<std::string> &group_stack
) {
    for(const auto &active_group_id : group_stack) {
        if(active_group_id == group_id) {
            return mapper_result::failure("cyclic group reference: " + fixture_group_stack_description(group_stack, group_id));
        }
    }

    const fixture_group *group{group_set.find_group(group_id)};
    if(!group) {
        return mapper_result::failure("unknown group: " + group_id);
    }

    group_stack.push_back(group_id);
    for(const auto &entry : group->entries) {
        if(entry.type == fixture_group_entry_type::group) {
            mapper_result result{append_resolved_fixture_group_entries(group_set, patch, entry.id, emitted_fixture_ids, fixture_ids, group_stack)};
            if(!result.ok) {
                group_stack.pop_back();
                return result;
            }
            continue;
        }

        if(!find_patch_fixture(patch, entry.id)) {
            group_stack.pop_back();
            return mapper_result::failure("group " + group_id + " references unknown fixture: " + entry.id);
        }
        if(emitted_fixture_ids.find(entry.id) != emitted_fixture_ids.end()) {
            continue;
        }
        emitted_fixture_ids.insert(entry.id);
        fixture_ids.push_back(entry.id);
    }
    group_stack.pop_back();
    return mapper_result::success();
}

inline mapper_result validate_fixture_group_ids(const fixture_group_set &group_set) {
    std::set<std::string> group_ids{};
    for(const auto &group : group_set.groups) {
        if(group.id.empty()) {
            return mapper_result::failure("group id is empty");
        }
        if(group_ids.find(group.id) != group_ids.end()) {
            return mapper_result::failure("duplicate group id: " + group.id);
        }
        group_ids.insert(group.id);
    }
    return mapper_result::success();
}

inline mapper_result validate_fixture_groups_for_patch(const fixture_group_set &group_set, const fixture_patch &patch) {
    mapper_result result{validate_fixture_group_ids(group_set)};
    if(!result.ok) {
        return result;
    }

    for(const auto &group : group_set.groups) {
        std::set<std::string> emitted_fixture_ids{};
        std::vector<std::string> fixture_ids{};
        std::vector<std::string> group_stack{};
        result = append_resolved_fixture_group_entries(group_set, patch, group.id, emitted_fixture_ids, fixture_ids, group_stack);
        if(!result.ok) {
            return result;
        }
    }
    return mapper_result::success();
}

inline mapper_result resolve_fixture_group_fixture_ids(
    const fixture_group_set &group_set,
    const fixture_patch &patch,
    const std::string &group_id,
    std::vector<std::string> &fixture_ids
) {
    mapper_result result{validate_fixture_group_ids(group_set)};
    if(!result.ok) {
        return result;
    }

    std::set<std::string> emitted_fixture_ids{};
    std::vector<std::string> group_stack{};
    fixture_ids.clear();
    return append_resolved_fixture_group_entries(group_set, patch, group_id, emitted_fixture_ids, fixture_ids, group_stack);
}

} // namespace bbb::dmx
