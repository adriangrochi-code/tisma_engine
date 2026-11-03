// Own design; see PROVENANCE.md.

#include "fea/zone.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace tisma {

namespace {

using V = std::array<double, 3>;

V sub(const V& a, const V& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
V add(const V& a, const V& b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
V mul(const V& a, double s) { return {a[0] * s, a[1] * s, a[2] * s}; }
double dot(const V& a, const V& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
V cross(const V& a, const V& b) {
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
double dist2(const V& a, const V& b) { return dot(sub(a, b), sub(a, b)); }

// The point of segment ab nearest to p.
V closest_on_segment(const V& p, const V& a, const V& b) {
    const V ab = sub(b, a);
    const double len2 = dot(ab, ab);
    const double t = len2 > 0.0 ? std::clamp(dot(sub(p, a), ab) / len2, 0.0, 1.0) : 0.0;
    return add(a, mul(ab, t));
}

} // namespace

V closest_on_triangle(const V& p, const Triangle3& t) {
    const V e0 = sub(t[1], t[0]), e1 = sub(t[2], t[0]);
    const V n = cross(e0, e1);
    const double n2 = dot(n, n);
    if (n2 > 0.0) {
        // The projection on the plane, and whether it falls inside: the
        // signs of the three sub-triangles' areas against the normal.
        const V q = sub(p, mul(n, dot(sub(p, t[0]), n) / n2));
        const bool inside = dot(cross(sub(t[1], t[0]), sub(q, t[0])), n) >= 0.0 &&
                            dot(cross(sub(t[2], t[1]), sub(q, t[1])), n) >= 0.0 &&
                            dot(cross(sub(t[0], t[2]), sub(q, t[2])), n) >= 0.0;
        if (inside)
            return q;
    }
    // Outside (or a degenerate triangle): the nearest of its three sides.
    V best = closest_on_segment(p, t[0], t[1]);
    for (const auto& [a, b] : {std::pair{t[1], t[2]}, std::pair{t[2], t[0]}}) {
        const V c = closest_on_segment(p, a, b);
        if (dist2(p, c) < dist2(p, best))
            best = c;
    }
    return best;
}

std::vector<Triangle3> triangles_near(const std::vector<Triangle3>& mesh_triangles, const V& centre, double radius) {
    std::vector<Triangle3> out;
    for (const Triangle3& t : mesh_triangles)
        if (dist2(closest_on_triangle(centre, t), centre) <= radius * radius)
            out.push_back(t);
    return out;
}

std::int64_t SurfaceZone::key(long long i, long long j, long long k) const {
    // 21 bits a coordinate: cells of a part under 2 million cells across.
    const auto bits = [](long long v) { return std::uint64_t(v + (1LL << 20)) & ((1ULL << 21) - 1); };
    return std::int64_t((bits(i) << 42) | (bits(j) << 21) | bits(k));
}

SurfaceZone::SurfaceZone(const ZoneShape& shape, double spacing, double reach) {
    spacing = std::max(spacing, 1e-3);
    reach = std::max(reach, 1e-3);
    cell_ = reach;
    reach2_ = reach * reach;
    disc_ = shape.disc;
    centre_ = shape.centre;
    disc_reach2_ = (shape.radius + reach) * (shape.radius + reach);
    double lo[3] = {std::numeric_limits<double>::max(), std::numeric_limits<double>::max(),
                    std::numeric_limits<double>::max()};
    double hi[3] = {-lo[0], -lo[1], -lo[2]};
    const auto put = [&](const V& q) {
        if (disc_ && dist2(q, centre_) > disc_reach2_)
            return; // a disc: only what is near its centre
        cells_[key(std::llround(std::floor(q[0] / cell_)), std::llround(std::floor(q[1] / cell_)),
                   std::llround(std::floor(q[2] / cell_)))]
            .push_back(q);
        for (int a = 0; a < 3; ++a)
            lo[a] = std::min(lo[a], q[std::size_t(a)]), hi[a] = std::max(hi[a], q[std::size_t(a)]);
    };
    // Samples over each triangle: a grid of barycentric steps no more than
    // `spacing` apart along its two sides, its corners included.
    for (const Triangle3& t : shape.triangles) {
        const double a = std::sqrt(dist2(t[0], t[1])), b = std::sqrt(dist2(t[0], t[2]));
        const int n = std::clamp(int(std::ceil(std::max(a, b) / spacing)), 1, 4000);
        for (int i = 0; i <= n; ++i)
            for (int j = 0; i + j <= n; ++j) {
                const double u = double(i) / n, v = double(j) / n;
                put(add(t[0], add(mul(sub(t[1], t[0]), u), mul(sub(t[2], t[0]), v))));
            }
    }
    if (shape.disc) // the centre itself, if no triangle was given
        put(shape.centre);
    for (int i = 0; i < 3; ++i)
        bounds_[std::size_t(i)] = lo[i] - reach, bounds_[std::size_t(i) + 3] = hi[i] + reach;
}

bool SurfaceZone::near(double x, double y, double z) const {
    if (x < bounds_[0] || y < bounds_[1] || z < bounds_[2] || x > bounds_[3] || y > bounds_[4] || z > bounds_[5])
        return false;
    const V p{x, y, z};
    if (disc_ && dist2(p, centre_) > disc_reach2_)
        return false;
    const long long ci = std::llround(std::floor(x / cell_)), cj = std::llround(std::floor(y / cell_)),
                    ck = std::llround(std::floor(z / cell_));
    for (long long i = ci - 1; i <= ci + 1; ++i)
        for (long long j = cj - 1; j <= cj + 1; ++j)
            for (long long k = ck - 1; k <= ck + 1; ++k) {
                const auto it = cells_.find(key(i, j, k));
                if (it == cells_.end())
                    continue;
                for (const V& q : it->second)
                    if (dist2(p, q) <= reach2_)
                        return true;
            }
    return false;
}

} // namespace tisma
