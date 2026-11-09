#include "config/settings.hpp"
#include "geom/polyholes.hpp"
#include "mesh/indexed_mesh.hpp"
#include "mesh/shapes.hpp"
#include "slice/pipeline.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

using namespace tisma;
using Catch::Matchers::WithinAbs;

namespace {

// A clockwise circle (a hole) of `points` points.
Polygon hole(double x, double y, double r, int points = 64) {
    Polygon p;
    for (int k = points - 1; k >= 0; --k) {
        const double a = 2.0 * std::numbers::pi * k / points;
        p.push_back({to_units(x + r * std::cos(a)), to_units(y + r * std::sin(a))});
    }
    return p;
}

// A counter-clockwise rectangle from the origin.
Polygon rectangle(double w, double h) {
    const std::int64_t x = to_units(w), y = to_units(h), o = 0;
    return Polygon{{o, o}, {x, o}, {x, y}, {o, y}};
}

Polygons plate_with_holes() {
    Polygons region{rectangle(100.0, 60.0)};
    region.push_back(hole(10.0, 30.0, 1.5));
    region.push_back(hole(30.0, 30.0, 5.0));
    region.push_back(hole(70.0, 30.0, 15.0));
    return region;
}

// The region's holes (clockwise paths), by their centre's x.
std::vector<Polygon> holes_of(const Polygons& region) {
    std::vector<Polygon> out;
    for (const Polygon& p : region)
        if (Clipper2Lib::Area(p) < 0.0)
            out.push_back(p);
    std::sort(out.begin(), out.end(), [](const Polygon& a, const Polygon& b) { return a[0].x < b[0].x; });
    return out;
}

} // namespace

TEST_CASE("polyholes: sides about twice the diameter with 0.4, capped (orca_2_5.md, rule 5)", "[polyholes]") {
    CHECK(polyhole_sides(3.0, 0.4, 50) == 6);
    CHECK(polyhole_sides(10.0, 0.4, 50) == 20);
    CHECK(polyhole_sides(30.0, 0.4, 50) == 50);
    CHECK(polyhole_sides(10.0, 0.8, 50) == 10);
    CHECK(polyhole_sides(0.5, 0.4, 50) == 3);
}

TEST_CASE("polyholes: holes of 3, 10 and 30 mm become 6, 20 and 50 flat sides touching the circle (OR5)",
          "[polyholes]") {
    std::vector<Polygons> layers(3, plate_with_holes());
    const std::vector<char> changed = make_polyholes(layers, {});
    for (std::size_t l = 0; l < layers.size(); ++l) {
        CHECK(changed[l]);
        const std::vector<Polygon> holes = holes_of(layers[l]);
        REQUIRE(holes.size() == 3);
        const double radii[3] = {1.5, 5.0, 15.0}, xs[3] = {10.0, 30.0, 70.0};
        const std::size_t sides[3] = {6, 20, 50};
        for (int h = 0; h < 3; ++h) {
            CHECK(holes[std::size_t(h)].size() == sides[h]);
            // Each flat's middle lies on the circle.
            const Polygon& p = holes[std::size_t(h)];
            for (std::size_t i = 0; i < p.size(); ++i) {
                const auto& a = p[i];
                const auto& b = p[(i + 1) % p.size()];
                const double mx = to_mm((a.x + b.x) / 2), my = to_mm((a.y + b.y) / 2);
                CHECK_THAT(std::hypot(mx - xs[h], my - 30.0), WithinAbs(radii[h], 1e-3));
            }
        }
    }
    // Off: nothing changes.
    Settings s;
    CHECK_FALSE(s.polyholes);
}

TEST_CASE("polyholes: with twist the polygon turns each layer and repeats every five (OR5)", "[polyholes]") {
    std::vector<Polygons> layers(11, plate_with_holes());
    PolyholeOptions o;
    o.twist = true;
    make_polyholes(layers, o);
    const auto first_corner = [&](std::size_t l) { return holes_of(layers[l])[1]; };
    CHECK(first_corner(0) != first_corner(1));
    CHECK(first_corner(1) != first_corner(2));
    CHECK(first_corner(0) == first_corner(5));
    CHECK(first_corner(3) == first_corner(8));
    CHECK(first_corner(5) == first_corner(10));
}

TEST_CASE("polyholes: a hole on one layer only, a coarse hole and an oval stay as they are", "[polyholes]") {
    // Layers 0 and 2 plain; layer 1 has a round hole of its own: it lasts one
    // layer and is not on the first, so it stays.
    const Polygons plate{rectangle(50.0, 50.0)};
    Polygons with = plate;
    with.push_back(hole(25.0, 25.0, 5.0));
    std::vector<Polygons> layers{plate, with, plate};
    CHECK_FALSE(make_polyholes(layers, {})[1]);
    CHECK(layers[1] == with);
    // On the first layer alone it is turned.
    std::vector<Polygons> first{with, plate};
    CHECK(make_polyholes(first, {})[0]);
    // A hole of 8 points (not more than 8) stays.
    Polygons coarse = plate;
    coarse.push_back(hole(25.0, 25.0, 5.0, 8));
    std::vector<Polygons> c{coarse, coarse};
    CHECK_FALSE(make_polyholes(c, {})[0]);
    // An oval 10 by 6 mm strays more than 20 % of its radius from its circle.
    Polygon oval = hole(25.0, 25.0, 5.0);
    for (auto& p : oval)
        p.y = to_units(25.0 + (to_mm(p.y) - 25.0) * 0.6);
    Polygons ovals = plate;
    ovals.push_back(oval);
    std::vector<Polygons> o{ovals, ovals};
    CHECK_FALSE(make_polyholes(o, {})[0]);
    // A margin in mm wide enough takes it.
    PolyholeOptions wide;
    wide.margin = 3.0;
    wide.margin_percent = false;
    CHECK(make_polyholes(o, wide)[0]);
}

TEST_CASE("polyholes: a sliced tube's 10 mm hole prints as 20 flat sides (OR5)", "[polyholes]") {
    const IndexedMesh tube = weld_exact(make_tube(15.0, 5.0, 2.0, 128));
    Settings s;
    s.polyholes = true;
    const SlicedModel m = slice_model(tube, s);
    REQUIRE(m.layers.size() > 2);
    for (std::size_t l = 0; l < m.layers.size(); ++l) {
        INFO("layer " << l);
        const std::vector<Polygon> holes = holes_of(m.regions[l]);
        REQUIRE(holes.size() == 1);
        CHECK(holes[0].size() == 20);
    }
    s.polyholes = false;
    const SlicedModel plain = slice_model(tube, s);
    CHECK(holes_of(plain.regions[1])[0].size() > 20);
}
