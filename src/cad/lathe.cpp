// Solids of rings around the Z axis. Own design; see PROVENANCE.md.

#include "cad/lathe.hpp"

#include <cmath>
#include <numbers>
#include <utility>

namespace tisma::cad {

Lathe::Lathe(int segments) {
    for (int i = 0; i < segments; ++i)
        angles_.push_back(2.0 * std::numbers::pi * i / segments);
}

Lathe::Lathe(std::vector<double> angles) : angles_(std::move(angles)) {}

TriangleMesh Lathe::mesh(const std::vector<Ring>& rings, bool loop) const {
    TriangleMesh m;
    const std::size_t n = angles_.size(), count = rings.size();
    if (n < 3 || count < 2)
        return m;
    m.reserve(count * n * 2 + 2 * n);
    // Every ring's points once, so shared corners are the same numbers.
    std::vector<std::vector<Vec3f>> points(count, std::vector<Vec3f>(n));
    for (std::size_t g = 0; g < count; ++g)
        for (std::size_t k = 0; k < n; ++k) {
            const double a = angles_[k] + rings[g].turn, r = rings[g].r[k];
            points[g][k] = {float(r * std::cos(a)), float(r * std::sin(a)), float(rings[g].z)};
        }
    const auto add = [&](const Vec3f& a, const Vec3f& b, const Vec3f& c) {
        if (a == b || b == c || a == c)
            return; // a ring that touches its neighbour there
        m.add_triangle(a, b, c);
    };
    const std::size_t joins = loop ? count : count - 1;
    for (std::size_t g = 0; g < joins; ++g) {
        const std::vector<Vec3f>& a = points[g];
        const std::vector<Vec3f>& b = points[(g + 1) % count];
        for (std::size_t i = 0; i < n; ++i) {
            const std::size_t j = (i + 1) % n;
            add(a[i], a[j], b[j]);
            add(a[i], b[j], b[i]);
        }
    }
    if (!loop) {
        const Vec3f c0{0, 0, float(rings.front().z)}, c1{0, 0, float(rings.back().z)};
        for (std::size_t i = 0; i < n; ++i) {
            const std::size_t j = (i + 1) % n;
            add(c0, points.front()[j], points.front()[i]);
            add(c1, points.back()[i], points.back()[j]);
        }
    }
    return m;
}

TriangleMesh outward(TriangleMesh m) {
    if (m.signed_volume() >= 0.0)
        return m;
    std::vector<Vec3f> corners = m.corners();
    for (std::size_t i = 0; i + 2 < corners.size(); i += 3)
        std::swap(corners[i + 1], corners[i + 2]);
    return TriangleMesh(std::move(corners));
}

} // namespace tisma::cad
