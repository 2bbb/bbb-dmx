#pragma once

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

struct fixture_group {
public:
    std::string id{};
    std::vector<std::string> fixture_ids{};
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
            std::string fixture_id{};
            if(fixture_value.type == json_type::string) {
                fixture_id = fixture_value.string_value;
            } else if(fixture_value.type == json_type::number && std::isfinite(fixture_value.number_value) && std::floor(fixture_value.number_value) == fixture_value.number_value) {
                std::ostringstream stream{};
                stream << std::fixed << std::setprecision(0) << fixture_value.number_value;
                fixture_id = stream.str();
            } else {
                return mapper_result::failure("group fixture id must be string or integer: " + entry.first);
            }
            if(fixture_id.empty()) {
                return mapper_result::failure("group fixture id is empty: " + entry.first);
            }
            group.fixture_ids.push_back(fixture_id);
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

inline mapper_result validate_fixture_groups_for_patch(const fixture_group_set &group_set, const fixture_patch &patch) {
    std::set<std::string> group_ids{};
    for(const auto &group : group_set.groups) {
        if(group.id.empty()) {
            return mapper_result::failure("group id is empty");
        }
        if(group_ids.find(group.id) != group_ids.end()) {
            return mapper_result::failure("duplicate group id: " + group.id);
        }
        group_ids.insert(group.id);
        for(const auto &fixture_id : group.fixture_ids) {
            if(!find_patch_fixture(patch, fixture_id)) {
                return mapper_result::failure("group " + group.id + " references unknown fixture: " + fixture_id);
            }
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
    const fixture_group *group{group_set.find_group(group_id)};
    if(!group) {
        return mapper_result::failure("unknown group: " + group_id);
    }

    std::set<std::string> requested_ids{};
    for(const auto &fixture_id : group->fixture_ids) {
        if(!find_patch_fixture(patch, fixture_id)) {
            return mapper_result::failure("group " + group_id + " references unknown fixture: " + fixture_id);
        }
        requested_ids.insert(fixture_id);
    }

    fixture_ids.clear();
    fixture_ids.reserve(requested_ids.size());
    for(const auto &fixture : patch.fixtures) {
        if(requested_ids.find(fixture.id) != requested_ids.end()) {
            fixture_ids.push_back(fixture.id);
        }
    }
    return mapper_result::success();
}

} // namespace bbb::dmx
