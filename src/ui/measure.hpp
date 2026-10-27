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
    enum class Kind { Point, Plane } kind = Kind::Point;
    Point3 point{};  // the vertex, or a point of the plane (the face's centre)
    Point3 normal{}; // a plane's, of unit length, facing out of the model
};

// The plane of a triangle of the mesh.
MeasureFeature plane_feature(const TriangleMesh& mesh, std::size_t triangle);
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
