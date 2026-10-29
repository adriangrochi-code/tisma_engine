#include "config/settings.hpp"
#include "gcode/check.hpp"
#include "gcode/export.hpp"
#include "gcode/writer.hpp"
#include "io/stl.hpp"
#include "mesh/indexed_mesh.hpp"
#include "slice/pipeline.hpp"
#include "slice/printable.hpp"
#include "test_printer.hpp"
#include "test_temp.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <sstream>
#include <string>

using namespace tisma;
using Catch::Matchers::ContainsSubstring;

namespace {

// A box from (x0, y0, z0) to (x1, y1, z1), facing out.
void add_box(IndexedMesh& m, float x0, float y0, float z0, float x1, float y1, float z1) {
    const auto base = std::uint32_t(m.vertices.size());
    for (float z : {z0, z1})
        for (const auto& p : {std::array<float, 2>{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}})
            m.vertices.push_back({p[0], p[1], z});
    auto quad = [&](std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d) {
        m.triangles.push_back({base + a, base + b, base + c});
        m.triangles.push_back({base + a, base + c, base + d});
    };
    quad(0, 3, 2, 1);
    quad(4, 5, 6, 7);
    for (std::uint32_t i = 0; i < 4; ++i)
        quad(i, (i + 1) % 4, 4 + (i + 1) % 4, 4 + i);
}

IndexedMesh cube() {
    return weld_exact(read_stl(std::filesystem::path(TISMA_TEST_DATA_DIR) / "cube_ascii.stl").mesh);
}

// A ring 2 mm thick around a 6 mm hole, two separate posts (islands), and a
// table: a slab 3 mm up on one leg (an overhang for supports).
IndexedMesh ring() {
    IndexedMesh m;
    add_box(m, 0, 0, 0, 10, 2, 3);
    add_box(m, 0, 8, 0, 10, 10, 3);
    add_box(m, 0, 2, 0, 2, 8, 3);
    add_box(m, 8, 2, 0, 10, 8, 3);
    return m;
}
IndexedMesh posts() {
    IndexedMesh m;
    add_box(m, 0, 0, 0, 3, 3, 4);
    add_box(m, 12, 5, 0, 15, 8, 2);
    return m;
}
IndexedMesh table() {
    IndexedMesh m;
    add_box(m, 0, 0, 0, 3, 3, 3);
    add_box(m, 0, 0, 3, 12, 8, 4);
    return m;
}

struct Output {
    std::string gcode;
    GcodeLimits limits;
};

Output slice_and_write(const IndexedMesh& mesh, const Settings& s) {
    const SlicedModel model = slice_model(mesh, s);
    std::ostringstream out;
    write_gcode(out, model, s);
    return {out.str(), gcode_limits(model, s)};
}

std::string problems_of(const GcodeCheck& c) {
    std::string out;
    for (const GcodeProblem& p : c.problems)
        out += "line " + std::to_string(p.line) + ": " + p.message + "\n";
    return out;
}

} // namespace

TEST_CASE("G-code check: the cube is clean, with plausible numbers", "[gcode][check]") {
    const Settings s = test_printer();
    const Output o = slice_and_write(cube(), s);
    const GcodeCheck c = check_gcode(o.gcode, o.limits);
    INFO(problems_of(c));
    CHECK(c.ok());
    CHECK(c.stats.layers == 50);
    CHECK(c.stats.extrusions > 1000);
    CHECK(c.stats.retractions > 0);
    CHECK(c.stats.max_z > 9.99);
    CHECK(c.stats.max_z < 10.01);
    // Filament: the cube's 1000 mm3 at 20 % infill with 3 walls and 3 + 3
    // solid layers is several hundred mm3, over 2.4 mm2 of filament.
    CHECK(c.stats.filament > 100.0);
    CHECK(c.stats.filament < 420.0);
}

TEST_CASE("G-code check: every kind of damage is caught", "[gcode][check]") {
    Settings s = test_printer();
    s.start_gcode = "M117 Hola, Tisma\nPRINT_START BED=60 EXTRUDER=215\nG28\nG1 X0 Y-5 Z0.3 F3000\nG1 X60 E9 F1000";
    s.end_gcode = "M104 S0\nG91\nG1 Z10\nG90\nM84";
    const Output o = slice_and_write(cube(), s);
    {
        const GcodeCheck c = check_gcode(o.gcode, o.limits);
        INFO(problems_of(c));
        CHECK(c.ok()); // the user's own G-code is not judged
    }
    // Damage the G-code one way at a time and expect the check to see it.
    auto damaged = [&](const std::function<void(std::string&)>& damage, const std::string& expected) {
        std::string g = o.gcode;
        damage(g);
        const GcodeCheck c = check_gcode(g, o.limits);
        INFO("expected: " << expected << "\n" << problems_of(c));
        CHECK(!c.ok());
        CHECK_THAT(problems_of(c), ContainsSubstring(expected));
    };
    const std::size_t layer = o.gcode.find(";LAYER_CHANGE\n;Z:1.000");
    REQUIRE(layer != std::string::npos);
    auto replace_first = [](std::string& g, std::size_t from, const std::string& what, const std::string& with) {
        const std::size_t at = g.find(what, from);
        REQUIRE(at != std::string::npos);
        g.replace(at, what.size(), with);
    };
    damaged(
        [](std::string& g) {
            g.resize(g.size() / 2);
            while (!g.empty() && g.back() == '\n') // in the middle of a line, wherever the half falls
                g.pop_back();
        },
        "cut short");
    damaged([](std::string& g) { g.resize(g.find("; estimated printing time")); }, "closing summary is missing");
    damaged([&](std::string& g) { g.insert(layer, "G1 X500 Y500\n"); }, "leaves the allowed area");
    damaged([&](std::string& g) { g.insert(layer, "G1 X5 Ynan\n"); }, "unreadable parameter 'Ynan'");
    damaged([&](std::string& g) { g.insert(layer, "G1 X5 F99999\n"); }, "above the fastest allowed");
    damaged([&](std::string& g) { g.insert(layer, "M106 S300\n"); }, "fan speed out of 0 to 255");
    damaged([&](std::string& g) { replace_first(g, layer, ";Z:1.000", ";Z:0.400"); }, "does not go above");
    damaged([&](std::string& g) {
        // A move that pushes 50 % more filament than its line asks for.
        const std::size_t at = g.find(";TYPE:Perimeter", layer);
        const std::size_t e = g.find(" E", g.find("\nG1 X", g.find("\nG1 X", at) + 1) + 1);
        const std::size_t end = g.find_first_of(" \n", e + 2);
        const double value = std::stod(g.substr(e + 2, end - e - 2));
        const std::size_t previous = g.rfind(" E", e - 1);
        const double before = std::stod(g.substr(previous + 2, g.find_first_of(" \n", previous + 2) - previous - 2));
        char text[32];
        std::snprintf(text, sizeof text, "%.5f", before + 1.5 * (value - before));
        g.replace(e + 2, end - e - 2, text);
    }, "ask for");
    damaged([&](std::string& g) {
        // Drop the move that undoes a retraction.
        const std::size_t retract = g.find("\nG1 E", layer);
        const std::size_t undo = g.find("\nG1 E", retract + 1);
        g.erase(undo + 1, g.find('\n', undo + 1) - undo);
    }, "while the filament is retracted");
    // The heater set to 0 and the print going on (the spec team's BH10); set again, it is fine.
    damaged([&](std::string& g) { g.insert(layer, "M104 S0\n"); }, "with the nozzle's heater set to 0");
    {
        std::string g = o.gcode;
        g.insert(layer, "M104 S0\nM109 S210\nM104 T1 S0\n");
        const GcodeCheck c = check_gcode(g, o.limits);
        INFO(problems_of(c));
        CHECK(c.ok());
    }
}

TEST_CASE("G-code check: read in chunks, each problem on its own line", "[gcode][check]") {
    // The check reads the file in chunks of about 256 KiB of whole lines, in
    // parallel, and takes them in order: lines on both sides of a cut.
    IndexedMesh box; // posts, for many lines
    for (int i = 0; i < 5; ++i)
        for (int j = 0; j < 5; ++j)
            add_box(box, 8.0f * float(i), 8.0f * float(j), 0, 8.0f * float(i) + 4, 8.0f * float(j) + 4, 12);
    const Settings s = test_printer();
    const Output o = slice_and_write(box, s);
    REQUIRE(o.gcode.size() > 600 * 1024);
    {
        const GcodeCheck c = check_gcode(o.gcode, o.limits);
        INFO(problems_of(c));
        CHECK(c.ok());
        CHECK(c.stats.lines == std::size_t(std::count(o.gcode.begin(), o.gcode.end(), '\n')));
    }
    for (const std::size_t cut : {std::size_t(256 * 1024), std::size_t(512 * 1024)}) {
        const std::size_t next = o.gcode.find('\n', cut) + 1; // the first line of the next chunk
        const std::size_t last = o.gcode.rfind('\n', next - 2) + 1; // the last line of this one
        for (const std::size_t at : {last, next}) {
            std::string g = o.gcode;
            g.insert(at, "M106 S300\n");
            const std::size_t line = std::size_t(std::count(g.begin(), g.begin() + std::ptrdiff_t(at), '\n')) + 1;
            const GcodeCheck c = check_gcode(g, o.limits);
            INFO(problems_of(c));
            REQUIRE(c.problems.size() == 1);
            CHECK(c.problems.front().line == line);
            CHECK_THAT(c.problems.front().message, ContainsSubstring("fan speed"));
        }
    }
}

TEST_CASE("G-code check: models by settings, every combination clean", "[gcode][check]") {
    struct Model {
        const char* name;
        IndexedMesh mesh;
    };
    const Model models[] = {{"cube", cube()}, {"ring", ring()}, {"posts", posts()}, {"table", table()}};
    struct Variant {
        const char* name;
        std::function<void(Settings&)> apply;
    };
    const Variant variants[] = {
        {"defaults", [](Settings&) {}},
        {"no walls", [](Settings& s) { s.wall_count = 0; }},
        {"one wall", [](Settings& s) { s.wall_count = 1; }},
        {"five walls", [](Settings& s) { s.wall_count = 5; }},
        {"no infill", [](Settings& s) { s.infill_density = 0.0; }},
        {"solid infill", [](Settings& s) { s.infill_density = 100.0; }},
        {"gyroid", [](Settings& s) { s.infill_pattern = InfillPattern::Gyroid; }},
        {"grid", [](Settings& s) { s.infill_pattern = InfillPattern::Grid; }},
        {"relative E", [](Settings& s) { s.relative_extrusion = true; }},
        {"supports", [](Settings& s) { s.support = true; }},
        {"organic supports", [](Settings& s) { s.support = true; s.support_style = SupportStyle::Organic; }},
        {"brim, no skirt", [](Settings& s) { s.brim_width = 4.0; s.skirt_loops = 0; }},
        {"thick first layer, thin layers", [](Settings& s) { s.first_layer_height = 0.3; s.layer_height = 0.12; }},
        {"lift on travel", [](Settings& s) { s.retract_lift = 0.4; }},
        {"no retraction", [](Settings& s) { s.retract_length = 0.0; }},
        {"slow layers", [](Settings& s) { s.min_layer_time = 30.0; }},
        {"nearest seam", [](Settings& s) { s.seam = SeamMode::Nearest; }},
        {"random seam", [](Settings& s) { s.seam = SeamMode::Random; }},
        {"Klipper", [](Settings& s) { s.flavor = GcodeFlavor::Klipper; }},
        {"2.85 mm filament, wide lines", [](Settings& s) { s.filament_diameter = 2.85; s.wall_width = 0.6; }},
        {"on a bed", [](Settings& s) { s.bed = {{-50, -50}, {100, -50}, {100, 100}, {-50, 100}}; }},
    };
    for (const Model& m : models)
        for (const Variant& v : variants) {
            Settings s = test_printer();
            v.apply(s);
            const Output o = slice_and_write(m.mesh, s);
            const GcodeCheck c = check_gcode(o.gcode, o.limits);
            INFO(m.name << ", " << v.name << ":\n" << problems_of(c));
            CHECK(c.ok());
            CHECK(c.stats.layers > 0);
        }
}

TEST_CASE("a model with nothing to extrude is not printable", "[gcode][check]") {
    // Walls of 0.3 mm with lines of 0.8 mm and no walls: the slicing finds the
    // layers but no line fits, so there is no G-code to write.
    IndexedMesh m;
    add_box(m, 0, 0, 0, 0.3f, 20, 5);
    add_box(m, 5, 0, 0, 5.3f, 20, 5);
    Settings s = test_printer();
    s.wall_count = 0;
    s.wall_width = 0.8;
    s.infill_width = s.solid_width = s.top_width = 0.8;
    const SlicedModel model = slice_model(m, s);
    CHECK_THAT(unprintable_reason(model, s), ContainsSubstring("nothing would be printed"));
    CHECK(unprintable_reason(slice_model(cube(), test_printer()), test_printer()).empty());
    // A path shorter than the G-code's resolution is not something printed:
    // its moves merge away (the spec team's BH7).
    SlicedModel tiny = slice_model(cube(), test_printer());
    for (auto& f : tiny.fills)
        f.top.clear(), f.solid.clear(), f.sparse.clear(), f.bridge.clear(), f.gaps.clear();
    for (auto& w : tiny.walls) {
        w.toolpaths.resize(1);
        w.toolpaths.front().points = {{{10.0, 10.0}, 0.45}, {{10.005, 10.0}, 0.45}};
    }
    CHECK_THAT(unprintable_reason(tiny, test_printer()), ContainsSubstring("nothing would be printed"));
    for (auto& w : tiny.walls)
        w.toolpaths.front().points.push_back({{12.0, 10.0}, 0.45});
    CHECK(unprintable_reason(tiny, test_printer()).empty());
}

TEST_CASE("export: a checked file under its name, or nothing", "[gcode][check]") {
    const auto dir = test_temp_dir();
    const auto path = dir / "tisma_test_export.gcode";
    auto part = path, rejected = path;
    part += ".part";
    rejected += ".rejected";
    std::filesystem::remove(path);
    std::filesystem::remove(rejected);
    const Settings s = test_printer();
    SlicedModel model = slice_model(cube(), s);

    const GcodeExport good = export_gcode(path, model, s);
    INFO(good.error);
    CHECK(good.written);
    CHECK(good.check.ok());
    CHECK(good.bytes == std::filesystem::file_size(path));
    CHECK(!std::filesystem::exists(part));
    std::filesystem::remove(path);

    // A skirt far from the part (a writer that went wrong): rejected, kept
    // aside for inspection, nothing under the requested name.
    model.skirt.front().points.push_back({{100.0, 100.0}, 0.4});
    const GcodeExport bad = export_gcode(path, model, s);
    CHECK(!bad.written);
    CHECK_THAT(bad.error, ContainsSubstring("failed its check"));
    CHECK_THAT(bad.error, ContainsSubstring("leaves the allowed area"));
    CHECK(!std::filesystem::exists(path));
    CHECK(!std::filesystem::exists(part));
    CHECK(std::filesystem::exists(rejected));
    std::filesystem::remove(rejected);

    // A folder that does not exist.
    const GcodeExport nowhere = export_gcode(dir / "tisma_no_such_dir" / "x.gcode", model, s);
    CHECK(!nowhere.written);
    CHECK_THAT(nowhere.error, ContainsSubstring("cannot write"));
}

TEST_CASE("G-code check: a wipe's step whose E rounds to the last one's stays in the wipe", "[gcode][check]") {
    // The fuzzer's run 287 (seed 20261028): a wipe went over a point 0.001
    // mm from the next, both steps wrote the same E, and the pieces after it
    // read as a second retraction.
    Settings s = test_printer();
    s.wipe = true;
    const Output o = slice_and_write(cube(), s);
    REQUIRE(check_gcode(o.gcode, o.limits).ok());
    // The first wipe step: a move in X and Y whose E is below the last E.
    std::istringstream in(o.gcode);
    std::string line, out;
    double last_e = 0.0;
    bool done = false;
    while (std::getline(in, line)) {
        out += line + "\n";
        const std::size_t e = line.find(" E");
        if (done || !line.starts_with("G1 ") || e == std::string::npos)
            continue;
        const double value = std::stod(line.substr(e + 2));
        if (value < last_e - 1e-9 && line.find(" X") != std::string::npos) {
            // A step of the same E right after it, a hair away.
            const std::size_t x = line.find(" X");
            const double at = std::stod(line.substr(x + 2));
            char extra[96];
            std::snprintf(extra, sizeof extra, "G1 X%.3f E%.5f\n", at + 0.001, value);
            out += extra;
            done = true;
        }
        last_e = value;
    }
    REQUIRE(done);
    const GcodeCheck c = check_gcode(out, o.limits);
    INFO(problems_of(c));
    CHECK(c.ok());
}

TEST_CASE("placement.drop_to_bed off: what is under the bed is not printed, a part off it is refused (placement.md, "
          "PL5, PL9)",
          "[gcode][check]") {
    Settings s = test_printer();
    s.placement_drop_to_bed = false;
    // PL5: a 20 mm cube from Z -5 to 15, as it comes: 10 mm printed, from the bed.
    IndexedMesh low;
    add_box(low, 90, 90, -5, 110, 110, 10);
    REQUIRE(unprintable_reason(low, s).empty());
    const Output o = slice_and_write(low, s);
    const GcodeCheck c = check_gcode(o.gcode, o.limits);
    INFO(problems_of(c));
    CHECK(c.ok());
    CHECK_THAT(o.gcode, ContainsSubstring(";Z:0.200\n"));
    CHECK(c.stats.max_z > 9.99);
    CHECK(c.stats.max_z < 10.01);
    // On by default: the same part drops to the bed, 15 mm tall (PL4).
    s.placement_drop_to_bed = true;
    const Output dropped = slice_and_write(low, s);
    CHECK(check_gcode(dropped.gcode, dropped.limits).stats.max_z > 14.99);
    // PL9: a cube from Z 50 to 70 off the bed is refused; one under it too.
    s.placement_drop_to_bed = false;
    IndexedMesh high;
    add_box(high, 90, 90, 50, 110, 110, 70);
    CHECK_THAT(unprintable_reason(high, s), ContainsSubstring("no extrusions on the first layer"));
    IndexedMesh under;
    add_box(under, 90, 90, -20, 110, 110, -1);
    CHECK_THAT(unprintable_reason(under, s), ContainsSubstring("under the bed"));
    CHECK(parse_settings(R"({"placement": {"drop_to_bed": false}})").placement_drop_to_bed == false);
}

TEST_CASE("G-code check: a part below or above z = 0 prints from the bed up", "[gcode][check]") {
    // The engine does not move parts in X and Y, but a part can only print
    // resting on the bed: Z in the G-code counts from the part's bottom.
    Settings s = test_printer();
    s.changes.push_back([] {
        HeightChange c;
        c.z = 2.0;
        c.temperature = 200.0;
        return c;
    }());
    const Output on_bed = slice_and_write(table(), s);
    REQUIRE_THAT(on_bed.gcode, ContainsSubstring("M104 S200 ; change at Z 2.000"));
    for (const float shift : {-50.0f, -3.5f, 12.25f}) {
        INFO("shifted by " << shift);
        IndexedMesh m = table();
        for (Vec3f& v : m.vertices)
            v.z += shift;
        const Output o = slice_and_write(m, s);
        const GcodeCheck c = check_gcode(o.gcode, o.limits);
        INFO(problems_of(c));
        CHECK(c.ok());
        CHECK(c.stats.max_z > 3.99); // the table is 4 mm tall
        CHECK(c.stats.max_z < 4.01);
        CHECK_THAT(o.gcode, ContainsSubstring(";Z:0.200\n"));
        // The same G-code as the part on the bed, height changes included.
        CHECK(o.gcode == on_bed.gcode);
    }
}
