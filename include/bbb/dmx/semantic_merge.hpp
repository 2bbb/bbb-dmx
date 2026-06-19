#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "bbb/dmx/color_mapping.hpp"
#include "bbb/dmx/fixture_mapper.hpp"
#include "bbb/dmx/semantic_overrides.hpp"
#include "bbb/dmx/value.hpp"

namespace bbb::dmx {

struct semantic_merge_channel {
public:
    int universe{1};
    int address{1};
};

struct semantic_merge_parameter_reference {
public:
    std::string key{};
    fixture_parameter_type type{fixture_parameter_type::u8};
    byte_order order{byte_order::coarse_fine};
    std::vector<semantic_merge_channel> channels{};
};

struct semantic_lowest_parameter_group {
public:
    std::string fixture_id{};
    semantic_merge_parameter_reference parameter{};
    semantic_merge_parameter_reference gate{};
    bool has_gate{false};
};

struct semantic_merge_policy {
public:
    std::vector<semantic_lowest_parameter_group> cmy_lowest_groups{};
};

inline std::size_t semantic_merge_required_channel_count(fixture_parameter_type type) {
    if(type == fixture_parameter_type::u16) {
        return 2;
    }
    if(type == fixture_parameter_type::u24) {
        return 3;
    }
    return 1;
}

inline mapper_result semantic_merge_parameter_reference_for_fixture(
    const fixture_mapper &mapper,
    const fixture_instance &fixture,
    const fixture_mode &mode,
    const std::string &parameter_key,
    semantic_merge_parameter_reference &reference
) {
    const fixture_parameter *parameter{mode.find_parameter(parameter_key)};
    if(!parameter) {
        return mapper_result::failure("semantic merge unknown parameter: " + fixture.id + ":" + parameter_key);
    }

    std::vector<std::pair<int, int>> addresses{};
    const mapper_result address_result{mapper.parameter_channel_addresses(fixture.id, parameter_key, addresses)};
    if(!address_result.ok) {
        return address_result;
    }

    const std::size_t required_channel_count{semantic_merge_required_channel_count(parameter->type)};
    if(addresses.size() < required_channel_count) {
        return mapper_result::failure("semantic merge parameter has too few channels: " + fixture.id + ":" + parameter_key);
    }

    reference = semantic_merge_parameter_reference{};
    reference.key = parameter_key;
    reference.type = parameter->type;
    reference.order = parameter->order;
    reference.channels.reserve(required_channel_count);
    for(std::size_t index{0}; index < required_channel_count; index++) {
        reference.channels.push_back(semantic_merge_channel{addresses[index].first, addresses[index].second});
    }
    return mapper_result::success();
}

inline std::uint32_t semantic_merge_parameter_raw_value(
    const semantic_merge_parameter_reference &reference,
    const std::vector<int> &values
) {
    if(reference.type == fixture_parameter_type::u16 && 2 <= values.size()) {
        return (std::uint32_t)combine_16((std::uint8_t)values[0], (std::uint8_t)values[1], reference.order);
    }
    if(reference.type == fixture_parameter_type::u24 && 3 <= values.size()) {
        return combine_24((std::uint8_t)values[0], (std::uint8_t)values[1], (std::uint8_t)values[2], reference.order);
    }
    if(values.empty()) {
        return 0;
    }
    return (std::uint32_t)clamp_byte(values[0]);
}

inline mapper_result semantic_merge_append_cmy_parameter_group(
    const fixture_mapper &mapper,
    const fixture_instance &fixture,
    const fixture_mode &mode,
    const std::string &parameter_key,
    const std::string &gate_parameter_key,
    semantic_merge_policy &policy
) {
    if(parameter_key.empty()) {
        return mapper_result::success();
    }

    semantic_lowest_parameter_group group{};
    group.fixture_id = fixture.id;
    mapper_result result{semantic_merge_parameter_reference_for_fixture(mapper, fixture, mode, parameter_key, group.parameter)};
    if(!result.ok) {
        return result;
    }
    if(!gate_parameter_key.empty()) {
        result = semantic_merge_parameter_reference_for_fixture(mapper, fixture, mode, gate_parameter_key, group.gate);
        if(!result.ok) {
            return result;
        }
        group.has_gate = true;
    }
    policy.cmy_lowest_groups.push_back(group);
    return mapper_result::success();
}

inline mapper_result build_semantic_cmy_lowest_merge_policy(
    const fixture_mapper &mapper,
    const fixture_semantic_overrides &overrides,
    semantic_merge_policy &policy
) {
    semantic_merge_policy built{};
    for(const auto &fixture : mapper.patch().fixtures) {
        const fixture_semantic_mode_override *mode_override{overrides.find_mode_override(fixture.profile, fixture.mode)};
        const fixture_profile *profile{mapper.find_profile(fixture.profile)};
        if(!profile) {
            return mapper_result::failure("semantic merge missing profile: " + fixture.profile);
        }
        const fixture_mode *mode{profile->find_mode(fixture.mode)};
        if(!mode) {
            return mapper_result::failure("semantic merge missing mode: " + fixture.profile + ":" + fixture.mode);
        }
        if(mode_override && !mode_override->cmy_blocks.empty()) {
            for(const auto &block : mode_override->cmy_blocks) {
                mapper_result result{semantic_merge_append_cmy_parameter_group(mapper, fixture, *mode, block.cyan, block.dimmer, built)};
                if(!result.ok) {
                    return result;
                }
                result = semantic_merge_append_cmy_parameter_group(mapper, fixture, *mode, block.magenta, block.dimmer, built);
                if(!result.ok) {
                    return result;
                }
                result = semantic_merge_append_cmy_parameter_group(mapper, fixture, *mode, block.yellow, block.dimmer, built);
                if(!result.ok) {
                    return result;
                }
            }
            continue;
        }

        const std::vector<cmy_color_block> cmy_blocks{cmy_color_blocks_for_mode(*mode)};
        for(const auto &block : cmy_blocks) {
            const std::vector<std::string> block_parameters{block.cyan, block.magenta, block.yellow};
            const fixture_parameter *associated_dimmer{associated_color_dimmer_parameter(*mode, block_parameters)};
            const std::string gate_parameter_key{associated_dimmer ? associated_dimmer->key : ""};
            mapper_result result{semantic_merge_append_cmy_parameter_group(mapper, fixture, *mode, block.cyan, gate_parameter_key, built)};
            if(!result.ok) {
                return result;
            }
            result = semantic_merge_append_cmy_parameter_group(mapper, fixture, *mode, block.magenta, gate_parameter_key, built);
            if(!result.ok) {
                return result;
            }
            result = semantic_merge_append_cmy_parameter_group(mapper, fixture, *mode, block.yellow, gate_parameter_key, built);
            if(!result.ok) {
                return result;
            }
        }
    }
    policy = built;
    return mapper_result::success();
}

} // namespace bbb::dmx
