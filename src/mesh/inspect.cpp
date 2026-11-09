// A mesh's summary and its flat faces. Own design; see PROVENANCE.md.

#include "mesh/inspect.hpp"

#include "mesh/indexed_mesh.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <numeric>
#include <sstream>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace tisma {
namespace {

using V3 = std::array<double, 3>;

V3 at(const Vec3f& p) { return {double(p.x), double(p.y), double(p.z)}; }
V3 minus(const V3& a, const V3& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
V3 cross(const V3& a, const V3& b) {
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
double dot(const V3& a, const V3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
double length(const V3& a) { return std::sqrt(dot(a, a)); }

// A triangle's unit normal and twice its area (0 for a degenerate one).
std::pair<V3, double> normal_of(const TriangleMesh& mesh, std::size_t t) {
    const V3 a = at(mesh.corner(t, 0));
    const V3 n = cross(minus(at(mesh.corner(t, 1)), a), minus(at(mesh.corner(t, 2)), a));
    const double l = length(n);
    if (!(l > 0.0))
        return {{0.0, 0.0, 0.0}, 0.0};
    return {{n[0] / l, n[1] / l, n[2] / l}, l};
}

std::string number(double v) {
    std::ostringstream s;
    s.precision(10);
    s << (std::abs(v) < 5e-10 ? 0.0 : v);
    return s.str();
}

} // namespace

MeshInspection inspect_mesh(const TriangleMesh& mesh) {
    MeshInspection out;
    out.triangles = mesh.triangle_count();
    if (mesh.empty())
        return out;
    const BoundingBox box = mesh.bounding_box();
    out.size = {double(box.max.x) - box.min.x, double(box.max.y) - box.min.y, double(box.max.z) - box.min.z};
    out.volume = std::abs(mesh.signed_volume());
    const IndexedMesh indexed = weld_exact(mesh);
    // Each undirected edge and how many triangles use it.
    std::unordered_map<std::uint64_t, std::uint32_t> uses;
    uses.reserve(indexed.triangles.size() * 3);
    for (const auto& t : indexed.triangles)
        for (int k = 0; k < 3; ++k) {
            std::uint64_t a = t[std::size_t(k)], b = t[std::size_t((k + 1) % 3)];
            if (a > b)
                std::swap(a, b);
            ++uses[(a << 32) | b];
        }
    for (const auto& [edge, n] : uses)
        out.open_edges += n == 2 ? 0 : 1;
    out.closed = out.open_edges == 0;
    // Parts: corners joined through triangles (union-find).
    std::vector<std::uint32_t> parent(indexed.vertices.size());
    std::iota(parent.begin(), parent.end(), 0u);
    const auto root = [&](std::uint32_t v) {
        while (parent[v] != v)
            v = parent[v] = parent[parent[v]];
        return v;
    };
    for (const auto& t : indexed.triangles)
        for (int k = 1; k < 3; ++k)
            parent[root(t[std::size_t(k)])] = root(t[0]);
    std::vector<bool> used(indexed.vertices.size(), false), is_root(indexed.vertices.size(), false);
    for (const auto& t : indexed.triangles)
        for (const auto v : t)
            used[v] = true;
    for (std::uint32_t v = 0; v < parent.size(); ++v)
        if (used[v] && !is_root[root(v)]) {
            is_root[root(v)] = true;
            ++out.parts;
        }
    return out;
}

std::string inspection_json(const MeshInspection& m) {
    std::ostringstream s;
    s << "{\"size\": [" << number(m.size[0]) << ", " << number(m.size[1]) << ", " << number(m.size[2])
      << "], \"volume\": " << number(m.volume) << ", \"triangles\": " << m.triangles
      << ", \"closed\": " << (m.closed ? "true" : "false") << ", \"open_edges\": " << m.open_edges
      << ", \"parts\": " << m.parts << "}";
    return s.str();
}

std::optional<std::array<double, 3>> largest_flat_face(const TriangleMesh& mesh) {
    // Planes by their normal (to about 0.5 degrees) and offset (0.01 mm).
    constexpr double kNormalStep = 0.01, kOffsetStep = 0.01;
    std::map<std::tuple<long, long, long, long>, std::pair<double, V3>> planes;
    for (std::size_t t = 0; t < mesh.triangle_count(); ++t) {
        const auto [n, area2] = normal_of(mesh, t);
        if (!(area2 > 0.0))
            continue;
        const double d = dot(n, at(mesh.corner(t, 0)));
        auto& [area, sum] = planes[{std::lround(n[0] / kNormalStep), std::lround(n[1] / kNormalStep),
                                    std::lround(n[2] / kNormalStep), std::lround(d / kOffsetStep)}];
        area += 0.5 * area2;
        for (int i = 0; i < 3; ++i)
            sum[std::size_t(i)] += n[std::size_t(i)] * area2;
    }
    if (planes.empty())
        return std::nullopt;
    const auto best = std::max_element(planes.begin(), planes.end(),
                                       [](const auto& a, const auto& b) { return a.second.first < b.second.first; });
    const V3 sum = best->second.second;
    const double l = length(sum);
    return V3{sum[0] / l, sum[1] / l, sum[2] / l};
}

std::optional<std::array<double, 3>> face_through(const TriangleMesh& mesh, const std::array<double, 3>& point) {
    constexpr double kTolerance = 0.01;
    for (std::size_t t = 0; t < mesh.triangle_count(); ++t) {
        const auto [n, area2] = normal_of(mesh, t);
        if (!(area2 > 0.0))
            continue;
        const V3 a = at(mesh.corner(t, 0)), b = at(mesh.corner(t, 1)), c = at(mesh.corner(t, 2));
        if (std::abs(dot(n, minus(point, a))) > kTolerance)
            continue;
        // Inside: on the inner side of each edge, within the tolerance.
        bool inside = true;
        for (const auto& [p, q] : {std::pair{a, b}, std::pair{b, c}, std::pair{c, a}}) {
            const V3 edge = minus(q, p);
            const V3 inward = cross(n, edge);
            const double l = length(inward);
            if (l > 0.0 && dot(inward, minus(point, p)) / l < -kTolerance)
                inside = false;
        }
        if (inside)
            return n;
    }
    return std::nullopt;
}

TriangleMesh laid_on_face(const TriangleMesh& mesh, std::array<double, 3> normal) {
    const double l = length(normal);
    if (!(l > 0.0))
        return mesh;
    for (double& v : normal)
        v /= l;
    const V3 down{0.0, 0.0, -1.0};
    const double c = dot(normal, down);
    if (c > 1.0 - 1e-12)
        return mesh;
    // Rodrigues' rotation about normal x down by the angle between them; a
    // normal straight up turns half a turn about X.
    V3 axis = cross(normal, down);
    double s = length(axis);
    if (s < 1e-12)
        axis = {1.0, 0.0, 0.0}, s = 0.0;
    else
        for (double& v : axis)
            v /= s;
    const double cos_a = s == 0.0 ? -1.0 : c, sin_a = s;
    const auto turn = [&](const Vec3f& p) {
        const V3 v = at(p);
        const V3 k_v = cross(axis, v);
        const double k_dot = dot(axis, v);
        V3 r;
        for (std::size_t i = 0; i < 3; ++i)
            r[i] = v[i] * cos_a + k_v[i] * sin_a + axis[i] * k_dot * (1.0 - cos_a);
        return Vec3f{float(r[0]), float(r[1]), float(r[2])};
    };
    TriangleMesh out;
    out.reserve(mesh.triangle_count());
    for (std::size_t t = 0; t < mesh.triangle_count(); ++t)
        out.add_triangle(turn(mesh.corner(t, 0)), turn(mesh.corner(t, 1)), turn(mesh.corner(t, 2)));
    return out;
}

} // namespace tisma
