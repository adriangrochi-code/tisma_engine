#pragma once

#include "geom/bitgrid.hpp"

#include <vector>

namespace tisma {

// The squared Euclidean distance, in pixels squared, from the centre of each
// pixel of the grid to the centre of the nearest unset pixel; outside the
// grid counts as unset, so a set pixel on the edge is 1 away. Unset pixels
// are 0. Row after row, width() values a row. Exact, in time linear in the
// pixels: first along each column, then along each row as the lower envelope
// of the parabolas the columns give (Felzenszwalb and Huttenlocher,
// «Distance Transforms of Sampled Functions», Theory of Computing 8, 2012,
// section 2). The CPU reference of the GPU kernel. Own code.
std::vector<float> squared_distance_inside(const BitGrid& grid);

// The pixels whose distance is at least `pixels`: the region shrunk by that
// many pixels, as a grid of the same size.
BitGrid at_least(const BitGrid& grid, const std::vector<float>& squared, double pixels);

} // namespace tisma
