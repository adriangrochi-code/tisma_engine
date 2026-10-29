#pragma once

#include "config/settings.hpp"
#include "geom/polygons.hpp"
#include "mesh/indexed_mesh.hpp"
#include "slice/layers.hpp"

#include <array>
#include <vector>

// Resin supports and the pad (docs/spec/features/resin_supports.md, rules 1
// to 3): points where the part hangs, a pillar under each, and the plate
// they stand on. Own design; see PROVENANCE.md.
namespace tisma {

// The faces that look down within this angle of flat get points.
inline constexpr double kResinSupportFaceAngle = 45.0; // degrees

// The support points (rule 3), on the part as it is printed: on every face
// looking down within kResinSupportFaceAngle of flat, at the nodes of a
// grid 4 mm apart at resin.support_density 100 (closer as it grows) that
// fall on it; and one inside every island of a section that starts over
// nothing (the lowest point of a piece) without a point of its own. Points
// lower than half a millimetre over `floor` (the pad's top or the platform)
// are left out.
std::vector<std::array<double, 3>> support_points(const IndexedMesh& part, const std::vector<Polygons>& sections,
                                                  const std::vector<Layer>& layers, const Settings& settings,
                                                  double floor);

// A support (rule 3): a tip of resin.support_tip_diameter into the part at
// the point, widening to the pillar over max(1 mm, the pillar's diameter);
// the pillar straight down; and at its foot, where it stands on the pad or
// the platform, a flare from the pillar to twice its diameter over one
// diameter. A pillar the part blocks on its way down stands on the part
// instead, on a tip like the top one.
struct ResinPillar {
    double x = 0.0, y = 0.0;
    double point = 0.0; // the support point's height
    double land = 0.0;  // where it stands
    bool on_part = false;
};

// The pillars under the points: each goes down from its point until the
// part (with 0.3 mm to spare) is in its way, or to `floor`. Points with no
// room under them for their tip are dropped.
std::vector<ResinPillar> support_pillars(const std::vector<std::array<double, 3>>& points,
                                         const std::vector<Polygons>& sections, const std::vector<Layer>& layers,
                                         const Settings& settings, double floor);

// A pillar's radius at height z; 0 where there is none.
double pillar_radius(const ResinPillar& pillar, const Settings& settings, double z);

// The pad's outline at its top (rule 2): the pillars' feet standing on it,
// and with resin.pad_around_object the part's sections within the pad's
// height, grown by resin.pad_margin and closed over the gaps between them.
Polygons pad_outline(const std::vector<ResinPillar>& pillars, const std::vector<Polygons>& sections,
                     const std::vector<Layer>& layers, const Settings& settings);

// What the supports and the pad fill at height z: the pillars' discs, and
// under the pad's top its outline, wider downwards by resin.pad_wall_slope.
Polygons support_section(const std::vector<ResinPillar>& pillars, const Polygons& pad, const Settings& settings,
                         double z);

} // namespace tisma
