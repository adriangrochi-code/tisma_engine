#pragma once

#include "config/settings.hpp"
#include "mesh/indexed_mesh.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// Curved layers on a 3-axis printer (docs/spec/features/nonplanar.md): each
// layer is the surface z = s + D(x, y, z) for a slicing height s. The mesh is
// moved to z' = z - D, sliced with flat layers, and every point of the G-code
// goes back to z = z' + D, its material by the local thickness J. Own design
// after the spec; see PROVENANCE.md.
namespace tisma {

class CurvedError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// D = g(x, y, z) · ramp(z), x and y from the centre of the part's box.
struct CurvedField {
    CurvedMode mode = CurvedMode::Disabled;
    CurvedPattern pattern = CurvedPattern::Egg;
    double cx = 0.0, cy = 0.0;  // the part's box centre in XY, mm
    double base = 0.0;          // the part's lowest z (on the bed), mm
    double height = 0.0;        // the part's height, mm
    double amplitude = 0.0;     // a wave's, mm
    double k = 0.0;             // a wave's 2π / λ, 1/mm
    double angle = 0.0;         // ridges' direction, radians
    double twist = 0.0;         // twisted ridges' turn, radians per mm of height
    double cone = 0.0;          // a cone's slope; above 0 it rises outwards
    double flat_below = 0.6, ramp_height = 5.0;
    bool flat_top = false;
    double radius = 0.0;        // how far from the centre the nozzle may go (the bed's corners), mm
    // Top faces (nonplanar_modes.md, rules 14 to 18): h on a grid of nx by
    // ny nodes `cell` mm apart from (grid_x, grid_y), bilinear between and
    // level past its edges, by `scale`; ramped in evenly from flat_below up
    // to `full` (from the base), where the sloping top faces start.
    std::vector<float> grid;
    int nx = 0, ny = 0;
    double grid_x = 0.0, grid_y = 0.0, cell = 1.0, full = 0.0, scale = 1.0;

    // The shape at (x, y) (from the part's centre) and height z (from its base).
    double shape(double x, double y, double z) const;
    // From 0 under curved.flat_below up to 1 (rule 2), z from the part's base.
    double ramp(double z) const;
    // D at a point of the bed's coordinates.
    double offset(double x, double y, double z) const;
    double offset_dz(double x, double y, double z) const; // ∂D/∂z
    std::array<double, 2> slope(double x, double y, double z) const; // ∂D/∂x, ∂D/∂y
    // The local thickness J = 1 / (1 - ∂D/∂z).
    double thickness(double x, double y, double z) const;
    // The real z of the point at (x, y) of the flat height `flat`: z - D = flat.
    double real_z(double x, double y, double flat) const;
    // Whether D is 0 at every point of flat height `flat` (under the ramp).
    bool flat_at(double flat) const { return flat <= base + flat_below + 1e-9; }
    // The most |D| anywhere, mm.
    double most() const;
};

// The field the settings ask for on this part (rules 1 to 3 and 6): the
// wave or cone, as large as the head lets it be with curved.fit_head, and
// shrunk until J stays within 0.6 to 1.4 everywhere in the part. With
// surface (nonplanar_modes.md, rules 14 to 18), one smooth h fitted by least
// squares to the slopes of the faces looking up, each weighed by its area
// and by how flat it lies, the steepest at the head's free slope, then
// shrunk the same way; empty when nothing slopes (a flat top, CO1). Without
// curved.fit_head, a layer steeper than the head allows, or one whose
// material rises next to the nozzle past head.clearance_height, or J out of
// bounds, is a CurvedError. Empty with curved.mode disabled.
std::optional<CurvedField> curved_field(const Settings& settings, const IndexedMesh& mesh);

// The mesh with every edge longer than `longest` split in halves until none
// is (edges wholly under `flat_below` are left as they are), the shared
// edges split once so the mesh stays closed. origin[t] is the triangle of
// the given mesh the new triangle t comes from; children follow one another
// in the given triangles' order.
IndexedMesh refined_mesh(const IndexedMesh& mesh, double longest, double flat_below, std::vector<std::uint32_t>* origin);

// Each vertex moved to z - D (the slicing space).
void flatten_mesh(IndexedMesh& mesh, const CurvedField& field);

// The G-code of the flat slicing with every point back on its curved layer
// (rules 4 and 5): moves on curved layers split every curved.segment_length
// mm, each piece's Z the real one, its E the flat one by J at its middle,
// its feed divided by J where J > 1 (never faster than planned) with
// curved.flow_policy preserve or uniform, and no faster
// than machine.max_feed_z lets Z go. Relative E (rule 7). Flat layers and
// what is not a move pass as they are.
std::string curve_gcode(std::string_view text, const CurvedField& field, const Settings& settings);

} // namespace tisma
