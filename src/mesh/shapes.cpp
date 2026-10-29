// Simple solids for the plate's Add menu. Own design; see PROVENANCE.md.

#include "mesh/shapes.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace tisma {
namespace {

Vec3f at(double x, double y, double z) {
  return {float(x), float(y), float(z)};
}

} // namespace

TriangleMesh make_box(double sx, double sy, double sz) {
  const double x = sx / 2.0, y = sy / 2.0;
  const Vec3f c[8] = {at(-x, -y, 0), at(x, -y, 0),   at(x, y, 0),
                      at(-x, y, 0),  at(-x, -y, sz), at(x, -y, sz),
                      at(x, y, sz),  at(-x, y, sz)};
  // Two triangles per face, counter-clockwise seen from outside.
  static constexpr int faces[12][3] = {
      {0, 2, 1}, {0, 3, 2}, {4, 5, 6}, {4, 6, 7}, {0, 1, 5}, {0, 5, 4},
      {1, 2, 6}, {1, 6, 5}, {2, 3, 7}, {2, 7, 6}, {3, 0, 4}, {3, 4, 7}};
  TriangleMesh m;
  m.reserve(12);
  for (const auto &f : faces)
    m.add_triangle(c[f[0]], c[f[1]], c[f[2]]);
  return m;
}

TriangleMesh make_cylinder(double r, double h, int sides) {
  sides = std::max(sides, 3);
  TriangleMesh m;
  m.reserve(std::size_t(4 * sides));
  const Vec3f bottom = at(0, 0, 0), top = at(0, 0, h);
  for (int i = 0; i < sides; ++i) {
    const double a0 = 2.0 * std::numbers::pi * i / sides,
                 a1 = 2.0 * std::numbers::pi * (i + 1) / sides;
    const Vec3f b0 = at(r * std::cos(a0), r * std::sin(a0), 0),
                b1 = at(r * std::cos(a1), r * std::sin(a1), 0);
    const Vec3f t0 = at(r * std::cos(a0), r * std::sin(a0), h),
                t1 = at(r * std::cos(a1), r * std::sin(a1), h);
    m.add_triangle(bottom, b1, b0);
    m.add_triangle(top, t0, t1);
    m.add_triangle(b0, b1, t1);
    m.add_triangle(b0, t1, t0);
  }
  return m;
}

TriangleMesh make_sphere(double r, int rings, int segments) {
  rings = std::max(rings, 2);
  segments = std::max(segments, 3);
  // A point at latitude ring k (0 = bottom pole, rings = top pole) and
  // longitude j, the sphere's centre at height r.
  const auto point = [&](int k, int j) {
    const double phi = std::numbers::pi * k / rings - std::numbers::pi / 2.0;
    const double theta = 2.0 * std::numbers::pi * j / segments;
    const double c = (k == 0 || k == rings) ? 0.0 : std::cos(phi);
    return at(r * c * std::cos(theta), r * c * std::sin(theta),
              r + r * std::sin(phi));
  };
  TriangleMesh m;
  m.reserve(std::size_t(2 * segments * (rings - 1)));
  for (int k = 0; k < rings; ++k)
    for (int j = 0; j < segments; ++j) {
      const Vec3f a = point(k, j), b = point(k, j + 1), c = point(k + 1, j + 1),
                  d = point(k + 1, j);
      if (k > 0) // the bottom band's lower edge is the pole: one triangle
        m.add_triangle(a, b, c);
      if (k + 1 < rings) // and the top band's upper edge
        m.add_triangle(a, c, d);
    }
  return m;
}

} // namespace tisma
