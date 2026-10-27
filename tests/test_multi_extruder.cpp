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
    // A single-extruder MMU purges 140 mm3 a change: 30 lines of 58.8 mm.
    const std::string mmu_text = gcode_of(two_colours(), tower_base(true));
    const TowerTrace mmu = tower_box(mmu_text);
    CHECK_THAT(mmu.layer_box.at(10)[3], WithinAbs(152.6, 0.05)); // Tisma Slicer: 154.4
    const double per_layer = tower_of(trace(mmu_text)).length.at(10);
    CHECK_THAT(per_layer, WithinAbs(30 * 58.8 + 29 * 0.4 + 2 * (59.6 + 12.4) + 7 * 0.0, 60.0)); // and a little fill
    Settings s = tower_base(true);
    s.purge_volumes = {{0, 300}, {300, 0}};
    CHECK_THAT(at10(gcode_of(two_colours(), s))[3], WithinAbs(166.2, 0.05)); // 64 lines; Tisma: 169.4
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
    CHECK_THAT(at10(gcode_of(two_colours(), s))[3], WithinAbs(146.6, 0.05)); // 15 lines; Tisma: 147.9
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
    // A tower that would stand on the part is refused.
    Settings over = tower_base(true);
    over.wipe_tower_x = 95.0;
    over.wipe_tower_y = 95.0;
    const Plate p = stacked();
    const SlicedModel model = slice_model(weld_exact(p.mesh), over, nullptr, nullptr, nullptr, &p.objects);
    CHECK_THAT(unprintable_reason(model, over), ContainsSubstring("wipe tower lies on the part"));
}
