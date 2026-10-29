#include "slice/transform.hpp"

#include "mesh/indexed_mesh.hpp"
#include "slice/contours.hpp"
#include "slice/level_set.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <utility>

namespace tisma {
namespace {

using Point = std::array<double, 2>;

double cross(const Point& o, const Point& a, const Point& b) {
    return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0]);
}

double signed_area(const std::vector<Point>& ring) {
    double a = 0.0;
    for (std::size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++)
        a += ring[j][0] * ring[i][1] - ring[i][0] * ring[j][1];
    return 0.5 * a;
}

bool inside_ring(const std::vector<Point>& ring, const Point& p) {
    bool in = false;
    for (std::size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++)
        if ((ring[i][1] > p[1]) != (ring[j][1] > p[1]) &&
            p[0] < (ring[j][0] - ring[i][0]) * (p[1] - ring[i][1]) / (ring[j][1] - ring[i][1]) + ring[i][0])
            in = !in;
    return in;
}

// Whether p is inside the triangle (a, b, c) turning counter-clockwise, its
// edges included.
bool in_triangle(const Point& a, const Point& b, const Point& c, const Point& p) {
    return cross(a, b, p) >= 0.0 && cross(b, c, p) >= 0.0 && cross(c, a, p) >= 0.0;
}

TriangleMesh rebuilt(const TriangleMesh& mesh, bool turn) {
    TriangleMesh out;
    out.reserve(mesh.triangle_count());
    for (std::size_t t = 0; t < mesh.triangle_count(); ++t)
        if (turn)
            out.add_triangle(mesh.corner(t, 0), mesh.corner(t, 2), mesh.corner(t, 1));
        else
            out.add_triangle(mesh.corner(t, 0), mesh.corner(t, 1), mesh.corner(t, 2));
    return out;
}

template <class F>
TriangleMesh mapped(const TriangleMesh& mesh, F f) {
    TriangleMesh out;
    out.reserve(mesh.triangle_count());
    for (std::size_t t = 0; t < mesh.triangle_count(); ++t)
        out.add_triangle(f(mesh.corner(t, 0)), f(mesh.corner(t, 1)), f(mesh.corner(t, 2)));
    return out;
}

// Whether a ray from p crosses the triangle (Möller and Trumbore 1997). The
// ray leans a little off the X axis, so that it does not run along edges.
bool ray_hits(const std::array<double, 3>& p, const Vec3f& a, const Vec3f& b, const Vec3f& c) {
    const std::array<double, 3> d{1.0, 1.3e-4, 0.7e-4};
    const std::array<double, 3> e1{double(b.x) - a.x, double(b.y) - a.y, double(b.z) - a.z};
    const std::array<double, 3> e2{double(c.x) - a.x, double(c.y) - a.y, double(c.z) - a.z};
    const std::array<double, 3> h{d[1] * e2[2] - d[2] * e2[1], d[2] * e2[0] - d[0] * e2[2], d[0] * e2[1] - d[1] * e2[0]};
    const double det = e1[0] * h[0] + e1[1] * h[1] + e1[2] * h[2];
    if (std::abs(det) < 1e-18)
        return false;
    const double inv = 1.0 / det;
    const std::array<double, 3> s{p[0] - a.x, p[1] - a.y, p[2] - a.z};
    const double u = inv * (s[0] * h[0] + s[1] * h[1] + s[2] * h[2]);
    if (u < 0.0 || u > 1.0)
        return false;
    const std::array<double, 3> q{s[1] * e1[2] - s[2] * e1[1], s[2] * e1[0] - s[0] * e1[2], s[0] * e1[1] - s[1] * e1[0]};
    const double v = inv * (d[0] * q[0] + d[1] * q[1] + d[2] * q[2]);
    if (v < 0.0 || u + v > 1.0)
        return false;
    return inv * (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]) > 0.0;
}

} // namespace

std::array<double, 3> box_centre(const TriangleMesh& mesh) {
    const BoundingBox box = mesh.bounding_box();
    if (box.empty)
        return {0.0, 0.0, 0.0};
    return {0.5 * (double(box.min.x) + box.max.x), 0.5 * (double(box.min.y) + box.max.y),
            0.5 * (double(box.min.z) + box.max.z)};
}

namespace {

void scale_about(TriangleMesh& mesh, double sx, double sy, double sz, const std::array<double, 3>& c) {
    TriangleMesh out = mapped(mesh, [&](const Vec3f& v) {
        return Vec3f{float(c[0] + (v.x - c[0]) * sx), float(c[1] + (v.y - c[1]) * sy), float(c[2] + (v.z - c[2]) * sz)};
    });
    const int negative = (sx < 0.0) + (sy < 0.0) + (sz < 0.0);
    mesh = negative % 2 ? rebuilt(out, true) : std::move(out);
}

void rotate_about(TriangleMesh& mesh, double x_degrees, double y_degrees, double z_degrees, const std::array<double, 3>& c) {
    constexpr double kDegree = 3.14159265358979323846 / 180.0;
    const double cx = std::cos(x_degrees * kDegree), sx = std::sin(x_degrees * kDegree);
    const double cy = std::cos(y_degrees * kDegree), sy = std::sin(y_degrees * kDegree);
    const double cz = std::cos(z_degrees * kDegree), sz = std::sin(z_degrees * kDegree);
    mesh = mapped(mesh, [&](const Vec3f& v) {
        double x = v.x - c[0], y = v.y - c[1], z = v.z - c[2];
        double t = y * cx - z * sx;
        z = y * sx + z * cx, y = t;
        t = x * cy + z * sy;
        z = -x * sy + z * cy, x = t;
        t = x * cz - y * sz;
        y = x * sz + y * cz, x = t;
        return Vec3f{float(c[0] + x), float(c[1] + y), float(c[2] + z)};
    });
}

} // namespace

void scale_mesh(TriangleMesh& mesh, double sx, double sy, double sz) {
    scale_about(mesh, sx, sy, sz, box_centre(mesh));
}

double fit_factor(const TriangleMesh& mesh, const std::array<double, 3>& size) {
    const BoundingBox box = mesh.bounding_box();
    if (box.empty)
        return 1.0;
    const double own[3] = {double(box.max.x) - box.min.x, double(box.max.y) - box.min.y, double(box.max.z) - box.min.z};
    double f = std::numeric_limits<double>::infinity();
    for (int k = 0; k < 3; ++k)
        if (own[k] > 0.0)
            f = std::min(f, size[std::size_t(k)] / own[k]);
    return std::isfinite(f) ? f : 1.0;
}

void rotate_mesh(TriangleMesh& mesh, double x_degrees, double y_degrees, double z_degrees) {
    rotate_about(mesh, x_degrees, y_degrees, z_degrees, box_centre(mesh));
}

void mirror_mesh(TriangleMesh& mesh, int axis) {
    scale_mesh(mesh, axis == 0 ? -1.0 : 1.0, axis == 1 ? -1.0 : 1.0, axis == 2 ? -1.0 : 1.0);
}

std::vector<TriangleMesh> apply_transform(TriangleMesh mesh, const Settings& s, TriangleMesh* follower) {
    if (follower && !follower->empty() && (s.transform_cut_z || s.transform_split))
        throw std::invalid_argument("negative volumes do not go with transform.cut_z or transform.split yet");
    double f = s.transform_scale;
    if (s.transform_scale_to_fit)
        f *= fit_factor(mesh, *s.transform_scale_to_fit);
    // About the part's centre; what follows it, about the same point.
    if (f != 1.0) {
        const std::array<double, 3> c = box_centre(mesh);
        scale_about(mesh, f, f, f, c);
        if (follower)
            scale_about(*follower, f, f, f, c);
    }
    if (s.transform_rotate != std::array<double, 3>{0.0, 0.0, 0.0}) {
        const std::array<double, 3> c = box_centre(mesh);
        rotate_about(mesh, s.transform_rotate[0], s.transform_rotate[1], s.transform_rotate[2], c);
        if (follower)
            rotate_about(*follower, s.transform_rotate[0], s.transform_rotate[1], s.transform_rotate[2], c);
    }
    std::vector<TriangleMesh> out;
    if (s.transform_cut_z && !mesh.empty()) {
        // The height counts from the part's lowest point, where it sits on the bed.
        CutResult cut = cut_mesh(mesh, double(mesh.bounding_box().min.z) + *s.transform_cut_z);
        for (TriangleMesh* half : {&cut.below, &cut.above})
            if (!half->empty())
                out.push_back(std::move(*half));
    } else {
        out.push_back(std::move(mesh));
    }
    if (s.transform_split) {
        std::vector<TriangleMesh> pieces;
        for (const TriangleMesh& m : out)
            for (TriangleMesh& p : split_mesh(m))
                pieces.push_back(std::move(p));
        out = std::move(pieces);
    }
    return out;
}

std::vector<std::array<std::size_t, 3>> triangulate_polygon(const std::vector<Point>& outer,
                                                            const std::vector<std::vector<Point>>& holes) {
    std::vector<std::array<std::size_t, 3>> out;
    // Every point, the outer's and then each hole's.
    std::vector<Point> points = outer;
    std::vector<std::size_t> hole_start;
    for (const std::vector<Point>& h : holes) {
        hole_start.push_back(points.size());
        points.insert(points.end(), h.begin(), h.end());
    }
    // The ring to clip, as indices: the outer, then each hole joined to it by
    // a bridge from its rightmost point to a point of the ring it sees.
    std::vector<std::size_t> ring(outer.size());
    std::iota(ring.begin(), ring.end(), std::size_t(0));
    std::vector<std::size_t> order(holes.size());
    std::iota(order.begin(), order.end(), std::size_t(0));
    const auto rightmost = [&](std::size_t h) {
        std::size_t best = 0;
        for (std::size_t k = 1; k < holes[h].size(); ++k)
            if (holes[h][k][0] > holes[h][best][0])
                best = k;
        return best;
    };
    std::sort(order.begin(), order.end(),
              [&](std::size_t a, std::size_t b) { return holes[a][rightmost(a)][0] > holes[b][rightmost(b)][0]; });
    for (const std::size_t h : order) {
        if (holes[h].size() < 3)
            continue;
        const std::size_t mk = rightmost(h);
        const std::size_t m = hole_start[h] + mk;
        const Point& M = points[m];
        // The nearest edge the ray to +x meets, going up (the inside on its left).
        double best_x = std::numeric_limits<double>::infinity();
        std::size_t edge = ring.size();
        for (std::size_t i = 0; i < ring.size(); ++i) {
            const Point& a = points[ring[i]];
            const Point& b = points[ring[(i + 1) % ring.size()]];
            if (!(a[1] <= M[1] && M[1] <= b[1]) || a[1] == b[1])
                continue;
            const double x = a[0] + (M[1] - a[1]) * (b[0] - a[0]) / (b[1] - a[1]);
            if (x >= M[0] && x < best_x)
                best_x = x, edge = i;
        }
        if (edge == ring.size())
            continue; // not inside: left out
        // The edge's end farther right, unless a reflex point of the ring lies
        // in the triangle it makes with the ray: then the one nearest the ray.
        const std::size_t ia = edge, ib = (edge + 1) % ring.size();
        std::size_t bridge = points[ring[ia]][0] > points[ring[ib]][0] ? ia : ib;
        const Point I{best_x, M[1]};
        const Point P = points[ring[bridge]];
        double best_angle = std::numeric_limits<double>::infinity();
        for (std::size_t i = 0; i < ring.size(); ++i) {
            const Point& r = points[ring[i]];
            const Point& before = points[ring[(i + ring.size() - 1) % ring.size()]];
            const Point& after = points[ring[(i + 1) % ring.size()]];
            if (i == bridge || cross(before, r, after) > 0.0)
                continue; // convex
            const bool in = P[1] > M[1] ? in_triangle(M, I, P, r) : in_triangle(M, P, I, r);
            if (!in || r == M)
                continue;
            const double angle = std::atan2(std::abs(r[1] - M[1]), r[0] - M[0]);
            if (angle < best_angle) {
                best_angle = angle;
                bridge = i;
            }
        }
        // ring: ..., P, M, the hole on from M, M, P, ...
        std::vector<std::size_t> joined;
        joined.reserve(ring.size() + holes[h].size() + 2);
        joined.insert(joined.end(), ring.begin(), ring.begin() + std::ptrdiff_t(bridge) + 1);
        for (std::size_t k = 0; k <= holes[h].size(); ++k)
            joined.push_back(hole_start[h] + (mk + k) % holes[h].size());
        joined.insert(joined.end(), ring.begin() + std::ptrdiff_t(bridge), ring.end());
        ring = std::move(joined);
    }
    // Ear clipping on a linked ring.
    const std::size_t n = ring.size();
    if (n < 3)
        return out;
    std::vector<std::size_t> prev(n), next(n);
    for (std::size_t i = 0; i < n; ++i)
        prev[i] = (i + n - 1) % n, next[i] = (i + 1) % n;
    const auto is_ear = [&](std::size_t i) {
        const Point& a = points[ring[prev[i]]];
        const Point& b = points[ring[i]];
        const Point& c = points[ring[next[i]]];
        if (cross(a, b, c) < 0.0)
            return false;
        for (std::size_t j = next[next[i]]; j != prev[i]; j = next[j]) {
            const Point& p = points[ring[j]];
            if (p == a || p == b || p == c)
                continue; // the same point again, through a bridge
            if (in_triangle(a, b, c, p))
                return false;
        }
        return true;
    };
    std::size_t left = n, i = 0, tried = 0;
    while (left > 3) {
        if (is_ear(i) || tried >= left) {
            // A ring with no ear left (it crosses itself) is clipped anyway.
            out.push_back({ring[prev[i]], ring[i], ring[next[i]]});
            next[prev[i]] = next[i];
            prev[next[i]] = prev[i];
            i = prev[i];
            --left;
            tried = 0;
        } else {
            i = next[i];
            ++tried;
        }
    }
    out.push_back({ring[prev[i]], ring[i], ring[next[i]]});
    return out;
}

CutResult cut_mesh(const TriangleMesh& source, double height) {
    CutResult out;
    const IndexedMesh mesh = weld_exact(source);
    const std::vector<float> z = height_field(mesh);
    const float iso = float(height);
    const auto vertex = [&](std::uint32_t i) { return mesh.vertices[i]; };
    const auto crossing = [&](std::uint32_t a, std::uint32_t b) { return edge_crossing(mesh, z, iso, edge_key(a, b)); };
    for (const auto& t : mesh.triangles) {
        const bool up[3] = {z[t[0]] >= iso, z[t[1]] >= iso, z[t[2]] >= iso};
        const int ups = up[0] + up[1] + up[2];
        if (ups == 0 || ups == 3) {
            (ups == 3 ? out.above : out.below).add_triangle(vertex(t[0]), vertex(t[1]), vertex(t[2]));
            continue;
        }
        // The corner alone on its side, then the other two in winding order.
        int k = 0;
        while (up[k] == up[(k + 1) % 3] || up[k] == up[(k + 2) % 3])
            ++k;
        const std::uint32_t a = t[std::size_t(k)], b = t[std::size_t((k + 1) % 3)], c = t[std::size_t((k + 2) % 3)];
        const Vec3f pab = crossing(a, b), pac = crossing(a, c);
        TriangleMesh& alone = up[k] ? out.above : out.below;
        TriangleMesh& pair = up[k] ? out.below : out.above;
        alone.add_triangle(vertex(a), pab, pac);
        pair.add_triangle(pab, vertex(b), vertex(c));
        pair.add_triangle(pab, vertex(c), pac);
    }
    if (out.below.empty() || out.above.empty())
        return out;
    // The section's faces, from its contours: up under the part below, down
    // over the part above.
    std::vector<LevelSegment> segments;
    level_set_segments(mesh, z, iso, segments);
    const std::vector<Contour> contours = join_segments(segments);
    std::vector<std::vector<Point>> rings;
    std::vector<const Contour*> of;
    for (const Contour& c : contours)
        if (c.closed && c.points.size() >= 3) {
            std::vector<Point> ring;
            ring.reserve(c.points.size());
            for (const Vec3f& p : c.points)
                ring.push_back({p.x, p.y});
            rings.push_back(std::move(ring));
            of.push_back(&c);
        }
    std::vector<double> area(rings.size());
    for (std::size_t i = 0; i < rings.size(); ++i)
        area[i] = signed_area(rings[i]);
    std::vector<std::vector<std::size_t>> holes_of(rings.size());
    for (std::size_t h = 0; h < rings.size(); ++h) {
        if (area[h] >= 0.0)
            continue;
        // The smallest outer around it.
        std::size_t best = rings.size();
        for (std::size_t o = 0; o < rings.size(); ++o)
            if (area[o] > 0.0 && inside_ring(rings[o], rings[h].front()) &&
                (best == rings.size() || area[o] < area[best]))
                best = o;
        if (best < rings.size())
            holes_of[best].push_back(h);
    }
    for (std::size_t o = 0; o < rings.size(); ++o) {
        if (area[o] <= 0.0)
            continue;
        std::vector<std::vector<Point>> holes;
        std::vector<const Contour*> sources{of[o]};
        for (const std::size_t h : holes_of[o]) {
            holes.push_back(rings[h]);
            sources.push_back(of[h]);
        }
        std::vector<Vec3f> corners;
        for (const Contour* c : sources)
            corners.insert(corners.end(), c->points.begin(), c->points.end());
        for (const auto& tri : triangulate_polygon(rings[o], holes)) {
            out.below.add_triangle(corners[tri[0]], corners[tri[1]], corners[tri[2]]);
            out.above.add_triangle(corners[tri[0]], corners[tri[2]], corners[tri[1]]);
        }
    }
    return out;
}

std::vector<TriangleMesh> split_mesh(const TriangleMesh& source) {
    std::vector<TriangleMesh> pieces;
    const IndexedMesh mesh = weld_exact(source);
    if (mesh.triangles.empty())
        return pieces;
    // The shells: triangles joined through shared corners.
    std::vector<std::uint32_t> parent(mesh.vertices.size());
    std::iota(parent.begin(), parent.end(), 0u);
    const auto root = [&](std::uint32_t v) {
        while (parent[v] != v)
            v = parent[v] = parent[parent[v]];
        return v;
    };
    for (const auto& t : mesh.triangles) {
        const std::uint32_t a = root(t[0]);
        parent[root(t[1])] = a;
        parent[root(t[2])] = a;
    }
    std::vector<std::size_t> shell_of_root(mesh.vertices.size(), SIZE_MAX);
    std::vector<std::vector<std::size_t>> shells;
    for (std::size_t t = 0; t < mesh.triangles.size(); ++t) {
        const std::uint32_t r = root(mesh.triangles[t][0]);
        if (shell_of_root[r] == SIZE_MAX) {
            shell_of_root[r] = shells.size();
            shells.emplace_back();
        }
        shells[shell_of_root[r]].push_back(t);
    }
    // Which shells hold which: a shell inside an odd number of others is a
    // cavity of the innermost of them.
    const std::size_t n = shells.size();
    std::vector<std::vector<std::size_t>> inside(n);
    std::vector<double> volume(n, 0.0);
    for (std::size_t s = 0; s < n; ++s)
        for (const std::size_t t : shells[s]) {
            const Vec3f& a = mesh.vertices[mesh.triangles[t][0]];
            const Vec3f& b = mesh.vertices[mesh.triangles[t][1]];
            const Vec3f& c = mesh.vertices[mesh.triangles[t][2]];
            volume[s] += (double(a.x) * (double(b.y) * c.z - double(b.z) * c.y) -
                          double(a.y) * (double(b.x) * c.z - double(b.z) * c.x) +
                          double(a.z) * (double(b.x) * c.y - double(b.y) * c.x)) /
                         6.0;
        }
    if (n > 1)
        for (std::size_t s = 0; s < n; ++s) {
            const Vec3f& v = mesh.vertices[mesh.triangles[shells[s].front()][0]];
            const std::array<double, 3> p{v.x, v.y, v.z};
            for (std::size_t o = 0; o < n; ++o) {
                if (o == s)
                    continue;
                int hits = 0;
                for (const std::size_t t : shells[o])
                    hits += ray_hits(p, mesh.vertices[mesh.triangles[t][0]], mesh.vertices[mesh.triangles[t][1]],
                                     mesh.vertices[mesh.triangles[t][2]]);
                if (hits % 2)
                    inside[s].push_back(o);
            }
        }
    std::vector<std::size_t> piece_of(n, SIZE_MAX);
    for (std::size_t s = 0; s < n; ++s)
        if (inside[s].size() % 2 == 0) {
            piece_of[s] = pieces.size();
            pieces.emplace_back();
        }
    for (std::size_t s = 0; s < n; ++s)
        if (piece_of[s] == SIZE_MAX) {
            // The smallest shell around the cavity.
            std::size_t best = inside[s].front();
            for (const std::size_t o : inside[s])
                if (std::abs(volume[o]) < std::abs(volume[best]))
                    best = o;
            piece_of[s] = piece_of[best] != SIZE_MAX ? piece_of[best] : 0;
        }
    for (std::size_t s = 0; s < n; ++s)
        for (const std::size_t t : shells[s]) {
            const auto& tri = mesh.triangles[t];
            pieces[piece_of[s]].add_triangle(mesh.vertices[tri[0]], mesh.vertices[tri[1]], mesh.vertices[tri[2]]);
        }
    return pieces;
}

} // namespace tisma
