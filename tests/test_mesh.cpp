#include "mesh/mesh.hpp"

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
