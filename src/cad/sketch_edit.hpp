#pragma once

// Editing a sketch (docs/spec/features/cad_design.md, rule 17): trim and
// extend to the nearest curve, offset by a distance, chamfer a corner,
// mirror about a line, rectangular and polar patterns, move, copy, rotate and
// scale about a base point, and selection by window or by crossing. Own
// design (see PROVENANCE.md): plane geometry on the sketch's own entities;
// what a change makes stale (the constraints and dimensions on an entity
// that changed) is dropped, so the sketch never keeps a rule it no longer
// meets.

#include <cstddef>
#include <vector>

#include "cad/sketch.hpp"

namespace tisma::cad {

// Where the curves of a sketch cross the entity `entity`'s curve: points on
// it, from every other curve and from its own non-neighbouring segments.
std::vector<Pt> crossings_on(const Sketch& sketch, std::size_t entity);

// TR (rule 17; CD17): the piece of the curve under `pick` (within
// `tolerance` mm) between the crossings either side of it is cut away. A
// line or a circle that nothing crosses is deleted whole. A rectangle
// trimmed becomes a chain; a circle, an arc; a chain cut in the middle, two.
// False when no curve is near.
bool trim_at(Sketch& sketch, const Pt& pick, double tolerance);

// EX: the end of an open chain nearest `pick` moves along its last segment
// to the nearest curve ahead. False when nothing is ahead.
bool extend_at(Sketch& sketch, const Pt& pick, double tolerance);

// O: a copy of the entity `distance` mm away on the side of `side`: a
// circle or an arc concentric, a rectangle grown or shrunk, a chain with each
// segment moved and the corners mitred. The new entity's index, or none when
// it cannot be (a radius or a rectangle through zero).
std::optional<std::size_t> offset_entity(Sketch& sketch, std::size_t entity, double distance, const Pt& side);

// CHA: the corner `corner` of a chain (not an open chain's end) or a
// rectangle (as point_of counts them) cut `distance` mm back along both sides.
// A rectangle becomes a closed chain. False when a side is too short.
bool chamfer_corner(Sketch& sketch, std::size_t entity, std::size_t corner, double distance);

// F: the corner rounded with an arc of `radius` tangent to both sides. The
// chain is opened there: the sides before and after it and the arc meet end
// to end, so a closed outline stays a region (joined_loops). False when the
// sides are too short for the radius or the corner is straight.
bool fillet_corner(Sketch& sketch, std::size_t entity, std::size_t corner, double radius);

// A plane map x' = a x + b y + tx, y' = c x + d y + ty.
struct Map2 {
    double a = 1.0, b = 0.0, c = 0.0, d = 1.0, tx = 0.0, ty = 0.0;
    Pt operator()(const Pt& p) const { return {a * p.x + b * p.y + tx, c * p.x + d * p.y + ty}; }
    Map2 then(const Map2& next) const; // this, then next
};
Map2 moved(double dx, double dy);
Map2 rotated(const Pt& base, double degrees);       // counter-clockwise
Map2 scaled(const Pt& base, double factor);
Map2 mirrored(const Pt& p1, const Pt& p2);         // about the line through p1 and p2

// M, RO, SC (in place) and CO (copy = true): the entities mapped; a
// rectangle no longer square to the axes becomes a closed chain; a circle's
// radius scales; a mirror turns arcs the other way. The indices of the
// mapped entities (the copies, when copying). Nothing for a scale of 0.
std::vector<std::size_t> map_entities(Sketch& sketch, const std::vector<std::size_t>& which, const Map2& map, bool copy);

// MI: the entities mirrored about the line p1 p2, as copies, the originals
// kept unless `erase_source`.
std::vector<std::size_t> mirror_entities(Sketch& sketch, const std::vector<std::size_t>& which, const Pt& p1, const Pt& p2,
                                         bool erase_source = false);

// AR: copies in rows and columns (counts include the original; the steps
// in mm along x and y), or about a centre (count in all, over `angle`
// degrees: a full turn shares it out, a part of one puts the last copy at
// its end; CD18). The copies' indices.
std::vector<std::size_t> pattern_rectangular(Sketch& sketch, const std::vector<std::size_t>& which, int columns, double dx,
                                             int rows, double dy);
std::vector<std::size_t> pattern_polar(Sketch& sketch, const std::vector<std::size_t>& which, const Pt& centre, int count,
                                       double angle = 360.0);

// Selection by a box from `from` to `to` (rule 17): dragged to the right, a
// window, only what lies inside; to the left, a crossing, all it touches.
std::vector<std::size_t> select_box(const Sketch& sketch, const Pt& from, const Pt& to);

// Medir (rule 18): the region under a point, without adding dimensions: the
// smallest outline round it (a closed chain, a rectangle, a circle, a loop
// of loose pieces) less the outlines inside it (its holes); its area in mm²
// and its perimeter, the holes' edges counted. None outside every region.
struct RegionMeasure {
    double area = 0.0, perimeter = 0.0;
    int holes = 0;
};
std::optional<RegionMeasure> measure_region_at(const Sketch& sketch, const Pt& at);
// Every closed outline of the sketch as a polygon (circles in 720 sides).
std::vector<std::vector<Pt>> sketch_outlines(const Sketch& sketch);

// The entity removed, and the rules on it with it; later indices shift.
void erase_entity(Sketch& sketch, std::size_t entity);

} // namespace tisma::cad
