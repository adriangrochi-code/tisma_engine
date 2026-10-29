#pragma once

#include "geom/polygons.hpp"
#include "perimeters/sites.hpp"
#include "slice/layers.hpp"

#include <cstddef>
#include <vector>

// Adaptive cubic infill (infill_patterns.md, rule 7): the cubic infill's
// cubes, small near the walls and the solid surfaces and bigger inside, as in
// an octree. How deep each point lies in the sparse infill comes from an exact
// Euclidean distance transform on a voxel grid (Felzenszwalb and Huttenlocher
// 2012); the lines are the cubic infill's, with every other line left out for
// each level deeper, so the coarse lines are lines of the fine lattice and
// run on through every level. Own design; see PROVENANCE.md.
namespace tisma {

// The distance, in voxels, from the centre of each set voxel of an nx by ny by
// nz grid (x fastest) to the centre of the nearest unset one; 0 where unset.
// Exact: the 1D transform of squared distances along each axis in turn
// (Felzenszwalb and Huttenlocher 2012, sections 2 and 3). Only voxels of the
// grid count: with none unset, every distance is huge.
std::vector<float> distance_transform(const std::vector<char>& set, int nx, int ny, int nz);

// How deep each voxel lies inside the sparse infill: the distance (mm) from
// its centre to the nearest point outside it (walls, solid layers, air); 0
// outside. Cubic voxels `cell` mm wide from (x0, y0, z0).
struct DepthField {
    double x0 = 0.0, y0 = 0.0, z0 = 0.0, cell = 1.0;
    int nx = 0, ny = 0, nz = 0;
    std::vector<float> depth;
    double deepest = 0.0; // the largest depth
    bool empty() const { return depth.empty(); }
    std::size_t index(int i, int j, int k) const {
        return (std::size_t(k) * std::size_t(ny) + std::size_t(j)) * std::size_t(nx) + std::size_t(i);
    }
    // The depth at any point, between the voxels' centres (trilinear); 0 off the grid.
    double at(double x, double y, double z) const;
};

// What the depth is measured from: every surface of the sparse infill (walls,
// solid layers above and below, air), or all but the solid layers below it,
// which it does not hold up (the support cubic infill, rule 8): what lies
// right above it, and the walls beside it in its plane, so that a coarse
// cube still fits between them.
enum class DepthFrom { Surfaces, TopsAndWalls };

// The field of the layers' sparse infill. A voxel is inside only where every
// layer its height overlaps has sparse infill, so a thin solid layer between
// two sparse ones still counts as a surface. Voxels `cell` mm wide, wider if
// the grid would have more than `max_voxels`.
DepthField sparse_depth(const std::vector<Polygons>& sparse, const std::vector<Layer>& layers, double cell,
                        DepthFrom from = DepthFrom::Surfaces, std::size_t max_voxels = std::size_t(1) << 22);

// Where, in the plane at height z, the infill is at least `depth` mm deep.
Polygons deep_region(const DepthField& field, double z, double depth);

// How deep level l (1, 2, ...) starts for lines `spacing` apart: its cubes,
// 2^l times the finest, fit whole there (the depth of a cube's corners from
// its centre).
double level_depth(int level, double spacing);

// The adaptive cubic infill of a region at height z: the cubic infill's three
// families of lines `spacing` apart at 0, 60 and 120 degrees (rule 7: not at
// the infill angle), moving across by z / sqrt(2) with the height; where the
// field says level l fits, only every 2^l-th line. With no field, the cubic
// infill at those angles.
std::vector<Toolpath> adaptive_cubic_fill(const Polygons& region, const DepthField* field, double spacing,
                                          double width, double z);

} // namespace tisma
