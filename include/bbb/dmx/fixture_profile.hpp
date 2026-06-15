#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "bbb/dmx/value.hpp"

namespace bbb::dmx {

enum class fixture_parameter_type {
    u8,
    u16,
    u24,
    enum_u8,
};

struct fixture_channel {
public:
    int offset{0};
    std::string key{};
    int default_value{0};
    std::string label{};
    bool hold{false};
};

struct fixture_parameter_range {
public:
    int from{0};
    int to{0};
    std::string function{};
    std::string label{};
    std::string wheel{};
    bool has_wheel_slot{false};
    int wheel_slot{0};
    bool has_physical_from{false};
    double physical_from{0.0};
    bool has_physical_to{false};
    double physical_to{0.0};
};

struct fixture_parameter {
public:
    std::string key{};
    fixture_parameter_type type{fixture_parameter_type::u8};
    std::vector<std::string> channels{};
    byte_order order{byte_order::coarse_fine};
    int default_value{0};
    double range_degrees{0.0};
    std::string wheel{};
    std::vector<fixture_parameter_range> ranges{};
};

struct fixture_photometry {
public:
    bool has_beam_angle_degrees{false};
    double beam_angle_degrees{0.0};
    bool has_field_angle_degrees{false};
    double field_angle_degrees{0.0};
    bool has_beam_radius{false};
    double beam_radius{0.0};
    bool has_luminous_flux{false};
    double luminous_flux{0.0};
    bool has_color_temperature{false};
    double color_temperature{0.0};
};

struct fixture_mode {
public:
    std::string key{};
    std::string label{};
    int footprint{0};
    std::vector<fixture_channel> channels{};
    std::vector<fixture_parameter> parameters{};

    const fixture_channel *find_channel(const std::string &channel_key) const {
        for(const auto &channel : channels) {
            if(channel.key == channel_key) {
                return &channel;
            }
        }
        return nullptr;
    }

    const fixture_parameter *find_parameter(const std::string &parameter_key) const {
        for(const auto &parameter : parameters) {
            if(parameter.key == parameter_key) {
                return &parameter;
            }
        }
        return nullptr;
    }
};

struct fixture_wheel_slot_color {
public:
    bool has_cie_xyY{false};
    double cie_x{0.0};
    double cie_y{0.0};
    double cie_Y{0.0};
    bool has_rgb{false};
    int red{0};
    int green{0};
    int blue{0};
};

struct fixture_wheel_slot {
public:
    int index{0};
    std::string id{};
    std::string label{};
    std::string kind{};
    std::string filter{};
    std::string media{};
    fixture_wheel_slot_color color{};
};

struct fixture_wheel {
public:
    std::string id{};
    std::string label{};
    std::string type{};
    std::vector<fixture_wheel_slot> slots{};

    const fixture_wheel_slot *find_slot(int slot_index) const {
        for(const auto &slot : slots) {
            if(slot.index == slot_index) {
                return &slot;
            }
        }
        return nullptr;
    }
};

struct fixture_profile {
public:
    std::string key{};
    std::string manufacturer{};
    std::string model{};
    fixture_photometry photometry{};
    std::vector<fixture_wheel> wheels{};
    std::vector<fixture_mode> modes{};

    const fixture_mode *find_mode(const std::string &mode_key) const {
        for(const auto &mode : modes) {
            if(mode.key == mode_key) {
                return &mode;
            }
        }
        return nullptr;
    }

    const fixture_wheel *find_wheel(const std::string &wheel_id) const {
        for(const auto &wheel : wheels) {
            if(wheel.id == wheel_id) {
                return &wheel;
            }
        }
        return nullptr;
    }
};

} // namespace bbb::dmx
