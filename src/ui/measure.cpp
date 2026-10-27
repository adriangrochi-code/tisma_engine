#include "ui/measure.hpp"

#include <algorithm>
#include <cmath>

namespace tisma::ui {
namespace {

Point3 sub(const Point3& a, const Point3& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
double dot(const Point3& a, const Point3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Point3 of(const Vec3f& v) { return {v.x, v.y, v.z}; }

constexpr double kDegree = 3.14159265358979323846 / 180.0;

} // namespace

MeasureFeature plane_feature(const TriangleMesh& mesh, std::size_t triangle) {
    const Point3 a = of(mesh.corner(triangle, 0)), b = of(mesh.corner(triangle, 1)), c = of(mesh.corner(triangle, 2));
    const Point3 u = sub(b, a), v = sub(c, a);
    Point3 n{u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
    const double l = std::sqrt(dot(n, n));
    if (l > 0.0)
        n = {n[0] / l, n[1] / l, n[2] / l};
    MeasureFeature f;
    f.kind = MeasureFeature::Kind::Plane;
    f.point = {(a[0] + b[0] + c[0]) / 3.0, (a[1] + b[1] + c[1]) / 3.0, (a[2] + b[2] + c[2]) / 3.0};
    f.normal = n;
    return f;
}

MeasureFeature point_feature(const TriangleMesh& mesh, std::size_t triangle, const Point3& hit) {
    MeasureFeature f;
    double best = INFINITY;
    for (int k = 0; k < 3; ++k) {
        const Point3 p = of(mesh.corner(triangle, k));
        const Point3 d = sub(p, hit);
        if (dot(d, d) < best)
            best = dot(d, d), f.point = p;
    }
    return f;
}

Measurement measure(const MeasureFeature& a, const MeasureFeature& b) {
    using Kind = MeasureFeature::Kind;
    Measurement m;
    if (a.kind == Kind::Point && b.kind == Kind::Point) {
        const Point3 d = sub(a.point, b.point);
        m.distance = std::sqrt(dot(d, d));
    } else if (a.kind != b.kind) {
        const MeasureFeature& plane = a.kind == Kind::Plane ? a : b;
        const MeasureFeature& point = a.kind == Kind::Plane ? b : a;
        m.distance = std::abs(dot(sub(point.point, plane.point), plane.normal));
    } else {
        const double c = std::clamp(dot(a.normal, b.normal), -1.0, 1.0);
        if (std::abs(c) >= std::cos(0.5 * kDegree))
            m.distance = std::abs(dot(sub(b.point, a.point), a.normal));
        else
            m.angle = 180.0 - std::acos(c) / kDegree; // between the faces, from their outward normals
    }
    return m;
}

} // namespace tisma::ui
