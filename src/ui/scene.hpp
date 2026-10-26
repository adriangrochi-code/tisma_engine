#pragma once

#include "gcode/preview.hpp"
#include "mesh/mesh.hpp"
#include "ui/math.hpp"

#include <array>
#include <cstdint>
#include <vector>

// What the 3D view draws, as flat arrays ready for the GPU: the bed, the model
// and the toolpaths of a G-code preview. Nothing here touches OpenGL, so it is
// tested like the rest of the engine. Own design; see PROVENANCE.md.
namespace tisma::ui {

using BedPolygon = std::vector<std::array<double, 2>>;

// The offset that centres the box on the bed (on the box of its polygon) and
// sets its bottom on z = 0. With no bed, it centres on the origin.
Vec3 centre_on_bed(const BoundingBox& box, const BedPolygon& bed);

// The mesh moved by offset.
TriangleMesh translated(const TriangleMesh& mesh, Vec3 offset);

// One vertex per triangle corner, moved by offset: position (3 floats) and the
// triangle's unit normal (3 floats), for flat shading.
std::vector<float> mesh_vertices(const TriangleMesh& mesh, Vec3 offset);

// A coloured line vertex: position and 0xRRGGBB colour with alpha in the top
// byte (0xAARRGGBB).
struct LineVertex {
    float x = 0, y = 0, z = 0;
    std::uint32_t argb = 0;
};

struct BedLook {
    double step = 10.0;  // mm between grid lines
    int major_every = 5; // every this many lines, a major one
    std::uint32_t minor = 0x40FFFFFF, major = 0x70FFFFFF, outline = 0xFF7A24C9;
};

// Grid lines clipped to the bed polygon (even-odd, so any simple polygon
// works) and its outline, as pairs of vertices for GL_LINES, a little above
// z = 0 so they draw over the plate. Lines on the edge of the bed's box are
// left to the outline.
std::vector<LineVertex> bed_lines(const BedPolygon& bed, const BedLook& look);

// The plate as triangles (x, y, z per vertex) at z = 0: a fan from the first
// point, right for convex beds.
std::vector<float> bed_triangles(const BedPolygon& bed);

// One extrusion of the preview, as the 3D view's shader reads it: the move's
// two ends, its width and height, its feed, and its kind and layer packed in
// one integer (kind in the low 8 bits). Where the bead goes on into the next
// or previous move, the join is the unit normal (in XY) of the plane both
// beads end on, halfway between their directions; (0, 0) at a free end.
struct PreviewInstance {
    float x0 = 0, y0 = 0, z0 = 0;
    float x1 = 0, y1 = 0, z1 = 0;
    float width = 0, height = 0;
    float feed = 0;
    std::uint32_t kind_layer = 0;
    float join0_x = 0, join0_y = 0; // at (x0, y0, z0)
    float join1_x = 0, join1_y = 0; // at (x1, y1, z1)
};
static_assert(sizeof(PreviewInstance) == 56);

// Two moves in a row join when the bead turns by at most this (degrees): past
// it the mitre would reach more than twice the bead's half width out, and the
// ends stay free, as at a zigzag's turn.
inline constexpr float kPreviewJoinTurn = 120.0f;

struct PreviewBuffers {
    std::vector<PreviewInstance> extrusions; // in layer order
    std::vector<std::size_t> layer_extrusions; // layer l is extrusions[layer_extrusions[l], layer_extrusions[l + 1])
    std::vector<LineVertex> travels;           // pairs for GL_LINES, in layer order
    std::vector<std::size_t> layer_travels;    // the same, in vertices
    float min_feed = 0, max_feed = 0;          // of the extrusions, mm/s
    float min_width = 0, max_width = 0;        // of the extrusions, mm
    float min_height = 0, max_height = 0;      // of the extrusions, mm (layer heights by zone vary them)
    // Per extrusion, 1 when the part hides it from every side as long as all
    // of it is drawn (hidden_inside): the view leaves those out then.
    std::vector<std::uint8_t> hidden;
};

PreviewBuffers preview_buffers(const GcodePreview& preview, std::uint32_t travel_argb = 0xFF4FA3FF);

// Which extrusions the part itself hides from every side when all of it is
// drawn (docs/LARGE_MODELS.md): the inner walls and the sparse infill that
// lie, a cell to spare, inside what the outer walls enclose both in the layer
// below and in the layer above (those layers cover them from under and over,
// and their own layer's outer wall from the sides), and are not within two
// bead widths of a step: where the next or the last layer's outer wall lies
// more than 0.05 mm from this one's, the groove between their beads opens
// onto the beads inside. Never in the first or the last layer. The areas are
// rasters of cells of `cell` mm (0: a size from the part's extent, at least
// 0.2 mm), filled by the even-odd rule along each row; a row the outer walls
// cross an odd number of times (a wall not closed) counts as outside. When in
// doubt, an extrusion is not hidden. Layers in parallel (oneTBB).
std::vector<std::uint8_t> hidden_inside(const PreviewBuffers& preview, double cell = 0.0);

// The colour of each line kind in the legend and the view (0xRRGGBB).
std::uint32_t kind_colour(LineKind kind);

} // namespace tisma::ui
