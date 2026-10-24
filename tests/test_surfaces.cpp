#include "config/settings.hpp"
#include "io/stl.hpp"
#include "mesh/indexed_mesh.hpp"
#include "slice/pipeline.hpp"
#include "surfaces/surfaces.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <filesystem>

using namespace tisma;
using Catch::Matchers::WithinAbs;

namespace {

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

TEST_CASE("the cube gets 3 solid layers at the bottom and 3 at the top", "[surfaces]") {
    const IndexedMesh m = weld_exact(read_stl(std::filesystem::path(TISMA_TEST_DATA_DIR) / "cube_ascii.stl").mesh);
    const SlicedModel sliced = slice_model(m, Settings{}); // 3 walls, 3 top and 3 bottom layers
    REQUIRE(sliced.surfaces.size() == 50);
    const double infill = 7.6 * 7.6;
    for (std::size_t i = 0; i < 50; ++i) {
        INFO("layer " << i);
        const LayerSurfaces& s = sliced.surfaces[i];
        CHECK_THAT(area_mm2(s.top), WithinAbs(i >= 47 ? infill : 0.0, 1e-6));
        CHECK_THAT(area_mm2(s.bottom), WithinAbs(i < 3 ? infill : 0.0, 1e-6));
        CHECK_THAT(area_mm2(s.sparse), WithinAbs(i >= 3 && i < 47 ? infill : 0.0, 1e-6));
    }
}

TEST_CASE("a step: the lower box's top around the upper box is solid, the upper box's bottom is not", "[surfaces]") {
    // A 20 mm box 5 mm high with a 10 mm box on top, centred: layers 0-24 and 25-49.
    TriangleMesh mesh;
    add_box(mesh, 0, 0, 0, 20, 20, 5);
    add_box(mesh, 5, 5, 5, 15, 15, 10);
    const SlicedModel sliced = slice_model(weld_exact(mesh), Settings{});
    REQUIRE(sliced.surfaces.size() == 50);
    const double lower_infill = 17.6 * 17.6, upper_infill = 7.6 * 7.6;
    for (std::size_t i : {22u, 23u, 24u}) {
        INFO("layer " << i);
        // Solid where the upper box does not cover it: the infill minus the 10 mm square.
        CHECK_THAT(area_mm2(sliced.surfaces[i].top), WithinAbs(lower_infill - 100.0, 1e-6));
        CHECK_THAT(area_mm2(sliced.surfaces[i].sparse), WithinAbs(100.0, 1e-6));
    }
    CHECK_THAT(area_mm2(sliced.surfaces[21].top), WithinAbs(0.0, 1e-6));
    for (std::size_t i : {25u, 26u, 27u}) {
        INFO("layer " << i);
        // The upper box stands on the lower one: no bottom surface.
        CHECK_THAT(area_mm2(sliced.surfaces[i].bottom), WithinAbs(0.0, 1e-6));
        CHECK_THAT(area_mm2(sliced.surfaces[i].sparse), WithinAbs(upper_infill, 1e-6));
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
    const SlicedModel sliced = slice_model(weld_exact(mesh), Settings{});
    REQUIRE(sliced.surfaces.size() == 35);
    // The plate's first layer: its infill (1.2 mm in) between the pillars is a bridge.
    const LayerSurfaces& first = sliced.surfaces[25];
    CHECK_THAT(area_mm2(first.bridge), WithinAbs(10.0 * 7.6, 1e-6));
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
    CHECK_THAT(area_mm2(sliced.surfaces[26].bottom), WithinAbs(10.0 * 7.6, 1e-6));
    // Nothing else in the model is a bridge: not the first layer on the bed, not the pillars.
    for (std::size_t i = 0; i < sliced.surfaces.size(); ++i)
        if (i != 25)
            CHECK(sliced.surfaces[i].bridge.empty());
}

TEST_CASE("bridges: none in the cube", "[surfaces]") {
    const IndexedMesh m = weld_exact(read_stl(std::filesystem::path(TISMA_TEST_DATA_DIR) / "cube_ascii.stl").mesh);
    const SlicedModel sliced = slice_model(m, Settings{});
    for (const LayerSurfaces& s : sliced.surfaces)
        CHECK(s.bridge.empty());
    for (const LayerFill& f : sliced.fills)
        CHECK(f.bridge.empty());
}
