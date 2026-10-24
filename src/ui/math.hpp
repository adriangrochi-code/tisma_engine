#pragma once

#include <array>
#include <cmath>

// Small vector and matrix helpers for the interface's 3D view. Matrices are
// column-major, as OpenGL takes them. Own design; see PROVENANCE.md.
namespace tisma::ui {

struct Vec3 {
    float x = 0, y = 0, z = 0;
    friend Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
    friend Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
    friend Vec3 operator*(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
    friend Vec3 operator*(float s, Vec3 a) { return a * s; }
    friend bool operator==(const Vec3&, const Vec3&) = default;
};

inline float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline float length(Vec3 a) { return std::sqrt(dot(a, a)); }
inline Vec3 normalize(Vec3 a) {
    const float l = length(a);
    return l > 0 ? a * (1.0f / l) : Vec3{};
}

// Column-major: element (row r, column c) is m[c * 4 + r].
using Mat4 = std::array<float, 16>;

inline Mat4 identity() { return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}; }

inline Mat4 operator*(const Mat4& a, const Mat4& b) {
    Mat4 m{};
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r) {
            float s = 0;
            for (int k = 0; k < 4; ++k)
                s += a[k * 4 + r] * b[c * 4 + k];
            m[c * 4 + r] = s;
        }
    return m;
}

// The point p transformed by m, divided by w.
inline Vec3 transform_point(const Mat4& m, Vec3 p) {
    const float x = m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12];
    const float y = m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13];
    const float z = m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14];
    const float w = m[3] * p.x + m[7] * p.y + m[11] * p.z + m[15];
    return {x / w, y / w, z / w};
}

} // namespace tisma::ui
