// Iso-contours of a vertex field on a triangle mesh: linear interpolation along
// each crossed edge (marching triangles). Own design; see PROVENANCE.md.

#include "slice/level_set.hpp"

#include <cassert>
#include <utility>

namespace tisma {
namespace {

Vec3f crossing(const IndexedMesh& mesh, std::span<const float> values, float iso, std::uint32_t a, std::uint32_t b) {
    if (b < a)
        std::swap(a, b);
    const Vec3f& pa = mesh.vertices[a];
    const Vec3f& pb = mesh.vertices[b];
    // One end is >= iso and the other < iso, so the denominator is never zero.
    const double t = (double(iso) - values[a]) / (double(values[b]) - values[a]);
    return {float(pa.x + t * (double(pb.x) - pa.x)), float(pa.y + t * (double(pb.y) - pa.y)),
            float(pa.z + t * (double(pb.z) - pa.z))};
}

} // namespace

namespace {

void triangle_segment(const IndexedMesh& mesh, std::span<const float> values, float iso, std::uint32_t t,
                      std::vector<LevelSegment>& out) {
    const auto& tri = mesh.triangles[t];
    bool above[3];
    int count = 0;
    for (int k = 0; k < 3; ++k) {
        above[k] = values[tri[std::size_t(k)]] >= iso;
        count += above[k] ? 1 : 0;
    }
    if (count == 0 || count == 3)
        return;
    // Walking the triangle in winding order, the contour leaves through the
    // edge that goes from above to below and enters through the edge that
    // goes from below to above.
    LevelSegment seg{};
    seg.triangle = t;
    for (int k = 0; k < 3; ++k) {
        const std::uint32_t a = tri[std::size_t(k)];
        const std::uint32_t b = tri[std::size_t((k + 1) % 3)];
        if (above[k] == above[(k + 1) % 3])
            continue;
        if (above[k]) {
            seg.from = crossing(mesh, values, iso, a, b);
            seg.edge_from = edge_key(a, b);
        } else {
            seg.to = crossing(mesh, values, iso, a, b);
            seg.edge_to = edge_key(a, b);
        }
    }
    out.push_back(seg);
}

} // namespace

void level_set_segments(const IndexedMesh& mesh, std::span<const float> values, float iso,
                        std::vector<LevelSegment>& out) {
    assert(values.size() == mesh.vertices.size());
    for (std::size_t t = 0; t < mesh.triangles.size(); ++t)
        triangle_segment(mesh, values, iso, static_cast<std::uint32_t>(t), out);
}

void level_set_segments(const IndexedMesh& mesh, std::span<const float> values, float iso,
                        std::span<const std::uint32_t> triangles, std::vector<LevelSegment>& out) {
    assert(values.size() == mesh.vertices.size());
    for (const std::uint32_t t : triangles)
        triangle_segment(mesh, values, iso, t, out);
}

std::vector<float> height_field(const IndexedMesh& mesh) {
    std::vector<float> z(mesh.vertices.size());
    for (std::size_t i = 0; i < z.size(); ++i)
        z[i] = mesh.vertices[i].z;
    return z;
}

} // namespace tisma
