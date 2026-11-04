#pragma once

// A pack printer's bed picture (profile_pack.md, rule 14) as flat coloured
// triangles on the plate: the SVG's filled and stroked shapes, each in its own
// colour, stretched over the bed's box. Triangles, not a texture, so both 3D
// views draw it as they draw the paint over a model. Own design; nanosvg (Zlib, see
// THIRD_PARTY.md) reads the SVG, Clipper2 resolves the fill rules and widens
// the strokes, triangulate_polygon fills the result.

#include <cstdint>
#include <string_view>
#include <vector>

namespace tisma::ui {

struct BedArt {
    struct Group {
        std::uint32_t rgb = 0;       // 0xRRGGBB
        std::uint32_t first = 0;     // the group's first vertex
        std::uint32_t count = 0;     // its vertices (three a triangle)
    };
    std::vector<float> vertices; // a vertex: x, y, z in mm and the normal (0, 0, 1), as mesh_vertices
    std::vector<Group> groups;   // in the SVG's painting order
};

// The SVG `svg` over the box [min_x, max_x] × [min_y, max_y] of the bed, at
// height `z`: the picture's top edge at max_y (an SVG's y points down). Empty
// when the SVG cannot be read or draws nothing; gradients and fully clear
// shapes are left out, partly clear ones drawn solid.
BedArt bed_svg_art(std::string_view svg, double min_x, double min_y, double max_x, double max_y, double z = 0.01);

} // namespace tisma::ui
