#pragma once

#include "geom/polygons.hpp"
#include "perimeters/sites.hpp"

#include <vector>

namespace tisma {

// Organic (tree) supports (docs/spec/features/supports_more.md, rules 15 to
// 17). Tips under what overhangs go down as round branches, layer by layer:
// each branch leans towards its nearest neighbour (at most angle_preferred
// from the vertical) and joins it where they meet, widens on its way down,
// and keeps xy_gap off the part, leaning up to `angle` to get round it; one
// that cannot rests on the part. The rest reach the bed. Own design on the
// idea of Vanek, Galicia and Benes, «Clever Support» (2014): points under the
// overhangs that merge downwards into a tree.
struct TreeParams {
    double angle = 40.0;           // degrees from the vertical, to keep off the part
    double angle_preferred = 25.0; // degrees from the vertical, to join another branch
    double tip_diameter = 0.8;     // mm
    double branch_diameter = 2.0;  // mm, reached a few layers under the tip
    double widen_angle = 5.0;      // degrees the radius grows by on the way down
    double tip_spacing = 3.0;      // mm between tips under an overhang
    double xy_gap = 0.6;           // mm between a branch and the part
    int z_gap_layers = 1;          // empty layers between the support and the part above it
    int interface_layers = 3;      // dense layers right under what overhangs, over the tips
};

// The spacing of the tips (support.tree_top_rate, support.tree_tip_diameter,
// support.tree_branch_distance), fitted to the spec's measurements (SM13):
// 3 mm with 0.8 mm tips at 15 %, closer as the rate grows, wider with wider
// tips.
double tree_tip_spacing(double tip_diameter, double top_rate, double branch_distance);

// A branch at a layer: its centre and radius, in mm.
struct TreeNode {
    double x = 0.0, y = 0.0, r = 0.0;
};

// The branches of every layer (regions[l] the part's region, heights[l] the
// layer's thickness, overhangs[l] what layer l needs support under). The tips
// stand under the interface.
std::vector<std::vector<TreeNode>> tree_nodes(const std::vector<Polygons>& regions, const std::vector<double>& heights,
                                              const std::vector<Polygons>& overhangs, const TreeParams& params);

// The support area of every layer: the union of its branches' discs and of
// the interface right under what overhangs (rule 17: the overhangs of the
// interface_layers layers past the Z gap), kept xy_gap off the part.
std::vector<Polygons> tree_support_areas(const std::vector<Polygons>& regions,
                                         const std::vector<std::vector<TreeNode>>& nodes,
                                         const std::vector<Polygons>& overhangs, const TreeParams& params);

// The paths of a layer of organic support, `area` (tree_support_areas):
// where the part lies just above (`above`), the interface, in dense lines at
// `angle` + 45 degrees; elsewhere a wall round every branch, two round where
// a branch is wider than `double_wall` mm (0: never), and inside the walls
// lines `spacing` mm apart at `angle` degrees.
struct TreeFill {
    std::vector<Toolpath> support, interface;
};
TreeFill tree_fill(const Polygons& area, const Polygons& above, double bead, double spacing, double double_wall,
                   double angle);

} // namespace tisma
