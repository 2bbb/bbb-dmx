#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>

#include "bbb/dmx/common.hpp"

namespace bbb::dmx {

struct movertrack_settings {
public:
    vec3 fixture_position{0.0, 0.0, 0.0};
    vec3 rotation_degrees{0.0, 0.0, 0.0};
    double pan_range_degrees{540.0};
    double tilt_range_degrees{270.0};
    double pan_offset_degrees{0.0};
    double tilt_offset_degrees{0.0};
    bool pan_invert{false};
    bool tilt_invert{false};
    byte_order order{byte_order::coarse_fine};
    tracking_mode tracking{tracking_mode::smart};
};

struct movertrack_output {
public:
    std::uint16_t pan{neutral_u16()};
    std::uint16_t tilt{neutral_u16()};
    std::array<int, 4> bytes{127, 255, 127, 255};
    double pan_degrees{0.0};
    double tilt_degrees{0.0};
};

class movertrack_engine {
public:
    movertrack_engine() = default;
    explicit movertrack_engine(const movertrack_settings &settings)
    : settings_{settings}
    {
        settings_.pan_range_degrees = sanitize_positive_range(settings_.pan_range_degrees);
        settings_.tilt_range_degrees = sanitize_positive_range(settings_.tilt_range_degrees);
        update_world_to_local_matrix();
    }

    const movertrack_settings &settings() const {
        return settings_;
    }

    bool set_fixture_position(const vec3 &position) {
        if(!position.finite()) {
            return false;
        }
        settings_.fixture_position = position;
        return true;
    }

    bool set_rotation_degrees(const vec3 &rotation_degrees) {
        if(!rotation_degrees.finite()) {
            return false;
        }
        if(settings_.rotation_degrees.x == rotation_degrees.x
            && settings_.rotation_degrees.y == rotation_degrees.y
            && settings_.rotation_degrees.z == rotation_degrees.z)
        {
            return true;
        }
        settings_.rotation_degrees = rotation_degrees;
        update_world_to_local_matrix();
        return true;
    }

    bool set_ranges(double pan_range_degrees, double tilt_range_degrees) {
        if(!is_finite(pan_range_degrees) || !is_finite(tilt_range_degrees)) {
            return false;
        }
        settings_.pan_range_degrees = sanitize_positive_range(pan_range_degrees);
        settings_.tilt_range_degrees = sanitize_positive_range(tilt_range_degrees);
        return true;
    }

    bool set_pan_range(double pan_range_degrees) {
        if(!is_finite(pan_range_degrees)) {
            return false;
        }
        settings_.pan_range_degrees = sanitize_positive_range(pan_range_degrees);
        return true;
    }

    bool set_tilt_range(double tilt_range_degrees) {
        if(!is_finite(tilt_range_degrees)) {
            return false;
        }
        settings_.tilt_range_degrees = sanitize_positive_range(tilt_range_degrees);
        return true;
    }

    bool set_pan_offset(double pan_offset_degrees) {
        if(!is_finite(pan_offset_degrees)) {
            return false;
        }
        settings_.pan_offset_degrees = pan_offset_degrees;
        return true;
    }

    bool set_tilt_offset(double tilt_offset_degrees) {
        if(!is_finite(tilt_offset_degrees)) {
            return false;
        }
        settings_.tilt_offset_degrees = tilt_offset_degrees;
        return true;
    }

    void set_pan_invert(bool pan_invert) {
        settings_.pan_invert = pan_invert;
    }

    void set_tilt_invert(bool tilt_invert) {
        settings_.tilt_invert = tilt_invert;
    }

    void set_byte_order(byte_order order) {
        settings_.order = order;
        if(has_last_output_) {
            last_output_.bytes = pan_tilt_to_bytes(last_output_.pan, last_output_.tilt, settings_.order);
        }
    }

    void set_shortest_pan(bool shortest_pan) {
        settings_.tracking = shortest_pan ? tracking_mode::smart : tracking_mode::off;
    }

    void set_tracking_mode(tracking_mode mode) {
        settings_.tracking = mode;
    }

    void reset_tracking() {
        has_previous_pan_ = false;
    }

    bool has_last_target() const {
        return has_last_target_;
    }

    bool has_last_output() const {
        return has_last_output_;
    }

    movertrack_output bang() {
        if(!has_last_target_) {
            return neutral_output();
        }
        if(last_target_is_relative_) {
            return compute_relative(last_relative_target_);
        }
        return compute(last_target_);
    }

    movertrack_output compute(const vec3 &target_position) {
        if(!target_position.finite()) {
            return has_last_output_ ? last_output_with_current_order() : neutral_output();
        }

        last_target_ = target_position;
        last_target_is_relative_ = false;
        has_last_target_ = true;

        return compute_world_vector(target_position - settings_.fixture_position);
    }

    movertrack_output compute_relative(const vec3 &relative_target_position) {
        if(!relative_target_position.finite()) {
            return has_last_output_ ? last_output_with_current_order() : neutral_output();
        }

        last_relative_target_ = relative_target_position;
        last_target_is_relative_ = true;
        has_last_target_ = true;

        return compute_world_vector(relative_target_position);
    }

    bool calibrate_pan_offset(const vec3 &target_position, std::uint16_t desired_pan_value) {
        pan_tilt_degrees raw_angles{};
        if(!raw_angles_for_target(target_position, raw_angles)) {
            return false;
        }
        const double desired_angle{u16_to_angle(desired_pan_value, settings_.pan_range_degrees)};
        settings_.pan_offset_degrees = settings_.pan_invert ? -desired_angle - raw_angles.pan : desired_angle - raw_angles.pan;
        reset_tracking();
        return true;
    }

    bool calibrate_tilt_offset(const vec3 &target_position, std::uint16_t desired_tilt_value) {
        pan_tilt_degrees raw_angles{};
        if(!raw_angles_for_target(target_position, raw_angles)) {
            return false;
        }
        const double desired_angle{u16_to_angle(desired_tilt_value, settings_.tilt_range_degrees)};
        settings_.tilt_offset_degrees = settings_.tilt_invert ? -desired_angle - raw_angles.tilt : desired_angle - raw_angles.tilt;
        reset_tracking();
        return true;
    }

private:
    struct tracking_candidate {
    public:
        pan_tilt_degrees angles{};
        bool valid{false};
    };

    bool raw_angles_for_target(const vec3 &target_position, pan_tilt_degrees &angles) const {
        if(!target_position.finite()) {
            return false;
        }
        const vec3 world_vector{target_position - settings_.fixture_position};
        if(world_vector.nearly_zero()) {
            return false;
        }
        const vec3 local_vector{world_to_local_matrix_ * world_vector};
        angles = vector_to_pan_tilt(local_vector);
        return true;
    }

    pan_tilt_degrees apply_calibration(const pan_tilt_degrees &raw_angles) const {
        pan_tilt_degrees calibrated_angles{
            raw_angles.pan + settings_.pan_offset_degrees,
            raw_angles.tilt + settings_.tilt_offset_degrees,
        };
        if(settings_.pan_invert) {
            calibrated_angles.pan = -calibrated_angles.pan;
        }
        if(settings_.tilt_invert) {
            calibrated_angles.tilt = -calibrated_angles.tilt;
        }
        return calibrated_angles;
    }

    bool angle_in_range(double angle, double range_degrees) const {
        const double half_range{sanitize_positive_range(range_degrees) * 0.5};
        constexpr double range_epsilon{1.0e-9};
        return -half_range - range_epsilon <= angle && angle <= half_range + range_epsilon;
    }

    bool angles_in_range(const pan_tilt_degrees &angles) const {
        return angle_in_range(angles.pan, settings_.pan_range_degrees)
            && angle_in_range(angles.tilt, settings_.tilt_range_degrees);
    }

    double tracking_score(const pan_tilt_degrees &angles) const {
        const double pan_delta{angles.pan - previous_pan_degrees_};
        const double tilt_delta{angles.tilt - previous_tilt_degrees_};
        return pan_delta * pan_delta + tilt_delta * tilt_delta;
    }

    void consider_candidate(
        const pan_tilt_degrees &raw_angles,
        bool hold_previous_pan,
        tracking_candidate &best_candidate,
        double &best_score
    ) const {
        pan_tilt_degrees calibrated_angles{apply_calibration(raw_angles)};
        if(hold_previous_pan) {
            calibrated_angles.pan = previous_pan_degrees_;
        }
        if(!angles_in_range(calibrated_angles)) {
            return;
        }
        const double score{has_previous_pan_ ? tracking_score(calibrated_angles) : calibrated_angles.pan * calibrated_angles.pan + calibrated_angles.tilt * calibrated_angles.tilt};
        if(!best_candidate.valid || score < best_score) {
            best_candidate = tracking_candidate{calibrated_angles, true};
            best_score = score;
        }
    }

    pan_tilt_degrees choose_smart_solution(const pan_tilt_degrees &raw_angles, const vec3 &local_vector) const {
        tracking_candidate best_candidate{};
        double best_score{std::numeric_limits<double>::max()};
        const double horizontal_distance{std::sqrt(local_vector.x * local_vector.x + local_vector.y * local_vector.y)};
        const bool hold_previous_pan{has_previous_pan_ && horizontal_distance < 1.0e-6 && angle_in_range(previous_pan_degrees_, settings_.pan_range_degrees)};

        for(int turn_offset = -2; turn_offset <= 2; turn_offset++) {
            const double pan_turn{360.0 * (double)turn_offset};
            consider_candidate(
                pan_tilt_degrees{raw_angles.pan + pan_turn, raw_angles.tilt},
                hold_previous_pan,
                best_candidate,
                best_score
            );
            consider_candidate(
                pan_tilt_degrees{raw_angles.pan + 180.0 + pan_turn, -raw_angles.tilt},
                hold_previous_pan,
                best_candidate,
                best_score
            );
        }

        if(best_candidate.valid) {
            return best_candidate.angles;
        }
        return apply_calibration(raw_angles);
    }

    movertrack_output compute_world_vector(const vec3 &world_vector) {
        if(world_vector.nearly_zero()) {
            return has_last_output_ ? last_output_with_current_order() : neutral_output();
        }

        const vec3 local_vector{world_to_local_matrix_ * world_vector};

        const pan_tilt_degrees raw_angles{vector_to_pan_tilt(local_vector)};
        pan_tilt_degrees angles{choose_tracking_solution(raw_angles, local_vector)};

        angles.pan = clamp_angle_to_range(angles.pan, settings_.pan_range_degrees);
        angles.tilt = clamp_angle_to_range(angles.tilt, settings_.tilt_range_degrees);

        previous_pan_degrees_ = angles.pan;
        previous_tilt_degrees_ = angles.tilt;
        has_previous_pan_ = true;

        movertrack_output output{};
        output.pan = angle_to_u16(angles.pan, settings_.pan_range_degrees);
        output.tilt = angle_to_u16(angles.tilt, settings_.tilt_range_degrees);
        output.bytes = pan_tilt_to_bytes(output.pan, output.tilt, settings_.order);
        output.pan_degrees = angles.pan;
        output.tilt_degrees = angles.tilt;

        last_output_ = output;
        has_last_output_ = true;
        return output;
    }

    pan_tilt_degrees choose_tracking_solution(const pan_tilt_degrees &raw_angles, const vec3 &local_vector) const {
        pan_tilt_degrees calibrated_angles{apply_calibration(raw_angles)};
        if(settings_.tracking == tracking_mode::off) {
            return calibrated_angles;
        }
        if(settings_.tracking == tracking_mode::pan) {
            if(has_previous_pan_) {
                calibrated_angles.pan = choose_shortest_pan(calibrated_angles.pan, previous_pan_degrees_);
            }
            return calibrated_angles;
        }
        return choose_smart_solution(raw_angles, local_vector);
    }

    movertrack_output neutral_output() const {
        movertrack_output output{};
        output.pan = neutral_u16();
        output.tilt = neutral_u16();
        output.bytes = pan_tilt_to_bytes(output.pan, output.tilt, settings_.order);
        output.pan_degrees = 0.0;
        output.tilt_degrees = 0.0;
        return output;
    }

    movertrack_output last_output_with_current_order() const {
        movertrack_output output{last_output_};
        output.bytes = pan_tilt_to_bytes(output.pan, output.tilt, settings_.order);
        return output;
    }

    void update_world_to_local_matrix() {
        world_to_local_matrix_ = fixture_rotation_xyz_degrees(
            settings_.rotation_degrees.x,
            settings_.rotation_degrees.y,
            settings_.rotation_degrees.z
        ).transpose();
    }

    movertrack_settings settings_{};
    mat3 world_to_local_matrix_{mat3::identity()};
    vec3 last_target_{};
    vec3 last_relative_target_{};
    bool last_target_is_relative_{false};
    bool has_last_target_{false};
    double previous_pan_degrees_{0.0};
    double previous_tilt_degrees_{0.0};
    bool has_previous_pan_{false};
    movertrack_output last_output_{};
    bool has_last_output_{false};
};

} // namespace bbb::dmx
