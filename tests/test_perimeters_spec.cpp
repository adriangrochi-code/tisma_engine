// The cases of docs/spec/features/perimeters.md not covered elsewhere (P3,
// P4, P5 to P7, P9), read from the engine's G-code.

#include "config/settings.hpp"
#include "gcode/writer.hpp"
#include "mesh/indexed_mesh.hpp"
#include "mesh/mesh.hpp"
#include "slice/pipeline.hpp"
#include "test_printer.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace tisma;
using Catch::Matchers::WithinAbs;

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kCx = 100.0, kCy = 100.0; // every part is centred here

void add_box(TriangleMesh& m, float x0, float y0, float z0, float x1, float y1, float z1) {
    const Vec3f v[8] = {{x0, y0, z0}, {x1, y0, z0}, {x1, y1, z0}, {x0, y1, z0},
                        {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}};
    const int faces[6][4] = {{0, 3, 2, 1}, {4, 5, 6, 7}, {0, 1, 5, 4}, {1, 2, 6, 5}, {2, 3, 7, 6}, {3, 0, 4, 7}};
    for (const auto& f : faces) {
        m.add_triangle(v[f[0]], v[f[1]], v[f[2]]);
        m.add_triangle(v[f[0]], v[f[2]], v[f[3]]);
    }
}

TriangleMesh cylinder(float radius, float height, int sides) {
    TriangleMesh m;
    const Vec3f bc{float(kCx), float(kCy), 0}, tc{float(kCx), float(kCy), height};
    for (int k = 0; k < sides; ++k) {
        const double a0 = 2.0 * kPi * k / sides, a1 = 2.0 * kPi * (k + 1) / sides;
        const Vec3f b0{float(kCx + radius * std::cos(a0)), float(kCy + radius * std::sin(a0)), 0};
        const Vec3f b1{float(kCx + radius * std::cos(a1)), float(kCy + radius * std::sin(a1)), 0};
        const Vec3f t0{b0.x, b0.y, height}, t1{b1.x, b1.y, height};
        m.add_triangle(bc, b1, b0);
        m.add_triangle(tc, t0, t1);
        m.add_triangle(b0, b1, t1);
        m.add_triangle(b0, t1, t0);
    }
    return m;
}

std::string gcode_of(const TriangleMesh& m, const Settings& s) {
    std::ostringstream out;
    write_gcode(out, slice_model(weld_exact(m), s), s);
    return out.str();
}

struct Loop {
    int layer = 0;
    std::string type;
    double x = 0.0, y = 0.0; // where it starts
};

// Every run of extruding moves of one wall type, per layer, with its start.
std::vector<Loop> wall_loops(const std::string& gcode) {
    std::vector<Loop> loops;
    std::istringstream in(gcode);
    std::string line, type;
    int layer = -1;
    double x = 0.0, y = 0.0;
    bool open = false;
    for (; std::getline(in, line);) {
        if (line == ";LAYER_CHANGE") {
            ++layer;
            open = false;
            continue;
        }
        if (line.rfind(";TYPE:", 0) == 0) {
            type = line.substr(6);
            open = false;
            continue;
        }
        if (line.rfind("G1 ", 0) != 0 && line.rfind("G0 ", 0) != 0)
            continue;
        double nx = x, ny = y;
        bool extrudes = false, moves = false;
        std::istringstream words(line.substr(0, line.find(';')));
        for (std::string w; words >> w;) {
            if (w[0] == 'X')
                nx = std::stod(w.substr(1)), moves = true;
            else if (w[0] == 'Y')
                ny = std::stod(w.substr(1)), moves = true;
            else if (w[0] == 'E' && std::stod(w.substr(1)) > 0.0)
                extrudes = true;
        }
        const bool wall = type == "External perimeter" || type == "Perimeter";
        if (extrudes && moves && wall) {
            if (!open)
                loops.push_back({layer, type, x, y});
            open = true;
        } else if (moves) {
            open = false;
        }
        x = nx, y = ny;
    }
    return loops;
}

// The outer wall's start in every layer from the second.
std::vector<Loop> outer_starts(const std::string& gcode) {
    std::vector<Loop> out;
    for (const Loop& l : wall_loops(gcode))
        if (l.type == "External perimeter" && l.layer >= 1 && (out.empty() || out.back().layer != l.layer))
            out.push_back(l);
    return out;
}

} // namespace

TEST_CASE("perimeters.md P3: a 0.1 mm wall prints nothing", "[perimeters-spec]") {
    TriangleMesh m;
    add_box(m, 90, 99.95f, 0, 110, 100.05f, 5);
    Settings s = test_printer();
    const SlicedModel sliced = slice_model(weld_exact(m), s);
    std::size_t paths = 0;
    for (const LayerWalls& w : sliced.walls)
        paths += w.toolpaths.size();
    CHECK(paths == 0);
}

TEST_CASE("perimeters.md P4: the hole's beads, innermost first, then the outside's", "[perimeters-spec]") {
    // A 20 mm frame with a 10 mm hole, three walls.
    TriangleMesh m;
    add_box(m, 90, 90, 0, 95, 110, 5);
    add_box(m, 105, 90, 0, 110, 110, 5);
    add_box(m, 95, 90, 0, 105, 95, 5);
    add_box(m, 95, 105, 0, 105, 110, 5);
    Settings s = test_printer();
    s.wall_count = 3;
    std::vector<double> reach; // each loop's distance from the centre, square measure, layer 10
    for (const Loop& l : wall_loops(gcode_of(m, s)))
        if (l.layer == 10)
            reach.push_back(std::max(std::abs(l.x - kCx), std::abs(l.y - kCy)));
    REQUIRE(reach.size() == 6);
    // The hole's band (5 mm out) first, then the outside's (10 mm out), each
    // inside out: away from the hole towards it, then outwards.
    for (int k = 0; k < 3; ++k)
        CHECK(reach[std::size_t(k)] < 7.5);
    for (int k = 3; k < 6; ++k)
        CHECK(reach[std::size_t(k)] > 7.5);
    CHECK(reach[0] > reach[1]);
    CHECK(reach[1] > reach[2]);
    CHECK(reach[3] < reach[4]);
    CHECK(reach[4] < reach[5]);
}

TEST_CASE("perimeters.md P5-P7, P9: where the outer wall starts", "[perimeters-spec]") {
    Settings s = test_printer();
    SECTION("P5: aligned on the cube: the corner of greatest X and Y, in every layer") {
        TriangleMesh cube;
        add_box(cube, 90, 90, 0, 110, 110, 10);
        const std::vector<Loop> starts = outer_starts(gcode_of(cube, s));
        REQUIRE(starts.size() >= 40);
        for (const Loop& l : starts) {
            INFO("layer " << l.layer << " at " << l.x << ", " << l.y);
            CHECK(l.x > 109.0);
            CHECK(l.y > 109.0);
            CHECK_THAT(l.x, WithinAbs(starts.front().x, 0.01));
            CHECK_THAT(l.y, WithinAbs(starts.front().y, 0.01));
        }
    }
    const TriangleMesh cyl = cylinder(10, 5, 128);
    SECTION("P6: aligned on the cylinder: the front (least Y)") {
        for (const Loop& l : outer_starts(gcode_of(cyl, s))) {
            INFO("layer " << l.layer << " at " << l.x << ", " << l.y);
            CHECK(l.y < kCy - 9.5);
        }
    }
    SECTION("P7: rear: the greatest Y") {
        s.seam = SeamMode::Rear;
        for (const Loop& l : outer_starts(gcode_of(cyl, s))) {
            INFO("layer " << l.layer << " at " << l.x << ", " << l.y);
            CHECK(l.y > kCy + 9.5);
        }
    }
    SECTION("P9: random: a different angle in each layer") {
        s.seam = SeamMode::Random;
        std::set<int> angles;
        const std::vector<Loop> starts = outer_starts(gcode_of(cyl, s));
        for (const Loop& l : starts)
            angles.insert(int(std::lround(std::atan2(l.y - kCy, l.x - kCx) * 180.0 / kPi / 10.0)));
        REQUIRE(starts.size() >= 20);
        CHECK(angles.size() >= starts.size() / 3); // spread over many tens of degrees
    }
}
