#pragma once

#include "config/settings.hpp"
#include "mesh/mesh.hpp"

namespace tisma {

// Puts a model where the command line prints it (docs/spec/features/
// placement.md, rule 2): with placement.arrange and a bed, the centre of its
// box in X and Y moves to placement.center, or to the centre of the bed's
// box; Z is left alone (the slicer lays the lowest point on the bed). Without
// a bed, or with placement.arrange off, nothing moves (rules 1 and 4).
void place_on_bed(TriangleMesh& mesh, const Settings& settings);

} // namespace tisma
