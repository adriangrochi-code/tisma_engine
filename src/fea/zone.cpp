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

SurfaceZone::SurfaceZone(const ZoneShape& shape, double /*spacing*/, double reach) {
    reach_ = std::max(reach, 1e-3);
    reach2_ = reach_ * reach_;
    disc_ = shape.disc;
    centre_ = shape.centre;
    disc_reach2_ = (shape.radius + reach_) * (shape.radius + reach_);
    double lo[3] = {std::numeric_limits<double>::max(), std::numeric_limits<double>::max(),
                    std::numeric_limits<double>::max()};
    double hi[3] = {-lo[0], -lo[1], -lo[2]};
    double edges = 0.0, area = 0.0;
    for (const Triangle3& t : shape.triangles) {
        V n = cross(sub(t[1], t[0]), sub(t[2], t[0]));
        const double len = std::sqrt(dot(n, n));
        area += 0.5 * len;
        normals_.push_back(len > 0.0 ? mul(n, 1.0 / len) : V{});
        triangles_.push_back(t);
        for (int k = 0; k < 3; ++k) {
            edges += std::sqrt(dist2(t[std::size_t(k)], t[std::size_t((k + 1) % 3)]));
            for (int a = 0; a < 3; ++a)
                lo[a] = std::min(lo[a], t[std::size_t(k)][std::size_t(a)]),
                hi[a] = std::max(hi[a], t[std::size_t(k)][std::size_t(a)]);
        }
    }
    if (triangles_.empty() && shape.disc) {
        centre_only_ = true; // the centre itself
        for (int a = 0; a < 3; ++a)
            lo[a] = hi[a] = shape.centre[std::size_t(a)];
    }
    if (disc_) // a disc: only what is near its centre
        for (int a = 0; a < 3; ++a) {
            lo[a] = std::max(lo[a], shape.centre[std::size_t(a)] - shape.radius);
            hi[a] = std::min(hi[a], shape.centre[std::size_t(a)] + shape.radius);
        }
    // The cells: twice the reach at least, the mean side of the triangles
    // (a few in each), and no more than about 20 000 of them over the area.
    const double mean_edge = triangles_.empty() ? 0.0 : edges / (3.0 * double(triangles_.size()));
    cell_ = std::max({2.0 * reach_, mean_edge, std::sqrt(area / 20000.0)});
    const auto cell_of = [&](double v) { return std::llround(std::floor(v / cell_)); };
    for (std::size_t t = 0; t < triangles_.size(); ++t) {
        double tlo[3], thi[3];
        for (int a = 0; a < 3; ++a) {
            tlo[a] = std::min({triangles_[t][0][std::size_t(a)], triangles_[t][1][std::size_t(a)],
                               triangles_[t][2][std::size_t(a)]});
            thi[a] = std::max({triangles_[t][0][std::size_t(a)], triangles_[t][1][std::size_t(a)],
                               triangles_[t][2][std::size_t(a)]});
            if (disc_) // the part of a large triangle near the disc
                tlo[a] = std::max(tlo[a], lo[a]), thi[a] = std::min(thi[a], hi[a]);
        }
        if (tlo[0] > thi[0] || tlo[1] > thi[1] || tlo[2] > thi[2])
            continue;
        for (long long i = cell_of(tlo[0]); i <= cell_of(thi[0]); ++i)
            for (long long j = cell_of(tlo[1]); j <= cell_of(thi[1]); ++j)
                for (long long k = cell_of(tlo[2]); k <= cell_of(thi[2]); ++k)
                    cells_[key(i, j, k)].push_back(std::uint32_t(t));
    }
    for (int i = 0; i < 3; ++i)
        bounds_[std::size_t(i)] = lo[i] - reach_, bounds_[std::size_t(i) + 3] = hi[i] + reach_;
}

bool SurfaceZone::near(double x, double y, double z) const { return find(x, y, z, nullptr); }

bool SurfaceZone::near_facing(double x, double y, double z, const std::array<double, 3>& normal) const {
    return find(x, y, z, &normal);
}

bool SurfaceZone::find(double x, double y, double z, const std::array<double, 3>* normal) const {
    if (x < bounds_[0] || y < bounds_[1] || z < bounds_[2] || x > bounds_[3] || y > bounds_[4] || z > bounds_[5])
        return false;
    const V p{x, y, z};
    if (disc_ && dist2(p, centre_) > disc_reach2_)
        return false;
    if (centre_only_)
        return dist2(p, centre_) <= reach2_;
    // The cells the ball of the reach around p touches (at most 2 a side).
    const auto cell_of = [&](double v) { return std::llround(std::floor(v / cell_)); };
    for (long long i = cell_of(x - reach_); i <= cell_of(x + reach_); ++i)
        for (long long j = cell_of(y - reach_); j <= cell_of(y + reach_); ++j)
            for (long long k = cell_of(z - reach_); k <= cell_of(z + reach_); ++k) {
                const auto it = cells_.find(key(i, j, k));
                if (it == cells_.end())
                    continue;
                for (const std::uint32_t t : it->second) {
                    const V& n = normals_[t];
                    if (normal && dot(n, n) > 0.0 && dot(n, *normal) <= 0.5)
                        continue;
                    if (dist2(p, closest_on_triangle(p, triangles_[t])) <= reach2_)
                        return true;
                }
            }
    return false;
}

} // namespace tisma
