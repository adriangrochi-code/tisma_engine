#pragma once

#include "geom/polygons.hpp"

#include <vector>

namespace tisma {

// What each layer's infill region becomes (F3).
struct LayerSurfaces {
    Polygons top;    // solid, under an exposed top surface
    // The part of top with more of its top layers above it: only the layer
    // right under the air is the top surface, printed as one; the rest is
    // solid infill (docs/spec/features/surfaces_infill.md, rule 2).
    Polygons top_under;
    Polygons bottom; // solid, over an exposed bottom surface (not already top), resting on what is printed below
    // The part of bottom that is the bottom surface itself: nothing of this
    // part's layer below it (the first layer, or over air or another part).
    // Only there a bottom pattern other than lines applies (infill_patterns.md, rule 17).
    Polygons bottom_exposed;
    Polygons sparse; // the rest: sparse infill
    Polygons bridge; // solid over nothing printed (see find_bridges)
    double bridge_angle = 0.0; // degrees: the direction of the bridge's lines
    // What is printed below, around the bridge: its lines' joins go only
    // over it (toolpath_time.md, rule 1). Empty: the lines stay apart.
    Polygons bridge_support;
};

// Keeping the shell thick on slopes (walls_extra.md, rule 7, test W4). With
// Partial, a top skin that a slope keeps going down from (layer i + count + 1
// does not cover what is past it) gets one bead more of solid infill. Full
// adds, along the infill's edge, one bead where the next layer's infill steps
// in (a slope the walls hide), and over an overhang the infill past the
// bottom count's layer below and one bead more (the walls do not count).
// z holds each layer's height (top of the layer); bead is a line's width.
struct ShellCheck {
    enum Mode { Off, Partial, Full } mode = Off;
    std::vector<double> z;
    double bead = 0.45;
};

// Splits the infill region of every layer into top, bottom and sparse parts.
// Part of layer i is top when one of the top_layers layers above it does not
// cover it (the layers above the last one cover nothing), and bottom likewise
// with the bottom_layers layers below. A layer covers what its region covers,
// walls included. Solid parts narrower than min_width (a bead) could not be
// filled: they grow by min_width into the infill region, so a gentle slope
// does not leave holes in its skin. Layers run in parallel (oneTBB).
// The same with a count per layer (variable layer heights: the skins are as
// many thick layers, docs/VARIABLE_LAYERS.md).
std::vector<LayerSurfaces> classify_surfaces(const std::vector<Polygons>& regions, const std::vector<Polygons>& infill,
                                             const std::vector<int>& top_layers, const std::vector<int>& bottom_layers,
                                             double min_width = 0.0, const ShellCheck* shell = nullptr);
std::vector<LayerSurfaces> classify_surfaces(const std::vector<Polygons>& regions, const std::vector<Polygons>& infill,
                                             int top_layers, int bottom_layers, double min_width = 0.0,
                                             const ShellCheck* shell = nullptr);

} // namespace tisma
