#pragma once

#include "geom/bitgrid.hpp"
#include "geom/polygons.hpp"

#include <span>
#include <vector>

namespace tisma {

namespace gpu {
class Context;
}

struct SupportParams {
    double threshold_angle = 0.0; // degrees from the horizontal; 0: automatic (half a bead per layer)
    int z_gap_layers = 1;         // empty layers between the support and the part above it
    int bottom_gap_layers = 0;    // empty layers between the part and the support standing on it
    double xy_gap = 0.6;          // mm between the support and the part beside it
    double bead = 0.4;            // mm, the wall bead width
    double margin = 0.0;          // mm the column reaches past what overhangs (supports_skirt.md, rule 2: about 1.6)
    // Painted faces (docs/PAINTING.md; supports_more.md, rules 3 to 5), one
    // area per layer, or null. In an enforcer, whatever reaches out past the
    // layer below gets support, however steep; in a blocker, nothing does.
    // With automatic off, only the enforced overhangs get support.
    bool automatic = true;
    const std::vector<Polygons>* enforce = nullptr;
    const std::vector<Polygons>* block = nullptr;
    gpu::Context* gpu = nullptr; // the column's steps on the GPU when given, with the same bits
};

// What each layer overhangs (docs/spec/BEHAVIOR.md, "Soportes"): the part of
// its region farther out than the layer below allows. A face flatter than the
// threshold angle from the horizontal reaches out more than height / tan(angle)
// per layer; that much is allowed. Layer 0 rests on the bed. heights[l] is the
// thickness of layer l.
std::vector<Polygons> overhangs(const std::vector<Polygons>& regions, const std::vector<double>& heights,
                                const SupportParams& params);

// What each layer needs support under: the overhangs the automatic support
// finds (with automatic on), plus the enforced ones, less the blocked ones.
std::vector<Polygons> supported_overhangs(const std::vector<Polygons>& regions, const std::vector<double>& heights,
                                          const SupportParams& params);

// The support area of every layer: the overhangs above it (those the
// automatic support finds, plus the enforced ones, less the blocked ones)
// projected straight down, starting z_gap_layers below the overhang and ending at the bed or where
// the part is below (the support then stands on the part), kept xy_gap away
// from the part; gaps narrower than two beads in it are closed. Own design.
std::vector<Polygons> support_areas(const std::vector<Polygons>& regions, const std::vector<double>& heights,
                                    const SupportParams& params);

// The column's steps over a chunk of layers, on its grid (support_areas):
// from the chunk's last layer down, the column takes adds[k] and is closed by
// `radius` pixels when adds[k] has a grid (an overhang above), then loses
// parts[k]; kept[k] becomes the column after it. The CPU reference of
// gpu::Context::support_column.
void support_column(BitGrid& column, std::span<const BitGrid> parts, std::span<const BitGrid> adds,
                    std::span<BitGrid> kept, int radius);

} // namespace tisma
