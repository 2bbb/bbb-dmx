#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "bbb/dmx/common.hpp"
#include "bbb/dmx/curve.hpp"
#include "bbb/dmx/mask.hpp"
#include "bbb/dmx/matrix_map.hpp"
#include "bbb/dmx/movertrack.hpp"
#include "bbb/dmx/pattern.hpp"

namespace {

bool nearly_equal(double left, double right, double tolerance = 1.0e-9) {
    return std::abs(left - right) <= tolerance;
}

void require(bool condition, const char *message) {
    if(!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        std::exit(1);
    }
}


const double *find_semantic_parameter(const bbb::dmx::semantic_color_mapping &mapping, const std::string &key) {
    for(const auto &parameter : mapping.parameters) {
        if(parameter.first == key) {
            return &parameter.second;
        }
    }
    return nullptr;
}

const bbb::dmx::semantic_shutter_mapping *find_shutter_mapping(const bbb::dmx::semantic_shutter_mappings &mappings, const std::string &key) {
    for(const auto &mapping : mappings.mappings) {
        if(mapping.parameter == key) {
            return &mapping;
        }
    }
    return nullptr;
}

bbb::dmx::fixture_parameter make_u8_parameter(const std::string &key) {
    bbb::dmx::fixture_parameter parameter{};
    parameter.key = key;
    parameter.type = bbb::dmx::fixture_parameter_type::u8;
    parameter.channels = {key};
    return parameter;
}

bbb::dmx::fixture_channel make_u8_channel(int offset, const std::string &key, int default_value = 0) {
    bbb::dmx::fixture_channel channel{};
    channel.offset = offset;
    channel.key = key;
    channel.default_value = default_value;
    return channel;
}

bbb::dmx::fixture_channel make_labeled_u8_channel(int offset, const std::string &key, const std::string &label, int default_value = 0) {
    bbb::dmx::fixture_channel channel{make_u8_channel(offset, key, default_value)};
    channel.label = label;
    return channel;
}

bbb::dmx::fixture_mode make_semantic_color_mode(const std::vector<std::string> &keys) {
    bbb::dmx::fixture_mode mode{};
    mode.key = "color";
    mode.footprint = (int)keys.size();
    for(std::size_t index = 0; index < keys.size(); index++) {
        mode.channels.push_back(make_u8_channel((int)index + 1, keys[index]));
        mode.parameters.push_back(make_u8_parameter(keys[index]));
    }
    return mode;
}

bbb::dmx::fixture_parameter_range make_parameter_range(int from, int to, const std::string &function_name, const std::string &label) {
    bbb::dmx::fixture_parameter_range range{};
    range.from = from;
    range.to = to;
    range.function = function_name;
    range.label = label;
    return range;
}

bbb::dmx::fixture_mode make_semantic_shutter_mode(const std::string &parameter_key, const std::string &channel_key) {
    bbb::dmx::fixture_mode mode{};
    mode.key = "shutter";
    mode.footprint = 1;
    mode.channels = {make_u8_channel(1, channel_key)};
    bbb::dmx::fixture_parameter parameter{make_u8_parameter(parameter_key)};
    parameter.channels = {channel_key};
    parameter.ranges = {
        make_parameter_range(0, 31, "closed", "Closed"),
        make_parameter_range(32, 63, "open", "Open"),
        make_parameter_range(64, 127, "strobe", "Strobe")
    };
    mode.parameters = {parameter};
    return mode;
}

bbb::dmx::fixture_mode make_sgm_q8_simplified_mode() {
    bbb::dmx::fixture_mode mode{};
    mode.key = "12.channel.simplified";
    mode.footprint = 12;
    mode.channels = {
        make_labeled_u8_channel(1, "dimmer", "Dimmer"),
        make_labeled_u8_channel(2, "shutter", "StrobeDuration"),
        make_labeled_u8_channel(3, "shutter_2", "StrobeRate"),
        make_labeled_u8_channel(4, "shutter_3", "Shutter1StrobeEffect"),
        make_labeled_u8_channel(5, "red", "ColorAdd_R", 255),
        make_labeled_u8_channel(6, "green", "ColorAdd_G", 255),
        make_labeled_u8_channel(7, "blue", "ColorAdd_B", 255),
        make_labeled_u8_channel(8, "white", "ColorAdd_W", 255),
        make_labeled_u8_channel(9, "dimmer_2", "Dimmer"),
        make_labeled_u8_channel(10, "shutter_4", "StrobeDuration"),
        make_labeled_u8_channel(11, "shutter_5", "StrobeRate"),
        make_labeled_u8_channel(12, "shutter_6", "Shutter2StrobeEffect")
    };
    bbb::dmx::fixture_parameter dimmer{make_u8_parameter("dimmer")};
    dimmer.channels = {"dimmer"};
    dimmer.ranges = {
        make_parameter_range(0, 0, "min", "Min"),
        make_parameter_range(1, 254, "dimmer", ""),
        make_parameter_range(255, 255, "max", "Max")
    };
    bbb::dmx::fixture_parameter duration{make_u8_parameter("shutter")};
    duration.channels = {"shutter"};
    duration.ranges = {
        make_parameter_range(0, 0, "strobe", "Short"),
        make_parameter_range(1, 254, "strobe", ""),
        make_parameter_range(255, 255, "strobe", "Long")
    };
    bbb::dmx::fixture_parameter rate{make_u8_parameter("shutter_2")};
    rate.channels = {"shutter_2"};
    rate.ranges = {
        make_parameter_range(0, 0, "strobe", "Slow"),
        make_parameter_range(1, 254, "strobe", ""),
        make_parameter_range(255, 255, "strobe", "Fast")
    };
    bbb::dmx::fixture_parameter effect{make_u8_parameter("shutter_3")};
    effect.channels = {"shutter_3"};
    effect.ranges = {
        make_parameter_range(0, 5, "strobe", "No Effect"),
        make_parameter_range(6, 10, "strobe", "Flicker"),
        make_parameter_range(251, 255, "strobe", "No Effect")
    };
    bbb::dmx::fixture_parameter dimmer_2{make_u8_parameter("dimmer_2")};
    dimmer_2.channels = {"dimmer_2"};
    dimmer_2.ranges = {
        make_parameter_range(0, 0, "min", "Min"),
        make_parameter_range(1, 254, "intensity", ""),
        make_parameter_range(255, 255, "max", "Max")
    };
    bbb::dmx::fixture_parameter duration_2{make_u8_parameter("shutter_4")};
    duration_2.channels = {"shutter_4"};
    duration_2.ranges = duration.ranges;
    bbb::dmx::fixture_parameter rate_2{make_u8_parameter("shutter_5")};
    rate_2.channels = {"shutter_5"};
    rate_2.ranges = rate.ranges;
    bbb::dmx::fixture_parameter effect_2{make_u8_parameter("shutter_6")};
    effect_2.channels = {"shutter_6"};
    effect_2.ranges = {
        make_parameter_range(0, 9, "strobe", "No Effect"),
        make_parameter_range(10, 19, "random", "Random Pixel"),
        make_parameter_range(251, 255, "strobe", "No Effect")
    };
    mode.parameters = {dimmer, duration, rate, effect, dimmer_2, duration_2, rate_2, effect_2};
    return mode;
}

} // namespace

int main() {
    require(bbb::dmx::angle_to_u16(-270.0, 540.0) == 0, "pan range minimum maps to 0");
    require(bbb::dmx::angle_to_u16(270.0, 540.0) == 65535, "pan range maximum maps to 65535");
    require(bbb::dmx::angle_to_u16(0.0, 540.0) == 32768, "center uses round() and maps to 32768");

    const std::array<int, 4> coarse_fine{bbb::dmx::pan_tilt_to_bytes(0x1234, 0xABCD, bbb::dmx::byte_order::coarse_fine)};
    require(coarse_fine == std::array<int, 4>{18, 52, 171, 205}, "coarse/fine byte order");

    const std::array<int, 4> fine_coarse{bbb::dmx::pan_tilt_to_bytes(0x1234, 0xABCD, bbb::dmx::byte_order::fine_coarse)};
    require(fine_coarse == std::array<int, 4>{52, 18, 205, 171}, "fine/coarse byte order");
    require(bbb::dmx::normalized_to_u8(0.5) == 128, "normalized half maps to u8 128");
    require(bbb::dmx::normalized_to_u16(0.5) == 32768, "normalized half maps to u16 32768");
    require(bbb::dmx::normalized_to_u24(1.0) == 16777215, "normalized one maps to max u24");
    require(bbb::dmx::combine_24(0x12, 0x34, 0x56, bbb::dmx::byte_order::coarse_mid_fine) == 0x123456, "combine u24 msb first");
    require(bbb::dmx::combine_24(0x56, 0x34, 0x12, bbb::dmx::byte_order::fine_mid_coarse) == 0x123456, "combine u24 lsb first");
    require(bbb::dmx::wildcard_match("pixel_*", "pixel_001"), "wildcard star matches suffix");
    require(!bbb::dmx::wildcard_match("spot_*", "pixel_001"), "wildcard rejects wrong prefix");

    bbb::dmx::dmx_universe curve_input{};
    curve_input.set_channel(1, 64);
    curve_input.set_channel(2, 128);
    bbb::dmx::dmx_curve_rule gamma_rule{};
    gamma_rule.universe = 1;
    gamma_rule.start = 1;
    gamma_rule.count = 1;
    gamma_rule.type = bbb::dmx::dmx_curve_type::gamma;
    gamma_rule.gamma = 2.0;
    const bbb::dmx::dmx_universe curve_output{bbb::dmx::apply_curve_rules(curve_input, 1, {gamma_rule})};
    require(curve_output.channel(1) < curve_input.channel(1), "gamma curve darkens low value");
    require(curve_output.channel(2) == 128, "curve only touches matching range");

    bbb::dmx::dmx_mask_rule mute_rule{};
    mute_rule.universe = 1;
    mute_rule.start = 2;
    mute_rule.count = 1;
    mute_rule.action = bbb::dmx::dmx_mask_action::mute;
    bbb::dmx::dmx_universe previous_mask_output{};
    previous_mask_output.set_channel(1, 99);
    previous_mask_output.set_channel(2, 88);
    const bbb::dmx::dmx_universe mask_output{bbb::dmx::apply_mask_rules(curve_input, previous_mask_output, 1, {mute_rule})};
    require(mask_output.channel(1) == 64, "mask leaves unmatched channel");
    require(mask_output.channel(2) == 0, "mask mutes matching channel");

    bbb::dmx::dmx_mask_rule hold_rule{};
    hold_rule.universe = 1;
    hold_rule.start = 1;
    hold_rule.count = 1;
    hold_rule.action = bbb::dmx::dmx_mask_action::hold;
    const bbb::dmx::dmx_universe hold_output{bbb::dmx::apply_mask_rules(curve_input, previous_mask_output, 1, {hold_rule})};
    require(hold_output.channel(1) == 99, "mask hold preserves previous output");

    require(nearly_equal(bbb::dmx::choose_shortest_pan(-179.0, 179.0), 181.0), "shortest pan crosses atan2 wrap");

    const bbb::dmx::pan_tilt_degrees forward{bbb::dmx::vector_to_pan_tilt(bbb::dmx::vec3{0.0, 10.0, 0.0})};
    require(nearly_equal(forward.pan, 0.0), "GDTF forward pan is zero");
    require(nearly_equal(forward.tilt, 90.0), "GDTF forward tilt is horizontal +90 from hanging rest");

    const bbb::dmx::pan_tilt_degrees right{bbb::dmx::vector_to_pan_tilt(bbb::dmx::vec3{10.0, 0.0, 0.0})};
    require(nearly_equal(right.pan, -90.0), "GDTF right pan is -90 around +Z");
    require(nearly_equal(right.tilt, 90.0), "GDTF right tilt is horizontal");

    const bbb::dmx::pan_tilt_degrees downward{bbb::dmx::vector_to_pan_tilt(bbb::dmx::vec3{0.0, 0.0, -10.0})};
    require(nearly_equal(downward.pan, 0.0), "GDTF rest pan is zero when target is on tilt axis");
    require(nearly_equal(downward.tilt, 0.0), "GDTF rest tilt points device-local -Z");

    const bbb::dmx::pan_tilt_degrees upward{bbb::dmx::vector_to_pan_tilt(bbb::dmx::vec3{0.0, 0.0, 10.0})};
    require(nearly_equal(upward.pan, 0.0), "GDTF upward pan is zero when target is on tilt axis");
    require(nearly_equal(upward.tilt, 180.0), "GDTF upward tilt is 180 from hanging rest");

    const bbb::dmx::vec3 world_forward{0.0, 10.0, 0.0};
    const bbb::dmx::vec3 local{bbb::dmx::world_to_fixture_local(world_forward, 0.0, 0.0, 90.0)};
    require(nearly_equal(local.x, 10.0), "inverse fixture rotation converts world forward to local right for rz=90");
    require(nearly_equal(local.y, 0.0, 1.0e-8), "inverse fixture rotation y component near zero");

    bbb::dmx::movertrack_engine engine{};
    auto output = engine.compute(bbb::dmx::vec3{0.0, 10.0, 0.0});
    require(nearly_equal(output.pan_degrees, 0.0), "movertrack GDTF forward pan degrees");
    require(nearly_equal(output.tilt_degrees, 90.0), "movertrack GDTF horizontal tilt degrees");
    require(output.pan == bbb::dmx::angle_to_u16(0.0, 540.0), "movertrack GDTF forward pan value");
    require(output.tilt == bbb::dmx::angle_to_u16(90.0, 270.0), "movertrack GDTF forward tilt value");

    output = engine.compute(bbb::dmx::vec3{10.0, 0.0, 0.0});
    require(nearly_equal(output.pan_degrees, -90.0), "movertrack GDTF right pan degrees");

    bbb::dmx::movertrack_engine relative_engine{};
    relative_engine.set_tracking_mode(bbb::dmx::tracking_mode::off);
    relative_engine.set_fixture_position(bbb::dmx::vec3{5.0, 10.0, 2.0});
    const bbb::dmx::movertrack_output absolute_from_offset{relative_engine.compute(bbb::dmx::vec3{5.0, 20.0, 2.0})};
    relative_engine.reset_tracking();
    const bbb::dmx::movertrack_output relative_from_origin{relative_engine.compute_relative(bbb::dmx::vec3{0.0, 10.0, 0.0})};
    require(nearly_equal(relative_from_origin.pan_degrees, absolute_from_offset.pan_degrees), "relative movertrack matches equivalent absolute pan");
    require(nearly_equal(relative_from_origin.tilt_degrees, absolute_from_offset.tilt_degrees), "relative movertrack matches equivalent absolute tilt");
    relative_engine.set_fixture_position(bbb::dmx::vec3{100.0, -50.0, 8.0});
    output = relative_engine.bang();
    require(nearly_equal(output.pan_degrees, relative_from_origin.pan_degrees), "bang preserves relative movertrack pan after fixture move");
    require(nearly_equal(output.tilt_degrees, relative_from_origin.tilt_degrees), "bang preserves relative movertrack tilt after fixture move");

    bbb::dmx::movertrack_engine tracking_engine{};
    tracking_engine.set_tracking_mode(bbb::dmx::tracking_mode::pan);
    output = tracking_engine.compute(bbb::dmx::vec3{-0.0174524064, -0.999847695, 0.0});
    require(nearly_equal(output.pan_degrees, 179.0, 1.0e-6), "movertrack initializes near +179");
    output = tracking_engine.compute(bbb::dmx::vec3{0.0174524064, -0.999847695, 0.0});
    require(nearly_equal(output.pan_degrees, 181.0, 1.0e-6), "movertrack tracks shortest pan to +181");

    bbb::dmx::movertrack_engine clamp_engine{};
    clamp_engine.set_tracking_mode(bbb::dmx::tracking_mode::off);
    clamp_engine.set_ranges(180.0, 270.0);
    output = clamp_engine.compute(bbb::dmx::vec3{10.0, 0.0, 0.0});
    require(nearly_equal(output.pan_degrees, -90.0), "movertrack pan at clamp boundary");
    output = clamp_engine.compute(bbb::dmx::vec3{0.0174524064, -0.999847695, 0.0});
    require(nearly_equal(output.pan_degrees, -90.0), "movertrack clamps pan below range");

    bbb::dmx::movertrack_engine smart_engine{};
    smart_engine.set_ranges(180.0, 270.0);
    output = smart_engine.compute(bbb::dmx::vec3{1.0, 0.1, 0.0});
    require(nearly_equal(output.pan_degrees, -84.28940686250036), "smart tracking initializes direct GDTF candidate");
    require(nearly_equal(output.tilt_degrees, 90.0), "smart tracking initializes horizontal GDTF tilt");
    output = smart_engine.compute(bbb::dmx::vec3{-0.1, -1.0, 0.0});
    require(nearly_equal(output.pan_degrees, -5.710593137499643), "smart tracking chooses valid GDTF pan-flip candidate");
    require(nearly_equal(output.tilt_degrees, -90.0), "smart tracking chooses equivalent negative GDTF tilt candidate");

    bbb::dmx::movertrack_engine pan_only_engine{};
    pan_only_engine.set_ranges(180.0, 270.0);
    pan_only_engine.set_tracking_mode(bbb::dmx::tracking_mode::pan);
    output = pan_only_engine.compute(bbb::dmx::vec3{1.0, 0.1, 0.0});
    output = pan_only_engine.compute(bbb::dmx::vec3{-0.1, -1.0, 0.0});
    require(nearly_equal(output.pan_degrees, -90.0), "pan-only tracking still clamps invalid pan candidate");

    bbb::dmx::movertrack_engine calibration_engine{};
    require(calibration_engine.calibrate_tilt_offset(
        bbb::dmx::vec3{0.0, 10.0, 0.0},
        bbb::dmx::angle_to_u16(0.0, 270.0)
    ), "calibrate tilt offset from target and u16");
    require(nearly_equal(calibration_engine.settings().tilt_offset_degrees, -90.0, 0.01), "calibrated tilt offset reflects GDTF horizontal zero correction");
    require(calibration_engine.calibrate_pan_offset(
        bbb::dmx::vec3{10.0, 0.0, 0.0},
        bbb::dmx::angle_to_u16(0.0, 540.0)
    ), "calibrate pan offset from target and u16");
    require(nearly_equal(calibration_engine.settings().pan_offset_degrees, 90.0, 0.01), "calibrated pan offset reflects GDTF right-hand pan sign");

    bbb::dmx::movertrack_engine gdtf_hang_engine{};
    gdtf_hang_engine.set_fixture_position(bbb::dmx::vec3{0.0, 3.84, 2.55});
    const bbb::dmx::movertrack_output gdtf_horizontal{gdtf_hang_engine.compute(bbb::dmx::vec3{0.0, 0.0, 2.55})};
    const bbb::dmx::movertrack_output gdtf_floor{gdtf_hang_engine.compute(bbb::dmx::vec3{0.0, 0.0, 0.0})};
    const bbb::dmx::movertrack_output gdtf_ceiling{gdtf_hang_engine.compute(bbb::dmx::vec3{0.0, 0.0, 4.0})};
    require(nearly_equal(gdtf_horizontal.pan_degrees, 0.0), "GDTF smart tracking uses the equivalent pan-center solution for world y=0");
    require(gdtf_horizontal.tilt < gdtf_floor.tilt, "GDTF smart tracking floor target increases tilt from horizontal in this rig");
    require(gdtf_ceiling.tilt < gdtf_horizontal.tilt, "GDTF smart tracking ceiling target decreases tilt from horizontal in this rig");

    bbb::dmx::byte_order parsed_order{bbb::dmx::byte_order::coarse_fine};
    require(bbb::dmx::byte_order_from_string("finecoarse", parsed_order), "parse finecoarse");
    require(parsed_order == bbb::dmx::byte_order::fine_coarse, "parsed finecoarse value");
    require(bbb::dmx::byte_order_from_string("coarsemidfine", parsed_order), "parse coarsemidfine");
    require(parsed_order == bbb::dmx::byte_order::coarse_mid_fine, "parsed coarsemidfine value");
    require(!bbb::dmx::byte_order_from_string("bad", parsed_order), "reject bad byte order");

    bbb::dmx::tracking_mode parsed_tracking_mode{bbb::dmx::tracking_mode::off};
    require(bbb::dmx::tracking_mode_from_string("smart", parsed_tracking_mode), "parse smart tracking mode");
    require(parsed_tracking_mode == bbb::dmx::tracking_mode::smart, "parsed smart tracking mode value");
    require(!bbb::dmx::tracking_mode_from_string("bad", parsed_tracking_mode), "reject bad tracking mode");



    bbb::dmx::fixture_profile profile{};
    profile.key = "generic.mover.16bit";
    bbb::dmx::fixture_mode mode{};
    mode.key = "basic16";
    mode.footprint = 8;
    mode.channels = {
        bbb::dmx::fixture_channel{1, "pan.coarse", 128},
        bbb::dmx::fixture_channel{2, "pan.fine", 0},
        bbb::dmx::fixture_channel{3, "tilt.coarse", 128},
        bbb::dmx::fixture_channel{4, "tilt.fine", 0},
        bbb::dmx::fixture_channel{5, "dimmer", 0},
        bbb::dmx::fixture_channel{6, "shutter", 0},
        bbb::dmx::fixture_channel{7, "color", 0},
        bbb::dmx::fixture_channel{8, "gobo", 0},
    };
    bbb::dmx::fixture_parameter pan_parameter{};
    pan_parameter.key = "pan";
    pan_parameter.type = bbb::dmx::fixture_parameter_type::u16;
    pan_parameter.channels = {"pan.coarse", "pan.fine"};
    pan_parameter.default_value = 32768;
    bbb::dmx::fixture_parameter tilt_parameter{};
    tilt_parameter.key = "tilt";
    tilt_parameter.type = bbb::dmx::fixture_parameter_type::u16;
    tilt_parameter.channels = {"tilt.coarse", "tilt.fine"};
    tilt_parameter.default_value = 32768;
    bbb::dmx::fixture_parameter dimmer_parameter{};
    dimmer_parameter.key = "dimmer";
    dimmer_parameter.type = bbb::dmx::fixture_parameter_type::u8;
    dimmer_parameter.channels = {"dimmer"};
    dimmer_parameter.default_value = 0;
    mode.parameters = {pan_parameter, tilt_parameter, dimmer_parameter};
    profile.modes = {mode};

    bbb::dmx::fixture_patch patch{};
    bbb::dmx::fixture_instance fixture{};
    fixture.id = "spot_01";
    fixture.profile = "generic.mover.16bit";
    fixture.mode = "basic16";
    fixture.universe = 1;
    fixture.address = 10;
    patch.fixtures = {fixture};

    bbb::dmx::fixture_mapper mapper{};
    auto map_result = mapper.add_profile(profile);
    require(map_result.ok, "fixture mapper accepts profile");
    map_result = mapper.set_patch(patch);
    require(map_result.ok, "fixture mapper accepts patch");
    require(mapper.universe(1).channel(10) == 128, "fixture mapper writes pan coarse default");
    require(mapper.universe(1).channel(11) == 0, "fixture mapper writes pan fine default");
    require(mapper.universe(1).channel(14) == 0, "fixture mapper writes dimmer default");

    map_result = mapper.set_u8("spot_01", "dimmer", 255);
    require(map_result.ok, "fixture mapper sets u8 parameter");
    require(mapper.universe(1).channel(14) == 255, "fixture mapper maps dimmer to absolute address");

    map_result = mapper.set_u16("spot_01", "pan", 0x1234);
    require(map_result.ok, "fixture mapper sets u16 parameter");
    require(mapper.universe(1).channel(10) == 0x12, "fixture mapper maps u16 coarse byte");
    require(mapper.universe(1).channel(11) == 0x34, "fixture mapper maps u16 fine byte");

    std::vector<std::pair<int, int>> parameter_addresses{};
    map_result = mapper.parameter_channel_addresses("spot_01", "pan", parameter_addresses);
    require(map_result.ok, "fixture mapper resolves u16 parameter addresses");
    require(parameter_addresses.size() == 2, "fixture mapper reports u16 address count");
    require(parameter_addresses[0] == std::pair<int, int>{1, 10}, "fixture mapper reports u16 coarse address");
    require(parameter_addresses[1] == std::pair<int, int>{1, 11}, "fixture mapper reports u16 fine address");
    map_result = mapper.parameter_channel_addresses("spot_01", "dimmer", parameter_addresses);
    require(map_result.ok, "fixture mapper resolves u8 parameter addresses");
    require(parameter_addresses.size() == 1, "fixture mapper reports u8 address count");
    require(parameter_addresses[0] == std::pair<int, int>{1, 14}, "fixture mapper reports u8 address");

    map_result = mapper.set_pan_tilt_bytes("spot_01", 1, 2, 3, 4);
    require(map_result.ok, "fixture mapper accepts movertrack byte tuple");
    require(mapper.universe(1).channel(10) == 1, "fixture mapper maps pan byte 1");
    require(mapper.universe(1).channel(11) == 2, "fixture mapper maps pan byte 2");
    require(mapper.universe(1).channel(12) == 3, "fixture mapper maps tilt byte 1");
    require(mapper.universe(1).channel(13) == 4, "fixture mapper maps tilt byte 2");

    bbb::dmx::fixture_patch overlap_patch{};
    bbb::dmx::fixture_instance overlap_a{fixture};
    bbb::dmx::fixture_instance overlap_b{fixture};
    overlap_b.id = "spot_02";
    overlap_b.address = 12;
    overlap_patch.fixtures = {overlap_a, overlap_b};
    map_result = mapper.set_patch(overlap_patch);
    require(!map_result.ok, "fixture mapper rejects overlapping fixtures");

    bbb::dmx::fixture_profile rgb24_profile{};
    rgb24_profile.key = "generic.rgb.24bit";
    bbb::dmx::fixture_mode rgb24_mode{};
    rgb24_mode.key = "rgb24";
    rgb24_mode.footprint = 9;
    rgb24_mode.channels = {
        bbb::dmx::fixture_channel{1, "red.coarse", 0},
        bbb::dmx::fixture_channel{2, "red.middle", 0},
        bbb::dmx::fixture_channel{3, "red.fine", 0},
        bbb::dmx::fixture_channel{4, "green.coarse", 0},
        bbb::dmx::fixture_channel{5, "green.middle", 0},
        bbb::dmx::fixture_channel{6, "green.fine", 0},
        bbb::dmx::fixture_channel{7, "blue.coarse", 0},
        bbb::dmx::fixture_channel{8, "blue.middle", 0},
        bbb::dmx::fixture_channel{9, "blue.fine", 0},
    };
    bbb::dmx::fixture_parameter red24_parameter{};
    red24_parameter.key = "red";
    red24_parameter.type = bbb::dmx::fixture_parameter_type::u24;
    red24_parameter.channels = {"red.coarse", "red.middle", "red.fine"};
    red24_parameter.order = bbb::dmx::byte_order::coarse_mid_fine;
    rgb24_mode.parameters = {red24_parameter};
    rgb24_profile.modes = {rgb24_mode};

    bbb::dmx::fixture_patch rgb24_patch{};
    bbb::dmx::fixture_instance rgb24_fixture{};
    rgb24_fixture.id = "pixel_01";
    rgb24_fixture.profile = "generic.rgb.24bit";
    rgb24_fixture.mode = "rgb24";
    rgb24_fixture.universe = 2;
    rgb24_fixture.address = 100;
    rgb24_patch.fixtures = {rgb24_fixture};

    bbb::dmx::fixture_mapper rgb24_mapper{};
    map_result = rgb24_mapper.add_profile(rgb24_profile);
    require(map_result.ok, "fixture mapper accepts u24 profile");
    map_result = rgb24_mapper.set_patch(rgb24_patch);
    require(map_result.ok, "fixture mapper accepts u24 patch");
    map_result = rgb24_mapper.set_normalized("pixel_01", "red", 0.5);
    require(map_result.ok, "fixture mapper sets u24 normalized parameter");
    require(rgb24_mapper.universe(2).channel(100) == 128, "fixture mapper maps u24 coarse byte");
    require(rgb24_mapper.universe(2).channel(101) == 0, "fixture mapper maps u24 middle byte");
    require(rgb24_mapper.universe(2).channel(102) == 0, "fixture mapper maps u24 fine byte");



    const std::string profile_json{R"json({
        "schema": "bbb.dmx.fixture.profile.v1",
        "key": "generic.json.mover",
        "manufacturer": "Generic",
        "model": "JSON Mover",
        "photometry": {
            "beam_angle_degrees": 4.5,
            "field_angle_degrees": 25.0,
            "beam_radius": 0.052,
            "luminous_flux": 1000.0,
            "color_temperature": 6500.0
        },
        "wheels": [
            {
                "id": "ColorWheel1",
                "label": "Color Wheel 1",
                "type": "color",
                "slots": [
                    { "index": 1, "id": "open", "label": "Open", "kind": "open", "rgb": [255, 255, 255] },
                    { "index": 2, "id": "red", "label": "Red", "kind": "color", "rgb": [255, 0, 0] },
                    { "index": 3, "id": "blue", "label": "Blue", "kind": "color", "cie_xyY": [0.15, 0.06, 100.0] }
                ]
            }
        ],
        "modes": {
            "basic16": {
                "footprint": 6,
                "channels": [
                    { "offset": 1, "key": "pan.coarse", "default": 128 },
                    { "offset": 2, "key": "pan.fine", "default": 0 },
                    { "offset": 3, "key": "tilt.coarse", "default": 128 },
                    { "offset": 4, "key": "tilt.fine", "default": 0 },
                    { "offset": 5, "key": "dimmer", "default": 0 },
                    { "offset": 6, "key": "shutter", "default": 32 }
                ],
                "parameters": {
                    "pan": { "type": "u16", "channels": ["pan.coarse", "pan.fine"], "byte_order": "coarsefine", "default": 32768 },
                    "tilt": { "type": "u16", "channels": ["tilt.coarse", "tilt.fine"], "byte_order": "coarsefine", "default": 32768 },
                    "color24": { "type": "u24", "channels": ["pan.coarse", "pan.fine", "tilt.coarse"], "byte_order": "coarsemidfine", "default": 8388608 },
                    "color_wheel": {
                        "type": "enum",
                        "channel": "shutter",
                        "wheel": "ColorWheel1",
                        "ranges": [
                            { "from": 0, "to": 9, "function": "open", "label": "Open", "wheel_slot": 1 },
                            { "from": 10, "to": 19, "function": "red", "label": "Red", "wheel_slot": 2 },
                            { "from": 20, "to": 29, "function": "blue", "label": "Blue", "wheel_slot": 3 }
                        ]
                    },
                    "shutter": {
                        "type": "u8",
                        "channel": "shutter",
                        "default": 32,
                        "ranges": [
                            { "from": 0, "to": 31, "function": "closed", "label": "Closed" },
                            { "from": 32, "to": 63, "function": "open", "label": "Open" },
                            { "from": 64, "to": 127, "function": "strobe", "label": "Strobe", "physical_from": 0.5, "physical_to": 10.0 }
                        ]
                    },
                    "dimmer": { "type": "u8", "channel": "dimmer", "default": 0 }
                }
            }
        }
    })json"};
    bbb::dmx::fixture_profile parsed_profile{};
    map_result = bbb::dmx::parse_fixture_profile_text(profile_json, parsed_profile);
    require(map_result.ok, "fixture JSON profile parses");
    require(parsed_profile.key == "generic.json.mover", "fixture JSON profile key");
    require(parsed_profile.modes.size() == 1, "fixture JSON profile mode count");
    require(parsed_profile.photometry.has_beam_angle_degrees, "fixture JSON photometry beam angle present");
    require(nearly_equal(parsed_profile.photometry.beam_angle_degrees, 4.5), "fixture JSON photometry beam angle value");
    require(parsed_profile.photometry.has_field_angle_degrees, "fixture JSON photometry field angle present");
    require(nearly_equal(parsed_profile.photometry.field_angle_degrees, 25.0), "fixture JSON photometry field angle value");
    require(parsed_profile.photometry.has_beam_radius, "fixture JSON photometry beam radius present");
    require(nearly_equal(parsed_profile.photometry.beam_radius, 0.052), "fixture JSON photometry beam radius value");
    require(parsed_profile.photometry.has_luminous_flux, "fixture JSON photometry luminous flux present");
    require(nearly_equal(parsed_profile.photometry.luminous_flux, 1000.0), "fixture JSON photometry luminous flux value");
    require(parsed_profile.photometry.has_color_temperature, "fixture JSON photometry color temperature present");
    require(nearly_equal(parsed_profile.photometry.color_temperature, 6500.0), "fixture JSON photometry color temperature value");
    require(parsed_profile.wheels.size() == 1, "fixture JSON wheel count");
    require(parsed_profile.wheels[0].slots.size() == 3, "fixture JSON wheel slot count");
    require(parsed_profile.wheels[0].slots[1].color.has_rgb, "fixture JSON wheel slot rgb present");
    require(parsed_profile.wheels[0].slots[2].color.has_cie_xyY, "fixture JSON wheel slot cie present");
    require(parsed_profile.modes[0].channels.size() == 6, "fixture JSON channel count");
    const bbb::dmx::fixture_parameter *parsed_color24{parsed_profile.modes[0].find_parameter("color24")};
    require(parsed_color24 != nullptr, "fixture JSON u24 parameter exists");
    require(parsed_color24->type == bbb::dmx::fixture_parameter_type::u24, "fixture JSON u24 parameter type");
    require(parsed_color24->order == bbb::dmx::byte_order::coarse_mid_fine, "fixture JSON u24 byte order");
    const bbb::dmx::fixture_parameter *parsed_shutter{parsed_profile.modes[0].find_parameter("shutter")};
    require(parsed_shutter != nullptr, "fixture JSON shutter parameter exists");
    require(parsed_shutter->ranges.size() == 3, "fixture JSON shutter ranges count");
    require(parsed_shutter->ranges[0].function == "closed", "fixture JSON shutter range function");
    require(parsed_shutter->ranges[1].from == 32 && parsed_shutter->ranges[1].to == 63, "fixture JSON shutter open range bounds");
    require(parsed_shutter->ranges[2].has_physical_from && parsed_shutter->ranges[2].has_physical_to, "fixture JSON shutter physical range present");
    require(nearly_equal(parsed_shutter->ranges[2].physical_from, 0.5), "fixture JSON shutter physical from");
    require(nearly_equal(parsed_shutter->ranges[2].physical_to, 10.0), "fixture JSON shutter physical to");
    const bbb::dmx::fixture_parameter *parsed_color_wheel{parsed_profile.modes[0].find_parameter("color_wheel")};
    require(parsed_color_wheel != nullptr, "fixture JSON color wheel parameter exists");
    require(parsed_color_wheel->wheel == "ColorWheel1", "fixture JSON color wheel parameter link");
    require(parsed_color_wheel->ranges[1].has_wheel_slot && parsed_color_wheel->ranges[1].wheel_slot == 2, "fixture JSON color wheel range slot");

    const std::string patch_json{R"json({
        "schema": "bbb.dmx.patch.v2",
        "coordinates": "gdtf",
        "profiles": ["fixtures/generic.json.mover.json"],
        "fixtures": [
            {
                "id": 12,
                "profile": "generic.json.mover",
                "mode": "basic16",
                "universe": 1,
                "address": 21,
                "position": [0.0, 0.0, 3.0],
                "rotation": [0.0, 0.0, 0.0],
                "calibration": { "pan_offset": 1.0, "tilt_offset": -2.0, "pan_invert": true, "tilt_invert": false }
            }
        ]
    })json"};
    bbb::dmx::fixture_patch parsed_patch{};
    map_result = bbb::dmx::parse_fixture_patch_text(patch_json, parsed_patch);
    require(map_result.ok, "fixture JSON patch parses");
    require(parsed_patch.schema == "bbb.dmx.patch.v2", "fixture JSON patch schema");
    require(parsed_patch.coordinates == "gdtf", "fixture JSON patch coordinates");
    require(parsed_patch.profile_paths.size() == 1, "fixture JSON patch profile path count");
    require(parsed_patch.fixtures.size() == 1, "fixture JSON patch fixture count");
    require(parsed_patch.fixtures[0].id == "12", "fixture JSON numeric patch id canonicalizes to string");
    require(parsed_patch.fixtures[0].address == 21, "fixture JSON patch address");
    require(parsed_patch.fixtures[0].calibration.pan_invert, "fixture JSON patch calibration bool");

    bbb::dmx::fixture_mapper json_mapper{};
    map_result = json_mapper.add_profile(parsed_profile);
    require(map_result.ok, "fixture JSON mapper accepts parsed profile");
    map_result = json_mapper.set_patch(parsed_patch);
    require(map_result.ok, "fixture JSON mapper accepts parsed patch");
    map_result = json_mapper.set_normalized("12", "dimmer", 0.5);
    require(map_result.ok, "fixture JSON mapper normalized set");
    require(json_mapper.universe(1).channel(25) == 128, "fixture JSON mapper normalized dimmer");

    bbb::dmx::fixture_instance group_fixture_a{};
    group_fixture_a.id = "fixture_01";
    bbb::dmx::fixture_instance group_fixture_b{};
    group_fixture_b.id = "fixture_02";
    bbb::dmx::fixture_patch group_patch{};
    group_patch.fixtures = {group_fixture_a, group_fixture_b};
    bbb::dmx::fixture_group_set group_set{};
    map_result = bbb::dmx::parse_fixture_groups_text(R"json({
        "schema": "bbb.dmx.groups.v1",
        "groups": {
            "front": ["fixture_02", "fixture_01", "fixture_02"]
        }
    })json", group_set);
    require(map_result.ok, "fixture groups JSON parses");
    map_result = bbb::dmx::validate_fixture_groups_for_patch(group_set, group_patch);
    require(map_result.ok, "fixture groups validate against patch");
    std::vector<std::string> resolved_group_fixture_ids{};
    map_result = bbb::dmx::resolve_fixture_group_fixture_ids(group_set, group_patch, "front", resolved_group_fixture_ids);
    require(map_result.ok, "fixture group resolves");
    require(resolved_group_fixture_ids.size() == 2, "fixture group de-duplicates fixture ids");
    require(resolved_group_fixture_ids[0] == "fixture_02" && resolved_group_fixture_ids[1] == "fixture_01", "fixture group resolves in group JSON order");


    bbb::dmx::matrixmap::matrix_map_config matrix_map{};
    map_result = bbb::dmx::matrixmap::parse_matrix_map_text(R"json({
        "schema": "bbb.dmx.matrixmap.v1",
        "fixtures": [
            {
                "group": "front",
                "sample": { "x": 0.5, "y": 0.25, "mode": "point" },
                "params": { "red": "r", "green": "g", "blue": "b" }
            }
        ]
    })json", matrix_map);
    require(map_result.ok, "matrixmap group fixture mapping parses");
    require(matrix_map.fixtures.size() == 1, "matrixmap group fixture mapping count");
    require(matrix_map.fixtures[0].fixture_id.empty(), "matrixmap group fixture mapping has no fixture id");
    require(matrix_map.fixtures[0].group_id == "front", "matrixmap group fixture mapping stores group id");
    require(matrix_map.fixtures[0].parameters.size() == 3, "matrixmap group fixture mapping stores RGB params");
    map_result = bbb::dmx::matrixmap::parse_matrix_map_text(R"json({
        "schema": "bbb.dmx.matrixmap.v1",
        "fixtures": [
            {
                "id": "fixture_01",
                "group": "front",
                "sample": { "x": 0.5, "y": 0.25 },
                "params": { "red": "r" }
            }
        ]
    })json", matrix_map);
    require(!map_result.ok, "matrixmap fixture mapping rejects id and group together");

    map_result = bbb::dmx::matrixmap::parse_matrix_map_text(R"json({
        "schema": "bbb.dmx.matrixmap.v1",
        "grid": {
            "fixture_pattern": "pixel_%02d",
            "start_index": 1,
            "cols": 2,
            "rows": 2,
            "x0": 0.2,
            "y0": 0.1,
            "x1": 0.8,
            "y1": 0.9,
            "mode": "average",
            "placement": "cell",
            "params": { "red": "r" }
        }
    })json", matrix_map);
    require(map_result.ok, "matrixmap bounded cell grid parses");
    require(matrix_map.fixtures.size() == 4, "matrixmap bounded cell grid expands count");
    require(matrix_map.fixtures[0].fixture_id == "pixel_01", "matrixmap bounded cell grid first id");
    require(nearly_equal(matrix_map.fixtures[0].sample.x, 0.35), "matrixmap bounded cell grid first x center");
    require(nearly_equal(matrix_map.fixtures[0].sample.y, 0.3), "matrixmap bounded cell grid first y center");
    require(nearly_equal(matrix_map.fixtures[0].sample.width, 0.3), "matrixmap bounded cell grid cell width");
    require(nearly_equal(matrix_map.fixtures[0].sample.height, 0.4), "matrixmap bounded cell grid cell height");
    require(nearly_equal(matrix_map.fixtures[3].sample.x, 0.65), "matrixmap bounded cell grid last x center");
    require(nearly_equal(matrix_map.fixtures[3].sample.y, 0.7), "matrixmap bounded cell grid last y center");

    map_result = bbb::dmx::matrixmap::parse_matrix_map_text(R"json({
        "schema": "bbb.dmx.matrixmap.v1",
        "grid": {
            "fixture_pattern": "point_%02d",
            "start_index": 10,
            "cols": 3,
            "rows": 2,
            "x0": 0.2,
            "y0": 0.1,
            "x1": 0.8,
            "y1": 0.9,
            "mode": "point",
            "placement": "points",
            "params": { "red": "r" }
        }
    })json", matrix_map);
    require(map_result.ok, "matrixmap bounded points grid parses");
    require(matrix_map.fixtures.size() == 6, "matrixmap bounded points grid expands count");
    require(matrix_map.fixtures[0].fixture_id == "point_10", "matrixmap bounded points grid first id");
    require(nearly_equal(matrix_map.fixtures[0].sample.x, 0.2), "matrixmap bounded points grid first x");
    require(nearly_equal(matrix_map.fixtures[0].sample.y, 0.1), "matrixmap bounded points grid first y");
    require(nearly_equal(matrix_map.fixtures[1].sample.x, 0.5), "matrixmap bounded points grid middle x");
    require(nearly_equal(matrix_map.fixtures[1].sample.y, 0.1), "matrixmap bounded points grid middle y");
    require(nearly_equal(matrix_map.fixtures[5].sample.x, 0.8), "matrixmap bounded points grid last x");
    require(nearly_equal(matrix_map.fixtures[5].sample.y, 0.9), "matrixmap bounded points grid last y");
    require(nearly_equal(matrix_map.fixtures[5].sample.width, 0.0), "matrixmap bounded points grid has no sample width");
    require(nearly_equal(matrix_map.fixtures[5].sample.height, 0.0), "matrixmap bounded points grid has no sample height");

    map_result = bbb::dmx::matrixmap::parse_matrix_map_text(R"json({
        "schema": "bbb.dmx.matrixmap.v1",
        "grid": {
            "fixture_pattern": "reverse_%02d",
            "cols": 2,
            "rows": 2,
            "x0": 0.8,
            "y0": 0.0,
            "x1": 0.2,
            "y1": 1.0,
            "placement": "cell",
            "params": { "red": "r" }
        }
    })json", matrix_map);
    require(map_result.ok, "matrixmap grid accepts reversed x bounds");
    require(matrix_map.fixtures.size() == 4, "matrixmap reversed grid expands count");
    require(nearly_equal(matrix_map.fixtures[0].sample.x, 0.65), "matrixmap reversed grid first x center");
    require(nearly_equal(matrix_map.fixtures[1].sample.x, 0.35), "matrixmap reversed grid second x center");
    require(nearly_equal(matrix_map.fixtures[0].sample.width, 0.3), "matrixmap reversed grid uses positive cell width");
    require(nearly_equal(matrix_map.fixtures[0].sample.height, 0.5), "matrixmap reversed grid uses positive cell height");

    map_result = bbb::dmx::matrixmap::parse_matrix_map_text(R"json({
        "schema": "bbb.dmx.matrixmap.v1",
        "grid": {
            "fixture_pattern": "flat_%02d",
            "cols": 3,
            "rows": 2,
            "x0": 0.4,
            "y0": 0.6,
            "x1": 0.4,
            "y1": 0.6,
            "placement": "points",
            "params": { "red": "r" }
        }
    })json", matrix_map);
    require(map_result.ok, "matrixmap grid accepts zero-area bounds");
    require(matrix_map.fixtures.size() == 6, "matrixmap zero-area grid expands count");
    require(nearly_equal(matrix_map.fixtures[0].sample.x, 0.4), "matrixmap zero-area grid first x");
    require(nearly_equal(matrix_map.fixtures[0].sample.y, 0.6), "matrixmap zero-area grid first y");
    require(nearly_equal(matrix_map.fixtures[5].sample.x, 0.4), "matrixmap zero-area grid last x");
    require(nearly_equal(matrix_map.fixtures[5].sample.y, 0.6), "matrixmap zero-area grid last y");

    std::vector<float> float32_rgb_noise(100 * 100 * 3, 0.0f);
    const long sample_x{50};
    const long sample_y{50};
    const std::size_t sample_index{(std::size_t)((sample_y * 100 + sample_x) * 3)};
    float32_rgb_noise[sample_index + 0] = 0.25f;
    float32_rgb_noise[sample_index + 1] = 0.5f;
    float32_rgb_noise[sample_index + 2] = 0.75f;
    bbb::dmx::matrixmap::matrix_read_view float32_rgb_view{};
    float32_rgb_view.data = (const char *)float32_rgb_noise.data();
    float32_rgb_view.width = 100;
    float32_rgb_view.height = 100;
    float32_rgb_view.plane_count = 3;
    float32_rgb_view.stride_x = 3 * (long)sizeof(float);
    float32_rgb_view.stride_y = 100 * float32_rgb_view.stride_x;
    float32_rgb_view.plane_order = bbb::dmx::matrixmap::plane_order_kind::rgba;
    float32_rgb_view.value_kind = bbb::dmx::matrixmap::matrix_value_kind::float32;
    const bbb::dmx::matrixmap::color_value sampled_float32_rgb{bbb::dmx::matrixmap::sample_point(float32_rgb_view, 0.5, 0.5)};
    require(nearly_equal(sampled_float32_rgb.red, 0.25), "matrixmap samples float32 three-plane red");
    require(nearly_equal(sampled_float32_rgb.green, 0.5), "matrixmap samples float32 three-plane green");
    require(nearly_equal(sampled_float32_rgb.blue, 0.75), "matrixmap samples float32 three-plane blue");
    require(nearly_equal(sampled_float32_rgb.alpha, 1.0), "matrixmap float32 three-plane alpha defaults to one");

    bbb::dmx::fixture_mode rgb_mode{make_semantic_color_mode({"red", "green", "blue"})};
    bbb::dmx::semantic_color_mapping color_mapping{bbb::dmx::semantic_color_parameters_for_mode(
        rgb_mode,
        bbb::dmx::make_semantic_color_request(1.2, 0.5, -0.1)
    )};
    require(color_mapping.ok, "semantic RGB mapping accepts RGB fixture");
    require(color_mapping.parameters.size() == 3, "semantic RGB mapping writes three parameters");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "red"), 1.0), "semantic RGB mapping clamps red");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "green"), 0.5), "semantic RGB mapping maps green");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "blue"), 0.0), "semantic RGB mapping clamps blue");

    bbb::dmx::fixture_mode jdc_color_mode{make_semantic_color_mode({"dimmer", "shutter", "shutter_2", "shutter_3", "dimmer_2", "red", "green", "blue"})};
    color_mapping = bbb::dmx::semantic_color_parameters_for_mode(
        jdc_color_mode,
        bbb::dmx::make_semantic_color_request(0.25, 0.5, 0.75)
    );
    require(color_mapping.ok, "semantic RGB mapping accepts RGB fixture with associated dimmer");
    require(color_mapping.parameters.size() == 4, "semantic RGB mapping writes RGB plus nearest associated dimmer");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "red"), 1.0 / 3.0), "semantic RGB mapping normalizes red through associated dimmer");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "green"), 2.0 / 3.0), "semantic RGB mapping normalizes green through associated dimmer");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "blue"), 1.0), "semantic RGB mapping normalizes blue through associated dimmer");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "dimmer_2"), 0.75), "semantic RGB mapping opens nearest color dimmer instead of earlier strobe dimmer");
    require(find_semantic_parameter(color_mapping, "dimmer") == nullptr, "semantic RGB mapping does not open earlier strobe dimmer");
    bbb::dmx::semantic_color_mapping intensity_mapping{bbb::dmx::semantic_intensity_parameters_for_mode(jdc_color_mode, 0.4)};
    require(intensity_mapping.ok, "semantic intensity accepts RGB fixture with associated dimmers");
    require(intensity_mapping.parameters.size() == 2, "semantic intensity writes all intensity dimmers");
    require(nearly_equal(*find_semantic_parameter(intensity_mapping, "dimmer"), 0.4), "semantic intensity maps earlier master dimmer");
    require(nearly_equal(*find_semantic_parameter(intensity_mapping, "dimmer_2"), 0.4), "semantic intensity maps nearest color dimmer");

    bbb::dmx::fixture_mode jdc_multi_color_mode{make_semantic_color_mode({"dimmer", "shutter", "shutter_2", "shutter_3", "dimmer_2", "red", "green", "blue", "dimmer_3", "red_2", "green_2", "blue_2"})};
    color_mapping = bbb::dmx::semantic_color_parameters_for_mode(
        jdc_multi_color_mode,
        bbb::dmx::make_semantic_color_request(0.25, 0.5, 0.75)
    );
    require(color_mapping.ok, "semantic RGB mapping accepts multi-block RGB fixture");
    require(color_mapping.parameters.size() == 8, "semantic RGB mapping writes both RGB blocks plus associated dimmers");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "red"), 1.0 / 3.0), "semantic RGB mapping normalizes first red block");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "green"), 2.0 / 3.0), "semantic RGB mapping normalizes first green block");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "blue"), 1.0), "semantic RGB mapping normalizes first blue block");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "red_2"), 1.0 / 3.0), "semantic RGB mapping normalizes second red block");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "green_2"), 2.0 / 3.0), "semantic RGB mapping normalizes second green block");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "blue_2"), 1.0), "semantic RGB mapping normalizes second blue block");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "dimmer_2"), 0.75), "semantic RGB mapping opens first associated color dimmer");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "dimmer_3"), 0.75), "semantic RGB mapping opens second associated color dimmer");
    require(find_semantic_parameter(color_mapping, "dimmer") == nullptr, "semantic RGB mapping still does not open earlier master dimmer");

    intensity_mapping = bbb::dmx::semantic_intensity_parameters_for_mode(jdc_multi_color_mode, 0.4);
    require(intensity_mapping.ok, "semantic intensity accepts multi-dimmer fixture");
    require(intensity_mapping.parameters.size() == 3, "semantic intensity writes all JDC dimmers");
    require(nearly_equal(*find_semantic_parameter(intensity_mapping, "dimmer"), 0.4), "semantic intensity maps JDC master dimmer");
    require(nearly_equal(*find_semantic_parameter(intensity_mapping, "dimmer_2"), 0.4), "semantic intensity maps JDC first color dimmer");
    require(nearly_equal(*find_semantic_parameter(intensity_mapping, "dimmer_3"), 0.4), "semantic intensity maps JDC second color dimmer");

    bbb::dmx::fixture_semantic_overrides semantic_overrides{};
    bbb::dmx::mapper_result overrides_result{bbb::dmx::parse_fixture_semantic_overrides_text(R"json({
        "schema": "bbb.dmx.semantic_overrides.v1",
        "profiles": {
            "test.profile": {
                "modes": {
                    "weird.mode": {
                        "aliases": {
                            "plate_dimmer": "plate_i"
                        },
                        "intensity": {
                            "primary": "master",
                            "parameters": ["master", "plate_i", "beam_i"]
                        },
                        "color": {
                            "rgb": [
                                {"red": "plate_r", "green": "plate_g", "blue": "plate_b", "dimmer": "plate_i"},
                                {"red": "beam_r", "green": "beam_g", "blue": "beam_b", "dimmer": "beam_i"}
                            ]
                        }
                    }
                }
            }
        }
    })json", semantic_overrides)};
    require(overrides_result.ok, "semantic overrides JSON parses");
    const bbb::dmx::fixture_semantic_mode_override *mode_override{semantic_overrides.find_mode_override("test.profile", "weird.mode")};
    require(mode_override != nullptr, "semantic overrides find profile/mode");
    require(mode_override->resolve_alias("plate_dimmer") == "plate_i", "semantic overrides resolve parameter alias");
    require(mode_override->resolve_alias("unknown") == "unknown", "semantic overrides leave unknown aliases unchanged");

    bbb::dmx::fixture_mode weird_color_mode{make_semantic_color_mode({"master", "plate_r", "plate_g", "plate_b", "plate_i", "beam_r", "beam_g", "beam_b", "beam_i"})};
    color_mapping = bbb::dmx::semantic_color_parameters_for_mode(
        nullptr,
        weird_color_mode,
        bbb::dmx::make_semantic_color_request(0.2, 0.4, 0.8),
        bbb::dmx::semantic_color_options{true, false},
        {},
        mode_override
    );
    require(color_mapping.ok, "semantic overrides map custom RGB blocks");
    require(color_mapping.parameters.size() == 8, "semantic overrides write two RGB blocks and dimmers");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "plate_r"), 0.25), "semantic overrides normalize first custom red");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "plate_g"), 0.5), "semantic overrides normalize first custom green");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "plate_b"), 1.0), "semantic overrides normalize first custom blue");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "plate_i"), 0.8), "semantic overrides open first custom dimmer");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "beam_r"), 0.25), "semantic overrides normalize second custom red");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "beam_g"), 0.5), "semantic overrides normalize second custom green");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "beam_b"), 1.0), "semantic overrides normalize second custom blue");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "beam_i"), 0.8), "semantic overrides open second custom dimmer");
    require(find_semantic_parameter(color_mapping, "master") == nullptr, "semantic overrides color does not implicitly open master dimmer");

    intensity_mapping = bbb::dmx::semantic_intensity_parameters_for_mode(weird_color_mode, 0.6, mode_override);
    require(intensity_mapping.ok, "semantic overrides map explicit intensity parameters");
    require(intensity_mapping.parameters.size() == 3, "semantic overrides write explicit intensity list");
    require(nearly_equal(*find_semantic_parameter(intensity_mapping, "master"), 0.6), "semantic overrides map master intensity");
    require(nearly_equal(*find_semantic_parameter(intensity_mapping, "plate_i"), 0.6), "semantic overrides map plate intensity");
    require(nearly_equal(*find_semantic_parameter(intensity_mapping, "beam_i"), 0.6), "semantic overrides map beam intensity");
    bbb::dmx::semantic_color_mapping primary_intensity_mapping{bbb::dmx::semantic_primary_intensity_parameter_for_mode(weird_color_mode, 0.7, mode_override)};
    require(primary_intensity_mapping.ok, "semantic overrides map primary intensity");
    require(primary_intensity_mapping.parameters.size() == 1, "semantic overrides primary intensity writes one parameter");
    require(nearly_equal(*find_semantic_parameter(primary_intensity_mapping, "master"), 0.7), "semantic overrides use explicit primary intensity");

    bbb::dmx::fixture_mode dimmer_curve_mode{make_semantic_color_mode({"dimmer", "dimmer_2"})};
    dimmer_curve_mode.parameters[1].ranges = {
        make_parameter_range(0, 4, "smooth.gamma.corrected", "Smooth Gamma Corrected"),
        make_parameter_range(60, 255, "reserved", "Reserved")
    };
    intensity_mapping = bbb::dmx::semantic_intensity_parameters_for_mode(dimmer_curve_mode, 0.4);
    require(intensity_mapping.ok, "semantic intensity accepts dimmer fixture with dimmer curve channel");
    require(intensity_mapping.parameters.size() == 1, "semantic intensity skips non-intensity dimmer curve parameter");
    require(nearly_equal(*find_semantic_parameter(intensity_mapping, "dimmer"), 0.4), "semantic intensity keeps real dimmer with dimmer curve present");
    require(find_semantic_parameter(intensity_mapping, "dimmer_2") == nullptr, "semantic intensity does not write dimmer curve parameter");

    bbb::dmx::fixture_mode rgbw_mode{make_semantic_color_mode({"red", "green", "blue", "white"})};
    color_mapping = bbb::dmx::semantic_color_parameters_for_mode(
        rgbw_mode,
        bbb::dmx::make_semantic_color_request(1.0, 0.75, 0.25),
        bbb::dmx::semantic_color_options{true}
    );
    require(color_mapping.ok, "semantic RGBW mapping accepts RGBW fixture");
    require(color_mapping.parameters.size() == 4, "semantic RGBW mapping writes white when enabled");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "red"), 0.75), "semantic RGBW mapping subtracts white from red");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "green"), 0.5), "semantic RGBW mapping subtracts white from green");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "blue"), 0.0), "semantic RGBW mapping subtracts white from blue");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "white"), 0.25), "semantic RGBW mapping extracts white");

    color_mapping = bbb::dmx::semantic_color_parameters_for_mode(
        rgbw_mode,
        bbb::dmx::make_semantic_color_request(1.0, 0.75, 0.25),
        bbb::dmx::semantic_color_options{false}
    );
    require(color_mapping.ok, "semantic RGBW mapping accepts disabled white mode");
    require(color_mapping.parameters.size() == 3, "semantic RGBW mapping leaves white untouched when disabled");
    require(find_semantic_parameter(color_mapping, "white") == nullptr, "semantic RGBW disabled mode does not emit white parameter");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "red"), 1.0), "semantic RGBW disabled mode keeps full red");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "green"), 0.75), "semantic RGBW disabled mode keeps full green");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "blue"), 0.25), "semantic RGBW disabled mode keeps full blue");

    bbb::dmx::fixture_mode robe_color_mode{make_semantic_color_mode({"red", "green", "blue", "white", "colormixmode"})};
    robe_color_mode.parameters.back().ranges = {
        make_parameter_range(40, 49, "addition.mode.virtual.colour.mix", "Addition mode (Virtual + Colour mix)"),
        make_parameter_range(255, 255, "colour.channels.colour.mix.has.priority", "Colour channels (Colour mix has priority)")
    };
    color_mapping = bbb::dmx::semantic_color_parameters_for_mode(
        robe_color_mode,
        bbb::dmx::make_semantic_color_request(1.0, 0.0, 0.0)
    );
    require(color_mapping.ok, "semantic RGBW mapping accepts Robe-style color mix mode");
    require(color_mapping.parameters.size() == 5, "semantic RGBW mapping writes color mix priority mode");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "red"), 1.0), "semantic RGBW color mix mode keeps requested red");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "green"), 0.0), "semantic RGBW color mix mode clears green");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "blue"), 0.0), "semantic RGBW color mix mode clears blue");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "white"), 0.0), "semantic RGBW color mix mode clears white for saturated red");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "colormixmode"), 1.0), "semantic RGBW mapping selects colour mix priority");

    bbb::dmx::fixture_mode cmy_mode{make_semantic_color_mode({"cyan", "magenta", "yellow"})};
    color_mapping = bbb::dmx::semantic_color_parameters_for_mode(
        cmy_mode,
        bbb::dmx::make_semantic_color_request(1.0, 0.25, 0.0)
    );
    require(color_mapping.ok, "semantic CMY mapping accepts CMY fixture");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "cyan"), 0.0), "semantic CMY mapping inverts red to cyan");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "magenta"), 0.75), "semantic CMY mapping inverts green to magenta");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "yellow"), 1.0), "semantic CMY mapping inverts blue to yellow");

    bbb::dmx::fixture_mode dimmer_only_mode{make_semantic_color_mode({"dimmer"})};
    color_mapping = bbb::dmx::semantic_color_parameters_for_mode(
        dimmer_only_mode,
        bbb::dmx::make_semantic_color_request(1.0, 1.0, 1.0)
    );
    require(!color_mapping.ok, "semantic color mapping rejects unsupported fixture");

    bbb::dmx::fixture_profile wheel_profile{};
    wheel_profile.key = "test.colorwheel";
    bbb::dmx::fixture_wheel color_wheel{};
    color_wheel.id = "ColorWheel1";
    color_wheel.type = "color";
    color_wheel.slots = {
        bbb::dmx::fixture_wheel_slot{1, "open", "Open", "open", "", "", bbb::dmx::fixture_wheel_slot_color{false, 0.0, 0.0, 0.0, true, 255, 255, 255}},
        bbb::dmx::fixture_wheel_slot{2, "red", "Red", "color", "", "", bbb::dmx::fixture_wheel_slot_color{false, 0.0, 0.0, 0.0, true, 255, 0, 0}},
        bbb::dmx::fixture_wheel_slot{3, "blue", "Blue", "color", "", "", bbb::dmx::fixture_wheel_slot_color{false, 0.0, 0.0, 0.0, true, 0, 0, 255}}
    };
    bbb::dmx::fixture_mode wheel_mode{};
    wheel_mode.key = "wheel";
    wheel_mode.footprint = 2;
    wheel_mode.channels = {
        make_u8_channel(1, "color_wheel"),
        make_u8_channel(2, "dimmer")
    };
    bbb::dmx::fixture_parameter wheel_parameter{make_u8_parameter("color_wheel")};
    wheel_parameter.type = bbb::dmx::fixture_parameter_type::enum_u8;
    wheel_parameter.channels = {"color_wheel"};
    wheel_parameter.wheel = "ColorWheel1";
    wheel_parameter.ranges = {
        make_parameter_range(0, 9, "open", "Open"),
        make_parameter_range(10, 19, "red", "Red"),
        make_parameter_range(20, 29, "blue", "Blue"),
        make_parameter_range(100, 109, "open", "Open")
    };
    wheel_parameter.ranges[0].has_wheel_slot = true;
    wheel_parameter.ranges[0].wheel_slot = 1;
    wheel_parameter.ranges[1].has_wheel_slot = true;
    wheel_parameter.ranges[1].wheel_slot = 2;
    wheel_parameter.ranges[2].has_wheel_slot = true;
    wheel_parameter.ranges[2].wheel_slot = 3;
    wheel_parameter.ranges[3].has_wheel_slot = true;
    wheel_parameter.ranges[3].wheel_slot = 1;
    bbb::dmx::fixture_parameter gobo_parameter{make_u8_parameter("gobo")};
    gobo_parameter.type = bbb::dmx::fixture_parameter_type::enum_u8;
    gobo_parameter.channels = {"gobo"};
    gobo_parameter.wheel = "GoboWheel1";
    gobo_parameter.ranges = {
        make_parameter_range(0, 9, "open", "Open"),
        make_parameter_range(10, 19, "gobo.1", "Gobo 1")
    };
    gobo_parameter.ranges[0].has_wheel_slot = true;
    gobo_parameter.ranges[0].wheel_slot = 1;
    bbb::dmx::fixture_parameter prism_parameter{make_u8_parameter("prism")};
    prism_parameter.type = bbb::dmx::fixture_parameter_type::enum_u8;
    prism_parameter.channels = {"prism"};
    prism_parameter.wheel = "PrismWheel1";
    prism_parameter.ranges = {
        make_parameter_range(0, 19, "open", "Open position"),
        make_parameter_range(20, 49, "prism.1", "Prism 1")
    };
    prism_parameter.ranges[0].has_wheel_slot = true;
    prism_parameter.ranges[0].wheel_slot = 1;
    wheel_mode.channels.push_back(make_u8_channel(3, "gobo"));
    wheel_mode.channels.push_back(make_u8_channel(4, "prism"));
    wheel_mode.footprint = 4;
    wheel_mode.parameters = {wheel_parameter, gobo_parameter, prism_parameter, make_u8_parameter("dimmer")};
    wheel_profile.wheels = {color_wheel};
    wheel_profile.modes = {wheel_mode};
    color_mapping = bbb::dmx::semantic_color_parameters_for_mode(
        &wheel_profile,
        wheel_mode,
        bbb::dmx::make_semantic_color_request(1.0, 0.0, 0.0),
        bbb::dmx::semantic_color_options{true, false}
    );
    require(!color_mapping.ok, "semantic color wheel fallback is opt-in");
    color_mapping = bbb::dmx::semantic_color_parameters_for_mode(
        &wheel_profile,
        wheel_mode,
        bbb::dmx::make_semantic_color_request(1.0, 0.0, 0.0),
        bbb::dmx::semantic_color_options{true, true}
    );
    require(color_mapping.ok, "semantic color wheel fallback maps nearest slot");
    require(color_mapping.parameters.size() == 2, "semantic color wheel fallback writes color wheel and dimmer");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "color_wheel"), 14.0 / 255.0), "semantic color wheel fallback uses range center");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "dimmer"), 1.0), "semantic color wheel fallback opens dimmer for full red");
    color_mapping = bbb::dmx::semantic_color_parameters_for_mode(
        &wheel_profile,
        wheel_mode,
        bbb::dmx::make_semantic_color_request(0.5, 0.0, 0.0),
        bbb::dmx::semantic_color_options{true, true}
    );
    require(color_mapping.ok, "semantic color wheel fallback maps dimmed red");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "color_wheel"), 14.0 / 255.0), "semantic color wheel fallback uses normalized hue for dimmed red");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "dimmer"), 0.5), "semantic color wheel fallback maps brightness to dimmer");
    color_mapping = bbb::dmx::semantic_color_parameters_for_mode(
        &wheel_profile,
        wheel_mode,
        bbb::dmx::make_semantic_color_request(1.0, 1.0, 1.0),
        bbb::dmx::semantic_color_options{true, true}
    );
    require(color_mapping.ok, "semantic color wheel fallback maps white to open");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "color_wheel"), 0.0), "semantic color wheel fallback uses lowest open value without current value");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "dimmer"), 1.0), "semantic color wheel fallback opens dimmer for white");
    color_mapping = bbb::dmx::semantic_color_parameters_for_mode(
        &wheel_profile,
        wheel_mode,
        bbb::dmx::make_semantic_color_request(1.0, 1.0, 1.0),
        bbb::dmx::semantic_color_options{true, true},
        {{"color_wheel", 105}}
    );
    require(color_mapping.ok, "semantic color wheel fallback maps white with current value");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "color_wheel"), 0.0), "semantic color wheel fallback ignores current value and uses lowest open");
    color_mapping = bbb::dmx::semantic_color_parameters_for_mode(
        &wheel_profile,
        wheel_mode,
        bbb::dmx::make_semantic_color_request(1.0, 1.0, 1.0),
        bbb::dmx::semantic_color_options{true, true},
        {{"gobo", 5}, {"prism", 9}}
    );
    require(color_mapping.ok, "semantic color wheel fallback ignores non-color wheels");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "color_wheel"), 0.0), "semantic color wheel fallback uses lowest color wheel open when gobo or prism is nearer");
    require(find_semantic_parameter(color_mapping, "gobo") == nullptr, "semantic color wheel fallback does not write gobo wheels");
    require(find_semantic_parameter(color_mapping, "prism") == nullptr, "semantic color wheel fallback does not write prism wheels");
    color_mapping = bbb::dmx::semantic_color_parameters_for_mode(
        &wheel_profile,
        wheel_mode,
        bbb::dmx::make_semantic_color_request(0.0, 0.0, 0.0),
        bbb::dmx::semantic_color_options{true, true},
        {{"color_wheel", 105}}
    );
    require(color_mapping.ok, "semantic color wheel fallback maps black to open");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "color_wheel"), 0.0), "semantic color wheel fallback maps black to lowest open");
    require(nearly_equal(*find_semantic_parameter(color_mapping, "dimmer"), 0.0), "semantic color wheel fallback maps black to dimmer closed");

    bbb::dmx::fixture_mode shutter_mode{make_semantic_shutter_mode("shutter", "shutter")};
    bbb::dmx::semantic_shutter_mapping shutter_mapping{bbb::dmx::semantic_shutter_parameter_for_mode(shutter_mode, true)};
    require(shutter_mapping.ok, "semantic shutter mapping accepts shutter ranges");
    require(shutter_mapping.parameter == "shutter", "semantic shutter mapping selects shutter parameter");
    require(shutter_mapping.value == 47, "semantic shutter open uses center of open range");
    shutter_mapping = bbb::dmx::semantic_shutter_parameter_for_mode(shutter_mode, false);
    require(shutter_mapping.ok, "semantic shutter mapping accepts closed ranges");
    require(shutter_mapping.value == 15, "semantic shutter close uses center of closed range");

    bbb::dmx::fixture_mode shutter_label_mode{make_semantic_shutter_mode("shutter", "shutter")};
    shutter_label_mode.parameters[0].ranges = {
        make_parameter_range(0, 31, "", "Shutter closed"),
        make_parameter_range(32, 63, "", "Shutter open"),
        make_parameter_range(64, 91, "", "Slow to fast 1/10"),
        make_parameter_range(92, 95, "", "Slow to fast 10/10"),
        make_parameter_range(96, 127, "", "Open")
    };
    shutter_mapping = bbb::dmx::semantic_shutter_parameter_for_mode(shutter_label_mode, false);
    require(shutter_mapping.ok, "semantic shutter mapping accepts prefixed closed labels");
    require(shutter_mapping.value == 15, "semantic shutter close uses Shutter closed range");
    shutter_mapping = bbb::dmx::semantic_shutter_parameter_for_mode(shutter_label_mode, true);
    require(shutter_mapping.ok, "semantic shutter mapping accepts prefixed open labels");
    require(shutter_mapping.value == 47, "semantic shutter open prefers Shutter open before later Open range");

    bbb::dmx::fixture_mode conflicting_shutter_label_mode{make_semantic_shutter_mode("shutter", "shutter")};
    conflicting_shutter_label_mode.parameters[0].ranges = {
        make_parameter_range(0, 31, "open", "Shutter closed"),
        make_parameter_range(32, 63, "closed", "Shutter open")
    };
    shutter_mapping = bbb::dmx::semantic_shutter_parameter_for_mode(conflicting_shutter_label_mode, false);
    require(shutter_mapping.ok, "semantic shutter mapping accepts conflicting function labels for closed state");
    require(shutter_mapping.value == 15, "semantic shutter closed uses label over contradictory function");
    shutter_mapping = bbb::dmx::semantic_shutter_parameter_for_mode(conflicting_shutter_label_mode, true);
    require(shutter_mapping.ok, "semantic shutter mapping accepts conflicting function labels for open state");
    require(shutter_mapping.value == 47, "semantic shutter open uses label over contradictory function");

    bbb::dmx::fixture_mode strobe_only_mode{make_semantic_shutter_mode("strobe", "shutter_strobe")};
    shutter_mapping = bbb::dmx::semantic_shutter_parameter_for_mode(strobe_only_mode, true);
    require(shutter_mapping.ok, "semantic shutter mapping accepts strobe-only shared channel ranges");
    require(shutter_mapping.parameter == "strobe", "semantic shutter mapping can target strobe parameter on shared shutter channel");
    require(shutter_mapping.value == 47, "semantic shutter strobe-only open uses open range");

    bbb::dmx::fixture_mode fallback_shutter_mode{};
    fallback_shutter_mode.key = "fallback-shutter";
    fallback_shutter_mode.footprint = 2;
    fallback_shutter_mode.channels = {
        make_u8_channel(1, "shutter.coarse"),
        make_u8_channel(2, "shutter.fine")
    };
    bbb::dmx::fixture_parameter fallback_shutter_parameter{};
    fallback_shutter_parameter.key = "shutter-strobe";
    fallback_shutter_parameter.type = bbb::dmx::fixture_parameter_type::u16;
    fallback_shutter_parameter.channels = {"shutter.coarse", "shutter.fine"};
    fallback_shutter_mode.parameters = {fallback_shutter_parameter};
    shutter_mapping = bbb::dmx::semantic_shutter_parameter_for_mode(fallback_shutter_mode, true);
    require(shutter_mapping.ok, "semantic shutter fallback accepts likely shutter-strobe parameter");
    require(shutter_mapping.value == 65535, "semantic shutter fallback opens u16 at max");
    shutter_mapping = bbb::dmx::semantic_shutter_parameter_for_mode(fallback_shutter_mode, false);
    require(shutter_mapping.ok, "semantic shutter fallback closes likely shutter-strobe parameter");
    require(shutter_mapping.value == 0, "semantic shutter fallback closes at zero");

    bbb::dmx::fixture_mode no_shutter_mode{make_semantic_color_mode({"dimmer"})};
    shutter_mapping = bbb::dmx::semantic_shutter_parameter_for_mode(no_shutter_mode, true);
    require(!shutter_mapping.ok, "semantic shutter mapping rejects unsupported fixture");

    bbb::dmx::fixture_mode unrelated_open_range_mode{make_semantic_shutter_mode("gobo", "gobo")};
    shutter_mapping = bbb::dmx::semantic_shutter_parameter_for_mode(unrelated_open_range_mode, true);
    require(!shutter_mapping.ok, "semantic shutter mapping ignores unrelated open ranges");

    bbb::dmx::fixture_mode shared_channel_mode{};
    shared_channel_mode.key = "shared-shutter-strobe";
    shared_channel_mode.footprint = 1;
    shared_channel_mode.channels = {make_u8_channel(1, "shutter_strobe")};
    bbb::dmx::fixture_parameter shared_shutter_parameter{make_u8_parameter("shutter")};
    shared_shutter_parameter.channels = {"shutter_strobe"};
    shared_shutter_parameter.ranges = {
        make_parameter_range(0, 31, "closed", "Closed"),
        make_parameter_range(32, 63, "open", "Open")
    };
    bbb::dmx::fixture_parameter shared_strobe_parameter{make_u8_parameter("strobe")};
    shared_strobe_parameter.channels = {"shutter_strobe"};
    shared_channel_mode.parameters = {shared_shutter_parameter, shared_strobe_parameter};
    bbb::dmx::fixture_profile shared_channel_profile{};
    shared_channel_profile.key = "test.shutter.shared";
    shared_channel_profile.modes = {shared_channel_mode};
    bbb::dmx::fixture_instance shared_channel_fixture{};
    shared_channel_fixture.id = "shutter_01";
    shared_channel_fixture.profile = "test.shutter.shared";
    shared_channel_fixture.mode = "shared-shutter-strobe";
    shared_channel_fixture.universe = 1;
    shared_channel_fixture.address = 1;
    bbb::dmx::fixture_patch shared_channel_patch{};
    shared_channel_patch.fixtures = {shared_channel_fixture};
    bbb::dmx::fixture_mapper shared_channel_mapper{};
    map_result = shared_channel_mapper.add_profile(shared_channel_profile);
    require(map_result.ok, "semantic shutter shared channel mapper accepts profile");
    map_result = shared_channel_mapper.set_patch(shared_channel_patch);
    require(map_result.ok, "semantic shutter shared channel mapper accepts patch");
    map_result = shared_channel_mapper.set_u8("shutter_01", "strobe", 99);
    require(map_result.ok, "semantic shutter shared channel mapper seeds strobe");
    require(shared_channel_mapper.universe(1).channel(1) == 99, "semantic shutter shared channel seed writes strobe channel");
    shutter_mapping = bbb::dmx::semantic_shutter_parameter_for_mode(shared_channel_mode, true);
    require(shutter_mapping.ok, "semantic shutter shared channel produces open mapping");
    map_result = shared_channel_mapper.set_u8("shutter_01", shutter_mapping.parameter, shutter_mapping.value);
    require(map_result.ok, "semantic shutter shared channel applies open mapping");
    require(shared_channel_mapper.universe(1).channel(1) == 47, "semantic shutter open overwrites shared strobe channel");

    bbb::dmx::fixture_mode jdc_strobe_mode{};
    jdc_strobe_mode.key = "jdc";
    jdc_strobe_mode.footprint = 20;
    jdc_strobe_mode.channels = {
        make_labeled_u8_channel(3, "dimmer", "Dimmer"),
        make_labeled_u8_channel(4, "shutter", "StrobeDuration"),
        make_labeled_u8_channel(5, "shutter_2", "StrobeRate", 255),
        make_labeled_u8_channel(6, "shutter_3", "StrobeModeStrobe"),
        make_labeled_u8_channel(8, "dimmer_2", "Dimmer"),
        make_labeled_u8_channel(9, "red", "ColorAdd_R", 255),
        make_labeled_u8_channel(10, "green", "ColorAdd_G", 255),
        make_labeled_u8_channel(11, "blue", "ColorAdd_B", 255),
        make_labeled_u8_channel(12, "shutter_4", "StrobeDuration"),
        make_labeled_u8_channel(13, "shutter_5", "StrobeRate", 255),
        make_labeled_u8_channel(14, "shutter_6", "StrobeModeStrobe"),
        make_labeled_u8_channel(20, "dimmer_3", "Dimmer")
    };
    bbb::dmx::fixture_parameter jdc_dimmer{make_u8_parameter("dimmer")};
    jdc_dimmer.channels = {"dimmer"};
    jdc_dimmer.ranges = {
        make_parameter_range(0, 0, "closed", "Closed"),
        make_parameter_range(1, 254, "dimmer", "Dimmer"),
        make_parameter_range(255, 255, "open", "Open")
    };
    bbb::dmx::fixture_parameter jdc_duration{make_u8_parameter("shutter")};
    jdc_duration.channels = {"shutter"};
    jdc_duration.ranges = {make_parameter_range(0, 255, "strobe", "Duration")};
    bbb::dmx::fixture_parameter jdc_rate{make_u8_parameter("shutter_2")};
    jdc_rate.channels = {"shutter_2"};
    jdc_rate.default_value = 255;
    jdc_rate.ranges = {make_parameter_range(0, 255, "strobe", "Rate")};
    bbb::dmx::fixture_parameter jdc_mode{make_u8_parameter("shutter_3")};
    jdc_mode.channels = {"shutter_3"};
    jdc_mode.ranges = {
        make_parameter_range(0, 0, "strobe", "No effect"),
        make_parameter_range(1, 36, "strobe", "No Effect"),
        make_parameter_range(37, 40, "strobe", "Ramp up")
    };
    bbb::dmx::fixture_parameter jdc_dimmer_2{make_u8_parameter("dimmer_2")};
    jdc_dimmer_2.channels = {"dimmer_2"};
    jdc_dimmer_2.ranges = jdc_dimmer.ranges;
    bbb::dmx::fixture_parameter jdc_red{make_u8_parameter("red")};
    jdc_red.channels = {"red"};
    bbb::dmx::fixture_parameter jdc_green{make_u8_parameter("green")};
    jdc_green.channels = {"green"};
    bbb::dmx::fixture_parameter jdc_blue{make_u8_parameter("blue")};
    jdc_blue.channels = {"blue"};
    bbb::dmx::fixture_parameter jdc_duration_2{make_u8_parameter("shutter_4")};
    jdc_duration_2.channels = {"shutter_4"};
    jdc_duration_2.ranges = {make_parameter_range(0, 255, "strobe", "Duration")};
    bbb::dmx::fixture_parameter jdc_rate_2{make_u8_parameter("shutter_5")};
    jdc_rate_2.channels = {"shutter_5"};
    jdc_rate_2.default_value = 255;
    jdc_rate_2.ranges = {make_parameter_range(0, 255, "strobe", "Rate")};
    bbb::dmx::fixture_parameter jdc_mode_2{make_u8_parameter("shutter_6")};
    jdc_mode_2.channels = {"shutter_6"};
    jdc_mode_2.ranges = {
        make_parameter_range(0, 0, "strobe", "No effect"),
        make_parameter_range(1, 36, "strobe", "Beam effect offset"),
        make_parameter_range(37, 40, "strobe", "Ramp up")
    };
    bbb::dmx::fixture_parameter jdc_dimmer_3{make_u8_parameter("dimmer_3")};
    jdc_dimmer_3.channels = {"dimmer_3"};
    jdc_dimmer_3.ranges = jdc_dimmer.ranges;
    jdc_strobe_mode.parameters = {jdc_dimmer, jdc_duration, jdc_rate, jdc_mode, jdc_dimmer_2, jdc_red, jdc_green, jdc_blue, jdc_duration_2, jdc_rate_2, jdc_mode_2, jdc_dimmer_3};
    const bbb::dmx::semantic_shutter_mappings jdc_open_mappings{bbb::dmx::semantic_shutter_parameters_for_mode(jdc_strobe_mode, true)};
    require(jdc_open_mappings.ok, "semantic shutter accepts GDTF strobe mode channels");
    const bbb::dmx::semantic_shutter_mapping *jdc_duration_mapping{find_shutter_mapping(jdc_open_mappings, "shutter")};
    require(jdc_duration_mapping != nullptr && jdc_duration_mapping->value == 0, "semantic shutter open resets StrobeDuration to default instead of max");
    const bbb::dmx::semantic_shutter_mapping *jdc_rate_mapping{find_shutter_mapping(jdc_open_mappings, "shutter_2")};
    require(jdc_rate_mapping != nullptr && jdc_rate_mapping->value == 255, "semantic shutter open resets StrobeRate to default");
    const bbb::dmx::semantic_shutter_mapping *jdc_duration_mapping_2{find_shutter_mapping(jdc_open_mappings, "shutter_4")};
    require(jdc_duration_mapping_2 != nullptr && jdc_duration_mapping_2->value == 0, "semantic shutter open resets second StrobeDuration to default instead of max");
    const bbb::dmx::semantic_shutter_mapping *jdc_rate_mapping_2{find_shutter_mapping(jdc_open_mappings, "shutter_5")};
    require(jdc_rate_mapping_2 != nullptr && jdc_rate_mapping_2->value == 255, "semantic shutter open resets second StrobeRate to default");
    const bbb::dmx::semantic_shutter_mapping *jdc_mode_mapping{find_shutter_mapping(jdc_open_mappings, "shutter_3")};
    require(jdc_mode_mapping != nullptr && jdc_mode_mapping->value == 0, "semantic shutter open resets StrobeModeStrobe to no effect");
    const bbb::dmx::semantic_shutter_mapping *jdc_mode_mapping_2{find_shutter_mapping(jdc_open_mappings, "shutter_6")};
    require(jdc_mode_mapping_2 != nullptr && jdc_mode_mapping_2->value == 0, "semantic shutter open resets second StrobeModeStrobe to no effect");

    const bbb::dmx::semantic_shutter_mappings jdc_closed_mappings{bbb::dmx::semantic_shutter_parameters_for_mode(jdc_strobe_mode, false)};
    require(jdc_closed_mappings.ok, "semantic shutter close accepts GDTF strobe mode channels");
    jdc_mode_mapping = find_shutter_mapping(jdc_closed_mappings, "shutter_3");
    require(jdc_mode_mapping != nullptr && jdc_mode_mapping->value == 0, "semantic shutter close resets StrobeModeStrobe to no effect");
    jdc_mode_mapping_2 = find_shutter_mapping(jdc_closed_mappings, "shutter_6");
    require(jdc_mode_mapping_2 != nullptr && jdc_mode_mapping_2->value == 0, "semantic shutter close resets second StrobeModeStrobe to no effect");
    require(find_shutter_mapping(jdc_closed_mappings, "dimmer") == nullptr, "semantic shutter close does not zero JDC strobe master dimmer");
    const bbb::dmx::semantic_shutter_mapping *jdc_closed_dimmer_mapping_2{find_shutter_mapping(jdc_closed_mappings, "dimmer_2")};
    require(jdc_closed_dimmer_mapping_2 != nullptr && jdc_closed_dimmer_mapping_2->value == 0, "semantic shutter close falls back to JDC semantic color dimmer only");
    require(find_shutter_mapping(jdc_closed_mappings, "dimmer_3") == nullptr, "semantic shutter close does not zero JDC pixel dimmers");
    require(find_shutter_mapping(jdc_open_mappings, "dimmer") == nullptr, "semantic shutter open does not force main dimmer open");

    const bbb::dmx::fixture_mode sgm_q8_simplified_mode{make_sgm_q8_simplified_mode()};
    const bbb::dmx::semantic_shutter_mappings sgm_q8_open_mappings{bbb::dmx::semantic_shutter_parameters_for_mode(sgm_q8_simplified_mode, true)};
    require(sgm_q8_open_mappings.ok, "semantic shutter accepts SGM Q-8 simplified strobe channels");
    const bbb::dmx::semantic_shutter_mapping *sgm_q8_duration_mapping{find_shutter_mapping(sgm_q8_open_mappings, "shutter")};
    require(sgm_q8_duration_mapping != nullptr && sgm_q8_duration_mapping->value == 0, "semantic shutter open resets SGM Q-8 StrobeDuration to default");
    const bbb::dmx::semantic_shutter_mapping *sgm_q8_rate_mapping{find_shutter_mapping(sgm_q8_open_mappings, "shutter_2")};
    require(sgm_q8_rate_mapping != nullptr && sgm_q8_rate_mapping->value == 0, "semantic shutter open resets SGM Q-8 StrobeRate to default");
    const bbb::dmx::semantic_shutter_mapping *sgm_q8_effect_mapping{find_shutter_mapping(sgm_q8_open_mappings, "shutter_3")};
    require(sgm_q8_effect_mapping != nullptr && sgm_q8_effect_mapping->value == 2, "semantic shutter open resets SGM Q-8 first strobe effect to no effect");
    const bbb::dmx::semantic_shutter_mapping *sgm_q8_duration_mapping_2{find_shutter_mapping(sgm_q8_open_mappings, "shutter_4")};
    require(sgm_q8_duration_mapping_2 != nullptr && sgm_q8_duration_mapping_2->value == 0, "semantic shutter open resets SGM Q-8 second StrobeDuration to default");
    const bbb::dmx::semantic_shutter_mapping *sgm_q8_rate_mapping_2{find_shutter_mapping(sgm_q8_open_mappings, "shutter_5")};
    require(sgm_q8_rate_mapping_2 != nullptr && sgm_q8_rate_mapping_2->value == 0, "semantic shutter open resets SGM Q-8 second StrobeRate to default");
    const bbb::dmx::semantic_shutter_mapping *sgm_q8_effect_mapping_2{find_shutter_mapping(sgm_q8_open_mappings, "shutter_6")};
    require(sgm_q8_effect_mapping_2 != nullptr && sgm_q8_effect_mapping_2->value == 4, "semantic shutter open resets SGM Q-8 second strobe effect to no effect");
    require(find_shutter_mapping(sgm_q8_open_mappings, "dimmer") == nullptr, "semantic shutter open does not force SGM Q-8 main dimmer open");
    require(find_shutter_mapping(sgm_q8_open_mappings, "dimmer_2") == nullptr, "semantic shutter open does not force SGM Q-8 second dimmer open");

    const bbb::dmx::semantic_shutter_mappings sgm_q8_closed_mappings{bbb::dmx::semantic_shutter_parameters_for_mode(sgm_q8_simplified_mode, false)};
    require(sgm_q8_closed_mappings.ok, "semantic shutter close accepts SGM Q-8 simplified strobe channels");
    const bbb::dmx::semantic_shutter_mapping *sgm_q8_closed_dimmer_mapping{find_shutter_mapping(sgm_q8_closed_mappings, "dimmer")};
    require(sgm_q8_closed_dimmer_mapping != nullptr && sgm_q8_closed_dimmer_mapping->value == 0, "semantic shutter close falls back to SGM Q-8 semantic color dimmer");
    require(find_shutter_mapping(sgm_q8_closed_mappings, "dimmer_2") == nullptr, "semantic shutter close does not zero unrelated SGM Q-8 dimmer");

    bbb::dmx::fixture_profile rgbw_profile{};
    rgbw_profile.key = "test.rgbw";
    rgbw_profile.modes = {rgbw_mode};
    bbb::dmx::fixture_instance rgbw_fixture{};
    rgbw_fixture.id = "rgbw_01";
    rgbw_fixture.profile = "test.rgbw";
    rgbw_fixture.mode = "color";
    rgbw_fixture.universe = 1;
    rgbw_fixture.address = 1;
    bbb::dmx::fixture_patch rgbw_patch{};
    rgbw_patch.fixtures = {rgbw_fixture};
    bbb::dmx::fixture_mapper rgbw_mapper{};
    map_result = rgbw_mapper.add_profile(rgbw_profile);
    require(map_result.ok, "semantic RGBW mapper accepts profile");
    map_result = rgbw_mapper.set_patch(rgbw_patch);
    require(map_result.ok, "semantic RGBW mapper accepts patch");
    map_result = rgbw_mapper.set_u8("rgbw_01", "white", 200);
    require(map_result.ok, "semantic RGBW mapper seeds white");
    color_mapping = bbb::dmx::semantic_color_parameters_for_mode(
        rgbw_mode,
        bbb::dmx::make_semantic_color_request(0.25, 0.5, 0.75),
        bbb::dmx::semantic_color_options{false}
    );
    for(const auto &parameter : color_mapping.parameters) {
        map_result = rgbw_mapper.set_normalized("rgbw_01", parameter.first, parameter.second);
        require(map_result.ok, "semantic RGBW mapper applies color parameter");
    }
    require(rgbw_mapper.universe(1).channel(1) == 64, "semantic RGBW mapper writes red with white disabled");
    require(rgbw_mapper.universe(1).channel(2) == 128, "semantic RGBW mapper writes green with white disabled");
    require(rgbw_mapper.universe(1).channel(3) == 191, "semantic RGBW mapper writes blue with white disabled");
    require(rgbw_mapper.universe(1).channel(4) == 200, "semantic RGBW mapper leaves white untouched when disabled");

    std::cout << "bbb_dmx_common_tests passed" << std::endl;
    return 0;
}
