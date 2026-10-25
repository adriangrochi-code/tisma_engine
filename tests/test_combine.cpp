#include "infill/combine.hpp"

#include "gcode/check.hpp"
#include "gcode/writer.hpp"
#include "slice/pipeline.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <cstdint>
#include <map>
#include <numbers>
#include <sstream>
#include <string>

using namespace tisma;
using Catch::Matchers::WithinAbs;
using Runs = std::vector<std::array<std::size_t, 2>>;

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

// The spec's base settings (infill_patterns.md): 0.4 nozzle, 0.2 layers, two
// walls, three solid layers up and down, rectilinear 20 %, no skirt or brim.
Settings base() {
    Settings s;
    s.gpu = GpuMode::Off;
    s.nozzle_diameter = 0.4;
    s.layer_height = 0.2;
    s.first_layer_height = 0.2;
    s.wall_count = 2;
    s.top_layers = 3;
    s.bottom_layers = 3;
    s.infill_density = 20.0;
    s.skirt_loops = 0;
    s.relative_extrusion = true;
    s.min_layer_time = 0.0;
    return s;
}

// Per layer (by its ;Z:), the sparse infill's height and filament per mm.
struct Sparse {
    double height = 0.0, filament = 0.0, length = 0.0;
};

std::map<long, Sparse> sparse_by_layer(const std::string& gcode) {
    std::map<long, Sparse> out;
    double x = 0, y = 0, layer_z = 0, height = 0;
    std::string type;
    std::istringstream in(gcode);
    for (std::string line; std::getline(in, line);) {
        if (line.rfind(";Z:", 0) == 0)
            layer_z = std::stod(line.substr(3));
        else if (line.rfind(";HEIGHT:", 0) == 0)
            height = std::stod(line.substr(8));
        else if (line.rfind(";TYPE:", 0) == 0)
            type = line.substr(6);
        else if (line.rfind("G1 ", 0) == 0) {
            double nx = x, ny = y, e = 0;
            std::istringstream words(line.substr(3));
            for (std::string w; words >> w;) {
                if (w[0] == 'X')
                    nx = std::stod(w.substr(1));
                else if (w[0] == 'Y')
                    ny = std::stod(w.substr(1));
                else if (w[0] == 'E')
                    e = std::stod(w.substr(1));
            }
            const double length = std::hypot(nx - x, ny - y);
            if (type == "Internal infill" && e > 0 && length > 0) {
                Sparse& s = out[std::lround(layer_z * 1000.0)];
                s.height = height;
                s.filament += e;
                s.length += length;
            }
            x = nx, y = ny;
        }
    }
    return out;
}

double minutes(const std::string& gcode) {
    const std::size_t at = gcode.find("; estimated printing time = ");
    REQUIRE(at != std::string::npos);
    double total = 0.0, value = 0.0;
    for (std::size_t i = at + 28; i < gcode.size() && gcode[i] != '\n'; ++i) {
        const char c = gcode[i];
        if (c >= '0' && c <= '9')
            value = value * 10 + (c - '0');
        else if (c == 'd' || c == 'h' || c == 'm' || c == 's') {
            total += value * (c == 'd' ? 1440.0 : c == 'h' ? 60.0 : c == 'm' ? 1.0 : 1.0 / 60.0);
            value = 0.0;
        }
    }
    return total;
}

} // namespace

TEST_CASE("combined infill: the runs of layers", "[combine]") {
    std::vector<Layer> layers;
    for (int i = 0; i < 10; ++i)
        layers.push_back({0.2 * i, 0.2 * (i + 1)});
    Settings s = base();
    CHECK(combine_runs(layers, s).empty()); // 1: each layer its own
    s.infill_combine_layers = 2;
    CHECK(combine_runs(layers, s) == Runs{{1, 2}, {3, 4}, {5, 6}, {7, 8}});
    s.infill_combine_layers = 3; // 0.6 mm would pass the 0.4 nozzle (rule 19): two
    CHECK(combine_runs(layers, s) == Runs{{1, 2}, {3, 4}, {5, 6}, {7, 8}});
    s.nozzle_diameter = 0.8;
    CHECK(combine_runs(layers, s) == Runs{{1, 3}, {4, 6}, {7, 9}});
    s.infill_combine_max_height = {0.5, false}; // mm: two
    CHECK(combine_runs(layers, s) == Runs{{1, 2}, {3, 4}, {5, 6}, {7, 8}});
    s.infill_combine_layers = 1;
    s.infill_combine_auto = true; // as many as fit in 0.5 mm
    CHECK(combine_runs(layers, s) == Runs{{1, 2}, {3, 4}, {5, 6}, {7, 8}});
    s.infill_combine_max_height = {100.0, true}; // the 0.8 nozzle: four
    CHECK(combine_runs(layers, s) == Runs{{1, 4}, {5, 8}});
    CHECK_THAT(combine_max_height(s), WithinAbs(0.8, 1e-12));
}

TEST_CASE("combined infill: the spec's cube (P11), sliced to checked G-code", "[combine]") {
    const IndexedMesh m = cube(20.0f);
    const Settings one = base();
    Settings two = base();
    two.infill_combine_layers = 2;
    std::ostringstream a, b;
    const SlicedModel sa = slice_model(m, one);
    const SlicedModel sb = slice_model(m, two);
    write_gcode(a, sa, one);
    write_gcode(b, sb, two);
    for (const auto* g : {&a, &b}) {
        const GcodeCheck check = check_gcode(g->str(), gcode_limits(g == &a ? sa : sb, g == &a ? one : two));
        INFO((check.problems.empty() ? std::string() : check.problems.front().message));
        CHECK(check.ok());
    }
    const std::map<long, Sparse> each = sparse_by_layer(a.str());
    const std::map<long, Sparse> combined = sparse_by_layer(b.str());
    // Sparse infill on the even layers from layer 4 (Z 1.0, 1.4, ...), 0.4 mm
    // high; layer 3 (Z 0.8) has none.
    CHECK(combined.count(800) == 0);
    REQUIRE(combined.count(1000) == 1);
    CHECK(combined.count(1200) == 0);
    REQUIRE(combined.count(1400) == 1);
    for (const auto& [z, s] : combined) {
        CHECK((z / 200) % 2 == 1); // Z 1.0, 1.4, ...: layers 4, 6, ...
        CHECK_THAT(s.height, WithinAbs(0.4, 1e-9));
    }
    // The filament per mm of a 0.4 mm layer: twice that of a 0.2 mm one.
    const Sparse& thin = each.at(1000);
    const Sparse& thick = combined.at(1000);
    CHECK_THAT((thick.filament / thick.length) / (thin.filament / thin.length), WithinAbs(2.0, 0.03));
    // The same infill in half the passes: less time.
    CHECK(minutes(b.str()) < minutes(a.str()));
}
