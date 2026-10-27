#include "ui/measure.hpp"

#include "mesh/indexed_mesh.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

namespace tisma::ui {
namespace {

Point3 sub(const Point3& a, const Point3& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
double dot(const Point3& a, const Point3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Point3 of(const Vec3f& v) { return {v.x, v.y, v.z}; }
Point3 cross3(const Point3& a, const Point3& b) {
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
Point3 unit(Point3 a) {
    const double l = std::sqrt(dot(a, a));
    return l > 0.0 ? Point3{a[0] / l, a[1] / l, a[2] / l} : a;
}

// The eigenvector of a symmetric 3 × 3 matrix with the smallest eigenvalue,
// by Jacobi rotations.
Point3 smallest_eigenvector(double m[3][3]) {
    double v[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    for (int sweep = 0; sweep < 50; ++sweep) {
        const double off = m[0][1] * m[0][1] + m[0][2] * m[0][2] + m[1][2] * m[1][2];
        if (off < 1e-30)
            break;
        for (int p = 0; p < 3; ++p)
            for (int q = p + 1; q < 3; ++q) {
                if (std::abs(m[p][q]) < 1e-300)
                    continue;
                const double theta = 0.5 * (m[q][q] - m[p][p]) / m[p][q];
                const double t = (theta >= 0 ? 1.0 : -1.0) / (std::abs(theta) + std::sqrt(theta * theta + 1.0));
                const double c = 1.0 / std::sqrt(t * t + 1.0), s = t * c;
                for (int k = 0; k < 3; ++k) { // columns
                    const double mkp = m[k][p], mkq = m[k][q];
                    m[k][p] = c * mkp - s * mkq, m[k][q] = s * mkp + c * mkq;
                }
                for (int k = 0; k < 3; ++k) { // rows
                    const double mpk = m[p][k], mqk = m[q][k];
                    m[p][k] = c * mpk - s * mqk, m[q][k] = s * mpk + c * mqk;
                }
                for (int k = 0; k < 3; ++k) {
                    const double vkp = v[k][p], vkq = v[k][q];
                    v[k][p] = c * vkp - s * vkq, v[k][q] = s * vkp + c * vkq;
                }
            }
    }
    int best = 0;
    for (int k = 1; k < 3; ++k)
        if (m[k][k] < m[best][best])
            best = k;
    return {v[0][best], v[1][best], v[2][best]};
}

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

MeasureFeature surface_feature(const TriangleMesh& source, std::size_t triangle) {
    const IndexedMesh mesh = weld_exact(source);
    const std::size_t n = mesh.triangles.size();
    if (triangle >= n)
        return plane_feature(source, triangle);
    std::vector<Point3> normal(n);
    for (std::size_t t = 0; t < n; ++t) {
        const auto& tri = mesh.triangles[t];
        const Point3 a = of(mesh.vertices[tri[0]]), b = of(mesh.vertices[tri[1]]), c = of(mesh.vertices[tri[2]]);
        normal[t] = unit(cross3(sub(b, a), sub(c, a)));
    }
    std::map<std::pair<std::uint32_t, std::uint32_t>, std::vector<std::size_t>> edges;
    for (std::size_t t = 0; t < n; ++t)
        for (int k = 0; k < 3; ++k) {
            std::uint32_t a = mesh.triangles[t][std::size_t(k)], b = mesh.triangles[t][std::size_t((k + 1) % 3)];
            edges[{std::min(a, b), std::max(a, b)}].push_back(t);
        }
    // The smooth stretch around the triangle.
    const double smooth = std::cos(25.0 * kDegree);
    std::vector<char> in(n, 0);
    std::vector<std::size_t> region{triangle}, todo{triangle};
    in[triangle] = 1;
    while (!todo.empty() && region.size() < 200000) {
        const std::size_t t = todo.back();
        todo.pop_back();
        for (int k = 0; k < 3; ++k) {
            std::uint32_t a = mesh.triangles[t][std::size_t(k)], b = mesh.triangles[t][std::size_t((k + 1) % 3)];
            for (const std::size_t u : edges[{std::min(a, b), std::max(a, b)}])
                if (!in[u] && dot(normal[u], normal[t]) >= smooth) {
                    in[u] = 1;
                    region.push_back(u);
                    todo.push_back(u);
                }
        }
    }
    bool flat = true;
    for (const std::size_t t : region)
        flat &= dot(normal[t], normal[triangle]) >= std::cos(1.0 * kDegree);
    if (flat || region.size() < 3)
        return plane_feature(source, triangle);
    // The axis: the direction the normals spread least along.
    Point3 mean{0, 0, 0};
    for (const std::size_t t : region)
        for (int k = 0; k < 3; ++k)
            mean[std::size_t(k)] += normal[t][std::size_t(k)] / double(region.size());
    double cov[3][3] = {};
    for (const std::size_t t : region) {
        const Point3 d = sub(normal[t], mean);
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                cov[i][j] += d[std::size_t(i)] * d[std::size_t(j)];
    }
    const Point3 axis = unit(smallest_eigenvector(cov));
    double lean = 0.0;
    for (const std::size_t t : region)
        lean += dot(normal[t], axis) / double(region.size());
    // The corners across the axis, and the circle through them (Kåsa).
    const Point3 u = unit(std::abs(axis[0]) < 0.9 ? cross3(axis, {1, 0, 0}) : cross3(axis, {0, 1, 0}));
    const Point3 v = cross3(axis, u);
    std::vector<char> used(mesh.vertices.size(), 0);
    std::vector<std::array<double, 3>> pts; // x, y across the axis, h along it
    for (const std::size_t t : region)
        for (const std::uint32_t i : mesh.triangles[t])
            if (!used[i]) {
                used[i] = 1;
                const Point3 p = of(mesh.vertices[i]);
                pts.push_back({dot(p, u), dot(p, v), dot(p, axis)});
            }
    double sxx = 0, sxy = 0, syy = 0, sx = 0, sy = 0, sxz = 0, syz = 0, sz = 0;
    for (const auto& p : pts) {
        const double z = p[0] * p[0] + p[1] * p[1];
        sxx += p[0] * p[0], sxy += p[0] * p[1], syy += p[1] * p[1], sx += p[0], sy += p[1];
        sxz += p[0] * z, syz += p[1] * z, sz += z;
    }
    const double N = double(pts.size());
    // Solve [sxx sxy sx; sxy syy sy; sx sy N] [D E F] = -[sxz syz sz].
    const double A[3][3] = {{sxx, sxy, sx}, {sxy, syy, sy}, {sx, sy, N}}, rhs[3] = {-sxz, -syz, -sz};
    const double det = A[0][0] * (A[1][1] * A[2][2] - A[1][2] * A[2][1]) - A[0][1] * (A[1][0] * A[2][2] - A[1][2] * A[2][0]) +
                       A[0][2] * (A[1][0] * A[2][1] - A[1][1] * A[2][0]);
    if (std::abs(det) < 1e-12)
        return plane_feature(source, triangle);
    const auto solve = [&](int col) {
        double M[3][3];
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                M[i][j] = j == col ? rhs[i] : A[i][j];
        return (M[0][0] * (M[1][1] * M[2][2] - M[1][2] * M[2][1]) - M[0][1] * (M[1][0] * M[2][2] - M[1][2] * M[2][0]) +
                M[0][2] * (M[1][0] * M[2][1] - M[1][1] * M[2][0])) /
               det;
    };
    const double cx = -0.5 * solve(0), cy = -0.5 * solve(1);
    // Each corner's distance from the axis against its height: a line.
    double hl = INFINITY, hh = -INFINITY, sh = 0, sr = 0, shh = 0, shr = 0;
    for (const auto& p : pts) {
        const double r = std::hypot(p[0] - cx, p[1] - cy);
        hl = std::min(hl, p[2]), hh = std::max(hh, p[2]);
        sh += p[2], sr += r, shh += p[2] * p[2], shr += p[2] * r;
    }
    const double var = shh - sh * sh / N;
    const double slope = var > 1e-12 ? (shr - sh * sr / N) / var : 0.0;
    const double r0 = sr / N - slope * sh / N;
    MeasureFeature f;
    f.normal = axis;
    const double hm = 0.5 * (hl + hh);
    f.point = {cx * u[0] + cy * v[0] + hm * axis[0], cx * u[1] + cy * v[1] + hm * axis[1], cx * u[2] + cy * v[2] + hm * axis[2]};
    const bool cone = std::abs(lean) > std::sin(2.0 * kDegree) && std::abs(slope) > std::tan(1.0 * kDegree);
    f.kind = cone ? MeasureFeature::Kind::Cone : MeasureFeature::Kind::Circle;
    f.diameter = 2.0 * std::max(0.0, r0 + slope * hl);
    f.diameter_end = cone ? 2.0 * std::max(0.0, r0 + slope * hh) : f.diameter;
    if (!cone)
        f.diameter = f.diameter_end = 2.0 * sr / N;
    f.angle = cone ? 2.0 * std::atan(std::abs(slope)) / kDegree : 0.0;
    // A hole's faces look at its axis.
    const auto& seed = mesh.triangles[triangle];
    Point3 c3{0, 0, 0};
    for (const std::uint32_t i : seed)
        for (int k = 0; k < 3; ++k)
            c3[std::size_t(k)] += of(mesh.vertices[i])[std::size_t(k)] / 3.0;
    const Point3 radial{dot(c3, u) - cx, dot(c3, v) - cy, 0.0};
    const Point3 seen{dot(normal[triangle], u), dot(normal[triangle], v), 0.0};
    f.hole = dot(radial, seen) < 0.0;
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
    // A round surface measures from the middle of its axis.
    const bool pa = a.kind == Kind::Plane, pb = b.kind == Kind::Plane;
    if (!pa && !pb) {
        const Point3 d = sub(a.point, b.point);
        m.distance = std::sqrt(dot(d, d));
    } else if (pa != pb) {
        const MeasureFeature& plane = pa ? a : b;
        const MeasureFeature& point = pa ? b : a;
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
