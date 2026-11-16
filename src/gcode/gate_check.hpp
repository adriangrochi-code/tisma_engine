#pragma once

#include "gcode/preview.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace tisma {

// Checks on a slice's G-code for the release gate (docs/spec/RELEASE_GATE.md):
// the part's size, support with nothing under it, and support under each
// overhang. They read the G-code as the preview does, not the engine's own
// toolpaths, so what is checked is what the printer gets.

// The box of the part's own extrusions (walls, infill, solid and top
// layers, bridges, gap fill, ironing), each as wide as its bead; x, y and z
// sizes in mm, z from the bed to the top layer. Zero if there are none.
std::array<double, 3> part_extent(const GcodePreview& preview);

// The share, 0 to 1, of the support's extruded length (support and its
// interface) that has nothing under it: above the first layer, no extrusion
// of any kind within `reach` mm sideways in the layers it sits on (those
// whose top reaches its bottom). 0 without support.
// `where`, when given, gets a point of each floating piece (x, y, z).
double floating_support_share(const GcodePreview& preview, double reach = 1.0,
                              std::vector<std::array<float, 3>>* where = nullptr);

// How far the extrusions of `kind` in the first layer that extrudes lie from
// the part's box seen from above (its extrusions in every layer): `gap` from
// the box to the nearest bead's inner edge, `outer` to the farthest bead's
// outer edge, mm. Both negative when there are none of that kind.
struct FirstLayerRing {
    double gap = -1.0, outer = -1.0;
    double width = 0.0; // the widest bead of that kind, mm
};
FirstLayerRing first_layer_ring(const GcodePreview& preview, LineKind kind);

// The middle layer of the part (by height): how many wall loops it has (runs
// of wall extrusions with no travel between them), and its sparse infill's
// density, 0 to 1: the share of the volume it fills, its bead's mean width
// (the rounded section's: width less (1 - pi/4) of the height) over the spacing of its parallel
// lines (the median gap between the lines along the longest one's
// direction; for line patterns such as rectilinear).
struct MiddleLayer {
    std::size_t layer = 0;
    int wall_loops = 0;
    double sparse_density = 0.0;
    double sparse_length = 0.0; // mm of sparse infill
};
MiddleLayer middle_layer(const GcodePreview& preview);

// A point under an overhang, on the model's downward face, mm.
struct OverhangPoint {
    float x = 0, y = 0, z = 0;
    float area = 0; // mm2 of the face it stands for
    std::uint32_t region = 0; // the overhang it is on: faces that touch, numbered from 0
};

// The overhang points with no support below them: no support extrusion
// within `reach` mm sideways in the `depth` mm under the point.
std::vector<OverhangPoint> overhangs_without_support(const GcodePreview& preview, const std::vector<OverhangPoint>& points,
                                                     double reach = 2.5, double depth = 3.0);

} // namespace tisma
