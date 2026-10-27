// Mixed colours with one nozzle (docs/spec/features/color_mixing.md, rules
// 5 to 16; tests FS1 to FS14).

#include "config/settings.hpp"
#include "gcode/check.hpp"
#include "gcode/writer.hpp"
#include "mesh/indexed_mesh.hpp"
#include "mesh/mesh.hpp"
#include "project/project.hpp"
#include "slice/color_mix.hpp"
#include "slice/pipeline.hpp"
#include "slice/placement.hpp"
#include "test_printer.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <sstream>
#include <string>
#include <vector>

using namespace tisma;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::WithinAbs;

namespace {

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

// The spec's base (walls_extra.md): 0.4 nozzle, 0.2 layers, 2 walls, no
// infill, no skirt or brim; one nozzle for `filaments` filaments, the wipe
// tower, relative E.
Settings base(int filaments = 2) {
    Settings s = test_printer();
    s.flavor = GcodeFlavor::Marlin;
    s.relative_extrusion = true;
    s.bed = {{0, 0}, {250, 0}, {250, 210}, {0, 210}};
    s.layer_height = s.first_layer_height = 0.2;
    s.wall_count = 2;
    s.infill_density = 0.0;
    s.skirt_loops = 0;
    s.brim_width = 0.0;
    s.extruders = filaments;
    s.single_extruder_mmu = true;
    s.wipe_tower = true;
    return s;
}

ColorMix blend(int id, std::vector<ColorMix::Component> components) {
    ColorMix m;
    m.id = id;
    m.components = std::move(components);
    return m;
}

struct Plate {
    TriangleMesh mesh;
    std::vector<MeshObject> objects;
};

// The spec's box, 20 x 20 x 10 mm (50 layers), with the given extruder.
Plate box(int extruder) {
    Plate p;
    add_box(p.mesh, 90, 90, 0, 110, 110, 10);
    p.objects = {{"box", 0, 12, 0, 0, extruder}};
    return p;
}

SlicedModel sliced(const Plate& p, const Settings& s, const std::vector<ColorMix>& mixes) {
    return slice_model(weld_exact(p.mesh), s, nullptr, nullptr, nullptr, &p.objects, &mixes);
}

// The filament of each layer, as digits: the box's walls' extruder.
std::string sequence(const SlicedModel& model) {
    std::string out;
    for (const LayerWalls& w : model.walls)
        out += w.toolpaths.empty() ? '?' : char('0' + w.toolpaths.front().tool);
    return out;
}

std::string gcode_of(const SlicedModel& model, const Settings& s) {
    std::ostringstream out;
    write_gcode(out, model, s);
    const GcodeCheck check = check_gcode(out.str(), gcode_limits(model, s));
    INFO((check.problems.empty() ? std::string()
                                 : std::to_string(check.problems.front().line) + ": " + check.problems.front().message));
    CHECK(check.ok());
    return out.str();
}

// Tool changes after the first one.
int changes(const std::string& text) {
    int n = -1;
    std::istringstream in(text);
    for (std::string line; std::getline(in, line);)
        n += line.size() == 2 && line[0] == 'T' && line[1] >= '0' && line[1] <= '9';
    return n;
}

std::string repeat(const std::string& cycle, std::size_t n) {
    std::string out;
    while (out.size() < n)
        out += cycle;
    return out.substr(0, n);
}

} // namespace

TEST_CASE("color mixing: the cycle of each blend (rule 7)", "[mixing]") {
    struct Row {
        double first;
        int layers, of_first;
    };
    for (const Row& r : {Row{0.50, 2, 1}, {0.45, 7, 3}, {0.40, 5, 2}, {0.35, 3, 1}, {0.30, 7, 2}, {0.25, 4, 1},
                         {0.20, 5, 1}, {0.15, 6, 1}, {0.10, 8, 1}, {0.07, 11, 1}, {0.05, 13, 1}}) {
        INFO(r.first);
        const std::vector<int> c = mix_cycle({r.first, 1.0 - r.first});
        REQUIRE(c.size() == 2);
        CHECK(c[0] + c[1] == r.layers);
        CHECK(c[0] == r.of_first);
    }
    CHECK(mix_cycle({1.0 / 3, 1.0 / 3, 1.0 / 3}) == std::vector<int>{1, 1, 1});
}

TEST_CASE("color mixing: the order in the cycle (rule 8, FS2 to FS7)", "[mixing]") {
    const Settings s = base();
    const auto of = [&](std::vector<ColorMix::Component> c) {
        return sequence(sliced(box(3), s, {blend(3, std::move(c))}));
    };
    // FS1: 1 : 1.
    CHECK(of({{1, 0.5}, {2, 0.5}}) == repeat("12", 50));
    // FS2: 1 : 3, read as such too (rule 16).
    CHECK(of({{1, 0.25}, {2, 0.75}}) == repeat("2122", 50));
    const std::vector<ColorMix> read = parse_color_mixes(
        R"({"virtual_extruders": [{"id": 3, "components": [{"extruder": 1, "ratio": 1}, {"extruder": 2, "ratio": 3}]}]})");
    REQUIRE(read.size() == 1);
    CHECK(sequence(sliced(box(3), s, read)) == repeat("2122", 50));
    // FS3 and FS4: 2 : 3 and 3 : 7 (a cycle of 7, not 10).
    CHECK(of({{1, 0.4}, {2, 0.6}}) == repeat("21212", 50));
    CHECK(of({{1, 0.3}, {2, 0.7}}) == repeat("2122212", 50));
    // FS5: a 1 every 8, 11 and 13 layers, the first at layer 4, 6 and 7.
    for (const auto& [share, every, first] : {std::tuple{0.1, 8, 4}, {0.07, 11, 6}, {0.05, 13, 7}}) {
        const std::string q = of({{1, share}, {2, 1.0 - share}});
        CHECK(q.find('1') + 1 == std::size_t(first));
        CHECK(q.find('1', q.find('1') + 1) - q.find('1') == std::size_t(every));
    }
    // FS7: three filaments, 1 : 1 : 1.
    Settings three = base(3);
    CHECK(sequence(sliced(box(4), three, {blend(4, {{1, 1.0}, {2, 1.0}, {3, 1.0}})})) == repeat("123", 50));
    // FS8: a component at 0, all with the other.
    CHECK(of({{1, 0.0}, {2, 1.0}}) == std::string(50, '2'));
}

TEST_CASE("color mixing: 49 changes in 50 layers, and their cost (FS1, FS11)", "[mixing]") {
    Settings s = base();
    s.filament_density = 1.24;
    const SlicedModel mixed = sliced(box(3), s, {blend(3, {{1, 0.5}, {2, 0.5}})});
    const std::string text = gcode_of(mixed, s);
    CHECK(changes(text) == 49);
    // Almost all the filament goes to the purge (FS11: 10.17 g of 11.19 g on
    // the tower in Tisma Slicer).
    const auto grams = [&](const char* line) {
        const std::size_t at = text.find(line);
        REQUIRE(at != std::string::npos);
        return std::atof(text.c_str() + at + std::string(line).size());
    };
    const double total = grams("; total filament used [g] = "), tower = grams("; total filament used for wipe tower [g] = ");
    CHECK(tower > 0.8 * total);
    // The box alone, with one filament: a small part of it.
    const std::string plain = gcode_of(sliced(box(1), s, {}), s);
    const std::size_t at = plain.find("; total filament used [g] = ");
    CHECK(std::atof(plain.c_str() + at + 28) < 0.2 * total);
}

TEST_CASE("color mixing: a gradient in bands of 10 layers (FS9)", "[mixing]") {
    ColorMix g = blend(3, {{1, 0.0}, {2, 1.0}});
    g.gradient = true;
    g.min_z = 0.0, g.max_z = 10.0;
    CHECK(sequence(sliced(box(3), base(), {g})) == "11112111111211121121212121212212221221222212222222");
    // Without a range, the run of layers the blend is in: the same here.
    g.min_z.reset(), g.max_z.reset();
    const std::string run = sequence(sliced(box(3), base(), {g}));
    CHECK(run.front() == '1');
    CHECK(run.back() == '2');
    // Under its range all the first filament, over it all the last.
    g.min_z = 4.0, g.max_z = 6.0;
    const std::string inside = sequence(sliced(box(3), base(), {g}));
    CHECK(inside.substr(0, 20) == std::string(20, '1'));
    CHECK(inside.substr(30) == std::string(20, '2'));
}

TEST_CASE("color mixing: half the box with a filament, half with a blend (FS10)", "[mixing]") {
    Plate p;
    add_box(p.mesh, 90, 90, 0, 100, 110, 10);
    add_box(p.mesh, 100, 90, 0, 110, 110, 10);
    p.objects = {{"plain", 0, 12, 0, 0, 1}, {"mixed", 12, 12, 1, 0, 3}};
    const Settings s = base();
    const SlicedModel model = sliced(p, s, {blend(3, {{1, 0.5}, {2, 0.5}})});
    // Odd layers all with 1; the even ones the first half with 1, then the
    // blend with 2: one change in and one out of every even layer, the next
    // layer starting with what the last ended with.
    for (std::size_t l = 0; l < model.walls.size(); ++l) {
        bool two = false;
        for (const Toolpath& t : model.walls[l].toolpaths)
            two = two || t.tool == 2;
        CHECK(two == (l % 2 == 1));
    }
    CHECK(changes(gcode_of(model, s)) == 49);
}

TEST_CASE("color mixing: blends this machine cannot print are left out (rule 5, FS12)", "[mixing]") {
    Settings one = base(1);
    one.single_extruder_mmu = false;
    one.wipe_tower = false;
    const ColorMix m = blend(3, {{1, 0.5}, {2, 0.5}});
    CHECK_FALSE(mix_usable(m, 1));
    CHECK(mix_usable(m, 2));
    CHECK_FALSE(mix_usable(blend(2, {{1, 0.5}, {2, 0.5}}), 2)); // its number is a real filament's
    const SlicedModel model = sliced(box(3), one, {m});
    CHECK(sequence(model) == std::string(50, '0'));
    // Two filaments, the blend's number not a blend: the last filament.
    CHECK(sequence(sliced(box(3), base(), {})) == std::string(50, '2'));
}

TEST_CASE("color mixing: an unknown variable in the change's template, known before export (rule 13, FS13)", "[mixing]") {
    Settings s = base();
    CHECK(toolchange_template_error(s).empty());
    s.gcode_toolchange = "; change\nT{no_such_variable}";
    const std::string error = toolchange_template_error(s);
    CHECK_THAT(error, ContainsSubstring("gcode.toolchange") && ContainsSubstring("line 2"));
    // The export fails the same way at the first change.
    const SlicedModel model = sliced(box(3), s, {blend(3, {{1, 0.5}, {2, 0.5}})});
    std::ostringstream out;
    CHECK_THROWS(write_gcode(out, model, s));
}

TEST_CASE("color mixing: the project's blends in their file (rules 14 to 16, FS14)", "[mixing]") {
    ColorMix a = blend(3, {{1, 0.5}, {2, 0.5}});
    a.color = "#f3e115";
    ColorMix b = blend(4, {{1, 0.75}, {2, 0.25}});
    b.color = "#f6e329";
    ColorMix g = blend(5, {{1, 0.0}, {2, 1.0}});
    g.gradient = true;
    g.min_z = 1.0, g.max_z = 9.0;
    const std::string text = color_mixes_json({a, b, g}, {"#FCE94F", "#FCE94F"});
    CHECK_THAT(text, ContainsSubstring(R"("kind": "fullspectrum")") && ContainsSubstring(R"("ratio": 0.75)") &&
                         ContainsSubstring(R"("kind": "gradient")") && ContainsSubstring(R"("position": 1.0)") &&
                         ContainsSubstring(R"("physical_extruders")") && ContainsSubstring(R"("minz": 1.0)"));
    const std::vector<ColorMix> back = parse_color_mixes(text);
    REQUIRE(back.size() == 3);
    CHECK(back[0] == a);
    CHECK(back[1] == b);
    CHECK(back[2] == g);
    CHECK_THROWS_AS(parse_color_mixes(R"({"virtual_extruders": [{"id": 3}]})"), ColorMixError);
    CHECK_THROWS_AS(parse_color_mixes("[1]"), ColorMixError);
    CHECK(parse_color_mixes("{}").empty());
    // The colour shown: between the two; white and black at 1 : 1, a quarter of white.
    CHECK(mix_color({"#ff0000", "#0000ff"}, {0.5, 0.5}) == "#400040");
    CHECK(mix_color({"#ffffff", "#000000"}, {0.5, 0.5}) == "#404040");
    CHECK(mix_color({"#ffffff"}, {1.0}) == "#ffffff");
}

TEST_CASE("color mixing: the blends and each model's extruder travel with the project (rule 14)", "[mixing]") {
    Project p;
    ProjectModel m;
    m.name = "box";
    add_box(m.mesh, 0, 0, 0, 10, 10, 10);
    m.extruder = 3;
    m.wipe_into_infill = true;
    p.models.push_back(m);
    ColorMix a = blend(3, {{1, 0.5}, {2, 0.5}});
    a.color = "#f3e115";
    p.mixes = {a};
    p.filament_colors = {"#FCE94F", "#204A87"};
    const Project back = read_project(write_project(p));
    REQUIRE(back.models.size() == 1);
    CHECK(back.models[0].extruder == 3);
    CHECK(back.models[0].wipe_into_infill);
    CHECK_FALSE(back.models[0].wipe_into_object);
    REQUIRE(back.mixes.size() == 1);
    CHECK(back.mixes[0] == a);
    CHECK(back.filament_colors == p.filament_colors);
    // A project without them reads as before.
    Project plain;
    plain.models.push_back(m);
    plain.models[0].extruder = 1;
    plain.models[0].wipe_into_infill = false;
    const Project read = read_project(write_project(plain));
    CHECK(read.mixes.empty());
    CHECK(read.models[0].extruder == 1);
}
