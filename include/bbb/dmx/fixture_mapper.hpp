#pragma once

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "bbb/dmx/fixture_patch.hpp"
#include "bbb/dmx/fixture_profile.hpp"
#include "bbb/dmx/universe.hpp"

namespace bbb::dmx {

struct mapper_result {
public:
    bool ok{false};
    std::string message{};

    static mapper_result success() {
        return mapper_result{true, ""};
    }

    static mapper_result failure(const std::string &message) {
        return mapper_result{false, message};
    }
};

class fixture_mapper {
public:
    using universe_map = std::map<int, dmx_universe>;

    void clear() {
        profiles_.clear();
        patch_ = fixture_patch{};
        universes_.clear();
        validated_ = false;
    }

    mapper_result add_profile(const fixture_profile &profile) {
        if(profile.key.empty()) {
            return mapper_result::failure("profile key is empty");
        }
        profiles_[profile.key] = profile;
        validated_ = false;
        return mapper_result::success();
    }

    mapper_result set_patch(const fixture_patch &patch) {
        patch_ = patch;
        validated_ = false;
        return validate_and_reset();
    }

    mapper_result validate_and_reset() {
        std::set<std::string> fixture_ids;
        std::map<int, std::set<int>> occupied_channels;

        for(const auto &fixture : patch_.fixtures) {
            if(fixture.id.empty()) {
                return mapper_result::failure("fixture id is empty");
            }
            if(fixture_ids.find(fixture.id) != fixture_ids.end()) {
                return mapper_result::failure("duplicate fixture id: " + fixture.id);
            }
            fixture_ids.insert(fixture.id);

            const fixture_profile *profile{find_profile(fixture.profile)};
            if(!profile) {
                return mapper_result::failure("missing profile: " + fixture.profile);
            }

            const fixture_mode *mode{profile->find_mode(fixture.mode)};
            if(!mode) {
                return mapper_result::failure("missing mode: " + fixture.profile + ":" + fixture.mode);
            }
            if(mode->footprint <= 0) {
                return mapper_result::failure("invalid footprint for mode: " + fixture.mode);
            }
            if(fixture.address < 1 || universe_channel_count < fixture.address) {
                return mapper_result::failure("fixture address outside 1..512: " + fixture.id);
            }
            if(universe_channel_count < fixture.address + mode->footprint - 1) {
                return mapper_result::failure("fixture footprint exceeds universe: " + fixture.id);
            }

            for(const auto &channel : mode->channels) {
                if(channel.offset < 1 || mode->footprint < channel.offset) {
                    return mapper_result::failure("channel offset outside fixture footprint: " + fixture.id + ":" + channel.key);
                }
                const int absolute_address{fixture.address + channel.offset - 1};
                auto &occupied_for_universe = occupied_channels[fixture.universe];
                if(occupied_for_universe.find(absolute_address) != occupied_for_universe.end()) {
                    return mapper_result::failure("overlapping DMX channel at universe " + std::to_string(fixture.universe) + " channel " + std::to_string(absolute_address));
                }
                occupied_for_universe.insert(absolute_address);
            }
        }

        reset_universes();
        validated_ = true;
        return mapper_result::success();
    }

    mapper_result reset_universes() {
        universes_.clear();
        for(const auto &fixture : patch_.fixtures) {
            const fixture_profile *profile{find_profile(fixture.profile)};
            if(!profile) {
                return mapper_result::failure("missing profile: " + fixture.profile);
            }
            const fixture_mode *mode{profile->find_mode(fixture.mode)};
            if(!mode) {
                return mapper_result::failure("missing mode: " + fixture.mode);
            }
            dmx_universe &universe = universes_[fixture.universe];
            for(const auto &channel : mode->channels) {
                const int address{fixture.address + channel.offset - 1};
                const write_result result{universe.set_channel(address, channel.default_value)};
                if(!result.ok) {
                    return mapper_result::failure(result.message);
                }
            }
            for(const auto &parameter : mode->parameters) {
                mapper_result result{write_parameter_default(fixture, *mode, parameter)};
                if(!result.ok) {
                    return result;
                }
            }
        }
        return mapper_result::success();
    }

    mapper_result set_u8(const std::string &fixture_id, const std::string &parameter_key, int value) {
        const resolved_parameter resolved{resolve_parameter(fixture_id, parameter_key)};
        if(!resolved.ok) {
            return mapper_result::failure(resolved.message);
        }
        if(resolved.parameter->type != fixture_parameter_type::u8 && resolved.parameter->type != fixture_parameter_type::enum_u8) {
            return mapper_result::failure("parameter is not u8: " + parameter_key);
        }
        if(resolved.parameter->channels.empty()) {
            return mapper_result::failure("parameter has no channel: " + parameter_key);
        }
        const fixture_channel *channel{resolved.mode->find_channel(resolved.parameter->channels[0])};
        if(!channel) {
            return mapper_result::failure("parameter channel missing: " + parameter_key);
        }
        return write_u8(*resolved.fixture, *channel, value);
    }

    mapper_result set_u16(const std::string &fixture_id, const std::string &parameter_key, std::uint16_t value) {
        const resolved_parameter resolved{resolve_parameter(fixture_id, parameter_key)};
        if(!resolved.ok) {
            return mapper_result::failure(resolved.message);
        }
        if(resolved.parameter->type != fixture_parameter_type::u16) {
            return mapper_result::failure("parameter is not u16: " + parameter_key);
        }
        if(resolved.parameter->channels.size() < 2) {
            return mapper_result::failure("u16 parameter needs two channels: " + parameter_key);
        }
        const fixture_channel *first_channel{resolved.mode->find_channel(resolved.parameter->channels[0])};
        if(!first_channel) {
            return mapper_result::failure("parameter first channel missing: " + parameter_key);
        }
        return write_u16(*resolved.fixture, *first_channel, value, resolved.parameter->order);
    }

    mapper_result set_u24(const std::string &fixture_id, const std::string &parameter_key, std::uint32_t value) {
        const resolved_parameter resolved{resolve_parameter(fixture_id, parameter_key)};
        if(!resolved.ok) {
            return mapper_result::failure(resolved.message);
        }
        if(resolved.parameter->type != fixture_parameter_type::u24) {
            return mapper_result::failure("parameter is not u24: " + parameter_key);
        }
        if(resolved.parameter->channels.size() < 3) {
            return mapper_result::failure("u24 parameter needs three channels: " + parameter_key);
        }
        const fixture_channel *first_channel{resolved.mode->find_channel(resolved.parameter->channels[0])};
        if(!first_channel) {
            return mapper_result::failure("parameter first channel missing: " + parameter_key);
        }
        return write_u24(*resolved.fixture, *first_channel, value, resolved.parameter->order);
    }

    mapper_result set_normalized(const std::string &fixture_id, const std::string &parameter_key, double value) {
        const resolved_parameter resolved{resolve_parameter(fixture_id, parameter_key)};
        if(!resolved.ok) {
            return mapper_result::failure(resolved.message);
        }
        if(resolved.parameter->type == fixture_parameter_type::u16) {
            return set_u16(fixture_id, parameter_key, normalized_to_u16(value));
        }
        if(resolved.parameter->type == fixture_parameter_type::u24) {
            return set_u24(fixture_id, parameter_key, normalized_to_u24(value));
        }
        const int int_value{(int)normalized_to_u8(value)};
        return set_u8(fixture_id, parameter_key, int_value);
    }

    mapper_result current_raw_value(const std::string &fixture_id, const std::string &parameter_key, int &value) const {
        const resolved_parameter resolved{resolve_parameter(fixture_id, parameter_key)};
        if(!resolved.ok) {
            return mapper_result::failure(resolved.message);
        }
        if(resolved.parameter->channels.empty()) {
            return mapper_result::failure("parameter has no channel: " + parameter_key);
        }
        const fixture_channel *first_channel{resolved.mode->find_channel(resolved.parameter->channels[0])};
        if(!first_channel) {
            return mapper_result::failure("parameter first channel missing: " + parameter_key);
        }
        const dmx_universe &target_universe{universe(resolved.fixture->universe)};
        const int first_address{resolved.fixture->address + first_channel->offset - 1};
        if(resolved.parameter->type == fixture_parameter_type::u16) {
            if(resolved.parameter->channels.size() < 2) {
                return mapper_result::failure("u16 parameter needs two channels: " + parameter_key);
            }
            const fixture_channel *second_channel{resolved.mode->find_channel(resolved.parameter->channels[1])};
            if(!second_channel) {
                return mapper_result::failure("parameter second channel missing: " + parameter_key);
            }
            const int second_address{resolved.fixture->address + second_channel->offset - 1};
            value = (int)combine_16(
                (std::uint8_t)target_universe.channel(first_address),
                (std::uint8_t)target_universe.channel(second_address),
                resolved.parameter->order
            );
            return mapper_result::success();
        }
        if(resolved.parameter->type == fixture_parameter_type::u24) {
            if(resolved.parameter->channels.size() < 3) {
                return mapper_result::failure("u24 parameter needs three channels: " + parameter_key);
            }
            const fixture_channel *second_channel{resolved.mode->find_channel(resolved.parameter->channels[1])};
            const fixture_channel *third_channel{resolved.mode->find_channel(resolved.parameter->channels[2])};
            if(!second_channel || !third_channel) {
                return mapper_result::failure("parameter u24 channel missing: " + parameter_key);
            }
            const int second_address{resolved.fixture->address + second_channel->offset - 1};
            const int third_address{resolved.fixture->address + third_channel->offset - 1};
            value = (int)combine_24(
                (std::uint8_t)target_universe.channel(first_address),
                (std::uint8_t)target_universe.channel(second_address),
                (std::uint8_t)target_universe.channel(third_address),
                resolved.parameter->order
            );
            return mapper_result::success();
        }
        value = target_universe.channel(first_address);
        return mapper_result::success();
    }

    mapper_result parameter_channel_addresses(
        const std::string &fixture_id,
        const std::string &parameter_key,
        std::vector<std::pair<int, int>> &addresses
    ) const
    {
        addresses.clear();
        const resolved_parameter resolved{resolve_parameter(fixture_id, parameter_key)};
        if(!resolved.ok) {
            return mapper_result::failure(resolved.message);
        }
        std::size_t required_channel_count{1};
        if(resolved.parameter->type == fixture_parameter_type::u16) {
            required_channel_count = 2;
        } else if(resolved.parameter->type == fixture_parameter_type::u24) {
            required_channel_count = 3;
        }
        if(resolved.parameter->channels.size() < required_channel_count) {
            return mapper_result::failure("parameter has too few channels: " + parameter_key);
        }
        addresses.reserve(required_channel_count);
        for(std::size_t index{0}; index < required_channel_count; index++) {
            const fixture_channel *channel{resolved.mode->find_channel(resolved.parameter->channels[index])};
            if(!channel) {
                return mapper_result::failure("parameter channel missing: " + parameter_key);
            }
            addresses.push_back({resolved.fixture->universe, resolved.fixture->address + channel->offset - 1});
        }
        return mapper_result::success();
    }

    mapper_result set_pan_tilt_bytes(const std::string &fixture_id, int pan_1, int pan_2, int tilt_1, int tilt_2) {
        const resolved_parameter pan{resolve_parameter(fixture_id, "pan")};
        if(!pan.ok) {
            return mapper_result::failure(pan.message);
        }
        const resolved_parameter tilt{resolve_parameter(fixture_id, "tilt")};
        if(!tilt.ok) {
            return mapper_result::failure(tilt.message);
        }
        mapper_result result{set_u8_bytes(*pan.fixture, *pan.mode, *pan.parameter, pan_1, pan_2)};
        if(!result.ok) {
            return result;
        }
        return set_u8_bytes(*tilt.fixture, *tilt.mode, *tilt.parameter, tilt_1, tilt_2);
    }

    mapper_result set_channel(int universe_id, int address, int value) {
        const write_result result{universes_[universe_id].set_channel(address, value)};
        if(!result.ok) {
            return mapper_result::failure(result.message);
        }
        return mapper_result::success();
    }

    const dmx_universe &universe(int universe_id) const {
        static const dmx_universe empty_universe{};
        const auto found = universes_.find(universe_id);
        if(found == universes_.end()) {
            return empty_universe;
        }
        return found->second;
    }

    universe_map universe_snapshot() const {
        return universes_;
    }

    void restore_universes(const universe_map &universes) {
        universes_ = universes;
    }

    std::vector<int> universe_ids() const {
        std::vector<int> ids;
        ids.reserve(universes_.size());
        for(const auto &entry : universes_) {
            ids.push_back(entry.first);
        }
        return ids;
    }

    bool validated() const {
        return validated_;
    }

    const fixture_patch &patch() const {
        return patch_;
    }

    const fixture_profile *find_profile(const std::string &profile_key) const {
        const auto found = profiles_.find(profile_key);
        if(found == profiles_.end()) {
            return nullptr;
        }
        return &found->second;
    }

private:
    struct resolved_parameter {
    public:
        bool ok{false};
        std::string message{};
        const fixture_instance *fixture{nullptr};
        const fixture_profile *profile{nullptr};
        const fixture_mode *mode{nullptr};
        const fixture_parameter *parameter{nullptr};
    };

    const fixture_instance *find_fixture(const std::string &fixture_id) const {
        for(const auto &fixture : patch_.fixtures) {
            if(fixture.id == fixture_id) {
                return &fixture;
            }
        }
        return nullptr;
    }

    resolved_parameter resolve_parameter(const std::string &fixture_id, const std::string &parameter_key) const {
        const fixture_instance *fixture{find_fixture(fixture_id)};
        if(!fixture) {
            return resolved_parameter{false, "unknown fixture: " + fixture_id};
        }
        const fixture_profile *profile{find_profile(fixture->profile)};
        if(!profile) {
            return resolved_parameter{false, "missing profile: " + fixture->profile};
        }
        const fixture_mode *mode{profile->find_mode(fixture->mode)};
        if(!mode) {
            return resolved_parameter{false, "missing mode: " + fixture->mode};
        }
        const fixture_parameter *parameter{mode->find_parameter(parameter_key)};
        if(!parameter) {
            return resolved_parameter{false, "unknown parameter: " + parameter_key};
        }
        return resolved_parameter{true, "", fixture, profile, mode, parameter};
    }

    mapper_result write_u8(const fixture_instance &fixture, const fixture_channel &channel, int value) {
        dmx_universe &universe = universes_[fixture.universe];
        const int address{fixture.address + channel.offset - 1};
        const write_result result{universe.set_channel(address, value)};
        if(!result.ok) {
            return mapper_result::failure(result.message);
        }
        return mapper_result::success();
    }

    mapper_result write_u16(const fixture_instance &fixture, const fixture_channel &first_channel, std::uint16_t value, byte_order order) {
        dmx_universe &universe = universes_[fixture.universe];
        const int address{fixture.address + first_channel.offset - 1};
        const write_result result{universe.set_u16(address, value, order)};
        if(!result.ok) {
            return mapper_result::failure(result.message);
        }
        return mapper_result::success();
    }

    mapper_result write_u24(const fixture_instance &fixture, const fixture_channel &first_channel, std::uint32_t value, byte_order order) {
        dmx_universe &universe = universes_[fixture.universe];
        const int address{fixture.address + first_channel.offset - 1};
        const write_result result{universe.set_u24(address, value, order)};
        if(!result.ok) {
            return mapper_result::failure(result.message);
        }
        return mapper_result::success();
    }

    mapper_result set_u8_bytes(const fixture_instance &fixture, const fixture_mode &mode, const fixture_parameter &parameter, int first_value, int second_value) {
        if(parameter.channels.size() < 2) {
            return mapper_result::failure("u16 byte parameter needs two channels: " + parameter.key);
        }
        const fixture_channel *first_channel{mode.find_channel(parameter.channels[0])};
        const fixture_channel *second_channel{mode.find_channel(parameter.channels[1])};
        if(!first_channel || !second_channel) {
            return mapper_result::failure("u16 byte parameter channel missing: " + parameter.key);
        }
        mapper_result result{write_u8(fixture, *first_channel, first_value)};
        if(!result.ok) {
            return result;
        }
        return write_u8(fixture, *second_channel, second_value);
    }

    mapper_result write_parameter_default(const fixture_instance &fixture, const fixture_mode &mode, const fixture_parameter &parameter) {
        if(parameter.type == fixture_parameter_type::u16) {
            return set_u16(fixture.id, parameter.key, (std::uint16_t)std::max(0, std::min(65535, parameter.default_value)));
        }
        if(parameter.type == fixture_parameter_type::u24) {
            return set_u24(fixture.id, parameter.key, (std::uint32_t)std::max(0, std::min(16777215, parameter.default_value)));
        }
        return set_u8(fixture.id, parameter.key, parameter.default_value);
    }

    std::map<std::string, fixture_profile> profiles_{};
    fixture_patch patch_{};
    std::map<int, dmx_universe> universes_{};
    bool validated_{false};
};

} // namespace bbb::dmx
