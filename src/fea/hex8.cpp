// The trilinear hexahedron for linear elasticity (textbook formulation:
// Hughes 2000, Ch. 3; Zienkiewicz and Taylor, Vol. 1, Ch. 6). Own
// implementation; see PROVENANCE.md.

#include "fea/hex8.hpp"

#include <cmath>

namespace tisma {
namespace {

// The strain-displacement matrix B (6 x 24) of the unit cube at natural
// coordinates (xi, eta, zeta) in [-1, 1]^3. The cube maps to [0, 1]^3, so
// d/dx = 2 d/dxi.
void strain_matrix(double xi, double eta, double zeta, double b[6 * 24]) {
    for (int i = 0; i < 6 * 24; ++i)
        b[i] = 0.0;
    for (int a = 0; a < 8; ++a) {
        const double sx = 2.0 * kHexCorner[a][0] - 1.0, sy = 2.0 * kHexCorner[a][1] - 1.0,
                     sz = 2.0 * kHexCorner[a][2] - 1.0;
        // N_a = (1 + sx xi)(1 + sy eta)(1 + sz zeta) / 8
        const double nx = 2.0 * sx * (1.0 + sy * eta) * (1.0 + sz * zeta) / 8.0;
        const double ny = 2.0 * sy * (1.0 + sx * xi) * (1.0 + sz * zeta) / 8.0;
        const double nz = 2.0 * sz * (1.0 + sx * xi) * (1.0 + sy * eta) / 8.0;
        const int c = 3 * a;
        b[0 * 24 + c + 0] = nx;
        b[1 * 24 + c + 1] = ny;
        b[2 * 24 + c + 2] = nz;
        b[3 * 24 + c + 0] = ny, b[3 * 24 + c + 1] = nx;
        b[4 * 24 + c + 1] = nz, b[4 * 24 + c + 2] = ny;
        b[5 * 24 + c + 0] = nz, b[5 * 24 + c + 2] = nx;
    }
}

} // namespace

Hex8 hex8(double poisson) {
    Hex8 e{};
    const double nu = poisson;
    const double lambda = nu / ((1.0 + nu) * (1.0 - 2.0 * nu)), mu = 1.0 / (2.0 * (1.0 + nu));
    double* d = e.elasticity.data();
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            d[r * 6 + c] = lambda + (r == c ? 2.0 * mu : 0.0);
    for (int r = 3; r < 6; ++r)
        d[r * 6 + r] = mu;
    const double g = 1.0 / std::sqrt(3.0);
    const double det = 1.0 / 8.0; // Jacobian of [-1, 1]^3 -> [0, 1]^3; Gauss weights are 1
    double b[6 * 24], db[6 * 24];
    for (int p = 0; p < 8; ++p) {
        strain_matrix(kHexCorner[p][0] ? g : -g, kHexCorner[p][1] ? g : -g, kHexCorner[p][2] ? g : -g, b);
        for (int r = 0; r < 6; ++r)
            for (int c = 0; c < 24; ++c) {
                double s = 0.0;
                for (int k = 0; k < 6; ++k)
                    s += d[r * 6 + k] * b[k * 24 + c];
                db[r * 24 + c] = s;
            }
        for (int r = 0; r < 24; ++r)
            for (int c = 0; c < 24; ++c) {
                double s = 0.0;
                for (int k = 0; k < 6; ++k)
                    s += b[k * 24 + r] * db[k * 24 + c];
                e.stiffness[std::size_t(r) * 24 + std::size_t(c)] += s * det;
            }
    }
    strain_matrix(0.0, 0.0, 0.0, e.b_centre.data());
    return e;
}

double von_mises(const double s[6]) {
    const double a = s[0] - s[1], b = s[1] - s[2], c = s[2] - s[0];
    return std::sqrt(0.5 * (a * a + b * b + c * c) + 3.0 * (s[3] * s[3] + s[4] * s[4] + s[5] * s[5]));
}

} // namespace tisma
