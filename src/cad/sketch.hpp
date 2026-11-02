#pragma once

// Sketches of the Design tab (docs/spec/features/cad_design.md, stage D2):
// lines, rectangles, circles and arcs on a plane, the points typed as the
// drafting programs take them (rule 13), the object snaps (rule 15),
// ortho and polar tracking (rule 16), and the SolidCraft commands that
// draw the sketch and extrude or revolve it (rules 4, 6 and 7). Own design;
// see PROVENANCE.md.

#include <optional>
#include <string>
#include <vector>

namespace tisma::cad {

struct Pt {
    double x = 0.0, y = 0.0;
    friend bool operator==(const Pt&, const Pt&) = default;
};

// The sketch's plane: sketch x, y along world +X, +Y (XY); +X, -Z (XZ);
// +Y, +Z (YZ), as SolidCraft lays them (its sketch.create).
enum class SketchPlane { XY, XZ, YZ };
const char* plane_name(SketchPlane plane);
std::optional<SketchPlane> plane_named(const std::string& name);

// One drawn thing. A chain is the Line tool's connected points (closed: the
// last joins the first); a rectangle is two opposite corners; a circle its
// centre and radius; an arc its centre, start and end, counter-clockwise
// unless clockwise.
struct Entity {
    enum class Kind { Chain, Rectangle, Circle, Arc } kind = Kind::Chain;
    std::vector<Pt> points; // chain: its points; rectangle: p0, p1; circle: centre; arc: centre, start, end
    double radius = 0.0;    // circle
    bool closed = false;    // chain
    bool clockwise = false; // arc
    bool construction = false;
    friend bool operator==(const Entity&, const Entity&) = default;
};

// A point of an entity: a chain's point `index`, a rectangle's corner
// (0 bottom-left, 1 bottom-right, 2 top-right, 3 top-left), a circle's or
// arc's centre (index 0), an arc's start (1) or end (2).
struct EntityPoint {
    std::size_t entity = 0;
    std::size_t index = 0;
    friend bool operator==(const EntityPoint&, const EntityPoint&) = default;
};

// What a snap left behind (rule 15): the point tied to another point, to a
// line's midpoint, or onto a curve.
struct Constraint {
    enum class Kind { Coincident, Midpoint, OnCurve } kind = Kind::Coincident;
    EntityPoint point;     // the point that snapped
    std::size_t target = 0; // the entity it snapped to
    std::size_t target_index = 0; // its point (coincident), or the segment of a chain (midpoint, on curve)
    friend bool operator==(const Constraint&, const Constraint&) = default;
};

// A length typed for a segment (rule 14; CD13 «con sus cotas»): a chain's
// segment `segment` (from point i to i + 1) is `length` mm.
struct Dimension {
    std::size_t entity = 0, segment = 0;
    double length = 0.0;
    friend bool operator==(const Dimension&, const Dimension&) = default;
};

struct Sketch {
    SketchPlane plane = SketchPlane::XY;
    std::vector<Entity> entities;
    std::vector<Constraint> constraints;
    std::vector<Dimension> dimensions;
    bool empty() const { return entities.empty(); }
    // Whether any entity closes a region to extrude: a closed chain of three
    // points or more, a rectangle, a circle.
    bool has_region() const;
    friend bool operator==(const Sketch&, const Sketch&) = default;
};

// The point an entity's EntityPoint names.
Pt point_of(const Entity& e, std::size_t index);
// A chain's segments (closed: back to the start), a rectangle's four sides,
// as pairs of points; none for a circle or an arc.
std::vector<std::pair<Pt, Pt>> segments_of(const Entity& e);

// A point typed on the command line (rule 13): `x,y` absolute; `@dx,dy` from
// `last`; `@distance<angle` from `last`, degrees from +X counter-clockwise.
// Numbers take '.' as the decimal point (the comma separates x from y).
// None when the text is not a point.
std::optional<Pt> parse_typed_point(const std::string& text, const Pt& last);
// A plain number typed for a length or an angle at the cursor (rule 14):
// '.' or ',' as the decimal point. None when not a number.
std::optional<double> parse_typed_value(const std::string& text);

// The snaps (rule 15), with a marker of its own each.
enum class SnapKind { None, Endpoint, Midpoint, Center, Quadrant, Intersection, Perpendicular, Tangent, Nearest, Grid };
const char* snap_name(SnapKind kind);
struct Snap {
    SnapKind kind = SnapKind::None;
    Pt at;
    std::size_t entity = 0; // the entity snapped to (not for Grid)
    std::size_t index = 0;  // its point (Endpoint, Center, Quadrant) or segment (Midpoint, Perpendicular, Tangent, Nearest)
};
struct SnapOptions {
    bool enabled = true;      // F3
    double radius = 2.0;      // mm: how near the cursor must be
    bool grid = false;        // F9
    double grid_step = 1.0;   // mm
    std::optional<Pt> from;   // the segment's start, for perpendicular and tangent
};
// The nearest snap to `cursor`, the stronger kinds first (endpoint over
// midpoint over centre...), within the radius; a grid point when the grid is
// on and nothing else is near; None otherwise.
Snap snap_point(const Sketch& sketch, const Pt& cursor, const SnapOptions& options);

// Ortho (F8) and polar tracking (F10, rule 16): the point the cursor gives
// for a segment from `from`: ortho keeps the segment horizontal or vertical
// (whichever is nearer); polar snaps its angle to the nearest multiple of
// `polar_step` degrees when within `polar_tolerance` degrees of it, keeping
// the length. Ortho wins over polar.
Pt tracked_point(const Pt& from, const Pt& cursor, bool ortho, double polar_step, double polar_tolerance = 4.0);

// The operation a sketch feeds (rule 7): extrude by a distance along the
// plane's normal, or revolve about a sketch axis (x or y) by an angle.
struct SketchOperation {
    enum class Kind { Extrude, Revolve } kind = Kind::Extrude;
    double distance = 20.0;  // mm, extrude
    char axis = 'y';         // 'x' or 'y', revolve
    double angle = 360.0;    // degrees, revolve
    friend bool operator==(const SketchOperation&, const SketchOperation&) = default;
};

// The SolidCraft commands that draw the sketch, as JSON objects in order
// (sketch.create, the entities with their ids counted as SolidCraft counts
// them, the constraints, the dimensions, sketch.finish), then the operation
// that makes the body named `body`.
std::vector<std::string> sketch_commands(const Sketch& sketch, const SketchOperation& op, const std::string& body);

// A chain's point `index` as SolidCraft names it ("l3.start", "l3.end"),
// given the sketch's entities before it: the line ids count every line of
// every earlier chain and rectangle.
std::string point_id(const Sketch& sketch, const EntityPoint& p);
// A chain's segment, or a rectangle's side, as a line id ("l3").
std::string line_id(const Sketch& sketch, std::size_t entity, std::size_t segment);

// The sketch as JSON (the project keeps it) and back. Throws
// std::runtime_error on bad input.
std::string sketch_json(const Sketch& sketch, const SketchOperation& op);
void parse_sketch(const std::string& json, Sketch& sketch, SketchOperation& op);

} // namespace tisma::cad
