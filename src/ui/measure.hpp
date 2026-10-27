#pragma once

#include "mesh/mesh.hpp"

#include <array>
#include <cstddef>
#include <optional>

// The Measure tool (docs/spec/features/model_tools.md, rule 20): a face of
// a model picks its plane, a vertex its point; between two of them, the
// distance or the angle, and the factor that makes a distance what it
// should be. Own design on that spec.
namespace tisma::ui {

using Point3 = std::array<double, 3>;

struct MeasureFeature {
    enum class Kind { Point, Plane, Circle, Cone } kind = Kind::Point;
    Point3 point{};  // the vertex, a point of the plane (the face's centre), or the middle of a round surface's axis
    Point3 normal{}; // a plane's, of unit length, facing out of the model; a round surface's axis
    // A round surface (a cylinder, a hole, a cone): its diameter at both
    // ends (equal for a cylinder), its full angle in degrees (a cone's) and
    // whether it is a hole (the surface faces its axis).
    double diameter = 0.0, diameter_end = 0.0, angle = 0.0;
    bool hole = false;
};

// The plane of a triangle of the mesh.
MeasureFeature plane_feature(const TriangleMesh& mesh, std::size_t triangle);
// What the surface a triangle belongs to is (rule 20, «círculos»): the
// smooth stretch of faces around it, joined while neighbours turn less than
// 25°. All flat: its plane. Round: the axis that its faces' normals turn
// about (the direction their spread is least along) and the circle fitted to
// its corners across that axis (Kåsa 1976): a cylinder or a hole with its
// diameter, or, when the normals lean on the axis, a cone with its angle and
// the diameters at its two ends.
MeasureFeature surface_feature(const TriangleMesh& mesh, std::size_t triangle);

// The triangle's corner nearest to where it was hit.
MeasureFeature point_feature(const TriangleMesh& mesh, std::size_t triangle, const Point3& hit);

// Between two features: the distance (two points, a point and a plane, or
// two parallel planes, within half a degree) or, between planes that are not
// parallel, the angle between them in degrees (90 for two sides of a box).
struct Measurement {
    std::optional<double> distance;
    std::optional<double> angle;
};
Measurement measure(const MeasureFeature& a, const MeasureFeature& b);

} // namespace tisma::ui
