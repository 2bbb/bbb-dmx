#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "bbb/dmx/universe.hpp"

namespace bbb::dmx {

inline int sanitize_universe_id(int universe_id) {
    return universe_id < 1 ? 1 : universe_id;
}

struct dmx_frame_set {
public:
    void clear() {
        universes.clear();
    }

    dmx_universe &ensure_universe(int universe_id) {
        return universes[sanitize_universe_id(universe_id)];
    }

    const dmx_universe &universe(int universe_id) const {
        static const dmx_universe empty_universe{};
        const auto found = universes.find(sanitize_universe_id(universe_id));
        if(found == universes.end()) {
            return empty_universe;
        }
        return found->second;
    }

    write_result set_universe(int universe_id, const std::vector<int> &values) {
        if(values.size() < (std::size_t)universe_channel_count) {
            return write_result{false, "universe requires 512 values"};
        }
        dmx_universe &target = ensure_universe(universe_id);
        for(int address = 1; address <= universe_channel_count; address++) {
            target.set_channel(address, values[(std::size_t)(address - 1)]);
        }
        return write_result{true, ""};
    }

    write_result set_channel(int universe_id, int address, int value) {
        return ensure_universe(universe_id).set_channel(address, value);
    }

    std::vector<int> universe_ids() const {
        std::vector<int> ids;
        ids.reserve(universes.size());
        for(const auto &entry : universes) {
            ids.push_back(entry.first);
        }
        return ids;
    }

    bool empty() const {
        return universes.empty();
    }

    std::map<int, dmx_universe> universes{};
};

inline std::vector<int> changed_channels(const dmx_universe &before, const dmx_universe &after) {
    std::vector<int> changes;
    for(int address = 1; address <= universe_channel_count; address++) {
        const int after_value{after.channel(address)};
        if(before.channel(address) != after_value) {
            changes.push_back(address);
            changes.push_back(after_value);
        }
    }
    return changes;
}

} // namespace bbb::dmx
