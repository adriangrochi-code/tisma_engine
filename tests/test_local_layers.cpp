#include "slice/local_layers.hpp"

#include "gcode/check.hpp"
#include "gcode/preview.hpp"
#include "gcode/writer.hpp"
#include "mesh/indexed_mesh.hpp"
#include "slice/pipeline.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <sstream>

using namespace tisma;
using Catch::Matchers::WithinAbs;

namespace {

Polygon square(double x0, double y0, double x1, double y1) {
    return {{to_units(x0), to_units(y0)}, {to_units(x1), to_units(y0)}, {to_units(x1), to_units(y1)},
            {to_units(x0), to_units(y1)}};
}

bool inside(const Polygons& zone, double x, double y) {
    int winding = 0;
    for (const Polygon& p : zone)
        if (Clipper2Lib::PointInPolygon(Clipper2Lib::Point64(to_units(x), to_units(y)), p) !=
            Clipper2Lib::PointInPolygonResult::IsOutside)
            winding += Clipper2Lib::IsPositive(p) ? 1 : -1;
    return winding > 0;
}

double length(const Toolpath& t) {
    double sum = 0.0;
    for (std::size_t i = 1; i < t.points.size(); ++i)
        sum += std::hypot(t.points[i].p.x - t.points[i - 1].p.x, t.points[i].p.y - t.points[i - 1].p.y);
    if (t.closed && t.points.size() > 1)
        sum += std::hypot(t.points.front().p.x - t.points.back().p.x, t.points.front().p.y - t.points.back().p.y);
    return sum;
}

} // namespace

TEST_CASE("local layers: thick where the sections agree, fine where they change", "[local_layers]") {
    LocalLayerParams params;
    params.ratio = 2;

    SECTION("a straight block: every group thick, but a last one too short") {
        std::vector<Polygons> regions(6, Polygons{square(0, 0, 20, 20)});
        const std::vector<LayerGroup> groups = plan_layer_groups(regions, params);
        REQUIRE(groups.size() == 3);
        CHECK(groups[0].first == 1);
        CHECK(groups[0].last == 2);
        CHECK(groups[2].first == 5);
        CHECK(groups[2].last == 5);
        CHECK_THAT(area_mm2(groups[0].coarse), WithinAbs(400.0, 0.01));
        CHECK_THAT(area_mm2(groups[1].coarse), WithinAbs(400.0, 0.01));
        CHECK(groups[2].coarse.empty()); // one fine layer: nothing to make thick
    }

    SECTION("a pyramid: fine along the sloping outline, thick in the middle") {
        std::vector<Polygons> regions;
        for (int l = 0; l < 7; ++l) {
            const double in = 0.2 * l; // the outline steps in 0.2 mm a layer: more than the tolerance
            regions.push_back({square(in, in, 20 - in, 20 - in)});
        }
        const std::vector<LayerGroup> groups = plan_layer_groups(regions, params);
        REQUIRE(groups.size() == 3);
        const Polygons& coarse = groups[0].coarse; // layers 1 and 2
        CHECK(inside(coarse, 10, 10));
        CHECK_FALSE(inside(coarse, 0.5, 10)); // by the slope
        CHECK_FALSE(inside(coarse, 1.6, 10)); // within the margin of the detail
        CHECK(inside(coarse, 3.0, 10));
    }

    SECTION("two parts at the same height: the straight one thick, the sloping one fine") {
        std::vector<Polygons> regions;
        for (int l = 0; l < 5; ++l) {
            const double in = 0.2 * l;
            regions.push_back({square(0, 0, 10, 10), square(20 + in, in, 30 - in, 10 - in)});
        }
        const std::vector<LayerGroup> groups = plan_layer_groups(regions, params);
        REQUIRE(groups.size() == 2);
        const Polygons& coarse = groups[0].coarse;
        CHECK(inside(coarse, 0.3, 5)); // the straight part's wall goes thick
        CHECK(inside(coarse, 5, 5));
        CHECK_FALSE(inside(coarse, 20.5, 5)); // the sloping one's wall goes fine
        CHECK(inside(coarse, 25, 5));         // its middle, away from the slope, thick
        // The sloping part's thick zone: its section at the group's top, less what the layer above leaves
        // uncovered (a pyramid's every layer shows its edge), less the margin.
        CHECK_THAT(area_mm2(coarse), WithinAbs(100.0 + std::pow(10.0 - 2 * 0.6 - 2 * 1.35, 2.0), 0.5));
    }

    SECTION("a step smaller than the tolerance stays thick") {
        std::vector<Polygons> regions;
        for (int l = 0; l < 5; ++l)
            regions.push_back({square(0.01 * l, 0, 20, 20)});
        const std::vector<LayerGroup> groups = plan_layer_groups(regions, params);
        REQUIRE(groups.size() == 2);
        CHECK(inside(groups[0].coarse, 0.5, 10));
    }

    SECTION("ratio 3 groups three layers; the part's top group prints fine") {
        params.ratio = 3;
        std::vector<Polygons> regions(10, Polygons{square(0, 0, 20, 20)});
        const std::vector<LayerGroup> groups = plan_layer_groups(regions, params);
        REQUIRE(groups.size() == 3);
        CHECK(groups[1].first == 4);
        CHECK(groups[1].last == 6);
        CHECK_FALSE(groups[0].coarse.empty());
        CHECK_FALSE(groups[1].coarse.empty());
        CHECK(groups[2].coarse.empty()); // layers 7 to 9: the top of the part
    }

    SECTION("a bottom over air prints fine: a bridge never goes thick") {
        std::vector<Polygons> regions;
        regions.push_back({square(0, 0, 4, 20)}); // a post
        for (int l = 1; l < 6; ++l)
            regions.push_back({square(0, 0, 20, 20)}); // a slab on it from layer 1
        const std::vector<LayerGroup> groups = plan_layer_groups(regions, params);
        REQUIRE(groups.size() == 3);
        CHECK_FALSE(inside(groups[0].coarse, 12, 10)); // over air at layer 1
        CHECK(inside(groups[1].coarse, 12, 10));       // layers 3 and 4: on the slab
    }
}

TEST_CASE("local layers: toolpaths are cut at the zone's edge", "[local_layers]") {
    const Polygons zone{square(5, -5, 20, 15)};
    Toolpath loop;
    loop.closed = true;
    loop.points = {{{0, 0}, 0.4}, {{10, 0}, 0.4}, {{10, 10}, 0.4}, {{0, 10}, 0.4}};

    const std::vector<Toolpath> in = clip_toolpaths({loop}, zone, true);
    const std::vector<Toolpath> out = clip_toolpaths({loop}, zone, false);
    REQUIRE(in.size() == 1);
    REQUIRE(out.size() == 1); // the piece through the start is one
    CHECK_FALSE(in[0].closed);
    CHECK_FALSE(out[0].closed);
    CHECK_THAT(length(in[0]) + length(out[0]), WithinAbs(length(loop), 1e-9));
    CHECK_THAT(in[0].points.front().p.x, WithinAbs(5.0, 1e-9));
    CHECK_THAT(in[0].points.back().p.x, WithinAbs(5.0, 1e-9));
    CHECK_THAT(out[0].points.front().p.x, WithinAbs(5.0, 1e-9));
    CHECK_THAT(out[0].points.front().p.y, WithinAbs(10.0, 1e-9));
    CHECK_THAT(out[0].points.back().p.y, WithinAbs(0.0, 1e-9));

    // The width at a cut is the one between the ends.
    Toolpath ramp;
    ramp.points = {{{0, 1}, 0.4}, {{10, 1}, 0.8}};
    const std::vector<Toolpath> half = clip_toolpaths({ramp}, zone, true);
    REQUIRE(half.size() == 1);
    CHECK_THAT(half[0].points.front().width, WithinAbs(0.6, 1e-9));
    CHECK_THAT(half[0].points.back().width, WithinAbs(0.8, 1e-9));

    // A path on one side stays as it was, closed.
    Toolpath small = loop;
    for (ToolpathPoint& p : small.points)
        p.p.x += 7.0, p.p.y = p.p.y * 0.5;
    for (ToolpathPoint& p : small.points)
        p.p.x = 6.0 + (p.p.x - 7.0) * 0.5;
    const std::vector<Toolpath> kept = clip_toolpaths({small}, zone, true);
    REQUIRE(kept.size() == 1);
    CHECK(kept[0].closed);
    CHECK(clip_toolpaths({small}, zone, false).empty());
    CHECK(clip_toolpaths({loop}, Polygons{}, false).size() == 1); // no zone: all outside
    CHECK(clip_toolpaths({loop}, Polygons{}, true).empty());
}

namespace {

// Adds a box from (x0, y0, 0) to (x1, y1, h), facing out.
void add_box(IndexedMesh& m, float x0, float y0, float x1, float y1, float h) {
    const auto base = std::uint32_t(m.vertices.size());
    for (float z : {0.0f, h})
        for (const auto& p : {std::array<float, 2>{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}})
            m.vertices.push_back({p[0], p[1], z});
    auto quad = [&](std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d) {
        m.triangles.push_back({base + a, base + b, base + c});
        m.triangles.push_back({base + a, base + c, base + d});
    };
    quad(0, 3, 2, 1);
    quad(4, 5, 6, 7);
    for (std::uint32_t i = 0; i < 4; ++i)
        quad(i, (i + 1) % 4, 4 + (i + 1) % 4, 4 + i);
}

// A square pyramid on (x0, y0)-(x0 + side, y0 + side), its tip h high.
void add_pyramid(IndexedMesh& m, float x0, float y0, float side, float h) {
    const auto base = std::uint32_t(m.vertices.size());
    for (const auto& p : {std::array<float, 2>{x0, y0}, {x0 + side, y0}, {x0 + side, y0 + side}, {x0, y0 + side}})
        m.vertices.push_back({p[0], p[1], 0.0f});
    m.vertices.push_back({x0 + side / 2, y0 + side / 2, h});
    m.triangles.push_back({base + 0, base + 2, base + 1});
    m.triangles.push_back({base + 0, base + 3, base + 2});
    for (std::uint32_t i = 0; i < 4; ++i)
        m.triangles.push_back({base + i, base + (i + 1) % 4, base + 4});
}

double extruded_volume(const std::string& gcode, double filament_diameter) {
    // Relative extrusion: every move's E adds up.
    double e = 0.0;
    std::istringstream in(gcode);
    for (std::string line; std::getline(in, line);) {
        if (line.rfind("G1 ", 0) != 0)
            continue;
        if (const std::size_t at = line.find(" E"); at != std::string::npos)
            e += std::stod(line.substr(at + 2));
    }
    return e * std::numbers::pi * filament_diameter * filament_diameter / 4.0;
}

} // namespace

TEST_CASE("local layers: a straight box and a pyramid side by side, sliced to checked G-code", "[local_layers]") {
    IndexedMesh m;
    add_box(m, 0, 0, 20, 20, 9);
    add_pyramid(m, 30, 0, 20, 9);
    Settings fine;
    fine.gpu = GpuMode::Off;
    fine.relative_extrusion = true;
    fine.layer_height = 0.1;
    fine.first_layer_height = 0.3;
    Settings zones = fine;
    zones.layer_height = 0.3;
    zones.detail_height = 0.1;
    // The skins count in thick layers: the same millimetres in the all-fine reference.
    fine.top_layers = zones.top_layers * 3;
    fine.bottom_layers = zones.bottom_layers * 3;
    REQUIRE(detail_ratio(zones) == 3);
    CHECK(detail_ratio(fine) == 1);
    Settings r;
    r.layer_height = 0.1, r.detail_height = 0.025; // four of 0.025 would be too thin: two of 0.05
    CHECK(detail_ratio(r) == 2);
    r.layer_height = 0.06, r.detail_height = 0.03; // two of 0.03: too thin, off
    CHECK(detail_ratio(r) == 1);
    r.layer_height = 0.4, r.detail_height = 0.05; // eight would be asked: four at most
    CHECK(detail_ratio(r) == 4);
    r.detail_height = 0.5; // not below the layer height: off
    CHECK(detail_ratio(r) == 1);

    const SlicedModel a = slice_model(m, fine);
    const SlicedModel b = slice_model(m, zones);
    CHECK(a.layers.size() == b.layers.size()); // the same fine stack
    CHECK(a.thick.empty());
    REQUIRE(!b.groups.empty());
    std::size_t thick_layers = 0;
    for (const ThickLayer& t : b.thick)
        thick_layers += !t.empty();
    CHECK(thick_layers > b.groups.size() / 2);

    std::ostringstream ga, gb;
    write_gcode(ga, a, fine);
    write_gcode(gb, b, zones);
    const GcodeCheck ca = check_gcode(ga.str(), gcode_limits(a, fine));
    const GcodeCheck cb = check_gcode(gb.str(), gcode_limits(b, zones));
    CHECK(ca.ok());
    INFO((cb.problems.empty() ? std::string() : cb.problems.front().message));
    CHECK(cb.ok());

    // The thick zone prints 0.3 mm beads: the box's outer wall among them.
    const std::string g = gb.str();
    const std::size_t thick = g.find(";HEIGHT:0.300\n", g.find(";Z:1.200"));
    REQUIRE(thick != std::string::npos);
    CHECK(g.find(";TYPE:External perimeter", thick) < g.find(";LAYER_CHANGE", thick));

    // About the same material, in less time.
    const double va = extruded_volume(ga.str(), fine.filament_diameter);
    const double vb = extruded_volume(g, zones.filament_diameter);
    CHECK_THAT(vb / va, WithinAbs(1.0, 0.05));
    const double ta = read_gcode_preview(ga.str(), fine.filament_diameter).estimated_seconds;
    const double tb = read_gcode_preview(g, zones.filament_diameter).estimated_seconds;
    CHECK(tb < ta * 0.9);
    UNSCOPED_INFO("all fine " << ta << " s, by zone " << tb << " s; volume " << va << " and " << vb << " mm3");
}
