// Involute gears for printing (the owner's request, 2026-11-02).

#include "cad/gears.hpp"
#include "config/settings.hpp"
#include "gcode/check.hpp"
#include "gcode/writer.hpp"
#include "mesh/indexed_mesh.hpp"
#include "slice/pipeline.hpp"
#include "test_printer.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <map>
#include <numbers>
#include <sstream>
#include <tuple>

using namespace tisma;
using namespace tisma::cad;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

constexpr double pi = std::numbers::pi;

bool closed_and_oriented(const TriangleMesh& m) {
    using Key = std::tuple<float, float, float>;
    std::map<std::pair<Key, Key>, int> edges;
    for (std::size_t t = 0; t < m.triangle_count(); ++t)
        for (int k = 0; k < 3; ++k) {
            const Vec3f a = m.corner(t, k), b = m.corner(t, (k + 1) % 3);
            ++edges[{{a.x, a.y, a.z}, {b.x, b.y, b.z}}];
        }
    for (const auto& [edge, count] : edges)
        if (count != 1 || !edges.contains({edge.second, edge.first}))
            return false;
    return true;
}

} // namespace

TEST_CASE("gears: the involute outline and the standard proportions", "[gears]") {
    GearSpec g;
    g.teeth = 20;
    g.module = 2.0;
    g.backlash = 0.0;
    const GearDims d = gear_dims(g);
    CHECK_THAT(d.pitch_diameter, WithinAbs(40.0, 1e-9));
    CHECK_THAT(d.outside_diameter, WithinAbs(44.0, 1e-9)); // + 2 m
    CHECK_THAT(d.root_diameter, WithinAbs(35.0, 1e-9));    // - 2.5 m
    CHECK_THAT(d.base_diameter, WithinAbs(40.0 * std::cos(20.0 * pi / 180.0), 1e-9));
    // On the pitch circle tooth and gap are as wide: the outline crosses the
    // pitch radius at a quarter of a tooth pitch from the tooth's middle.
    const double quarter = pi / 20.0 / 2.0;
    CHECK_THAT(gear_outline_radius(g, quarter), WithinAbs(20.0, 1e-6));
    CHECK(gear_outline_radius(g, 0.0) == 22.0);              // the tip
    CHECK(gear_outline_radius(g, pi / 20.0) == 17.5);        // the middle of the gap
    CHECK(gear_outline_radius(g, 2.0 * pi / 20.0) == 22.0);  // the next tooth
    CHECK(gear_outline_radius(g, -quarter) == gear_outline_radius(g, quarter));
    // The backlash takes that much off the tooth on the pitch circle.
    g.backlash = 0.2;
    const double half = (pi * 2.0 / 2.0 - 0.2) / 2.0 / 20.0; // half the thickness, as an angle
    CHECK_THAT(gear_outline_radius(g, half), WithinAbs(20.0, 1e-6));
    CHECK_THAT(gear_dims(g).tooth_thickness, WithinAbs(pi - 0.2, 1e-12));
    CHECK_THAT(center_distance(g, 40), WithinAbs(60.0, 1e-9));
}

TEST_CASE("gears: spur, helical and herringbone gears are closed solids", "[gears]") {
    for (const int teeth : {8, 20, 61})
        for (const double helix : {0.0, 20.0})
            for (const bool herringbone : {false, true}) {
                if (herringbone && helix == 0.0)
                    continue;
                GearSpec g;
                g.teeth = teeth;
                g.helix_angle = helix;
                g.herringbone = herringbone;
                g.bore = teeth > 8 ? 5.0 : 0.0;
                INFO(teeth << " teeth, helix " << helix << (herringbone ? " herringbone" : ""));
                const TriangleMesh m = make_gear(g);
                CHECK(closed_and_oriented(m));
                const BoundingBox box = m.bounding_box();
                CHECK_THAT(box.max.z, WithinAbs(g.width, 1e-4));
                CHECK(box.max.x <= gear_dims(g).outside_diameter / 2.0 + 1e-3);
                // Between the root cylinder (less the bore) and the tip cylinder.
                const GearDims d = gear_dims(g);
                const double hole = pi * g.bore * g.bore / 4.0 * g.width;
                CHECK(m.signed_volume() > pi * d.root_diameter * d.root_diameter / 4.0 * g.width - hole);
                CHECK(m.signed_volume() < pi * d.outside_diameter * d.outside_diameter / 4.0 * g.width - hole);
            }
}

TEST_CASE("gears: a helical gear twists by width tan(beta) / r", "[gears]") {
    GearSpec g;
    g.teeth = 20;
    g.module = 2.0;
    g.width = 10.0;
    g.helix_angle = 15.0;
    g.bore = 0.0;
    const TriangleMesh m = make_gear(g);
    // The tip in the middle of the tooth at the bottom is at angle 0; at the
    // top, at width tan(15°) / 20.
    const double twist = 10.0 * std::tan(15.0 * pi / 180.0) / 20.0;
    bool bottom = false, top = false;
    for (const Vec3f& v : m.corners()) {
        if (std::hypot(double(v.x), double(v.y)) < 21.99)
            continue;
        const double a = std::atan2(double(v.y), double(v.x));
        if (v.z == 0.0f && std::abs(a) < 1e-6)
            bottom = true;
        if (std::abs(double(v.z) - 10.0) < 1e-5 && std::abs(a - twist) < 1e-5)
            top = true;
    }
    CHECK(bottom);
    CHECK(top);
    // Its mate turns the other way.
    g.left_hand = true;
    bool mirrored = false;
    const TriangleMesh left = make_gear(g);
    for (const Vec3f& v : left.corners())
        if (std::abs(double(v.z) - 10.0) < 1e-5 && std::hypot(double(v.x), double(v.y)) >= 21.99 &&
            std::abs(std::atan2(double(v.y), double(v.x)) + twist) < 1e-5)
            mirrored = true;
    CHECK(mirrored);
}

TEST_CASE("gears: the checks, the saved spec, and a gear that slices", "[gears]") {
    GearSpec g;
    CHECK(check_gear(g).empty());
    g.teeth = 4;
    CHECK_FALSE(check_gear(g).empty());
    g.teeth = 20;
    g.bore = 40.0; // through the teeth
    CHECK_FALSE(check_gear(g).empty());
    CHECK_THROWS_AS(make_gear(g), std::invalid_argument);
    g.bore = 6.0;
    g.helix_angle = 30.0;
    g.herringbone = true;
    const GearSpec back = parse_gear(gear_json(g));
    CHECK(back.teeth == 20);
    CHECK(back.bore == 6.0);
    CHECK(back.helix_angle == 30.0);
    CHECK(back.herringbone);
    CHECK_THROWS(parse_gear("{\"format\":\"tisma-gear-1\",\"teeth\":2}"));

    TriangleMesh part = make_gear(g);
    part.translate(100.0f, 100.0f, 0.0f);
    const Settings s = test_printer();
    const SlicedModel model = slice_model(weld_exact(part), s);
    REQUIRE(!model.layers.empty());
    std::ostringstream out;
    write_gcode(out, model, s);
    const GcodeCheck check = check_gcode(out.str(), gcode_limits(model, s));
    INFO((check.problems.empty() ? std::string() : check.problems.front().message));
    CHECK(check.ok());
}
