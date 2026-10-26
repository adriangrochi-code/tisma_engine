#include "gcode/cooling.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace tisma;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

TEST_CASE("a layer slower than the minimum time is unchanged", "[cooling]") {
    const std::map<double, double> extruded{{60.0, 600.0}}; // 10 s
    const Slowdown s = plan_slowdown(extruded, 1.0, 5.0, 10.0);
    CHECK(s.stage == 0);
    CHECK(s.feed(60.0) == 60.0);
    CHECK(plan_slowdown({}, 0.5, 5.0, 10.0).stage == 0); // nothing to slow down
}

TEST_CASE("stage 1 slows every extrusion by one factor, not below the minimum speed", "[cooling]") {
    const std::map<double, double> extruded{{60.0, 120.0}, {30.0, 30.0}, {8.0, 4.0}}; // 2 + 1 + 0.5 s
    const Slowdown s = plan_slowdown(extruded, 0.5, 5.0, 10.0);
    CHECK(s.stage == 1);
    CHECK_THAT(layer_time(extruded, 0.5, s), WithinRel(5.0, 1e-9));
    CHECK(s.scale > 0.0);
    CHECK(s.scale < 1.0);
    CHECK_THAT(s.feed(60.0) / 60.0, WithinRel(s.scale, 1e-12)); // the same factor where above the minimum
    CHECK(s.feed(30.0) >= 10.0);
    CHECK(s.feed(8.0) == 8.0); // already slower than the minimum: stays
}

TEST_CASE("stage 2 puts every extrusion at one common speed", "[cooling]") {
    // At the minimum speed (10 mm/s) the layer takes 1 + 1 s: short of 5 s.
    const std::map<double, double> extruded{{60.0, 10.0}, {1.0, 1.0}};
    const Slowdown s = plan_slowdown(extruded, 0.0, 5.0, 10.0);
    CHECK(s.stage == 2);
    // 10 / v + 1 / 1 = 5: v = 2.5 mm/s, and the move at 1 mm/s stays.
    CHECK_THAT(s.cap, WithinRel(2.5, 1e-9));
    CHECK_THAT(s.feed(60.0), WithinRel(2.5, 1e-9));
    CHECK(s.feed(1.0) == 1.0);
    CHECK_THAT(layer_time(extruded, 0.0, s), WithinRel(5.0, 1e-9));
    // Never below 1 mm/min.
    const Slowdown slowest = plan_slowdown(extruded, 0.0, 1e9, 10.0);
    CHECK(slowest.stage == 2);
    CHECK_THAT(slowest.feed(60.0), WithinAbs(kSlowestFeed, 1e-15));
}

TEST_CASE("sparing the outer walls: the rest slows down first (docs/GLOSS.md)", "[cooling]") {
    // An outer wall of 60 mm at 30 mm/s (2 s) and 120 mm of infill at 60 mm/s
    // (2 s): 4 s, short of 5 s.
    const std::map<double, double> all{{30.0, 60.0}, {60.0, 120.0}}, outer{{30.0, 60.0}};
    const Slowdown s = plan_slowdown(all, outer, 0.0, 5.0, 10.0);
    CHECK(s.stage == 1);
    CHECK(s.spare_outer);
    CHECK(s.feed(30.0, true) == 30.0);           // the outer wall keeps its speed
    CHECK_THAT(s.feed(60.0, false), WithinRel(40.0, 1e-9)); // the infill takes the 3 s: 120 mm in 3 s
    CHECK_THAT(layer_time(all, outer, 0.0, s), WithinRel(5.0, 1e-9));
    // The same layer without sparing: both slow by one factor.
    const Slowdown plain = plan_slowdown(all, 0.0, 5.0, 10.0);
    CHECK_FALSE(plain.spare_outer);
    CHECK(plain.feed(30.0) < 30.0);
    // When the rest at the minimum speed is not enough, every extrusion slows
    // as before, the outer wall too.
    const Slowdown hard = plan_slowdown(all, outer, 0.0, 20.0, 10.0);
    CHECK_FALSE(hard.spare_outer);
    CHECK(hard.feed(30.0, true) < 30.0);
    CHECK_THAT(layer_time(all, outer, 0.0, hard), WithinRel(20.0, 1e-9));
    // A layer of outer wall only, or with no outer wall: the plain plan.
    CHECK_FALSE(plan_slowdown(outer, outer, 0.0, 5.0, 10.0).spare_outer);
    const Slowdown none = plan_slowdown(all, {}, 0.0, 5.0, 10.0);
    CHECK(none.stage == plain.stage);
    CHECK(none.scale == plain.scale);
}
