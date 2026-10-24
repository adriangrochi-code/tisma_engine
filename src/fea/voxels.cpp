// Voxels of a part for the finite element analysis. Own design; see
// PROVENANCE.md.

#include "fea/voxels.hpp"

#include "geom/polygons.hpp"
#include "slice/layers.hpp"
#include "slice/level_set.hpp"

#include <oneapi/tbb/parallel_for.h>

#include <algorithm>
#include <cmath>

namespace tisma {

std::size_t VoxelGrid::count() const {
    return std::size_t(std::count_if(solid.begin(), solid.end(), [](std::uint8_t s) { return s != 0; }));
}

VoxelGrid voxelize(const IndexedMesh& mesh, double h) {
    VoxelGrid g;
    g.h = h;
    if (mesh.vertices.empty() || !(h > 0.0))
        return g;
    double min[3] = {1e300, 1e300, 1e300}, max[3] = {-1e300, -1e300, -1e300};
    for (const Vec3f& v : mesh.vertices) {
        const double p[3] = {v.x, v.y, v.z};
        for (int a = 0; a < 3; ++a)
            min[a] = std::min(min[a], p[a]), max[a] = std::max(max[a], p[a]);
    }
    g.x0 = min[0], g.y0 = min[1], g.z0 = min[2];
    g.nx = std::max(1, int(std::ceil((max[0] - min[0]) / h)));
    g.ny = std::max(1, int(std::ceil((max[1] - min[1]) / h)));
    g.nz = std::max(1, int(std::ceil((max[2] - min[2]) / h)));
    g.solid.assign(std::size_t(g.nx) * std::size_t(g.ny) * std::size_t(g.nz), 0);

    std::vector<float> heights(std::size_t(g.nz));
    for (int k = 0; k < g.nz; ++k)
        heights[std::size_t(k)] = float(g.z0 + (k + 0.5) * h);
    const std::vector<std::vector<Contour>> layers = slice_levels(mesh, height_field(mesh), heights);
    tbb::parallel_for(0, g.nz, [&](int k) {
        const Polygons region = layer_region(to_polygons(layers[std::size_t(k)]));
        // Every row of voxel centres crosses the region's edges at some x;
        // between the first and second crossing, the third and fourth, and so
        // on, the row is inside (half-open rule at the edges' ends).
        std::vector<double> xs;
        for (int j = 0; j < g.ny; ++j) {
            const double y = g.y0 + (j + 0.5) * h;
            xs.clear();
            for (const Polygon& p : region)
                for (std::size_t e = 0; e < p.size(); ++e) {
                    const double ax = to_mm(p[e].x), ay = to_mm(p[e].y);
                    const double bx = to_mm(p[(e + 1) % p.size()].x), by = to_mm(p[(e + 1) % p.size()].y);
                    if ((ay > y) != (by > y))
                        xs.push_back(ax + (y - ay) * (bx - ax) / (by - ay));
                }
            std::sort(xs.begin(), xs.end());
            for (std::size_t c = 0; c + 1 < xs.size(); c += 2) {
                // Voxels whose centre x0 + (i + 0.5) h lies in [xs[c], xs[c + 1]).
                const int i0 = std::max(0, int(std::ceil((xs[c] - g.x0) / h - 0.5)));
                const int i1 = std::min(g.nx - 1, int(std::ceil((xs[c + 1] - g.x0) / h - 0.5)) - 1);
                for (int i = i0; i <= i1; ++i)
                    g.solid[g.index(i, j, k)] = 1;
            }
        }
    });
    return g;
}

} // namespace tisma
