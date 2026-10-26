#pragma once

#include "geom/polygons.hpp"

#include <vector>

namespace tisma {

struct SupportParams {
    double threshold_angle = 0.0; // degrees from the horizontal; 0: automatic (half a bead per layer)
    int z_gap_layers = 1;         // empty layers between the support and the part above it
    double xy_gap = 0.6;          // mm between the support and the part beside it
    double bead = 0.4;            // mm, the wall bead width
    // Painted faces (docs/PAINTING.md; supports_more.md, rules 3 to 5), one
    // area per layer, or null. In an enforcer, whatever reaches out past the
    // layer below gets support, however steep; in a blocker, nothing does.
    // With automatic off, only the enforced overhangs get support.
    bool automatic = true;
    const std::vector<Polygons>* enforce = nullptr;
    const std::vector<Polygons>* block = nullptr;
};

// What each layer overhangs (docs/spec/BEHAVIOR.md, "Soportes"): the part of
// its region farther out than the layer below allows. A face flatter than the
// threshold angle from the horizontal reaches out more than height / tan(angle)
// per layer; that much is allowed. Layer 0 rests on the bed. heights[l] is the
// thickness of layer l.
std::vector<Polygons> overhangs(const std::vector<Polygons>& regions, const std::vector<double>& heights,
                                const SupportParams& params);

// The support area of every layer: the overhangs above it (those the
// automatic support finds, plus the enforced ones, less the blocked ones)
// projected straight down, starting z_gap_layers below the overhang and ending at the bed or where
// the part is below (the support then stands on the part), kept xy_gap away
// from the part; gaps narrower than two beads in it are closed. Own design.
std::vector<Polygons> support_areas(const std::vector<Polygons>& regions, const std::vector<double>& heights,
                                    const SupportParams& params);

} // namespace tisma
