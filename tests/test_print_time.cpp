#include "gcode/print_time.hpp"
#include "gcode/time_learning.hpp"

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

TEST_CASE("print_time.md rules 15 and 16: a factor per part learned from finished prints", "[time]") {
    // A printer slower than the estimate on travel (1.3) and infill (1.1),
    // faster on walls (0.95), slower to prepare (1.2), with 1 % of noise.
    const TimeSplit truth{1.0, 0.95, 1.1, 1.3, 1.2};
    const auto real = [&](const TimeSplit& e, int i) {
        double t = 0.0;
        for (std::size_t k = 0; k < kTimeParts; ++k)
            t += truth[k] * e[k];
        return t * (1.0 + 0.01 * ((i % 3) - 1));
    };
    std::vector<TimeRecord> records;
    for (int i = 0; i < 8; ++i) {
        const TimeSplit e{300.0 + 40.0 * i, 1200.0 + 300.0 * (i % 4), 900.0 + 500.0 * ((i + 1) % 3), 200.0 + 60.0 * (i % 5),
                          240.0 + 30.0 * (i % 2)};
        records.push_back({e, real(e, i)});
    }
    // Fewer than three: not fitted, the estimate as it is.
    const TimeCalibration two = fit_time({records[0], records[1]});
    CHECK_FALSE(two.fitted);
    CHECK(two.adjust(records[0].estimate) == records[0].estimate[0] + records[0].estimate[1] + records[0].estimate[2] +
                                                records[0].estimate[3] + records[0].estimate[4]);
    const TimeCalibration fit = fit_time(records);
    REQUIRE(fit.fitted);
    CHECK(fit.prints == 8);
    // A new print: within 2 % once adjusted (the goal with five prints), worse without.
    const TimeSplit next{500.0, 2000.0, 1500.0, 400.0, 300.0};
    const double actual = real(next, 1), plain = 4700.0;
    CHECK(std::abs(fit.adjust(next) - actual) < 0.02 * actual);
    CHECK(std::abs(plain - actual) > 0.03 * actual);
    CHECK(fit.typical_error < 0.02 * actual);
    // Rule 16: half the estimate gone in more real time: the rest scales the same.
    CHECK_THAT(remaining_time(3600.0, 1800.0, 2700.0), WithinRel(2700.0, 1e-9));
    CHECK_THAT(remaining_time(3600.0, 0.0, 0.0), WithinRel(3600.0, 1e-9));
}
