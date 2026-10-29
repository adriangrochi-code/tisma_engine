#include "mesh/mesh.hpp"
#include "mesh/shapes.hpp"

#include <cmath>
#include <numbers>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace tisma;
using Catch::Matchers::WithinAbs;

TEST_CASE("empty mesh has an empty bounding box and no volume", "[mesh]") {
    TriangleMesh m;
    CHECK(m.bounding_box().empty);
    CHECK(m.signed_volume() == 0.0);
}

TEST_CASE("tetrahedron volume and bounding box", "[mesh]") {
    // Unit right tetrahedron, outward winding: volume 1/6.
    const Vec3f o{0, 0, 0}, x{1, 0, 0}, y{0, 1, 0}, z{0, 0, 1};
    TriangleMesh m;
    m.add_triangle(o, y, x);
    m.add_triangle(o, x, z);
    m.add_triangle(o, z, y);
    m.add_triangle(x, y, z);
    CHECK(m.triangle_count() == 4);
    CHECK_THAT(m.signed_volume(), WithinAbs(1.0 / 6.0, 1e-12));

    const BoundingBox box = m.bounding_box();
    CHECK_FALSE(box.empty);
    CHECK(box.min == o);
    CHECK(box.max == Vec3f{1, 1, 1});
}

TEST_CASE("the Add menu's shapes are closed, wound outwards and stand on the bed", "[mesh]") {
    const auto stands = [](const TriangleMesh& m, double sx, double sy, double sz) {
        const BoundingBox box = m.bounding_box();
        CHECK_THAT(box.min.z, WithinAbs(0.0, 1e-5));
        CHECK_THAT(box.max.z, WithinAbs(sz, 1e-4));
        CHECK_THAT(box.max.x - box.min.x, WithinAbs(sx, 1e-3));
        CHECK_THAT(box.max.y - box.min.y, WithinAbs(sy, 1e-3));
        CHECK_THAT(box.min.x + box.max.x, WithinAbs(0.0, 1e-4));
    };
    const TriangleMesh box = make_box(20, 10, 5);
    CHECK(box.triangle_count() == 12);
    CHECK_THAT(box.signed_volume(), WithinAbs(1000.0, 1e-3));
    stands(box, 20, 10, 5);

    // A polygon of n sides inscribed in the circle: (n/2) r^2 sin(2 pi/n) h.
    const TriangleMesh cylinder = make_cylinder(10, 20, 64);
    CHECK(cylinder.triangle_count() == 256);
    CHECK_THAT(cylinder.signed_volume(), WithinAbs(32.0 * 100.0 * std::sin(2.0 * std::numbers::pi / 64) * 20.0, 0.05));
    stands(cylinder, 20, 20, 20);

    const TriangleMesh sphere = make_sphere(10, 32, 64);
    CHECK(sphere.triangle_count() == 2 * 64 * 31);
    const double ball = 4.0 / 3.0 * std::numbers::pi * 1000.0;
    CHECK(sphere.signed_volume() > 0.98 * ball);
    CHECK(sphere.signed_volume() < ball);
    stands(sphere, 20, 20, 20);
}
