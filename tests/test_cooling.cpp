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
