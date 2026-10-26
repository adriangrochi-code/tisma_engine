#pragma once

#include "perimeters/sites.hpp"

namespace tisma {

// The scarf seam (docs/spec/features/walls_extra.md, rule 4): a closed loop,
// starting at its seam, laid as
// - a ramp along the loop from `length` mm before the seam up to the seam, the
//   nozzle rising from `start` (a fraction of the layer's height) over the
//   layer below to the layer's top, the bead as thick as the nozzle's height;
// - then the whole loop at the top, the seam to the seam: its last `length` mm
//   pass over the ramp again, the bead thinning as the ramp under it
//   thickens, so the loop lays the filament of a plain one;
// - with `length` at the loop's length or more, the ramp goes the whole way
//   round, and the cover too.
// The ramp and its cover go in pieces of at most `max_segment` mm. The heights
// go in the points (dz: the nozzle off the layer's top; below: the ramp under
// the cover, over the layer below's top), and the path is marked to lay each
// bead down to what is under it (Toolpath::wavy). The result is an open path
// that ends at the seam. `height`: the layer's, mm. Own design.
Toolpath scarf_loop(const Toolpath& loop, double height, double start, double length, double max_segment);

} // namespace tisma
