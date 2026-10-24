#include "gcode/travel.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <functional>

using namespace tisma;
using Catch::Matchers::WithinAbs;

namespace {

Polygon ring(std::initializer_list<Point2d> points) {
    Polygon p;
    for (const Point2d& q : points)
        p.push_back({to_units(q.x), to_units(q.y)});
    return p;
}

// A U open at the top: arms x in [0, 10] and [20, 30] for y in [10, 30], joined below y = 10.
Polygons u_shape() {
    return {ring({{0, 0}, {30, 0}, {30, 30}, {20, 30}, {20, 10}, {10, 10}, {10, 30}, {0, 30}})};
}

// Samples every segment of the plan and checks the predicate on each sample.
bool all_along(Point2d from, const TravelPlan& plan, const std::function<bool(Point2d)>& ok) {
    for (const Point2d& to : plan.points) {
        for (int k = 0; k <= 50; ++k) {
            const double t = k / 50.0;
            if (!ok({from.x + t * (to.x - from.x), from.y + t * (to.y - from.y)}))
                return false;
        }
        from = to;
    }
    return true;
}

} // namespace

TEST_CASE("a travel inside a square goes straight", "[travel]") {
    const TravelPlanner planner({ring({{0, 0}, {20, 0}, {20, 20}, {0, 20}})}, 0.4);
    const TravelPlan plan = planner.plan({2, 2}, {18, 15});
    REQUIRE(plan.points.size() == 1);
    CHECK_FALSE(plan.leaves_part);
    CHECK_THAT(plan.length, WithinAbs(std::hypot(16.0, 13.0), 1e-9));
}

TEST_CASE("a travel between the arms of a U goes round the bottom", "[travel]") {
    const TravelPlanner planner(u_shape(), 0.4);
    const Point2d from{5, 25}, to{25, 25};
    const TravelPlan plan = planner.plan(from, to);
    CHECK_FALSE(plan.leaves_part);
    REQUIRE(plan.points.size() >= 3);
    CHECK(plan.points.back().x == to.x);
    CHECK(plan.points.back().y == to.y);
    // Never in the gap between the arms, and at least one bead from the walls.
    CHECK(all_along(from, plan, [](Point2d p) {
        const bool in_gap = p.x > 9.6 - 1e-6 && p.x < 20.4 + 1e-6 && p.y > 9.6 + 1e-6;
        const bool in_part = p.x > 0.4 - 1e-6 && p.x < 29.6 + 1e-6 && p.y > 0.4 - 1e-6 && p.y < 29.6 + 1e-6;
        return in_part && !in_gap;
    }));
    // The shortest way: to the corners of the gap's bottom, one bead off the walls.
    const double shortest = 2.0 * std::hypot(4.6, 15.4) + 10.8;
    CHECK(plan.length >= shortest - 1e-6);
    CHECK(plan.length < shortest + 0.1);
}

TEST_CASE("a travel from the outer wall hops inside and comes back out", "[travel]") {
    const TravelPlanner planner(u_shape(), 0.4);
    const Point2d from{0.2, 25}, to{29.8, 25}; // on the outer wall's centre line
    const TravelPlan plan = planner.plan(from, to);
    CHECK_FALSE(plan.leaves_part);
    CHECK(plan.points.back().x == to.x);
    CHECK(all_along(from, plan, [](Point2d p) { return !(p.x > 9.6 && p.x < 20.4 && p.y > 9.6 + 1e-6); }));
    // Two hops of 0.2 onto the travel region, then the shortest way round inside it.
    CHECK(plan.length < 0.4 + 2.0 * std::hypot(9.2, 15.4) + 10.8 + 0.05);
}

TEST_CASE("a travel past a hole goes round it the shorter way", "[travel]") {
    const Polygons region{ring({{0, 0}, {30, 0}, {30, 30}, {0, 30}}), ring({{10, 10}, {10, 18}, {20, 18}, {20, 10}})};
    const TravelPlanner planner(region, 0.4);
    const Point2d from{5, 12}, to{25, 12};
    const TravelPlan plan = planner.plan(from, to);
    CHECK_FALSE(plan.leaves_part);
    CHECK(all_along(from, plan, [](Point2d p) {
        return !(p.x > 9.6 + 1e-6 && p.x < 20.4 - 1e-6 && p.y > 9.6 + 1e-6 && p.y < 18.4 - 1e-6);
    }));
    // Round the bottom (y = 9.6) is shorter than round the top (y = 18.4).
    const double below = 2.0 * std::hypot(4.6, 2.4) + 10.8;
    CHECK(plan.length < below + 0.1);
    for (const Point2d& p : plan.points)
        CHECK(p.y < 12.0 + 1e-9);
}

TEST_CASE("a travel between two separate parts leaves the part even when short", "[travel]") {
    const Polygons region{ring({{0, 0}, {10, 0}, {10, 10}, {0, 10}}), ring({{10.5, 0}, {20.5, 0}, {20.5, 10}, {10.5, 10}})};
    const TravelPlanner planner(region, 0.4);
    const TravelPlan plan = planner.plan({9.5, 5}, {11, 5});
    CHECK(plan.leaves_part);
    REQUIRE(plan.points.size() == 1);
    CHECK_THAT(plan.length, WithinAbs(1.5, 1e-9));
    // Within one part it stays inside.
    CHECK_FALSE(planner.plan({1, 1}, {9, 9}).leaves_part);
}

TEST_CASE("a travel into an island inside a hole leaves the part", "[travel]") {
    const Polygons region{ring({{0, 0}, {30, 0}, {30, 30}, {0, 30}}), ring({{5, 5}, {5, 25}, {25, 25}, {25, 5}}),
                          ring({{12, 12}, {18, 12}, {18, 18}, {12, 18}})};
    const TravelPlanner planner(region, 0.4);
    CHECK(planner.plan({2, 15}, {15, 15}).leaves_part);
    CHECK_FALSE(planner.plan({13, 13}, {17, 17}).leaves_part);
    CHECK_FALSE(planner.plan({2, 2}, {28, 28}).leaves_part);
}
