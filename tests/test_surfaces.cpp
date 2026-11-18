#include "config/settings.hpp"
#include "io/stl.hpp"
#include "mesh/indexed_mesh.hpp"
#include "slice/pipeline.hpp"
#include "surfaces/surfaces.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <numbers>
#include <set>

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

TEST_CASE("surfaces.top_expansion: the top surface grows into the infill (orca_2_5.md, rule 7)", "[surfaces]") {
    const auto mm2 = [](const Polygons& p) { return Clipper2Lib::Area(p) / (kUnitsPerMm * kUnitsPerMm); };
    const auto rect = [](double x0, double y0, double x1, double y1) {
        return Polygons{{{to_units(x0), to_units(y0)}, {to_units(x1), to_units(y0)}, {to_units(x1), to_units(y1)},
                         {to_units(x0), to_units(y1)}}};
    };
    const std::vector<Polygons> infill{square(0, 0, 20)};
    // Two surfaces split by a 2 mm strip of infill: outward joins them, inward does not.
    LayerSurfaces split;
    split.top = Clipper2Lib::Union(rect(0, 0, 9, 20), rect(11, 0, 20, 20), Clipper2Lib::FillRule::NonZero);
    split.sparse = rect(9, 0, 11, 20);
    for (const TopExpansionDirection d : {TopExpansionDirection::Outward, TopExpansionDirection::Both}) {
        std::vector<LayerSurfaces> s{split};
        expand_top_surfaces(s, infill, 1.5, 0.0, d);
        CHECK_THAT(mm2(s[0].top), WithinAbs(400.0, 1e-3));
        CHECK(s[0].sparse.empty());
    }
    std::vector<LayerSurfaces> inward{split};
    expand_top_surfaces(inward, infill, 1.5, 0.0, TopExpansionDirection::Inward);
    CHECK_THAT(mm2(inward[0].top), WithinAbs(360.0, 1e-3));
    // A surface with a hole around what rises from it: inward closes 1 mm of it.
    LayerSurfaces holed;
    holed.top = Clipper2Lib::Difference(square(0, 0, 20), rect(8, 8, 12, 12), Clipper2Lib::FillRule::NonZero);
    holed.sparse = rect(8, 8, 12, 12);
    std::vector<LayerSurfaces> closed{holed};
    expand_top_surfaces(closed, infill, 1.0, 0.0, TopExpansionDirection::Inward);
    CHECK_THAT(mm2(closed[0].sparse), WithinAbs(4.0, 1e-3));
    std::vector<LayerSurfaces> kept{holed};
    expand_top_surfaces(kept, infill, 1.0, 0.0, TopExpansionDirection::Outward);
    CHECK_THAT(mm2(kept[0].sparse), WithinAbs(16.0, 1e-3));
    // The margin keeps what grows that far from the walls.
    LayerSurfaces half;
    half.top = rect(0, 0, 9, 20);
    half.sparse = rect(9, 0, 20, 20);
    std::vector<LayerSurfaces> margin{half};
    expand_top_surfaces(margin, infill, 5.0, 8.0, TopExpansionDirection::Outward);
    CHECK_THAT(mm2(margin[0].top), WithinAbs(180.0 + 3.0 * 4.0, 1e-3));
    // 0 leaves it as it was.
    std::vector<LayerSurfaces> off{split};
    expand_top_surfaces(off, infill, 0.0, 0.0, TopExpansionDirection::Both);
    CHECK_THAT(mm2(off[0].top), WithinAbs(360.0, 1e-3));
    // The settings read and are checked.
    const Settings read = parse_settings(
        R"({"surfaces": {"top_expansion": 1, "top_expansion_margin": 0.2, "top_expansion_direction": "inward"}})");
    CHECK(read.top_expansion == 1.0);
    CHECK(read.top_expansion_direction == TopExpansion::Inward);
    CHECK_THROWS(parse_settings(R"({"surfaces": {"top_expansion_direction": "up"}})"));
}

TEST_CASE("O2-7b and O2-7c: the expanded top surface takes the inner walls round a hole, not at the edge (orca_2_5.md, rule 7)", "[surfaces]") {
    // A 20 x 20 x 5 mm plate with a 6 mm square hole in the middle, of four boxes.
    TriangleMesh mesh;
    add_box(mesh, 0, 0, 0, 20, 7.5f, 5);
    add_box(mesh, 0, 12.5f, 0, 20, 20, 5);
    add_box(mesh, 0, 7, 0, 7, 13, 5);
    add_box(mesh, 13, 7, 0, 20, 13, 5);
    Settings s; // 3 walls, 3 top layers
    s.wall_width = s.outer_wall_width = 0.4;
    s.infill_solid_below_area = 0.0;
    // The inner walls (index > 0) of the last layer by the hole and by the plate's edge.
    const auto inner_walls = [](const SlicedModel& m) {
        int hole = 0, edge = 0;
        for (const Toolpath& t : m.walls.back().toolpaths) {
            if (t.index == 0)
                continue;
            for (const ToolpathPoint& q : t.points) {
                if (q.p.x > 5.5 && q.p.x < 14.5 && q.p.y > 5.5 && q.p.y < 14.5)
                    ++hole;
                if (q.p.x < 1.5 || q.p.x > 18.5 || q.p.y < 1.5 || q.p.y > 18.5)
                    ++edge;
            }
        }
        return std::pair{hole, edge};
    };
    const SlicedModel plain = slice_model(weld_exact(mesh), s);
    const auto [hole_before, edge_before] = inner_walls(plain);
    CHECK(hole_before > 0);
    CHECK(edge_before > 0);
    s.top_expansion = 2.0;
    s.top_expansion_direction = TopExpansion::Inward;
    const SlicedModel grown = slice_model(weld_exact(mesh), s);
    const auto [hole_after, edge_after] = inner_walls(grown);
    CHECK(hole_after == 0);       // only the wall touching the hole stays round it
    CHECK(edge_after == edge_before); // the plate's edge keeps its 3 walls
    // The outer walls are all still there, and the surface reaches the hole's wall.
    const auto outer = [](const SlicedModel& m) {
        int n = 0;
        for (const Toolpath& t : m.walls.back().toolpaths)
            n += t.index == 0;
        return n;
    };
    CHECK(outer(grown) == outer(plain));
    CHECK(area_mm2(grown.surfaces.back().top) > area_mm2(plain.surfaces.back().top) + 10.0);
    // The layers under the top surface keep their walls.
    CHECK(grown.walls.front().toolpaths.size() == plain.walls.front().toolpaths.size());
    // O2-7c: outward too, the island's edge keeps all its walls; the hole's go as before.
    s.top_expansion_direction = TopExpansion::InwardAndOutward;
    const SlicedModel both = slice_model(weld_exact(mesh), s);
    const auto [hole_both, edge_both] = inner_walls(both);
    CHECK(hole_both == 0);
    CHECK(edge_both == edge_before);
    s.top_expansion_direction = TopExpansion::Outward;
    const auto [hole_out, edge_out] = inner_walls(slice_model(weld_exact(mesh), s));
    CHECK(hole_out == hole_before);
    CHECK(edge_out == edge_before);
}

TEST_CASE("infill.separated: each part's infill centred on its own box (orca_2_5.md, rule 12)", "[surfaces][infill]") {
    // Two 10 mm cubes 23.3 mm apart: off the infill's grid from each other.
    TriangleMesh mesh;
    add_box(mesh, 0, 0, 0, 10, 10, 10);
    add_box(mesh, 23.3f, 0, 0, 33.3f, 10, 10);
    Settings s;
    s.wall_width = s.outer_wall_width = 0.4;
    s.infill_solid_below_area = 0.0;
    // The sparse infill's points of the middle layer on each cube, the second
    // moved back by 23.3 mm, rounded to the micron.
    const auto points = [](const SlicedModel& m, std::size_t layer = 25, bool solid = false) {
        std::set<std::pair<long, long>> left, right;
        for (const Toolpath& t : solid ? m.fills[layer].solid : m.fills[layer].sparse)
            for (const ToolpathPoint& q : t.points) {
                if (q.p.x < 15.0)
                    left.insert({std::lround(q.p.x * 1000), std::lround(q.p.y * 1000)});
                else
                    right.insert({std::lround((q.p.x - 23.3) * 1000), std::lround(q.p.y * 1000)});
            }
        return std::pair{left, right};
    };
    const SlicedModel shared = slice_model(weld_exact(mesh), s);
    const auto [a, b] = points(shared);
    REQUIRE_FALSE(a.empty());
    CHECK(a != b); // one grid for the plate: the cubes' lines differ
    s.infill_separated = true;
    const SlicedModel own = slice_model(weld_exact(mesh), s);
    const auto [c, d] = points(own);
    REQUIRE_FALSE(c.empty());
    CHECK(c == d); // each on its own centre: the same lines
    // The solid inside the part (layer 1, over the bottom surface) too.
    const auto [e, f] = points(own, 1, true);
    REQUIRE_FALSE(e.empty());
    CHECK(e == f);
    // The bottom surface itself stays where the plate puts it.
    CHECK(points(own, 0, true) == points(shared, 0, true));
    CHECK(parse_settings(R"({"infill": {"separated": true}})").infill_separated);
    CHECK_THROWS(parse_settings(R"({"infill": {"separated": 1}})"));
}

TEST_CASE("infill.adaptive_mode: gyroid denser by the faces, sparser inside (orca_2_5.md, rule 10, OR9)", "[surfaces][infill]") {
    CHECK(adaptive_density(0.15, 0.05, 0.5, AdaptiveGradient::Linear) == Catch::Approx(0.10));
    CHECK(adaptive_density(0.15, 0.05, 0.5, AdaptiveGradient::Quadratic) == Catch::Approx(0.125));
    CHECK(adaptive_density(0.15, 0.05, 0.5, AdaptiveGradient::Exponential) == Catch::Approx(0.15 * std::sqrt(1.0 / 3.0)));
    TriangleMesh mesh;
    add_box(mesh, 0, 0, 0, 40, 40, 40);
    Settings s;
    s.wall_width = s.outer_wall_width = 0.4;
    s.infill_pattern = InfillPattern::Gyroid;
    s.infill_density = 15.0;
    s.infill_solid_below_area = 0.0;
    // The sparse lines' length per mm2 at mid height: in a ring by the faces and in the middle.
    const auto per_area = [](const SlicedModel& m) {
        double ring = 0.0, middle = 0.0;
        for (const Toolpath& t : m.fills[m.fills.size() / 2].sparse)
            for (std::size_t i = 1; i < t.points.size(); ++i) {
                const Point2d a = t.points[i - 1].p, b = t.points[i].p;
                const double x = 0.5 * (a.x + b.x), y = 0.5 * (a.y + b.y), len = std::hypot(b.x - a.x, b.y - a.y);
                if (std::abs(x - 20.0) < 6.0 && std::abs(y - 20.0) < 6.0)
                    middle += len;
                else if (std::min({x, y, 40.0 - x, 40.0 - y}) < 6.0)
                    ring += len;
            }
        const double ring_area = 40.0 * 40.0 - 28.0 * 28.0, middle_area = 12.0 * 12.0;
        return std::pair{ring / ring_area, middle / middle_area};
    };
    const auto [ring0, middle0] = per_area(slice_model(weld_exact(mesh), s));
    CHECK(std::abs(ring0 - middle0) < 0.2 * ring0); // even without it
    s.infill_adaptive_mode = AdaptiveMode::SteppedShells;
    s.infill_adaptive_interior_density = 5.0;
    const SlicedModel shells = slice_model(weld_exact(mesh), s);
    const auto [ring1, middle1] = per_area(shells);
    CHECK(middle1 < 0.7 * ring1); // sparser in the middle
    CHECK(middle1 < 0.7 * middle0);
    // smooth_blend: the same gradient as one blended field, without the shells' cuts.
    s.infill_adaptive_mode = AdaptiveMode::SmoothBlend;
    const SlicedModel blended = slice_model(weld_exact(mesh), s);
    const auto [ring2, middle2] = per_area(blended);
    CHECK(middle2 < 0.8 * ring2);
    CHECK(blended.fills[blended.fills.size() / 2].sparse.size() < shells.fills[shells.fills.size() / 2].sparse.size());
    // normal_z: the depth within each layer, the same gradient from the walls.
    s.infill_adaptive_mode = AdaptiveMode::NormalZ;
    const auto [ring3, middle3] = per_area(slice_model(weld_exact(mesh), s));
    CHECK(middle3 < 0.8 * ring3);
    // lobes: one lobe in a cube, the same gradient.
    s.infill_adaptive_mode = AdaptiveMode::Lobes;
    const auto [ring4, middle4] = per_area(slice_model(weld_exact(mesh), s));
    CHECK(middle4 < 0.8 * ring4);
    CHECK(parse_settings(R"({"infill": {"adaptive_mode": "distance_warp", "adaptive_gradient": "quadratic"}})")
              .infill_adaptive_gradient == AdaptiveGradient::Quadratic);
    CHECK_THROWS(parse_settings(R"({"infill": {"adaptive_mode": "wavy"}})"));
}

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

TEST_CASE("vertical shell: solid infill under a 45 degree slope by mode (walls_extra.md, rule 7)", "[surfaces]") {
    // A pyramid of 0.2 mm layers, 0.2 mm in per side per layer, with walls
    // 0.4 mm wide and lines 0.4 apart. Measured from the infill's edge: off,
    // the top skin is what layer i + 3 leaves, a ring 0.2 mm wide. Partial
    // adds a bead past it, up to what layer i + 4 still covers (0.4 more
    // in): a ring 0.4 wide. Full adds, where the next layer's infill steps in
    // (0.2), three quarters of a bead more: a ring 0.5 wide.
    std::vector<Polygons> regions, infill;
    std::vector<double> z;
    for (int i = 0; i < 20; ++i) {
        regions.push_back(square(i * 0.2, i * 0.2, 20.0 - i * 0.4));
        infill.push_back(square(i * 0.2 + 0.4, i * 0.2 + 0.4, 19.2 - i * 0.4));
        z.push_back(0.2 * (i + 1));
    }
    const auto ring = [](double side, double width) { return side * side - (side - 2 * width) * (side - 2 * width); };
    ShellCheck partial{ShellCheck::Partial, z, 0.4}, full{ShellCheck::Full, z, 0.4}, off{ShellCheck::Off, z, 0.4};
    const std::vector<LayerSurfaces> a = classify_surfaces(regions, infill, 3, 0, 0.0, &off);
    const std::vector<LayerSurfaces> b = classify_surfaces(regions, infill, 3, 0, 0.0, &partial);
    const std::vector<LayerSurfaces> c = classify_surfaces(regions, infill, 3, 0, 0.0, &full);
    for (int i = 0; i <= 15; ++i) {
        INFO("layer " << i);
        const double side = 19.2 - i * 0.4;
        const std::size_t l = std::size_t(i);
        CHECK_THAT(area_mm2(a[l].top), WithinAbs(ring(side, 0.2), 1e-3));
        CHECK_THAT(area_mm2(b[l].top), WithinAbs(ring(side, 0.4), 1e-3));
        CHECK_THAT(area_mm2(c[l].top), WithinAbs(ring(side, 0.5), 1e-3));
        CHECK_THAT(area_mm2(c[l].top) + area_mm2(c[l].sparse), WithinAbs(side * side, 1e-3));
    }
    // Upside down (an overhang): only Full looks down. The infill past the
    // infill 3 layers below (1.0 in from the region's edge, 0.6 from the
    // infill's) and two beads more: a ring 1.4 wide.
    std::reverse(regions.begin(), regions.end());
    std::reverse(infill.begin(), infill.end());
    const std::vector<LayerSurfaces> d = classify_surfaces(regions, infill, 0, 3, 0.0, &partial);
    const std::vector<LayerSurfaces> e = classify_surfaces(regions, infill, 0, 3, 0.0, &full);
    for (int i = 3; i < 16; ++i) {
        INFO("layer " << i);
        const double side = 19.2 - (19 - i) * 0.4;
        CHECK_THAT(area_mm2(d[std::size_t(i)].bottom), WithinAbs(ring(side, 0.2), 1e-3));
        CHECK_THAT(area_mm2(e[std::size_t(i)].bottom), WithinAbs(ring(side, 1.4), 1e-3));
    }
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
