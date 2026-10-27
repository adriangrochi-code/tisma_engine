#include "infill/nonplanar.hpp"

#include "gcode/check.hpp"
#include "gcode/writer.hpp"
#include "slice/pipeline.hpp"
#include "test_printer.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <optional>
#include <sstream>
#include <string>

using namespace tisma;
using Catch::Matchers::WithinAbs;

namespace {

IndexedMesh cube(float side) {
    IndexedMesh m;
    m.vertices = {{0, 0, 0}, {side, 0, 0}, {side, side, 0}, {0, side, 0},
                  {0, 0, side}, {side, 0, side}, {side, side, side}, {0, side, side}};
    auto quad = [&](std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d) {
        m.triangles.push_back({a, b, c});
        m.triangles.push_back({a, c, d});
    };
    quad(0, 3, 2, 1);
    quad(4, 5, 6, 7);
    for (std::uint32_t i = 0; i < 4; ++i)
        quad(i, (i + 1) % 4, 4 + (i + 1) % 4, 4 + i);
    return m;
}

Settings wavy(InfillPattern pattern) {
    Settings s = test_printer();
    s.gpu = GpuMode::Off;
    s.nozzle_diameter = 0.4;
    s.wall_width = 0.4;
    s.layer_height = 0.2;
    s.first_layer_height = 0.2;
    s.wall_count = 2;
    s.infill_density = 20.0;
    s.infill_pattern = pattern;
    s.infill_nonplanar = true;
    s.skirt_loops = 0;
    s.min_layer_time = 0.0;
    return s;
}

} // namespace

TEST_CASE("wavy infill: the wave, its share by depth, and which settings get it", "[nonplanar]") {
    WaveParams p;
    p.amplitude = 0.5;
    p.wavelength = 2.0;
    p.direction = 90.0; // a line at 45 degrees to it sees a wave 2 mm long
    double lo = 1e9, hi = -1e9, steepest = 0.0;
    const Point2d d{std::cos(std::numbers::pi / 4.0), std::sin(std::numbers::pi / 4.0)};
    for (int i = 0; i <= 800; ++i) {
        const double s = 0.005 * i, ds = 0.005;
        const double h = wave_height(p, {s * d.x, s * d.y});
        const double next = wave_height(p, {(s + ds) * d.x, (s + ds) * d.y});
        lo = std::min(lo, h), hi = std::max(hi, h);
        steepest = std::max(steepest, std::abs(next - h) / ds);
    }
    CHECK_THAT(lo, WithinAbs(-0.25, 1e-3));
    CHECK_THAT(hi, WithinAbs(0.25, 1e-3));
    CHECK_THAT(steepest, WithinAbs(0.5, 1e-3)); // 0.5 mm up over 1 mm, as in ZigZagZ
    // A trough and a crest a wavelength apart along the line.
    CHECK_THAT(wave_height(p, {0.0, 0.0}), WithinAbs(wave_height(p, {2.0 * d.x, 2.0 * d.y}), 1e-9));
    // The share: none near the walls, all deep inside.
    p.margin = 0.45;
    p.fade = 1.0;
    CHECK(wave_share(p, 0.3) == 0.0);
    CHECK_THAT(wave_share(p, 0.95), WithinAbs(0.5, 1e-9));
    CHECK(wave_share(p, 3.0) == 1.0);
    // Only patterns that repeat layer to layer, and not with lines too close.
    CHECK(wave_params(wavy(InfillPattern::Grid)));
    CHECK(wave_params(wavy(InfillPattern::Honeycomb)));
    CHECK(!wave_params(wavy(InfillPattern::Rectilinear)));
    CHECK(!wave_params(wavy(InfillPattern::Gyroid)));
    Settings dense = wavy(InfillPattern::AlignedRectilinear);
    dense.infill_density = 80.0; // lines 0.5 mm apart: the nozzle could not clear a crest
    CHECK(!wave_params(dense));
    Settings off = wavy(InfillPattern::Grid);
    off.infill_nonplanar = false;
    CHECK(!wave_params(off));
    // Settings built in code skip the JSON ranges: a wave without a length is
    // no wave, and the slice still ends.
    Settings flat = wavy(InfillPattern::Grid);
    flat.infill_nonplanar_wavelength = 0.0;
    CHECK(!wave_params(flat));
    flat.infill_nonplanar_wavelength = std::nan("");
    CHECK(!wave_params(flat));
    const SlicedModel m = slice_model(cube(10.0f), flat);
    for (const LayerFill& f : m.fills)
        for (const Toolpath& t : f.sparse)
            CHECK(!t.wavy);
}

TEST_CASE("wavy infill: a cube with grid infill, nested layers and checked G-code", "[nonplanar]") {
    const Settings s = wavy(InfillPattern::Grid);
    const SlicedModel m = slice_model(cube(30.0f), s);
    REQUIRE(m.fills.size() == 150);
    const std::optional<WaveParams> p = wave_params(s);
    REQUIRE(p);
    // In the middle layer: wavy paths, up to the whole amplitude, nested on
    // the layer below (the same height where both have the whole wave).
    double highest = 0.0, nested = 0.0, all = 0.0;
    for (const Toolpath& t : m.fills[75].sparse) {
        CHECK(t.wavy);
        for (const ToolpathPoint& q : t.points) {
            highest = std::max(highest, std::abs(q.dz));
            all += 1.0;
            nested += std::abs(q.dz - q.below) < 1e-9;
            // The waves fade out by the walls: flat where the lines meet them
            // (the sparse infill starts 0.8 mm in, behind two walls).
            const double edge = std::min({q.p.x, q.p.y, 30.0 - q.p.x, 30.0 - q.p.y});
            if (edge < 1.1)
                CHECK(std::abs(q.dz) < 1e-9);
        }
    }
    // Up to the crest, but for where the 0.25 mm cuts fall: at most half a cut's rise short.
    CHECK_THAT(highest, WithinAbs(0.5 * p->amplitude, 0.5 * 0.5 * p->step));
    CHECK(nested > 0.95 * all);
    // Just over the bottom solid layers the waves are still small.
    for (const Toolpath& t : m.fills[4].sparse)
        for (const ToolpathPoint& q : t.points)
            CHECK(std::abs(q.dz) < 0.25 * p->amplitude);
    // The G-code passes the check, with each bead as thick as its gap.
    std::ostringstream g;
    write_gcode(g, m, s);
    const std::string text = g.str();
    CHECK(text.find(";NONPLANAR:1") != std::string::npos);
    const GcodeCheck check = check_gcode(text, gcode_limits(m, s));
    INFO((check.problems.empty() ? std::string() : check.problems.front().message));
    CHECK(check.ok());
}

TEST_CASE("wavy infill: the check wants each wavy bead as thick as its ;HEIGHT", "[nonplanar]") {
    const Settings s = wavy(InfillPattern::Grid);
    const SlicedModel m = slice_model(cube(30.0f), s);
    std::ostringstream g;
    write_gcode(g, m, s);
    std::string text = g.str();
    // Doubling one wavy move's E breaks it.
    const std::size_t block = text.find(";NONPLANAR:1");
    REQUIRE(block != std::string::npos);
    std::size_t at = text.find("\nG1 X", block);
    REQUIRE(at != std::string::npos);
    const std::size_t e = text.find(" E", at + 1);
    const std::size_t end = text.find_first_of(" \n", e + 2);
    REQUIRE(e != std::string::npos);
    const double value = std::stod(text.substr(e + 2, end - e - 2));
    text.replace(e + 2, end - e - 2, std::to_string(2.0 * value + 0.05));
    CHECK(!check_gcode(text, gcode_limits(m, s)).ok());
    // Without wavy infill in the settings, a nonplanar block is refused.
    Settings flat = s;
    flat.infill_nonplanar = false;
    CHECK(!check_gcode(g.str(), gcode_limits(m, flat)).ok());
}

TEST_CASE("wavy infill: patterns whose lines move from layer to layer stay flat", "[nonplanar]") {
    const Settings s = wavy(InfillPattern::Rectilinear);
    const SlicedModel m = slice_model(cube(20.0f), s);
    for (const LayerFill& f : m.fills)
        for (const Toolpath& t : f.sparse) {
            CHECK(!t.wavy);
            for (const ToolpathPoint& q : t.points)
                CHECK(q.dz == 0.0);
        }
}
