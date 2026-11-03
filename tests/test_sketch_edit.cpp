// Editing a sketch (cad_design.md, rule 17; CD17, CD18).

#include "cad/constraints.hpp"
#include "cad/sketch_edit.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <numbers>
#include <random>

using namespace tisma::cad;
using Catch::Matchers::WithinAbs;

namespace {

Entity line(Pt a, Pt b) {
    Entity e;
    e.kind = Entity::Kind::Chain;
    e.points = {a, b};
    return e;
}

Entity circle(Pt c, double r) {
    Entity e;
    e.kind = Entity::Kind::Circle;
    e.points = {c};
    e.radius = r;
    return e;
}

Entity rectangle(Pt a, Pt b) {
    Entity e;
    e.kind = Entity::Kind::Rectangle;
    e.points = {a, b};
    return e;
}

bool near(const Pt& a, const Pt& b, double tol = 1e-9) { return std::hypot(a.x - b.x, a.y - b.y) <= tol; }

} // namespace

TEST_CASE("sketch edit: CD17, TR on one end of two crossing lines cuts it at the crossing", "[cad][sketch_edit]") {
    Sketch s;
    s.entities = {line({0, 0}, {20, 20}), line({0, 20}, {20, 0})};
    s.rules.push_back({GeoConstraint::Kind::Length, {0, 0}, {}, 28.28, {}});
    REQUIRE(trim_at(s, {17, 17}, 1.0));
    REQUIRE(s.entities.size() == 2);
    const Entity& e = s.entities[0];
    REQUIRE(e.points.size() == 2);
    CHECK(near(e.points[0], {0, 0}));
    CHECK(near(e.points[1], {10, 10}, 1e-9));
    CHECK(s.entities[1] == line({0, 20}, {20, 0})); // the other line stays
    CHECK(s.rules.empty());                          // the length no longer holds: dropped
    // The picked end of the other line, near its start, goes the same way.
    REQUIRE(trim_at(s, {2, 18}, 1.0));
    CHECK(near(s.entities[1].points[0], {10, 10}));
    CHECK(near(s.entities[1].points[1], {20, 0}));
    // Nothing near: nothing changes.
    const Sketch before = s;
    CHECK_FALSE(trim_at(s, {50, 50}, 1.0));
    CHECK(s == before);
}

TEST_CASE("sketch edit: trims in the middle, on rectangles and circles", "[cad][sketch_edit]") {
    SECTION("the middle of a line between two crossings splits it in two") {
        Sketch s;
        s.entities = {line({0, 0}, {30, 0}), line({10, -5}, {10, 5}), line({20, -5}, {20, 5})};
        REQUIRE(trim_at(s, {15, 0.2}, 1.0));
        REQUIRE(s.entities.size() == 4);
        CHECK(near(s.entities[0].points.back(), {10, 0}));
        CHECK(near(s.entities[3].points.front(), {20, 0}));
        CHECK(near(s.entities[3].points.back(), {30, 0}));
    }
    SECTION("a line nothing crosses goes whole, and the rules after it shift") {
        Sketch s;
        s.entities = {line({0, 0}, {10, 0}), circle({50, 50}, 5)};
        s.rules.push_back({GeoConstraint::Kind::Radius, {1, 0}, {}, 5.0, {}});
        REQUIRE(trim_at(s, {5, 0}, 1.0));
        REQUIRE(s.entities.size() == 1);
        REQUIRE(s.rules.size() == 1);
        CHECK(s.rules[0].a.entity == 0);
    }
    SECTION("a rectangle side cut by a line opens it into a chain") {
        Sketch s;
        s.entities = {rectangle({0, 0}, {20, 10}), line({5, -5}, {5, 15})};
        REQUIRE(trim_at(s, {2, 0}, 0.5)); // the bottom side, left of the line
        const Entity& e = s.entities[0];
        REQUIRE(e.kind == Entity::Kind::Chain);
        CHECK_FALSE(e.closed);
        CHECK(near(e.points.front(), {5, 0}));
        CHECK(near(e.points.back(), {0, 0}));
        CHECK(e.points.size() == 5);
    }
    SECTION("a circle cut by a line keeps the far arc") {
        Sketch s;
        s.entities = {circle({0, 0}, 10), line({6, -20}, {6, 20})};
        REQUIRE(trim_at(s, {10, 0}, 0.5));
        const Entity& e = s.entities[0];
        REQUIRE(e.kind == Entity::Kind::Arc);
        CHECK(near(e.points[1], {6, 8}, 1e-9));
        CHECK(near(e.points[2], {6, -8}, 1e-9));
        // The arc left is the long one, through (-10, 0): trimming it there
        // again needs two crossings inside it, and there are none: it goes.
        REQUIRE(trim_at(s, {-10, 0}, 0.5));
        CHECK(s.entities.size() == 1);
    }
}

TEST_CASE("sketch edit: EX takes an end to the nearest curve ahead", "[cad][sketch_edit]") {
    Sketch s;
    s.entities = {line({0, 0}, {5, 0}), line({10, -5}, {10, 5}), line({20, -5}, {20, 5})};
    REQUIRE(extend_at(s, {4.5, 0}, 1.0));
    CHECK(near(s.entities[0].points[1], {10, 0}));
    REQUIRE(extend_at(s, {9.5, 0}, 1.0));
    CHECK(near(s.entities[0].points[1], {20, 0}));
    CHECK_FALSE(extend_at(s, {19.5, 0}, 1.0)); // nothing ahead
    CHECK(near(s.entities[0].points[1], {20, 0}));
    // Ahead to a circle.
    Sketch c;
    c.entities = {line({0, 0}, {2, 0}), circle({20, 0}, 5)};
    REQUIRE(extend_at(c, {1.9, 0}, 0.5));
    CHECK(near(c.entities[0].points[1], {15, 0}, 1e-9));
}

TEST_CASE("sketch edit: O offsets circles, rectangles and chains to the picked side", "[cad][sketch_edit]") {
    Sketch s;
    s.entities = {circle({0, 0}, 10), rectangle({0, 0}, {20, 10})};
    const auto out = offset_entity(s, 0, 2, {20, 0});
    REQUIRE(out);
    CHECK(s.entities[*out].radius == 12.0);
    const auto in = offset_entity(s, 0, 2, {1, 0});
    REQUIRE(in);
    CHECK(s.entities[*in].radius == 8.0);
    CHECK_FALSE(offset_entity(s, 0, 10, {1, 0})); // through zero
    const auto grown = offset_entity(s, 1, 1, {30, 30});
    REQUIRE(grown);
    CHECK(near(point_of(s.entities[*grown], 0), {-1, -1}));
    CHECK(near(point_of(s.entities[*grown], 2), {21, 11}));
    const auto shrunk = offset_entity(s, 1, 1, {10, 5});
    REQUIRE(shrunk);
    CHECK(near(point_of(s.entities[*shrunk], 0), {1, 1}));
    // An L of two lines, offset 1 to its inside: the corner mitred.
    Sketch l;
    Entity chain = line({0, 0}, {10, 0});
    chain.points.push_back({10, 10});
    l.entities = {chain};
    const auto inner = offset_entity(l, 0, 1, {5, 3});
    REQUIRE(inner);
    const Entity& e = l.entities[*inner];
    REQUIRE(e.points.size() == 3);
    CHECK(near(e.points[0], {0, 1}));
    CHECK(near(e.points[1], {9, 1}));
    CHECK(near(e.points[2], {9, 10}));
    // A closed square offset out: still a square, 2 bigger each side.
    Sketch q;
    Entity sq = line({0, 0}, {10, 0});
    sq.points.push_back({10, 10});
    sq.points.push_back({0, 10});
    sq.closed = true;
    q.entities = {sq};
    const auto big = offset_entity(q, 0, 2, {-5, 5});
    REQUIRE(big);
    CHECK(near(q.entities[*big].points[0], {-2, -2}));
    CHECK(near(q.entities[*big].points[2], {12, 12}));
}

TEST_CASE("sketch edit: CHA cuts a corner and the region stays closed", "[cad][sketch_edit]") {
    Sketch s;
    s.entities = {rectangle({0, 0}, {20, 10})};
    REQUIRE(chamfer_corner(s, 0, 2, 3));
    const Entity& e = s.entities[0];
    REQUIRE(e.kind == Entity::Kind::Chain);
    CHECK(e.closed);
    REQUIRE(e.points.size() == 5);
    CHECK(near(e.points[2], {20, 7}));
    CHECK(near(e.points[3], {17, 10}));
    CHECK(s.has_region());
    CHECK_FALSE(chamfer_corner(s, 0, 0, 30)); // longer than its sides
    Sketch open;
    open.entities = {line({0, 0}, {10, 0})};
    CHECK_FALSE(chamfer_corner(open, 0, 0, 1)); // an open end is no corner
}

TEST_CASE("sketch edit: CD18, a polar pattern of a circle, 6 in 360 degrees", "[cad][sketch_edit]") {
    Sketch s;
    s.entities = {circle({30, 0}, 4)};
    const auto made = pattern_polar(s, {0}, {0, 0}, 6);
    REQUIRE(made.size() == 5);
    REQUIRE(s.entities.size() == 6);
    for (std::size_t k = 0; k < 6; ++k) {
        const double a = double(k) * std::numbers::pi / 3.0;
        CHECK(near(s.entities[k].points[0], {30 * std::cos(a), 30 * std::sin(a)}, 1e-9));
        CHECK(s.entities[k].radius == 4.0);
    }
    // Over a part of a turn the last copy sits at its end.
    Sketch p;
    p.entities = {circle({10, 0}, 1)};
    pattern_polar(p, {0}, {0, 0}, 3, 90);
    CHECK(near(p.entities[1].points[0], {10 * std::cos(std::numbers::pi / 4), 10 * std::sin(std::numbers::pi / 4)}, 1e-9));
    CHECK(near(p.entities[2].points[0], {0, 10}, 1e-9));
    CHECK(pattern_polar(p, {0}, {0, 0}, 1).empty());
}

TEST_CASE("sketch edit: rectangular pattern, mirror, move, copy, rotate, scale", "[cad][sketch_edit]") {
    Sketch s;
    s.entities = {rectangle({0, 0}, {4, 2})};
    const auto grid = pattern_rectangular(s, {0}, 3, 10, 2, 5);
    CHECK(grid.size() == 5);
    CHECK(near(point_of(s.entities.back(), 0), {20, 5}));

    Sketch m;
    Entity arc;
    arc.kind = Entity::Kind::Arc;
    arc.points = {{10, 0}, {15, 0}, {10, 5}};
    m.entities = {arc, rectangle({1, 1}, {3, 2})};
    const auto mirrored_ones = mirror_entities(m, {0, 1}, {0, 0}, {0, 1});
    REQUIRE(mirrored_ones.size() == 2);
    const Entity& ma = m.entities[mirrored_ones[0]];
    CHECK(near(ma.points[0], {-10, 0}));
    CHECK(near(ma.points[1], {-15, 0}));
    CHECK(ma.clockwise); // a mirror turns it
    CHECK(m.entities[mirrored_ones[1]].kind == Entity::Kind::Rectangle);
    CHECK(near(point_of(m.entities[mirrored_ones[1]], 0), {-3, 1}));
    const auto replaced = mirror_entities(m, {0}, {0, 0}, {1, 0}, true);
    CHECK(m.entities.size() == 4);
    CHECK(near(m.entities[replaced[0]].points[2], {10, -5}));

    Sketch t;
    t.entities = {rectangle({0, 0}, {10, 4}), circle({0, 0}, 2)};
    t.rules.push_back({GeoConstraint::Kind::Fixed, {1, 0}, {}, 0.0, {0, 0}});
    t.rules.push_back({GeoConstraint::Kind::Radius, {1, 0}, {}, 2.0, {}});
    map_entities(t, {1}, moved(5, 5), false);
    CHECK(near(t.entities[1].points[0], {5, 5}));
    REQUIRE(t.rules.size() == 1); // the radius holds; the fixed point does not
    CHECK(t.rules[0].kind == GeoConstraint::Kind::Radius);
    map_entities(t, {0}, rotated({0, 0}, 90), false);
    CHECK(t.entities[0].kind == Entity::Kind::Rectangle); // a quarter turn keeps it square to the axes
    CHECK(near(point_of(t.entities[0], 0), {-4, 0}));
    map_entities(t, {0}, rotated({0, 0}, 30), false);
    CHECK(t.entities[0].kind == Entity::Kind::Chain);
    CHECK(t.entities[0].closed);
    map_entities(t, {1}, scaled({5, 5}, 3), false);
    CHECK(t.entities[1].radius == 6.0);
    const auto copies = map_entities(t, {1}, moved(10, 0), true);
    REQUIRE(copies.size() == 1);
    CHECK(t.entities.size() == 3);
    CHECK(map_entities(t, {1}, scaled({0, 0}, 0), false).empty());
    // A whole sketch of rectangles moved together stays solvable.
    CHECK(solve_sketch(t).solved);
}

TEST_CASE("sketch edit: window and crossing selection", "[cad][sketch_edit]") {
    Sketch s;
    s.entities = {line({1, 1}, {4, 4}), line({3, 3}, {12, 3}), circle({20, 20}, 2), rectangle({-5, -5}, {-1, -1})};
    // Left to right: only what is wholly inside.
    CHECK(select_box(s, {0, 0}, {10, 10}) == std::vector<std::size_t>{0});
    // Right to left: everything it touches, a line crossing it without an end inside too.
    CHECK(select_box(s, {10, 10}, {0, 0}) == std::vector<std::size_t>{0, 1});
    CHECK(select_box(s, {5, 2}, {6, 4}) == std::vector<std::size_t>{});
    CHECK(select_box(s, {6, 4}, {5, 2}) == std::vector<std::size_t>{1});
    CHECK(select_box(s, {23, 23}, {17, 17}) == std::vector<std::size_t>{2});
    CHECK(select_box(s, {22.5, 20.5}, {21.8, 19.5}) == std::vector<std::size_t>{2}); // the circle's edge
    CHECK(select_box(s, {-6, -6}, {0, 0}) == std::vector<std::size_t>{3});
}

TEST_CASE("sketch edit: random edits never leave a rule on a stale entity", "[cad][sketch_edit]") {
    std::mt19937 rng(17);
    std::uniform_real_distribution<double> u(-20, 20);
    for (int round = 0; round < 300; ++round) {
        Sketch s;
        for (int k = 0; k < 5; ++k) {
            switch (rng() % 3) {
            case 0: s.entities.push_back(line({u(rng), u(rng)}, {u(rng), u(rng)})); break;
            case 1: s.entities.push_back(circle({u(rng), u(rng)}, 1 + std::abs(u(rng)) / 4)); break;
            default: s.entities.push_back(rectangle({u(rng), u(rng)}, {u(rng), u(rng)})); break;
            }
        }
        for (std::size_t k = 0; k < s.entities.size(); ++k)
            if (s.entities[k].kind == Entity::Kind::Circle)
                s.rules.push_back({GeoConstraint::Kind::Radius, {k, 0}, {}, s.entities[k].radius, {}});
        for (int step = 0; step < 6; ++step) {
            const Pt p{u(rng), u(rng)};
            switch (rng() % 5) {
            case 0: trim_at(s, p, 3.0); break;
            case 1: extend_at(s, p, 3.0); break;
            case 2:
                if (!s.entities.empty())
                    offset_entity(s, rng() % s.entities.size(), 1.0, p);
                break;
            case 3:
                if (!s.entities.empty())
                    map_entities(s, {rng() % s.entities.size()}, rotated(p, u(rng)), rng() % 2 == 0);
                break;
            default:
                if (!s.entities.empty())
                    chamfer_corner(s, rng() % s.entities.size(), rng() % 4, 0.5);
                break;
            }
            for (const Entity& e : s.entities) {
                for (const Pt& q : e.points)
                    REQUIRE((std::isfinite(q.x) && std::isfinite(q.y)));
                if (e.kind == Entity::Kind::Chain)
                    REQUIRE(e.points.size() >= 2);
            }
            for (const GeoConstraint& r : s.rules) {
                REQUIRE(r.a.entity < s.entities.size());
                REQUIRE(s.entities[r.a.entity].kind == Entity::Kind::Circle);
                REQUIRE(std::abs(s.entities[r.a.entity].radius - r.value) < 1e-9);
            }
        }
    }
}

namespace {
double area(const std::vector<Pt>& q) {
    double a = 0;
    for (std::size_t i = 0; i < q.size(); ++i)
        a += q[i].x * q[(i + 1) % q.size()].y - q[(i + 1) % q.size()].x * q[i].y;
    return std::abs(a) / 2;
}
} // namespace

TEST_CASE("sketch edit: F rounds a corner and the outline stays a region (rule 6)", "[cad][sketch_edit]") {
    Sketch s;
    s.entities = {rectangle({0, 0}, {20, 10})};
    REQUIRE(fillet_corner(s, 0, 2, 3)); // the top right corner
    REQUIRE(s.entities.size() == 2);
    const Entity& arc = s.entities[0];
    REQUIRE(arc.kind == Entity::Kind::Arc);
    CHECK(near(arc.points[0], {17, 7}, 1e-9));
    CHECK(near(arc.points[1], {20, 7}, 1e-9));
    CHECK(near(arc.points[2], {17, 10}, 1e-9));
    CHECK_FALSE(arc.clockwise); // the rectangle runs counter-clockwise
    CHECK(s.has_region());
    const auto loops = joined_loops(s);
    REQUIRE(loops.size() == 1);
    CHECK_THAT(area(loops[0]), WithinAbs(200 - (9 - 9 * std::numbers::pi / 4), 0.01));
    // Another corner of what is now an open chain, then the radius too big.
    const std::size_t chain = 1;
    REQUIRE(s.entities[chain].kind == Entity::Kind::Chain);
    REQUIRE(fillet_corner(s, chain, 1, 2));
    CHECK(joined_loops(s).size() == 1);
    CHECK_THAT(area(joined_loops(s)[0]), WithinAbs(200 - (9 + 4) * (1 - std::numbers::pi / 4), 0.01));
    Sketch big;
    big.entities = {rectangle({0, 0}, {20, 10})};
    CHECK_FALSE(fillet_corner(big, 0, 0, 11));
    // A clockwise outline gets a clockwise arc.
    Sketch cw;
    Entity tri = line({0, 0}, {0, 10});
    tri.points.push_back({10, 0});
    tri.closed = true;
    cw.entities = {tri};
    REQUIRE(fillet_corner(cw, 0, 1, 1));
    CHECK(cw.entities[0].clockwise);
    CHECK(cw.has_region());
}

TEST_CASE("sketch edit: open pieces drawn end to end close a region", "[cad][sketch_edit]") {
    Sketch s;
    Entity a = line({0, 0}, {10, 0});
    a.points.push_back({10, 10});
    s.entities = {a, line({0, 10}, {10, 10})};
    CHECK_FALSE(s.has_region());
    s.entities.push_back(line({0, 0}, {0, 10})); // drawn the other way round
    CHECK(s.has_region());
    REQUIRE(joined_loops(s).size() == 1);
    CHECK_THAT(area(joined_loops(s)[0]), WithinAbs(100, 1e-9));
    s.entities[2].construction = true;
    CHECK_FALSE(s.has_region());
}
