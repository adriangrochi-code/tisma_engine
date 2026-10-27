#include "support/support.hpp"
#include "support/tree.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>

using namespace tisma;
using Catch::Matchers::WithinAbs;

namespace {

Polygons square(double x0, double y0, double x1, double y1) {
    return {{{to_units(x0), to_units(y0)}, {to_units(x1), to_units(y0)}, {to_units(x1), to_units(y1)}, {to_units(x0), to_units(y1)}}};
}

// The overhang of supports_more.md (SM13), in layers of 0.2: a column X 90
// to 100, Y 90 to 110, Z 0 to 10, under a slab X 90 to 110 from Z 8 to 10
// (layers 40 to 49).
std::vector<Polygons> overhang_piece() {
    std::vector<Polygons> regions;
    for (int l = 0; l < 50; ++l)
        regions.push_back(l < 40 ? square(90, 90, 100, 110) : square(90, 90, 110, 110));
    return regions;
}

// The shelf of supports_more.md: a base X 90 to 120, Z 0 to 3 (layers 0 to
// 14), a column at its -X end from Z 3 to 10 and a slab over it all from Z 10
// to 12 (layers 50 to 59).
std::vector<Polygons> shelf_piece() {
    std::vector<Polygons> regions;
    for (int l = 0; l < 60; ++l)
        regions.push_back(l < 15 ? square(90, 90, 120, 110) : l < 50 ? square(90, 90, 100, 110) : square(90, 90, 120, 110));
    return regions;
}

SupportParams support_params() {
    SupportParams p;
    p.z_gap_layers = 1;
    p.xy_gap = 0.6;
    return p;
}

TreeParams tree_params(const SupportParams& p) {
    TreeParams t;
    t.z_gap_layers = p.z_gap_layers;
    t.xy_gap = p.xy_gap;
    t.interface_layers = 3;
    return t;
}

// The distance from (x, y) to a region's outline.
double to_outline(const Polygons& region, double x, double y) {
    double best = 1e9;
    for (const Polygon& ring : region)
        for (std::size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++) {
            const double ax = to_mm(ring[j].x), ay = to_mm(ring[j].y), bx = to_mm(ring[i].x), by = to_mm(ring[i].y);
            const double dx = bx - ax, dy = by - ay, len2 = dx * dx + dy * dy;
            const double t = len2 > 0.0 ? std::clamp(((x - ax) * dx + (y - ay) * dy) / len2, 0.0, 1.0) : 0.0;
            best = std::min(best, std::hypot(ax + t * dx - x, ay + t * dy - y));
        }
    return best;
}

} // namespace

TEST_CASE("tree support: the tips' spacing, 3 mm by default, closer with the top rate, wider with wider tips", "[support][tree]") {
    CHECK_THAT(tree_tip_spacing(0.8, 15.0, 1.0), WithinAbs(3.0, 1e-9));
    CHECK(tree_tip_spacing(0.8, 30.0, 1.0) < 3.0);
    CHECK(tree_tip_spacing(1.5, 15.0, 1.0) > 3.0);
    CHECK_THAT(tree_tip_spacing(0.8, 15.0, 2.0), WithinAbs(6.0, 1e-9));
}

TEST_CASE("tree support: SM13, tips under the interface, branches that join down to the bed", "[support][tree]") {
    const std::vector<Polygons> regions = overhang_piece();
    const std::vector<double> heights(regions.size(), 0.2);
    const SupportParams sp = support_params();
    TreeParams tp = tree_params(sp);
    const std::vector<Polygons> over = supported_overhangs(regions, heights, sp);
    REQUIRE_FALSE(over[40].empty());
    const auto nodes = tree_nodes(regions, heights, over, tp);
    // The tips at Z 7.2 (layer 35): under the 3 interface layers and the gap.
    for (std::size_t l = 36; l < nodes.size(); ++l)
        CHECK(nodes[l].empty());
    const std::vector<TreeNode>& tips = nodes[35];
    CHECK(tips.size() >= 18);
    CHECK(tips.size() <= 26); // 22 in the spec
    for (const TreeNode& t : tips) {
        CHECK_THAT(t.r, WithinAbs(0.4, 1e-9));
        CHECK(t.x > 100.0);
        CHECK(t.x < 110.0 + 1e-9);
    }
    // No two tips nearer than 0.8 spacings.
    for (std::size_t i = 0; i < tips.size(); ++i)
        for (std::size_t j = i + 1; j < tips.size(); ++j)
            CHECK(std::hypot(tips[i].x - tips[j].x, tips[i].y - tips[j].y) >= 0.8 * tp.tip_spacing - 1e-9);
    // Down to the bed, fewer and wider: they join.
    REQUIRE_FALSE(nodes[0].empty());
    CHECK(nodes[0].size() < tips.size() / 2);
    for (std::size_t l = 1; l < 35; ++l)
        CHECK(nodes[l - 1].size() <= nodes[l].size());
    const auto widest = [](const std::vector<TreeNode>& layer) {
        double r = 0.0;
        for (const TreeNode& t : layer)
            r = std::max(r, t.r);
        return r;
    };
    CHECK(widest(nodes[0]) > widest(nodes[30]));
    CHECK(widest(nodes[30]) >= 1.0); // the branch diameter, 2 mm
    // Every branch off the column by the gap and its radius.
    for (std::size_t l = 0; l < 36; ++l)
        for (const TreeNode& t : nodes[l]) {
            CHECK_FALSE(region_contains(regions[l], t.x, t.y));
            CHECK(to_outline(regions[l], t.x, t.y) >= t.r + tp.xy_gap - 1e-3);
        }
    // The areas: the interface under the slab on layers 36 to 38, kept 0.6
    // off the column; nothing in the gap (layer 39).
    const std::vector<Polygons> areas = tree_support_areas(regions, nodes, over, tp);
    for (std::size_t l = 36; l <= 38; ++l) {
        CHECK_THAT(area_mm2(areas[l]), WithinAbs(9.4 * 20.0, 1e-3));
        CHECK(region_contains(areas[l], 100.7, 100.0));
        CHECK_FALSE(region_contains(areas[l], 100.5, 100.0));
    }
    for (std::size_t l = 39; l < areas.size(); ++l)
        CHECK(areas[l].empty());
    for (std::size_t l = 0; l <= 35; ++l) {
        CHECK_FALSE(areas[l].empty());
        CHECK(area_mm2(Clipper2Lib::Intersect(areas[l], offset(regions[l], 0.59), Clipper2Lib::FillRule::NonZero)) < 1e-9);
    }
}

TEST_CASE("tree support: wider tips are fewer, a higher top rate more (SM13)", "[support][tree]") {
    const std::vector<Polygons> regions = overhang_piece();
    const std::vector<double> heights(regions.size(), 0.2);
    const SupportParams sp = support_params();
    const std::vector<Polygons> over = supported_overhangs(regions, heights, sp);
    const auto tips = [&](double tip_diameter, double top_rate) {
        TreeParams tp = tree_params(sp);
        tp.tip_diameter = tip_diameter;
        tp.tip_spacing = tree_tip_spacing(tip_diameter, top_rate, 1.0);
        return tree_nodes(regions, heights, over, tp)[35].size();
    };
    const std::size_t plain = tips(0.8, 15.0), wide = tips(1.5, 15.0), dense = tips(0.8, 30.0);
    CHECK(wide < plain);  // 18 against 22 in the spec
    CHECK(dense > plain); // 32
    CHECK(dense >= 28);
}

TEST_CASE("tree support: branches over the shelf's base rest on it, a gap above it", "[support][tree]") {
    const std::vector<Polygons> regions = shelf_piece();
    const std::vector<double> heights(regions.size(), 0.2);
    const SupportParams sp = support_params();
    const TreeParams tp = tree_params(sp);
    const std::vector<Polygons> over = supported_overhangs(regions, heights, sp);
    const auto nodes = tree_nodes(regions, heights, over, tp);
    CHECK(nodes[45].size() >= 30); // tips under the 20 x 20 overhang
    // Nothing in the base; over it, branches from layer 16 up, with the gap
    // layer (15) empty: only the branches leaning past its edge are there.
    for (std::size_t l = 0; l <= 15; ++l)
        for (const TreeNode& t : nodes[l])
            CHECK_FALSE(region_contains(offset(regions[l], tp.xy_gap), t.x, t.y));
    const auto over_base = [](const TreeNode& t) { return t.x > 101.5 && t.x < 118.5 && t.y > 91.5 && t.y < 108.5; };
    CHECK(std::none_of(nodes[15].begin(), nodes[15].end(), over_base));
    CHECK(std::count_if(nodes[16].begin(), nodes[16].end(), over_base) >= 3);
}

TEST_CASE("tree support: a branch's paths, one wall, two when wide, lines inside, dense under the part", "[support][tree]") {
    const auto disc = [](double r) {
        Polygon p;
        for (int k = 0; k < 64; ++k) {
            const double a = 2.0 * 3.14159265358979 * k / 64;
            p.push_back({to_units(r * std::cos(a)), to_units(r * std::sin(a))});
        }
        return Polygons{p};
    };
    const auto closed = [](const std::vector<Toolpath>& paths) {
        return std::count_if(paths.begin(), paths.end(), [](const Toolpath& t) { return t.closed; });
    };
    // A 2 mm branch: one wall, half a bead in.
    const TreeFill thin = tree_fill(disc(1.0), {}, 0.4, 2.5, 3.0, 0.0);
    CHECK(closed(thin.support) == 1);
    CHECK(thin.interface.empty());
    for (const ToolpathPoint& p : thin.support.front().points)
        CHECK_THAT(std::hypot(p.p.x, p.p.y), WithinAbs(0.8, 0.01));
    // A 6 mm trunk: two walls and lines inside.
    const TreeFill wide = tree_fill(disc(3.0), {}, 0.4, 2.5, 3.0, 0.0);
    CHECK(closed(wide.support) == 2);
    CHECK(wide.support.size() > 2);
    // Two walls never: one.
    CHECK(closed(tree_fill(disc(3.0), {}, 0.4, 2.5, 0.0, 0.0).support) == 1);
    // Under the part, the interface: dense lines, no wall.
    const TreeFill roof = tree_fill(disc(3.0), disc(10.0), 0.4, 2.5, 3.0, 0.0);
    CHECK(roof.support.empty());
    double laid = 0.0;
    for (const Toolpath& t : roof.interface)
        for (std::size_t i = 1; i < t.points.size(); ++i)
            laid += std::hypot(t.points[i].p.x - t.points[i - 1].p.x, t.points[i].p.y - t.points[i - 1].p.y);
    CHECK(laid > 0.7 * 3.14159265358979 * 9.0 / 0.4); // lines a bead apart
}

TEST_CASE("tree support: tips no closer than two beads, however the settings ask (a big overhang stays quick)", "[support][tree]") {
    // A 10 x 40 wall under a 40 x 40 slab: 30 x 40 mm overhang.
    std::vector<Polygons> regions;
    for (int l = 0; l < 30; ++l)
        regions.push_back(l < 25 ? square(0, 0, 10, 40) : square(0, 0, 40, 40));
    const std::vector<double> heights(regions.size(), 0.2);
    const SupportParams sp = support_params();
    TreeParams tp = tree_params(sp);
    tp.tip_diameter = 0.2;
    tp.tip_spacing = tree_tip_spacing(0.2, 100.0, 0.1); // far below a bead
    tp.bead = 0.4;
    const std::vector<Polygons> over = supported_overhangs(regions, heights, sp);
    const auto nodes = tree_nodes(regions, heights, over, tp);
    const std::vector<TreeNode>& tips = nodes[20]; // under the interface and the gap
    REQUIRE_FALSE(tips.empty());
    CHECK(tips.size() <= std::size_t((30.0 / 0.8 + 2.0) * (40.0 / 0.8 + 2.0)));
    // None nearer each other than 0.8 of the spacing, two beads.
    std::size_t close = 0;
    for (std::size_t i = 0; i < tips.size(); ++i)
        for (std::size_t j = i + 1; j < tips.size(); ++j)
            close += std::hypot(tips[i].x - tips[j].x, tips[i].y - tips[j].y) < 0.8 * 0.8 - 1e-9;
    CHECK(close == 0);
    CHECK(nodes[0].size() < tips.size()); // they still join on the way down
}

