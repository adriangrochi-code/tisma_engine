#include "mesh/lattice.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <numbers>
#include <tuple>

namespace tisma {

std::vector<LatticeBar> lattice_bars(const std::array<double, 3>& lo, const std::array<double, 3>& hi, double a) {
    std::vector<LatticeBar> out;
    if (!(a > 0.0))
        return out;
    std::array<int, 3> n{};
    for (int k = 0; k < 3; ++k)
        n[std::size_t(k)] = std::max(0, int(std::ceil((hi[std::size_t(k)] - lo[std::size_t(k)]) / a - 1e-9)));
    // Each bar once: keyed by its two nodes (integer grid points).
    using Node = std::tuple<int, int, int>;
    std::map<std::pair<Node, Node>, bool> seen;
    const auto add = [&](Node p, Node q) {
        if (q < p)
            std::swap(p, q);
        if (!seen.emplace(std::make_pair(p, q), true).second)
            return;
        const auto at = [&](const Node& m) {
            return std::array<double, 3>{lo[0] + std::get<0>(m) * a, lo[1] + std::get<1>(m) * a, lo[2] + std::get<2>(m) * a};
        };
        out.push_back({at(p), at(q)});
    };
    for (int k = 0; k < n[2]; ++k)
        for (int j = 0; j < n[1]; ++j)
            for (int i = 0; i < n[0]; ++i) {
                for (int dj = 0; dj < 2; ++dj) // the 4 vertical edges
                    for (int di = 0; di < 2; ++di)
                        add({i + di, j + dj, k}, {i + di, j + dj, k + 1});
                for (int dj = 0; dj < 2; ++dj) { // the XZ faces (y fixed): both diagonals
                    add({i, j + dj, k}, {i + 1, j + dj, k + 1});
                    add({i + 1, j + dj, k}, {i, j + dj, k + 1});
                }
                for (int di = 0; di < 2; ++di) { // the YZ faces (x fixed)
                    add({i + di, j, k}, {i + di, j + 1, k + 1});
                    add({i + di, j + 1, k}, {i + di, j, k + 1});
                }
            }
    return out;
}

TriangleMesh lattice_mesh(const std::vector<LatticeBar>& bars, double diameter) {
    TriangleMesh m;
    const double r = 0.5 * diameter;
    for (const LatticeBar& b : bars) {
        const double d[3] = {b.to[0] - b.from[0], b.to[1] - b.from[1], b.to[2] - b.from[2]};
        const double length = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        if (!(length > 0.0))
            continue;
        const double w[3] = {d[0] / length, d[1] / length, d[2] / length};
        // Two directions square to the bar: u from a helper axis, v = w x u.
        const double h[3] = {std::abs(w[0]) < 0.9 ? 1.0 : 0.0, std::abs(w[0]) < 0.9 ? 0.0 : 1.0, 0.0};
        double u[3] = {h[1] * w[2] - h[2] * w[1], h[2] * w[0] - h[0] * w[2], h[0] * w[1] - h[1] * w[0]};
        const double ul = std::sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
        for (double& c : u)
            c /= ul;
        const double v[3] = {w[1] * u[2] - w[2] * u[1], w[2] * u[0] - w[0] * u[2], w[0] * u[1] - w[1] * u[0]};
        Vec3f bottom[6], top[6];
        for (int s = 0; s < 6; ++s) {
            const double t = s * std::numbers::pi / 3.0, cu = r * std::cos(t), sv = r * std::sin(t);
            bottom[s] = Vec3f{float(b.from[0] + cu * u[0] + sv * v[0]), float(b.from[1] + cu * u[1] + sv * v[1]),
                              float(b.from[2] + cu * u[2] + sv * v[2])};
            top[s] = Vec3f{float(b.to[0] + cu * u[0] + sv * v[0]), float(b.to[1] + cu * u[1] + sv * v[1]),
                           float(b.to[2] + cu * u[2] + sv * v[2])};
        }
        const Vec3f c0{float(b.from[0]), float(b.from[1]), float(b.from[2])}, c1{float(b.to[0]), float(b.to[1]), float(b.to[2])};
        // u, v, w right-handed: counter-clockwise about w, so the bottom cap
        // (facing -w) goes clockwise.
        for (int s = 0; s < 6; ++s) {
            const int e = (s + 1) % 6;
            m.add_triangle(c0, bottom[e], bottom[s]);
            m.add_triangle(c1, top[s], top[e]);
            m.add_triangle(bottom[s], bottom[e], top[e]);
            m.add_triangle(bottom[s], top[e], top[s]);
        }
    }
    return m;
}

bool inside_mesh(const TriangleMesh& mesh, const std::array<double, 3>& p) {
    // A tiny offset in X and Y keeps the ray off the mesh's edges and corners
    // (grid points of a lattice often lie in line with a box's edges).
    const double x = p[0] + 1.234567e-5, y = p[1] + 2.345671e-5;
    int crossings = 0;
    for (std::uint32_t t = 0; t < mesh.triangle_count(); ++t) {
        const Vec3f a = mesh.corner(t, 0), b = mesh.corner(t, 1), c = mesh.corner(t, 2);
        // Barycentric coordinates of (x, y) in the triangle's shadow on XY.
        const double d = (double(b.y) - c.y) * (double(a.x) - c.x) + (double(c.x) - b.x) * (double(a.y) - c.y);
        if (std::abs(d) < 1e-18)
            continue; // a face standing on its edge: the ray runs along it
        const double l1 = ((double(b.y) - c.y) * (x - c.x) + (double(c.x) - b.x) * (y - c.y)) / d;
        const double l2 = ((double(c.y) - a.y) * (x - c.x) + (double(a.x) - c.x) * (y - c.y)) / d;
        const double l3 = 1.0 - l1 - l2;
        if (l1 < 0.0 || l2 < 0.0 || l3 < 0.0)
            continue;
        if (l1 * a.z + l2 * b.z + l3 * c.z > p[2])
            ++crossings;
    }
    return crossings % 2 == 1;
}

std::vector<LatticeBar> bars_touching(const std::vector<LatticeBar>& bars, const TriangleMesh& part) {
    std::vector<LatticeBar> out;
    for (const LatticeBar& b : bars)
        for (double t : {0.5, 0.0, 1.0, 0.25, 0.75}) {
            const std::array<double, 3> q{b.from[0] + t * (b.to[0] - b.from[0]), b.from[1] + t * (b.to[1] - b.from[1]),
                                          b.from[2] + t * (b.to[2] - b.from[2])};
            if (inside_mesh(part, q)) {
                out.push_back(b);
                break;
            }
        }
    return out;
}

double lattice_density(double diameter, double a) {
    if (!(a > 0.0))
        return 0.0;
    const double r = 0.5 * diameter;
    return std::numbers::pi * (1.0 + 4.0 * std::numbers::sqrt2) * (r / a) * (r / a);
}

} // namespace tisma
