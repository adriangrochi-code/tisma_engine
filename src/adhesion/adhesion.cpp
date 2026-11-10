// Skirt and brim. Own design; see PROVENANCE.md.

#include "adhesion/adhesion.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace tisma {
namespace {

// The region's outer contours (counter-clockwise ones), holes left out.
Polygons outer_contours(const Polygons& region) {
    Polygons out;
    for (const Polygon& p : region)
        if (Clipper2Lib::Area(p) > 0.0)
            out.push_back(p);
    return out;
}

// Skirt and brim loops get negative indices (-1 for the first) so that their
// seams never count as the seams of a wall index (SeamMemory).
Toolpath loop_of(const Polygon& polygon, double bead, int index) {
    Toolpath t;
    t.closed = true;
    t.index = index;
    t.points.reserve(polygon.size());
    for (const auto& q : polygon)
        t.points.push_back({{to_mm(q.x), to_mm(q.y)}, bead});
    return t;
}

int brim_count(double width, double spacing) { return width > 0.0 ? int(std::floor(width / spacing + 1e-9)) : 0; }

// The outer contours grown by delta mm with round corners (arcs within 5 um).
Polygons grown(const Polygons& outers, double delta) {
    return Clipper2Lib::InflatePaths(outers, delta * kUnitsPerMm, Clipper2Lib::JoinType::Round,
                                     Clipper2Lib::EndType::Polygon, 2.0, 0.005 * kUnitsPerMm);
}

} // namespace

double loop_spacing(double bead, double height) {
    return std::max(bead - (1.0 - std::numbers::pi / 4.0) * std::max(height, 0.0), 0.5 * bead);
}

std::vector<Toolpath> brim_loops(const Polygons& region, double width, double bead, double height) {
    AdhesionShape shape;
    shape.brim_width = width;
    return brim_loops(region, shape, bead, height);
}

std::vector<Toolpath> skirt_loops(const Polygons& region, double brim_width, int loops, double distance,
                                  double bead, double height) {
    AdhesionShape shape;
    shape.brim_width = brim_width;
    shape.skirt_loops = loops;
    shape.skirt_distance = distance;
    return skirt_loops(region, shape, bead, height);
}

AdhesionShape adhesion_shape(const Settings& s) {
    AdhesionShape a;
    a.brim_width = s.brim_width;
    a.brim_type = s.brim_type;
    a.brim_gap = s.brim_gap;
    a.ears = s.brim_ears;
    a.ears_max_angle = s.brim_ears_max_angle;
    a.ears_detection = s.brim_ears_detection_length;
    a.ears_outer_only = s.brim_ears_outer_only;
    a.skirt_loops = s.skirt_layers > 0 || s.draft_shield == DraftShield::Enabled ? s.skirt_loops : 0;
    a.skirt_distance = s.skirt_distance;
    a.shield = s.draft_shield != DraftShield::Disabled;
    a.skirt_min_filament = s.skirt_min_filament;
    a.filament_area = std::numbers::pi * s.filament_diameter * s.filament_diameter / 4.0;
    return a;
}

namespace {

bool outer_brim(const AdhesionShape& a) {
    return a.brim_width > 0.0 && (a.brim_type == BrimType::Outer || a.brim_type == BrimType::OuterAndInner);
}

// How far from the part the skirt's loop k runs.
double skirt_offset(const AdhesionShape& a, double spacing, int k) {
    const double base = outer_brim(a) && !a.shield ? a.brim_gap + brim_count(a.brim_width, spacing) * spacing : 0.0;
    return base + a.skirt_distance + spacing * (k + 0.5);
}

} // namespace

namespace {

// Brim ears (orca_2_5.md, rule 14): discs of gap + width round each sharp
// corner, the outer contours' and, unless outer only, the holes'. A corner
// is sharp when the angle inside the shape it bounds (the part for an outer
// contour, the hole for a hole) is under the maximum, and it points out of
// that shape.
Polygons ear_discs(const Polygons& region, const AdhesionShape& a, bool outers, bool holes) {
    Polygons discs;
    const double radius = a.brim_gap + a.brim_width;
    const Polygons shapes = a.ears_detection > 0.0 ? simplify(region, a.ears_detection) : region;
    for (const Polygon& p : shapes) {
        const bool outer = Clipper2Lib::Area(p) > 0.0;
        if ((outer && !outers) || (!outer && !holes) || p.size() < 3)
            continue;
        const std::size_t n = p.size();
        for (std::size_t i = 0; i < n; ++i) {
            const auto& prev = p[(i + n - 1) % n];
            const auto& at = p[i];
            const auto& next = p[(i + 1) % n];
            const double ux = to_mm(prev.x) - to_mm(at.x), uy = to_mm(prev.y) - to_mm(at.y);
            const double vx = to_mm(next.x) - to_mm(at.x), vy = to_mm(next.y) - to_mm(at.y);
            const double lu = std::hypot(ux, uy), lv = std::hypot(vx, vy);
            if (!(lu > 0.0) || !(lv > 0.0))
                continue;
            // Convex for the shape it bounds: a left turn on a counter-clockwise
            // outer contour, a right turn on a clockwise hole.
            const double turn = (to_mm(at.x) - to_mm(prev.x)) * vy - (to_mm(at.y) - to_mm(prev.y)) * vx;
            if (outer ? turn <= 0.0 : turn >= 0.0)
                continue;
            const double angle = std::acos(std::clamp((ux * vx + uy * vy) / (lu * lv), -1.0, 1.0)) * 180.0 / std::numbers::pi;
            if (!(angle < a.ears_max_angle))
                continue;
            Polygon disc;
            constexpr int kSides = 48;
            for (int k = 0; k < kSides; ++k) {
                const double t = 2.0 * std::numbers::pi * k / kSides;
                disc.push_back({static_cast<std::int64_t>(std::llround(double(at.x) + radius * std::cos(t) * kUnitsPerMm)),
                                static_cast<std::int64_t>(std::llround(double(at.y) + radius * std::sin(t) * kUnitsPerMm))});
            }
            discs.push_back(std::move(disc));
        }
    }
    return Clipper2Lib::Union(discs, Clipper2Lib::FillRule::NonZero);
}

// The parts of the closed loops inside the ears, as open paths.
void add_ear_pieces(const Polygons& loops, const Polygons& ears, double bead, int index, std::vector<Toolpath>& out) {
    if (loops.empty() || ears.empty())
        return;
    Polygons open;
    for (const Polygon& p : loops) {
        Polygon closed = p;
        closed.push_back(p.front());
        open.push_back(std::move(closed));
    }
    Clipper2Lib::Clipper64 clipper;
    clipper.AddOpenSubject(open);
    clipper.AddClip(ears);
    Polygons inside, pieces;
    clipper.Execute(Clipper2Lib::ClipType::Intersection, Clipper2Lib::FillRule::NonZero, inside, pieces);
    for (const Polygon& piece : pieces) {
        if (piece.size() < 2)
            continue;
        Toolpath t = loop_of(piece, bead, index);
        t.closed = false;
        out.push_back(std::move(t));
    }
}

} // namespace

std::vector<Toolpath> brim_loops(const Polygons& region, const AdhesionShape& a, double bead, double height) {
    std::vector<Toolpath> out;
    const double spacing = loop_spacing(bead, height);
    const int count = brim_count(a.brim_width, spacing);
    if (a.ears && a.brim_width > 0.0 && a.brim_type != BrimType::None) {
        const bool holes = !a.ears_outer_only;
        const Polygons outer_ears = outer_brim(a) ? ear_discs(region, a, true, false) : Polygons{};
        const Polygons hole_ears = holes ? ear_discs(region, a, false, true) : Polygons{};
        for (int k = 0; k < count; ++k) {
            const double offset = a.brim_gap + spacing * (k + 0.5);
            Polygons outer_loops, hole_loops;
            for (const Polygon& p : grown(region, offset))
                (Clipper2Lib::Area(p) > 0.0 ? outer_loops : hole_loops).push_back(p);
            add_ear_pieces(outer_loops, outer_ears, bead, -1 - k, out);
            add_ear_pieces(hole_loops, hole_ears, bead, -1 - k, out);
        }
        return out;
    }
    // With a shield, no brim loop under a skirt loop: one whose bead the
    // skirt's axis falls in (rule 4, B3).
    const auto under_skirt = [&](double offset) {
        if (!a.shield)
            return false;
        for (int j = 0; j < a.skirt_loops; ++j)
            if (std::abs(offset - skirt_offset(a, spacing, j)) < spacing / 2.0 + 1e-9)
                return true;
        return false;
    };
    if (outer_brim(a)) {
        const Polygons outers = outer_contours(region);
        for (int k = 0; k < count; ++k) {
            const double offset = a.brim_gap + spacing * (k + 0.5);
            if (!under_skirt(offset))
                for (const Polygon& p : grown(outers, offset))
                    out.push_back(loop_of(p, bead, -1 - k));
        }
    }
    if (a.brim_width > 0.0 && (a.brim_type == BrimType::Inner || a.brim_type == BrimType::OuterAndInner)) {
        // The whole region grown: its holes shrink, round any island in them.
        for (int k = 0; k < count; ++k) {
            bool any = false;
            for (const Polygon& p : grown(region, a.brim_gap + spacing * (k + 0.5)))
                if (Clipper2Lib::Area(p) < 0.0) {
                    out.push_back(loop_of(p, bead, -1 - k));
                    any = true;
                }
            if (!any) // the holes are full
                break;
        }
    }
    return out;
}

std::vector<Toolpath> skirt_loops(const Polygons& region, const AdhesionShape& a, double bead, double height) {
    std::vector<Toolpath> out;
    if (a.skirt_loops <= 0)
        return out;
    const Polygons outers = outer_contours(region);
    const double spacing = loop_spacing(bead, height);
    double filament = 0.0; // mm laid so far
    const auto enough = [&] {
        return !(a.skirt_min_filament > 0.0 && a.filament_area > 0.0) || filament >= a.skirt_min_filament;
    };
    // Past the loops asked, more until enough filament (rule 1), up to a bound.
    for (int k = 0; k < a.skirt_loops || (!enough() && k < kMaxSkirtLoops); ++k)
        for (const Polygon& p : grown(outers, skirt_offset(a, spacing, k)))
            if (Clipper2Lib::Area(p) > 0.0) {
                out.push_back(loop_of(p, bead, -1 - k));
                double length = 0.0;
                for (std::size_t i = 0; i < p.size(); ++i) {
                    const auto& q = p[i];
                    const auto& r = p[(i + 1) % p.size()];
                    length += std::hypot(to_mm(r.x) - to_mm(q.x), to_mm(r.y) - to_mm(q.y));
                }
                filament += length * bead * height / std::max(a.filament_area, 1e-9);
            }
    return out;
}

} // namespace tisma
