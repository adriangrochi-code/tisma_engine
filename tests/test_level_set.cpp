#include "io/stl.hpp"
#include "mesh/indexed_mesh.hpp"
#include "slice/level_set.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <filesystem>
#include <map>

using namespace tisma;
using Catch::Matchers::WithinAbs;

namespace {

IndexedMesh cube() {
    return weld_exact(read_stl(std::filesystem::path(TISMA_TEST_DATA_DIR) / "cube_ascii.stl").mesh);
}

// Twice the signed area enclosed by the segments, projected on XY.
double twice_area_xy(const std::vector<LevelSegment>& segs) {
    double sum = 0.0;
    for (const LevelSegment& s : segs)
        sum += double(s.from.x) * s.to.y - double(s.to.x) * s.from.y;
    return sum;
}

// Every segment end must meet exactly one segment start, at the same point.
void check_closed(const std::vector<LevelSegment>& segs) {
    std::map<EdgeKey, const LevelSegment*> by_start;
    for (const LevelSegment& s : segs)
        REQUIRE(by_start.emplace(s.edge_from, &s).second);
    for (const LevelSegment& s : segs) {
        const auto it = by_start.find(s.edge_to);
        REQUIRE(it != by_start.end());
        CHECK(it->second->from == s.to); // bitwise equal, not just close
    }
}

} // namespace

TEST_CASE("planar level set of the cube is a closed counter-clockwise square", "[slice]") {
    const IndexedMesh m = cube();
    const std::vector<float> z = height_field(m);
    std::vector<LevelSegment> segs;
    level_set_segments(m, z, 2.5f, segs);
    CHECK(segs.size() == 8); // two triangles on each side face
    check_closed(segs);
    CHECK_THAT(twice_area_xy(segs), WithinAbs(200.0, 1e-9));
    for (const LevelSegment& s : segs) {
        CHECK(s.from.z == 2.5f);
        CHECK(s.to.z == 2.5f);
    }
}

TEST_CASE("levels through vertices follow the value >= iso rule", "[slice]") {
    const IndexedMesh m = cube();
    const std::vector<float> z = height_field(m);
    std::vector<LevelSegment> segs;
    level_set_segments(m, z, 0.0f, segs); // every vertex counts as above
    CHECK(segs.empty());
    level_set_segments(m, z, 10.0f, segs); // top vertices above, bottom ones below
    check_closed(segs);
    CHECK_THAT(twice_area_xy(segs), WithinAbs(200.0, 1e-9));
    segs.clear();
    level_set_segments(m, z, 10.5f, segs);
    CHECK(segs.empty());
}

TEST_CASE("reversed winding reverses the contour", "[slice]") {
    IndexedMesh m = cube();
    for (auto& t : m.triangles)
        std::swap(t[1], t[2]);
    std::vector<LevelSegment> segs;
    level_set_segments(m, height_field(m), 5.0f, segs);
    check_closed(segs);
    CHECK_THAT(twice_area_xy(segs), WithinAbs(-200.0, 1e-9));
}

TEST_CASE("a non-planar field gives a closed contour too", "[slice]") {
    // Field x + z on the cube: the level 7 is a closed loop around the cube
    // that is not planar in z.
    const IndexedMesh m = cube();
    std::vector<float> f(m.vertices.size());
    for (std::size_t i = 0; i < f.size(); ++i)
        f[i] = m.vertices[i].x + m.vertices[i].z;
    std::vector<LevelSegment> segs;
    level_set_segments(m, f, 7.0f, segs);
    REQUIRE_FALSE(segs.empty());
    check_closed(segs);
    for (const LevelSegment& s : segs)
        CHECK_THAT(double(s.from.x) + s.from.z, WithinAbs(7.0, 1e-5));
}
