#pragma once

#include "geom/polygons.hpp"
#include "perimeters/sites.hpp"
#include "slice/layers.hpp"

#include <vector>

// Lightning infill (infill_patterns.md, rule 9): only what holds up the top
// surfaces. Branches start at the walls and reach in under each ceiling, and
// lower down they draw back towards the walls, so there is much infill near
// the top and almost none near the bottom. After the ribbed support vaults of
// Tricard, Claux and Lefebvre (2020): from the top down, every point to hold
// up gets a branch within a set distance, joined to the nearest branch or
// wall, and each layer's branches are the next one's, pulled back at their
// tips as far as a printable overhang allows. Own design; see PROVENANCE.md.
namespace tisma {

struct LightningParams {
    double width = 0.4;     // bead width
    double reach = 1.0;     // mm: every point to hold up lies at most this far from a branch
    double overhang = 45.0; // degrees from the vertical a branch's tip may lean out, layer to layer
};

// The reach for a density: the spacing of rectilinear lines at that density,
// width / density (with half of it, the spec's 20 mm cube got twice the
// branches of P7 under its top; with all of it, about as many).
double lightning_reach(double width, double density);

// The lightning infill of every layer. Layer l holds up what lies over its
// sparse infill in layer l + 1 (its part outside its own sparse infill:
// skins, walls) and the branches of layer l + 1, each pulled back at its tips
// by the height of layer l + 1 times tan(overhang). `regions` are the layers'
// whole regions, `sparse` their sparse infill.
// The branches are printed where `print` says (by default, in `sparse`): the
// parts of the sparse infill left to it.
std::vector<std::vector<Toolpath>> lightning_layers(const std::vector<Polygons>& sparse,
                                                    const std::vector<Polygons>& regions,
                                                    const std::vector<Layer>& layers, const LightningParams& params,
                                                    const std::vector<Polygons>* print = nullptr);

} // namespace tisma
