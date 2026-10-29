#include "config/settings.hpp"
#include "gcode/check.hpp"
#include "gcode/preview.hpp"
#include "gcode/print_time.hpp"
#include "gcode/writer.hpp"
#include "mesh/indexed_mesh.hpp"
#include "slice/pipeline.hpp"
#include "test_printer.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

using namespace tisma;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

// The cube of the firmware tests, 20 x 20 x 10 mm.
IndexedMesh cube() {
    IndexedMesh m;
    for (float z : {0.0f, 10.0f})
        for (const auto& p : {std::array<float, 2>{-10, -10}, {10, -10}, {10, 10}, {-10, 10}})
            m.vertices.push_back({p[0], p[1], z});
    const auto quad = [&](std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d) {
        m.triangles.push_back({a, b, c});
        m.triangles.push_back({a, c, d});
    };
    quad(0, 3, 2, 1);
    quad(4, 5, 6, 7);
    quad(0, 1, 5, 4);
    quad(1, 2, 6, 5);
    quad(2, 3, 7, 6);
    quad(3, 0, 4, 7);
    return m;
}

std::vector<std::string> lines_of(const std::string& text) {
    std::vector<std::string> out;
    std::istringstream in(text);
    for (std::string line; std::getline(in, line);)
        out.push_back(line);
    return out;
}

// The G-code of the cube, checked.
std::string gcode_of(const Settings& s) {
    const SlicedModel model = slice_model(cube(), s);
    std::ostringstream out;
    write_gcode(out, model, s);
    const GcodeCheck check = check_gcode(out.str(), gcode_limits(model, s));
    INFO((check.problems.empty() ? std::string() : check.problems.front().message));
    CHECK(check.ok());
    return out.str();
}

// Where each line first appears; npos when it does not.
std::size_t line_at(const std::vector<std::string>& lines, const std::string& line) {
    for (std::size_t i = 0; i < lines.size(); ++i)
        if (lines[i] == line)
            return i;
    return std::string::npos;
}

// The FW tests' printer: the limits of the test profile.
Settings firmware_printer(GcodeFlavor flavor) {
    Settings s = test_printer();
    s.flavor = flavor;
    s.max_accel_x = s.max_accel_y = 9000.0;
    s.max_accel_z = 500.0;
    s.max_accel_e = 10000.0;
    s.max_feed_x = s.max_feed_y = 500.0;
    s.max_feed_z = 12.0;
    s.max_feed_e = 120.0;
    s.acceleration = s.retract_acceleration = s.travel_acceleration = 1500.0;
    s.jerk_x = s.jerk_y = 10.0;
    s.jerk_z = 0.2;
    s.jerk_e = 2.5;
    s.variables["temperature"] = {"200"};
    s.start_gcode = "G28 ; home";
    return s;
}

double estimate(const std::string& gcode, const std::string& which) {
    const std::string key = "; estimated " + which + " = ";
    const std::size_t at = gcode.find(key);
    REQUIRE(at != std::string::npos);
    return parse_duration(gcode.substr(at + key.size(), gcode.find('\n', at) - at - key.size()));
}

} // namespace

TEST_CASE("firmware: Marlin 2 writes its limits and the temperatures round the start (FW1)", "[firmware]") {
    Settings s = firmware_printer(GcodeFlavor::Marlin);
    s.limits_usage = LimitsUsage::Emit;
    s.gcode_progress = true;
    const std::vector<std::string> lines = lines_of(gcode_of(s));
    const std::vector<std::string> expected = {"M201 X9000 Y9000 Z500 E10000",
                                               "M203 X500 Y500 Z12 E120",
                                               "M204 P1500 R1500 T1500",
                                               "M205 X10.00 Y10.00 Z0.20 E2.50",
                                               "M205 S0 T0",
                                               "M104 S200",
                                               "G28 ; home",
                                               "M109 S200"};
    std::size_t last = line_at(lines, "M73 P0 R" + lines[2].substr(lines[2].find('R') + 1));
    REQUIRE(lines[2].starts_with("M73 P0 R"));
    for (const std::string& line : expected) {
        INFO(line);
        const std::size_t at = line_at(lines, line);
        REQUIRE(at != std::string::npos);
        CHECK(at > last);
        last = at;
    }
}

TEST_CASE("firmware: RepRapFirmware's limits in mm/min, G10 and M116 (FW2)", "[firmware]") {
    Settings s = firmware_printer(GcodeFlavor::RepRapFirmware);
    s.limits_usage = LimitsUsage::Emit;
    const std::string g = gcode_of(s);
    for (const char* line : {"\nM201 X9000 Y9000 Z500 E10000\n", "\nM203 X30000 Y30000 Z720 E7200\n",
                             "\nM204 P1500 T1500\n", "\nM566 X600.00 Y600.00 Z12.00 E150.00\n", "\nG10 S200 P0\n",
                             "\nM116\n"}) {
        INFO(line);
        CHECK(g.find(line) != std::string::npos);
    }
    CHECK(g.find("M205") == std::string::npos);
    CHECK(g.find("M109") == std::string::npos);
}

TEST_CASE("firmware: Klipper takes its limits from its configuration (FW4); generic RepRap writes none (FW8)",
          "[firmware]") {
    Settings klipper = firmware_printer(GcodeFlavor::Klipper);
    klipper.limits_usage = LimitsUsage::Emit;
    CHECK_THROWS_WITH(check_settings(klipper), ContainsSubstring("machine.limits_usage: Klipper"));
    Settings reprap = firmware_printer(GcodeFlavor::RepRap);
    reprap.limits_usage = LimitsUsage::Emit;
    reprap.gcode_progress = true;
    reprap.silent_mode = true;
    reprap.silent_limits = {{"acceleration", 300.0}};
    CHECK_NOTHROW(check_settings(reprap));
    const std::string g = gcode_of(reprap);
    CHECK(g.find("M201") == std::string::npos);
    CHECK(g.find("\nM73 P0 R") != std::string::npos);
    CHECK(g.find("M73 Q") == std::string::npos); // FW8: M73 P R only
    CHECK(g.find("\nM104 S200\nG28 ; home\nM109 S200\n") != std::string::npos);
}

TEST_CASE("firmware: Marlin 1 and Marlin 2 write M204 their own way (FW9)", "[firmware]") {
    for (const GcodeFlavor flavor : {GcodeFlavor::MarlinLegacy, GcodeFlavor::Marlin}) {
        Settings s = firmware_printer(flavor);
        s.limits_usage = LimitsUsage::Emit;
        s.acceleration = 1100.0;
        s.retract_acceleration = 1200.0;
        s.travel_acceleration = 1300.0;
        const std::string g = gcode_of(s);
        CHECK(g.find(flavor == GcodeFlavor::Marlin ? "\nM204 P1100 R1200 T1300\n" : "\nM204 S1100 T1200\n") !=
              std::string::npos);
    }
    // A junction deviation in place of the jerk.
    Settings s = firmware_printer(GcodeFlavor::Marlin);
    s.limits_usage = LimitsUsage::Emit;
    s.junction_deviation = 0.013;
    const std::string g = gcode_of(s);
    CHECK(g.find("\nM205 J0.013\n") != std::string::npos);
    CHECK(g.find("M205 X") == std::string::npos);
}

TEST_CASE("firmware: a start G-code that heats is left as it is (rule 1)", "[firmware]") {
    Settings s = firmware_printer(GcodeFlavor::Marlin);
    s.start_gcode = "M190 S60\nm109 s210\nG28";
    const std::string g = gcode_of(s);
    CHECK(g.find("M104 S200") == std::string::npos);
    CHECK(g.find("M109 S200") == std::string::npos);
    // Without a temperature variable, nothing either.
    s.start_gcode = "G28";
    s.variables.erase("temperature");
    CHECK(gcode_of(s).find("M104") == std::string::npos);
    CHECK(sets_temperature("  G10 S200 P0"));
    CHECK_FALSE(sets_temperature("G10 ; retract"));
    CHECK_FALSE(sets_temperature("G28\n; M104 S200"));
}

TEST_CASE("firmware: Marlin with relative E restarts it at each layer (rule 3, FW5)", "[firmware]") {
    Settings s = firmware_printer(GcodeFlavor::Marlin);
    s.relative_extrusion = true;
    const std::vector<std::string> lines = lines_of(gcode_of(s));
    std::size_t resets = 0;
    for (std::size_t i = 1; i < lines.size(); ++i)
        if (lines[i] == "G92 E0") {
            ++resets;
            CHECK(lines[i - 1].starts_with("G1 Z")); // right after the move up
        }
    CHECK(resets == 50);
    s.flavor = GcodeFlavor::Klipper;
    CHECK(gcode_of(s).find("G92 E0\n") == std::string::npos);
}

TEST_CASE("firmware: M73 by the estimated time (rule 5, FW7)", "[firmware]") {
    Settings s = firmware_printer(GcodeFlavor::Klipper);
    s.gcode_progress = true;
    const std::string g = gcode_of(s);
    const double total = estimate(g, "printing time");
    long last_p = -1, last_r = 1 << 20;
    std::size_t count = 0;
    for (const std::string& line : lines_of(g))
        if (line.starts_with("M73 ")) {
            ++count;
            long p = 0, r = 0;
            REQUIRE(std::sscanf(line.c_str(), "M73 P%ld R%ld", &p, &r) == 2);
            CHECK(p >= last_p);
            CHECK(r <= last_r);
            CHECK((p != last_p || r != last_r));
            last_p = p, last_r = r;
        }
    CHECK(last_p == 100);
    CHECK(last_r == 0);
    CHECK(count >= 100); // every percent, and every minute
    CHECK(count <= 101 + std::size_t(total / 60.0) + 1);
    CHECK(g.find("\x01") == std::string::npos);
    // The same G-code but for the M73 lines.
    s.gcode_progress = false;
    std::string without;
    for (const std::string& line : lines_of(g))
        if (!line.starts_with("M73 ") && line != "; gcode.progress = true")
            without += line + "\n";
    std::string plain;
    for (const std::string& line : lines_of(gcode_of(s)))
        if (line != "; gcode.progress = false")
            plain += line + "\n";
    CHECK(without == plain);
}

TEST_CASE("firmware: a silent mode gets its own estimate and M73 Q S (gcode_extras.md, rule 7, X6)", "[firmware]") {
    Settings s = firmware_printer(GcodeFlavor::Marlin);
    const std::string normal = gcode_of(s);
    s.silent_mode = true;
    s.silent_limits = {{"acceleration", 300.0}, {"travel_acceleration", 300.0}, {"max_feed_x", 200.0}};
    s.gcode_progress = true;
    const std::string g = gcode_of(s);
    CHECK_THAT(estimate(g, "printing time (normal mode)"), WithinAbs(estimate(normal, "printing time"), 0.5));
    CHECK(estimate(g, "printing time (silent mode)") > estimate(g, "printing time (normal mode)") + 10.0);
    CHECK(estimate(g, "first layer printing time (silent mode)") >= estimate(g, "first layer printing time (normal mode)"));
    CHECK(estimate(g, "first layer printing time (normal mode)") > 0.0);
    CHECK(g.find("\nM73 Q0 S") != std::string::npos);
    CHECK(g.find("\nM73 Q100 S0\n") != std::string::npos);
    // The preview reads the normal mode's time.
    CHECK_THAT(read_gcode_preview(g).estimated_seconds, WithinAbs(estimate(g, "printing time (normal mode)"), 0.5));
    // Silent limits with an unknown name are refused.
    CHECK_THROWS_WITH(parse_settings(R"({"machine": {"silent": {"speed": 3}}})"),
                      ContainsSubstring("machine.silent.speed: is not a machine limit"));
    const Settings parsed = parse_settings(R"({"machine": {"silent_mode": true, "silent": {"jerk_x": 7}}})");
    CHECK(parsed.silent_limits.at("jerk_x") == 7.0);
    CHECK(silent_settings(parsed).jerk_x == 7.0);
    CHECK(parse_settings(settings_json(parsed)).silent_limits == parsed.silent_limits);
}

TEST_CASE("firmware: with the limits written no acceleration passes them (speeds_accels.md, V9)", "[firmware]") {
    Settings s = firmware_printer(GcodeFlavor::Marlin);
    s.accel_default = 2000.0;
    s.accel_infill = 3000.0;
    s.accel_walls = 1000.0;
    s.accel_travel = 4000.0;
    std::string g = gcode_of(s);
    CHECK(g.find("\nM204 P3000\n") != std::string::npos);
    CHECK(g.find("\nM204 T4000\n") != std::string::npos);
    s.limits_usage = LimitsUsage::Emit;
    g = gcode_of(s);
    CHECK(g.find("M204 P3000") == std::string::npos);
    CHECK(g.find("M204 T4000") == std::string::npos);
    CHECK(g.find("\nM204 P1000\n") != std::string::npos);
    CHECK(g.find("\nM204 P1500\n") != std::string::npos); // the infill's and the default, at the limit
}

TEST_CASE("firmware: the limits change the estimate, unless ignored (print_time.md, rules 2 and 3)", "[firmware]") {
    Settings s = firmware_printer(GcodeFlavor::Marlin);
    s.max_accel_x = s.max_accel_y = 0.0;
    const double free = estimate(gcode_of(s), "printing time");
    s.max_accel_x = s.max_accel_y = 300.0;
    const double slow = estimate(gcode_of(s), "printing time");
    CHECK(slow > free + 10.0);
    s.limits_usage = LimitsUsage::Ignore;
    s.jerk_x = s.jerk_y = 0.0;
    Settings plain = s;
    plain.max_accel_x = plain.max_accel_y = 0.0;
    CHECK_THAT(estimate(gcode_of(s), "printing time"), WithinAbs(estimate(gcode_of(plain), "printing time"), 0.5));
}

TEST_CASE("time: a corner by its junction deviation or by the axes' jerk", "[time]") {
    // A right angle: the circle that rounds it 0.05 mm from the corner has a
    // radius of 0.05 sin45 / (1 - sin45); at 1000 mm/s2, about 10.99 mm/s.
    const double r = 0.05 * std::sqrt(0.5) / (1.0 - std::sqrt(0.5)), v = std::sqrt(1000.0 * r);
    const auto corner = [](const Kinematics& k) {
        PrintTime t(k);
        t.add(20.0, 0.0, 0.0, 0.5, 60.0, true);
        t.add(0.0, 20.0, 0.0, 0.5, 60.0, true);
        return t.flush();
    };
    Kinematics deviation, corner_speed, jerk;
    deviation.acceleration = corner_speed.acceleration = jerk.acceleration = 1000.0;
    deviation.junction_deviation = 0.05;
    corner_speed.corner_speed = v;
    CHECK_THAT(corner(deviation), WithinRel(corner(corner_speed), 1e-9));
    // Jerk: X stops from v and Y starts to v at once, so v is the jerk.
    jerk.jerk[0] = jerk.jerk[1] = 7.0;
    corner_speed.corner_speed = 7.0;
    CHECK_THAT(corner(jerk), WithinRel(corner(corner_speed), 1e-9));
    // The axes' accelerations: a move along X at most X's.
    Kinematics capped;
    capped.acceleration = 1000.0;
    capped.max_accel[0] = 200.0;
    Kinematics low;
    low.acceleration = 200.0;
    PrintTime a(capped), b(low);
    a.add(30.0, 0.0, 0.0, 1.0, 60.0, true);
    b.add(30.0, 0.0, 0.0, 1.0, 60.0, true);
    CHECK_THAT(a.flush(), WithinRel(b.flush(), 1e-12));
    // The slowest feed the machine moves.
    Kinematics floor;
    floor.min_extrude_feed = 20.0;
    PrintTime c(floor), d(Kinematics{});
    c.add(10.0, 0.0, 0.0, 0.1, 5.0, true);
    d.add(10.0, 0.0, 0.0, 0.1, 20.0, true);
    CHECK_THAT(c.flush(), WithinRel(d.flush(), 1e-12));
}

TEST_CASE("time: marks give the time up to them", "[time]") {
    PrintTime t(Kinematics{});
    const std::size_t start = t.mark();
    t.add(20.0, 0.0, 0.0, 0.5, 60.0, true);
    const std::size_t middle = t.mark();
    t.add(20.0, 0.0, 0.0, 0.5, 60.0, true);
    const double first = t.flush();
    const std::size_t end = t.mark();
    t.flush();
    CHECK(t.time_at(start) == 0.0);
    CHECK(t.time_at(middle) > 0.0);
    CHECK(t.time_at(middle) < first);
    CHECK_THAT(t.time_at(middle), WithinRel(first / 2.0, 1e-9)); // two equal moves through a straight junction
    CHECK(t.time_at(end) == t.total());
}
