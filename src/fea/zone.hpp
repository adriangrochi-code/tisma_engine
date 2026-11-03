#pragma once

#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

// A part of a model's surface for a fixture or a load (engineering_panel.md,
// rule 5; formats.md, rule 11 bis): the triangles marked, or the disc of a
// radius around a point clicked on the surface. Own design; see PROVENANCE.md.
namespace tisma {

using Triangle3 = std::array<std::array<double, 3>, 3>;

// What a condition holds: triangles of the part's mesh, in the coordinates
// the analysis works in, and for a point its centre and radius.
struct ZoneShape {
    std::vector<Triangle3> triangles;
    bool disc = false;
    std::array<double, 3> centre{};
    double radius = 0.0;
};

// The shape made fast to ask: samples over its triangles a `spacing` apart in
// a grid of cells; a point is in it when it lies within `reach` of a sample
// (and, for a disc, within the radius plus `reach` of its centre). The
// analysis takes reach as about a voxel, so the nodes of the voxels the
// surface passes through count.
class SurfaceZone {
public:
    SurfaceZone(const ZoneShape& shape, double spacing, double reach);
    bool near(double x, double y, double z) const;
    bool empty() const { return cells_.empty(); }
    // Its samples' box, grown by reach: nothing outside it is near.
    const std::array<double, 6>& bounds() const { return bounds_; }

private:
    std::int64_t key(long long i, long long j, long long k) const;
    std::unordered_map<std::int64_t, std::vector<std::array<double, 3>>> cells_;
    double cell_ = 1.0, reach2_ = 1.0;
    bool disc_ = false;
    std::array<double, 3> centre_{};
    double disc_reach2_ = 0.0;
    std::array<double, 6> bounds_{};
};

// The triangles of `mesh_triangles` that come within `radius` of `centre`.
std::vector<Triangle3> triangles_near(const std::vector<Triangle3>& mesh_triangles, const std::array<double, 3>& centre,
                                      double radius);

// The point of the triangle nearest to p (textbook geometry: by the region of
// the triangle's plane the projection falls in).
std::array<double, 3> closest_on_triangle(const std::array<double, 3>& p, const Triangle3& t);

} // namespace tisma
