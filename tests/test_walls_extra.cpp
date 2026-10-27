// Wall options (docs/spec/features/walls_extra.md): staggered inner seams
// (W5), one wall on top (W7) and on the first layer (W8).

#include "config/settings.hpp"
#include "gcode/check.hpp"
#include "gcode/writer.hpp"
#include "mesh/indexed_mesh.hpp"
#include "slice/pipeline.hpp"
#include "test_printer.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <numbers>
#include <sstream>
#include <string>
#include <vector>

using namespace tisma;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

// Boxes from (x0, y0, z0) to (x1, y1, z1), in one mesh.
IndexedMesh boxes(const std::vector<std::array<float, 6>>& list) {
    IndexedMesh m;
    for (const auto& b : list) {
        const auto base = std::uint32_t(m.vertices.size());
        for (float z : {b[2], b[5]})
            for (const auto& p : {std::array<float, 2>{b[0], b[1]}, {b[3], b[1]}, {b[3], b[4]}, {b[0], b[4]}})
                m.vertices.push_back({p[0], p[1], z});
        const auto quad = [&](std::uint32_t a, std::uint32_t c, std::uint32_t d, std::uint32_t e) {
            m.triangles.push_back({base + a, base + c, base + d});
            m.triangles.push_back({base + a, base + d, base + e});
        };
        quad(0, 3, 2, 1);
        quad(4, 5, 6, 7);
        quad(0, 1, 5, 4);
        quad(1, 2, 6, 5);
        quad(2, 3, 7, 6);
        quad(3, 0, 4, 7);
    }
    return m;
}

// A cylinder of radius r around (100, 100) from z 0 to h, n sides.
IndexedMesh cylinder(double r, double h, int n = 96) {
    IndexedMesh m;
    for (int ring = 0; ring < 2; ++ring)
        for (int i = 0; i < n; ++i) {
            const double a = 2.0 * std::numbers::pi * i / n;
            m.vertices.push_back({float(100.0 + r * std::cos(a)), float(100.0 + r * std::sin(a)), float(ring * h)});
        }
    m.vertices.push_back({100.0f, 100.0f, 0.0f});
    m.vertices.push_back({100.0f, 100.0f, float(h)});
    const std::uint32_t bottom = std::uint32_t(2 * n), top = bottom + 1;
    for (int i = 0; i < n; ++i) {
        const std::uint32_t a = std::uint32_t(i), b = std::uint32_t((i + 1) % n);
        m.triangles.push_back({a, b, b + std::uint32_t(n)});
        m.triangles.push_back({a, b + std::uint32_t(n), a + std::uint32_t(n)});
        m.triangles.push_back({bottom, b, a});
        m.triangles.push_back({top, a + std::uint32_t(n), b + std::uint32_t(n)});
    }
    return m;
}

// The spec's base: 0.2 layers, 2 walls, no infill, no solid layers, no skirt.
Settings base() {
    Settings s = test_printer();
    s.layer_height = s.first_layer_height = 0.2;
    s.wall_count = 2;
    s.infill_density = 0.0;
    s.top_layers = s.bottom_layers = 0;
    s.skirt_loops = 0;
    s.brim_width = 0.0;
    return s;
}

std::string gcode_of(const IndexedMesh& mesh, const Settings& s) {
    const SlicedModel model = slice_model(mesh, s);
    std::ostringstream out;
    write_gcode(out, model, s);
    const GcodeCheck check = check_gcode(out.str(), gcode_limits(model, s));
    INFO((check.problems.empty() ? std::string() : check.problems.front().message));
    CHECK(check.ok());
    return out.str();
}

// One run of extrusions of one type, without a travel in between.
struct Run {
    int layer = -1;
    std::string type;
    double x0 = 0, y0 = 0; // where it starts
    double length = 0.0;
};

std::vector<Run> runs_of(const std::string& gcode) {
    std::vector<Run> out;
    double x = 0, y = 0, last_e = 0.0; // E is absolute (M82)
    int layer = -1;
    std::string type;
    bool open = false;
    std::istringstream in(gcode);
    for (std::string line; std::getline(in, line);) {
        if (line.starts_with(";LAYER_CHANGE")) {
            ++layer;
            open = false;
            continue;
        }
        if (line.starts_with(";TYPE:")) {
            type = line.substr(6);
            open = false;
            continue;
        }
        const std::string code = line.substr(0, line.find(';'));
        if (code.starts_with("G92")) { // E restarts at each layer
            last_e = 0.0;
            continue;
        }
        if (!code.starts_with("G1 "))
            continue;
        double nx = x, ny = y, e = 0.0;
        bool xy = false;
        std::istringstream words(code.substr(3));
        for (std::string w; words >> w;) {
            const double v = std::atof(w.c_str() + 1);
            if (w[0] == 'X')
                nx = v, xy = true;
            else if (w[0] == 'Y')
                ny = v, xy = true;
            else if (w[0] == 'E')
                e = v;
        }
        // An extrusion moves in X or Y to a higher E than the last.
        if (code.find(" E") == std::string::npos) {
            if (xy)
                open = false;
        } else if (xy && e > last_e) {
            if (!open) {
                out.push_back({layer, type, x, y, 0.0});
                open = true;
            }
            out.back().length += std::hypot(nx - x, ny - y);
        } else {
            open = false;
        }
        if (code.find(" E") != std::string::npos)
            last_e = e;
        x = nx, y = ny;
    }
    return out;
}

double length_of(const std::vector<Run>& runs, int layer, const std::string& type) {
    double total = 0.0;
    for (const Run& r : runs)
        if (r.layer == layer && r.type == type)
            total += r.length;
    return total;
}

} // namespace

TEST_CASE("walls: one wall on the first layer (walls_extra.md, rule 11, W8)", "[walls]") {
    Settings s = base();
    s.wall_count = 3;
    s.first_layer_one_wall = true;
    const std::vector<Run> runs = runs_of(gcode_of(boxes({{90, 90, 0, 110, 110, 20}}), s));
    CHECK(length_of(runs, 0, "Perimeter") == 0.0);
    CHECK_THAT(length_of(runs, 0, "External perimeter"), WithinAbs(78.34, 1.0));
    // The two inner walls, over the air where the first layer has only its outer wall.
    CHECK(length_of(runs, 1, "Perimeter") + length_of(runs, 1, "Overhang perimeter") > 140.0);
    CHECK(length_of(runs, 1, "External perimeter") > 0.0);
}

TEST_CASE("walls: the inner walls' seams staggered a bead per level (walls_extra.md, rule 3, W5)", "[walls]") {
    Settings s = base();
    s.wall_count = 4;
    s.seam = SeamMode::Aligned;
    const IndexedMesh part = cylinder(10.0, 5.0);
    // Where each wall of layer 10 starts, by its distance from the axis (outer first).
    const auto starts = [&](const Settings& settings) {
        std::map<double, std::array<double, 2>, std::greater<>> out;
        for (const Run& r : runs_of(gcode_of(part, settings)))
            if (r.layer == 10 && (r.type == "External perimeter" || r.type == "Perimeter"))
                out[std::hypot(r.x0 - 100.0, r.y0 - 100.0)] = {r.x0, r.y0};
        std::vector<std::array<double, 2>> list;
        for (const auto& [radius, p] : out)
            list.push_back(p);
        return list;
    };
    const std::vector<std::array<double, 2>> plain = starts(s);
    s.staggered_inner_seams = true;
    const std::vector<std::array<double, 2>> staggered = starts(s);
    REQUIRE(plain.size() == 4);
    REQUIRE(staggered.size() == 4);
    CHECK_THAT(staggered[0][0], WithinAbs(plain[0][0], 1e-3));
    CHECK_THAT(staggered[0][1], WithinAbs(plain[0][1], 1e-3));
    for (std::size_t k = 1; k < 4; ++k) {
        CAPTURE(k);
        const double dx = plain[k][0] - staggered[k][0], dy = plain[k][1] - staggered[k][1];
        CHECK_THAT(std::hypot(dx, dy), WithinAbs(double(k) * s.wall_width, 0.03));
        // Earlier on its counter-clockwise way round: the usual start comes after it.
        const double ax = staggered[k][0] - 100.0, ay = staggered[k][1] - 100.0;
        const double bx = plain[k][0] - 100.0, by = plain[k][1] - 100.0;
        CHECK(ax * by - ay * bx > 0.0);
    }
}

TEST_CASE("walls: one wall over the top surfaces (walls_extra.md, rule 10, W7)", "[walls]") {
    Settings s = base();
    s.wall_count = 3;
    s.top_layers = s.bottom_layers = 3;
    // A block of 30 x 20 x 4 mm with one of 10 x 20 x 4 mm on top.
    const IndexedMesh step = boxes({{85, 90, 0, 115, 110, 4}, {95, 90, 4, 105, 110, 8}});
    const std::vector<Run> none = runs_of(gcode_of(step, s));
    s.top_one_wall = TopOneWall::Top;
    const std::vector<Run> top = runs_of(gcode_of(step, s));
    s.top_one_wall = TopOneWall::Topmost;
    const std::vector<Run> topmost = runs_of(gcode_of(step, s));
    // The step's top (layer 19, Z 4): the inner walls round all of it, or
    // only round the block that goes on up (the spec: 186.51 and 106.42 mm).
    const double inner_none = length_of(none, 19, "Perimeter");
    const double inner_top = length_of(top, 19, "Perimeter");
    CHECK_THAT(inner_none, WithinRel(186.51, 0.03));
    CHECK_THAT(inner_top, WithinRel(106.42, 0.03));
    CHECK(length_of(top, 19, "Top solid infill") > length_of(none, 19, "Top solid infill") + 50.0);
    CHECK_THAT(length_of(top, 19, "External perimeter"), WithinAbs(length_of(none, 19, "External perimeter"), 1e-6));
    // The part's top (layer 39): no inner walls with top or topmost.
    CHECK(length_of(none, 39, "Perimeter") > 100.0);
    CHECK(length_of(top, 39, "Perimeter") == 0.0);
    CHECK(length_of(topmost, 39, "Perimeter") == 0.0);
    // Topmost leaves the step as it was.
    CHECK_THAT(length_of(topmost, 19, "Perimeter"), WithinAbs(inner_none, 1e-6));
    // A layer whose top is covered keeps its walls.
    CHECK_THAT(length_of(top, 10, "Perimeter"), WithinAbs(length_of(none, 10, "Perimeter"), 1e-6));
}
