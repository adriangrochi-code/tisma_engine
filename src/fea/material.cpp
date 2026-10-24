// Orthotropic elasticity and the Tsai-Wu failure criterion (textbook: Jones,
// Mechanics of Composite Materials, 1999; Tsai and Wu 1971). Own
// implementation; see PROVENANCE.md.

#include "fea/material.hpp"

#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace tisma {

Stiffness isotropic_stiffness(double young, double poisson) {
    const double nu = poisson;
    const double lambda = young * nu / ((1.0 + nu) * (1.0 - 2.0 * nu)), mu = young / (2.0 * (1.0 + nu));
    Stiffness d{};
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            d[std::size_t(r * 6 + c)] = lambda + (r == c ? 2.0 * mu : 0.0);
    for (int r = 3; r < 6; ++r)
        d[std::size_t(r * 6 + r)] = mu;
    return d;
}

Stiffness local_stiffness(const OrthotropicMaterial& m) {
    // Normal block of the compliance, then its inverse; shear terms are 1/G.
    const double s[3][3] = {{1.0 / m.e1, -m.nu12 / m.e1, -m.nu13 / m.e1},
                            {-m.nu12 / m.e1, 1.0 / m.e2, -m.nu23 / m.e2},
                            {-m.nu13 / m.e1, -m.nu23 / m.e2, 1.0 / m.e3}};
    const double det = s[0][0] * (s[1][1] * s[2][2] - s[1][2] * s[2][1]) -
                       s[0][1] * (s[1][0] * s[2][2] - s[1][2] * s[2][0]) +
                       s[0][2] * (s[1][0] * s[2][1] - s[1][1] * s[2][0]);
    if (!(det > 0.0))
        throw std::invalid_argument("orthotropic material: the compliance is not positive definite");
    Stiffness d{};
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c) {
            // Cofactor of s[c][r] over the determinant.
            const int r1 = (c + 1) % 3, r2 = (c + 2) % 3, c1 = (r + 1) % 3, c2 = (r + 2) % 3;
            d[std::size_t(r * 6 + c)] = (s[r1][c1] * s[r2][c2] - s[r1][c2] * s[r2][c1]) / det;
        }
    d[3 * 6 + 3] = m.g12;
    d[4 * 6 + 4] = m.g23;
    d[5 * 6 + 5] = m.g13;
    return d;
}

Stiffness strain_rotation(double theta) {
    const double c = std::cos(theta), s = std::sin(theta);
    // Rows: material (11, 22, 33, 12, 23, 31); columns: global (xx, yy, zz, xy, yz, zx).
    return {c * c,        s * s,       0.0, c * s,         0.0, 0.0, //
            s * s,        c * c,       0.0, -c * s,        0.0, 0.0, //
            0.0,          0.0,         1.0, 0.0,           0.0, 0.0, //
            -2.0 * c * s, 2.0 * c * s, 0.0, c * c - s * s, 0.0, 0.0, //
            0.0,          0.0,         0.0, 0.0,           c,   -s,  //
            0.0,          0.0,         0.0, 0.0,           s,   c};
}

Stiffness stress_rotation(double theta) {
    const double c = std::cos(theta), s = std::sin(theta);
    // The strain rotation with the factor 2 moved from the shear row to the
    // shear column (shear stress is not doubled like engineering strain).
    return {c * c,  s * s, 0.0, 2.0 * c * s,   0.0, 0.0, //
            s * s,  c * c, 0.0, -2.0 * c * s,  0.0, 0.0, //
            0.0,    0.0,   1.0, 0.0,           0.0, 0.0, //
            -c * s, c * s, 0.0, c * c - s * s, 0.0, 0.0, //
            0.0,    0.0,   0.0, 0.0,           c,   -s,  //
            0.0,    0.0,   0.0, 0.0,           s,   c};
}

Stiffness global_stiffness(const OrthotropicMaterial& m, double theta) {
    const Stiffness d = local_stiffness(m), t = strain_rotation(theta);
    Stiffness dt{}, out{};
    for (int r = 0; r < 6; ++r)
        for (int c = 0; c < 6; ++c)
            for (int k = 0; k < 6; ++k)
                dt[std::size_t(r * 6 + c)] += d[std::size_t(r * 6 + k)] * t[std::size_t(k * 6 + c)];
    for (int r = 0; r < 6; ++r)
        for (int c = 0; c < 6; ++c)
            for (int k = 0; k < 6; ++k)
                out[std::size_t(r * 6 + c)] += t[std::size_t(k * 6 + r)] * dt[std::size_t(k * 6 + c)];
    return out;
}

Stiffness layer_average_stiffness(const OrthotropicMaterial& m) {
    // The rotated stiffness holds harmonics of order 0, 2 and 4 in theta:
    // eight equally spaced angles over a half turn average it exactly.
    Stiffness out{};
    constexpr int kAngles = 8;
    for (int a = 0; a < kAngles; ++a) {
        const Stiffness d = global_stiffness(m, std::numbers::pi * a / kAngles);
        for (std::size_t i = 0; i < out.size(); ++i)
            out[i] += d[i] / kAngles;
    }
    return out;
}

double tsai_wu_safety(const OrthotropicMaterial& m, const Voigt& s) {
    const double f1 = 1.0 / m.xt - 1.0 / m.xc, f2 = 1.0 / m.yt - 1.0 / m.yc, f3 = 1.0 / m.zt - 1.0 / m.zc;
    const double f11 = 1.0 / (m.xt * m.xc), f22 = 1.0 / (m.yt * m.yc), f33 = 1.0 / (m.zt * m.zc);
    const double f44 = 1.0 / (m.s12 * m.s12), f55 = 1.0 / (m.s23 * m.s23), f66 = 1.0 / (m.s13 * m.s13);
    const double f12 = -0.5 * std::sqrt(f11 * f22), f13 = -0.5 * std::sqrt(f11 * f33), f23 = -0.5 * std::sqrt(f22 * f33);
    // F(k s) = a k^2 + b k = 1 for the factor k.
    const double a = f11 * s[0] * s[0] + f22 * s[1] * s[1] + f33 * s[2] * s[2] + 2.0 * f12 * s[0] * s[1] +
                     2.0 * f13 * s[0] * s[2] + 2.0 * f23 * s[1] * s[2] + f44 * s[3] * s[3] + f55 * s[4] * s[4] +
                     f66 * s[5] * s[5];
    const double b = f1 * s[0] + f2 * s[1] + f3 * s[2];
    if (a <= 0.0)
        return b > 0.0 ? 1.0 / b : std::numeric_limits<double>::infinity();
    return (-b + std::sqrt(b * b + 4.0 * a)) / (2.0 * a);
}

FailureMode failure_mode(const OrthotropicMaterial& m, const Voigt& s) {
    const double ratio[6] = {s[0] / (s[0] >= 0.0 ? m.xt : -m.xc), s[1] / (s[1] >= 0.0 ? m.yt : -m.yc),
                             s[2] / (s[2] >= 0.0 ? m.zt : -m.zc), std::abs(s[3]) / m.s12,
                             std::abs(s[4]) / m.s23,                std::abs(s[5]) / m.s13};
    static constexpr FailureMode kMode[6] = {FailureMode::Line,         FailureMode::BetweenLines,
                                             FailureMode::BetweenLayers, FailureMode::BetweenLines,
                                             FailureMode::BetweenLayers, FailureMode::BetweenLayers};
    FailureMode mode = FailureMode::None;
    double worst = 0.0;
    for (int i = 0; i < 6; ++i)
        if (ratio[i] > worst)
            worst = ratio[i], mode = kMode[i];
    return mode;
}

} // namespace tisma
