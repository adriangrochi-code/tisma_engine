#include "cad/view_dxf.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <tuple>
#include <unordered_map>

namespace tisma::cad {

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

// Triangles for the ray test, under a bounding volume hierarchy (median
// splits on the longest axis).
struct Rays {
    std::vector<V> a, e1, e2;
    struct Node {
        V lo, hi;
        int left = -1, right = -1, first = 0, count = 0;
    };
    std::vector<Node> nodes;
    std::vector<int> order;

    int split(int first, int count) {
        Node node;
        node.lo = {1e300, 1e300, 1e300};
        node.hi = {-1e300, -1e300, -1e300};
        V clo = node.lo, chi = node.hi;
        for (int k = first; k < first + count; ++k) {
            const std::size_t t = std::size_t(order[std::size_t(k)]);
            const V corners[3] = {a[t], add(a[t], e1[t]), add(a[t], e2[t])};
            const V centre = mul(add(add(corners[0], corners[1]), corners[2]), 1.0 / 3.0);
            for (std::size_t d = 0; d < 3; ++d) {
                for (const V& q : corners) {
                    node.lo[d] = std::min(node.lo[d], q[d]);
                    node.hi[d] = std::max(node.hi[d], q[d]);
                }
                clo[d] = std::min(clo[d], centre[d]);
                chi[d] = std::max(chi[d], centre[d]);
            }
        }
        const int index = int(nodes.size());
        nodes.push_back(node);
        if (count <= 4) {
            nodes[std::size_t(index)].first = first;
            nodes[std::size_t(index)].count = count;
            return index;
        }
        std::size_t axis = 0;
        for (std::size_t d = 1; d < 3; ++d)
            if (chi[d] - clo[d] > chi[axis] - clo[axis])
                axis = d;
        const int half = count / 2;
        const auto centre = [&](int t) {
            const std::size_t s = std::size_t(t);
            return 3.0 * a[s][axis] + e1[s][axis] + e2[s][axis];
        };
        std::nth_element(order.begin() + first, order.begin() + first + half, order.begin() + first + count,
                         [&](int x, int y) { return centre(x) < centre(y); });
        const int left = split(first, half);
        const int right = split(first + half, count - half);
        nodes[std::size_t(index)].left = left;
        nodes[std::size_t(index)].right = right;
        return index;
    }

    // Whether a triangle other than `skip` lies along origin + t dir, t in
    // (from, infinity).
    bool blocked(const V& origin, const V& dir, double from, const std::vector<std::uint32_t>& skip) const {
        if (nodes.empty())
            return false;
        int stack[128];
        int top = 0;
        stack[top++] = 0;
        V inv;
        for (std::size_t d = 0; d < 3; ++d)
            inv[d] = 1.0 / (dir[d] != 0 ? dir[d] : 1e-300);
        while (top > 0) {
            const Node& node = nodes[std::size_t(stack[--top])];
            double t0 = 0.0, t1 = 1e300;
            bool miss = false;
            for (std::size_t d = 0; d < 3 && !miss; ++d) {
                double ta = (node.lo[d] - origin[d]) * inv[d];
                double tb = (node.hi[d] - origin[d]) * inv[d];
                if (ta > tb)
                    std::swap(ta, tb);
                t0 = std::max(t0, ta);
                t1 = std::min(t1, tb);
                miss = t0 > t1;
            }
            if (miss)
                continue;
            if (node.left >= 0) {
                if (top + 2 <= 128) {
                    stack[top++] = node.left;
                    stack[top++] = node.right;
                }
                continue;
            }
            for (int k = node.first; k < node.first + node.count; ++k) {
                const std::size_t t = std::size_t(order[std::size_t(k)]);
                if (std::find(skip.begin(), skip.end(), std::uint32_t(t)) != skip.end())
                    continue;
                // Möller and Trumbore.
                const V pv = cross(dir, e2[t]);
                const double det = dot(e1[t], pv);
                if (std::abs(det) < 1e-14)
                    continue;
                const double id = 1.0 / det;
                const V tv = sub(origin, a[t]);
                const double u = dot(tv, pv) * id;
                if (u < 0.0 || u > 1.0)
                    continue;
                const V qv = cross(tv, e1[t]);
                const double v = dot(dir, qv) * id;
                if (v < 0.0 || u + v > 1.0)
                    continue;
                if (dot(e2[t], qv) * id > from)
                    return true;
            }
        }
        return false;
    }
};

} // namespace

std::vector<ViewLine> view_lines(const TriangleMesh& mesh, const ViewDxfOptions& options) {
    std::vector<ViewLine> lines;
    const std::size_t n = mesh.triangle_count();
    const V look = unit(options.look);
    V right = unit(cross(look, options.up));
    if (norm(right) == 0.0)
        right = unit(cross(look, std::abs(look[2]) < 0.9 ? V{0, 0, 1} : V{1, 0, 0}));
    const V up = cross(right, look);
    if (n == 0 || norm(look) == 0.0)
        return lines;
    const BoundingBox box = mesh.bounding_box();
    const double size = std::max(1e-9, double(norm(V{box.max.x - box.min.x, box.max.y - box.min.y, box.max.z - box.min.z})));

    // The corners welded (within a millionth of the size), the triangles'
    // normals and the ray test.
    const double q = size * 1e-6;
    std::unordered_map<std::uint64_t, std::uint32_t> welded;
    std::vector<V> points;
    std::vector<std::array<std::uint32_t, 3>> tri(n);
    std::vector<V> normal(n);
    Rays rays;
    rays.a.resize(n), rays.e1.resize(n), rays.e2.resize(n);
    for (std::size_t t = 0; t < n; ++t) {
        V c[3];
        for (int k = 0; k < 3; ++k) {
            const Vec3f& p = mesh.corner(t, k);
            c[k] = {double(p.x), double(p.y), double(p.z)};
            const auto cell = [&](double v, double lo) { return std::uint64_t(std::llround((v - lo) / q)) & 0x1FFFFF; };
            const std::uint64_t key = cell(c[k][0], double(box.min.x)) | cell(c[k][1], double(box.min.y)) << 21 |
                                      cell(c[k][2], double(box.min.z)) << 42;
            const auto [it, added] = welded.try_emplace(key, std::uint32_t(points.size()));
            if (added)
                points.push_back(c[k]);
            tri[t][std::size_t(k)] = it->second;
        }
        rays.a[t] = c[0], rays.e1[t] = sub(c[1], c[0]), rays.e2[t] = sub(c[2], c[0]);
        normal[t] = unit(cross(rays.e1[t], rays.e2[t]));
    }
    rays.order.resize(n);
    for (std::size_t t = 0; t < n; ++t)
        rays.order[t] = int(t);
    rays.nodes.reserve(2 * n);
    rays.split(0, int(n));

    // The edges and the triangles on each.
    std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> edges;
    edges.reserve(2 * n);
    for (std::size_t t = 0; t < n; ++t)
        for (std::size_t k = 0; k < 3; ++k) {
            std::uint32_t i = tri[t][k], j = tri[t][(k + 1) % 3];
            if (i == j)
                continue;
            if (i > j)
                std::swap(i, j);
            edges[std::uint64_t(i) << 32 | j].push_back(std::uint32_t(t));
        }

    // The edges a drawing shows: borders and folds, folds sharper than the
    // crease angle, and silhouettes (one side faces the eye, the other not).
    const double crease = std::cos(options.crease_degrees * 3.14159265358979323846 / 180.0);
    const double eps = size * 1e-4;
    const double nudge = size * 1e-5;
    const double step = size / 400.0;
    const auto to2d = [&](const V& p) { return std::array<double, 2>{dot(p, right), dot(p, up)}; };
    for (const auto& [key, faces] : edges) {
        bool shown = faces.size() != 2;
        if (!shown) {
            const V& na = normal[faces[0]];
            const V& nb = normal[faces[1]];
            shown = dot(na, nb) < crease || (dot(na, look) < 0.0) != (dot(nb, look) < 0.0);
        }
        if (!shown)
            continue;
        const V p0 = points[std::size_t(key >> 32)], p1 = points[std::size_t(key & 0xFFFFFFFFu)];
        const auto a = to2d(p0), b = to2d(p1);
        const double length = std::hypot(b[0] - a[0], b[1] - a[1]);
        if (length < eps)
            continue; // seen end on
        // From just beside the edge toward the eye, its own faces left out.
        // Beside it inside the part for a border or a fold: an edge that runs
        // along a face seen edge on (the back of a box seen square on) is then
        // behind the face's front border. Outside for a true silhouette (one
        // face toward the eye, the other away), where inside the ray would
        // leave through the next facets of a curved side.
        const V towards = mul(look, -1.0);
        V out{0, 0, 0};
        for (std::uint32_t f : faces)
            out = add(out, normal[f]);
        out = unit(out);
        bool silhouette = false;
        if (faces.size() == 2) {
            const double fa = dot(normal[faces[0]], look), fb = dot(normal[faces[1]], look);
            silhouette = (fa < -1e-9 && fb > 1e-9) || (fa > 1e-9 && fb < -1e-9);
        }
        const V beside = mul(out, silhouette ? nudge : -nudge);
        const int pieces = std::clamp(int(std::ceil(length / step)), 1, 256);
        int start = 0;
        bool start_hidden = false;
        for (int k = 0; k <= pieces; ++k) {
            bool hidden = false;
            if (k < pieces) {
                const double s = (k + 0.5) / pieces;
                const V p = add(add(p0, mul(sub(p1, p0), s)), beside);
                hidden = rays.blocked(p, towards, nudge * 0.01, faces);
                if (k == 0) {
                    start_hidden = hidden;
                    continue;
                }
                if (hidden == start_hidden)
                    continue;
            }
            if (options.hidden || !start_hidden) {
                const double s0 = double(start) / pieces, s1 = double(k) / pieces;
                lines.push_back({a[0] + (b[0] - a[0]) * s0, a[1] + (b[1] - a[1]) * s0, a[0] + (b[0] - a[0]) * s1,
                                 a[1] + (b[1] - a[1]) * s1, start_hidden});
            }
            start = k;
            start_hidden = hidden;
        }
    }
    // The same drawing every time (the edge map has no order).
    std::sort(lines.begin(), lines.end(), [](const ViewLine& l, const ViewLine& r) {
        return std::tie(l.hidden, l.x0, l.y0, l.x1, l.y1) < std::tie(r.hidden, r.x0, r.y0, r.x1, r.y1);
    });
    return lines;
}

std::string write_view_dxf(const std::vector<ViewLine>& lines) {
    std::string out;
    const auto group = [&](int code, const std::string& value) {
        out += std::to_string(code);
        out += '\n';
        out += value;
        out += '\n';
    };
    const auto num = [&](int code, double v) {
        char buf[64];
        const auto [end, ec] = std::to_chars(buf, buf + sizeof buf, std::abs(v) < 1e-12 ? 0.0 : v, std::chars_format::general, 12);
        group(code, ec == std::errc() ? std::string(buf, end) : std::string("0"));
    };
    group(0, "SECTION");
    group(2, "HEADER");
    group(9, "$ACADVER");
    group(1, "AC1009");
    group(9, "$INSUNITS");
    group(70, "4");
    group(0, "ENDSEC");
    // Line types and layers: the hidden edges dashed, 3 mm on and 1.5 off.
    group(0, "SECTION");
    group(2, "TABLES");
    group(0, "TABLE");
    group(2, "LTYPE");
    group(70, "2");
    group(0, "LTYPE");
    group(2, "CONTINUOUS");
    group(70, "0");
    group(3, "Solid line");
    group(72, "65");
    group(73, "0");
    num(40, 0.0);
    group(0, "LTYPE");
    group(2, "DASHED");
    group(70, "0");
    group(3, "Dashed __ __ __");
    group(72, "65");
    group(73, "2");
    num(40, 4.5);
    num(49, 3.0);
    num(49, -1.5);
    group(0, "ENDTAB");
    group(0, "TABLE");
    group(2, "LAYER");
    group(70, "2");
    group(0, "LAYER");
    group(2, "VISIBLE");
    group(70, "0");
    group(62, "7");
    group(6, "CONTINUOUS");
    group(0, "LAYER");
    group(2, "HIDDEN");
    group(70, "0");
    group(62, "8");
    group(6, "DASHED");
    group(0, "ENDTAB");
    group(0, "ENDSEC");
    group(0, "SECTION");
    group(2, "ENTITIES");
    for (const ViewLine& l : lines) {
        group(0, "LINE");
        group(8, l.hidden ? "HIDDEN" : "VISIBLE");
        num(10, l.x0), num(20, l.y0), num(30, 0);
        num(11, l.x1), num(21, l.y1), num(31, 0);
    }
    group(0, "ENDSEC");
    group(0, "EOF");
    return out;
}

} // namespace tisma::cad
