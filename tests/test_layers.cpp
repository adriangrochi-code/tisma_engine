#include "io/stl.hpp"
#include "mesh/indexed_mesh.hpp"
#include "slice/layers.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <filesystem>
#include <stdexcept>

using namespace tisma;
using Catch::Matchers::WithinAbs;

namespace {

double area_xy(const Contour& c) {
    double sum = 0.0;
    for (std::size_t i = 0; i < c.points.size(); ++i) {
        const Vec3f& a = c.points[i];
        const Vec3f& b = c.points[(i + 1) % c.points.size()];
        sum += double(a.x) * b.y - double(b.x) * a.y;
    }
    return sum / 2.0;
}

// Square pyramid: base 10 x 10 at z = 0, apex (5, 5, 10). Its section at height
// z is a square of side 10 - z, so the area is known exactly.
IndexedMesh pyramid() {
    const Vec3f p[5] = {{0, 0, 0}, {10, 0, 0}, {10, 10, 0}, {0, 10, 0}, {5, 5, 10}};
    TriangleMesh m;
    m.add_triangle(p[0], p[2], p[1]);
    m.add_triangle(p[0], p[3], p[2]);
    m.add_triangle(p[0], p[1], p[4]);
    m.add_triangle(p[1], p[2], p[4]);
    m.add_triangle(p[2], p[3], p[4]);
    m.add_triangle(p[3], p[0], p[4]);
    return weld_exact(m);
}

std::vector<float> slice_heights(const std::vector<Layer>& layers) {
    std::vector<float> z;
    for (const Layer& l : layers)
        z.push_back(float(l.slice_z()));
    return z;
}

} // namespace

TEST_CASE("layer plan: first layer, then regular layers", "[layers]") {
    const std::vector<Layer> layers = plan_layers(0.0, 10.0, {0.3, 0.2});
    // 0.3 + 48 x 0.2 = 9.9. The 50th layer (9.9 to 10.1) has its middle right
    // on the model's top, 10.0: it is made (slicing.md, rule 3 and C4).
    REQUIRE(layers.size() == 50);
    CHECK(layers[0].bottom == 0.0);
    CHECK_THAT(layers[0].top, WithinAbs(0.3, 1e-12));
    CHECK_THAT(layers[1].bottom, WithinAbs(0.3, 1e-12));
    CHECK_THAT(layers[1].top, WithinAbs(0.5, 1e-12));
    for (std::size_t i = 1; i < layers.size(); ++i)
        CHECK_THAT(layers[i].bottom, WithinAbs(layers[i - 1].top, 1e-12));
    CHECK_THAT(layers.back().top, WithinAbs(10.1, 1e-12));
    // Just above the top, no more layers.
    CHECK(plan_layers(0.0, 9.999, {0.3, 0.2}).size() == 49);
}

TEST_CASE("layer plan starts at the model's lowest point", "[layers]") {
    const std::vector<Layer> layers = plan_layers(5.0, 6.0, {0.2, 0.2});
    REQUIRE(layers.size() == 5);
    CHECK(layers[0].bottom == 5.0);
}

TEST_CASE("layer plan rejects non-positive heights", "[layers]") {
    CHECK_THROWS_AS(plan_layers(0.0, 1.0, {0.0, 0.2}), std::invalid_argument);
    CHECK_THROWS_AS(plan_layers(0.0, 1.0, {0.2, -0.1}), std::invalid_argument);
}

TEST_CASE("every layer of the cube is one 100 mm2 square", "[layers]") {
    const IndexedMesh m = weld_exact(read_stl(std::filesystem::path(TISMA_TEST_DATA_DIR) / "cube_ascii.stl").mesh);
    const std::vector<float> z = slice_heights(plan_layers(0.0, 10.0, {0.3, 0.2}));
    const auto layers = slice_levels(m, height_field(m), z);
    REQUIRE(layers.size() == z.size());
    for (const auto& contours : layers) {
        REQUIRE(contours.size() == 1);
        CHECK(contours[0].closed);
        CHECK_THAT(area_xy(contours[0]), WithinAbs(100.0, 1e-9));
    }
}

TEST_CASE("pyramid layer areas match the exact section areas", "[layers]") {
    const IndexedMesh m = pyramid();
    const std::vector<float> z = slice_heights(plan_layers(0.0, 10.0, {0.2, 0.2}));
    const auto layers = slice_levels(m, height_field(m), z);
    REQUIRE(layers.size() == z.size());
    for (std::size_t l = 0; l < layers.size(); ++l) {
        REQUIRE(layers[l].size() == 1);
        const double side = 10.0 - double(z[l]);
        CHECK_THAT(area_xy(layers[l][0]), WithinAbs(side * side, 1e-3));
    }
}

TEST_CASE("bucketed slicing finds the same segments as visiting every triangle", "[layers]") {
    const IndexedMesh m = pyramid();
    const std::vector<float> f = height_field(m);
    const std::vector<float> z = {0.0f, 0.1f, 2.5f, 5.0f, 9.99f, 10.0f};
    const auto layers = slice_levels(m, f, z);
    for (std::size_t l = 0; l < z.size(); ++l) {
        std::vector<LevelSegment> all;
        level_set_segments(m, f, z[l], all);
        std::size_t points = 0;
        for (const Contour& c : layers[l])
            points += c.points.size();
        CHECK(points == all.size()); // closed contours have one point per segment
    }
}

TEST_CASE("slice_levels closes gaps when asked", "[layers]") {
    // Pyramid without its y = 0 side: each level has a gap as wide as that side.
    const Vec3f p[5] = {{0, 0, 0}, {10, 0, 0}, {10, 10, 0}, {0, 10, 0}, {5, 5, 10}};
    TriangleMesh soup;
    soup.add_triangle(p[0], p[2], p[1]);
    soup.add_triangle(p[0], p[3], p[2]);
    soup.add_triangle(p[1], p[2], p[4]);
    soup.add_triangle(p[2], p[3], p[4]);
    soup.add_triangle(p[3], p[0], p[4]); // the y = 0 side is missing
    const IndexedMesh m = weld_exact(soup);
    const std::vector<float> z = {2.0f, 5.0f};
    const auto raw = slice_levels(m, height_field(m), z);
    CHECK_FALSE(raw[0][0].closed);
    const auto closed = slice_levels(m, height_field(m), z, 10.0);
    for (std::size_t l = 0; l < z.size(); ++l) {
        REQUIRE(closed[l].size() == 1);
        CHECK(closed[l][0].closed);
        const double side = 10.0 - double(z[l]);
        CHECK_THAT(area_xy(closed[l][0]), WithinAbs(side * side, 1e-3));
    }
}
