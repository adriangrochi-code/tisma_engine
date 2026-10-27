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
#include "io/stl.hpp"
#include "mesh/indexed_mesh.hpp"
#include "slice/pipeline.hpp"
#include "slice/printable.hpp"
#include "test_printer.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
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

// With TISMA_FUZZ_DUMP set to a folder, a failing run leaves its model, its
// settings and its G-code there, to run again with tisma-engine.
void dump(int run, const IndexedMesh& mesh, const Settings& s, const std::string& gcode) {
    const char* folder = std::getenv("TISMA_FUZZ_DUMP");
    if (!folder)
        return;
    const std::string base = std::string(folder) + "/run" + std::to_string(run);
    TriangleMesh triangles;
    for (const auto& t : mesh.triangles)
        triangles.add_triangle(mesh.vertices[t[0]], mesh.vertices[t[1]], mesh.vertices[t[2]]);
    std::ofstream(base + ".stl", std::ios::binary) << stl_binary(triangles);
    std::ofstream(base + ".json") << settings_json(s);
    if (!gcode.empty())
        std::ofstream(base + ".gcode") << gcode;
}

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
        Settings s = test_printer();
        s.layer_height = uni(0.08, 0.32);
        s.first_layer_height = uni(0.1, 0.4);
        s.wall_count = pick(7);
        s.wall_width = uni(0.3, 0.8);
        s.infill_density = pick(5) == 0 ? 0.0 : pick(5) == 0 ? 100.0 : uni(5, 60);
        s.infill_pattern = std::array{InfillPattern::Rectilinear, InfillPattern::Grid, InfillPattern::Gyroid,
                                      InfillPattern::Triangles, InfillPattern::Cubic,
                                      InfillPattern::AdaptiveCubic, InfillPattern::SupportCubic,
                                      InfillPattern::Lightning, InfillPattern::Stars, InfillPattern::AlignedRectilinear,
                                      InfillPattern::Zigzag, InfillPattern::Line, InfillPattern::Honeycomb,
                                      InfillPattern::Concentric, InfillPattern::Hilbert, InfillPattern::Archimedean,
                                      InfillPattern::Octagram}[std::size_t(pick(17))];
        s.infill_angle = uni(0, 180);
        s.top_layers = pick(6);
        s.bottom_layers = pick(6);
        s.monotonic_top = pick(2) == 0;
        s.support = pick(3) == 0;
        s.support_angle = pick(2) == 0 ? 0.0 : uni(20, 70);
        if (pick(2) == 0) { // organic supports
            s.support_style = SupportStyle::Organic;
            s.tree_angle = uni(0, 85);
            s.tree_angle_preferred = uni(0, 85);
            s.tree_branch_diameter = uni(0.5, 8);
            s.tree_branch_diameter_angle = uni(0, 15);
            s.tree_double_wall_diameter = pick(3) == 0 ? 0.0 : uni(1, 10);
            s.tree_branch_distance = uni(0.3, 4);
            s.tree_tip_diameter = uni(0.2, 3);
            s.tree_top_rate = uni(1, 100);
        }
        s.brim_width = pick(3) == 0 ? uni(0, 8) : 0.0;
        s.skirt_loops = pick(4);
        s.skirt_distance = uni(1, 6);
        s.retract_length = pick(4) == 0 ? 0.0 : uni(0.2, 6);
        s.retract_lift = pick(3) == 0 ? uni(0, 1) : 0.0;
        s.relative_extrusion = pick(2) == 0;
        s.seam = std::array{SeamMode::Aligned, SeamMode::Nearest, SeamMode::Random, SeamMode::Rear}[std::size_t(pick(4))];
        if (pick(8) == 0 && !s.support) // vase mode, never with supports
            s.vase = true;
        if (pick(3) == 0) { // the scarf seam
            s.scarf = pick(2) == 0 ? ScarfMode::Contours : ScarfMode::Everywhere;
            s.scarf_only_smooth = pick(2) == 0;
            s.scarf_start_height = pick(2) == 0 ? 0.0 : uni(0, 90);
            s.scarf_length = uni(0.5, 30);
            s.scarf_entire_loop = pick(4) == 0;
            s.scarf_max_segment = uni(0.2, 3);
            s.scarf_inner = pick(3) == 0;
        }
        s.detect_overhangs = pick(4) != 0;
        if (pick(4) == 0) { // templates around the layer changes, and colour changes and pauses
            s.gcode_before_layer = pick(2) == 0 ? ";before [layer_num] [layer_z]" : "G91\nG1 Z0.5\nG90";
            s.gcode_after_layer = pick(2) == 0 ? "" : ";after {layer_z}";
            HeightChange c;
            c.z = uni(0.5, 8);
            (pick(2) == 0 ? c.color_change : c.pause) = true;
            s.changes.push_back(c);
        }
        if (pick(5) == 0) { // the G-code's extras
            s.z_offset = uni(-0.3, 0.3);
            s.volumetric_e = pick(2) == 0;
        }
        if (pick(5) == 0) { // a raft
            s.raft_layers = 1 + pick(5);
            s.raft_gap = pick(3) == 0 ? 0.0 : uni(0.05, 0.3);
            s.raft_expansion = uni(0, 4);
            s.raft_first_layer_density = uni(30, 100);
            s.raft_first_layer_expansion = uni(0, 5);
            s.first_layer_over_raft = pick(2) == 0 ? Speed{} : Speed{uni(10, 60), false};
        }
        if (pick(4) == 0) { // ironing
            s.ironing = true;
            s.ironing_surfaces = std::array{IroningSurfaces::Top, IroningSurfaces::Topmost,
                                            IroningSurfaces::Solid}[std::size_t(pick(3))];
            s.ironing_flow = uni(5, 40);
            s.ironing_spacing = uni(0.05, 0.4);
            s.ironing_speed = uni(5, 60);
        }
        if (pick(4) == 0) { // walls of one width
            s.wall_generator = WallGenerator::Fixed;
            s.wall_gap_fill = pick(4) != 0;
        }
        if (pick(4) == 0) { // arcs over the overhangs
            s.arcs = true;
            s.arcs_overlap = uni(0, 40);
            s.arcs_speed = pick(3) == 0 ? 0.0 : uni(2, 30);
            s.arcs_max_distance = pick(2) == 0 ? 0.0 : uni(1, 20);
        }
        // Speeds of their own and the bridges' flow (speeds_accels.md).
        if (pick(3) == 0) {
            s.bridge_speed = pick(2) == 0 ? 0.0 : uni(5, 120);
            s.over_bridges_speed = pick(2) == 0 ? Speed{uni(10, 150), true} : Speed{uni(5, 120), false};
            s.first_layer_infill_speed = pick(2) == 0 ? Speed{uni(10, 150), true} : Speed{uni(5, 120), false};
            s.travel_z_speed = pick(2) == 0 ? 0.0 : uni(2, 60);
            s.infill_max_speed = pick(2) == 0 ? 0.0 : uni(10, 200);
            s.infill_max_crossing_speed = pick(2) == 0 ? 0.0 : uni(10, 200);
            s.bridge_flow_ratio = uni(0.5, 1.5);
        }
        if (pick(4) == 0) { // walls by how much rests on the layer below
            s.dynamic_overhangs = true;
            for (Speed& v : s.overhang_speeds)
                v = pick(2) == 0 ? Speed{uni(10, 120), true} : Speed{uni(2, 60), false};
        }
        if (pick(4) == 0) { // the flow equalizer
            s.flow_slope_up = pick(2) == 0 ? 0.0 : uni(0.2, 20);
            s.flow_slope_down = pick(2) == 0 ? 0.0 : uni(0.2, 20);
        }
        if (pick(3) == 0) { // accelerations by kind
            s.accel_default = pick(3) == 0 ? 0.0 : uni(500, 5000);
            for (double* a : {&s.accel_walls, &s.accel_outer_wall, &s.accel_infill, &s.accel_solid_infill,
                              &s.accel_top_solid_infill, &s.accel_bridges, &s.accel_first_layer, &s.accel_travel,
                              &s.accel_short_travel})
                *a = pick(2) == 0 ? 0.0 : uni(300, 10000);
        }
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
        // Variable layer heights by Z (docs/VARIABLE_LAYERS.md): by slope, by a list, within the machine's.
        if (pick(3) == 0)
            s.cusp_height = uni(0.02, 0.2);
        if (pick(4) == 0) {
            double z = uni(0.3, 3.0);
            for (int i = 0, n = 1 + pick(3); i < n; ++i, z += uni(0.3, 3.0))
                s.layer_heights.push_back({z, uni(0.03, 0.6)});
        }
        if (pick(4) == 0)
            s.min_layer_height = uni(0.04, 0.12);
        if (pick(4) == 0)
            s.max_layer_height = uni(0.12, 0.5);
        // Wavy nonplanar infill (docs/NONPLANAR_INFILL.md).
        if (pick(4) == 0) {
            s.infill_nonplanar = true;
            s.infill_nonplanar_amplitude = uni(50, 400);
            s.infill_nonplanar_wavelength = uni(1, 6);
        }
        // Shell thickness and solid layers every N (docs/INFILL.md).
        if (pick(5) == 0) {
            s.top_min_thickness = uni(0, 2);
            s.bottom_min_thickness = uni(0, 2);
            s.infill_solid_every = 2 + pick(8);
        }
        // Combined sparse infill (docs/INFILL.md).
        if (pick(4) == 0) {
            s.infill_combine_layers = 2 + pick(3);
            s.infill_combine_auto = pick(3) == 0;
            s.infill_combine_max_height = pick(2) == 0 ? Speed{uni(50, 150), true} : Speed{uni(0.1, 0.8), false};
        }
        if (pick(4) == 0) {
            s.infill_dense_under_top = true;
            s.infill_dense_density = uni(10, 100);
        }
        // Z anti-aliasing (docs/ZAA.md).
        if (pick(3) == 0) {
            s.zaa = true;
            s.zaa_min_thickness = uni(10, 90);
            s.zaa_raise = pick(2) == 0;
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
                std::printf("  zaa %d min %.0f raise %d detail %.3f tol %.3f margin %.2f cusp %.3f layers %.3f-%.3f steps",
                            int(s.zaa), s.zaa_min_thickness, int(s.zaa_raise), s.detail_height, s.detail_tolerance,
                            s.detail_margin, s.cusp_height, s.min_layer_height, s.max_layer_height);
                for (const auto& step : s.layer_heights)
                    std::printf(" %.3f:%.3f", step[0], step[1]);
                std::printf("\n");
                std::printf("  vase %d scarf %d smooth %d start %.0f length %.2f entire %d segment %.2f inner %d overhangs %d\n",
                            int(s.vase), int(s.scarf), int(s.scarf_only_smooth), s.scarf_start_height, s.scarf_length,
                            int(s.scarf_entire_loop), s.scarf_max_segment, int(s.scarf_inner), int(s.detect_overhangs));
                std::printf("  arcs %d overlap %.1f speed %.1f max %.2f\n", int(s.arcs), s.arcs_overlap, s.arcs_speed,
                            s.arcs_max_distance);
                std::printf("  flavor %d flow caps %.1f %.1f accel %.0f %.0f chamber %.0f/%.0f changes", int(s.flavor),
                            s.machine_max_flow, s.filament_max_flow, s.acceleration, s.travel_acceleration,
                            s.chamber_temperature, s.max_chamber_temp);
                for (const HeightChange& h : s.changes)
                    std::printf(" [z %.2f%s%s%s%s%s%s%s%s]", h.z, h.temperature ? " T" : "", h.flow ? " flow" : "",
                                h.speed ? " speed" : "", h.retract_length ? " retract" : "",
                                h.volumetric_flow ? " volumetric" : "", h.acceleration ? " accel" : "",
                                h.pressure_advance ? " pa" : "", h.fan ? " fan" : "");
                std::printf("\n");
                dump(run, mesh, s, out.str());
            }
        } catch (const std::exception& e) {
            ++failures;
            std::printf("run %d (%s): exception %s\n", run, name, e.what());
            dump(run, mesh, s, {});
        }
    }
    std::printf("%d runs, %d rejected as unprintable, %d failures\n", runs, rejected, failures);
    return failures != 0;
}
