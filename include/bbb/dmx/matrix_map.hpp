#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "bbb/dmx/fixture_json.hpp"

namespace bbb::dmx::matrixmap {

enum class sample_mode {
    point,
    average,
};

enum class grid_placement {
    cell,
    points,
};

enum class color_source_kind {
    red,
    green,
    blue,
    alpha,
    luma,
    max_rgb,
    constant,
};

enum class plane_order_kind {
    rgba,
    argb,
    bgra,
    gray,
};

enum class universe_output_mode {
    all,
    selected,
    changed,
};

enum class matrix_value_kind {
    uint8,
    float32,
};

struct color_value {
public:
    double red{0.0};
    double green{0.0};
    double blue{0.0};
    double alpha{1.0};
};

struct color_source {
public:
    color_source_kind kind{color_source_kind::red};
    double constant_value{0.0};
};

struct parameter_binding {
public:
    std::string parameter{};
    color_source source{};
};

struct sample_region {
public:
    sample_mode mode{sample_mode::point};
    double x{0.5};
    double y{0.5};
    double width{0.0};
    double height{0.0};
};

struct fixture_mapping {
public:
    std::string fixture_id{};
    std::string group_id{};
    sample_region sample{};
    std::vector<parameter_binding> parameters{};
};

struct matrix_map_config {
public:
    std::vector<fixture_mapping> fixtures{};
};

struct matrix_read_view {
public:
    const char *data{nullptr};
    long width{0};
    long height{0};
    long plane_count{0};
    long stride_x{0};
    long stride_y{0};
    plane_order_kind plane_order{plane_order_kind::rgba};
    matrix_value_kind value_kind{matrix_value_kind::uint8};
};

inline double clamp_normalized(double value) {
    if(value < 0.0) {
        return 0.0;
    }
    if(1.0 < value) {
        return 1.0;
    }
    return value;
}

inline double constant_source_to_normalized(double value) {
    if(1.0 < value) {
        return clamp_normalized(value / 255.0);
    }
    return clamp_normalized(value);
}

inline bool parse_sample_mode(const std::string &text, sample_mode &mode) {
    if(text == "point") {
        mode = sample_mode::point;
        return true;
    }
    if(text == "average") {
        mode = sample_mode::average;
        return true;
    }
    return false;
}

inline bool parse_grid_placement(const std::string &text, grid_placement &placement) {
    if(text == "cell" || text == "cells") {
        placement = grid_placement::cell;
        return true;
    }
    if(text == "points" || text == "intersections") {
        placement = grid_placement::points;
        return true;
    }
    return false;
}

inline bool parse_plane_order(const std::string &text, plane_order_kind &order) {
    if(text == "rgba") {
        order = plane_order_kind::rgba;
        return true;
    }
    if(text == "argb") {
        order = plane_order_kind::argb;
        return true;
    }
    if(text == "bgra") {
        order = plane_order_kind::bgra;
        return true;
    }
    if(text == "gray") {
        order = plane_order_kind::gray;
        return true;
    }
    return false;
}

inline const char *plane_order_to_string(plane_order_kind order) {
    switch(order) {
        case plane_order_kind::argb: return "argb";
        case plane_order_kind::bgra: return "bgra";
        case plane_order_kind::gray: return "gray";
        case plane_order_kind::rgba:
        default: return "rgba";
    }
}

inline bool parse_universe_output_mode(const std::string &text, universe_output_mode &mode) {
    if(text == "all") {
        mode = universe_output_mode::all;
        return true;
    }
    if(text == "selected") {
        mode = universe_output_mode::selected;
        return true;
    }
    if(text == "changed") {
        mode = universe_output_mode::changed;
        return true;
    }
    return false;
}

inline const char *universe_output_mode_to_string(universe_output_mode mode) {
    switch(mode) {
        case universe_output_mode::selected: return "selected";
        case universe_output_mode::changed: return "changed";
        case universe_output_mode::all:
        default: return "all";
    }
}

inline bool parse_color_source(const std::string &text, color_source &source) {
    if(text == "r" || text == "red") {
        source.kind = color_source_kind::red;
        return true;
    }
    if(text == "g" || text == "green") {
        source.kind = color_source_kind::green;
        return true;
    }
    if(text == "b" || text == "blue") {
        source.kind = color_source_kind::blue;
        return true;
    }
    if(text == "a" || text == "alpha") {
        source.kind = color_source_kind::alpha;
        return true;
    }
    if(text == "luma") {
        source.kind = color_source_kind::luma;
        return true;
    }
    if(text == "maxrgb") {
        source.kind = color_source_kind::max_rgb;
        return true;
    }
    const std::string prefix{"constant:"};
    if(text.rfind(prefix, 0) == 0) {
        source.kind = color_source_kind::constant;
        source.constant_value = constant_source_to_normalized(std::atof(text.substr(prefix.size()).c_str()));
        return true;
    }
    return false;
}

inline double color_source_value(const color_source &source, const color_value &color) {
    switch(source.kind) {
        case color_source_kind::green: return color.green;
        case color_source_kind::blue: return color.blue;
        case color_source_kind::alpha: return color.alpha;
        case color_source_kind::luma: return clamp_normalized(0.2126 * color.red + 0.7152 * color.green + 0.0722 * color.blue);
        case color_source_kind::max_rgb: return std::max(color.red, std::max(color.green, color.blue));
        case color_source_kind::constant: return source.constant_value;
        case color_source_kind::red:
        default: return color.red;
    }
}

inline long clamp_long(long value, long minimum, long maximum) {
    return std::max(minimum, std::min(maximum, value));
}

inline int plane_index_for(plane_order_kind order, char component, long plane_count) {
    if(order == plane_order_kind::gray) {
        return component == 'a' ? -1 : 0;
    }
    int index{0};
    switch(order) {
        case plane_order_kind::argb:
            if(component == 'a') { index = 0; }
            if(component == 'r') { index = 1; }
            if(component == 'g') { index = 2; }
            if(component == 'b') { index = 3; }
            break;
        case plane_order_kind::bgra:
            if(component == 'b') { index = 0; }
            if(component == 'g') { index = 1; }
            if(component == 'r') { index = 2; }
            if(component == 'a') { index = 3; }
            break;
        case plane_order_kind::rgba:
        default:
            if(component == 'r') { index = 0; }
            if(component == 'g') { index = 1; }
            if(component == 'b') { index = 2; }
            if(component == 'a') { index = 3; }
            break;
    }
    if(index < 0 || plane_count <= index) {
        return component == 'a' ? -1 : 0;
    }
    return index;
}

inline double pixel_component(const matrix_read_view &view, long x, long y, char component) {
    if(!view.data || view.width <= 0 || view.height <= 0) {
        return component == 'a' ? 1.0 : 0.0;
    }
    const int plane_index{plane_index_for(view.plane_order, component, view.plane_count)};
    if(plane_index < 0) {
        return 1.0;
    }
    x = clamp_long(x, 0, view.width - 1);
    y = clamp_long(y, 0, view.height - 1);
    const char *pixel = view.data + y * view.stride_y + x * view.stride_x;
    if(view.value_kind == matrix_value_kind::float32) {
        const float *values = (const float *)pixel;
        return clamp_normalized((double)values[plane_index]);
    }
    const unsigned char *values = (const unsigned char *)pixel;
    return (double)values[plane_index] / 255.0;
}

inline color_value sample_point(const matrix_read_view &view, double normalized_x, double normalized_y) {
    const long x{clamp_long((long)std::llround(clamp_normalized(normalized_x) * (double)(std::max<long>(1, view.width) - 1)), 0, std::max<long>(0, view.width - 1))};
    const long y{clamp_long((long)std::llround(clamp_normalized(normalized_y) * (double)(std::max<long>(1, view.height) - 1)), 0, std::max<long>(0, view.height - 1))};
    return color_value{
        pixel_component(view, x, y, 'r'),
        pixel_component(view, x, y, 'g'),
        pixel_component(view, x, y, 'b'),
        pixel_component(view, x, y, 'a'),
    };
}

inline color_value sample_average(const matrix_read_view &view, const sample_region &region) {
    const double half_width{std::max(0.0, region.width) * 0.5};
    const double half_height{std::max(0.0, region.height) * 0.5};
    const double left{clamp_normalized(region.x - half_width)};
    const double right{clamp_normalized(region.x + half_width)};
    const double top{clamp_normalized(region.y - half_height)};
    const double bottom{clamp_normalized(region.y + half_height)};
    const long x0{clamp_long((long)std::floor(left * (double)(view.width - 1)), 0, view.width - 1)};
    const long x1{clamp_long((long)std::ceil(right * (double)(view.width - 1)), 0, view.width - 1)};
    const long y0{clamp_long((long)std::floor(top * (double)(view.height - 1)), 0, view.height - 1)};
    const long y1{clamp_long((long)std::ceil(bottom * (double)(view.height - 1)), 0, view.height - 1)};
    double red{0.0};
    double green{0.0};
    double blue{0.0};
    double alpha{0.0};
    long long count{0};
    for(long y = y0; y <= y1; y++) {
        for(long x = x0; x <= x1; x++) {
            red += pixel_component(view, x, y, 'r');
            green += pixel_component(view, x, y, 'g');
            blue += pixel_component(view, x, y, 'b');
            alpha += pixel_component(view, x, y, 'a');
            count++;
        }
    }
    if(count <= 0) {
        return sample_point(view, region.x, region.y);
    }
    return color_value{
        red / (double)count,
        green / (double)count,
        blue / (double)count,
        alpha / (double)count,
    };
}

inline color_value sample_region_color(const matrix_read_view &view, const sample_region &region) {
    if(region.mode == sample_mode::average && 0.0 < region.width && 0.0 < region.height) {
        return sample_average(view, region);
    }
    return sample_point(view, region.x, region.y);
}

inline bbb::dmx::mapper_result parse_sample_region(const bbb::dmx::json_value &object, sample_region &sample) {
    if(object.type != bbb::dmx::json_type::object) {
        return bbb::dmx::mapper_result::failure("sample must be object");
    }
    std::string error{};
    std::string mode_text{"point"};
    bbb::dmx::json_string(object, "mode", mode_text, false, error);
    if(!parse_sample_mode(mode_text, sample.mode)) {
        return bbb::dmx::mapper_result::failure("unknown sample mode: " + mode_text);
    }
    bbb::dmx::json_double(object, "x", sample.x, true, error);
    if(!error.empty()) {
        return bbb::dmx::mapper_result::failure(error);
    }
    bbb::dmx::json_double(object, "y", sample.y, true, error);
    if(!error.empty()) {
        return bbb::dmx::mapper_result::failure(error);
    }
    bbb::dmx::json_double(object, "w", sample.width, false, error);
    bbb::dmx::json_double(object, "h", sample.height, false, error);
    sample.x = clamp_normalized(sample.x);
    sample.y = clamp_normalized(sample.y);
    sample.width = std::max(0.0, std::min(1.0, sample.width));
    sample.height = std::max(0.0, std::min(1.0, sample.height));
    return bbb::dmx::mapper_result::success();
}

inline bbb::dmx::mapper_result parse_parameter_bindings(const bbb::dmx::json_value &object, std::vector<parameter_binding> &parameters) {
    if(object.type != bbb::dmx::json_type::object) {
        return bbb::dmx::mapper_result::failure("params must be object");
    }
    parameters.clear();
    for(const auto &entry : object.object_value) {
        parameter_binding binding{};
        binding.parameter = entry.first;
        if(entry.second.type == bbb::dmx::json_type::string) {
            if(!parse_color_source(entry.second.string_value, binding.source)) {
                return bbb::dmx::mapper_result::failure("unknown color source: " + entry.second.string_value);
            }
        } else if(entry.second.type == bbb::dmx::json_type::number) {
            binding.source.kind = color_source_kind::constant;
            binding.source.constant_value = constant_source_to_normalized(entry.second.number_value);
        } else {
            return bbb::dmx::mapper_result::failure("param source must be string or number: " + entry.first);
        }
        parameters.push_back(binding);
    }
    return bbb::dmx::mapper_result::success();
}

inline bbb::dmx::mapper_result parse_fixture_mapping(const bbb::dmx::json_value &object, fixture_mapping &mapping) {
    if(object.type != bbb::dmx::json_type::object) {
        return bbb::dmx::mapper_result::failure("fixture mapping must be object");
    }
    std::string error{};
    bbb::dmx::json_string(object, "id", mapping.fixture_id, false, error);
    if(!error.empty()) {
        return bbb::dmx::mapper_result::failure(error);
    }
    bbb::dmx::json_string(object, "group", mapping.group_id, false, error);
    if(!error.empty()) {
        return bbb::dmx::mapper_result::failure(error);
    }
    if(mapping.fixture_id.empty() == mapping.group_id.empty()) {
        return bbb::dmx::mapper_result::failure("fixture mapping requires exactly one of id or group");
    }
    const std::string target_id{!mapping.fixture_id.empty() ? mapping.fixture_id : mapping.group_id};
    const bbb::dmx::json_value *sample{object.find("sample")};
    if(!sample) {
        return bbb::dmx::mapper_result::failure("fixture mapping requires sample: " + target_id);
    }
    bbb::dmx::mapper_result result{parse_sample_region(*sample, mapping.sample)};
    if(!result.ok) {
        return result;
    }
    const bbb::dmx::json_value *params{object.find("params")};
    if(!params) {
        return bbb::dmx::mapper_result::failure("fixture mapping requires params: " + target_id);
    }
    return parse_parameter_bindings(*params, mapping.parameters);
}

inline bbb::dmx::mapper_result expand_grid(const bbb::dmx::json_value &object, matrix_map_config &config) {
    if(object.type != bbb::dmx::json_type::object) {
        return bbb::dmx::mapper_result::failure("grid must be object");
    }
    std::string error{};
    std::string fixture_pattern{};
    if(!bbb::dmx::json_string(object, "fixture_pattern", fixture_pattern, true, error)) {
        return bbb::dmx::mapper_result::failure(error);
    }
    int cols{0};
    int rows{0};
    int start_index{1};
    if(!bbb::dmx::json_int(object, "cols", cols, true, error)) {
        return bbb::dmx::mapper_result::failure(error);
    }
    if(!bbb::dmx::json_int(object, "rows", rows, true, error)) {
        return bbb::dmx::mapper_result::failure(error);
    }
    bbb::dmx::json_int(object, "start_index", start_index, false, error);
    std::string mode_text{"average"};
    bbb::dmx::json_string(object, "mode", mode_text, false, error);
    sample_mode mode{sample_mode::average};
    if(!parse_sample_mode(mode_text, mode)) {
        return bbb::dmx::mapper_result::failure("unknown grid sample mode: " + mode_text);
    }
    std::string placement_text{"cell"};
    bbb::dmx::json_string(object, "placement", placement_text, false, error);
    if(!error.empty()) {
        return bbb::dmx::mapper_result::failure(error);
    }
    grid_placement placement{grid_placement::cell};
    if(!parse_grid_placement(placement_text, placement)) {
        return bbb::dmx::mapper_result::failure("unknown grid placement: " + placement_text);
    }
    double x0{0.0};
    double y0{0.0};
    double x1{1.0};
    double y1{1.0};
    bbb::dmx::json_double(object, "x0", x0, false, error);
    if(!error.empty()) {
        return bbb::dmx::mapper_result::failure(error);
    }
    bbb::dmx::json_double(object, "y0", y0, false, error);
    if(!error.empty()) {
        return bbb::dmx::mapper_result::failure(error);
    }
    bbb::dmx::json_double(object, "x1", x1, false, error);
    if(!error.empty()) {
        return bbb::dmx::mapper_result::failure(error);
    }
    bbb::dmx::json_double(object, "y1", y1, false, error);
    if(!error.empty()) {
        return bbb::dmx::mapper_result::failure(error);
    }
    if(x0 < 0.0 || 1.0 < x0 || y0 < 0.0 || 1.0 < y0 || x1 < 0.0 || 1.0 < x1 || y1 < 0.0 || 1.0 < y1) {
        return bbb::dmx::mapper_result::failure("grid bounds must be normalized 0..1");
    }
    const bbb::dmx::json_value *params{object.find("params")};
    if(!params) {
        return bbb::dmx::mapper_result::failure("grid requires params");
    }
    std::vector<parameter_binding> bindings{};
    bbb::dmx::mapper_result result{parse_parameter_bindings(*params, bindings)};
    if(!result.ok) {
        return result;
    }
    if(cols <= 0 || rows <= 0) {
        return bbb::dmx::mapper_result::failure("grid cols and rows must be positive");
    }
    const double bounds_width{x1 - x0};
    const double bounds_height{y1 - y0};
    const double cell_width{bounds_width / (double)cols};
    const double cell_height{bounds_height / (double)rows};
    const double sample_width{std::abs(cell_width)};
    const double sample_height{std::abs(cell_height)};
    for(int row = 0; row < rows; row++) {
        for(int col = 0; col < cols; col++) {
            const int fixture_index{start_index + row * cols + col};
            char fixture_id_buffer[512];
            std::snprintf(fixture_id_buffer, sizeof(fixture_id_buffer), fixture_pattern.c_str(), fixture_index);
            fixture_mapping mapping{};
            mapping.fixture_id = fixture_id_buffer;
            mapping.sample.mode = mode;
            if(placement == grid_placement::points) {
                mapping.sample.x = cols == 1
                    ? x0 + bounds_width * 0.5
                    : x0 + ((double)col / (double)(cols - 1)) * bounds_width;
                mapping.sample.y = rows == 1
                    ? y0 + bounds_height * 0.5
                    : y0 + ((double)row / (double)(rows - 1)) * bounds_height;
                mapping.sample.width = 0.0;
                mapping.sample.height = 0.0;
            } else {
                mapping.sample.x = x0 + ((double)col + 0.5) * cell_width;
                mapping.sample.y = y0 + ((double)row + 0.5) * cell_height;
                mapping.sample.width = sample_width;
                mapping.sample.height = sample_height;
            }
            mapping.parameters = bindings;
            config.fixtures.push_back(mapping);
        }
    }
    return bbb::dmx::mapper_result::success();
}

inline bbb::dmx::mapper_result parse_matrix_map_text(const std::string &text, matrix_map_config &config) {
    const bbb::dmx::json_parse_result parsed{bbb::dmx::parse_json_text(text)};
    if(!parsed.ok) {
        return bbb::dmx::mapper_result::failure(parsed.message);
    }
    if(parsed.value.type != bbb::dmx::json_type::object) {
        return bbb::dmx::mapper_result::failure("matrixmap root must be object");
    }
    matrix_map_config loaded{};
    const bbb::dmx::json_value *fixtures{parsed.value.find("fixtures")};
    if(fixtures) {
        if(fixtures->type != bbb::dmx::json_type::array) {
            return bbb::dmx::mapper_result::failure("fixtures must be array");
        }
        for(const auto &fixture_value : fixtures->array_value) {
            fixture_mapping mapping{};
            bbb::dmx::mapper_result result{parse_fixture_mapping(fixture_value, mapping)};
            if(!result.ok) {
                return result;
            }
            loaded.fixtures.push_back(mapping);
        }
    }
    const bbb::dmx::json_value *grid{parsed.value.find("grid")};
    if(grid) {
        bbb::dmx::mapper_result result{expand_grid(*grid, loaded)};
        if(!result.ok) {
            return result;
        }
    }
    const bbb::dmx::json_value *grids{parsed.value.find("grids")};
    if(grids) {
        if(grids->type != bbb::dmx::json_type::array) {
            return bbb::dmx::mapper_result::failure("grids must be array");
        }
        for(const auto &grid_value : grids->array_value) {
            bbb::dmx::mapper_result result{expand_grid(grid_value, loaded)};
            if(!result.ok) {
                return result;
            }
        }
    }
    if(loaded.fixtures.empty()) {
        return bbb::dmx::mapper_result::failure("matrixmap has no fixtures or grids");
    }
    config = loaded;
    return bbb::dmx::mapper_result::success();
}

} // namespace bbb::dmx::matrixmap
