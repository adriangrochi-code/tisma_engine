#include "gcode/print_time.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

using namespace tisma;
using Catch::Matchers::WithinRel;

TEST_CASE("a long move: accelerate, cruise, decelerate", "[time]") {
    Kinematics k;
    k.acceleration = 1000.0;
    PrintTime t(k);
    t.add(100.0, 0.0, 0.0, 1.0, 50.0, true);
    // 50 mm/s reached after 0.05 s and 1.25 mm, the same to stop: 97.5 mm at 50 mm/s.
    CHECK_THAT(t.flush(), WithinRel(0.05 + 0.05 + 97.5 / 50.0, 1e-12));
    CHECK_THAT(t.total(), WithinRel(2.05, 1e-12));
}

TEST_CASE("a short move never reaches its feed", "[time]") {
    Kinematics k;
    k.travel_acceleration = 1000.0;
    PrintTime t(k);
    t.add(1.0, 0.0, 0.0, 0.0, 100.0, false);
    // Triangle: half the way speeding up, half slowing down: t = 2 sqrt(L / a).
    CHECK_THAT(t.flush(), WithinRel(2.0 * std::sqrt(1.0 / 1000.0), 1e-12));
    CHECK_THAT(move_time(1.0, 0.0, 0.0, 100.0, 1000.0), WithinRel(2.0 * std::sqrt(0.001), 1e-12));
}

TEST_CASE("corners cost time; straight runs keep their speed", "[time]") {
    Kinematics k;
    PrintTime square(k), line(k);
    for (int i = 0; i < 4; ++i) {
        const double dx[4] = {20, 0, -20, 0}, dy[4] = {0, 20, 0, -20};
        square.add(dx[i], dy[i], 0.0, 0.5, 60.0, true);
        line.add(20.0, 0.0, 0.0, 0.5, 60.0, true); // the same length, no turns
    }
    const double s = square.flush(), l = line.flush();
    CHECK(s > 80.0 / 60.0);
    CHECK(l > 80.0 / 60.0);
    CHECK(s > l);
    // Collinear pieces join at full speed: the same as one move of 80 mm.
    PrintTime one(k);
    one.add(80.0, 0.0, 0.0, 2.0, 60.0, true);
    CHECK_THAT(l, WithinRel(one.flush(), 1e-12));
}

TEST_CASE("axis limits clamp the feed", "[time]") {
    Kinematics k;
    k.max_feed[2] = 5.0;
    k.travel_acceleration = 1e9; // next to no acceleration time
    PrintTime t(k);
    t.add(0.0, 0.0, 10.0, 0.0, 100.0, false); // Z at most 5 mm/s
    CHECK_THAT(t.flush(), WithinRel(2.0, 1e-6));
    PrintTime diagonal(k);
    diagonal.add(30.0, 0.0, 4.0, 0.0, 100.0, false); // Z is 4 / sqrt(916) of the move
    CHECK_THAT(diagonal.flush(), WithinRel(4.0 / 5.0, 1e-6));
    k.max_feed[3] = 40.0;
    PrintTime e_only(k);
    e_only.add(0.0, 0.0, 0.0, -2.0, 60.0, false); // E alone: 2 mm at 40 mm/s
    CHECK_THAT(e_only.flush(), WithinRel(0.05, 1e-6));
}

TEST_CASE("durations as the preview reads them", "[time]") {
    CHECK(format_duration(4056.0) == "1h 7m 36s");
    CHECK(format_duration(456.4) == "7m 36s");
    CHECK(format_duration(36.0) == "36s");
    CHECK(format_duration(3605.0) == "1h 0m 5s");
    CHECK(format_duration(2 * 86400.0 + 3605.0) == "2d 1h 0m 5s");
}
