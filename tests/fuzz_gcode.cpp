// A random search for G-code the checker rejects: random settings and shapes
// go through slicing, writing and check_gcode (docs/GCODE_CHECK.md). Models
// the slicing refuses as unprintable are counted, not failed. Used by the
// maintenance blocks with new seeds (docs/DEVELOPMENT.md, "Mantenimiento");
// CTest runs a few cases so it keeps building and working.
//
//   tisma_fuzz_gcode [runs] [seed]   exit code 1 if any case fails
#include "config/settings.hpp"
#include "gcode/binary.hpp"
#include "gcode/check.hpp"
#include "gcode/dialect.hpp"
#include "gcode/writer.hpp"
#include "io/stl.hpp"
#include "mesh/indexed_mesh.hpp"
#include "slice/pipeline.hpp"
#include "slice/placement.hpp"
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
        if (pick(4) == 0) { // skirt_brim_raft.md: brim types and gap, draft shield, the skirt's filament
            s.brim_type = std::array{BrimType::None, BrimType::Outer, BrimType::Inner, BrimType::OuterAndInner}[std::size_t(pick(4))];
            s.brim_gap = pick(2) == 0 ? uni(0, 2) : 0.0;
            s.draft_shield = std::array{DraftShield::Disabled, DraftShield::Limited, DraftShield::Enabled}[std::size_t(pick(3))];
            s.skirt_min_filament = pick(2) == 0 ? uni(0, 400) : 0.0;
        }
        s.skirt_distance = uni(1, 6);
        s.retract_length = pick(4) == 0 ? 0.0 : uni(0.2, 6);
        s.retract_lift = pick(3) == 0 ? uni(0, 1) : 0.0;
        s.relative_extrusion = pick(2) == 0;
        if (pick(3) == 0) { // retraction and travel options (retraction_travel.md)
            s.unretract_speed = pick(2) == 0 ? uni(5, 80) : 0.0;
            s.restart_extra = pick(2) == 0 ? uni(0, 1) : 0.0;
            s.retract_layer_change = pick(2) == 0;
            s.firmware_retraction = pick(4) == 0;
            s.wipe = !s.firmware_retraction && pick(2) == 0;
            s.before_wipe = pick(2) == 0 ? uni(0, 100) : 0.0;
            s.lift_above = pick(3) == 0 ? uni(0, 5) : 0.0;
            s.lift_below = pick(3) == 0 ? uni(0, 10) : 0.0;
            s.ramping_lift = pick(2) == 0;
            s.travel_slope = pick(4) == 0 ? 0.0 : uni(1, 60);
            s.max_lift = pick(3) == 0 ? 0.0 : uni(0.1, 2);
            s.avoid_crossing_walls = pick(3) != 0;
            s.max_detour = pick(2) == 0 ? Speed{} : Speed{uni(0, 300), pick(2) == 0};
            s.walls_clockwise = pick(2) == 0;
            s.staggered_inner_seams = pick(2) == 0;
            s.top_one_wall = std::array{TopOneWall::None, TopOneWall::Top, TopOneWall::Topmost}[std::size_t(pick(3))];
            s.first_layer_one_wall = pick(3) == 0;
            s.extra_on_overhangs = pick(3) == 0;
            if (pick(3) == 0) // the filament's own values
                s.filament_retraction = {{"retraction.length", pick(3) == 0 ? 0.0 : uni(0.2, 4)},
                                         {"retraction.lift", uni(0, 0.6)},
                                         {"retraction.min_travel", uni(0, 20)},
                                         {"retraction.wipe", s.firmware_retraction ? 0.0 : double(pick(2))}};
        }
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
            s.gcode_after_layer = pick(3) == 0   ? ""
                                  : pick(2) == 0 ? ";after {layer_z}"
                                                 : "{if layer_num % 2 == 0};even {layer_z * 2}{else};odd {digits(layer_z, 8, 3)}{endif}";
            s.filament_start_gcode = pick(2) == 0 ? "{global first = layer_z};filament {first_layer_print_size[0]}" : "";
            s.filament_end_gcode = pick(2) == 0 ? ";end {total_layer_count} {layer_num}" : "";
            HeightChange c;
            c.z = uni(0.5, 8);
            (pick(2) == 0 ? c.color_change : c.pause) = true;
            s.changes.push_back(c);
        }
        if (pick(5) == 0) { // the G-code's extras
            s.z_offset = uni(-0.3, 0.3);
            s.volumetric_e = pick(2) == 0;
            s.shrinkage_xy = pick(2) == 0 ? uni(0, 3) : 0.0;
            s.shrinkage_z = pick(2) == 0 ? uni(0, 3) : 0.0;
            s.verbose_comments = pick(2) == 0;
            s.gcode_arcs = pick(2) == 0;
            s.gcode_binary = pick(2) == 0;
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
            s.small_loop_speed = pick(2) == 0 ? Speed{uni(10, 150), true} : Speed{uni(5, 120), false};
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
        s.flavor = pick(4) == 0 ? std::array{GcodeFlavor::Repetier, GcodeFlavor::Smoothie, GcodeFlavor::Teacup,
                                             GcodeFlavor::MakerWare, GcodeFlavor::Sailfish, GcodeFlavor::Mach3,
                                             GcodeFlavor::Machinekit, GcodeFlavor::NoExtrusion}[std::size_t(pick(8))]
                                : std::array{GcodeFlavor::Marlin, GcodeFlavor::Klipper, GcodeFlavor::RepRap,
                                             GcodeFlavor::MarlinLegacy, GcodeFlavor::RepRapFirmware}[std::size_t(pick(5))];
        if (pick(4) == 0) { // the firmware's limits, a silent mode, M73
            s.limits_usage = std::array{LimitsUsage::TimeOnly, LimitsUsage::Emit, LimitsUsage::Ignore}[std::size_t(pick(3))];
            if (s.flavor == GcodeFlavor::Klipper && s.limits_usage == LimitsUsage::Emit)
                s.limits_usage = LimitsUsage::TimeOnly;
            s.max_accel_x = s.max_accel_y = pick(2) == 0 ? uni(200, 5000) : 0.0;
            s.retract_acceleration = pick(2) == 0 ? uni(200, 5000) : 0.0;
            s.jerk_x = s.jerk_y = pick(2) == 0 ? uni(1, 20) : 0.0;
            s.junction_deviation = pick(3) == 0 ? uni(0.005, 0.1) : 0.0;
            s.min_extrude_feed = pick(3) == 0 ? uni(0, 5) : 0.0;
            s.silent_mode = pick(2) == 0;
            if (s.silent_mode)
                s.silent_limits = {{"acceleration", uni(100, 1000)}, {"max_feed_x", uni(50, 200)}};
            s.gcode_progress = pick(2) == 0;
            s.variables["temperature"] = {std::to_string(int(uni(180, 260)))};
        }
        s.machine_max_flow = pick(3) == 0 ? uni(2, 30) : 0.0;
        s.filament_max_flow = pick(4) == 0 ? uni(2, 30) : 0.0;
        s.speed_max_flow = pick(5) == 0 ? uni(2, 30) : 0.0;
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
        const auto& [name, shape] = shapes[std::size_t(pick(int(shapes.size())))];
        // Copies, printed together or one by one (sequential.md).
        IndexedMesh mesh = shape;
        std::vector<MeshObject> objects;
        if (pick(6) == 0) {
            s.sequential = pick(2) == 0;
            s.gcode_between_objects = pick(2) == 0 ? ";between [current_object_idx]" : "";
            s.object_labels = std::array{ObjectLabels::None, ObjectLabels::OctoPrint, ObjectLabels::Firmware}[std::size_t(pick(3))];
            TriangleMesh one;
            for (const auto& t : shape.triangles)
                one.add_triangle(shape.vertices[t[0]], shape.vertices[t[1]], shape.vertices[t[2]]);
            mesh = weld_exact(place_copies(one, std::size_t(2 + pick(2)), s, name, &objects));
        }
        // Several extruders (multi_extruder.md): the copies each with one,
        // a kind of extrusion with another, the nozzles offset.
        std::vector<ColorMix> mixes;
        if (pick(5) == 0) {
            s.extruders = 2 + pick(2);
            for (std::size_t k = 0; k < objects.size(); ++k)
                objects[k].extruder = 1 + int(k % std::size_t(s.extruders));
            for (int* kind : {&s.extruder_walls, &s.extruder_infill, &s.extruder_solid_infill, &s.extruder_support,
                              &s.extruder_support_interface})
                *kind = pick(3) == 0 ? pick(s.extruders + 1) : 0;
            s.bed_temperature_from = pick(s.extruders + 1);
            s.ooze_prevention = pick(3) == 0;
            s.standby_delta = -uni(0, 40);
            s.toolchange_length = {uni(0, 12), uni(0, 12)};
            s.toolchange_restart_extra = {uni(0, 2)};
            s.interface_shells = pick(2) == 0;
            // Mixed colours (color_mixing.md): a blend, or a gradient, for some copies.
            if (pick(3) == 0) {
                ColorMix m;
                m.id = s.extruders + 1;
                m.gradient = pick(3) == 0;
                const double a = uni(0, 1);
                m.components = {{1, m.gradient ? 0.0 : a}, {2, m.gradient ? 1.0 : 1.0 - a}};
                if (s.extruders > 2 && pick(2) == 0)
                    m.components.push_back({3, m.gradient ? uni(0, 1) : uni(0, 1)});
                if (m.gradient && pick(2) == 0)
                    m.min_z = uni(0, 5), m.max_z = *m.min_z + uni(0.5, 10);
                mixes.push_back(m);
                for (MeshObject& o : objects)
                    if (pick(2) == 0)
                        o.extruder = m.id;
            }
            // Different nozzles (multimaterial.md), where the layers fit 75 % of the finest.
            if (pick(3) == 0) {
                const std::vector<double> nozzles = {s.nozzle_diameter, s.nozzle_diameter * uni(0.8, 1.8)};
                const double finest = std::min(nozzles[0], nozzles[1]);
                if (std::max(s.layer_height, s.first_layer_height) <= 0.75 * finest) {
                    s.nozzle_diameters = nozzles;
                    s.fine_walls_coarse_infill = pick(2) == 0;
                    if (pick(2) == 0)
                        s.infill_combine_auto = true;
                }
            }
            if (pick(3) == 0) {
                s.interlock_beams = true;
                s.interlock_width = uni(0.4, 2.0);
                s.interlock_layers = 1 + pick(3);
                s.interlock_orientation = uni(0, 180);
                s.interlock_depth = pick(4);
                s.interlock_avoidance = pick(4);
            }
            if (pick(2) == 0)
                s.extruder_offset = {{0.0, 0.0}, {uni(-5, 5), uni(-5, 5)}};
            s.gcode_toolchange = std::array{"", ";TC [previous_extruder] [next_extruder] {toolchange_z}",
                                            "T{next_extruder} ; own"}[std::size_t(pick(3))];
            s.variables["temperature"] = {"200", "215", "230"};
            s.variables["bed_temperature"] = {"60", "70"};
            // The wipe tower, away from the models (centred on the origin).
            if (pick(2) == 0) {
                s.wipe_tower = true;
                s.relative_extrusion = true;
                s.single_extruder_mmu = pick(2) == 0;
                s.wipe_tower_x = uni(80, 100), s.wipe_tower_y = uni(80, 100);
                s.wipe_tower_rotation = pick(2) == 0 ? 0.0 : uni(-90, 90);
                s.wipe_tower_width = uni(20, 60);
                s.wipe_tower_brim = uni(0, 4);
                s.wipe_tower_bridging = uni(3, 15);
                s.wipe_tower_extra_spacing = uni(80, 200);
                s.wipe_tower_extra_flow = uni(80, 200);
                s.wipe_tower_no_sparse_layers = pick(3) == 0;
                s.min_purge_on_tower = {uni(0, 60)};
                for (MeshObject& o : objects)
                    (pick(2) == 0 ? o.wipe_into_infill : o.wipe_into_object) = pick(2) == 0;
                s.purge_multiplier = {uni(30, 150)};
                if (pick(2) == 0)
                    s.purge_volumes = {{0, uni(0, 300), uni(0, 300)}, {uni(0, 300), 0, uni(0, 300)}, {uni(0, 300), uni(0, 300), 0}};
            }
        }
        // Curved layers (nonplanar.md): a wave fitted to the head, or a
        // gentle cone, without what does not go with them.
        if (pick(12) == 0) {
            s.curved_mode =
                std::array{CurvedMode::Wave, CurvedMode::Conical, CurvedMode::Surface, CurvedMode::Overhangs,
                           CurvedMode::Automatic}[std::size_t(pick(5))];
            s.curved_pattern = std::array{CurvedPattern::Egg, CurvedPattern::Ridges, CurvedPattern::Twisted}[std::size_t(pick(3))];
            s.curved_angle = uni(0, 90);
            s.curved_fit_head = s.curved_mode != CurvedMode::Conical || pick(2) == 0;
            s.curved_overhang_threshold = uni(35, 55);
            s.curved_cone_angle = uni(-1, 1);
            s.curved_ramp_height = uni(8, 12);
            s.curved_flat_top = pick(3) == 0;
            s.curved_flat_below = uni(0.2, 2);
            s.head_free_angle_x = pick(2) == 0 ? 0.0 : uni(10, 40);
            s.relative_extrusion = true;
            s.vase = s.zaa = s.infill_nonplanar = s.gcode_arcs = s.sequential = false;
            s.scarf = ScarfMode::Nowhere;
            s.detail_height = 0.0;
            s.wipe_tower = false;
            if (!relative_e(s))
                s.flavor = GcodeFlavor::Marlin;
        }
        // Last of the picks, so the runs before it keep their settings.
        s.walls_staggered = pick(4) == 0;
        if (pick(4) == 0) {
            s.dynamic_overhang_fan = true;
            for (double& f : s.overhang_fans)
                f = double(pick(101));
        }
        if (pick(3) == 0) {
            s.slowdown_logic = SlowdownLogic::ConsistentSurface;
            s.transition_distance = double(pick(4)) * 3.0;
        }
        s.support_pattern = SupportPattern(pick(3));
        s.support_sheath = pick(3) != 0;
        s.support_interface_pattern = SupportInterfacePattern(pick(3));
        s.support_interface_spacing = pick(3) == 0 ? 0.5 * double(pick(4)) : 0.0;
        s.support_contact_loops = pick(3) == 0;
        s.support_enforce_layers = pick(4) == 0 ? pick(30) : 0;
        s.corner_smoothing = pick(3) == 0 ? 0.01 * double(pick(6)) : 0.0;
        s.monotonic_bottom = pick(2) == 0;
        s.infill_before_walls = pick(4) == 0;
        s.infill_max_link = pick(3) == 0 ? double(pick(60)) : 50.0;
        s.bridge_angle = pick(4) == 0 ? double(pick(180)) : 0.0;
        if (s.support_style == SupportStyle::Grid && pick(3) == 0) {
            s.support_style = SupportStyle::Snug;
            s.support_closing_radius = 0.25 * double(pick(12));
        }
        // With TISMA_FUZZ_RUN set to a run's number, that run's model and
        // settings go to TISMA_FUZZ_DUMP before it is sliced (one that hangs).
        if (const char* only = std::getenv("TISMA_FUZZ_RUN"); only && std::atoi(only) == run)
            dump(run, mesh, s, {});
        try {
            const SlicedModel model =
                slice_model(mesh, s, nullptr, nullptr, nullptr, objects.empty() ? nullptr : &objects, &mixes);
            if (!unprintable_reason(model, s).empty()) {
                ++rejected;
                continue;
            }
            std::ostringstream out;
            write_gcode(out, model, s);
            const GcodeCheck c = check_gcode(out.str(), gcode_limits(model, s));
            if (s.gcode_binary) { // the binary format reads back to the same G-code
                try {
                    const BinaryGcode b = read_binary_gcode(binary_gcode(out.str(), s));
                    if (b.gcode.empty() || !out.str().starts_with(b.gcode))
                        throw std::runtime_error("its G-code is not the text's");
                } catch (const std::exception& e) {
                    ++failures;
                    std::printf("run %d (%s): binary G-code: %s\n", run, name, e.what());
                }
            }
            if (!c.ok()) {
                ++failures;
                std::printf("run %d (%s): %zu problems; first: line %zu: %s\n", run, name, c.problem_count,
                            c.problems[0].line, c.problems[0].message.c_str());
                for (std::size_t k = 1; k < std::min<std::size_t>(c.problems.size(), 3); ++k)
                    std::printf("  then line %zu: %s\n", c.problems[k].line, c.problems[k].message.c_str());
                std::printf("  layer %.3f first %.3f walls %d width %.3f infill %.1f pattern %d support %d brim %.2f skirt %d retract %.2f lift %.2f relE %d seam %d minlt %.1f fil %.2f xyoff %.3f shrink %.3f maxv %.0f travel %.0f\n",
                            s.layer_height, s.first_layer_height, s.wall_count, s.wall_width, s.infill_density, int(s.infill_pattern), int(s.support), s.brim_width, s.skirt_loops, s.retract_length, s.retract_lift, int(s.relative_extrusion), int(s.seam), s.min_layer_time, s.filament_diameter, s.xy_offset, s.first_layer_shrink, s.max_speed, s.travel_speed);
                std::printf("  zaa %d min %.0f raise %d detail %.3f tol %.3f margin %.2f cusp %.3f layers %.3f-%.3f steps",
                            int(s.zaa), s.zaa_min_thickness, int(s.zaa_raise), s.detail_height, s.detail_tolerance,
                            s.detail_margin, s.cusp_height, s.min_layer_height, s.max_layer_height);
                for (const auto& step : s.layer_heights)
                    std::printf(" %.3f:%.3f", step[0], step[1]);
                std::printf("\n");
                std::printf("  vase %d scarf %d smooth %d start %.0f length %.2f entire %d segment %.2f inner %d overhangs %d staggered %d "
                            "overhang fan %d %.0f %.0f %.0f %.0f slowdown %d tail %.0f support pattern %d sheath %d interface %d "
                            "%.1f contact %d enforce %d style %d closing %.2f corners %.2f bottom %d before %d link %.0f bridge %.0f\n",
                            int(s.vase), int(s.scarf), int(s.scarf_only_smooth), s.scarf_start_height, s.scarf_length,
                            int(s.scarf_entire_loop), s.scarf_max_segment, int(s.scarf_inner), int(s.detect_overhangs),
                            int(s.walls_staggered), int(s.dynamic_overhang_fan), s.overhang_fans[0],
                            s.overhang_fans[1], s.overhang_fans[2], s.overhang_fans[3], int(s.slowdown_logic),
                            s.transition_distance, int(s.support_pattern), int(s.support_sheath),
                            int(s.support_interface_pattern), s.support_interface_spacing,
                            int(s.support_contact_loops), s.support_enforce_layers, int(s.support_style),
                            s.support_closing_radius, s.corner_smoothing, int(s.monotonic_bottom),
                            int(s.infill_before_walls), s.infill_max_link, s.bridge_angle);
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
        } catch (const CurvedError&) {
            ++rejected; // curved layers the part cannot take, said so
        } catch (const std::exception& e) {
            ++failures;
            std::printf("run %d (%s): exception %s\n", run, name, e.what());
            dump(run, mesh, s, {});
        }
    }
    std::printf("%d runs, %d rejected as unprintable, %d failures\n", runs, rejected, failures);
    return failures != 0;
}
