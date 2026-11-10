#pragma once

// Polyholes (docs/spec/features/orca_2_5.md, rule 5): round holes printed as
// polygons with a few flat sides whose flats touch the hole's circle, so the
// printed hole keeps its diameter.

#include "geom/polygons.hpp"

#include <vector>

namespace tisma {

struct PolyholeOptions {
    double margin = 20.0;        // how far from its circle a hole may stray: mm, or % of the radius
    bool margin_percent = true;
    double nozzle = 0.4;         // mm; the sides are about 2 per mm of diameter with 0.4
    int max_edges = 50;
    bool twist = false;          // turn the polygon each layer, five orientations repeating
};

// Turns the round holes of each layer's region into polygons. A hole counts as
// round when it is convex, has more than 8 points and keeps within the margin
// of its estimated circle; it is turned only on the first layer or when a hole
// within the same margin is on the layer below or above. Returns, by layer,
// whether the region changed.
std::vector<char> make_polyholes(std::vector<Polygons>& regions, const PolyholeOptions& options);

// The number of sides for a hole of `diameter` mm.
int polyhole_sides(double diameter, double nozzle, int max_edges);

} // namespace tisma
