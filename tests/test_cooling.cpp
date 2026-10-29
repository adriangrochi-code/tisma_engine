#include "gcode/cooling.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace tisma;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

TEST_CASE("a layer slower than the minimum time is unchanged", "[cooling]") {
    const std::map<double, double> extruded{{60.0, 600.0}}; // 10 s
    const Slowdown s = plan_slowdown(extruded, {}, 1.0, 5.0, 10.0);
    CHECK(s.stage == 0);
    CHECK(s.feed(60.0) == 60.0);
    CHECK(plan_slowdown({}, {}, 0.5, 5.0, 10.0).stage == 0); // nothing to slow down
}

TEST_CASE("stage 1 puts the faster extrusions at one common speed, not below the minimum (cooling.md, rule 2)",
          "[cooling]") {
    const std::map<double, double> extruded{{60.0, 120.0}, {30.0, 30.0}, {8.0, 4.0}}; // 2 + 1 + 0.5 s
    const Slowdown s = plan_slowdown(extruded, {}, 0.5, 5.0, 10.0);
    CHECK(s.stage == 1);
    // 120 / v + 1 + 0.5 + 0.5 = 5: v = 40 mm/s; the slower ones stay.
    CHECK_THAT(s.cap, WithinRel(40.0, 1e-9));
    CHECK_THAT(s.feed(60.0), WithinRel(40.0, 1e-9));
    CHECK(s.feed(30.0) == 30.0);
    CHECK(s.feed(8.0) == 8.0);
    CHECK_THAT(layer_time(extruded, {}, 0.5, s), WithinRel(5.0, 1e-9));
}

TEST_CASE("stage 2 goes under the minimum speed, down to 1 mm/min", "[cooling]") {
    // At the minimum speed (10 mm/s) the layer takes 1 + 1 s: short of 5 s.
    const std::map<double, double> extruded{{60.0, 10.0}, {1.0, 1.0}};
    const Slowdown s = plan_slowdown(extruded, {}, 0.0, 5.0, 10.0);
    CHECK(s.stage == 2);
    // 10 / v + 1 / 1 = 5: v = 2.5 mm/s, and the move at 1 mm/s stays.
    CHECK_THAT(s.cap, WithinRel(2.5, 1e-9));
    CHECK_THAT(s.feed(60.0), WithinRel(2.5, 1e-9));
    CHECK(s.feed(1.0) == 1.0);
    CHECK_THAT(layer_time(extruded, {}, 0.0, s), WithinRel(5.0, 1e-9));
    // Never below 1 mm/min.
    const Slowdown slowest = plan_slowdown(extruded, {}, 0.0, 1e9, 10.0);
    CHECK(slowest.stage == 2);
    CHECK_THAT(slowest.feed(60.0), WithinAbs(kSlowestFeed, 1e-15));
}

TEST_CASE("the surface walls slow down last (cooling_more.md, rule 5; docs/GLOSS.md)", "[cooling]") {
    // An outer wall of 60 mm at 30 mm/s (2 s) and 120 mm of infill at 60 mm/s
    // (2 s): 4 s, short of 5 s.
    const std::map<double, double> rest{{60.0, 120.0}}, surface{{30.0, 60.0}};
    const Slowdown s = plan_slowdown(rest, surface, 0.0, 5.0, 10.0);
    CHECK(s.stage == 1);
    CHECK(s.feed(30.0, true) == 30.0);                      // the outer wall keeps its speed
    CHECK_THAT(s.feed(60.0), WithinRel(40.0, 1e-9));        // the infill takes the 3 s: 120 mm in 3 s
    CHECK_THAT(layer_time(rest, surface, 0.0, s), WithinRel(5.0, 1e-9));
    // 15 s: the rest at the minimum speed (12 s), the outer wall at 20 mm/s (3 s).
    const Slowdown more = plan_slowdown(rest, surface, 0.0, 15.0, 10.0);
    CHECK(more.stage == 1);
    CHECK(more.feed(60.0) == 10.0);
    CHECK_THAT(more.feed(30.0, true), WithinRel(20.0, 1e-9));
    CHECK_THAT(layer_time(rest, surface, 0.0, more), WithinRel(15.0, 1e-9));
    // 20 s: not even both at the minimum (18 s): everything at one speed,
    // 180 mm in 20 s.
    const Slowdown hard = plan_slowdown(rest, surface, 0.0, 20.0, 10.0);
    CHECK(hard.stage == 2);
    CHECK_THAT(hard.feed(30.0, true), WithinRel(9.0, 1e-9));
    CHECK_THAT(hard.feed(60.0), WithinRel(9.0, 1e-9));
    CHECK_THAT(layer_time(rest, surface, 0.0, hard), WithinRel(20.0, 1e-9));
    // Only surface walls: they slow down as the rest would.
    const Slowdown alone = plan_slowdown({}, surface, 0.0, 5.0, 10.0);
    CHECK(alone.stage == 1);
    CHECK_THAT(alone.feed(30.0, true), WithinRel(12.0, 1e-9));
}

TEST_CASE("the fan by layer time (cooling.md, rule 3 and K1)", "[cooling]") {
    // K1's layers, with the defaults: 5 s, 60 s, 35 to 100 %. The test table
    // shows whole percentages, cut down.
    auto fan = [](double t, bool always = false) { return layer_fan(t, 5.0, 60.0, 35.0, 100.0, always); };
    CHECK(int(fan(6.6)) == 98);
    CHECK(int(fan(10.3)) == 93);
    CHECK(int(fan(16.8)) == 86);
    CHECK(int(fan(46.7)) == 50);
    CHECK(fan(67.9) == 0.0);
    CHECK(fan(67.9, true) == 35.0); // never below fan_min
    CHECK(fan(4.0) == 100.0);       // the minimum time or less: fan_max
    CHECK(fan(5.0) == 100.0);
    CHECK(fan(60.0) == 0.0);
    CHECK_THAT(fan(32.5), WithinRel(67.5, 1e-12)); // half way: half way
    // fan_below_time at or under the minimum time: nothing in between.
    CHECK(layer_fan(5.0, 5.0, 5.0, 35.0, 100.0, false) == 100.0);
    CHECK(layer_fan(5.1, 5.0, 3.0, 35.0, 100.0, false) == 0.0);
}
