#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include "bbb/dmx/value.hpp"

namespace bbb::dmx {

constexpr int universe_channel_count = 512;

struct write_result {
public:
    bool ok{false};
    const char *message{""};
};

inline int clamp_byte(int value) {
    return std::max(0, std::min(255, value));
}

class dmx_universe {
public:
    dmx_universe() {
        clear();
    }

    void clear(int value = 0) {
        channels_.fill((std::uint8_t)clamp_byte(value));
    }

    bool valid_address(int address) const {
        return 1 <= address && address <= universe_channel_count;
    }

    write_result set_channel(int address, int value) {
        if(!valid_address(address)) {
            return write_result{false, "channel address outside 1..512"};
        }
        channels_[(std::size_t)(address - 1)] = (std::uint8_t)clamp_byte(value);
        return write_result{true, ""};
    }

    write_result set_u16(int first_address, std::uint16_t value, byte_order order) {
        if(!valid_address(first_address) || !valid_address(first_address + 1)) {
            return write_result{false, "u16 write exceeds universe bounds"};
        }
        const split_u16 split{split_16(value)};
        if(order == byte_order::fine_coarse) {
            channels_[(std::size_t)(first_address - 1)] = split.fine;
            channels_[(std::size_t)first_address] = split.coarse;
        } else {
            channels_[(std::size_t)(first_address - 1)] = split.coarse;
            channels_[(std::size_t)first_address] = split.fine;
        }
        return write_result{true, ""};
    }

    write_result set_u24(int first_address, std::uint32_t value, byte_order order) {
        if(!valid_address(first_address) || !valid_address(first_address + 2)) {
            return write_result{false, "u24 write exceeds universe bounds"};
        }
        const split_u24 split{split_24(value)};
        if(order == byte_order::fine_mid_coarse) {
            channels_[(std::size_t)(first_address - 1)] = split.fine;
            channels_[(std::size_t)first_address] = split.middle;
            channels_[(std::size_t)(first_address + 1)] = split.coarse;
        } else {
            channels_[(std::size_t)(first_address - 1)] = split.coarse;
            channels_[(std::size_t)first_address] = split.middle;
            channels_[(std::size_t)(first_address + 1)] = split.fine;
        }
        return write_result{true, ""};
    }

    int channel(int address) const {
        if(!valid_address(address)) {
            return 0;
        }
        return (int)channels_[(std::size_t)(address - 1)];
    }

    const std::array<std::uint8_t, universe_channel_count> &bytes() const {
        return channels_;
    }

    std::vector<int> to_int_vector() const {
        std::vector<int> result;
        result.reserve(channels_.size());
        for(const std::uint8_t value : channels_) {
            result.push_back((int)value);
        }
        return result;
    }

private:
    std::array<std::uint8_t, universe_channel_count> channels_{};
};

} // namespace bbb::dmx
