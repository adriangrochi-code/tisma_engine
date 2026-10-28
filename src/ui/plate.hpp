#pragma once

#include "config/settings.hpp"
#include "mesh/mesh.hpp"
#include "mesh/paint.hpp"
#include "ui/math.hpp"

#include <array>
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
};

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
std::string with_own_key(const std::string& own, const Settings& profile, std::string_view key);
std::string without_own_key(const std::string& own, std::string_view key);
std::string own_values(const std::string& own, const Settings& edited);
// The keys an object may set today (walls, infill and surfaces; own_settings
// refuses the rest), in the panel's order.
std::vector<std::string> own_setting_keys(const Settings& profile);

} // namespace tisma::ui
