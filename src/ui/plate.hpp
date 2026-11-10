#pragma once

#include "project/engineering.hpp"
#include "config/settings.hpp"
#include "mesh/mesh.hpp"
#include "mesh/paint.hpp"
#include "ui/math.hpp"

#include <array>
#include <map>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// The plate: the models on the bed, each with where it goes, how it is turned
// and how big it is, and what the Arrange tool does with them. Nothing here
// touches OpenGL, so it is tested like the rest of the engine. Own design; see
// PROVENANCE.md.
namespace tisma::ui {

// A rotation as a row-major 3 x 3 matrix.
using Rotation = std::array<double, 9>;
inline constexpr Rotation kNoRotation{1, 0, 0, 0, 1, 0, 0, 0, 1};

// Where a model goes: scaled along its own axes about the centre of its box,
// then turned, then set down with its base on the bed and the centre of its
// box at (x, y).
struct Placement {
    double x = 0.0, y = 0.0;
    Rotation rotation = kNoRotation;
    std::array<double, 3> scale{1.0, 1.0, 1.0}; // each above 0: a mirror would turn the triangles inside out
};

// A modifier or a negative volume added to an object (settings_levels.md,
// rule 10; model_tools.md, rules 2 and 3): a box or a cylinder, placed from
// the model's own box, as its file has it (the centre in X and Y, the bottom
// in Z), in the model's millimetres, with a modifier's own settings. It is
// scaled, turned and set on the bed with the model.
struct PlateVolume {
    enum class Kind { Modifier, Negative } kind = Kind::Modifier;
    // Lattice (structural_optimization.md, rule 7): the support-free lattice
    // of cells of side lattice_cell and bars of lattice_diameter over the
    // part where it stands on the bed, bars vertical whatever its turn; its
    // place and size are the part's.
    enum class Shape { Box, Cylinder, Lattice } shape = Shape::Box;
    std::array<double, 3> offset{0.0, 0.0, 0.0}; // mm: its box's centre in X and Y, its bottom in Z
    std::array<double, 3> size{10.0, 10.0, 10.0}; // mm
    std::string settings{};
    Rotation turn = kNoRotation; // its own turn in the model's frame, about its centre
    double lattice_cell = 0.0, lattice_diameter = 0.0; // mm, for a lattice
    friend bool operator==(const PlateVolume&, const PlateVolume&) = default;
};
// A height range of an object (model_tools.md, rule 6): from and to over its
// bottom, mm, with settings of its own (its layer height among them).
struct PlateRange {
    double from = 0.0, to = 2.0;
    std::string settings{};
    friend bool operator==(const PlateRange&, const PlateRange&) = default;
};

struct PlateObject {
    std::string name;
    TriangleMesh mesh; // as its file has it
    Placement place;
    Painting painting; // by triangle of the mesh: it follows the model however it is placed
    // Its extruder (from 1, or a mixed colour's number), and whether a
    // filament change may purge into its sparse infill or into all of it
    // (multi_extruder.md, rules 1 and 13).
    int extruder = 1;
    bool wipe_into_infill = false, wipe_into_object = false;
    // Its own settings (model_tools.md, rule 7; settings_levels.md, rules 10
    // and 11): a JSON object of groups over the profile's; empty: none.
    std::string settings{};
    std::vector<PlateVolume> volumes{};
    std::vector<PlateRange> ranges{};
    // Where it came from, when it came from the Design tab (cad_design.md,
    // rules 21 and 22): the design as `cad::design_json` writes it, the
    // SolidCraft design file, and a number of this session that «Editar en
    // Diseño» and a second «Mandar a la placa» find it by (0: none).
    std::string design{};
    std::string design_file{};
    std::string fastener{}; // a part of the Fasteners tab: its spec (cad::fastener_json)
    int design_id = 0;
    // Its Engineering data (engineering_panel.md, rule 17): conditions in
    // the frame and triangles of `mesh`.
    ProjectEngineering engineering{};
    // The file it was read from, for Reload from Disk (interface.md, «Más»);
    // empty for a shape, a design or a project's model.
    std::string source{};
};

// The painting of a model carried onto another mesh of the same part (a
// design built again with a measure changed, cad_design.md, rule 21): each
// painted triangle's marks go to the triangle of `to` nearest to it that
// faces the same way. Empty when either mesh is empty or they are too big to
// match (over 40 million pairs).
Painting carried_painting(const Painting& painting, const TriangleMesh& from, const TriangleMesh& to);

// A volume's mesh where its object stands (a cylinder of 48 sides, its axis
// on the model's Z), its faces out: `placed` is placed_mesh(object).
TriangleMesh volume_mesh(const PlateVolume& volume, const PlateObject& object, const TriangleMesh& placed);

// The model's size as its file has it, before its scale and turn, mm.
std::array<double, 3> model_size(const PlateObject& object);

// A map x' = a x + b between two frames of a model.
struct Affine {
    std::array<double, 9> a{1, 0, 0, 0, 1, 0, 0, 0, 1};
    std::array<double, 3> b{0.0, 0.0, 0.0};
};
// From the model's frame to the bed, as placed_mesh sets it down (placed:
// placed_mesh(object)).
Affine placing(const PlateObject& object, const TriangleMesh& placed);
// The same map from the object alone, as placed_mesh would set it down, but
// without making that mesh: two passes over its corners for the boxes.
Affine placement_map(const PlateObject& object);
// The map that takes points placed by `from` to where `to` places them:
// to after from's inverse (from's linear part invertible: the scales are
// above 0).
Affine between(const Affine& from, const Affine& to);
// The volume in the model's new frame when the model's mesh becomes `map`
// of itself (saved as placed, or mirrored): `from` and `to` are the model's
// box in the old frame and in the new. Its sizes stretch as the map does
// along its axes; a mirror leaves it turned, not mirrored (a box and a
// cylinder are their own mirror images).
PlateVolume carried_volume(const PlateVolume& volume, const BoundingBox& from, const Affine& map, const BoundingBox& to);

// A box modifier with these settings that volume_mesh sets down on the bed
// as the box from `lo` to `hi` (bed axes, mm): the zones of
// structural_optimization.md, rule 3. None when the model's map turns the
// bed's axes into ones that are not square to each other (a turn with
// unequal scales), where no turned box fits.
std::optional<PlateVolume> volume_for_bed_box(const PlateObject& object, const TriangleMesh& placed,
                                              const std::array<double, 3>& lo, const std::array<double, 3>& hi,
                                              const std::string& settings);
// The same with the model's map to the bed given (as placing gave it when
// the box was found: the volume then follows the model wherever it moved).
std::optional<PlateVolume> volume_for_bed_box(const PlateObject& object, const Affine& to_bed,
                                              const std::array<double, 3>& lo, const std::array<double, 3>& hi,
                                              const std::string& settings);

// r turned by `degrees` about the bed's axis 0 (X), 1 (Y) or 2 (Z).
Rotation rotated(const Rotation& r, int axis, double degrees);

// r turned so that a face whose normal, already turned by r, is `normal` lies
// on the bed (the normal points straight down).
Rotation laid_on(const Rotation& r, Vec3 normal);

// The angles in degrees about X, then Y, then Z that give the rotation, for the
// panel's fields (r = Rz * Ry * Rx).
std::array<double, 3> euler_degrees(const Rotation& r);
Rotation from_euler_degrees(const std::array<double, 3>& degrees);

// The model's mesh as placed on the bed.
TriangleMesh placed_mesh(const PlateObject& object);

// Every placed mesh in one, as the slicer takes the plate.
TriangleMesh plate_mesh(const std::vector<TriangleMesh>& placed);

// Every model's painting in one, in the order plate_mesh joins the meshes.
Painting plate_painting(const std::vector<PlateObject>& objects);

// One channel's painted pieces as triangles for the 3D view (position and
// normal, six floats per corner, as mesh_vertices), each mark's together:
// groups[k] holds `count` corners from `first`. `placed[i]` is objects[i]'s
// placed mesh.
struct PaintMesh {
    struct Group {
        std::uint8_t mark = 0;
        std::size_t first = 0, count = 0;
    };
    std::vector<float> vertices;
    std::vector<Group> groups;
};
PaintMesh paint_mesh(const std::vector<PlateObject>& objects, const std::vector<TriangleMesh>& placed,
                     PaintChannel channel);

// The same, kept between strokes: each painted triangle's pieces are walked
// again only when a stroke changed it (touch), and the rest are copied, so a
// long stroke on a part with much paint costs the triangles it reaches and
// not all the paint (the testers' BH44). Anything else that changes the
// paint, the plate or the channel calls reset(). Own design.
class PaintMeshCache {
public:
    void reset() { objects_.clear(); }
    void touch(std::size_t object, const std::vector<std::uint32_t>& triangles);
    PaintMesh build(const std::vector<PlateObject>& objects, const std::vector<TriangleMesh>& placed, PaintChannel channel);

private:
    struct Piece {
        std::vector<float> vertices;     // 18 floats a piece: three corners of position and normal
        std::vector<std::uint8_t> marks; // one a piece
    };
    struct Object {
        std::map<std::uint32_t, Piece> triangles; // the painted ones
        std::vector<std::uint32_t> stale;         // touched since the last build
    };
    std::vector<Object> objects_; // empty: everything to walk again
};

// The size of the box of the model, scaled and turned (mm).
std::array<double, 3> placed_size(const PlateObject& object);

// Arrange (docs/spec/features/placement.md, rule 2): the centres for boxes of
// these sizes (x, y) in a compact grid, `spacing` mm between boxes, the whole
// set centred on `centre`. Rows of about the square root of the count, filled
// with the deepest boxes first; nothing is turned or scaled.
std::vector<std::array<double, 2>> arrange(const std::vector<std::array<double, 2>>& sizes,
                                           std::array<double, 2> centre, double spacing);

// What a ray from the eye hits first among the placed meshes: the model, the
// triangle and how far along the ray (direction of unit length).
struct PlateHit {
    std::size_t object = 0, triangle = 0;
    double distance = 0.0;
};
std::optional<PlateHit> pick(const std::vector<TriangleMesh>& placed, Vec3 origin, Vec3 direction);

// A bounding volume hierarchy over one mesh's triangles, so that a click or a
// brush on a model of a million triangles looks at a few dozen of them: each
// box is split at the median of its triangles' centres along its longest side,
// down to a few triangles a leaf. Own design.
class TriangleIndex {
public:
    TriangleIndex() = default;
    explicit TriangleIndex(const TriangleMesh& mesh);
    bool empty() const { return nodes_.empty(); }
    // The nearest triangle the ray hits and how far (direction of unit length).
    std::optional<std::pair<std::size_t, double>> hit(const TriangleMesh& mesh, Vec3 origin, Vec3 direction) const;
    // The triangles whose boxes meet the box from lo to hi, in no order.
    std::vector<std::uint32_t> near(Vec3f lo, Vec3f hi) const;

private:
    struct Node {
        Vec3f lo, hi;
        std::uint32_t first = 0, count = 0; // a leaf's triangles in order_; count 0: an inner node,
        std::uint32_t second = 0;           // whose children are nodes first and second
    };
    std::vector<Node> nodes_;
    std::vector<std::uint32_t> order_;
};

// pick with an index for each placed mesh (the same answer, sooner).
std::optional<PlateHit> pick(const std::vector<TriangleMesh>& placed, const std::vector<TriangleIndex>& indexes,
                             Vec3 origin, Vec3 direction);

// The unit normal of a triangle of a mesh (from its winding).
Vec3 triangle_normal(const TriangleMesh& mesh, std::size_t triangle);

// An object's own settings (settings_levels.md, rules 10 and 11), as the
// JSON text PlateObject::settings holds: the keys it sets ("infill.density"),
// in order; a key added with the profile's value now; a key taken away; and
// its keys' values taken from settings edited in the panel.
std::vector<std::string> own_keys(const std::string& own);

// The rows under an object in the list of the plate's objects
// (settings_levels.md, rules 11 and 13): one per group of its own settings
// (walls, infill and surfaces, in that order, then any other), one per
// modifier or negative volume, one per height range.
struct ObjectRow {
    enum class Kind { Settings, Volume, Range } kind = Kind::Settings;
    std::string group{};   // Settings: its group
    std::size_t index = 0; // Volume and Range: which one
    friend bool operator==(const ObjectRow&, const ObjectRow&) = default;
};
std::vector<ObjectRow> object_rows(const PlateObject& object);
std::string with_own_key(const std::string& own, const Settings& profile, std::string_view key);
std::string without_own_key(const std::string& own, std::string_view key);
std::string own_values(const std::string& own, const Settings& edited);
// The keys an object may set today (walls, infill and surfaces; own_settings
// refuses the rest), in the panel's order.
std::vector<std::string> own_setting_keys(const Settings& profile);

} // namespace tisma::ui
