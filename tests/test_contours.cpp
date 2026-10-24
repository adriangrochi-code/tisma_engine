#include "io/stl.hpp"
#include "mesh/indexed_mesh.hpp"
#include "slice/contours.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <filesystem>

using namespace tisma;
using Catch::Matchers::WithinAbs;

namespace {

TriangleMesh cube_soup(float dx = 0.0f) {
    TriangleMesh m = read_stl(std::filesystem::path(TISMA_TEST_DATA_DIR) / "cube_ascii.stl").mesh;
    if (dx == 0.0f)
        return m;
    TriangleMesh moved;
    for (std::size_t t = 0; t < m.triangle_count(); ++t) {
        Vec3f v[3];
        for (int k = 0; k < 3; ++k)
            v[k] = {m.corner(t, k).x + dx, m.corner(t, k).y, m.corner(t, k).z};
        moved.add_triangle(v[0], v[1], v[2]);
    }
    return moved;
}

std::vector<Contour> slice_at(const IndexedMesh& m, float z) {
    std::vector<LevelSegment> segs;
    level_set_segments(m, height_field(m), z, segs);
    return join_segments(segs);
}

double area_xy(const Contour& c) {
    double sum = 0.0;
    for (std::size_t i = 0; i < c.points.size(); ++i) {
        const Vec3f& a = c.points[i];
        const Vec3f& b = c.points[(i + 1) % c.points.size()];
        sum += double(a.x) * b.y - double(b.x) * a.y;
    }
    return sum / 2.0;
}

} // namespace

TEST_CASE("the cube slices into one closed counter-clockwise contour", "[slice]") {
    const std::vector<Contour> cs = slice_at(weld_exact(cube_soup()), 5.0f);
    REQUIRE(cs.size() == 1);
    CHECK(cs[0].closed);
    CHECK(cs[0].points.size() == 8); // collinear points are kept
    CHECK_THAT(area_xy(cs[0]), WithinAbs(100.0, 1e-9));
}

TEST_CASE("two separate cubes give two contours", "[slice]") {
    TriangleMesh both = cube_soup();
    const TriangleMesh other = cube_soup(20.0f);
    for (std::size_t t = 0; t < other.triangle_count(); ++t)
        both.add_triangle(other.corner(t, 0), other.corner(t, 1), other.corner(t, 2));
    const std::vector<Contour> cs = slice_at(weld_exact(both), 5.0f);
    REQUIRE(cs.size() == 2);
    for (const Contour& c : cs) {
        CHECK(c.closed);
        CHECK_THAT(area_xy(c), WithinAbs(100.0, 1e-9));
    }
}

TEST_CASE("a mesh with a missing face gives one open contour", "[slice]") {
    // Drop the two triangles of the +x face (all corners at x = 10).
    const TriangleMesh full = cube_soup();
    TriangleMesh open;
    for (std::size_t t = 0; t < full.triangle_count(); ++t) {
        const bool on_plus_x = full.corner(t, 0).x == 10 && full.corner(t, 1).x == 10 && full.corner(t, 2).x == 10;
        if (!on_plus_x)
            open.add_triangle(full.corner(t, 0), full.corner(t, 1), full.corner(t, 2));
    }
    REQUIRE(open.triangle_count() == 10);
    const std::vector<Contour> cs = slice_at(weld_exact(open), 5.0f);
    REQUIRE(cs.size() == 1);
    CHECK_FALSE(cs[0].closed);
    CHECK(cs[0].points.size() == 7); // 6 segments on the 3 remaining side faces
    // It starts and ends on the edges of the missing face.
    CHECK(cs[0].points.front().x == 10.0f);
    CHECK(cs[0].points.back().x == 10.0f);
}

TEST_CASE("no segments, no contours", "[slice]") {
    CHECK(join_segments({}).empty());
}
