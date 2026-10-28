// Several extruders (docs/spec/features/multi_extruder.md, rules 1 to 8 and
// 16; tests ME1 to ME5 and ME10).

#include "config/settings.hpp"
#include "gcode/check.hpp"
#include "gcode/writer.hpp"
#include "mesh/indexed_mesh.hpp"
#include "mesh/mesh.hpp"
#include "slice/pipeline.hpp"
#include "slice/placement.hpp"
#include "slice/printable.hpp"
#include "test_printer.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

using namespace tisma;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::WithinAbs;

namespace {

// A box from (x0, y0, z0) to (x1, y1, z1).
void add_box(TriangleMesh& m, float x0, float y0, float z0, float x1, float y1, float z1) {
    const Vec3f v[8] = {{x0, y0, z0}, {x1, y0, z0}, {x1, y1, z0}, {x0, y1, z0},
                        {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}};
    const auto quad = [&](int a, int b, int c, int d) {
        m.add_triangle(v[a], v[b], v[c]);
        m.add_triangle(v[a], v[c], v[d]);
    };
    quad(0, 3, 2, 1);
    quad(4, 5, 6, 7);
    quad(0, 1, 5, 4);
    quad(1, 2, 6, 5);
    quad(2, 3, 7, 6);
    quad(3, 0, 4, 7);
}

struct Plate {
    TriangleMesh mesh;
    std::vector<MeshObject> objects;
};

// The spec's two colours: halves of 10 x 20 x 10 mm side by side, X 90 to
// 100 with extruder 1 and 100 to 110 with extruder 2.
Plate two_colours() {
    Plate p;
    add_box(p.mesh, 90, 90, 0, 100, 110, 10);
    add_box(p.mesh, 100, 90, 0, 110, 110, 10);
    p.objects = {{"left", 0, 12, 0, 0, 1}, {"right", 12, 12, 1, 0, 2}};
    return p;
}

// Stacked: a 20 x 20 x 5 mm block with extruder 1, another on top with 2.
Plate stacked() {
    Plate p;
    add_box(p.mesh, 90, 90, 0, 110, 110, 5);
    add_box(p.mesh, 90, 90, 5, 110, 110, 10);
    p.objects = {{"bottom", 0, 12, 0, 0, 1}, {"top", 12, 12, 1, 0, 2}};
    return p;
}

// One 20 mm cube with the given extruder.
Plate cube(int extruder) {
    Plate p;
    add_box(p.mesh, 90, 90, 0, 110, 110, 20);
    p.objects = {{"cube", 0, 12, 0, 0, extruder}};
    return p;
}

// The spec's base (walls_extra.md): 0.2 layers, 2 walls, no infill, no
// skirt, two extruders at 200 and 220 C, relative E to read the retractions.
Settings base() {
    Settings s = test_printer();
    s.flavor = GcodeFlavor::Marlin;
    s.relative_extrusion = true;
    s.bed = {{0, 0}, {250, 0}, {250, 210}, {0, 210}};
    s.layer_height = s.first_layer_height = 0.2;
    s.wall_count = 2;
    s.infill_density = 0.0;
    s.top_layers = s.bottom_layers = 0;
    s.skirt_loops = 0;
    s.brim_width = 0.0;
    s.extruders = 2;
    s.variables["temperature"] = {"200", "220"};
    return s;
}

std::string gcode_of(const Plate& p, const Settings& s) {
    const SlicedModel model = slice_model(weld_exact(p.mesh), s, nullptr, nullptr, nullptr, &p.objects);
    std::ostringstream out;
    write_gcode(out, model, s);
    const GcodeCheck check = check_gcode(out.str(), gcode_limits(model, s));
    INFO((check.problems.empty() ? std::string() : std::to_string(check.problems.front().line) + ": " +
                                                       check.problems.front().message));
    CHECK(check.ok());
    return out.str();
}

// What a G-code does, by layer, tool and line type.
struct Trace {
    std::vector<std::pair<int, int>> changes; // (layer, tool) of each T command; layer -1 before the first
    std::map<std::tuple<int, int, std::string>, double> extruded; // mm of path
    std::map<int, std::array<double, 4>> box;                      // each tool's extrusions: x0, y0, x1, y1
};

Trace trace(const std::string& text) {
    Trace t;
    std::istringstream in(text);
    std::string line, type;
    int layer = -1, tool = 0;
    double x = 0.0, y = 0.0;
    while (std::getline(in, line)) {
        if (line == ";LAYER_CHANGE") {
            ++layer;
            continue;
        }
        if (line.rfind(";TYPE:", 0) == 0) {
            type = line.substr(6);
            continue;
        }
        if (line.size() > 1 && line[0] == 'T' && std::isdigit(static_cast<unsigned char>(line[1]))) {
            tool = std::atoi(line.c_str() + 1);
            t.changes.push_back({layer, tool});
            continue;
        }
        if (line.rfind("G1 ", 0) != 0)
            continue;
        double nx = x, ny = y, e = 0.0;
        std::istringstream words(line.substr(3));
        std::string w;
        while (words >> w) {
            if (w[0] == ';')
                break;
            const double v = std::atof(w.c_str() + 1);
            if (w[0] == 'X')
                nx = v;
            else if (w[0] == 'Y')
                ny = v;
            else if (w[0] == 'E')
                e = v;
        }
        const double d = std::hypot(nx - x, ny - y);
        if (e > 0.0 && d > 0.0) {
            t.extruded[{layer, tool, type}] += d;
            auto [it, fresh] = t.box.try_emplace(tool, std::array<double, 4>{nx, ny, nx, ny});
            auto& b = it->second;
            for (const auto& [px, py] : {std::pair{x, y}, std::pair{nx, ny}})
                b = {std::min(b[0], px), std::min(b[1], py), std::max(b[2], px), std::max(b[3], py)};
            (void)fresh;
        }
        x = nx, y = ny;
    }
    return t;
}

double laid(const Trace& t, int layer, int tool, const std::string& type) {
    const auto it = t.extruded.find({layer, tool, type});
    return it == t.extruded.end() ? 0.0 : it->second;
}

int layers_of(const std::string& text) {
    int n = 0;
    for (std::size_t at = text.find(";LAYER_CHANGE"); at != std::string::npos; at = text.find(";LAYER_CHANGE", at + 1))
        ++n;
    return n;
}

} // namespace

TEST_CASE("extruders: each colour heated, the first chosen, one change per layer (ME1)", "[extruders]") {
    const std::string text = gcode_of(two_colours(), base());
    CHECK_THAT(text, ContainsSubstring("M104 S200 T0\nM104 S220 T1\n"));
    CHECK_THAT(text, ContainsSubstring("M109 S200 T0\nM109 S220 T1\nT0\n"));
    const Trace t = trace(text);
    const int layers = layers_of(text);
    REQUIRE(layers == 50);
    // T0 at the start, then each layer starts with the last one and changes once.
    REQUIRE(t.changes.size() == std::size_t(layers) + 1);
    CHECK(t.changes.front() == std::pair{-1, 0});
    for (int l = 0; l < layers; ++l)
        CHECK(t.changes[std::size_t(l) + 1] == std::pair{l, l % 2 == 0 ? 1 : 0});
    // Each half with its extruder, its own outer wall where they touch: a
    // loop round the 10 x 20 mm half, half a bead in.
    CHECK_THAT(laid(t, 5, 0, "External perimeter"), WithinAbs(2.0 * (9.6 + 19.6), 0.5));
    CHECK_THAT(laid(t, 5, 1, "External perimeter"), WithinAbs(2.0 * (9.6 + 19.6), 0.5));
    CHECK(t.box.at(0)[2] < 100.0);
    CHECK(t.box.at(1)[0] > 100.0);
}

TEST_CASE("extruders: the tool change's retraction, G-code and return (ME2)", "[extruders]") {
    Settings s = base();
    s.toolchange_length = {4.0};
    s.toolchange_restart_extra = {1.0};
    s.gcode_toolchange = ";TC [previous_extruder]->[next_extruder] at {toolchange_z}";
    const std::string text = gcode_of(two_colours(), s);
    const std::size_t change = text.find(";TC 0->1 at 0.2\n");
    REQUIRE(change != std::string::npos);
    // The leaving tool pulls 4 mm back first, standing, then the change and T1.
    const std::size_t pull = text.rfind("G1 E-4.00000 F", change);
    REQUIRE(pull != std::string::npos);
    CHECK(text.find("\nG1 X", pull) > change); // no move between
    const std::string sequence = ";TC 0->1 at 0.2\nT1\n";
    CHECK(text.compare(change, sequence.size(), sequence) == 0);
    // At the start, from none.
    CHECK_THAT(text, ContainsSubstring(";TC -1->0 at 0.2\nT0\n"));
    // Back to T0 on the next layer: its 4 mm and 1 more.
    const std::size_t back = text.find(";TC 1->0");
    REQUIRE(back != std::string::npos);
    const std::size_t push = text.find("\nG1 E", back);
    REQUIRE(push != std::string::npos);
    const std::string undo = "\nG1 E5.00000 F";
    CHECK(text.compare(push, undo.size(), undo) == 0);
    // A G-code that changes the tool itself: no other T.
    s.gcode_toolchange = ";TC {previous_extruder}\nT{next_extruder} ; mine";
    const std::string own = gcode_of(two_colours(), s);
    CHECK_THAT(own, ContainsSubstring(";TC -1\nT0 ; mine\n"));
    CHECK_THAT(own, ContainsSubstring(";TC 0\nT1 ; mine\n"));
    CHECK(own.find("\nT1\n") == std::string::npos);
    CHECK(own.find("\nT0\n") == std::string::npos);
}

TEST_CASE("extruders: an extruder per kind of extrusion and the bed's temperatures (ME3)", "[extruders]") {
    Settings s = base();
    s.infill_density = 20.0;
    s.top_layers = s.bottom_layers = 3;
    s.extruder_walls = 1;
    s.extruder_infill = s.extruder_solid_infill = 2;
    s.variables["first_layer_bed_temperature"] = {"55", "75"};
    s.variables["bed_temperature"] = {"50", "70"};
    const std::string text = gcode_of(cube(1), s);
    const Trace t = trace(text);
    double walls[2] = {0, 0}, fills[2] = {0, 0};
    for (const auto& [key, length] : t.extruded) {
        const auto& [layer, tool, type] = key;
        if (type == "External perimeter" || type == "Perimeter")
            walls[tool] += length;
        else if (type == "Internal infill" || type == "Solid infill" || type == "Top solid infill" ||
                 type == "Bridge infill")
            fills[tool] += length;
    }
    CHECK(walls[0] > 0.0);
    CHECK(walls[1] == 0.0);
    CHECK(fills[0] == 0.0);
    CHECK(fills[1] > 0.0);
    // Layer 5 starts with the infill: its first change is to T0.
    for (const auto& [layer, tool] : t.changes)
        if (layer == 5) {
            CHECK(tool == 0);
            break;
        }
    // The bed: the first layer's of the walls' extruder, from the second
    // layer that of the infill's, which starts it.
    const std::size_t second = text.find(";LAYER_CHANGE", text.find(";LAYER_CHANGE") + 1);
    CHECK(text.find("M190 S55\n") < text.find(";LAYER_CHANGE"));
    CHECK(text.find("M140 S70\n", second) != std::string::npos);
    CHECK(text.find("M140 S70\n", second) < text.find(";LAYER_CHANGE", second + 1));
    // All with extruder 2: the bed from extruder 1 when asked, else its own.
    Settings two = base();
    two.variables = s.variables;
    two.bed_temperature_from = 1;
    std::string all = gcode_of(cube(2), two);
    CHECK_THAT(all, ContainsSubstring("M190 S55\n"));
    CHECK_THAT(all, ContainsSubstring("M140 S50\n"));
    two.bed_temperature_from = 0;
    all = gcode_of(cube(2), two);
    CHECK_THAT(all, ContainsSubstring("M190 S75\n"));
    CHECK_THAT(all, ContainsSubstring("M140 S70\n"));
    CHECK(trace(all).changes.front().second == 1);
}

TEST_CASE("extruders: the waiting extruders cool down (ME4)", "[extruders]") {
    Settings s = base();
    s.ooze_prevention = true;
    s.standby_delta = -30.0;
    const std::string text = gcode_of(two_colours(), s);
    CHECK_THAT(text, ContainsSubstring("M104 S200 T0\nM104 S190 T1\n"));
    CHECK_THAT(text, ContainsSubstring("M109 S200 T0\nT0\n"));
    CHECK(text.find("M109 S220 T1\nT0") == std::string::npos);
    CHECK_THAT(text, ContainsSubstring("M104 S170 T0 ;cooldown\nT1\nM109 S220 T1\n"));
    CHECK_THAT(text, ContainsSubstring("M104 S190 T1 ;cooldown\nT0\nM109 S200 T0\n"));
}

TEST_CASE("extruders: each nozzle's offset comes off its coordinates (ME5)", "[extruders]") {
    Settings s = base();
    s.extruder_offset = {{0.0, 0.0}, {10.0, 5.0}};
    const Trace t = trace(gcode_of(two_colours(), s));
    const auto& b = t.box.at(1);
    CHECK_THAT(b[0], WithinAbs(90.2, 0.02));
    CHECK_THAT(b[2], WithinAbs(99.8, 0.02));
    CHECK_THAT(b[1], WithinAbs(85.2, 0.02));
    CHECK_THAT(b[3], WithinAbs(104.8, 0.02));
    CHECK_THAT(t.box.at(0)[0], WithinAbs(90.2, 0.02));
    // Changing over the wipe tower, the new nozzle first moves over the
    // point where the old one stopped (the check reads each line's length).
    s.wipe_tower = true;
    const std::string text = gcode_of(two_colours(), s);
    const std::size_t change = text.find("\nT1\n");
    REQUIRE(change != std::string::npos);
    CHECK(text.find("\nG1 X", change) < text.find(" E", text.find("\nG1 X", change)));
}

TEST_CASE("extruders: solid layers between materials only with interface shells (ME10)", "[extruders]") {
    Settings s = base();
    s.infill_density = 20.0;
    s.top_layers = s.bottom_layers = 3;
    // Without: the two blocks are one part, sparse infill all through.
    Trace t = trace(gcode_of(stacked(), s));
    for (int l = 21; l <= 24; ++l)
        CHECK(laid(t, l, 0, "Internal infill") > 0.0);
    for (int l = 25; l <= 27; ++l)
        CHECK(laid(t, l, 1, "Internal infill") > 0.0);
    // With: the bottom block's top (solid over its infill, solid, top) and
    // the top block's bottom layers. Tisma Slicer lays layer 22 as a bridge
    // over the sparse infill; the engine lays it solid (PROGRESS.md).
    s.interface_shells = true;
    t = trace(gcode_of(stacked(), s));
    CHECK(laid(t, 22, 0, "Solid infill") + laid(t, 22, 0, "Bridge infill") > 0.0);
    CHECK(laid(t, 23, 0, "Solid infill") > 0.0);
    CHECK(laid(t, 24, 0, "Top solid infill") > 0.0);
    for (int l = 22; l <= 24; ++l)
        CHECK(laid(t, l, 0, "Internal infill") == 0.0);
    for (int l = 25; l <= 27; ++l) {
        CHECK(laid(t, l, 1, "Solid infill") > 0.0);
        CHECK(laid(t, l, 1, "Internal infill") == 0.0);
        CHECK(laid(t, l, 1, "Bridge infill") == 0.0); // it rests on the block below
    }
}

TEST_CASE("extruders: settings round trip and one extruder unchanged", "[extruders]") {
    Settings s = base();
    s.extruder_offset = {{0.0, 0.0}, {10.0, 5.0}};
    s.toolchange_length = {4.0, 6.0};
    s.extruder_walls = 2;
    s.ooze_prevention = true;
    s.standby_delta = -20.0;
    s.interface_shells = true;
    s.gcode_toolchange = "T{next_extruder}";
    const Settings back = parse_settings(settings_json(s));
    CHECK(back.extruders == 2);
    CHECK(back.extruder_offset == s.extruder_offset);
    CHECK(back.toolchange_length == s.toolchange_length);
    CHECK(back.extruder_walls == 2);
    CHECK(back.ooze_prevention);
    CHECK(back.standby_delta == -20.0);
    CHECK(back.interface_shells);
    CHECK(back.gcode_toolchange == "T{next_extruder}");
    CHECK_THROWS_AS(parse_settings(R"({"machine": {"extruders": 17}})"), SettingsError);
    CHECK_THROWS_AS(parse_settings(R"({"retraction": {"toolchange_length": [-1]}})"), SettingsError);
    Settings bad = base();
    bad.extruder_infill = 3;
    CHECK_THROWS_AS(check_settings(bad), SettingsError);
    // One extruder: no T, whatever the objects say.
    Settings one = base();
    one.extruders = 1;
    one.variables["temperature"] = {"200"};
    const std::string text = gcode_of(two_colours(), one);
    CHECK(trace(text).changes.empty());
    CHECK(text.find("M104 S200\n") != std::string::npos);
}

namespace {

// The wipe tower's extrusions of each layer: length and box.
struct TowerTrace {
    std::map<int, double> length;
    std::array<double, 4> box{1e9, 1e9, -1e9, -1e9};
    std::map<int, std::array<double, 4>> layer_box;
};

TowerTrace tower_of(const Trace& t) {
    TowerTrace w;
    for (const auto& [key, length] : t.extruded)
        if (std::get<2>(key) == "Wipe tower")
            w.length[std::get<0>(key)] += length;
    return w;
}

// The box of the wipe tower's extrusions, by layer, from the G-code.
TowerTrace tower_box(const std::string& text) {
    TowerTrace w;
    std::istringstream in(text);
    std::string line, type;
    int layer = -1;
    double x = 0.0, y = 0.0;
    while (std::getline(in, line)) {
        if (line == ";LAYER_CHANGE")
            ++layer;
        else if (line.rfind(";TYPE:", 0) == 0)
            type = line.substr(6);
        if (line.rfind("G1 ", 0) != 0)
            continue;
        double nx = x, ny = y, e = 0.0;
        std::istringstream words(line.substr(3));
        std::string word;
        while (words >> word) {
            if (word[0] == ';')
                break;
            const double v = std::atof(word.c_str() + 1);
            if (word[0] == 'X')
                nx = v;
            else if (word[0] == 'Y')
                ny = v;
            else if (word[0] == 'E')
                e = v;
        }
        if (type == "Wipe tower" && e > 0.0 && (nx != x || ny != y))
            for (std::array<double, 4>* b : {&w.box, &w.layer_box.try_emplace(layer, std::array<double, 4>{1e9, 1e9, -1e9, -1e9}).first->second})
                for (const auto& [px, py] : {std::pair{x, y}, std::pair{nx, ny}})
                    *b = {std::min((*b)[0], px), std::min((*b)[1], py), std::max((*b)[2], px), std::max((*b)[3], py)};
        x = nx, y = ny;
    }
    return w;
}

Settings tower_base(bool mmu) {
    Settings s = base();
    s.wipe_tower = true;
    s.single_extruder_mmu = mmu;
    return s;
}

} // namespace

TEST_CASE("wipe tower: relative E only, and where it stands (ME6)", "[extruders][tower]") {
    Settings absolute = tower_base(false);
    absolute.relative_extrusion = false;
    CHECK_THROWS_AS(check_settings(absolute), SettingsError);
    CHECK_NOTHROW(check_settings(tower_base(false)));
    // Several nozzles: the tower only primes, whatever the purge volumes.
    // (Layer 10's beads: the first layer adds the brim.)
    const auto at10 = [](const std::string& text) { return tower_box(text).layer_box.at(10); };
    const std::array<double, 4> nozzles = at10(gcode_of(two_colours(), tower_base(false)));
    CHECK_THAT(nozzles[0], WithinAbs(180.2, 0.05));
    CHECK_THAT(nozzles[2], WithinAbs(239.8, 0.05));
    CHECK_THAT(nozzles[1], WithinAbs(140.2, 0.05));
    CHECK_THAT(nozzles[3], WithinAbs(142.6, 0.05)); // 5 lines and the outline: 2.8 mm deep (Tisma Slicer: 3)
    Settings more = tower_base(false);
    more.purge_volumes = {{0, 300}, {300, 0}};
    CHECK_THAT(at10(gcode_of(two_colours(), more))[3], WithinAbs(142.6, 0.05));
    // A single-extruder MMU rams (4 lines 0.6 mm apart), then purges 140 mm3
    // a change: 30 lines of 58.8 mm.
    const std::string mmu_text = gcode_of(two_colours(), tower_base(true));
    const TowerTrace mmu = tower_box(mmu_text);
    CHECK_THAT(mmu.layer_box.at(10)[3], WithinAbs(155.0, 0.05)); // Tisma Slicer: 154.4
    const double per_layer = tower_of(trace(mmu_text)).length.at(10);
    CHECK_THAT(per_layer, WithinAbs(2123.98, 1.0)); // the ramming within speed.max (80 mm/s); Tisma Slicer: 2038.1
    Settings s = tower_base(true);
    s.purge_volumes = {{0, 300}, {300, 0}};
    CHECK_THAT(at10(gcode_of(two_colours(), s))[3], WithinAbs(168.6, 0.05)); // 64 lines; Tisma: 169.4
    s = tower_base(true);
    s.wipe_tower_width = 40.0;
    const std::array<double, 4> narrow = at10(gcode_of(two_colours(), s));
    CHECK_THAT(narrow[2], WithinAbs(219.8, 0.05));
    CHECK(narrow[3] > mmu.layer_box.at(10)[3]); // shorter lines, more of them
    // The brim on the first layer: 5 loops for 2 mm, 13 for 5 mm.
    CHECK_THAT(mmu.layer_box.at(0)[0], WithinAbs(178.2, 0.01));
    s = tower_base(true);
    s.wipe_tower_brim = 5.0;
    CHECK_THAT(tower_box(gcode_of(two_colours(), s)).layer_box.at(0)[0], WithinAbs(175.0, 0.01));
    // Spacing makes it deeper, flow shorter, the multiplier smaller.
    s = tower_base(true);
    s.wipe_tower_extra_spacing = 150.0;
    CHECK(at10(gcode_of(two_colours(), s))[3] > mmu.layer_box.at(10)[3] + 5.0);
    s = tower_base(true);
    s.wipe_tower_extra_flow = 150.0;
    CHECK(tower_of(trace(gcode_of(two_colours(), s))).length.at(10) < 0.75 * per_layer);
    s = tower_base(true);
    s.purge_multiplier = {50.0};
    CHECK_THAT(at10(gcode_of(two_colours(), s))[3], WithinAbs(149.0, 0.05)); // 15 lines; Tisma: 147.9
}

TEST_CASE("wipe tower: the change over the tower, sparse up to the last change (ME7)", "[extruders][tower]") {
    const std::string text = gcode_of(stacked(), tower_base(true));
    const TowerTrace t = tower_of(trace(text));
    // Dense with its brim on the first layer, sparse up to the change at
    // layer 25, nothing above.
    REQUIRE(t.length.count(0));
    for (int l = 1; l <= 24; ++l) {
        REQUIRE(t.length.count(l));
        CHECK(t.length.at(l) < 0.4 * t.length.at(0));
    }
    REQUIRE(t.length.count(25));
    CHECK(t.length.at(25) > 5.0 * t.length.at(24));
    CHECK(t.length.count(26) == 0);
    // Its filament in the summary (1.24 g/cm3).
    Settings dense = tower_base(true);
    dense.filament_density = 1.24;
    const std::string weighed = gcode_of(stacked(), dense);
    const std::size_t at = weighed.find("; total filament used for wipe tower [g] = ");
    REQUIRE(at != std::string::npos);
    CHECK(std::atof(weighed.c_str() + at + 44) > 0.1);
    // The one change: over the tower, after the travel there.
    const std::size_t change = text.find("\nT1\n");
    REQUIRE(change != std::string::npos);
    const std::size_t move = text.rfind("\nG1 X", change);
    REQUIRE(move != std::string::npos);
    CHECK(std::atof(text.c_str() + move + 5) >= 180.0);
    // Without sparse layers: only the first layer and the change's; the
    // nozzle goes down to the tower's second layer, at Z 0.4, and back up.
    Settings no_sparse = tower_base(true);
    no_sparse.wipe_tower_no_sparse_layers = true;
    const std::string compact = gcode_of(stacked(), no_sparse);
    const TowerTrace c = tower_of(trace(compact));
    CHECK(c.length.count(0));
    for (int l = 1; l <= 24; ++l)
        CHECK(c.length.count(l) == 0);
    REQUIRE(c.length.count(25));
    const std::size_t down = compact.find("\nG1 Z0.400", compact.find("\nT1\n"));
    REQUIRE(down != std::string::npos);
    CHECK(down < compact.find(";LAYER_CHANGE", compact.find("\nT1\n")));
    CHECK(compact.find("\nG1 Z5.200", down) < compact.find(";LAYER_CHANGE", compact.find("\nT1\n")));
    // A tower that would stand on the part is refused.
    Settings over = tower_base(true);
    over.wipe_tower_x = 95.0;
    over.wipe_tower_y = 95.0;
    const Plate p = stacked();
    const SlicedModel model = slice_model(weld_exact(p.mesh), over, nullptr, nullptr, nullptr, &p.objects);
    CHECK_THAT(unprintable_reason(model, over), ContainsSubstring("wipe tower lies on the part"));
}

TEST_CASE("wipe tower: the purge goes into the objects' infill or the whole objects first (ME8)", "[extruders][tower]") {
    Settings s = tower_base(true);
    s.infill_density = 20.0;
    s.top_layers = s.bottom_layers = 3;
    const auto tower_at = [&](const Plate& p, const Settings& settings) {
        const TowerTrace t = tower_of(trace(gcode_of(p, settings)));
        return t.length.count(10) ? t.length.at(10) : 0.0;
    };
    const double plain = tower_at(two_colours(), s);
    Plate infill = two_colours();
    for (MeshObject& o : infill.objects)
        o.wipe_into_infill = true;
    const double into_infill = tower_at(infill, s);
    Plate whole = two_colours();
    for (MeshObject& o : whole.objects)
        o.wipe_into_object = true;
    const double into_object = tower_at(whole, s);
    CHECK(into_infill < plain - 20.0);
    CHECK(into_object < into_infill - 20.0);
    // Right after the change, the next filament prints the infill it purges into.
    const std::string text = gcode_of(infill, s);
    std::size_t change = text.find("\nT1\n", text.find(";LAYER_CHANGE\n;Z:2.200"));
    REQUIRE(change != std::string::npos);
    const std::size_t next_type = text.find(";TYPE:", text.find(";TYPE:Wipe tower", change) + 1);
    std::size_t after_tower = next_type;
    while (after_tower != std::string::npos && text.compare(after_tower, 16, ";TYPE:Wipe tower") == 0)
        after_tower = text.find(";TYPE:", after_tower + 1);
    REQUIRE(after_tower != std::string::npos);
    CHECK(text.compare(after_tower, 21, ";TYPE:Internal infill") == 0);
    // At least extrusion.min_purge_on_tower on the tower first.
    s.min_purge_on_tower = {10000.0};
    CHECK_THAT(tower_at(infill, s), WithinAbs(plain, 0.5));
}

TEST_CASE("wipe tower: an MMU rams, unloads, cools and loads at each change (ME9)", "[extruders][tower]") {
    const std::string text = gcode_of(two_colours(), tower_base(true));
    const std::size_t change = text.find("\nT1\n");
    REQUIRE(change != std::string::npos);
    // The ramming before it: lines 0.6 wide on the tower, faster and faster.
    const std::size_t ram = text.rfind(";WIDTH:0.600", change);
    REQUIRE(ram != std::string::npos);
    std::vector<double> feeds;
    for (std::size_t at = text.find(" F", ram); at < text.find(";TYPE:Custom", ram); at = text.find(" F", at + 1))
        feeds.push_back(std::atof(text.c_str() + at + 2));
    REQUIRE(feeds.size() >= 5);
    CHECK(feeds.back() > feeds.front() * 1.4);
    // Out to the middle of the cooling tube (94 mm, with the travel's
    // retraction), in four pieces; no retraction between the ramming's.
    const std::size_t swap = text.rfind(";TYPE:Custom", change);
    CHECK(text.find("G1 E-", ram) == text.rfind("G1 E-", swap));
    const std::size_t last_ram = text.rfind("G1 E-", swap);
    std::istringstream in(text.substr(last_ram, change - last_ram));
    std::string line;
    double out = 0.0, cooling = 0.0;
    int cooled = 0;
    while (std::getline(in, line)) {
        if (line.rfind("G1 E-", 0) == 0)
            out += -std::atof(line.c_str() + 4);
        if (line.rfind("G1 X", 0) == 0 && line.find(" E") != std::string::npos) {
            ++cooled;
            cooling += std::atof(line.c_str() + line.find(" E") + 2);
        }
    }
    CHECK_THAT(out, WithinAbs(94.0 + 10.0, 0.01)); // and the change's 10 mm retraction
    CHECK(cooled == 8);                            // 4 moves out and back in the tube
    CHECK_THAT(cooling, WithinAbs(0.0, 1e-6));
    CHECK_THAT(text, ContainsSubstring("G1 E-13.00000 F6000")); // the first 15 mm, 2 of them pulled already
    // In: 90 mm, a fifth slowly, then fast, the last tenth slowly.
    const std::string load = "\nT1\nG1 E18.00000 F180\nG1 E63.00000 F1680\nG1 E9.00000 F180\n";
    CHECK(text.compare(change, load.size(), load) == 0);
}

TEST_CASE("extruders: beams of each material go into the other (ME11)", "[extruders]") {
    Settings s = base();
    s.infill_density = 20.0;
    s.top_layers = s.bottom_layers = 3;
    const Trace plain = trace(gcode_of(two_colours(), s));
    s.interlock_beams = true;
    const Trace beams = trace(gcode_of(two_colours(), s));
    // Without beams extruder 1 stops at X 99.8; with them its beams reach
    // into the other half (2 beam widths, 1.6 mm).
    CHECK_THAT(plain.box.at(0)[2], WithinAbs(99.8, 0.02));
    CHECK(beams.box.at(0)[2] > 101.0);
    CHECK(beams.box.at(1)[0] < 99.0);
    // The border between them a comb: each half's outer wall much longer.
    CHECK_THAT(laid(plain, 10, 0, "External perimeter"), WithinAbs(58.4, 0.5));
    CHECK(laid(beams, 10, 0, "External perimeter") > 80.0);
    CHECK(laid(beams, 10, 1, "External perimeter") > 80.0);
    // Two layers alike, then the beams turn.
    CHECK_THAT(laid(beams, 10, 0, "External perimeter"), WithinAbs(laid(beams, 11, 0, "External perimeter"), 1e-6));
    CHECK(std::abs(laid(beams, 12, 0, "External perimeter") - laid(beams, 11, 0, "External perimeter")) > 1.0);
    // Clear of the outside: the halves' outer sides stay straight there.
    CHECK_THAT(beams.box.at(0)[1], WithinAbs(90.2, 0.02));
}

// Different nozzles (docs/spec/features/multimaterial.md, rules 1 to 5 and
// 10; tests MM1, MM2 and MM5).

TEST_CASE("nozzles: one per extruder, read and written, and each one's layer limit (MM3, rule 2)", "[extruders][nozzles]") {
    Settings s = parse_settings(R"({"machine": {"extruders": 2, "nozzle_diameter": [0.4, 0.6]},
                                    "multi": {"fine_walls_coarse_infill": true, "color_mixing": false}})");
    CHECK(s.nozzle_diameter == 0.4);
    CHECK(s.nozzle_diameters == std::vector<double>{0.4, 0.6});
    CHECK(nozzle_of(s, 1) == 0.4);
    CHECK(nozzle_of(s, 2) == 0.6);
    CHECK(nozzle_of(s, 3) == 0.6); // a short list repeats its last
    CHECK(s.fine_walls_coarse_infill);
    REQUIRE(s.color_mixing.has_value());
    CHECK_FALSE(*s.color_mixing);
    const Settings back = parse_settings(settings_json(s));
    CHECK(back.nozzle_diameters == s.nozzle_diameters);
    CHECK(back.fine_walls_coarse_infill);
    CHECK(back.color_mixing == s.color_mixing);
    // The general one sets them all (rule 1).
    s = parse_settings(R"({"machine": {"nozzle_diameter": 0.5}})");
    CHECK(s.nozzle_diameters.empty());
    CHECK(nozzle_of(s, 2) == 0.5);
    CHECK_THROWS_AS(parse_settings(R"({"machine": {"nozzle_diameter": []}})"), SettingsError);
    CHECK_THROWS_AS(parse_settings(R"({"machine": {"nozzle_diameter": [0.4, -1]}})"), SettingsError);
    CHECK_THROWS_AS(parse_settings(R"({"multi": {"color_mixing": 1}})"), SettingsError);
    // Mixed colours offered by default with a nozzle per filament, not with
    // an MMU (rules 9 and 11).
    Settings mixing = base();
    CHECK(color_mixing_offered(mixing));
    mixing.single_extruder_mmu = true;
    CHECK_FALSE(color_mixing_offered(mixing));
    mixing.color_mixing = true;
    CHECK(color_mixing_offered(mixing));
    // Each nozzle lays 75 % of its diameter at most (rule 2): 0.3 with 0.4.
    Settings layers = base();
    layers.nozzle_diameters = {0.4, 0.6};
    layers.layer_height = layers.first_layer_height = 0.3;
    CHECK_NOTHROW(check_settings(layers));
    layers.layer_height = 0.32;
    CHECK_THROWS_AS(check_settings(layers), SettingsError);
    layers.nozzle_diameters.clear(); // one nozzle: up to its diameter, as before
    CHECK_NOTHROW(check_settings(layers));
}

namespace {

// MM1's printer: nozzles of 0.25 and 0.6, 0.15 mm layers, a 20 mm cube
// with sparse infill, walls 0.3 wide for the fine nozzle.
Settings nozzles_base() {
    Settings s = base();
    s.nozzle_diameter = 0.25;
    s.nozzle_diameters = {0.25, 0.6};
    s.layer_height = s.first_layer_height = 0.15;
    s.wall_width = 0.3;
    s.infill_density = 20.0;
    s.top_layers = s.bottom_layers = 3;
    s.fine_walls_coarse_infill = true;
    return s;
}

// The ;WIDTH and ;HEIGHT of each line type's extrusions, and the layers
// it shows up in.
struct Shown {
    std::map<std::string, std::vector<double>> widths, heights;
    std::map<std::string, std::vector<int>> layers;
};

Shown shown(const std::string& text) {
    Shown out;
    std::istringstream in(text);
    std::string line, type;
    int layer = -1;
    while (std::getline(in, line)) {
        if (line == ";LAYER_CHANGE")
            ++layer;
        else if (line.rfind(";TYPE:", 0) == 0) {
            type = line.substr(6);
            std::vector<int>& at = out.layers[type];
            if (at.empty() || at.back() != layer)
                at.push_back(layer);
        } else if (line.rfind(";WIDTH:", 0) == 0)
            out.widths[type].push_back(std::atof(line.c_str() + 7));
        else if (line.rfind(";HEIGHT:", 0) == 0)
            out.heights[type].push_back(std::atof(line.c_str() + 8));
    }
    return out;
}

} // namespace

TEST_CASE("nozzles: fine walls, coarse sparse infill combined (MM1)", "[extruders][nozzles]") {
    const Settings s = nozzles_base();
    REQUIRE_NOTHROW(check_settings(s));
    const KindExtruders kinds = kind_extruders(s);
    CHECK(kinds.walls == 1);
    CHECK(kinds.solid_infill == 1);
    CHECK(kinds.support == 1);
    CHECK(kinds.support_interface == 1);
    CHECK(kinds.wipe_tower == 1);
    CHECK(kinds.infill == 2);
    const std::string text = gcode_of(cube(1), s);
    const Trace t = trace(text);
    double walls_1 = 0.0, solid_1 = 0.0, sparse_1 = 0.0, sparse_2 = 0.0, other_2 = 0.0;
    for (const auto& [key, length] : t.extruded) {
        const auto& [layer, tool, type] = key;
        if (tool == 0 && (type == "External perimeter" || type == "Perimeter"))
            walls_1 += length;
        else if (tool == 0 && (type == "Solid infill" || type == "Top solid infill"))
            solid_1 += length;
        else if (type == "Internal infill")
            (tool == 1 ? sparse_2 : sparse_1) += length;
        else if (tool == 1)
            other_2 += length;
    }
    CHECK(walls_1 > 0.0);
    CHECK(solid_1 > 0.0);
    CHECK(sparse_2 > 0.0);
    CHECK(sparse_1 == 0.0);
    CHECK(other_2 == 0.0);
    // The coarse nozzle's infill: 0.675 mm wide (1.125 nozzles, I9), combined
    // three layers at a time (0.45 mm, 75 % of 0.6), a layer in three.
    const Shown lines = shown(text);
    REQUIRE(lines.widths.count("Internal infill"));
    for (const double w : lines.widths.at("Internal infill"))
        CHECK_THAT(w, WithinAbs(0.675, 0.005));
    for (const char* walls : {"External perimeter", "Perimeter"})
        if (lines.widths.count(walls))
            for (const double w : lines.widths.at(walls))
                CHECK(w < 0.4);
    int thick = 0;
    for (std::size_t at = text.find(";HEIGHT:0.450\n"); at != std::string::npos; at = text.find(";HEIGHT:0.450\n", at + 1)) {
        CHECK(text.find(";TYPE:Internal infill", at) < text.find(";HEIGHT:0.150", at));
        ++thick;
    }
    CHECK(thick > 10);
    std::set<int> infill_layers, all_layers;
    for (const auto& [key, length] : t.extruded) {
        all_layers.insert(std::get<0>(key));
        if (std::get<2>(key) == "Internal infill" && length > 0.0)
            infill_layers.insert(std::get<0>(key));
    }
    CHECK(int(infill_layers.size()) < int(all_layers.size()) / 3 + 3);
    CHECK(int(infill_layers.size()) > int(all_layers.size()) / 3 - 6);
    // Each with its change to the coarse nozzle.
    int to_coarse = 0;
    for (const auto& [layer, tool] : t.changes)
        to_coarse += tool == 1;
    CHECK(to_coarse == int(infill_layers.size()));
}

TEST_CASE("nozzles: turned off, every kind back to the print's extruders (MM2)", "[extruders][nozzles]") {
    Settings s = nozzles_base();
    s.fine_walls_coarse_infill = false;
    const KindExtruders kinds = kind_extruders(s);
    CHECK(kinds.walls == 0);
    CHECK(kinds.infill == 0);
    CHECK(kinds.solid_infill == 0);
    s.extruder_infill = 2; // the print profile's own choice holds
    CHECK(kind_extruders(s).infill == 2);
    s.extruder_infill = 0;
    // The cube prints with its extruder only, the infill each layer.
    const std::string text = gcode_of(cube(1), s);
    for (const auto& [layer, tool] : trace(text).changes)
        CHECK(tool == 0);
    const Shown lines = shown(text);
    for (const double h : lines.heights.at("Internal infill"))
        CHECK_THAT(h, WithinAbs(0.15, 0.005));
    // Nozzles all alike: the switch changes nothing.
    s.fine_walls_coarse_infill = true;
    s.nozzle_diameters = {0.25, 0.25};
    CHECK(kind_extruders(s).infill == 0);
    // A cube on the coarse extruder takes its widths.
    s.nozzle_diameters = {0.25, 0.6};
    s.fine_walls_coarse_infill = false;
    const Shown coarse = shown(gcode_of(cube(2), s));
    REQUIRE(coarse.widths.count("Perimeter"));
    for (const double w : coarse.widths.at("Perimeter"))
        CHECK(w > 0.5);
}

TEST_CASE("nozzles: the purge after slicing, on the tower and into the objects (MM5)", "[extruders][nozzles][tower]") {
    Settings s = tower_base(true);
    s.filament_density = 1.24;
    s.infill_density = 20.0;
    s.top_layers = s.bottom_layers = 3;
    const auto purge_of = [&](const Plate& p) {
        const SlicedModel model = slice_model(weld_exact(p.mesh), s, nullptr, nullptr, nullptr, &p.objects);
        std::ostringstream out;
        std::vector<Warning> warnings;
        write_gcode(out, model, s, &warnings);
        for (const Warning& w : warnings)
            if (w.kind == "purge")
                return std::make_pair(w, out.str());
        return std::make_pair(Warning{}, out.str());
    };
    const auto [plain, text] = purge_of(two_colours());
    REQUIRE(plain.kind == "purge");
    CHECK(plain.severity == "information");
    CHECK_THAT(plain.message, ContainsSubstring("on the wipe tower") && ContainsSubstring("0.00 g into the objects"));
    // The same grams as the footer's tower line.
    const std::size_t at = text.find("; total filament used for wipe tower [g] = ");
    REQUIRE(at != std::string::npos);
    CHECK_THAT(plain.value, WithinAbs(std::atof(text.c_str() + at + 43), 0.006));
    Plate infill = two_colours();
    for (MeshObject& o : infill.objects)
        o.wipe_into_infill = true;
    const Warning into = purge_of(infill).first;
    REQUIRE(into.kind == "purge");
    CHECK_THAT(into.message, !ContainsSubstring("0.00 g into the objects"));
    // A purge is a purge, wherever it goes: about as much in all.
    CHECK_THAT(into.value, WithinAbs(plain.value, 0.25 * plain.value));
    // A nozzle per filament: no purge to report.
    s.single_extruder_mmu = false;
    CHECK(purge_of(two_colours()).first.kind.empty());
}

TEST_CASE("extruders: each extruder's parts keep their combined infill", "[extruders][nozzles]") {
    Settings s = base();
    s.infill_density = 20.0;
    s.top_layers = s.bottom_layers = 3;
    s.infill_combine_layers = 2;
    const std::string text = gcode_of(two_colours(), s);
    const Trace t = trace(text);
    double sparse[2] = {0.0, 0.0};
    for (const auto& [key, length] : t.extruded)
        if (std::get<2>(key) == "Internal infill")
            sparse[std::get<1>(key)] += length;
    CHECK(sparse[0] > 100.0);
    CHECK(sparse[1] > 100.0);
    CHECK_THAT(sparse[0], WithinAbs(sparse[1], 0.2 * sparse[1]));
    CHECK(text.find(";HEIGHT:0.400\n") != std::string::npos);
}
