#include "config/settings.hpp"
#include "io/stl.hpp"
#include "mesh/indexed_mesh.hpp"
#include "slice/layers.hpp"
#include "slice/pipeline.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <numbers>
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

// A prism of n sides around the z axis, its corners r from it, from z0 to
// z1: each side split by a diagonal into two triangles, as a cylinder comes
// from CAD, and fans closing both ends. Without side `missing` when it is one.
IndexedMesh prism(int n, float r, float z0, float z1, int missing = -1) {
    const auto at = [&](int k, float z) {
        const double a = 2.0 * std::numbers::pi * double(k % n) / double(n);
        return Vec3f{float(r * std::cos(a)), float(r * std::sin(a)), z};
    };
    TriangleMesh m;
    for (int k = 0; k < n; ++k) {
        const Vec3f a0 = at(k, z0), b0 = at(k + 1, z0), a1 = at(k, z1), b1 = at(k + 1, z1);
        m.add_triangle({0, 0, z0}, b0, a0);
        m.add_triangle({0, 0, z1}, a1, b1);
        if (k == missing)
            continue;
        m.add_triangle(a0, b0, b1);
        m.add_triangle(a0, b1, a1);
    }
    return weld_exact(m);
}

bool same_paths(const std::vector<Toolpath>& a, const std::vector<Toolpath>& b) {
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].points.size() != b[i].points.size() || a[i].closed != b[i].closed || a[i].index != b[i].index)
            return false;
        for (std::size_t k = 0; k < a[i].points.size(); ++k)
            if (a[i].points[k].p.x != b[i].points[k].p.x || a[i].points[k].p.y != b[i].points[k].p.y ||
                a[i].points[k].width != b[i].points[k].width)
                return false;
    }
    return true;
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

TEST_CASE("same_crossings: no vertex from one level up to the next", "[layers]") {
    // A vertex on a level is above it (value >= iso), so it changes sides
    // between isos[l - 1] <= v and v < isos[l].
    const std::vector<float> values = {0.0f, 5.0f, 10.0f};
    CHECK(same_crossings(values, std::vector<float>{1, 2, 4, 5, 6, 9}) == std::vector<char>{0, 1, 1, 1, 0, 1});
    CHECK(same_crossings(values, std::vector<float>{0, 0.5f}) == std::vector<char>{0, 0});
    CHECK(same_crossings(values, std::vector<float>{2, 1}) == std::vector<char>{0, 0}); // not ascending
    CHECK(same_crossings(values, std::vector<float>{}).empty());
}

TEST_CASE("levels with the same crossings are moved along the edges, to the bit", "[layers]") {
    for (const IndexedMesh& m : {pyramid(), prism(12, 5.0f, 0.0f, 10.0f)}) {
        const std::vector<float> f = height_field(m);
        std::vector<float> z;
        for (int i = 0; i < 40; ++i)
            z.push_back(0.125f + 0.25f * float(i));
        const std::vector<char> same = same_crossings(f, z);
        CHECK(std::count(same.begin(), same.end(), char(1)) == 39); // no vertex between 0 and 10
        const auto all = slice_levels(m, f, z);
        for (std::size_t l = 0; l < z.size(); ++l) {
            INFO("level " << l);
            const auto alone = slice_levels(m, f, std::vector<float>{z[l]});
            REQUIRE(alone[0].size() == all[l].size());
            for (std::size_t c = 0; c < all[l].size(); ++c) {
                CHECK(alone[0][c].closed == all[l][c].closed);
                CHECK(alone[0][c].points == all[l][c].points);
            }
        }
    }
}

TEST_CASE("a prism's levels have one section: the slicer leaves them to the caller", "[layers]") {
    const std::vector<float> z = {0.1f, 1.0f, 2.0f, 5.0f, 9.9f};
    // Its diagonals cross every level at another point, but on the sides.
    const IndexedMesh p = prism(32, 30.0f, 0.0f, 10.0f);
    const std::vector<char> one = {0, 1, 1, 1, 1};
    CHECK(same_sections(p, height_field(p), z) == one);
    std::vector<char> marked;
    const auto cut = slice_levels(p, height_field(p), z, 0.0, &marked);
    CHECK(marked == one);
    REQUIRE(cut[0].size() == 1);
    CHECK(cut[0][0].closed);
    CHECK(cut[0][0].points.size() == 64); // each side's corner and its diagonal
    for (std::size_t l = 1; l < z.size(); ++l)
        CHECK(cut[l].empty());
    // Not asked: every level comes back.
    for (const auto& level : slice_levels(p, height_field(p), z))
        CHECK(level.size() == 1);
    // A pyramid crosses the same edges, but its section shrinks.
    const IndexedMesh pyr = pyramid();
    CHECK(same_sections(pyr, height_field(pyr), z) == std::vector<char>(5, 0));
    slice_levels(pyr, height_field(pyr), z, 0.0, &marked);
    CHECK(marked == std::vector<char>(5, 0));
    // Without a side, the contours stay open: every level is cut.
    const IndexedMesh open = prism(32, 30.0f, 0.0f, 10.0f, 3);
    const auto open_cut = slice_levels(open, height_field(open), z, 0.0, &marked);
    CHECK(marked == std::vector<char>(5, 0));
    for (const auto& level : open_cut) {
        REQUIRE(level.size() == 1);
        CHECK_FALSE(level[0].closed);
    }
}

TEST_CASE("slicing a prism works out its first layers and repeats them", "[layers]") {
    const IndexedMesh p = prism(256, 20.0f, 0.0f, 6.0f);
    Settings s; // 0.2 mm layers, 3 walls, 3 top and bottom layers, rectilinear infill
    const SlicedModel m = slice_model(p, s);
    REQUIRE(m.layers.size() == 30);
    for (std::size_t l = 2; l < m.layers.size(); ++l) {
        INFO("layer " << l);
        CHECK(m.regions[l] == m.regions[1]);
        CHECK(m.walls[l].infill == m.walls[1].infill);
        CHECK(same_paths(m.walls[l].toolpaths, m.walls[1].toolpaths));
    }
    // The sparse infill turns every other layer: layers two apart are alike.
    for (std::size_t l = 5; l + 5 < m.layers.size(); ++l) {
        INFO("layer " << l);
        CHECK(same_paths(m.fills[l].sparse, m.fills[l - 2].sparse));
        CHECK(m.fills[l].top.empty());
        CHECK(m.fills[l].solid.empty());
    }
    // The first layer shrinks alone (elephant foot); the second does not.
    s.first_layer_shrink = 0.2;
    const SlicedModel shrunk = slice_model(p, s);
    CHECK(area_mm2(shrunk.regions[0]) < area_mm2(shrunk.regions[1]) - 1.0);
    CHECK(shrunk.regions[1] == m.regions[1]);
    // Fuzzy skin is drawn anew in every layer.
    s.first_layer_shrink = 0.0;
    s.fuzzy_mode = FuzzyMode::External;
    const SlicedModel fuzzy = slice_model(p, s);
    CHECK(fuzzy.regions[6] == fuzzy.regions[5]);
    CHECK_FALSE(same_paths(fuzzy.walls[6].toolpaths, fuzzy.walls[5].toolpaths));
}
