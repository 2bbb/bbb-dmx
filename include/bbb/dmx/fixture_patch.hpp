#pragma once

#include <string>
#include <vector>

#include "bbb/dmx/math.hpp"

namespace bbb::dmx {

struct fixture_calibration {
public:
    double pan_offset{0.0};
    double tilt_offset{0.0};
    bool pan_invert{false};
    bool tilt_invert{false};
};

struct fixture_instance {
public:
    std::string id{};
    std::string profile{};
    std::string mode{};
    int universe{1};
    int address{1};
    vec3 position{};
    vec3 rotation{};
    fixture_calibration calibration{};
};

struct fixture_patch {
public:
    std::string schema{"bbb.dmx.patch.v2"};
    std::string coordinates{"gdtf"};
    std::vector<std::string> profile_paths{};
    std::vector<fixture_instance> fixtures{};
};

} // namespace bbb::dmx
