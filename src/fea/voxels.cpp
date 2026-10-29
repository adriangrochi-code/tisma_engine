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

std::vector<double> layer_planes(const std::vector<Layer>& layers, int per_layer) {
    std::vector<double> z;
    const int n = std::max(per_layer, 1);
    for (const Layer& layer : layers) {
        if (z.empty())
            z.push_back(layer.bottom);
        for (int s = 1; s <= n; ++s)
            z.push_back(s == n ? layer.top : layer.bottom + (layer.top - layer.bottom) * s / n);
    }
    return z;
}

VoxelGrid voxelize(const IndexedMesh& mesh, double hx, double hy, std::vector<double> z) {
    VoxelGrid g;
    g.hx = hx, g.hy = hy;
    if (mesh.vertices.empty() || !(hx > 0.0) || !(hy > 0.0) || z.size() < 2)
        return g;
    double min_x = 1e300, min_y = 1e300, max_x = -1e300, max_y = -1e300;
    for (const Vec3f& v : mesh.vertices) {
        min_x = std::min(min_x, double(v.x)), max_x = std::max(max_x, double(v.x));
        min_y = std::min(min_y, double(v.y)), max_y = std::max(max_y, double(v.y));
    }
    g.x0 = min_x, g.y0 = min_y;
    g.nx = std::max(1, int(std::ceil((max_x - min_x) / hx)));
    g.ny = std::max(1, int(std::ceil((max_y - min_y) / hy)));
    g.nz = int(z.size()) - 1;
    g.z = std::move(z);
    g.solid.assign(std::size_t(g.nx) * std::size_t(g.ny) * std::size_t(g.nz), 0);

    std::vector<float> heights(std::size_t(g.nz));
    for (int k = 0; k < g.nz; ++k)
        heights[std::size_t(k)] = float(0.5 * (g.z[std::size_t(k)] + g.z[std::size_t(k) + 1]));
    const std::vector<std::vector<Contour>> layers = slice_levels(mesh, height_field(mesh), heights);
    tbb::parallel_for(0, g.nz, [&](int k) {
        const Polygons region = layer_region(to_polygons(layers[std::size_t(k)]));
        // Every row of voxel centres crosses the region's edges at some x;
        // between the first and second crossing, the third and fourth, and so
        // on, the row is inside (half-open rule at the edges' ends).
        std::vector<double> xs;
        for (int j = 0; j < g.ny; ++j) {
            const double y = g.y0 + (j + 0.5) * hy;
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
                // Voxels whose centre x0 + (i + 0.5) hx lies in [xs[c], xs[c + 1]).
                const int i0 = std::max(0, int(std::ceil((xs[c] - g.x0) / hx - 0.5)));
                const int i1 = std::min(g.nx - 1, int(std::ceil((xs[c + 1] - g.x0) / hx - 0.5)) - 1);
                for (int i = i0; i <= i1; ++i)
                    g.solid[g.index(i, j, k)] = 1;
            }
        }
    });
    return g;
}

VoxelGrid voxelize(const IndexedMesh& mesh, double h) {
    if (mesh.vertices.empty() || !(h > 0.0))
        return voxelize(mesh, h, h, {});
    double min_z = 1e300, max_z = -1e300;
    for (const Vec3f& v : mesh.vertices)
        min_z = std::min(min_z, double(v.z)), max_z = std::max(max_z, double(v.z));
    const int nz = std::max(1, int(std::ceil((max_z - min_z) / h)));
    std::vector<double> z(std::size_t(nz) + 1);
    for (int k = 0; k <= nz; ++k)
        z[std::size_t(k)] = min_z + k * h;
    return voxelize(mesh, h, h, std::move(z));
}

} // namespace tisma
