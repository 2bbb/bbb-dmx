#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "bbb/dmx/universe.hpp"

namespace bbb::dmx {

enum class dmx_mask_action {
    mute,
    hold,
    allow,
    force,
};

struct dmx_mask_rule {
public:
    int universe{0}; // 0 = all universes
    int start{1};
    int count{universe_channel_count};
    dmx_mask_action action{dmx_mask_action::mute};
    int value{0};
};

inline bool mask_action_from_string(const std::string &text, dmx_mask_action &action) {
    if(text == "mute") {
        action = dmx_mask_action::mute;
        return true;
    }
    if(text == "hold") {
        action = dmx_mask_action::hold;
        return true;
    }
    if(text == "allow") {
        action = dmx_mask_action::allow;
        return true;
    }
    if(text == "force") {
        action = dmx_mask_action::force;
        return true;
    }
    return false;
}

inline bool mask_rule_matches(const dmx_mask_rule &rule, int universe_id, int address) {
    if(rule.universe != 0 && rule.universe != universe_id) {
        return false;
    }
    const int start{std::max(1, rule.start)};
    const int count{std::max(0, rule.count)};
    return start <= address && address < start + count;
}

inline bool has_allow_rules(const std::vector<dmx_mask_rule> &rules) {
    for(const auto &rule : rules) {
        if(rule.action == dmx_mask_action::allow) {
            return true;
        }
    }
    return false;
}

inline dmx_universe apply_mask_rules(const dmx_universe &input, const dmx_universe &previous_output, int universe_id, const std::vector<dmx_mask_rule> &rules) {
    dmx_universe output{input};
    const bool allow_mode{has_allow_rules(rules)};
    for(int address = 1; address <= universe_channel_count; address++) {
        bool allowed{!allow_mode};
        int value{input.channel(address)};
        for(const auto &rule : rules) {
            if(!mask_rule_matches(rule, universe_id, address)) {
                continue;
            }
            switch(rule.action) {
                case dmx_mask_action::allow:
                    allowed = true;
                    break;
                case dmx_mask_action::hold:
                    value = previous_output.channel(address);
                    break;
                case dmx_mask_action::force:
                    value = rule.value;
                    break;
                case dmx_mask_action::mute:
                default:
                    value = 0;
                    break;
            }
        }
        if(!allowed) {
            value = 0;
        }
        output.set_channel(address, value);
    }
    return output;
}

} // namespace bbb::dmx
