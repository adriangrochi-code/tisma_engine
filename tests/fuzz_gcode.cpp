// A random search for G-code the checker rejects: random settings and shapes
// go through slicing, writing and check_gcode (docs/GCODE_CHECK.md). Models
// the slicing refuses as unprintable are counted, not failed. Used by the
// maintenance blocks with new seeds (docs/DEVELOPMENT.md, "Mantenimiento");
// CTest runs a few cases so it keeps building and working.
//
//   tisma_fuzz_gcode [runs] [seed]   exit code 1 if any case fails
#include "config/settings.hpp"
#include "gcode/check.hpp"
#include "gcode/writer.hpp"
#include "mesh/indexed_mesh.hpp"
#include "slice/pipeline.hpp"
#include "slice/printable.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <numbers>
#include <random>
#include <sstream>
using namespace tisma;
namespace {

void add_box(IndexedMesh& m, float x0, float y0, float z0, float x1, float y1, float z1) {
    const auto base = std::uint32_t(m.vertices.size());
    for (float z : {z0, z1})
        for (const auto& p : {std::array<float, 2>{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}})
            m.vertices.push_back({p[0], p[1], z});
    auto quad = [&](std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d) {
        m.triangles.push_back({base + a, base + b, base + c});
        m.triangles.push_back({base + a, base + c, base + d});
    };
    quad(0, 3, 2, 1); quad(4, 5, 6, 7);
    for (std::uint32_t i = 0; i < 4; ++i) quad(i, (i + 1) % 4, 4 + (i + 1) % 4, 4 + i);
}
// UV sphere / cylinder with outward facing triangles.
IndexedMesh revolve(int seg, int rings, float r, float h, bool sphere, float cx, float cy) {
    IndexedMesh m;
    for (int i = 0; i <= rings; ++i) {
        const double t = double(i) / rings;
        const double phi = std::numbers::pi * t;
        const double rad = sphere ? r * std::sin(phi) : r;
        const double z = sphere ? r - r * std::cos(phi) : h * t;
        for (int j = 0; j < seg; ++j) {
            const double a = 2 * std::numbers::pi * j / seg;
            m.vertices.push_back({float(cx + rad * std::cos(a)), float(cy + rad * std::sin(a)), float(z)});
        }
    }
    auto v = [&](int i, int j) { return std::uint32_t(i * seg + (j % seg)); };
    for (int i = 0; i < rings; ++i)
        for (int j = 0; j < seg; ++j) {
            m.triangles.push_back({v(i, j), v(i, j + 1), v(i + 1, j + 1)});
            m.triangles.push_back({v(i, j), v(i + 1, j + 1), v(i + 1, j)});
        }
    if (!sphere) { // caps
        const std::uint32_t b = std::uint32_t(m.vertices.size());
        m.vertices.push_back({cx, cy, 0.0f});
        m.vertices.push_back({cx, cy, h});
        for (int j = 0; j < seg; ++j) {
            m.triangles.push_back({b, v(0, j + 1), v(0, j)});
            m.triangles.push_back({b + 1, v(rings, j), v(rings, j + 1)});
        }
    }
    return m;
}
} // namespace

int main(int argc, char** argv) {
    const int runs = argc > 1 ? std::atoi(argv[1]) : 200;
    std::mt19937_64 rng(argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 1);
    auto uni = [&](double a, double b) { return std::uniform_real_distribution<double>(a, b)(rng); };
    auto pick = [&](int n) { return int(std::uniform_int_distribution<int>(0, n - 1)(rng)); };
    std::vector<std::pair<const char*, IndexedMesh>> shapes;
    { IndexedMesh m; add_box(m, 0, 0, 0, 10, 10, 6); shapes.push_back({"box", m}); }
    { IndexedMesh m; add_box(m, 0, 0, 0, 10, 2, 3); add_box(m, 0, 8, 0, 10, 10, 3); add_box(m, 0, 2, 0, 2, 8, 3); add_box(m, 8, 2, 0, 10, 8, 3); shapes.push_back({"ring", m}); }
    { IndexedMesh m; add_box(m, 0, 0, 0, 3, 3, 3); add_box(m, 0, 0, 3, 12, 8, 4); shapes.push_back({"table", m}); }
    { IndexedMesh m; add_box(m, 0, 0, 0, 0.5f, 20, 5); add_box(m, 3, 0, 0, 3.9f, 20, 5); shapes.push_back({"thin walls", m}); }
    shapes.push_back({"sphere", revolve(96, 48, 8.0f, 0, true, 5, 5)});
    shapes.push_back({"cylinder", revolve(128, 8, 5.0f, 7.0f, false, 0, 0)});
    int failures = 0, rejected = 0;
    for (int run = 0; run < runs; ++run) {
        Settings s;
        s.layer_height = uni(0.08, 0.32);
        s.first_layer_height = uni(0.1, 0.4);
        s.wall_count = pick(7);
        s.wall_width = uni(0.3, 0.8);
        s.infill_density = pick(5) == 0 ? 0.0 : pick(5) == 0 ? 100.0 : uni(5, 60);
        s.infill_pattern = std::array{InfillPattern::Rectilinear, InfillPattern::Grid, InfillPattern::Gyroid}[std::size_t(pick(3))];
        s.infill_angle = uni(0, 180);
        s.top_layers = pick(6);
        s.bottom_layers = pick(6);
        s.monotonic_top = pick(2) == 0;
        s.support = pick(3) == 0;
        s.support_angle = pick(2) == 0 ? 0.0 : uni(20, 70);
        s.brim_width = pick(3) == 0 ? uni(0, 8) : 0.0;
        s.skirt_loops = pick(4);
        s.skirt_distance = uni(1, 6);
        s.retract_length = pick(4) == 0 ? 0.0 : uni(0.2, 6);
        s.retract_lift = pick(3) == 0 ? uni(0, 1) : 0.0;
        s.relative_extrusion = pick(2) == 0;
        s.seam = std::array{SeamMode::Aligned, SeamMode::Nearest, SeamMode::Random, SeamMode::Rear}[std::size_t(pick(4))];
        s.min_layer_time = uni(0, 30);
        s.filament_diameter = pick(4) == 0 ? 2.85 : 1.75;
        s.xy_offset = pick(4) == 0 ? uni(-0.2, 0.2) : 0.0;
        s.first_layer_shrink = pick(4) == 0 ? uni(0, 0.3) : 0.0;
        s.max_speed = uni(40, 300);
        s.travel_speed = uni(80, 400);
        // Layer heights by zone (docs/LOCAL_LAYERS.md): fine sub-layers where the part changes.
        if (pick(3) == 0) {
            s.detail_height = s.layer_height / double(2 + pick(3));
            s.detail_tolerance = uni(0.01, 0.2);
            s.detail_margin = pick(2) == 0 ? 0.0 : uni(0.2, 3.0);
        }
        // The machine's capabilities and the firmware's commands (docs/CATALOG.md, "Capacidades").
        s.flavor = std::array{GcodeFlavor::Marlin, GcodeFlavor::Klipper, GcodeFlavor::RepRap}[std::size_t(pick(3))];
        s.machine_max_flow = pick(3) == 0 ? uni(2, 30) : 0.0;
        s.filament_max_flow = pick(4) == 0 ? uni(2, 30) : 0.0;
        s.pressure_advance = pick(4) == 0 ? uni(0, 0.1) : 0.0;
        s.acceleration = uni(300, 10000);
        s.travel_acceleration = uni(300, 10000);
        if (pick(4) == 0) {
            s.chamber_temperature = uni(30, 60);
            s.max_chamber_temp = pick(2) == 0 ? uni(40, 70) : 0.0;
        }
        // Height changes like the calibration towers' (docs/CALIBRATION.md).
        for (int i = pick(4); i > 0; --i) {
            HeightChange c;
            c.z = uni(0, 8);
            switch (pick(8)) {
            case 0: c.temperature = uni(180, 260); break;
            case 1: c.flow = uni(80, 120); break;
            case 2: c.speed = uni(50, 300); break;
            case 3: c.retract_length = uni(0, 5); break;
            case 4: c.volumetric_flow = uni(2, 30); break;
            case 5: c.acceleration = uni(500, 10000); break;
            case 6: c.pressure_advance = uni(0, 0.1); break;
            default: c.fan = uni(0, 100); break;
            }
            s.changes.push_back(c);
        }
        std::stable_sort(s.changes.begin(), s.changes.end(),
                         [](const HeightChange& a, const HeightChange& b) { return a.z < b.z; });
        s.gpu = GpuMode::Off;
        const auto& [name, mesh] = shapes[std::size_t(pick(int(shapes.size())))];
        try {
            const SlicedModel model = slice_model(mesh, s);
            if (!unprintable_reason(model, s).empty()) {
                ++rejected;
                continue;
            }
            std::ostringstream out;
            write_gcode(out, model, s);
            const GcodeCheck c = check_gcode(out.str(), gcode_limits(model, s));
            if (!c.ok()) {
                ++failures;
                std::printf("run %d (%s): %zu problems; first: line %zu: %s\n", run, name, c.problem_count,
                            c.problems[0].line, c.problems[0].message.c_str());
                std::printf("  layer %.3f first %.3f walls %d width %.3f infill %.1f pattern %d support %d brim %.2f skirt %d retract %.2f lift %.2f relE %d seam %d minlt %.1f fil %.2f xyoff %.3f shrink %.3f maxv %.0f travel %.0f\n",
                            s.layer_height, s.first_layer_height, s.wall_count, s.wall_width, s.infill_density, int(s.infill_pattern), int(s.support), s.brim_width, s.skirt_loops, s.retract_length, s.retract_lift, int(s.relative_extrusion), int(s.seam), s.min_layer_time, s.filament_diameter, s.xy_offset, s.first_layer_shrink, s.max_speed, s.travel_speed);
                std::printf("  detail %.3f tol %.3f margin %.2f\n", s.detail_height, s.detail_tolerance, s.detail_margin);
                std::printf("  flavor %d flow caps %.1f %.1f accel %.0f %.0f chamber %.0f/%.0f changes", int(s.flavor),
                            s.machine_max_flow, s.filament_max_flow, s.acceleration, s.travel_acceleration,
                            s.chamber_temperature, s.max_chamber_temp);
                for (const HeightChange& h : s.changes)
                    std::printf(" [z %.2f%s%s%s%s%s%s%s%s]", h.z, h.temperature ? " T" : "", h.flow ? " flow" : "",
                                h.speed ? " speed" : "", h.retract_length ? " retract" : "",
                                h.volumetric_flow ? " volumetric" : "", h.acceleration ? " accel" : "",
                                h.pressure_advance ? " pa" : "", h.fan ? " fan" : "");
                std::printf("\n");
            }
        } catch (const std::exception& e) {
            ++failures;
            std::printf("run %d (%s): exception %s\n", run, name, e.what());
        }
    }
    std::printf("%d runs, %d rejected as unprintable, %d failures\n", runs, rejected, failures);
    return failures != 0;
}
