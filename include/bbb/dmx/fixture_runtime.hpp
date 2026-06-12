#pragma once

#include <map>
#include <set>
#include <string>

#include "bbb/dmx/fixture_json.hpp"

namespace bbb::dmx {

using fixture_parameter_values = std::map<std::string, double>;

inline mapper_result parse_fixture_parameter_values(
    const json_value &value,
    fixture_parameter_values &parameters,
    const std::string &context_label
) {
    if(value.type != json_type::object) {
        return mapper_result::failure("parameter map must be object");
    }
    parameters.clear();
    for(const auto &entry : value.object_value) {
        if(entry.second.type != json_type::number) {
            return mapper_result::failure(context_label + " parameter must be numeric: " + entry.first);
        }
        parameters[entry.first] = entry.second.number_value;
    }
    return mapper_result::success();
}

inline std::set<int> fixture_universe_ids(const fixture_mapper &mapper) {
    std::set<int> universes{};
    for(const auto &fixture : mapper.patch().fixtures) {
        universes.insert(fixture.universe);
    }
    return universes;
}

} // namespace bbb::dmx
