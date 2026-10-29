#include "config/settings.hpp"
#include "io/stl.hpp"
#include "mesh/indexed_mesh.hpp"
#include "slice/pipeline.hpp"
#include "surfaces/surfaces.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <filesystem>
#include <numbers>

using namespace tisma;
using Catch::Matchers::WithinAbs;

namespace {

// The band of 3 walls 0.4 mm wide on 0.2 mm layers (perimeters.md, rule 1):
// half an overlap, then three spacings of 0.4 less the overlap.
const double kOverlap = 0.2 * (1.0 - std::numbers::pi / 4.0);
const double kBand = kOverlap / 2.0 + 3.0 * (0.4 - kOverlap);

// A closed box from (x0, y0, z0) to (x1, y1, z1), faces wound outwards.
void add_box(TriangleMesh& m, float x0, float y0, float z0, float x1, float y1, float z1) {
    const Vec3f v[8] = {{x0, y0, z0}, {x1, y0, z0}, {x1, y1, z0}, {x0, y1, z0},
                        {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}};
    const int faces[6][4] = {{0, 3, 2, 1}, {4, 5, 6, 7}, {0, 1, 5, 4}, {1, 2, 6, 5}, {2, 3, 7, 6}, {3, 0, 4, 7}};
    for (const auto& f : faces) {
        m.add_triangle(v[f[0]], v[f[1]], v[f[2]]);
        m.add_triangle(v[f[0]], v[f[2]], v[f[3]]);
    }
}

Polygons square(double x0, double y0, double side) {
    return {{{to_units(x0), to_units(y0)},
             {to_units(x0 + side), to_units(y0)},
             {to_units(x0 + side), to_units(y0 + side)},
             {to_units(x0), to_units(y0 + side)}}};
}

} // namespace

TEST_CASE("classify_surfaces on a column of equal layers", "[surfaces]") {
    // Ten layers of a 10 mm square; the infill is the whole square.
    const std::vector<Polygons> regions(10, square(0, 0, 10));
    const std::vector<LayerSurfaces> s = classify_surfaces(regions, regions, 3, 2);
    REQUIRE(s.size() == 10);
    for (std::size_t i = 0; i < 10; ++i) {
        INFO("layer " << i);
        const bool top = i >= 7, bottom = i < 2;
        CHECK_THAT(area_mm2(s[i].top), WithinAbs(top ? 100.0 : 0.0, 1e-9));
        CHECK_THAT(area_mm2(s[i].bottom), WithinAbs(bottom ? 100.0 : 0.0, 1e-9));
        CHECK_THAT(area_mm2(s[i].sparse), WithinAbs(top || bottom ? 0.0 : 100.0, 1e-9));
    }
    // No solid layers asked: everything is sparse.
    for (const LayerSurfaces& l : classify_surfaces(regions, regions, 0, 0))
        CHECK_THAT(area_mm2(l.sparse), WithinAbs(100.0, 1e-9));
}

TEST_CASE("classify_surfaces on a step: the repeated layers end where the regions change", "[surfaces]") {
    // A 10 mm square for ten layers, then a 6 mm one in its corner for ten.
    std::vector<Polygons> regions(10, square(0, 0, 10));
    regions.resize(20, square(0, 0, 6));
    const std::vector<LayerSurfaces> s = classify_surfaces(regions, regions, 3, 2);
    for (std::size_t i = 0; i < 20; ++i) {
        INFO("layer " << i);
        const double area = i < 10 ? 100.0 : 36.0;
        const double top = i >= 17 ? 36.0 : (i >= 7 && i < 10 ? 64.0 : 0.0);
        const double bottom = i < 2 ? 100.0 : 0.0; // the small square stands on the big one
        CHECK_THAT(area_mm2(s[i].top), WithinAbs(top, 1e-9));
        CHECK_THAT(area_mm2(s[i].bottom), WithinAbs(bottom, 1e-9));
        CHECK_THAT(area_mm2(s[i].sparse), WithinAbs(area - top - bottom, 1e-9));
    }
}

TEST_CASE("the cube gets 3 solid layers at the bottom and 3 at the top", "[surfaces]") {
    const IndexedMesh m = weld_exact(read_stl(std::filesystem::path(TISMA_TEST_DATA_DIR) / "cube_ascii.stl").mesh);
    Settings settings; // 3 walls, 3 top and 3 bottom layers
    settings.wall_width = settings.outer_wall_width = 0.4; // 0.4 mm beads (as measured), the outer wall as the others
    settings.infill_solid_below_area = 0.0; // the 10 mm cube's 57.8 mm2 of infill would be all solid (infill.solid_below_area)
    const SlicedModel sliced = slice_model(m, settings);
    REQUIRE(sliced.surfaces.size() == 50);
    const double infill = (10.0 - 2.0 * kBand) * (10.0 - 2.0 * kBand);
    for (std::size_t i = 0; i < 50; ++i) {
        INFO("layer " << i);
        const LayerSurfaces& s = sliced.surfaces[i];
        CHECK_THAT(area_mm2(s.top), WithinAbs(i >= 47 ? infill : 0.0, 1e-5));
        CHECK_THAT(area_mm2(s.bottom), WithinAbs(i < 3 ? infill : 0.0, 1e-5));
        CHECK_THAT(area_mm2(s.sparse), WithinAbs(i >= 3 && i < 47 ? infill : 0.0, 1e-5));
        // Only the last layer is the top surface; the two under it are solid
        // infill (surfaces_infill.md, rule 2 and I1).
        CHECK_THAT(area_mm2(s.top_under), WithinAbs(i == 47 || i == 48 ? infill : 0.0, 1e-5));
    }
    CHECK(sliced.fills[47].top.empty());
    CHECK(sliced.fills[48].top.empty());
    CHECK_FALSE(sliced.fills[48].solid.empty());
    CHECK_FALSE(sliced.fills[49].top.empty());
    CHECK(sliced.fills[49].solid.empty());
}

TEST_CASE("a step: the lower box's top around the upper box is solid, the upper box's bottom is not", "[surfaces]") {
    // A 20 mm box 5 mm high with a 10 mm box on top, centred: layers 0-24 and 25-49.
    TriangleMesh mesh;
    add_box(mesh, 0, 0, 0, 20, 20, 5);
    add_box(mesh, 5, 5, 5, 15, 15, 10);
    Settings s;
    s.wall_width = s.outer_wall_width = 0.4; // 0.4 mm beads (as measured), the outer wall as the others
    s.infill_solid_below_area = 0.0; // the upper box's 57.8 mm2 of infill would be all solid
    const SlicedModel sliced = slice_model(weld_exact(mesh), s);
    REQUIRE(sliced.surfaces.size() == 50);
    const double lower_infill = (20.0 - 2.0 * kBand) * (20.0 - 2.0 * kBand),
                 upper_infill = (10.0 - 2.0 * kBand) * (10.0 - 2.0 * kBand);
    for (std::size_t i : {22u, 23u, 24u}) {
        INFO("layer " << i);
        // Solid where the upper box does not cover it: the infill minus the 10 mm square.
        CHECK_THAT(area_mm2(sliced.surfaces[i].top), WithinAbs(lower_infill - 100.0, 1e-5));
        CHECK_THAT(area_mm2(sliced.surfaces[i].sparse), WithinAbs(100.0, 1e-6));
        // Layer 24's ring is under the air: the top surface; 22 and 23 are under it.
        CHECK_THAT(area_mm2(sliced.surfaces[i].top_under), WithinAbs(i < 24 ? lower_infill - 100.0 : 0.0, 1e-5));
    }
    CHECK_THAT(area_mm2(sliced.surfaces[21].top), WithinAbs(0.0, 1e-6));
    for (std::size_t i : {25u, 26u, 27u}) {
        INFO("layer " << i);
        // The upper box stands on the lower one: no bottom surface.
        CHECK_THAT(area_mm2(sliced.surfaces[i].bottom), WithinAbs(0.0, 1e-6));
        CHECK_THAT(area_mm2(sliced.surfaces[i].sparse), WithinAbs(upper_infill, 1e-5));
    }
}

TEST_CASE("solid parts narrower than a bead grow by a bead into the infill", "[surfaces]") {
    // A gentle slope: layer i is a square of side 20 - i (0.5 mm in per side per
    // layer). With walls 1.2 mm wide, layer i's infill is a square of side
    // 17.6 - i and layer i + 3 covers a square of side 17 - i, so the top skin is
    // a ring 0.3 mm wide: too thin for a 0.4 mm bead.
    std::vector<Polygons> regions, infill;
    for (int i = 0; i < 10; ++i) {
        regions.push_back(square(i * 0.5, i * 0.5, 20.0 - i));
        infill.push_back(square(i * 0.5 + 1.2, i * 0.5 + 1.2, 17.6 - i));
    }
    const std::vector<LayerSurfaces> thin = classify_surfaces(regions, infill, 3, 0);
    const std::vector<LayerSurfaces> wide = classify_surfaces(regions, infill, 3, 0, 0.4);
    for (int i = 0; i <= 6; ++i) {
        INFO("layer " << i);
        const double outer = 17.6 - i;
        CHECK_THAT(area_mm2(thin[std::size_t(i)].top), WithinAbs(outer * outer - (outer - 0.6) * (outer - 0.6), 1e-6));
        // Grown inwards by 0.4 mm: a ring 0.7 mm wide, taken from the sparse part.
        CHECK_THAT(area_mm2(wide[std::size_t(i)].top), WithinAbs(outer * outer - (outer - 1.4) * (outer - 1.4), 1e-6));
        CHECK_THAT(area_mm2(wide[std::size_t(i)].top) + area_mm2(wide[std::size_t(i)].sparse), WithinAbs(outer * outer, 1e-6));
    }
    // Parts already a bead wide do not change: the last 3 layers are all top either way.
    for (int i = 7; i < 10; ++i)
        CHECK_THAT(area_mm2(wide[std::size_t(i)].top), WithinAbs(area_mm2(thin[std::size_t(i)].top), 1e-9));
}

TEST_CASE("bridges: a plate on two pillars bridges the gap, with lines from pillar to pillar", "[surfaces]") {
    // Pillars 5 x 10 mm at x = 0..5 and x = 15..20, 5 mm high (layers 0-24),
    // and a 20 x 10 mm plate on top from z = 5 to 7 (layers 25-34).
    TriangleMesh mesh;
    add_box(mesh, 0, 0, 0, 5, 10, 5);
    add_box(mesh, 15, 0, 0, 20, 10, 5);
    add_box(mesh, 0, 0, 5, 20, 10, 7);
    Settings s;
    s.wall_width = s.outer_wall_width = 0.4; // 0.4 mm beads (as measured), the outer wall as the others
    const SlicedModel sliced = slice_model(weld_exact(mesh), s);
    REQUIRE(sliced.surfaces.size() == 35);
    // The plate's first layer: its infill (kBand in) between the pillars is a bridge.
    const LayerSurfaces& first = sliced.surfaces[25];
    CHECK_THAT(area_mm2(first.bridge), WithinAbs(10.0 * (10.0 - 2.0 * kBand), 1e-5));
    CHECK(first.bottom.empty());
    CHECK(first.bridge_angle == 0.0); // across the gap, from pillar to pillar
    REQUIRE_FALSE(sliced.fills[25].bridge.empty());
    for (const Toolpath& t : sliced.fills[25].bridge) {
        CHECK_FALSE(t.closed);
        CHECK_THAT(t.points.front().p.y, WithinAbs(t.points.back().p.y, 1e-6));
        CHECK(t.points.front().p.x < 5.3);
        CHECK(t.points.back().p.x > 14.7);
    }
    // The next layer rests on the bridge: still a bottom surface, no longer a bridge.
    CHECK(sliced.surfaces[26].bridge.empty());
    CHECK_THAT(area_mm2(sliced.surfaces[26].bottom), WithinAbs(10.0 * (10.0 - 2.0 * kBand), 1e-5));
    // Nothing else in the model is a bridge: not the first layer on the bed, not the pillars.
    for (std::size_t i = 0; i < sliced.surfaces.size(); ++i)
        if (i != 25)
            CHECK(sliced.surfaces[i].bridge.empty());
}

TEST_CASE("bridges: with no infill, the same bridge, and none from a narrowing hollow part", "[surfaces]") {
    // The plate on two pillars, with the sparse infill not printed.
    TriangleMesh plate;
    add_box(plate, 0, 0, 0, 5, 10, 5);
    add_box(plate, 15, 0, 0, 20, 10, 5);
    add_box(plate, 0, 0, 5, 20, 10, 7);
    Settings s;
    s.wall_width = s.outer_wall_width = 0.4; // 0.4 mm beads (as measured), the outer wall as the others
    s.infill_density = 0.0;
    const SlicedModel sliced = slice_model(weld_exact(plate), s);
    REQUIRE(sliced.surfaces.size() == 35);
    CHECK_THAT(area_mm2(sliced.surfaces[25].bridge), WithinAbs(10.0 * (10.0 - 2.0 * kBand), 1e-5));
    for (std::size_t i = 0; i < sliced.surfaces.size(); ++i)
        if (i != 25)
            CHECK(sliced.surfaces[i].bridge.empty());
    // A hollow star: its bottom layers rest on the first one, solid; none is
    // a bridge, though the sparse infill above them is not printed.
    TriangleMesh star;
    const int points = 7;
    const auto corner = [&](int i, float z) {
        const double r = (i % 2 == 0 ? 45.0 : 20.0) * (1.0 - 0.5 * double(z) / 160.0); // half in 160 mm
        const double a = std::numbers::pi * double(i % (2 * points)) / double(points);
        return Vec3f{float(r * std::cos(a)), float(r * std::sin(a)), z};
    };
    for (int i = 0; i < 2 * points; ++i) {
        star.add_triangle(corner(i, 0), corner(i + 1, 0), corner(i + 1, 20));
        star.add_triangle(corner(i, 0), corner(i + 1, 20), corner(i, 20));
        star.add_triangle({0, 0, 0}, corner(i + 1, 0), corner(i, 0));
        star.add_triangle({0, 0, 20}, corner(i, 20), corner(i + 1, 20));
    }
    const SlicedModel hollow = slice_model(weld_exact(star), s);
    REQUIRE(hollow.surfaces.size() == 100);
    for (std::size_t i = 0; i < 3; ++i)
        CHECK_FALSE(hollow.surfaces[i].bottom.empty());
    for (const LayerSurfaces& l : hollow.surfaces)
        CHECK(l.bridge.empty());
}

TEST_CASE("bridges: none in the cube", "[surfaces]") {
    const IndexedMesh m = weld_exact(read_stl(std::filesystem::path(TISMA_TEST_DATA_DIR) / "cube_ascii.stl").mesh);
    const SlicedModel sliced = slice_model(m, Settings{});
    for (const LayerSurfaces& s : sliced.surfaces)
        CHECK(s.bridge.empty());
    for (const LayerFill& f : sliced.fills)
        CHECK(f.bridge.empty());
}
