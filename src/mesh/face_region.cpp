// Own design; see PROVENANCE.md.

#include "mesh/face_region.hpp"

#include "mesh/indexed_mesh.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_map>
#include <utility>

namespace tisma {

namespace {

std::array<double, 3> unit_normal(const IndexedMesh& m, std::uint32_t t) {
    const auto& [a, b, c] = m.triangles[t];
    const Vec3f &p = m.vertices[a], &q = m.vertices[b], &r = m.vertices[c];
    const double u[3] = {double(q.x) - p.x, double(q.y) - p.y, double(q.z) - p.z};
    const double v[3] = {double(r.x) - p.x, double(r.y) - p.y, double(r.z) - p.z};
    std::array<double, 3> n{u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
    const double len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    if (len > 0.0)
        for (double& x : n)
            x /= len;
    return n;
}

} // namespace

std::vector<std::uint32_t> face_region(const TriangleMesh& mesh, std::uint32_t seed, double max_bend) {
    if (seed >= mesh.triangle_count())
        return {};
    const IndexedMesh m = weld_exact(mesh);
    // Each edge (its two vertices, the smaller first) and the triangles on it.
    std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> edges;
    edges.reserve(m.triangles.size() * 2);
    const auto key = [](std::uint32_t a, std::uint32_t b) {
        return (std::uint64_t(std::min(a, b)) << 32) | std::max(a, b);
    };
    for (std::uint32_t t = 0; t < m.triangles.size(); ++t)
        for (int k = 0; k < 3; ++k)
            edges[key(m.triangles[t][std::size_t(k)], m.triangles[t][std::size_t((k + 1) % 3)])].push_back(t);
    std::vector<std::array<double, 3>> normals(m.triangles.size());
    for (std::uint32_t t = 0; t < m.triangles.size(); ++t)
        normals[t] = unit_normal(m, t);
    const double min_cos = std::cos(max_bend * 3.14159265358979323846 / 180.0);
    std::vector<std::uint8_t> in(m.triangles.size(), 0);
    std::vector<std::uint32_t> stack{seed}, out;
    in[seed] = 1;
    while (!stack.empty()) {
        const std::uint32_t t = stack.back();
        stack.pop_back();
        out.push_back(t);
        const std::array<double, 3>& n = normals[t];
        if (n[0] == 0.0 && n[1] == 0.0 && n[2] == 0.0)
            continue; // degenerate: no direction to follow
        for (int k = 0; k < 3; ++k)
            for (const std::uint32_t u : edges[key(m.triangles[t][std::size_t(k)], m.triangles[t][std::size_t((k + 1) % 3)])]) {
                if (in[u])
                    continue;
                const std::array<double, 3>& o = normals[u];
                if (n[0] * o[0] + n[1] * o[1] + n[2] * o[2] >= min_cos) {
                    in[u] = 1;
                    stack.push_back(u);
                }
            }
    }
    std::sort(out.begin(), out.end());
    return out;
}

} // namespace tisma
