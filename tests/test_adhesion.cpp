#include "adhesion/adhesion.hpp"
#include "config/settings.hpp"
#include "gcode/writer.hpp"
#include "mesh/indexed_mesh.hpp"
#include "slice/pipeline.hpp"
#include "test_printer.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <sstream>
#include <string>

using namespace tisma;
using Catch::Matchers::WithinAbs;

namespace {

Polygon square(double x0, double y0, double x1, double y1) {
    return {{to_units(x0), to_units(y0)}, {to_units(x1), to_units(y0)}, {to_units(x1), to_units(y1)}, {to_units(x0), to_units(y1)}};
}

// A circle of radius r around (cx, cy), counter-clockwise (or clockwise: a hole).
Polygon circle(double cx, double cy, double r, bool hole = false, int n = 512) {
    Polygon p;
    for (int i = 0; i < n; ++i) {
        const double a = 2.0 * std::numbers::pi * i / n * (hole ? -1.0 : 1.0);
        p.push_back({to_units(cx + r * std::cos(a)), to_units(cy + r * std::sin(a))});
    }
    return p;
}

// The mean distance of a loop's points from (cx, cy).
double radius_of(const Toolpath& t, double cx = 0.0, double cy = 0.0) {
    double sum = 0.0;
    for (const ToolpathPoint& q : t.points)
        sum += std::hypot(q.p.x - cx, q.p.y - cy);
    return sum / double(t.points.size());
}

double loop_length(const Toolpath& t) {
    double sum = 0.0;
    for (std::size_t i = 0; i < t.points.size(); ++i) {
        const Point2d& a = t.points[i].p;
        const Point2d& b = t.points[(i + 1) % t.points.size()].p;
        sum += std::hypot(b.x - a.x, b.y - a.y);
    }
    return sum;
}

} // namespace

TEST_CASE("skirt: rounded loops at the beads' spacing (supports_skirt.md, rule 8, S7, S8)", "[adhesion]") {
    const double pi = 3.14159265358979;
    const double spacing = loop_spacing(0.4, 0.2); // 0.357 mm: beads 0.4 wide and 0.2 high overlap
    CHECK_THAT(spacing, WithinAbs(0.4 - 0.2 * (1.0 - pi / 4.0), 1e-12));
    const Polygons cube{square(0, 0, 20, 20)};
    // S7: one loop 6 mm away, its axis 6.18 mm from the part, the corners round: 118.5 mm in Tisma Slicer.
    const std::vector<Toolpath> one = skirt_loops(cube, 0.0, 1, 6.0, 0.4, 0.2);
    REQUIRE(one.size() == 1);
    CHECK(one[0].closed);
    const double r = 6.0 + spacing / 2.0;
    CHECK_THAT(r, WithinAbs(6.18, 0.005));
    CHECK_THAT(loop_length(one[0]), WithinAbs(80.0 + 2.0 * pi * r, 0.05));
    CHECK_THAT(loop_length(one[0]), WithinAbs(118.5, 0.5));
    CHECK(skirt_loops(cube, 0.0, 0, 6.0, 0.4, 0.2).empty());
    // S8: three loops 2 mm away, the outer axis at 2.89 mm; 287.0 mm a layer in Tisma Slicer.
    const std::vector<Toolpath> three = skirt_loops(cube, 0.0, 3, 2.0, 0.4, 0.2);
    REQUIRE(three.size() == 3);
    double total = 0.0;
    for (const Toolpath& t : three)
        total += loop_length(t);
    CHECK_THAT(loop_length(three[2]), WithinAbs(80.0 + 2.0 * pi * (2.0 + 2.5 * spacing), 0.05));
    CHECK_THAT(2.0 + 2.5 * spacing, WithinAbs(2.89, 0.01));
    CHECK_THAT(total, WithinAbs(287.0, 1.0));
    // Negative indices: a skirt's seam must not count as a wall's (aligned seams remember them by index).
    for (const Toolpath& t : skirt_loops(cube, 2.0, 3, 3.0, 0.4, 0.2))
        CHECK(t.index < 0);
    for (const Toolpath& t : brim_loops(cube, 2.0, 0.4, 0.2))
        CHECK(t.index < 0);
    // Around a brim of 2 mm (5 loops), 3 mm further out.
    const std::vector<Toolpath> around_brim = skirt_loops(cube, 2.0, 1, 3.0, 0.4, 0.2);
    REQUIRE(around_brim.size() == 1);
    CHECK_THAT(loop_length(around_brim[0]), WithinAbs(80.0 + 2.0 * pi * (5 * spacing + 3.0 + spacing / 2.0), 0.05));
    // A hole in the part gets no skirt inside it.
    const Polygons ring{square(0, 0, 30, 30), {{to_units(10), to_units(10)}, {to_units(10), to_units(20)},
                                                {to_units(20), to_units(20)}, {to_units(20), to_units(10)}}};
    CHECK(skirt_loops(ring, 0.0, 1, 3.0, 0.4, 0.2).size() == 1);
}

TEST_CASE("brim: loops at the beads' spacing up to its width, merged between close parts (rule 9, S9)", "[adhesion]") {
    const double pi = 3.14159265358979;
    const double spacing = loop_spacing(0.4, 0.2);
    // S9: a 5 mm brim on a 20 mm cube: 14 loops, the outer axis at 4.82 mm (its edge at 5.02); 1340.4 mm.
    const Polygons cube20{square(0, 0, 20, 20)};
    const std::vector<Toolpath> brim = brim_loops(cube20, 5.0, 0.4, 0.2);
    REQUIRE(brim.size() == 14);
    CHECK_THAT(loop_length(brim[0]), WithinAbs(80.0 + 2.0 * pi * spacing / 2.0, 0.05)); // touching the part
    CHECK_THAT(13.5 * spacing, WithinAbs(4.82, 0.005));
    double total = 0.0;
    for (const Toolpath& t : brim)
        total += loop_length(t);
    CHECK_THAT(total, WithinAbs(1340.4, 1.0));
    CHECK(brim_loops(cube20, 0.0, 0.4, 0.2).empty());
    // Two parts 1 mm apart: the first loops stay separate, the outer ones go round both.
    const Polygons two{square(0, 0, 10, 10), square(11, 0, 21, 10)};
    const std::vector<Toolpath> both = brim_loops(two, 2.0, 0.4, 0.2);
    int first = 0, last = 0;
    for (const Toolpath& t : both) {
        first += t.index == -1;
        last += t.index == -5;
    }
    CHECK(first == 2);
    CHECK(last == 1);
}

TEST_CASE("skirt: loops added until the skirt lays its filament (skirt_brim_raft.md, rule 1, B1)", "[adhesion]") {
    const Polygons cube{square(0, 0, 20, 20)};
    AdhesionShape a;
    a.skirt_loops = 1;
    a.skirt_distance = 6.0;
    a.filament_area = std::numbers::pi * 1.75 * 1.75 / 4.0;
    CHECK(skirt_loops(cube, a, 0.4, 0.2).size() == 1);
    a.skirt_min_filament = 300.0;
    const std::vector<Toolpath> loops = skirt_loops(cube, a, 0.4, 0.2);
    double path = 0.0;
    for (const Toolpath& t : loops)
        path += loop_length(t);
    const double per_mm = 0.4 * 0.2 / a.filament_area;
    CHECK(path * per_mm >= 300.0);
    CHECK((path - loop_length(loops.back())) * per_mm < 300.0); // the fewest that do
    CHECK(loops.size() > 40);
    CHECK(loops.size() < 70);
    // The loops go outwards, a bead's spacing apart.
    CHECK(loops.back().index == -int(loops.size()));
}

TEST_CASE("brim: outer, inner, both, and apart from the part (skirt_brim_raft.md, rules 5 to 8, B4)", "[adhesion]") {
    // B4: a tube of 10 mm radius with a hole of 5, a brim 3 mm wide: 8 loops.
    const Polygons tube{circle(0, 0, 10), circle(0, 0, 5, true)};
    const double spacing = loop_spacing(0.4, 0.2);
    AdhesionShape a;
    a.brim_width = 3.0;
    std::vector<double> outer, inner;
    for (const BrimType type : {BrimType::Outer, BrimType::Inner, BrimType::OuterAndInner, BrimType::None}) {
        a.brim_type = type;
        std::vector<double> radii;
        for (const Toolpath& t : brim_loops(tube, a, 0.4, 0.2))
            radii.push_back(radius_of(t));
        std::sort(radii.begin(), radii.end());
        if (type == BrimType::Outer)
            outer = radii;
        else if (type == BrimType::Inner)
            inner = radii;
        else if (type == BrimType::OuterAndInner)
            CHECK(radii.size() == outer.size() + inner.size());
        else
            CHECK(radii.empty());
    }
    REQUIRE(outer.size() == 8);
    REQUIRE(inner.size() == 8);
    CHECK_THAT(outer.front(), WithinAbs(10.0 + spacing / 2.0, 0.01)); // B4: 10.159
    CHECK_THAT(outer.back(), WithinAbs(10.0 + 7.5 * spacing, 0.01));  // 12.678
    CHECK_THAT(inner.back(), WithinAbs(5.0 - spacing / 2.0, 0.01));   // 4.801
    CHECK_THAT(inner.front(), WithinAbs(5.0 - 7.5 * spacing, 0.01));  // 2.319
    // A gap of 1 mm moves the whole brim out, as wide as before.
    a.brim_type = BrimType::Outer;
    a.brim_gap = 1.0;
    std::vector<double> apart;
    for (const Toolpath& t : brim_loops(tube, a, 0.4, 0.2))
        apart.push_back(radius_of(t));
    std::sort(apart.begin(), apart.end());
    REQUIRE(apart.size() == 8);
    CHECK_THAT(apart.front(), WithinAbs(11.0 + spacing / 2.0, 0.01)); // 11.168
    CHECK_THAT(apart.back(), WithinAbs(11.0 + 7.5 * spacing, 0.01));  // 13.673
    // An inner brim fills a small hole and stops.
    const Polygons small{circle(0, 0, 10), circle(0, 0, 1, true)};
    a.brim_type = BrimType::Inner;
    a.brim_gap = 0.0;
    const std::size_t n = brim_loops(small, a, 0.4, 0.2).size();
    CHECK(n >= 1);
    CHECK(n <= 3);
    // An island inside the hole keeps its place: the inner loops go round it.
    const Polygons island{circle(0, 0, 10), circle(0, 0, 6, true), circle(0, 0, 2)};
    a.brim_width = 1.0;
    for (const Toolpath& t : brim_loops(island, a, 0.4, 0.2))
        for (const ToolpathPoint& q : t.points)
            CHECK(std::hypot(q.p.x, q.p.y) > 2.0);
}

TEST_CASE("draft shield: the skirt from the part, up to the top (skirt_brim_raft.md, rule 4, B3)", "[adhesion]") {
    const Polygons cube{square(-10, -10, 10, 10)};
    const double spacing = loop_spacing(0.4, 0.2);
    AdhesionShape a;
    a.brim_width = 5.0;
    a.skirt_loops = 1;
    a.skirt_distance = 3.0;
    const std::size_t brim = brim_loops(cube, a, 0.4, 0.2).size();
    CHECK(brim == 14);
    a.shield = true;
    // The brim's loop where the skirt goes is left out: 13 loops (B3).
    CHECK(brim_loops(cube, a, 0.4, 0.2).size() == brim - 1);
    const std::vector<Toolpath> skirt = skirt_loops(cube, a, 0.4, 0.2);
    REQUIRE(skirt.size() == 1);
    double far = 0.0; // the skirt's distance from the part along X
    for (const ToolpathPoint& q : skirt[0].points)
        far = std::max(far, q.p.x);
    CHECK_THAT(far - 10.0, WithinAbs(3.0 + spacing / 2.0, 0.01));
    // In the G-code: enabled, on every layer; limited, on adhesion.skirt_layers.
    IndexedMesh m;
    for (float z : {0.0f, 10.0f})
        for (const auto& p : {std::array<float, 2>{-10, -10}, {10, -10}, {10, 10}, {-10, 10}})
            m.vertices.push_back({p[0], p[1], z});
    for (const auto& f : std::vector<std::array<std::uint32_t, 4>>{{0, 3, 2, 1}, {4, 5, 6, 7}, {0, 1, 5, 4}, {1, 2, 6, 5}, {2, 3, 7, 6}, {3, 0, 4, 7}}) {
        m.triangles.push_back({f[0], f[1], f[2]});
        m.triangles.push_back({f[0], f[2], f[3]});
    }
    Settings s = test_printer();
    s.brim_width = 5.0;
    s.skirt_loops = 1;
    s.skirt_distance = 3.0;
    const auto layers_with_skirt = [&](DraftShield shield, int skirt_layers) {
        s.draft_shield = shield;
        s.skirt_layers = skirt_layers;
        std::ostringstream out;
        write_gcode(out, slice_model(m, s), s);
        std::size_t layers = 0;
        bool counted = false;
        std::istringstream in(out.str());
        for (std::string line; std::getline(in, line);) {
            if (line == ";LAYER_CHANGE")
                counted = false;
            if (line == ";TYPE:Skirt/Brim" && !counted)
                ++layers, counted = true;
        }
        return layers;
    };
    CHECK(layers_with_skirt(DraftShield::Enabled, 1) == 50);
    CHECK(layers_with_skirt(DraftShield::Limited, 5) == 5);
    CHECK(layers_with_skirt(DraftShield::Disabled, 1) == 1);
}
