#include "infill/adaptive.hpp"
#include "infill/rectilinear.hpp"

#include "gcode/check.hpp"
#include "gcode/writer.hpp"
#include "slice/pipeline.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <numbers>
#include <random>
#include <sstream>

using namespace tisma;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

Polygons square(double x0, double y0, double side) {
    const auto u = [](double v) { return to_units(v); };
    return {{{u(x0), u(y0)}, {u(x0 + side), u(y0)}, {u(x0 + side), u(y0 + side)}, {u(x0), u(y0 + side)}}};
}

// A block of sparse infill `side` mm square from the origin, from z0 to z1,
// in a stack of 0.2 mm layers from 0 to `height`: as if walls and solid
// layers closed it all round.
struct Block {
    std::vector<Layer> layers;
    std::vector<Polygons> sparse;
};

Block block(double side, double height, double z0, double z1) {
    Block b;
    for (double z = 0.0; z + 0.1 < height; z += 0.2) {
        b.layers.push_back({z, z + 0.2});
        b.sparse.push_back(z >= z0 - 1e-9 && z + 0.2 <= z1 + 1e-9 ? square(0, 0, side) : Polygons{});
    }
    return b;
}

std::map<int, double> by_direction(const std::vector<Toolpath>& paths) {
    std::map<int, double> out;
    for (const Toolpath& t : paths)
        for (std::size_t i = 1; i < t.points.size(); ++i) {
            const double dx = t.points[i].p.x - t.points[i - 1].p.x, dy = t.points[i].p.y - t.points[i - 1].p.y;
            const double length = std::hypot(dx, dy);
            if (length < 0.3)
                continue;
            const double a = std::fmod(std::atan2(dy, dx) * 180.0 / std::numbers::pi + 360.0, 180.0);
            out[int(std::lround(a)) % 180] += length;
        }
    return out;
}

double total(const std::vector<Toolpath>& paths) {
    double sum = 0.0;
    for (const Toolpath& t : paths)
        for (std::size_t i = 1; i < t.points.size(); ++i)
            sum += std::hypot(t.points[i].p.x - t.points[i - 1].p.x, t.points[i].p.y - t.points[i - 1].p.y);
    return sum;
}

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

} // namespace

TEST_CASE("distance transform: exact, against every pair", "[adaptive]") {
    const int nx = 11, ny = 9, nz = 7;
    std::mt19937 random(7);
    std::vector<char> set(std::size_t(nx * ny * nz));
    for (char& c : set)
        c = random() % 10 < 8; // mostly set: long distances
    set[17] = 0;
    const std::vector<float> d = distance_transform(set, nx, ny, nz);
    for (int k = 0; k < nz; ++k)
        for (int j = 0; j < ny; ++j)
            for (int i = 0; i < nx; ++i) {
                const std::size_t at = std::size_t((k * ny + j) * nx + i);
                double best = set[at] != 0 ? std::numeric_limits<double>::infinity() : 0.0;
                for (int c = 0; c < nz; ++c)
                    for (int b = 0; b < ny; ++b)
                        for (int a = 0; a < nx; ++a)
                            if (set[std::size_t((c * ny + b) * nx + a)] == 0)
                                best = std::min(best, std::sqrt(double((a - i) * (a - i) + (b - j) * (b - j) +
                                                                       (c - k) * (c - k))));
                CHECK_THAT(d[at], WithinAbs(best, 1e-4));
            }
}

TEST_CASE("depth field: how deep a block of sparse infill is", "[adaptive]") {
    // 30 mm square, sparse from z 1 to 19.
    const Block b = block(30.0, 20.0, 1.0, 19.0);
    const DepthField f = sparse_depth(b.sparse, b.layers, 0.5);
    REQUIRE(!f.empty());
    // The middle is 9 mm from the solid layers, 15 from the walls.
    CHECK_THAT(f.deepest, WithinAbs(9.0, 0.5));
    // At z 10, 5 mm deep: the square shrunk by 5.
    CHECK_THAT(area_mm2(deep_region(f, 10.0, 5.0)), WithinAbs(20.0 * 20.0, 25.0));
    // 2 mm above the bottom solid layers, nothing is 5 mm deep.
    CHECK(deep_region(f, 3.0, 5.0).empty());
    // A thin solid layer in the middle counts as a surface, though the
    // voxels are thicker than it.
    Block split = b;
    split.sparse[50] = {}; // z 10 to 10.2
    const DepthField g = sparse_depth(split.sparse, split.layers, 0.5);
    CHECK_THAT(g.deepest, WithinAbs(4.5, 0.5));
    // Too many voxels: wider ones.
    const DepthField coarse = sparse_depth(b.sparse, b.layers, 0.1, DepthFrom::Surfaces, 100000);
    CHECK(coarse.cell > 0.1);
    CHECK(std::size_t(coarse.nx) * std::size_t(coarse.ny) * std::size_t(coarse.nz) <= 100000);
}

TEST_CASE("adaptive cubic: 0, 60 and 120 degrees whatever the infill angle (P6)", "[adaptive]") {
    FillParams p;
    p.width = 0.4;
    p.density = 0.2;
    p.angle = 45.0;
    p.pattern = InfillPattern::AdaptiveCubic;
    LayerSurfaces s;
    s.sparse = square(0, 0, 20.0);
    const std::vector<Toolpath> lines = fill_layer(s, 5, p, 1.0).sparse;
    const std::map<int, double> d = by_direction(lines);
    const double all = total(lines);
    REQUIRE(all > 0.0);
    for (const int a : {0, 60, 120})
        CHECK_THAT(d.count(a) ? d.at(a) / all : 0.0, WithinAbs(1.0 / 3.0, 0.06));
}

TEST_CASE("adaptive cubic: the cubic lines near the surfaces, every other one deep inside", "[adaptive]") {
    // 60 mm square, sparse from z 1 to 39: 19 mm deep in the middle.
    const Block b = block(60.0, 40.0, 1.0, 39.0);
    const DepthField f = sparse_depth(b.sparse, b.layers, 1.5);
    const double width = 0.4, spacing = 3.0 * width / 0.2; // 6 mm
    REQUIRE(level_depth(2, spacing) <= f.deepest); // two levels: cubes twice and four times the finest
    REQUIRE(level_depth(3, spacing) > f.deepest);
    FillParams cubic;
    cubic.width = width;
    cubic.density = 0.2;
    cubic.angle = 0.0;
    cubic.pattern = InfillPattern::Cubic;
    LayerSurfaces s;
    s.sparse = square(0, 0, 60.0);
    for (const double z : {2.1, 20.1}) {
        INFO("z " << z);
        const std::vector<Toolpath> plain = fill_layer(s, 0, cubic, z).sparse;
        const std::vector<Toolpath> adaptive = adaptive_cubic_fill(s.sparse, &f, spacing, width, z);
        if (z < 5.0) {
            // Near the bottom solid layers nothing is deep: the cubic infill.
            CHECK_THAT(total(adaptive), WithinRel(total(plain), 1e-6));
        } else {
            // Deep inside fewer lines, but more than the coarsest level alone.
            CHECK(total(adaptive) < 0.75 * total(plain));
            CHECK(total(adaptive) > 0.3 * total(plain));
        }
        // Every line is one of the cubic infill's, so the coarse lines run on
        // as the cubic planes through every level.
        const double shift = -z / std::numbers::sqrt2;
        int off = 0;
        for (const Toolpath& t : adaptive) {
            const auto& a = t.points.front().p;
            const auto& e = t.points.back().p;
            const double angle = std::atan2(e.y - a.y, e.x - a.x);
            const double across = -std::sin(angle) * a.x + std::cos(angle) * a.y;
            const double k = (across - shift) / spacing;
            off += std::abs(k - std::round(k)) > 1e-4;
        }
        CHECK(off == 0);
    }
}

TEST_CASE("adaptive cubic: a 60 mm cube sliced to checked G-code, less infill than cubic", "[adaptive]") {
    // Cubes twice the finest fit in the middle third, four times in a 20 mm
    // core: about 16 % less infill.
    const IndexedMesh m = cube(60.0f);
    Settings s;
    s.gpu = GpuMode::Off;
    s.nozzle_diameter = 0.4;
    s.layer_height = 0.2;
    s.first_layer_height = 0.2;
    s.wall_count = 2;
    s.infill_density = 20.0;
    s.skirt_loops = 0;
    s.min_layer_time = 0.0;
    double length[2] = {0.0, 0.0};
    for (const InfillPattern pattern : {InfillPattern::Cubic, InfillPattern::AdaptiveCubic}) {
        s.infill_pattern = pattern;
        const SlicedModel sliced = slice_model(m, s);
        std::ostringstream out;
        write_gcode(out, sliced, s);
        const GcodeCheck check = check_gcode(out.str(), gcode_limits(sliced, s));
        INFO((check.problems.empty() ? std::string() : check.problems.front().message));
        CHECK(check.ok());
        for (const LayerFill& fill : sliced.fills)
            length[pattern == InfillPattern::AdaptiveCubic] += total(fill.sparse);
    }
    CHECK(length[1] < 0.92 * length[0]);
    CHECK(length[1] > 0.75 * length[0]);
}

TEST_CASE("support cubic: fine under what it holds up, not over the solid layers below (rule 8)", "[adaptive]") {
    // 60 mm square, sparse from z 1 to 39.
    const Block b = block(60.0, 40.0, 1.0, 39.0);
    const DepthField all = sparse_depth(b.sparse, b.layers, 1.5);
    const DepthField tops = sparse_depth(b.sparse, b.layers, 1.5, DepthFrom::TopsAndWalls);
    // The middle of the bottom is 38 mm under the top but 30 from the walls.
    CHECK_THAT(tops.deepest, WithinAbs(30.0, 1.5));
    const double width = 0.4, spacing = 6.0;
    const Polygons region = square(0, 0, 60.0);
    // Just over the bottom solid layers: the adaptive cubic is the cubic, the
    // support cubic coarse in the middle.
    CHECK(total(adaptive_cubic_fill(region, &tops, spacing, width, 2.1)) <
          0.8 * total(adaptive_cubic_fill(region, &all, spacing, width, 2.1)));
    // Right under the top, both are the cubic.
    CHECK_THAT(total(adaptive_cubic_fill(region, &tops, spacing, width, 38.1)),
               WithinRel(total(adaptive_cubic_fill(region, &all, spacing, width, 38.1)), 1e-6));
}

TEST_CASE("support cubic: the spec's 20 mm cube, the same as adaptive cubic (P6)", "[adaptive]") {
    const IndexedMesh m = cube(20.0f);
    Settings s;
    s.gpu = GpuMode::Off;
    s.wall_count = 2;
    s.infill_density = 20.0;
    s.skirt_loops = 0;
    double length[2] = {0.0, 0.0};
    for (const InfillPattern pattern : {InfillPattern::AdaptiveCubic, InfillPattern::SupportCubic}) {
        s.infill_pattern = pattern;
        const SlicedModel sliced = slice_model(m, s);
        std::ostringstream out;
        write_gcode(out, sliced, s);
        const GcodeCheck check = check_gcode(out.str(), gcode_limits(sliced, s));
        INFO((check.problems.empty() ? std::string() : check.problems.front().message));
        CHECK(check.ok());
        for (const LayerFill& fill : sliced.fills)
            length[pattern == InfillPattern::SupportCubic] += total(fill.sparse);
    }
    REQUIRE(length[0] > 0.0);
    CHECK_THAT(length[1], WithinRel(length[0], 1e-6));
}
