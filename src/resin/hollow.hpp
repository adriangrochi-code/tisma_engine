#pragma once

#include "config/settings.hpp"
#include "geom/polygons.hpp"
#include "slice/layers.hpp"

#include <cstddef>
#include <vector>

// Hollowing a resin part (docs/spec/features/resin_supports.md, rules 4 to
// 6): the hollow inside it, the bars that fill the hollow, and the drain
// holes through its wall. Own design; see PROVENANCE.md.
namespace tisma {

// Squared Euclidean distance transform of a 3D grid of nx by ny by nz cells
// (x fastest): each cell gets the squared distance, in cells, to the
// nearest cell whose value is 0; the others are given as kFar. Separable,
// one dimension at a time, by the lower envelope of parabolas of
// Felzenszwalb and Huttenlocher ("Distance Transforms of Sampled Functions",
// Theory of Computing 8, 2012), in parallel by rows.
inline constexpr double kFar = 1e20;
void distance_transform(std::vector<double>& cells, int nx, int ny, int nz);

// The hollow of each layer (rule 4): the points of the part at least
// resin.hollow_wall from its surface, rounded by resin.hollow_closing (the
// part shrunk by the wall and the closing, then grown back by the closing;
// the closing no more than the part's depth past the wall leaves, so that a
// hollow is made wherever one fits), on a grid of cells of `cell` mm (0:
// chosen from the part's size, from 0.1 to 1 mm). sections are the part's,
// one per layer.
std::vector<Polygons> hollow_sections(const std::vector<Polygons>& sections, const std::vector<Layer>& layers,
                                      const Settings& settings, double cell = 0.0);

// The width of the inner bars for resin.hollow_infill (rule 5): density =
// 5 · (width / cell)².
double infill_bar_width(const Settings& settings);

// The bars at height z (rule 5): square bars of that width, vertical and at
// 45 degrees both ways in the XZ and YZ planes, on a lattice of
// resin.hollow_infill_cell from the origin; each rests on the layers below.
// Within `box` (xmin, ymin, xmax, ymax, mm).
Polygons infill_bars(const Settings& settings, double z, const double box[4]);

// A drain hole (rule 6): from the point (x, y, z) on the part's surface, a
// cylinder of `diameter` going `depth` mm into the part, against the outward
// normal (nx, ny, nz).
struct ResinDrain {
    double x = 0.0, y = 0.0, z = 0.0;
    double nx = 0.0, ny = 0.0, nz = -1.0;
    double diameter = 3.0, depth = 5.0;
};

// What the drain holes take out of each layer.
std::vector<Polygons> drain_sections(const std::vector<ResinDrain>& drains, const std::vector<Layer>& layers);

} // namespace tisma
