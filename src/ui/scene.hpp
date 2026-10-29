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

// The key light's camera for a shadow map (Williams 1978, "Casting curved
// shadows on curved surfaces"): orthographic, looking against `towards` (the
// direction to the light), holding the box [lo, hi] exactly in -1..1 on every
// axis, depth -1 nearest the light. OpenGL's clip space, column-major.
Mat4 shadow_view_projection(Vec3 towards, Vec3 lo, Vec3 hi);

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
    float min_feed = 0, max_feed = 0;          // of the extrusions' `feed`: mm/s, or the view's value
    float min_width = 0, max_width = 0;        // of the extrusions, mm
    float min_height = 0, max_height = 0;      // of the extrusions, mm (layer heights by zone vary them)
    // Per extrusion, 1 when the part hides it from every side as long as all
    // of it is drawn (hidden_inside): the view leaves those out then.
    std::vector<std::uint8_t> hidden;
};

// The number the preview's numeric views colour by (preview.md, rule 1),
// which the instances carry in `feed`: the feed asked and reached (mm/s), the
// fan (%), the nozzle's temperature (C), the flow asked and reached (mm3/s),
// the layer's time (s, or its log10), the extruder and the colour change.
enum class PreviewValue {
    Feed,
    ActualFeed,
    Fan,
    Temperature,
    Flow,
    ActualFlow,
    LayerTime,
    LayerTimeLog,
    Tool,
    ColorPrint
};
float preview_value(const GcodePreview& preview, const PreviewMove& move, PreviewValue value);

PreviewBuffers preview_buffers(const GcodePreview& preview, std::uint32_t travel_argb = 0xFF4FA3FF,
                               PreviewValue value = PreviewValue::Feed);

// A numeric view's legend (rule 7): 11 values in equal steps from the least
// to the most (in equal steps of the logarithm when `log`, the ends given as
// their log10); a single one when they are equal.
std::vector<double> legend_steps(double lo, double hi, bool log);

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

// The extrusions in drawing order (docs/LARGE_MODELS.md): first, layer by
// layer, those that can show; then, layer by layer, those the whole part hides
// (PreviewBuffers::hidden). Layer l's are [layer_shown[l], layer_shown[l + 1])
// and [layer_hidden[l], layer_hidden[l + 1]).
struct PreviewDrawOrder {
    std::vector<PreviewInstance> instances;
    std::vector<std::size_t> layer_shown, layer_hidden;
    std::uint32_t hidden_kinds = 0; // bit k: some extrusion of LineKind k is hidden
};
PreviewDrawOrder preview_draw_order(const PreviewBuffers& preview);

// What a view draws of it for layers lo to hi with the kinds of kind_mask:
// the shown range and the hidden one, which is empty when all of the part is
// drawn (from its first layer to its last, every kind but the hidden ones and
// the travels shown). Each as (first instance, count). With the move slider
// (preview.md, rule 4), only the first top_shown and top_hidden of the top
// layer's shown and hidden extrusions.
struct PreviewRange {
    std::size_t first = 0, count = 0;
};
inline constexpr std::size_t kWholeLayer = std::size_t(-1);
std::array<PreviewRange, 2> preview_ranges(const PreviewDrawOrder& order, std::size_t lo, std::size_t hi,
                                           std::uint32_t kind_mask, std::size_t top_shown = kWholeLayer,
                                           std::size_t top_hidden = kWholeLayer);

// The colour of each line kind in the legend and the view (0xRRGGBB).
std::uint32_t kind_colour(LineKind kind);

} // namespace tisma::ui
