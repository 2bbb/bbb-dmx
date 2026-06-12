#pragma once

#include <algorithm>
#include <cmath>

namespace bbb::dmx {

constexpr double pi = 3.141592653589793238462643383279502884;
constexpr double vector_epsilon = 1.0e-9;

inline bool is_finite(double value) {
    return std::isfinite(value);
}

inline double degrees_to_radians(double degrees) {
    return degrees * pi / 180.0;
}

inline double radians_to_degrees(double radians) {
    return radians * 180.0 / pi;
}

inline double clamp(double value, double minimum, double maximum) {
    return std::max(minimum, std::min(maximum, value));
}

inline double clamp_angle_to_range(double degrees, double range_degrees) {
    const double half_range{range_degrees * 0.5};
    return clamp(degrees, -half_range, half_range);
}

inline double sanitize_positive_range(double range_degrees, double fallback = 1.0) {
    if(!is_finite(range_degrees) || range_degrees <= 0.0) {
        return fallback;
    }
    return range_degrees;
}

struct vec3 {
public:
    double x{0.0};
    double y{0.0};
    double z{0.0};

    vec3() = default;
    vec3(double x, double y, double z)
    : x{x}
    , y{y}
    , z{z}
    {}

    bool finite() const {
        return is_finite(x) && is_finite(y) && is_finite(z);
    }

    double length_squared() const {
        return x * x + y * y + z * z;
    }

    double length() const {
        return std::sqrt(length_squared());
    }

    bool nearly_zero(double epsilon = vector_epsilon) const {
        return length() < epsilon;
    }
};

inline vec3 operator-(const vec3 &left, const vec3 &right) {
    return vec3{left.x - right.x, left.y - right.y, left.z - right.z};
}

struct mat3 {
public:
    double m[3][3]{{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}};

    static mat3 identity() {
        return mat3{};
    }

    static mat3 rotation_x(double radians) {
        const double cosine{std::cos(radians)};
        const double sine{std::sin(radians)};
        return mat3{{
            {1.0, 0.0, 0.0},
            {0.0, cosine, -sine},
            {0.0, sine, cosine},
        }};
    }

    static mat3 rotation_y(double radians) {
        const double cosine{std::cos(radians)};
        const double sine{std::sin(radians)};
        return mat3{{
            {cosine, 0.0, sine},
            {0.0, 1.0, 0.0},
            {-sine, 0.0, cosine},
        }};
    }

    static mat3 rotation_z(double radians) {
        const double cosine{std::cos(radians)};
        const double sine{std::sin(radians)};
        return mat3{{
            {cosine, -sine, 0.0},
            {sine, cosine, 0.0},
            {0.0, 0.0, 1.0},
        }};
    }

    mat3 transpose() const {
        mat3 result{};
        for(int row = 0; row < 3; row++) {
            for(int column = 0; column < 3; column++) {
                result.m[row][column] = m[column][row];
            }
        }
        return result;
    }
};

inline mat3 operator*(const mat3 &left, const mat3 &right) {
    mat3 result{};
    for(int row = 0; row < 3; row++) {
        for(int column = 0; column < 3; column++) {
            result.m[row][column] = 0.0;
            for(int index = 0; index < 3; index++) {
                result.m[row][column] += left.m[row][index] * right.m[index][column];
            }
        }
    }
    return result;
}

inline vec3 operator*(const mat3 &matrix, const vec3 &vector) {
    return vec3{
        matrix.m[0][0] * vector.x + matrix.m[0][1] * vector.y + matrix.m[0][2] * vector.z,
        matrix.m[1][0] * vector.x + matrix.m[1][1] * vector.y + matrix.m[1][2] * vector.z,
        matrix.m[2][0] * vector.x + matrix.m[2][1] * vector.y + matrix.m[2][2] * vector.z,
    };
}

inline mat3 fixture_rotation_xyz_degrees(double rotate_x_degrees, double rotate_y_degrees, double rotate_z_degrees) {
    const mat3 rotate_x{mat3::rotation_x(degrees_to_radians(rotate_x_degrees))};
    const mat3 rotate_y{mat3::rotation_y(degrees_to_radians(rotate_y_degrees))};
    const mat3 rotate_z{mat3::rotation_z(degrees_to_radians(rotate_z_degrees))};
    return rotate_z * rotate_y * rotate_x;
}

inline vec3 world_to_fixture_local(const vec3 &world_vector, double rotate_x_degrees, double rotate_y_degrees, double rotate_z_degrees) {
    return fixture_rotation_xyz_degrees(rotate_x_degrees, rotate_y_degrees, rotate_z_degrees).transpose() * world_vector;
}

} // namespace bbb::dmx
