#include "geom/medial_axis.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <utility>

using namespace tisma;
using Catch::Matchers::WithinAbs;

namespace {

Polygon rect(double x0, double y0, double w, double h, bool clockwise = false) {
    Polygon p = {{to_units(x0), to_units(y0)},
                 {to_units(x0 + w), to_units(y0)},
                 {to_units(x0 + w), to_units(y0 + h)},
                 {to_units(x0), to_units(y0 + h)}};
    if (clockwise)
        std::reverse(p.begin(), p.end());
    return p;
}

double max_radius(const std::vector<MedialEdge>& edges) {
    double r = 0.0;
    for (const MedialEdge& e : edges)
        r = std::max({r, e.radius_a, e.radius_b});
    return r;
}

bool has_point(const std::vector<MedialEdge>& edges, double x, double y) {
    for (const MedialEdge& e : edges)
        if ((std::hypot(e.a.x - x, e.a.y - y) < 1e-6) || (std::hypot(e.b.x - x, e.b.y - y) < 1e-6))
            return true;
    return false;
}

} // namespace

TEST_CASE("medial axis of a square: four edges from the centre to the corners", "[medial]") {
    const auto edges = medial_axis({rect(0, 0, 10, 10)});
    REQUIRE(edges.size() == 4);
    for (const MedialEdge& e : edges) {
        CHECK_FALSE(e.curved);
        const bool a_is_centre = std::hypot(e.a.x - 5, e.a.y - 5) < 1e-6;
        const MedialEdge& m = e;
        CHECK_THAT(a_is_centre ? m.radius_a : m.radius_b, WithinAbs(5.0, 1e-6));
        CHECK_THAT(a_is_centre ? m.radius_b : m.radius_a, WithinAbs(0.0, 1e-6));
    }
    CHECK(has_point(edges, 0, 0));
    CHECK(has_point(edges, 10, 10));
}

TEST_CASE("medial axis of a rectangle: a centre line at half the width", "[medial]") {
    const auto edges = medial_axis({rect(0, 0, 20, 10)});
    CHECK(edges.size() == 5);
    CHECK(has_point(edges, 5, 5));
    CHECK(has_point(edges, 15, 5));
    CHECK_THAT(max_radius(edges), WithinAbs(5.0, 1e-6));
}

TEST_CASE("medial axis of a square ring bends around the hole's corners", "[medial]") {
    // 10 mm square with a centred 4 mm hole: walls 3 mm thick. Along a wall the
    // axis is 1.5 mm from both sides; near each corner of the hole it bends
    // into a parabola (equidistant from the hole's corner and the outer walls),
    // and on the diagonal it reaches t = sqrt(2) (3 - t), t = 3 sqrt(2) / (1 + sqrt(2)).
    const auto edges = medial_axis({rect(0, 0, 10, 10), rect(3, 3, 4, 4, true)});
    const double t = 3.0 * std::sqrt(2.0) / (1.0 + std::sqrt(2.0));
    CHECK_THAT(max_radius(edges), WithinAbs(t, 1e-6));
    CHECK(has_point(edges, t, t));
    CHECK(has_point(edges, 10.0 - t, 10.0 - t));
    CHECK(std::any_of(edges.begin(), edges.end(), [](const MedialEdge& e) { return e.curved; }));
    for (const MedialEdge& e : edges) {
        // Nothing inside the hole, and every end at least as close as the wall middle allows.
        CHECK_FALSE((e.a.x > 3.0 + 1e-6 && e.a.x < 7.0 - 1e-6 && e.a.y > 3.0 + 1e-6 && e.a.y < 7.0 - 1e-6));
        CHECK(e.radius_a <= t + 1e-6);
    }
}

TEST_CASE("medial axis of an empty region", "[medial]") {
    CHECK(medial_axis({}).empty());
}

TEST_CASE("parabolic edges come in pieces of at most 0.2 mm", "[medial]") {
    const auto edges = medial_axis({rect(0, 0, 10, 10), rect(3, 3, 4, 4, true)});
    std::size_t curved = 0;
    for (const MedialEdge& e : edges) {
        if (!e.curved)
            continue;
        ++curved;
        CHECK(std::hypot(e.b.x - e.a.x, e.b.y - e.a.y) <= kMaxCurvedPiece + 1e-9);
        // Every piece end is as far from the boundary as its radius says: here the
        // nearest boundary is a hole corner or an outer wall.
        const std::pair<Point2d, double> ends[] = {{e.a, e.radius_a}, {e.b, e.radius_b}};
        for (const auto& [p, r] : ends) {
            const double to_walls = std::min({p.x, p.y, 10.0 - p.x, 10.0 - p.y});
            const double to_corners = std::min({std::hypot(p.x - 3, p.y - 3), std::hypot(p.x - 7, p.y - 3),
                                                std::hypot(p.x - 3, p.y - 7), std::hypot(p.x - 7, p.y - 7)});
            CHECK_THAT(std::min(to_walls, to_corners), WithinAbs(r, 1e-6));
            CHECK_THAT(to_walls, WithinAbs(to_corners, 1e-6)); // on the parabola: equidistant
        }
    }
    CHECK(curved >= 8); // four hole corners, a parabola on each side of each
}

TEST_CASE("central marking: the middle line of a strip, not the corner bisectors", "[medial]") {
    MedialGraph g = medial_graph({rect(0, 0, 20, 10)});
    mark_central(g);
    std::size_t central_edges = 0;
    for (const MedialGraph::Edge& e : g.edges) {
        const MedialGraph::Node& a = g.nodes[e.a];
        const MedialGraph::Node& b = g.nodes[e.b];
        const bool middle_line = std::fabs(a.p.y - 5) < 1e-6 && std::fabs(b.p.y - 5) < 1e-6;
        CHECK(e.central == middle_line); // bisectors of 90 degree corners have alpha = 90 < 135
        central_edges += e.central ? 1 : 0;
    }
    CHECK(central_edges == 1);
}

TEST_CASE("central marking: a square's centre is a local maximum", "[medial]") {
    MedialGraph g = medial_graph({rect(0, 0, 10, 10)});
    mark_central(g);
    REQUIRE(g.nodes.size() == 5);
    for (const MedialGraph::Node& n : g.nodes)
        CHECK(n.central == (std::hypot(n.p.x - 5, n.p.y - 5) < 1e-6));
    for (const MedialGraph::Edge& e : g.edges)
        CHECK_FALSE(e.central);
}

TEST_CASE("central marking follows the bisector angle threshold", "[medial]") {
    // A thin wedge: the bisector angle along its axis is 180 - wedge angle.
    // With a 30 degree wedge (alpha = 150 > 135) the axis is central; raising
    // alpha_max to 160 degrees makes it not significant.
    const double h = std::tan(15.0 * std::numbers::pi / 180.0) * 20.0;
    const Polygon wedge = {{to_units(0), to_units(0)}, {to_units(20), to_units(-h)}, {to_units(20), to_units(h)}};
    MedialGraph g = medial_graph({wedge});
    mark_central(g, 135.0);
    bool axis_central = false;
    for (const MedialGraph::Edge& e : g.edges)
        if (std::fabs(g.nodes[e.a].p.y) < 1e-6 && std::fabs(g.nodes[e.b].p.y) < 1e-6)
            axis_central = axis_central || e.central;
    CHECK(axis_central);
    mark_central(g, 160.0);
    bool still_central = false;
    for (const MedialGraph::Edge& e : g.edges)
        if (std::fabs(g.nodes[e.a].p.y) < 1e-6 && std::fabs(g.nodes[e.b].p.y) < 1e-6)
            still_central = still_central || e.central;
    CHECK_FALSE(still_central);
}

TEST_CASE("marking filter closes short unmarked gaps on the way up", "[medial]") {
    // Hand-made graph along x: a central edge at radius 1.0, a steep unmarked edge
    // up to radius 1.3 (slope 1, not significant), then a central edge at 1.3.
    auto make = [](double gap_length) {
        MedialGraph g;
        g.nodes = {{{0.0, 0}, 1.0, true}, {{1.0, 0}, 1.0, true}, {{1.0 + gap_length, 0}, 1.3, true},
                   {{3.0 + gap_length, 0}, 1.3, true}};
        g.edges = {{0, 1, false, true, {}, {}}, {1, 2, false, false, {}, {}}, {2, 3, false, true, {}, {}}};
        return g;
    };
    MedialGraph short_gap = make(0.3);
    filter_marking(short_gap, 0.4); // w* = 0.4
    CHECK(short_gap.edges[1].central);
    MedialGraph long_gap = make(0.6);
    filter_marking(long_gap, 0.4);
    CHECK_FALSE(long_gap.edges[1].central);
}

TEST_CASE("marking filter only walks upwards", "[medial]") {
    // From the central node at radius 1.3 the unmarked edge goes down to 1.0, and
    // from the central node at 1.0 it goes up: only the upward walk counts, and it
    // finds the other central node, so the gap is closed either way.
    MedialGraph g;
    g.nodes = {{{0.0, 0}, 1.3, true}, {{0.3, 0}, 1.0, true}, {{0.6, 0}, 0.7, false}};
    g.edges = {{0, 1, false, false, {}, {}}, {1, 2, false, false, {}, {}}};
    filter_marking(g, 0.4);
    CHECK(g.edges[0].central);       // 1 -> 0 goes up and reaches a central node
    CHECK_FALSE(g.edges[1].central); // 1 -> 2 goes down: never walked
    CHECK_FALSE(g.nodes[2].central);
}

namespace {

// Every end of every edge is exactly its radius away from both of its supports,
// and the supports lie on the region's boundary.
void check_supports(const std::vector<MedialEdge>& edges, const Polygons& region) {
    auto on_boundary = [&](const Point2d& p) {
        double best = 1e300;
        for (const Polygon& path : region)
            for (std::size_t i = 0; i < path.size(); ++i) {
                const auto& a = path[i];
                const auto& b = path[(i + 1) % path.size()];
                const double ax = to_mm(a.x), ay = to_mm(a.y), bx = to_mm(b.x), by = to_mm(b.y);
                const double dx = bx - ax, dy = by - ay;
                const double t = std::clamp(((p.x - ax) * dx + (p.y - ay) * dy) / (dx * dx + dy * dy), 0.0, 1.0);
                best = std::min(best, std::hypot(p.x - (ax + t * dx), p.y - (ay + t * dy)));
            }
        return best;
    };
    for (const MedialEdge& e : edges)
        for (int k = 0; k < 2; ++k) {
            CHECK_THAT(std::hypot(e.a.x - e.support_a[k].x, e.a.y - e.support_a[k].y), WithinAbs(e.radius_a, 1e-6));
            CHECK_THAT(std::hypot(e.b.x - e.support_b[k].x, e.b.y - e.support_b[k].y), WithinAbs(e.radius_b, 1e-6));
            CHECK(on_boundary(e.support_a[k]) < 1e-6);
            CHECK(on_boundary(e.support_b[k]) < 1e-6);
        }
}

} // namespace

TEST_CASE("supports of a strip's middle line are straight across", "[medial]") {
    const Polygons strip = {rect(0, 0, 30, 2)};
    const auto edges = medial_axis(strip);
    check_supports(edges, strip);
    for (const MedialEdge& e : edges) {
        if (std::fabs(e.a.y - 1.0) > 1e-6 || std::fabs(e.b.y - 1.0) > 1e-6)
            continue; // a corner bisector
        for (int k = 0; k < 2; ++k) {
            CHECK_THAT(e.support_a[k].x, WithinAbs(e.a.x, 1e-6)); // feet of the perpendiculars
            CHECK((std::fabs(e.support_a[k].y) < 1e-6 || std::fabs(e.support_a[k].y - 2.0) < 1e-6));
        }
        CHECK_THAT(e.support_a[0].y + e.support_a[1].y, WithinAbs(2.0, 1e-6)); // one on each side
    }
}

TEST_CASE("supports of a wedge and of a ring are at the radius, on the boundary", "[medial]") {
    const double h = std::tan(15.0 * std::numbers::pi / 180.0) * 20.0;
    const Polygons wedge = {{{to_units(0), to_units(0)}, {to_units(20), to_units(-h)}, {to_units(20), to_units(h)}}};
    check_supports(medial_axis(wedge), wedge);
    const Polygons ring = {rect(0, 0, 10, 10), rect(3, 3, 4, 4, true)};
    const auto edges = medial_axis(ring);
    check_supports(edges, ring);
    // On a parabolic piece one support is a corner of the hole.
    bool corner_support = false;
    for (const MedialEdge& e : edges)
        if (e.curved)
            for (int k = 0; k < 2; ++k)
                for (const Point2d c : {Point2d{3, 3}, Point2d{7, 3}, Point2d{3, 7}, Point2d{7, 7}})
                    corner_support = corner_support || std::hypot(e.support_a[k].x - c.x, e.support_a[k].y - c.y) < 1e-6;
    CHECK(corner_support);
}

TEST_CASE("every corner of a regular 64-gon starts a spoke, and the graph is a tree", "[medial]") {
    // The Voronoi vertex at a convex corner is computed in floating point and
    // may land a hair outside the region; the spoke must be kept anyway.
    Polygon poly;
    for (int i = 0; i < 64; ++i) {
        const double a = 2.0 * std::numbers::pi * i / 64;
        poly.push_back({to_units(5.0 * std::cos(a)), to_units(5.0 * std::sin(a))});
    }
    const auto edges = medial_axis({poly});
    for (const auto& c : poly)
        CHECK(has_point(edges, double(c.x) / kUnitsPerMm, double(c.y) / kUnitsPerMm));
    const MedialGraph g = medial_graph({poly});
    CHECK(g.edges.size() + 1 == g.nodes.size());
}

TEST_CASE("the bisector of two corners comes in short pieces with exact radii", "[medial]") {
    // A thin fin on a wide body: below the fin's base the axis runs between the
    // two reflex corners (8, 6) and (9, 6), where R = sqrt(0.25 + (6 - y)^2).
    const std::vector<Point2d> corners = {{0, 0}, {20, 0}, {20, 6}, {9, 6}, {9, 14}, {8, 14}, {8, 6}, {0, 6}};
    Polygon poly;
    for (const Point2d& c : corners)
        poly.push_back({to_units(c.x), to_units(c.y)});
    std::size_t pieces = 0;
    for (const MedialEdge& e : medial_axis({poly})) {
        const bool on_bisector = std::fabs(e.a.x - 8.5) < 1e-9 && std::fabs(e.b.x - 8.5) < 1e-9 && e.a.y < 6.0 + 1e-9 &&
                                 e.b.y < 6.0 + 1e-9 && e.a.y > 3.1 && e.b.y > 3.1;
        if (!on_bisector)
            continue;
        ++pieces;
        CHECK(std::hypot(e.b.x - e.a.x, e.b.y - e.a.y) <= kMaxCurvedPiece + 1e-9);
        CHECK_THAT(e.radius_a, WithinAbs(std::sqrt(0.25 + (6 - e.a.y) * (6 - e.a.y)), 1e-9));
        CHECK_THAT(e.radius_b, WithinAbs(std::sqrt(0.25 + (6 - e.b.y) * (6 - e.b.y)), 1e-9));
    }
    CHECK(pieces >= 13); // about 2.9 mm of bisector
}

TEST_CASE("degenerate input: a hair-thin channel between two blobs", "[medial]") {
    // A leftover of gap filling: two round blobs joined by a channel 0.2 um
    // wide. It used to subdivide one bisector into a billion pieces.
    const Polygon blobs = {
        {7544997, 1200953}, {7525582, 1203833}, {7506543, 1208602}, {7488063, 1215214},
        {7470321, 1223606}, {7453486, 1233696}, {7437721, 1245388}, {7423179, 1258569},
        {7409998, 1273111}, {7398306, 1288876}, {7388216, 1305711}, {7379824, 1323453},
        {7373212, 1341933}, {7368443, 1360972}, {7365563, 1380387}, {7364600, 1399990},
        {7365563, 1419593}, {7368443, 1439008}, {7373212, 1458047}, {7379824, 1476527},
        {7388216, 1494269}, {7398306, 1511104}, {7409998, 1526869}, {7423179, 1541411},
        {7437721, 1554592}, {7453486, 1566284}, {7464707, 1573009}, {7464600, 1573195},
        {8500010, 2170989}, {8500117, 2170803}, {8505731, 2174168}, {8523473, 2182560},
        {8541953, 2189172}, {8560992, 2193941}, {8580407, 2196821}, {8600010, 2197784},
        {8619613, 2196821}, {8639028, 2193941}, {8658067, 2189172}, {8676547, 2182560},
        {8694289, 2174168}, {8711124, 2164078}, {8726889, 2152386}, {8741431, 2139205},
        {8754612, 2124663}, {8766304, 2108898}, {8776394, 2092063}, {8784786, 2074321},
        {8791398, 2055841}, {8796167, 2036802}, {8799047, 2017387}, {8800000, 1997987},
        {8800000, 2459460}, {8799047, 2440061}, {8796167, 2420646}, {8791398, 2401607},
        {8784786, 2383127}, {8776394, 2365385}, {8766304, 2348550}, {8754612, 2332785},
        {8741431, 2318243}, {8726889, 2305062}, {8711124, 2293370}, {8699902, 2286644},
        {8700010, 2286459}, {6864600, 1226785}, {6864492, 1226970}, {6858879, 1223606},
        {6841137, 1215214}, {6822657, 1208602}, {6803618, 1203833}, {6784203, 1200953},
        {6764804, 1200000}, {7564396, 1200000},
    };
    const auto edges = medial_axis({blobs});
    CHECK_FALSE(edges.empty());
    CHECK(edges.size() < 100000);
}

namespace {

// The own diagram's medial axis and Boost's: every end of one has an end of
// the other within 1e-6 mm (both ways), and the same total length and radius
// integrated along the axis. (Not the same count of pieces: Boost's exact
// arithmetic keeps edges shorter than a nanometre that the own diagram,
// in floating point, merges into one node.)
void compare_with_reference(const Polygons& region) {
    const auto own = medial_axis(region);
    const auto ref = medial_axis_reference(region);
    REQUIRE_FALSE(ref.empty());
    auto total = [](const std::vector<MedialEdge>& edges) {
        double length = 0.0, radii = 0.0;
        for (const MedialEdge& e : edges) {
            const double l = std::hypot(e.b.x - e.a.x, e.b.y - e.a.y);
            length += l;
            radii += l * (e.radius_a + e.radius_b) / 2.0;
        }
        return std::make_pair(length, radii);
    };
    const auto [own_length, own_radii] = total(own);
    const auto [ref_length, ref_radii] = total(ref);
    CHECK_THAT(own_length, WithinAbs(ref_length, 1e-6 * ref_length));
    CHECK_THAT(own_radii, WithinAbs(ref_radii, 1e-6 * ref_radii));
    auto covered = [](const std::vector<MedialEdge>& a, const std::vector<MedialEdge>& b) {
        for (const MedialEdge& e : a)
            for (const Point2d& p : {e.a, e.b})
                if (!has_point(b, p.x, p.y))
                    return false;
        return true;
    };
    CHECK(covered(own, ref));
    CHECK(covered(ref, own));
}

} // namespace

TEST_CASE("the own Voronoi diagram gives Boost's medial axis", "[medial]") {
    compare_with_reference({rect(0, 0, 10, 10)});
    compare_with_reference({rect(0, 0, 30, 2)});
    compare_with_reference({rect(-10, -10, 20, 20), rect(-3, -3, 6, 6, true)});
    Polygon circle, star, wavy;
    for (int i = 0; i < 64; ++i) {
        const double a = 2.0 * std::numbers::pi * i / 64;
        circle.push_back({to_units(5.0 * std::cos(a)), to_units(5.0 * std::sin(a))});
        const double r = i % 2 == 0 ? 5.0 : 2.0;
        star.push_back({to_units(r * std::cos(a)), to_units(r * std::sin(a))});
    }
    for (int i = 0; i < 400; ++i) {
        const double a = 2.0 * std::numbers::pi * i / 400, r = 20.0 * (1.0 + 0.2 * std::sin(7.0 * a));
        wavy.push_back({to_units(r * std::cos(a)), to_units(r * std::sin(a))});
    }
    compare_with_reference({circle});
    compare_with_reference({star});
    compare_with_reference({wavy});
    // An L and a wedge 0.3 to 1.2 mm thick.
    const std::int64_t o = 0;
    const Polygon l = {{o, o}, {to_units(10), o}, {to_units(10), to_units(4)}, {to_units(4), to_units(4)},
                       {to_units(4), to_units(10)}, {o, to_units(10)}};
    const Polygon wedge = {{o, o}, {to_units(30), o}, {to_units(30), to_units(1.2)}, {o, to_units(0.3)}};
    compare_with_reference({l});
    compare_with_reference({wedge});
}

TEST_CASE("medial axis of a contour with nanometre zig-zags at its corners", "[medial]") {
    // A layer of the fuzz test's sphere offset by -0.00013 mm (2026-10-25):
    // 1 nm edges that turn back at its corners made the diagram throw.
    const Polygon zigzag{
    {5450060, -1866526}, {5898183, -1822389}, {6342463, -1749038}, {6781004, -1646785}, {7211904, -1516073},
    {7633348, -1357453}, {8043500, -1171615}, {8440634, -959343}, {8823019, -721560}, {9189041, -459272},
    {9537132, -173601}, {9865788, 134218}, {10173601, 462867}, {10173601, 462868}, {10459272, 810959},
    {10721560, 1176981}, {10959343, 1559365}, {10959343, 1559366}, {11171615, 1956500}, {11357452, 2366651},
    {11357452, 2366652}, {11516072, 2788096}, {11646785, 3218995}, {11646785, 3218996}, {11749039, 3657537},
    {11822390, 4101820}, {11866525, 4549939}, {11866525, 4549940}, {11881260, 5000004}, {11866525, 5450060},
    {11822390, 5898180}, {11749039, 6342463}, {11646785, 6781004}, {11516072, 7211904}, {11357452, 7633348},
    {11171615, 8043500}, {10959342, 8440634}, {10721560, 8823019}, {10459272, 9189041}, {10173601, 9537132},
    {9865788, 9865782}, {9537132, 10173601}, {9189040, 10459272}, {8823019, 10721560}, {8440634, 10959343},
    {8043500, 11171615}, {7633348, 11357452}, {7211904, 11516072}, {6781004, 11646785}, {6342463, 11749039},
    {5898183, 11822389}, {5450060, 11866525}, {5000000, 11881259}, {4549940, 11866525}, {4549939, 11866525},
    {4101816, 11822389}, {3657536, 11749039}, {3657537, 11749039}, {3218996, 11646785}, {3218995, 11646785},
    {2788095, 11516072}, {2788096, 11516072}, {2366652, 11357452}, {2366651, 11357452}, {1956500, 11171615},
    {1559366, 10959343}, {1176980, 10721560}, {1176981, 10721560}, {810959, 10459272}, {462868, 10173601},
    {134212, 9865782}, {-173601, 9537132}, {-459272, 9189041}, {-721560, 8823019}, {-959343, 8440634},
    {-1171615, 8043500}, {-1357453, 7633348}, {-1516073, 7211904}, {-1646785, 6781004}, {-1749038, 6342463},
    {-1822390, 5898180}, {-1866526, 5450060}, {-1881259, 5000004}, {-1866526, 4549940}, {-1866526, 4549939},
    {-1822390, 4101820}, {-1749038, 3657537}, {-1646785, 3218996}, {-1646785, 3218995}, {-1516073, 2788096},
    {-1357453, 2366652}, {-1357453, 2366651}, {-1171615, 1956500}, {-959343, 1559366}, {-959343, 1559365},
    {-721560, 1176981}, {-459272, 810959}, {-173601, 462868}, {-173601, 462867}, {134212, 134218}, {462868, -173601},
    {810959, -459272}, {810958, -459272}, {1176981, -721560}, {1176980, -721560}, {1559366, -959343},
    {1956500, -1171615}, {1956499, -1171615}, {2366651, -1357453}, {2366652, -1357453}, {2788096, -1516073},
    {2788095, -1516073}, {3218995, -1646785}, {3218996, -1646785}, {3657537, -1749038}, {3657536, -1749038},
    {4101816, -1822389}, {4549939, -1866526}, {4549940, -1866526}, {5000000, -1881258}};
    std::vector<MedialEdge> edges;
    REQUIRE_NOTHROW(edges = medial_axis({zigzag}));
    CHECK(!edges.empty());
    CHECK(edges.size() < 5000); // no runaway of pieces
    const MedialGraph g = medial_graph({zigzag});
    CHECK(g.nodes.size() < 5000);
}
