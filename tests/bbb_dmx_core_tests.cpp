#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>

#include "bbb/dmx/common.hpp"
#include "bbb/dmx/curve.hpp"
#include "bbb/dmx/mask.hpp"
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

    const std::string patch_json{R"json({
        "schema": "bbb.dmx.patch.v2",
        "coordinates": "gdtf",
        "profiles": ["fixtures/generic.json.mover.json"],
        "fixtures": [
            {
                "id": "json_spot_01",
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
    require(parsed_patch.fixtures[0].address == 21, "fixture JSON patch address");
    require(parsed_patch.fixtures[0].calibration.pan_invert, "fixture JSON patch calibration bool");

    bbb::dmx::fixture_mapper json_mapper{};
    map_result = json_mapper.add_profile(parsed_profile);
    require(map_result.ok, "fixture JSON mapper accepts parsed profile");
    map_result = json_mapper.set_patch(parsed_patch);
    require(map_result.ok, "fixture JSON mapper accepts parsed patch");
    map_result = json_mapper.set_normalized("json_spot_01", "dimmer", 0.5);
    require(map_result.ok, "fixture JSON mapper normalized set");
    require(json_mapper.universe(1).channel(25) == 128, "fixture JSON mapper normalized dimmer");

    std::cout << "bbb_dmx_common_tests passed" << std::endl;
    return 0;
}
