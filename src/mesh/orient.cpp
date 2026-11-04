// Automatic orientation for least support; see orient.hpp. Own design over
// docs/spec/features/orientation.md: an incremental convex hull, a bounding
// volume hierarchy for the downward rays (Möller and Trumbore's ray-triangle
// test, J. Graphics Tools 2, 1997) and the minimum-area rectangle over the
// hull's edges (Freeman and Shapira, CACM 18, 1975). See PROVENANCE.md.

#include "mesh/orient.hpp"

#include "config/settings.hpp"
#include "geom/polygons.hpp"
#include "mesh/indexed_mesh.hpp"
#include "resin/resin.hpp"

#include <oneapi/tbb/blocked_range.h>
#include <oneapi/tbb/parallel_reduce.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <numbers>
#include <optional>
#include <utility>
#include <vector>

namespace tisma {

namespace {

using V = std::array<double, 3>;

V sub(const V& a, const V& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
V add(const V& a, const V& b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
V mul(const V& a, double s) { return {a[0] * s, a[1] * s, a[2] * s}; }
double dot(const V& a, const V& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
V cross(const V& a, const V& b) { return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]}; }
double norm(const V& a) { return std::sqrt(dot(a, a)); }
V unit(const V& a) {
    const double n = norm(a);
    return n > 0.0 ? mul(a, 1.0 / n) : V{0, 0, 0};
}
V of(const Vec3f& v) { return {double(v.x), double(v.y), double(v.z)}; }

// The triangles once, with what every candidate needs.
struct Prepared {
    std::vector<V> a, e1, e2;  // corner and edges, for the ray test
    std::vector<V> normal;     // unit
    std::vector<V> centroid;
    std::vector<double> area;
    std::vector<V> vertices;   // the corners, unique enough for the extents
};

// A bounding volume hierarchy over the triangles: median splits on the
// longest axis of the centroids' box.
struct Bvh {
    struct Node {
        V lo, hi;
        int left = -1, right = -1; // children, or -1 for a leaf
        int first = 0, count = 0;  // the leaf's triangles in `order`
    };
    std::vector<Node> nodes;
    std::vector<int> order;

    void build(const Prepared& p) {
        const int n = int(p.area.size());
        order.resize(std::size_t(n));
        for (int i = 0; i < n; ++i)
            order[std::size_t(i)] = i;
        nodes.reserve(std::size_t(2 * n));
        if (n > 0)
            split(p, 0, n);
    }

    int split(const Prepared& p, int first, int count) {
        Node node;
        node.lo = {1e300, 1e300, 1e300};
        node.hi = {-1e300, -1e300, -1e300};
        V clo = node.lo, chi = node.hi;
        for (int k = first; k < first + count; ++k) {
            const int t = order[std::size_t(k)];
            for (const V& q : {p.a[std::size_t(t)], add(p.a[std::size_t(t)], p.e1[std::size_t(t)]), add(p.a[std::size_t(t)], p.e2[std::size_t(t)])})
                for (int d = 0; d < 3; ++d) {
                    node.lo[std::size_t(d)] = std::min(node.lo[std::size_t(d)], q[std::size_t(d)]);
                    node.hi[std::size_t(d)] = std::max(node.hi[std::size_t(d)], q[std::size_t(d)]);
                }
            for (int d = 0; d < 3; ++d) {
                clo[std::size_t(d)] = std::min(clo[std::size_t(d)], p.centroid[std::size_t(t)][std::size_t(d)]);
                chi[std::size_t(d)] = std::max(chi[std::size_t(d)], p.centroid[std::size_t(t)][std::size_t(d)]);
            }
        }
        const int index = int(nodes.size());
        nodes.push_back(node);
        if (count <= 4) {
            nodes[std::size_t(index)].first = first;
            nodes[std::size_t(index)].count = count;
            return index;
        }
        int axis = 0;
        for (int d = 1; d < 3; ++d)
            if (chi[std::size_t(d)] - clo[std::size_t(d)] > chi[std::size_t(axis)] - clo[std::size_t(axis)])
                axis = d;
        const int half = count / 2;
        std::nth_element(order.begin() + first, order.begin() + first + half, order.begin() + first + count, [&](int x, int y) {
            return p.centroid[std::size_t(x)][std::size_t(axis)] < p.centroid[std::size_t(y)][std::size_t(axis)];
        });
        const int left = split(p, first, half);
        const int right = split(p, first + half, count - half);
        nodes[std::size_t(index)].left = left;
        nodes[std::size_t(index)].right = right;
        return index;
    }

    // The nearest hit along origin + t dir, t in (0, limit), not `skip`.
    double nearest(const Prepared& p, const V& origin, const V& dir, double limit, int skip) const {
        if (nodes.empty())
            return limit;
        double best = limit;
        int stack[128];
        int top = 0;
        stack[top++] = 0;
        const V inv{1.0 / (dir[0] != 0 ? dir[0] : 1e-300), 1.0 / (dir[1] != 0 ? dir[1] : 1e-300), 1.0 / (dir[2] != 0 ? dir[2] : 1e-300)};
        while (top > 0) {
            const Node& node = nodes[std::size_t(stack[--top])];
            double t0 = 0.0, t1 = best;
            bool miss = false;
            for (int d = 0; d < 3 && !miss; ++d) {
                double ta = (node.lo[std::size_t(d)] - origin[std::size_t(d)]) * inv[std::size_t(d)];
                double tb = (node.hi[std::size_t(d)] - origin[std::size_t(d)]) * inv[std::size_t(d)];
                if (ta > tb)
                    std::swap(ta, tb);
                t0 = std::max(t0, ta);
                t1 = std::min(t1, tb);
                miss = t0 > t1;
            }
            if (miss)
                continue;
            if (node.left < 0) {
                for (int k = node.first; k < node.first + node.count; ++k) {
                    const int t = order[std::size_t(k)];
                    if (t == skip)
                        continue;
                    // Möller and Trumbore.
                    const V& e1 = p.e1[std::size_t(t)];
                    const V& e2 = p.e2[std::size_t(t)];
                    const V pv = cross(dir, e2);
                    const double det = dot(e1, pv);
                    if (std::abs(det) < 1e-14)
                        continue;
                    const double id = 1.0 / det;
                    const V tv = sub(origin, p.a[std::size_t(t)]);
                    const double u = dot(tv, pv) * id;
                    if (u < 0.0 || u > 1.0)
                        continue;
                    const V qv = cross(tv, e1);
                    const double v = dot(dir, qv) * id;
                    if (v < 0.0 || u + v > 1.0)
                        continue;
                    const double hit = dot(e2, qv) * id;
                    if (hit > 1e-6 && hit < best)
                        best = hit;
                }
            } else if (top + 2 <= 128) {
                stack[top++] = node.left;
                stack[top++] = node.right;
            }
        }
        return best;
    }
};

Prepared prepare(const TriangleMesh& mesh) {
    Prepared p;
    const std::size_t n = mesh.triangle_count();
    p.a.reserve(n), p.e1.reserve(n), p.e2.reserve(n), p.normal.reserve(n), p.centroid.reserve(n), p.area.reserve(n);
    for (std::size_t t = 0; t < n; ++t) {
        const V a = of(mesh.corner(t, 0)), b = of(mesh.corner(t, 1)), c = of(mesh.corner(t, 2));
        const V e1 = sub(b, a), e2 = sub(c, a);
        const V cr = cross(e1, e2);
        p.a.push_back(a), p.e1.push_back(e1), p.e2.push_back(e2);
        p.normal.push_back(unit(cr));
        p.area.push_back(0.5 * norm(cr));
        p.centroid.push_back(mul(add(add(a, b), c), 1.0 / 3.0));
    }
    // A mesh turned inside out (every triangle wound the other way): its
    // signed volume is negative; its normals are flipped so "looking down"
    // means what it means for the part.
    double volume = 0.0;
    for (std::size_t t = 0; t < n; ++t)
        volume += dot(p.a[t], cross(add(p.a[t], p.e1[t]), add(p.a[t], p.e2[t])));
    if (volume < 0.0)
        for (V& normal : p.normal)
            normal = mul(normal, -1.0);
    // Unique corners (exact).
    std::vector<V> all;
    all.reserve(3 * n);
    for (const Vec3f& v : mesh.corners())
        all.push_back(of(v));
    std::sort(all.begin(), all.end());
    all.erase(std::unique(all.begin(), all.end()), all.end());
    p.vertices = std::move(all);
    return p;
}

OrientCost cost_of(const Prepared& p, const Bvh& bvh, const V& up, const OrientOptions& o) {
    OrientCost r;
    double lo = std::numeric_limits<double>::infinity(), hi = -lo;
    for (const V& v : p.vertices) {
        const double h = dot(v, up);
        lo = std::min(lo, h);
        hi = std::max(hi, h);
    }
    if (p.vertices.empty())
        return r;
    r.height = hi - lo;
    const double limit = std::cos(std::clamp(o.support_angle, 0.0, 90.0) * std::numbers::pi / 180.0);
    const V down = mul(up, -1.0);
    // The pieces' size: a hundredth of the part's size, at least 0.5 mm.
    const double step = std::max(0.5, r.height / 100.0 + 1e-9);
    struct Sum {
        double support = 0.0, bed = 0.0;
    };
    const Sum total = tbb::parallel_reduce(
        tbb::blocked_range<std::size_t>(0, p.area.size(), 256), Sum{},
        [&](const tbb::blocked_range<std::size_t>& range, Sum s) {
            for (std::size_t t = range.begin(); t < range.end(); ++t) {
                const double c = dot(p.normal[t], up);
                if (c >= -limit || p.area[t] <= 0.0)
                    continue; // not looking down steeply enough to need support
                const double h = dot(p.centroid[t], up) - lo;
                if (h < 0.05) {
                    if (c < -0.999)
                        s.bed += p.area[t];
                    continue;
                }
                // Large faces in pieces about `step` wide, a ray from each
                // piece's centre: part of a face may be over the part and
                // part over the bed.
                const V& a = p.a[t];
                const V& e1 = p.e1[t];
                const V& e2 = p.e2[t];
                const double longest = std::max({norm(e1), norm(e2), norm(sub(e2, e1))});
                const int n = std::clamp(int(std::ceil(longest / step)), 1, 24);
                const double piece = p.area[t] / double(n * n);
                for (int i = 0; i < n; ++i)
                    for (int j = 0; i + j < n; ++j)
                        for (int flip = 0; flip < (i + j + 1 < n ? 2 : 1); ++flip) {
                            const double u = flip == 0 ? (i + 1.0 / 3.0) / n : (i + 2.0 / 3.0) / n;
                            const double v = flip == 0 ? (j + 1.0 / 3.0) / n : (j + 2.0 / 3.0) / n;
                            const V q = add(a, add(mul(e1, u), mul(e2, v)));
                            const double hq = dot(q, up) - lo;
                            if (hq <= 0.0)
                                continue;
                            // From just inside the part above the face: a
                            // face of the part right under it (two pieces
                            // touching) stops the ray at once.
                            const double reach = bvh.nearest(p, add(q, mul(up, 1e-4)), down, hq + 1e-4, int(t)) - 1e-4;
                            s.support += piece * (-c) * std::clamp(reach, 0.0, hq);
                        }
            }
            return s;
        },
        [](Sum x, const Sum& y) {
            x.support += y.support;
            x.bed += y.bed;
            return x;
        });
    r.support_mm3 = total.support;
    r.bed_area_mm2 = total.bed;
    r.cost = total.support - total.bed * 1.0;
    return r;
}

// The convex hull's outward face normals, incrementally (own code).
std::vector<V> hull_normals(std::vector<V> points) {
    std::vector<V> out;
    if (points.size() < 4)
        return out;
    // Many points: the extremes along evenly spread directions (a Fibonacci
    // sphere) are the hull's corners that matter.
    if (points.size() > 3000) {
        std::vector<V> kept;
        const int dirs = 1500;
        const double golden = std::numbers::pi * (3.0 - std::sqrt(5.0));
        for (int i = 0; i < dirs; ++i) {
            const double z = 1.0 - 2.0 * (i + 0.5) / dirs, r = std::sqrt(1.0 - z * z), phi = golden * i;
            const V d{r * std::cos(phi), r * std::sin(phi), z};
            std::size_t best = 0;
            for (std::size_t k = 1; k < points.size(); ++k)
                if (dot(points[k], d) > dot(points[best], d))
                    best = k;
            kept.push_back(points[best]);
        }
        std::sort(kept.begin(), kept.end());
        kept.erase(std::unique(kept.begin(), kept.end()), kept.end());
        points = std::move(kept);
    }
    // The starting tetrahedron.
    double span = 0.0;
    std::size_t i0 = 0, i1 = 0;
    for (std::size_t k = 1; k < points.size(); ++k)
        if (const double d = norm(sub(points[k], points[0])); d > span)
            span = d, i1 = k;
    if (span <= 0.0)
        return out;
    const double eps = 1e-9 * span;
    std::size_t i2 = 0;
    double best = 0.0;
    for (std::size_t k = 0; k < points.size(); ++k)
        if (const double d = norm(cross(sub(points[i1], points[i0]), sub(points[k], points[i0]))); d > best)
            best = d, i2 = k;
    if (best <= eps * span)
        return out;
    std::size_t i3 = 0;
    best = 0.0;
    const V plane = cross(sub(points[i1], points[i0]), sub(points[i2], points[i0]));
    for (std::size_t k = 0; k < points.size(); ++k)
        if (const double d = std::abs(dot(plane, sub(points[k], points[i0]))); d > best)
            best = d, i3 = k;
    if (best <= eps * norm(plane))
        return out; // flat
    struct Face {
        std::size_t a, b, c;
        V n;
        double offset;
    };
    const V inside = mul(add(add(points[i0], points[i1]), add(points[i2], points[i3])), 0.25);
    std::vector<Face> faces;
    const auto make = [&](std::size_t a, std::size_t b, std::size_t c) {
        Face f{a, b, c, unit(cross(sub(points[b], points[a]), sub(points[c], points[a]))), 0.0};
        if (dot(f.n, sub(inside, points[a])) > 0.0) {
            std::swap(f.b, f.c);
            f.n = mul(f.n, -1.0);
        }
        f.offset = dot(f.n, points[f.a]);
        return f;
    };
    faces.push_back(make(i0, i1, i2));
    faces.push_back(make(i0, i1, i3));
    faces.push_back(make(i0, i2, i3));
    faces.push_back(make(i1, i2, i3));
    for (std::size_t k = 0; k < points.size(); ++k) {
        if (k == i0 || k == i1 || k == i2 || k == i3)
            continue;
        const V& q = points[k];
        std::vector<char> visible(faces.size(), 0);
        bool any = false;
        for (std::size_t f = 0; f < faces.size(); ++f)
            if (dot(faces[f].n, q) - faces[f].offset > eps)
                visible[f] = 1, any = true;
        if (!any)
            continue;
        // The horizon: edges of visible faces whose reverse is not on one.
        std::map<std::pair<std::size_t, std::size_t>, int> edges;
        for (std::size_t f = 0; f < faces.size(); ++f)
            if (visible[f]) {
                const Face& g = faces[f];
                ++edges[{g.a, g.b}], ++edges[{g.b, g.c}], ++edges[{g.c, g.a}];
            }
        std::vector<Face> kept;
        kept.reserve(faces.size() + 8);
        for (std::size_t f = 0; f < faces.size(); ++f)
            if (!visible[f])
                kept.push_back(faces[f]);
        for (const auto& [e, n] : edges)
            if (!edges.contains({e.second, e.first})) {
                Face f{e.first, e.second, k, unit(cross(sub(points[e.second], points[e.first]), sub(q, points[e.first]))), 0.0};
                f.offset = dot(f.n, points[f.a]);
                kept.push_back(f);
            }
        faces = std::move(kept);
    }
    for (const Face& f : faces) {
        bool same = false;
        for (const V& n : out)
            if (dot(n, f.n) > 0.9998) {
                same = true;
                break;
            }
        if (!same && norm(f.n) > 0.5)
            out.push_back(f.n);
    }
    return out;
}

// The rotation taking `from` (unit) onto +Z, row-major.
std::array<double, 9> onto_z(const V& from) {
    const V z{0, 0, 1};
    const double c = dot(from, z);
    if (c > 1.0 - 1e-12)
        return {1, 0, 0, 0, 1, 0, 0, 0, 1};
    if (c < -1.0 + 1e-12)
        return {1, 0, 0, 0, -1, 0, 0, 0, -1}; // half a turn about X
    const V k = unit(cross(from, z));
    const double s = std::sqrt(std::max(0.0, 1.0 - c * c)), t = 1.0 - c;
    return {t * k[0] * k[0] + c,        t * k[0] * k[1] - s * k[2], t * k[0] * k[2] + s * k[1],
            t * k[0] * k[1] + s * k[2], t * k[1] * k[1] + c,        t * k[1] * k[2] - s * k[0],
            t * k[0] * k[2] - s * k[1], t * k[1] * k[2] + s * k[0], t * k[2] * k[2] + c};
}

V rotate_by(const std::array<double, 9>& r, const V& v) {
    return {r[0] * v[0] + r[1] * v[1] + r[2] * v[2], r[3] * v[0] + r[4] * v[1] + r[5] * v[2], r[6] * v[0] + r[7] * v[1] + r[8] * v[2]};
}

std::array<double, 9> compose(const std::array<double, 9>& a, const std::array<double, 9>& b) { // a then b: b·a
    std::array<double, 9> out{};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            for (int k = 0; k < 3; ++k)
                out[std::size_t(i * 3 + j)] += b[std::size_t(i * 3 + k)] * a[std::size_t(k * 3 + j)];
    return out;
}

// Rule 2: the angle about Z that lays the smallest rectangle round the
// points' XY shadow along X (landscape).
double landscape_angle(const std::vector<V>& points) {
    std::vector<std::pair<double, double>> p;
    p.reserve(points.size());
    for (const V& v : points)
        p.emplace_back(v[0], v[1]);
    std::sort(p.begin(), p.end());
    p.erase(std::unique(p.begin(), p.end()), p.end());
    if (p.size() < 3)
        return 0.0;
    // Monotone chain hull.
    const auto turn = [](const std::pair<double, double>& o, const std::pair<double, double>& a, const std::pair<double, double>& b) {
        return (a.first - o.first) * (b.second - o.second) - (a.second - o.second) * (b.first - o.first);
    };
    std::vector<std::pair<double, double>> h(2 * p.size());
    std::size_t k = 0;
    for (std::size_t i = 0; i < p.size(); ++i) {
        while (k >= 2 && turn(h[k - 2], h[k - 1], p[i]) <= 0)
            --k;
        h[k++] = p[i];
    }
    for (std::size_t i = p.size() - 1, t = k + 1; i > 0; --i) {
        while (k >= t && turn(h[k - 2], h[k - 1], p[i - 1]) <= 0)
            --k;
        h[k++] = p[i - 1];
    }
    h.resize(k - 1);
    double best_area = std::numeric_limits<double>::infinity(), best_angle = 0.0;
    for (std::size_t i = 0; i < h.size(); ++i) {
        const auto& a = h[i];
        const auto& b = h[(i + 1) % h.size()];
        const double angle = std::atan2(b.second - a.second, b.first - a.first);
        const double c = std::cos(-angle), s = std::sin(-angle);
        double x0 = 1e300, x1 = -1e300, y0 = 1e300, y1 = -1e300;
        for (const auto& q : h) {
            const double x = c * q.first - s * q.second, y = s * q.first + c * q.second;
            x0 = std::min(x0, x), x1 = std::max(x1, x), y0 = std::min(y0, y), y1 = std::max(y1, y);
        }
        const double area = (x1 - x0) * (y1 - y0);
        if (area < best_area - 1e-9) {
            best_area = area;
            // Turning by -angle lays this edge along X; if the box is
            // taller than wide, a quarter turn more.
            best_angle = -angle + ((y1 - y0) > (x1 - x0) + 1e-9 ? std::numbers::pi / 2 : 0.0);
        }
    }
    return best_angle;
}

} // namespace

OrientCost orientation_cost(const TriangleMesh& mesh, const std::array<double, 3>& up, const OrientOptions& options) {
    const Prepared p = prepare(mesh);
    Bvh bvh;
    bvh.build(p);
    return cost_of(p, bvh, unit(up), options);
}

OrientResult orient_least_support(const TriangleMesh& mesh, const OrientOptions& options) {
    OrientResult result;
    if (mesh.triangle_count() == 0)
        return result;
    const Prepared p = prepare(mesh);
    Bvh bvh;
    bvh.build(p);
    // Rule 1 and 3: as it is, and resting on each hull face.
    std::vector<V> ups{{0, 0, 1}};
    for (const V& n : hull_normals(p.vertices))
        ups.push_back(mul(n, -1.0));
    result.candidates = ups.size();
    const OrientCost before = cost_of(p, bvh, ups[0], options);
    result.cost_before = before.cost;
    // Rule 4: the cheapest; those touching the bed with less than 10 mm² are
    // avoided while another is left; ties go to the lowest.
    std::size_t best = 0;
    OrientCost best_cost = before;
    bool best_grips = before.bed_area_mm2 >= 10.0;
    for (std::size_t i = 1; i < ups.size(); ++i) {
        const OrientCost c = cost_of(p, bvh, ups[i], options);
        const bool grips = c.bed_area_mm2 >= 10.0;
        const double tie = 1e-6 * std::max(1.0, std::abs(best_cost.cost));
        bool better;
        if (grips != best_grips)
            better = grips;
        else if (std::abs(c.cost - best_cost.cost) <= tie)
            better = c.height < best_cost.height - 1e-9;
        else
            better = c.cost < best_cost.cost;
        if (better)
            best = i, best_cost = c, best_grips = grips;
    }
    result.cost = best_cost.cost;
    result.support_mm3 = best_cost.support_mm3;
    result.bed_area_mm2 = best_cost.bed_area_mm2;
    // Rule 2: up onto +Z, then landscape about Z.
    std::array<double, 9> r = onto_z(ups[best]);
    std::vector<V> turned;
    turned.reserve(p.vertices.size());
    for (const V& v : p.vertices)
        turned.push_back(rotate_by(r, v));
    const double a = landscape_angle(turned);
    const std::array<double, 9> rz{std::cos(a), -std::sin(a), 0, std::sin(a), std::cos(a), 0, 0, 0, 1};
    result.rotation = compose(r, rz);
    return result;
}

namespace {

// Rule 2 for any chosen up: onto +Z, then landscape about Z.
std::array<double, 9> final_rotation(const std::vector<V>& vertices, const V& up) {
    const std::array<double, 9> r = onto_z(up);
    std::vector<V> turned;
    turned.reserve(vertices.size());
    for (const V& v : vertices)
        turned.push_back(rotate_by(r, v));
    const double a = landscape_angle(turned);
    const std::array<double, 9> rz{std::cos(a), -std::sin(a), 0, std::sin(a), std::cos(a), 0, 0, 0, 1};
    return compose(r, rz);
}

TriangleMesh turned_mesh(const TriangleMesh& mesh, const std::array<double, 9>& r) {
    TriangleMesh out;
    out.reserve(mesh.triangle_count());
    for (std::size_t t = 0; t < mesh.triangle_count(); ++t) {
        Vec3f c[3];
        for (int k = 0; k < 3; ++k) {
            const V v = rotate_by(r, of(mesh.corner(t, k)));
            c[k] = {float(v[0]), float(v[1]), float(v[2])};
        }
        out.add_triangle(c[0], c[1], c[2]);
    }
    return out;
}

} // namespace

PeelResult orient_least_peel(const TriangleMesh& mesh, const Settings& settings, double precision) {
    PeelResult result;
    if (mesh.triangle_count() == 0)
        return result;
    const Prepared p = prepare(mesh);
    Bvh bvh;
    bvh.build(p);
    const bool raised = settings.resin_supports && settings.resin_support_elevation > 0.0 && !settings.resin_pad_around_object;
    // Rule 7: on the platform, the hull's faces; raised, directions spread
    // evenly over the sphere (a Fibonacci lattice); and as it is.
    std::vector<V> ups{{0, 0, 1}};
    if (!raised) {
        for (const V& n : hull_normals(p.vertices))
            ups.push_back(mul(n, -1.0));
    } else {
        const int count = 200;
        const double golden = std::numbers::pi * (3.0 - std::sqrt(5.0));
        for (int i = 0; i < count; ++i) {
            const double z = 1.0 - 2.0 * (i + 0.5) / count, r = std::sqrt(1.0 - z * z), phi = golden * i;
            ups.push_back({r * std::cos(phi), r * std::sin(phi), z});
        }
    }
    // Rule 8: the best by rule 4's cost, the current one always kept.
    OrientOptions options;
    options.support_angle = settings.resin_support_critical_angle > 0.0 ? settings.resin_support_critical_angle : 45.0;
    std::vector<std::pair<double, std::size_t>> ranked;
    for (std::size_t i = 1; i < ups.size(); ++i)
        ranked.emplace_back(cost_of(p, bvh, ups[i], options).cost, i);
    std::sort(ranked.begin(), ranked.end());
    const std::size_t keep = std::size_t(std::max(4, int(std::lround(16.0 * std::clamp(precision, 0.0, 1.0)))));
    std::vector<std::size_t> chosen{0};
    for (std::size_t k = 0; k < ranked.size() && chosen.size() < keep + 1; ++k)
        chosen.push_back(ranked[k].second);
    result.candidates = chosen.size();
    // Rule 9: each sliced whole; fewest cups, then the smallest largest layer.
    struct Score {
        int cups = 0;
        double largest = 0.0, height = 0.0;
        std::array<double, 9> rotation{};
    };
    std::optional<Score> best;
    for (const std::size_t i : chosen) {
        Score s;
        s.rotation = final_rotation(p.vertices, ups[i]);
        const TriangleMesh turned = turned_mesh(mesh, s.rotation);
        const BoundingBox box = turned.bounding_box();
        s.height = double(box.max.z - box.min.z);
        Settings coarse = settings;
        coarse.resin_layer_height = std::max(settings.resin_layer_height > 0.0 ? settings.resin_layer_height : 0.05, s.height / 400.0);
        const std::vector<Polygons> sections = resin_sections(weld_exact(turned), coarse);
        s.cups = int(find_cups(sections, coarse.resin_layer_height, raised).size());
        for (const Polygons& layer : sections)
            s.largest = std::max(s.largest, area_mm2(layer));
        if (i == 0) {
            result.cups_before = s.cups;
            result.largest_layer_before_mm2 = s.largest;
        }
        const bool better = !best || s.cups < best->cups ||
                            (s.cups == best->cups && (s.largest < best->largest - 1e-6 ||
                                                      (std::abs(s.largest - best->largest) <= 1e-6 && s.height < best->height - 1e-9)));
        if (better)
            best = s;
    }
    result.rotation = best->rotation;
    result.cups = best->cups;
    result.largest_layer_mm2 = best->largest;
    // The cost the cloud reports: the cups first, then the largest layer.
    result.cost = 1e6 * best->cups + best->largest;
    result.cost_before = 1e6 * result.cups_before + result.largest_layer_before_mm2;
    return result;
}

} // namespace tisma
