// Mixed colours with one nozzle (docs/spec/features/color_mixing.md, rules
// 5 to 16; tests FS1 to FS14).

#include "calibration/calibration.hpp"
#include "config/settings.hpp"
#include "gcode/check.hpp"
#include "gcode/writer.hpp"
#include "mesh/indexed_mesh.hpp"
#include "mesh/mesh.hpp"
#include "project/project.hpp"
#include "slice/color_mix.hpp"
#include "slice/pipeline.hpp"
#include "slice/placement.hpp"
#include "slice/wipe_tower.hpp"
#include "test_printer.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace tisma;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

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

// color_mixing_plus.md: written sequences, automatic pairs, the new data in
// the project (rules 1, 2, 5, 6, 30; MX1 to MX4, MX19).

TEST_CASE("color mixing plus: the automatic pairs of 4 filaments (rule 5, MX1)", "[mixing]") {
    const std::vector<ColorMix> pairs = auto_pair_mixes({"#ffffff", "#000000", "#ff0000", "#0000ff"});
    REQUIRE(pairs.size() == 6);
    for (std::size_t k = 0; k < pairs.size(); ++k) {
        CHECK(pairs[k].id == int(k) + 5);
        REQUIRE(pairs[k].components.size() == 2);
        CHECK(pairs[k].components[0].share == 0.5);
        CHECK(pairs[k].components[0].extruder < pairs[k].components[1].extruder);
        CHECK(mix_usable(pairs[k], 4));
    }
    CHECK(pairs[0].components[0].extruder == 1);
    CHECK(pairs[0].components[1].extruder == 2);
    CHECK(pairs[5].components[0].extruder == 3);
    CHECK(pairs[5].components[1].extruder == 4);
    CHECK(pairs[0].color == mix_color({"#ffffff", "#000000"}, {0.5, 0.5}));
    // Turned off, it is not offered (MX2).
    ColorMix off = pairs[0];
    off.enabled = false;
    CHECK_FALSE(mix_usable(off, 4));
    // A part that used it says so when sliced.
    ColorMix half = blend(3, {{1, 0.5}, {2, 0.5}});
    half.enabled = false;
    const SlicedModel model = sliced(box(3), base(), {half});
    CHECK(sequence(model) == std::string(50, '2'));
    REQUIRE(model.notices.size() == 1);
    CHECK_THAT(model.notices.front(), ContainsSubstring("blend 3 is turned off"));
    CHECK(sliced(box(3), base(), {blend(3, {{1, 0.5}, {2, 0.5}})}).notices.empty());
}

TEST_CASE("color mixing plus: a written sequence, by numbers or letters (rule 2, MX3, MX4)", "[mixing]") {
    const std::vector<int> want{1, 1, 2};
    CHECK(parse_mix_pattern("1,1,2", {}, 4) == want);
    CHECK(parse_mix_pattern("1 1 2", {}, 4) == want);
    CHECK(parse_mix_pattern("1/1-2", {}, 4) == want);
    CHECK(parse_mix_pattern("AAB", {}, 4) == want);
    CHECK(parse_mix_pattern("aab", {3, 1}, 4) == std::vector<int>{3, 3, 1}); // the blend's filaments
    CHECK(parse_mix_pattern("10, 2", {}, 12) == std::vector<int>{10, 2});   // a run of digits is one number
    // MX4: a filament that does not exist, a letter past the blend's, nothing.
    CHECK_THROWS_WITH(parse_mix_pattern("1,9", {}, 4), ContainsSubstring("filament 9"));
    CHECK_THROWS_AS(parse_mix_pattern("ABC", {1, 2}, 4), ColorMixError);
    CHECK_THROWS_AS(parse_mix_pattern(" , ", {}, 4), ColorMixError);
    CHECK_THROWS_AS(parse_mix_pattern("1;2", {}, 4), ColorMixError);
    // The share of each filament, from the sequence: 1 at 66.7 %.
    const ColorMix m = pattern_mix(3, want);
    REQUIRE(m.components.size() == 2);
    CHECK_THAT(m.components[0].share, WithinAbs(2.0 / 3.0, 1e-9));
    CHECK(m.components[1].extruder == 2);
    // Printed as it is, repeated, from the blend's first layer: the same
    // G-code from both writings.
    const Settings s = base();
    const SlicedModel a = sliced(box(3), s, {pattern_mix(3, parse_mix_pattern("1,1,2", {}, 2))});
    const SlicedModel b = sliced(box(3), s, {pattern_mix(3, parse_mix_pattern("AAB", {}, 2))});
    CHECK(sequence(a) == repeat("112", 50));
    CHECK(gcode_of(a, s) == gcode_of(b, s));
    // A sequence that is not the ratio's cycle: 1,2,2,1 stays as written,
    // where the ratio 1 : 1 would give 1,2.
    CHECK(sequence(sliced(box(3), s, {pattern_mix(3, {1, 2, 2, 1})})) == repeat("1221", 50));
}

TEST_CASE("color mixing plus: the new data in the project's file (rule 30, MX19)", "[mixing]") {
    // A project of ratio blends is written as before.
    const std::vector<ColorMix> old{blend(3, {{1, 0.25}, {2, 0.75}})};
    const std::string text = color_mixes_json(old, {"#ffffff", "#000000"});
    for (const char* key : {"pattern", "enabled", "name", "bias"})
        CHECK_THAT(text, !ContainsSubstring(std::string("\"") + key + "\""));
    CHECK(parse_color_mixes(text) == old);
    // A sequence, turned off, named, with a bias: back as it was.
    ColorMix p = pattern_mix(4, {1, 1, 2});
    p.color = "#555555";
    p.enabled = false;
    p.name = "Dark grey";
    p.bias = -0.5;
    const std::vector<ColorMix> back = parse_color_mixes(color_mixes_json({p}, {"#ffffff", "#000000"}));
    REQUIRE(back.size() == 1);
    CHECK(back[0] == p);
    CHECK_THROWS_AS(parse_color_mixes(R"({"virtual_extruders": [{"id": 3, "kind": "pattern", "components": []}]})"),
                    ColorMixError);
}

namespace {

// Per layer of a G-code (;LAYER_CHANGE), the Zs each tool extrudes at.
std::vector<std::map<int, std::set<double>>> extruding_z(const std::string& text) {
    std::vector<std::map<int, std::set<double>>> out;
    std::istringstream in(text);
    int tool = 0;
    double z = 0.0;
    bool tower = false;
    for (std::string line; std::getline(in, line);) {
        if (line.rfind(";LAYER_CHANGE", 0) == 0)
            out.emplace_back();
        if (line.rfind(";TYPE:", 0) == 0)
            tower = line == ";TYPE:Wipe tower" || line == ";TYPE:Custom";
        if (line.size() == 2 && line[0] == 'T' && line[1] >= '0' && line[1] <= '9')
            tool = line[1] - '0';
        if (line.rfind("G1", 0) != 0 && line.rfind("G0", 0) != 0)
            continue;
        if (const std::size_t at = line.find(" Z"); at != std::string::npos)
            z = std::atof(line.c_str() + at + 2);
        if (tower)
            continue; // the tower and the change's own lines are at the layer's top
        const std::size_t e = line.find(" E");
        if (!out.empty() && e != std::string::npos && std::atof(line.c_str() + e + 2) > 0.0 && line.find(" X") != std::string::npos)
            out.back()[tool].insert(std::round(z * 1000.0) / 1000.0);
    }
    return out;
}

} // namespace

TEST_CASE("color mixing plus: sublayers where the blend is, one pass per filament (rules 10 and 11, MX8 to MX10)", "[mixing]") {
    Settings s = base();
    s.mix_subdivide = true;
    const ColorMix half = blend(3, {{1, 0.5}, {2, 0.5}});
    // MX8: in each layer, two passes of 0.1 mm, one of each filament.
    const SlicedModel model = sliced(box(3), s, {half});
    REQUIRE(model.walls.size() == 50);
    const LayerWalls& w = model.walls[10];
    std::set<int> tools;
    for (const Toolpath& t : w.toolpaths) {
        tools.insert(t.tool);
        REQUIRE(t.wavy);
        const double dz = t.points.front().dz, below = t.points.front().below;
        CHECK(((std::abs(dz + 0.1) < 1e-9 && std::abs(below) < 1e-9) || (std::abs(dz) < 1e-9 && std::abs(below - 0.1) < 1e-9)));
    }
    CHECK(tools == std::set<int>{1, 2});
    const std::string text = gcode_of(model, s);
    const auto zs = extruding_z(text);
    REQUIRE(zs.size() >= 50);
    // Layer 11 (top 2.2 mm): one tool at 2.1, the other at 2.2; the lower first.
    const auto& layer = zs[10];
    REQUIRE(layer.size() == 2);
    std::set<double> all;
    for (const auto& [t, z] : layer) {
        all.insert(z.begin(), z.end());
        INFO("tool " << t << ": " << z.size() << " Zs, first " << *z.begin() << ", last " << *z.rbegin());
        CHECK(z.size() == 1);
    }
    CHECK(all == std::set<double>{2.1, 2.2});
    // One change a layer: the next layer starts with the filament this one ended with.
    CHECK(changes(text) <= 50);
    // A pass lays half the layer's plastic: the walls' filament as without sublayers.
    Settings plain = s;
    plain.mix_subdivide = false;
    const auto filament = [](const std::string& g) {
        const std::size_t at = g.find("; filament used [mm] = ");
        return at == std::string::npos ? 0.0 : std::atof(g.c_str() + at + 23);
    };
    INFO(text.substr(text.find("; filament used"), 80));
    const double sub = filament(text), whole = filament(gcode_of(sliced(box(3), plain, {half}), plain));
    CHECK(sub > 0.0);
    CHECK_THAT(sub, WithinRel(whole, 0.05));
    // MX10: the first layer whole, with one filament.
    REQUIRE(zs[0].size() == 1);
    CHECK(*zs[0].begin()->second.rbegin() == 0.2); // the part at the layer's top (the start's own lines aside)
    s.mix_sub_first_layer = true;
    CHECK(extruding_z(gcode_of(sliced(box(3), s, {half}), s))[0].size() == 2);
    // A share thinner than sub_min_height: the cycle, no sublayers.
    s.mix_sub_min_height = 0.15;
    const SlicedModel thin = sliced(box(3), s, {half}); // kept alive: the loop reads its walls
    for (const Toolpath& t : thin.walls[10].toolpaths)
        CHECK_FALSE(t.wavy);
}

TEST_CASE("color mixing plus: half the box without the blend prints once a layer, whole (rule 10, MX9)", "[mixing]") {
    Settings s = base();
    s.mix_subdivide = true;
    Plate p;
    add_box(p.mesh, 90, 90, 0, 100, 110, 10);
    add_box(p.mesh, 100, 90, 0, 110, 110, 10);
    p.objects = {{"plain", 0, 12, 0, 0, 1}, {"mixed", 12, 12, 1, 0, 3}};
    const SlicedModel model = sliced(p, s, {blend(3, {{1, 0.5}, {2, 0.5}})});
    const auto zs = extruding_z(gcode_of(model, s));
    REQUIRE(zs.size() > 10);
    // Layer 11: the blend's two passes at 2.1 and 2.2; the plain half only at 2.2.
    std::set<double> all;
    for (const auto& [t, z] : zs[10])
        all.insert(z.begin(), z.end());
    CHECK(all == std::set<double>{2.1, 2.2});
    int whole = 0;
    for (const Toolpath& t : model.walls[10].toolpaths)
        whole += !t.wavy;
    CHECK(whole > 0); // the plain half's walls, not split
}

TEST_CASE("color mixing plus: the changes a blend asks for, counted before slicing (rule 12)", "[mixing]") {
    Settings s = base();
    const ColorMix half = blend(3, {{1, 0.5}, {2, 0.5}});
    const ColorMix third = blend(3, {{1, 1.0}, {2, 2.0}});
    const auto estimate = [&](const ColorMix& m, const Settings& t) {
        MixChangeOptions o;
        o.first_layer_height = t.first_layer_height;
        o.layer_height = t.layer_height;
        o.subdivide = t.mix_subdivide;
        o.sub_first_layer = t.mix_sub_first_layer;
        o.sub_min_height = t.mix_sub_min_height;
        return mix_changes_estimate(m, 10.0, o);
    };
    for (const bool sub : {false, true})
        for (const bool first : {false, true})
            for (const ColorMix& m : {half, third}) {
                s.mix_subdivide = sub;
                s.mix_sub_first_layer = first;
                const int counted = changes(gcode_of(sliced(box(3), s, {m}), s));
                INFO("subdivide " << sub << ", first layer " << first << ", shares " << m.components[0].share);
                CHECK(estimate(m, s) == counted);
            }
    // Nothing to print, nothing to change.
    CHECK(mix_changes_estimate(half, 0.0, {}) == 0);
    // Sublayers ask for more changes than a 1:2 cycle (a 50/50 one already
    // changes every layer).
    MixChangeOptions o;
    o.subdivide = true;
    MixChangeOptions cycle;
    CHECK(mix_changes_estimate(third, 10.0, o) > mix_changes_estimate(third, 10.0, cycle));
}

TEST_CASE("color mixing plus: each filament with its own layer height (rule 8, MX7)", "[mixing]") {
    Settings s = base();
    s.mix_cadence_heights = true;
    s.mix_height_a = 0.3;
    s.mix_height_b = 0.15;
    const ColorMix half = blend(3, {{1, 0.5}, {2, 0.5}});
    const SlicedModel model = sliced(box(3), s, {half});
    REQUIRE(model.layers.size() > 10);
    // The first layer the first filament's; then 0.15 mm of the second and
    // 0.3 mm of the first in turn, up to the top.
    CHECK(sequence(model).front() == '1');
    for (std::size_t l = 1; l + 1 < model.layers.size(); ++l) {
        const double h = model.layers[l].top - model.layers[l].bottom;
        INFO("layer " << l << ": " << h << " mm");
        CHECK_THAT(h, WithinAbs(l % 2 ? 0.15 : 0.3, 1e-6));
        REQUIRE_FALSE(model.walls[l].toolpaths.empty());
        CHECK(model.walls[l].toolpaths.front().tool == (l % 2 ? 2 : 1));
    }
    CHECK_THAT(model.layers.back().top, WithinAbs(10.0, 0.16));
    // The G-code passes the check, and its layer changes follow the filaments: one each layer.
    const std::string text = gcode_of(model, s);
    CHECK(changes(text) == int(model.layers.size()) - 1);
    // Within the machine's limits: a height above the largest is cut to it, with a notice.
    s.max_layer_height = 0.25;
    const SlicedModel capped = sliced(box(3), s, {half});
    for (std::size_t l = 0; l < capped.layers.size(); ++l)
        CHECK(capped.layers[l].top - capped.layers[l].bottom <= 0.25 + 1e-9);
    CHECK(std::any_of(capped.notices.begin(), capped.notices.end(),
                      [](const std::string& n) { return n.find("kept within the machine's layer heights") != std::string::npos; }));
    // A gradient is not two heights: its cycle, every layer as thick as the
    // layer height, with a notice.
    s.max_layer_height = 0.0;
    ColorMix fade = half;
    fade.gradient = true;
    const SlicedModel cycle = sliced(box(3), s, {fade});
    CHECK(cycle.layers.size() == 50);
    CHECK(std::any_of(cycle.notices.begin(), cycle.notices.end(),
                      [](const std::string& n) { return n.find("cadence_heights needs one blend") != std::string::npos; }));
}

TEST_CASE("color mixing plus: the cycle counted in height (rule 9, MX28, MX29)", "[mixing]") {
    const ColorMix half = blend(3, {{1, 0.5}, {2, 0.5}});
    // MX28: no sublayers, step 0.2, layers of 0.1 then 0.3: the filament
    // changes every 0.2 mm of height, by each layer's middle.
    Settings s = base();
    s.mix_step = 0.2;
    s.layer_heights = {{0.2, 0.1}, {4.2, 0.3}};
    const SlicedModel model = sliced(box(3), s, {half});
    REQUIRE(model.layers.size() > 20);
    int thin = 0, thick = 0;
    for (std::size_t l = 0; l < model.layers.size(); ++l) {
        const Layer& layer = model.layers[l];
        const double middle = 0.5 * (layer.bottom + layer.top) - model.layers.front().bottom;
        const int want = int(std::floor(middle / 0.2 + 1e-9)) % 2 == 0 ? 1 : 2;
        REQUIRE_FALSE(model.walls[l].toolpaths.empty());
        INFO("layer " << l << " middle " << middle);
        CHECK(model.walls[l].toolpaths.front().tool == want);
        const double h = layer.top - layer.bottom;
        thin += std::abs(h - 0.1) < 1e-6, thick += std::abs(h - 0.3) < 1e-6;
    }
    CHECK(thin >= 10);
    CHECK(thick >= 10);
    // Two 0.1 mm layers in a row with the same filament.
    for (std::size_t l = 1; l + 1 < model.layers.size(); ++l)
        if (std::abs(model.layers[l].bottom - 1.0) < 1e-6) {
            CHECK(model.walls[l].toolpaths.front().tool == model.walls[l + 1].toolpaths.front().tool);
            CHECK(model.walls[l].toolpaths.front().tool != model.walls[l + 2].toolpaths.front().tool);
        }
    gcode_of(model, s);
    // The step as the layer: as without it.
    Settings same = base();
    same.mix_step = 0.2;
    CHECK(sequence(sliced(box(3), same, {half})) == sequence(sliced(box(3), base(), {half})));

    // MX29 with two filaments: sublayers, layer 0.2, step 0.1: two passes of
    // 0.1 a layer, each with the cycle's next element.
    Settings sub = base();
    sub.mix_subdivide = true;
    sub.mix_step = 0.1;
    const SlicedModel passes = sliced(box(3), sub, {half});
    for (const std::size_t l : {std::size_t(5), std::size_t(6)}) {
        std::set<int> tools;
        for (const Toolpath& t : passes.walls[l].toolpaths) {
            tools.insert(t.tool);
            CHECK(t.wavy);
            const double dz = t.points.front().dz;
            CHECK((std::abs(dz + 0.1) < 1e-9 || std::abs(dz) < 1e-9));
        }
        CHECK(tools == std::set<int>{1, 2});
    }
    gcode_of(passes, sub);
    // MX29 as written (layer 0.3, step 0.1: A, B, A): one filament twice in a
    // layer is not printed in passes yet; by height, whole, with a notice.
    sub.layer_height = sub.first_layer_height = 0.3;
    const SlicedModel three = sliced(box(3), sub, {half});
    CHECK(std::any_of(three.notices.begin(), three.notices.end(),
                      [](const std::string& n) { return n.find("mixing.step") != std::string::npos; }));
    for (std::size_t l = 0; l < three.layers.size(); ++l)
        for (const Toolpath& t : three.walls[l].toolpaths)
            CHECK_FALSE(t.wavy);
    gcode_of(three, sub);
}

TEST_CASE("color mixing plus: the blend only in the shell (rule 13, MX11, MX12)", "[mixing]") {
    Settings s = base();
    s.wall_count = 3;
    s.infill_density = 20.0;
    s.mix_surface_only = true;
    s.mix_surface_depth = 0.8;
    const ColorMix most = blend(3, {{1, 0.25}, {2, 0.75}}); // 2 is the larger share
    const SlicedModel model = sliced(box(3), s, {most});
    REQUIRE(model.walls.size() == 50);
    // MX11: in each layer the two outer walls with the layer's filament of
    // the cycle, the third wall and the sparse infill with filament 2.
    std::set<int> outer_tools;
    for (std::size_t l = 5; l < 45; ++l) {
        int layer_tool = -1;
        for (const Toolpath& t : model.walls[l].toolpaths) {
            INFO("layer " << l << " wall " << t.index);
            if (t.index >= 2) {
                CHECK(t.tool == 2);
            } else {
                if (layer_tool < 0)
                    layer_tool = t.tool;
                CHECK(t.tool == layer_tool);
            }
        }
        outer_tools.insert(layer_tool);
        for (const Toolpath& t : model.fills[l].sparse)
            CHECK(t.tool == 2);
    }
    CHECK(outer_tools == std::set<int>{1, 2}); // the outside still alternates
    const std::string shell = gcode_of(model, s);
    // MX12: no more changes than with the blend all through.
    Settings all = s;
    all.mix_surface_only = false;
    const std::string through = gcode_of(sliced(box(3), all, {most}), all);
    CHECK(changes(shell) <= changes(through));
    // Rule 19: without blends, the same moves (the settings' own comments aside).
    const auto moves = [](const std::string& g) {
        std::string out;
        std::istringstream in(g);
        for (std::string line; std::getline(in, line);)
            if (!line.empty() && line[0] != ';')
                out += line + '\n';
        return out;
    };
    CHECK(moves(gcode_of(sliced(box(1), s, {}), s)) == moves(gcode_of(sliced(box(1), all, {}), all)));
}

TEST_CASE("color mixing plus: more walls in the zones of another colour (rule 14, MX14)", "[mixing]") {
    Settings s = base();
    s.mix_extra_walls = 2;
    // One object: a box with filament 1 and, against it, a part painted with
    // filament 2; another object with filament 2 alone.
    Plate p;
    add_box(p.mesh, 90, 90, 0, 110, 110, 10);
    add_box(p.mesh, 110, 90, 0, 130, 110, 10);
    add_box(p.mesh, 60, 40, 0, 80, 60, 10);
    p.objects = {{"box", 0, 12, 0, 0, 1}, {"painted", 12, 12, 0, 0, 2}, {"other", 24, 12, 1, 0, 2}};
    const auto walls_of_tool = [](const SlicedModel& m, int tool, double x0, double x1) {
        int most = 0;
        for (const Toolpath& t : m.walls[20].toolpaths)
            if (t.tool == tool && !t.points.empty() && t.points.front().p.x > x0 && t.points.front().p.x < x1)
                most = std::max(most, t.index + 1);
        return most;
    };
    const SlicedModel model = sliced(p, s, {});
    CHECK(walls_of_tool(model, 1, 85, 115) == 2);  // the object's own filament: as many as always
    CHECK(walls_of_tool(model, 2, 105, 135) == 4); // the painted part: 2 more
    CHECK(walls_of_tool(model, 2, 55, 85) == 2);   // another object of filament 2: as always
    // A blend's part (a zone with a blend): 2 more as well.
    const SlicedModel blended = sliced(box(3), s, {blend(3, {{1, 0.5}, {2, 0.5}})});
    int most = 0;
    for (const Toolpath& t : blended.walls[20].toolpaths)
        most = std::max(most, t.index + 1);
    CHECK(most == 4);
    gcode_of(model, s);
}

TEST_CASE("color mixing plus: the bias sinks one filament's outer walls (rule 15, MX13)", "[mixing]") {
    Settings s = base();
    ColorMix half = blend(3, {{1, 0.5}, {2, 0.5}});
    half.bias = 0.5;
    const SlicedModel model = sliced(box(3), s, {half});
    // The outer wall's left edge and its width on a layer.
    const auto outer = [&](std::size_t l, int& tool, double& width) {
        double x = 1e9;
        for (const Toolpath& t : model.walls[l].toolpaths)
            if (t.index == 0)
                for (const ToolpathPoint& p : t.points)
                    if (p.p.x < x)
                        x = p.p.x, tool = t.tool, width = p.width;
        return x;
    };
    int t10 = 0, t11 = 0;
    double w10 = 0, w11 = 0;
    const double x10 = outer(10, t10, w10), x11 = outer(11, t11, w11);
    REQUIRE(t10 != t11);
    const double x1 = t10 == 1 ? x10 : x11, x2 = t10 == 1 ? x11 : x10;
    INFO("filament 1 at " << x1 << ", filament 2 at " << x2 << ", width " << w10);
    // The second filament's outer wall 0.5 x half its width further in.
    CHECK_THAT(x2 - x1, WithinAbs(0.25 * w10, 1e-6));
    gcode_of(model, s);
    // A negative bias sinks the first filament's instead.
    half.bias = -0.5;
    const SlicedModel other = sliced(box(3), s, {half});
    double left[2] = {1e9, 1e9};
    for (const std::size_t l : {std::size_t(10), std::size_t(11)})
        for (const Toolpath& t : other.walls[l].toolpaths)
            if (t.index == 0)
                for (const ToolpathPoint& p : t.points)
                    left[t.tool - 1] = std::min(left[t.tool - 1], p.p.x);
    CHECK(left[0] > left[1] + 0.05);
}

TEST_CASE("color mixing plus: the purge by the colours, more from dark to light (rule 37, MX25)", "[mixing]") {
    const double up = purge_from_colors("#000000", "#ffffff"), down = purge_from_colors("#ffffff", "#000000");
    CHECK(up > down);
    CHECK_THAT(up, WithinAbs(220.0, 0.5));   // 60 + 1.6 x 100 points of lightness
    CHECK_THAT(down, WithinAbs(60.0, 0.5));
    CHECK(purge_from_colors("#ff0000", "#ff0000") == 0.0);
    CHECK(purge_from_colors("#ff0000", "#0000ff") > 60.0); // colours apart purge more
    CHECK(purge_from_colors("#202020", "#ffff00") <= 300.0);
    const auto table = purge_table({"#000000", "#ffffff", "#ff0000"});
    REQUIRE(table.size() == 3);
    CHECK(table[0][0] == 0.0);
    CHECK(table[0][1] == up);
    CHECK(table[1][0] == down);
    CHECK(purge_from_colors("nonsense", "#808080") < 61.0); // unreadable: mid grey
}

TEST_CASE("color mixing plus: a step finer than the layers without sublayers goes by layer, and is said (rule 9)", "[mixing]") {
    // Step 0.05 on layers of 0.2: every layer's middle falls on an even
    // element of A, B, so the blend would print all in one filament.
    Settings s = base();
    s.mix_step = 0.05;
    const SlicedModel model = sliced(box(3), s, {blend(3, {{1, 0.5}, {2, 0.5}})});
    CHECK(std::any_of(model.notices.begin(), model.notices.end(),
                      [](const std::string& n) { return n.find("finer than the layers") != std::string::npos; }));
    // The layers follow the cycle layer by layer: both filaments, in turn.
    CHECK(sequence(model).substr(0, 6) == "121212");
    s.mix_subdivide = true; // with sublayers the passes follow the step: nothing to say
    const SlicedModel passes = sliced(box(3), s, {blend(3, {{1, 0.5}, {2, 0.5}})});
    CHECK(std::none_of(passes.notices.begin(), passes.notices.end(),
                       [](const std::string& n) { return n.find("finer than the layers") != std::string::npos; }));
}

TEST_CASE("color mixing plus: blends on one nozzle purge into the infill by default (rule 33, MX22)", "[mixing]") {
    Settings s = base();
    s.single_extruder_mmu = true;
    s.wipe_tower = true;
    s.infill_density = 20.0;
    const ColorMix half = blend(3, {{1, 0.5}, {2, 0.5}});
    const auto tower = [](const SlicedModel& m, const Settings& t, std::size_t& sinks) {
        const ToolPlan plan = plan_tools(m, t);
        double v = 0.0;
        sinks = 0;
        for (std::size_t l = 0; l < plan.tower_purge.size(); ++l) {
            for (const double p : plan.tower_purge[l])
                v += p;
            for (const auto& g : plan.sinks[l])
                sinks += g.size();
        }
        return v;
    };
    std::size_t into = 0, none = 0;
    const SlicedModel model = sliced(box(3), s, {half});
    REQUIRE_FALSE(model.objects.empty());
    CHECK(model.objects.front().wipe_into_infill);
    const double with = tower(model, s, into);
    Settings off = s;
    off.mix_purge_into_infill = false;
    const SlicedModel plain = sliced(box(3), off, {half});
    CHECK_FALSE(plain.objects.front().wipe_into_infill);
    const double without = tower(plain, off, none);
    INFO("tower " << with << " mm3 with " << into << " sinks, " << without << " mm3 without");
    CHECK(into > 0);
    CHECK(none == 0);
    CHECK(with < without);
    gcode_of(model, s);
    // A plate without blends keeps its parts' own choice.
    CHECK_FALSE(sliced(box(1), s, {}).objects.front().wipe_into_infill);
}

TEST_CASE("color mixing plus: no purge into a see-through filament's part (rule 33, MX27)", "[mixing]") {
    Settings s = base();
    s.single_extruder_mmu = true;
    s.wipe_tower = true;
    s.infill_density = 20.0;
    s.transmission_distance = {2.0, 15.0}; // filament 2 lets the light through 15 mm
    Plate p;
    add_box(p.mesh, 90, 90, 0, 110, 110, 10);
    add_box(p.mesh, 40, 40, 0, 60, 60, 10);
    add_box(p.mesh, 140, 40, 0, 160, 60, 10);
    p.objects = {{"blend", 0, 12, 0, 0, 3}, {"opaque", 12, 12, 1, 0, 1}, {"clear", 24, 12, 2, 0, 2}};
    const SlicedModel model = sliced(p, s, {blend(3, {{1, 0.5}, {2, 0.5}})});
    REQUIRE(model.objects.size() == 3);
    for (const SlicedObject& o : model.objects) {
        INFO(o.name);
        CHECK(o.wipe_into_infill == (o.name == "opaque")); // the blend has the clear filament in it
    }
    gcode_of(model, s);
    // One value for every filament.
    s.transmission_distance = {20.0};
    const SlicedModel all_clear = sliced(p, s, {blend(3, {{1, 0.5}, {2, 0.5}})});
    for (const SlicedObject& o : all_clear.objects)
        CHECK_FALSE(o.wipe_into_infill);
}

TEST_CASE("color mixing plus: after a change on one nozzle the part starts with its infill (rule 34)", "[mixing]") {
    Settings s = base();
    s.single_extruder_mmu = true;
    s.wipe_tower = true;
    s.infill_density = 20.0;
    s.mix_purge_into_infill = false; // the order alone, not the purge's own infill
    // For each layer with a change: its first kind in the part, and whether it has sparse infill.
    struct Changed {
        std::string first;
        bool sparse = false;
    };
    const auto changed_layers = [](const std::string& g) {
        std::vector<Changed> out;
        std::istringstream in(g);
        int tools = 0;
        bool changed = false, open = false;
        for (std::string line; std::getline(in, line);) {
            if (line.rfind(";LAYER_CHANGE", 0) == 0)
                changed = false, open = false;
            if (line.size() == 2 && line[0] == 'T' && line[1] >= '0' && line[1] <= '9' && ++tools > 1)
                changed = true;
            if (!changed || line.rfind(";TYPE:", 0) != 0 || line == ";TYPE:Wipe tower" || line == ";TYPE:Custom")
                continue;
            if (!open)
                out.push_back({line.substr(6)}), open = true;
            out.back().sparse = out.back().sparse || line == ";TYPE:Internal infill";
        }
        return out;
    };
    const SlicedModel model = sliced(box(3), s, {blend(3, {{1, 0.5}, {2, 0.5}})});
    const std::vector<Changed> layers = changed_layers(gcode_of(model, s));
    REQUIRE(layers.size() > 30);
    int with_sparse = 0;
    for (const Changed& c : layers)
        if (c.sparse) {
            ++with_sparse;
            CHECK(c.first == "Internal infill");
        }
    CHECK(with_sparse > 10);
    // A plate without blends keeps the walls first.
    Plate p;
    add_box(p.mesh, 90, 90, 0, 110, 110, 10);
    add_box(p.mesh, 40, 40, 0, 60, 60, 10);
    p.objects = {{"one", 0, 12, 0, 0, 1}, {"two", 12, 12, 1, 0, 2}};
    const std::vector<Changed> plain = changed_layers(gcode_of(sliced(p, s, {}), s));
    REQUIRE_FALSE(plain.empty());
    CHECK(std::none_of(plain.begin(), plain.end(), [](const Changed& c) { return c.first == "Internal infill"; }));
}

TEST_CASE("color mixing plus: the infill grows denser to take the purge (rule 35, MX23)", "[mixing]") {
    Settings s = base();
    s.single_extruder_mmu = true;
    s.wipe_tower = true;
    s.infill_density = 20.0;
    const ColorMix half = blend(3, {{1, 0.5}, {2, 0.5}});
    const auto sparse = [](const SlicedModel& m, std::size_t l) {
        double v = 0.0;
        const double h = m.layers[l].top - m.layers[l].bottom;
        for (const Toolpath& t : m.fills[l].sparse)
            for (std::size_t k = 1; k < t.points.size(); ++k)
                v += t.points[k].width * h * std::hypot(t.points[k].p.x - t.points[k - 1].p.x, t.points[k].p.y - t.points[k - 1].p.y);
        return v;
    };
    const auto tower = [](const SlicedModel& m, const Settings& t) {
        const ToolPlan plan = plan_tools(m, t);
        double v = 0.0;
        for (const auto& layer : plan.tower_purge)
            for (const double p : layer)
                v += p;
        return v;
    };
    const SlicedModel plain = sliced(box(3), s, {half});
    Settings more = s;
    more.mix_purge_infill_max = 60.0;
    const SlicedModel dense = sliced(box(3), more, {half});
    // A layer with a change in the middle: its sparse infill about 3 times
    // as much (20 % to 60 %: 140 mm3 of purge does not fit in either).
    const double before = sparse(plain, 20), after = sparse(dense, 20);
    INFO(before << " mm3 to " << after << " mm3");
    REQUIRE(before > 0.0);
    CHECK(after > 2.0 * before); // whole lines in a bounded region: not exactly 3
    CHECK(after < 3.5 * before);
    CHECK(tower(dense, more) < tower(plain, s));
    gcode_of(dense, more);
    // Off (0) or on several nozzles: as before.
    Settings tools = more;
    tools.single_extruder_mmu = false;
    CHECK_THAT(sparse(sliced(box(3), tools, {half}), 20), WithinRel(before, 1e-9));
}

TEST_CASE("color mixing plus: the denser infill also with the blend only in the shell (rules 13 and 35)", "[mixing]") {
    Settings s = base();
    s.single_extruder_mmu = true;
    s.wipe_tower = true;
    s.infill_density = 20.0;
    s.mix_surface_only = true;
    const ColorMix half = blend(3, {{1, 0.5}, {2, 0.5}});
    const auto sparse = [](const SlicedModel& m, std::size_t l) {
        double v = 0.0;
        for (const Toolpath& t : m.fills[l].sparse)
            for (std::size_t k = 1; k < t.points.size(); ++k)
                v += t.points[k].width * std::hypot(t.points[k].p.x - t.points[k - 1].p.x, t.points[k].p.y - t.points[k - 1].p.y);
        return v;
    };
    const SlicedModel plain = sliced(box(3), s, {half});
    Settings more = s;
    more.mix_purge_infill_max = 60.0;
    const SlicedModel dense = sliced(box(3), more, {half});
    // The layers where the shell's filament changes get the denser infill, in the blend's largest filament.
    double before = 0.0, after = 0.0;
    for (std::size_t l = 10; l < 40; ++l)
        before += sparse(plain, l), after += sparse(dense, l);
    CHECK(after > 1.3 * before);
    for (const Toolpath& t : dense.fills[20].sparse)
        CHECK(t.tool == 1);
    gcode_of(dense, more);
}

TEST_CASE("color mixing plus: blends on a plate line up their cycles (rule 39, MX26)", "[mixing]") {
    // Two boxes, blends 1 : 1 of (1, 2) and of (3, 1): as each starts, a has
    // filament 1 on the even layers and b on the odd ones, never together.
    Settings s = base(3);
    Plate p;
    add_box(p.mesh, 90, 90, 0, 110, 110, 10);
    add_box(p.mesh, 40, 40, 0, 60, 60, 10);
    p.objects = {{"a", 0, 12, 0, 0, 4}, {"b", 12, 12, 1, 0, 5}};
    const std::vector<ColorMix> mixes = {blend(4, {{1, 0.5}, {2, 0.5}}), blend(5, {{3, 0.5}, {1, 0.5}})};
    const auto together = [](const SlicedModel& m) {
        int both = 0;
        for (std::size_t l = 0; l < m.walls.size(); ++l) {
            std::set<int> a, b;
            for (const Toolpath& t : m.walls[l].toolpaths)
                (t.points.front().p.x > 80 ? a : b).insert(t.tool);
            both += a.count(1) && b.count(1);
        }
        return both;
    };
    const SlicedModel model = sliced(p, s, mixes);
    // In line: filament 1 in both boxes on every layer that has it (25 of 50).
    CHECK(together(model) == 25);
    // Each blend alone keeps its own cycle: b starts with 3.
    const SlicedModel alone = sliced(box(5), s, {mixes[1]});
    CHECK(sequence(alone).substr(0, 4) == "3131");
    gcode_of(model, s);
}

TEST_CASE("color mixing plus: what to suggest when the purge passes half the filament (rule 38)", "[mixing]") {
    const ColorMix half = blend(3, {{1, 0.5}, {2, 0.5}});
    MixChangeOptions o;
    o.subdivide = true;
    // A 10 mm box of 4000 mm3: 49+ changes of 140 mm3 are far above half.
    const auto list = mix_purge_suggestions(half, 10.0, o, 140.0, 4000.0);
    REQUIRE(list.size() == 3);
    CHECK(list[0].key == "subdivide");
    CHECK(list[1].key == "cadence_heights");
    CHECK(list[2].key == "surface_only");
    const int now = mix_changes_estimate(half, 10.0, o);
    CHECK(list[0].changes <= now);
    CHECK(list[1].changes < now);
    CHECK(list[1].purge_share < list[2].purge_share);
    for (const MixSuggestion& m : list) {
        CHECK(m.purge_share > 0.0);
        CHECK(m.purge_share < 1.0);
    }
    // Little purge: nothing to suggest.
    CHECK(mix_purge_suggestions(half, 10.0, o, 1.0, 4000.0).empty());
    // Three filaments: no cadence heights.
    const auto three = mix_purge_suggestions(blend(4, {{1, 1.0}, {2, 1.0}, {3, 1.0}}), 10.0, {}, 140.0, 4000.0);
    CHECK(std::none_of(three.begin(), three.end(), [](const MixSuggestion& m) { return m.key == "cadence_heights"; }));
}

TEST_CASE("color mixing plus: without a tower the purge goes to the discard, counted apart (rule 36, MX24)", "[mixing]") {
    Settings s = base();
    s.single_extruder_mmu = true;
    s.wipe_tower = false; // a printer that purges into a chute
    s.infill_density = 20.0;
    s.filament_density = 1.24;
    const SlicedModel model = sliced(box(3), s, {blend(3, {{1, 0.5}, {2, 0.5}})});
    std::ostringstream out;
    std::vector<Warning> warnings;
    write_gcode(out, model, s, &warnings);
    const auto purge = std::find_if(warnings.begin(), warnings.end(), [](const Warning& w) { return w.kind == "purge"; });
    REQUIRE(purge != warnings.end());
    INFO(purge->message);
    CHECK_THAT(purge->message, ContainsSubstring("to the discard"));
    CHECK_THAT(purge->message, !ContainsSubstring("0.00 g to the discard"));
    CHECK_THAT(purge->message, !ContainsSubstring("(0.00 g into their infill")); // the infill took its share first
    CHECK(out.str().find(";TYPE:Wipe tower") == std::string::npos);
}

TEST_CASE("color mixing plus: the colour sought, by CIEDE2000 (rule 4, MX5, MX6)", "[mixing]") {
    // Sharma, Wu and Dalal's published test pairs.
    CHECK_THAT(ciede2000_lab({50.0, 2.6772, -79.7751}, {50.0, 0.0, -82.7485}), WithinAbs(2.0425, 1e-4));
    CHECK_THAT(ciede2000_lab({50.0, 2.5, 0.0}, {73.0, 25.0, -18.0}), WithinAbs(27.1492, 1e-4));
    CHECK_THAT(ciede2000_lab({2.0776, 0.0795, -1.135}, {0.9033, -0.0636, -0.5514}), WithinAbs(0.9082, 1e-4));
    CHECK(ciede2000("#123456", "#123456") == 0.0);
    // MX5: grey from white and black: a blend of the two, with up to 3 more.
    const auto grey = match_color("#808080", {"#ffffff", "#000000"}, 3);
    REQUIRE_FALSE(grey.empty());
    CHECK(grey.size() <= 4);
    CHECK(grey.front().mix.components.size() == 2);
    CHECK(color_reached(grey));
    for (std::size_t k = 1; k < grey.size(); ++k)
        CHECK(grey[k].distance >= grey[k - 1].distance);
    // MX6: green is out of reach of white and black.
    CHECK_FALSE(color_reached(match_color("#00ff00", {"#ffffff", "#000000"}, 3)));
    // Three filaments: blends of 3 are tried too.
    const std::vector<std::string> rgb = {"#ff0000", "#00ff00", "#0000ff"};
    const auto three = match_color(mix_color(rgb, {1.0, 1.0, 1.0}), rgb, 4);
    REQUIRE_FALSE(three.empty());
    CHECK(three.front().mix.components.size() == 3);
    CHECK(three.front().distance < 0.5);
}

TEST_CASE("color mixing plus: the spectral prediction (rule 21, MX17)", "[mixing]") {
    const std::vector<std::string> by = {"#0000ff", "#ffff00"};
    // MX17: every TD at 0, the simple mixer's colour.
    CHECK(mix_color_spectral(by, {0.5, 0.5}, {0.0, 0.0}) == mix_color(by, {0.5, 0.5}));
    CHECK(mix_color_spectral(by, {0.5, 0.5}, {}) == mix_color(by, {0.5, 0.5}));
    // Same TD on both: a colour between the two, not either.
    const std::string even = mix_color_spectral(by, {0.5, 0.5}, {5.0, 5.0});
    CHECK(even != by[0]);
    CHECK(even != by[1]);
    // The more translucent filament weighs more: closer to its colour.
    const std::string yellow_clear = mix_color_spectral(by, {0.5, 0.5}, {1.0, 20.0});
    CHECK(ciede2000(yellow_clear, by[1]) < ciede2000(even, by[1]));
    // A filament alone keeps its colour (within rounding).
    CHECK(ciede2000(mix_color_spectral({"#c04020"}, {1.0}, {5.0}), "#c04020") < 1.0);
}

TEST_CASE("color mixing plus: mixing.predictor chooses how the colour sought is predicted (rules 20, 21)", "[mixing]") {
    // The setting: "mixer" by default, "spectral" accepted, anything else refused.
    CHECK(Settings{}.mix_predictor == "mixer");
    const auto dir = std::filesystem::temp_directory_path() / "tisma_mix_predictor";
    std::filesystem::create_directories(dir);
    const auto file = dir / "s.json";
    {
        std::ofstream(file) << R"({"mixing": {"predictor": "spectral"}})";
    }
    CHECK(load_settings(file).mix_predictor == "spectral");
    {
        std::ofstream(file) << R"({"mixing": {"predictor": "paint"}})";
    }
    CHECK_THROWS_AS(load_settings(file), SettingsError);
    std::filesystem::remove_all(dir);
    // match_color: with every TD at 0 the spectral prediction is the mixer's.
    const std::vector<std::string> by = {"#0000ff", "#ffff00"};
    const std::string target = mix_color(by, {0.5, 0.5});
    const auto plain = match_color(target, by, 1);
    const auto zero = match_color(target, by, 1, 6, 0.0, {0.0, 0.0});
    REQUIRE(plain.size() == zero.size());
    for (std::size_t k = 0; k < plain.size(); ++k)
        CHECK(plain[k].color == zero[k].color);
    // With TDs, each candidate's colour is the spectral one: Kubelka and Munk
    // darkens blue with yellow, so the light green needs another blend.
    const auto spectral = match_color(target, by, 1, 6, 0.0, {1.0, 20.0});
    REQUIRE_FALSE(spectral.empty());
    CHECK(spectral.front().color == mix_color_spectral(by, {spectral.front().mix.components[0].share,
                                                            spectral.front().mix.components[1].share},
                                                       {1.0, 20.0}));
    CHECK(spectral.front().distance == ciede2000(target, spectral.front().color));
    CHECK(spectral.front().mix.components != plain.front().mix.components);
}

TEST_CASE("color mixing plus: colour names and the quick palette (rules 22, 24, MX18)", "[mixing]") {
    CHECK(color_name("#ffffff").en == "White");
    CHECK(color_name("#000000").es == "Negro");
    CHECK(color_name("#808080").en == "Grey");
    CHECK(color_name("#ff0000").en == "Red");
    CHECK(color_name("#ff8000").en == "Orange");
    CHECK(color_name("#ffff00").en == "Light yellow");
    CHECK(color_name("#00a000").en == "Green");
    CHECK(color_name("#00ffff").en == "Light cyan");
    CHECK(color_name("#0000ff").en == "Dark blue");
    CHECK(color_name("#8000c0").en == "Dark purple");
    CHECK(color_name("#a040e0").en == "Purple");
    CHECK(color_name("#ff80c0").en == "Pink");
    CHECK(color_name("#ffc0e0").es == "Rosa claro");
    CHECK(color_name("#0000ff").es == "Azul oscuro");
    // Every name starts with a capital (the interface's list, 4 filaments).
    for (const char* hex : {"#ffffff", "#000000", "#ff0000", "#0000ff", "#800000", "#400060", "#ffc0e0", "#603010"})
        for (const std::string& n : {color_name(hex).en, color_name(hex).es})
            CHECK(std::isupper(static_cast<unsigned char>(n[0])));
    CHECK(color_name("#603010").en == "Brown");
    // MX18: 4 filaments, cycles of up to 2 layers: 10 samples, each named.
    const std::vector<std::string> four = {"#00ffff", "#ff00ff", "#ffff00", "#ffffff"};
    const auto ten = quick_palette(four, 2, 5);
    REQUIRE(ten.size() == 10);
    std::set<std::string> seen;
    for (std::size_t k = 0; k < ten.size(); ++k) {
        CHECK(ten[k].mix.id == 5 + int(k));
        CHECK_FALSE(ten[k].name.en.empty());
        CHECK_FALSE(ten[k].name.es.empty());
        CHECK(ten[k].mix.name == ten[k].name.en);
        CHECK(ten[k].color == ten[k].mix.color);
        std::string key;
        for (const auto& c : ten[k].mix.components)
            key += std::to_string(c.extruder) + ":" + std::to_string(c.share) + " ";
        CHECK(seen.insert(key).second);
    }
    CHECK(ten[0].mix.components.size() == 1);
    CHECK(ten[0].color == "#00ffff");
    // Up to 3 layers: 26.
    CHECK(quick_palette(four, 3, 5).size() == 26);
    // Spectral colours with the TDs.
    const auto spectral = quick_palette(four, 2, 5, {5.0, 5.0, 5.0, 5.0});
    CHECK(spectral[4].color == mix_color_spectral({four[0], four[1]}, {0.5, 0.5}, {5.0, 5.0}));
}

TEST_CASE("color mixing plus: the estimate before slicing with each filament's own height (rules 8 and 12)", "[mixing]") {
    Settings s = base();
    s.mix_cadence_heights = true;
    s.mix_height_a = 0.3;
    s.mix_height_b = 0.15;
    const ColorMix half = blend(3, {{1, 0.5}, {2, 0.5}});
    const int counted = changes(gcode_of(sliced(box(3), s, {half}), s));
    MixChangeOptions o;
    o.first_layer_height = s.first_layer_height;
    o.layer_height = s.layer_height;
    o.cadence_heights = true;
    o.height_a = s.mix_height_a;
    o.height_b = s.mix_height_b;
    CHECK(mix_changes_estimate(half, 10.0, o) == counted);
}

TEST_CASE("color mixing plus: the estimate before slicing follows mixing.step (rules 9 and 12)", "[mixing]") {
    const ColorMix half = blend(3, {{1, 0.5}, {2, 0.5}});
    for (const double step : {0.4, 0.6, 0.1}) { // coarser, coarser, and finer than the layers (by layer)
        Settings s = base();
        s.mix_step = step;
        const int counted = changes(gcode_of(sliced(box(3), s, {half}), s));
        MixChangeOptions o;
        o.first_layer_height = s.first_layer_height;
        o.layer_height = s.layer_height;
        o.step = s.mix_step;
        INFO("step " << step);
        CHECK(mix_changes_estimate(half, 10.0, o) == counted);
    }
}

TEST_CASE("color mixing plus: the estimate before slicing with mixing.step and sublayers (rules 9 and 12)", "[mixing]") {
    const ColorMix half = blend(3, {{1, 0.5}, {2, 0.5}});
    const ColorMix third = blend(3, {{1, 2.0 / 3.0}, {2, 1.0 / 3.0}});
    for (const ColorMix& mix : {half, third})
        for (const double step : {0.1, 0.08, 0.4}) {
            Settings s = base();
            s.mix_step = step;
            s.mix_subdivide = true;
            const int counted = changes(gcode_of(sliced(box(3), s, {mix}), s));
            MixChangeOptions o;
            o.first_layer_height = s.first_layer_height;
            o.layer_height = s.layer_height;
            o.subdivide = true;
            o.sub_min_height = s.mix_sub_min_height;
            o.sub_first_layer = s.mix_sub_first_layer;
            o.step = s.mix_step;
            INFO("step " << step << ", parts " << mix.components.front().share);
            CHECK(mix_changes_estimate(mix, 10.0, o) == counted);
        }
}

TEST_CASE("color mixing plus: a path cut at the bands of a gradient in X or Y (rule 3)", "[mixing]") {
    const auto at = [](double x, double y) { return ToolpathPoint{{x, y}, 0.45}; };
    // A line from x 0 to 25 across bands of 10 mm from 0: three pieces.
    Toolpath line;
    line.points = {at(0, 0), at(25, 0)};
    line.tool = 7;
    const auto pieces = split_by_bands(line, 0, 0.0, 10.0, 3);
    REQUIRE(pieces.size() == 3);
    CHECK(pieces[0].first == 0);
    CHECK(pieces[1].first == 1);
    CHECK(pieces[2].first == 2);
    CHECK(pieces[0].second.points.back().p.x == 10.0);
    CHECK(pieces[2].second.points.front().p.x == 20.0);
    CHECK(pieces[1].second.tool == 7);
    // Back again: the bands in reverse.
    std::swap(line.points[0], line.points[1]);
    const auto back = split_by_bands(line, 0, 0.0, 10.0, 3);
    REQUIRE(back.size() == 3);
    CHECK(back[0].first == 2);
    CHECK(back[2].first == 0);
    // A square loop from 2 to 18 across the edge at 10, by Y... and by X.
    Toolpath loop;
    loop.closed = true;
    loop.points = {at(2, 2), at(18, 2), at(18, 18), at(2, 18)};
    const auto two = split_by_bands(loop, 0, 0.0, 10.0, 2);
    REQUIRE(two.size() == 2); // the last piece and the first, both in band 0, are one
    double length = 0.0;
    for (const auto& [band, p] : two) {
        CHECK_FALSE(p.closed);
        for (std::size_t i = 1; i < p.points.size(); ++i)
            length += std::hypot(p.points[i].p.x - p.points[i - 1].p.x, p.points[i].p.y - p.points[i - 1].p.y);
    }
    CHECK_THAT(length, WithinAbs(64.0, 1e-9)); // nothing lost
    // Inside one band, the loop stays a loop.
    const auto one = split_by_bands(loop, 1, 0.0, 30.0, 2);
    REQUIRE(one.size() == 1);
    CHECK(one.front().second.closed);
    // The cycle along the gradient: filament 1 at the start, 2 at the end, 1 : 1 in the middle.
    ColorMix g = blend(5, {{1, 0.0}, {2, 1.0}});
    g.gradient = true;
    CHECK(gradient_cycle(g, 0.0) == std::vector<int>{1});
    CHECK(gradient_cycle(g, 1.0) == std::vector<int>{2});
    CHECK(gradient_cycle(g, 0.5).size() == 2);
}

TEST_CASE("color mixing plus: a gradient in X prints each band of the layer with its blend (rule 3)", "[mixing]") {
    Settings s = base();
    s.infill_density = 20.0;
    ColorMix g = blend(3, {{1, 0.0}, {2, 1.0}});
    g.gradient = true;
    g.axis = 'x';
    // The axis travels with the blends' JSON.
    const std::vector<ColorMix> read = parse_color_mixes(color_mixes_json({g}, {"#ffffff", "#000000"}));
    REQUIRE(read.size() == 1);
    CHECK(read.front().axis == 'x');
    const SlicedModel model = sliced(box(3), s, {g});
    // A middle layer has both filaments: 1 to the left, 2 to the right.
    const LayerWalls& w = model.walls[20];
    double sum[3] = {0, 0, 0}, count[3] = {0, 0, 0};
    for (const Toolpath& t : w.toolpaths) {
        REQUIRE((t.tool == 1 || t.tool == 2));
        for (const ToolpathPoint& q : t.points)
            sum[t.tool] += q.p.x, count[t.tool] += 1;
    }
    REQUIRE(count[1] > 0);
    REQUIRE(count[2] > 0);
    CHECK(sum[1] / count[1] < sum[2] / count[2]);
    // The outer bands (12.5 % and 87.5 % of the way, 1 : 9 by 10 %) are
    // mostly one filament: 1 at the left, 2 at the right.
    double left[3] = {0, 0, 0}, right[3] = {0, 0, 0};
    for (const LayerWalls& layer : model.walls)
        for (const Toolpath& t : layer.toolpaths)
            for (const ToolpathPoint& q : t.points) {
                if (q.p.x < 91.0 && t.tool <= 2)
                    left[t.tool] += 1;
                if (q.p.x > 109.0 && t.tool <= 2)
                    right[t.tool] += 1;
            }
    CHECK(left[1] > 4.0 * left[2]);
    CHECK(right[2] > 4.0 * right[1]);
    gcode_of(model, s); // checked G-code
}

TEST_CASE("color mixing plus: the estimate before slicing for a gradient in X (rules 3 and 12)", "[mixing]") {
    Settings s = base();
    ColorMix g = blend(3, {{1, 0.0}, {2, 1.0}});
    g.gradient = true;
    g.axis = 'x';
    const int counted = changes(gcode_of(sliced(box(3), s, {g}), s));
    MixChangeOptions o;
    o.first_layer_height = s.first_layer_height;
    o.layer_height = s.layer_height;
    o.extent = 20.0; // the box along X
    CHECK(mix_changes_estimate(g, 10.0, o) == counted);
}

TEST_CASE("color mixing plus: a gradient in X takes its bands from the part's size, as its estimate (rules 3 and 12)", "[mixing]") {
    // 22.6 mm wide: 4.52 bands of 5 mm, so 5; the walls' middles span only
    // 22.15 mm (4.43: 4 bands) if the beads' half widths are left out.
    Plate p;
    add_box(p.mesh, 90, 90, 0, 112.6, 110, 10);
    p.objects = {{"box", 0, 12, 0, 0, 3}};
    Settings s = base();
    ColorMix g = blend(3, {{1, 0.0}, {2, 1.0}});
    g.gradient = true;
    g.axis = 'x';
    const int counted = changes(gcode_of(sliced(p, s, {g}), s));
    MixChangeOptions o;
    o.first_layer_height = s.first_layer_height;
    o.layer_height = s.layer_height;
    o.extent = 22.6;
    CHECK(mix_changes_estimate(g, 10.0, o) == counted);
}

TEST_CASE("color mixing plus: a gradient between two points of the part (rule 3, MX31)", "[mixing]") {
    // MX31: a 20 mm cube, from (-10, -10, 0) to (10, 10, 20) of the cube, 1 to 2.
    Plate p;
    add_box(p.mesh, 90, 90, 0, 110, 110, 20);
    p.objects = {{"cube", 0, 12, 0, 0, 3}};
    Settings s = base();
    ColorMix g = blend(3, {{1, 0.0}, {2, 1.0}});
    g.gradient = true;
    g.from = std::array<double, 3>{-10, -10, 0};
    g.to = std::array<double, 3>{10, 10, 20};
    // From and to travel with the blends' JSON.
    const std::vector<ColorMix> read = parse_color_mixes(color_mixes_json({g}, {"#ffffff", "#000000"}));
    REQUIRE(read.size() == 1);
    CHECK(read.front().from == g.from);
    CHECK(read.front().to == g.to);
    const SlicedModel model = sliced(p, s, {g});
    // The bottom front left corner mostly 1, the top back right mostly 2
    // (the corners fall in the bands of 1 : 9 and 2 : 8, by 10 %), over ten layers each.
    const std::size_t top = model.walls.size() - 1;
    const auto count_near = [&](std::size_t from, std::size_t to, double x, double y, int tool) {
        int n = 0;
        for (std::size_t l = from; l <= to; ++l)
            for (const Toolpath& t : model.walls[l].toolpaths)
                for (const ToolpathPoint& q : t.points)
                    n += std::hypot(q.p.x - x, q.p.y - y) < 2.0 && t.tool == tool;
        return n;
    };
    CHECK(count_near(0, 9, 90.5, 90.5, 1) > 3 * count_near(0, 9, 90.5, 90.5, 2));
    CHECK(count_near(top - 9, top, 109.5, 109.5, 2) > 3 * count_near(top - 9, top, 109.5, 109.5, 1));
    std::set<int> middle;
    for (const Toolpath& t : model.walls[top / 2].toolpaths)
        middle.insert(t.tool);
    CHECK(middle == std::set<int>{1, 2});
    gcode_of(model, s);
    // (0, 0, 0) to (0, 0, 20): the same as a gradient in Z over the cube.
    ColorMix up = g;
    up.from = std::array<double, 3>{0, 0, 0};
    up.to = std::array<double, 3>{0, 0, 20};
    ColorMix z = blend(3, {{1, 0.0}, {2, 1.0}});
    z.gradient = true;
    z.min_z = 0.0;
    z.max_z = 20.0;
    CHECK(sequence(sliced(p, s, {up})) == sequence(sliced(p, s, {z})));
}

TEST_CASE("color mixing plus: the estimate before slicing for a gradient between two points (MX31, rule 12)", "[mixing]") {
    Plate p;
    add_box(p.mesh, 90, 90, 0, 110, 110, 20);
    p.objects = {{"cube", 0, 12, 0, 0, 3}};
    Settings s = base();
    ColorMix g = blend(3, {{1, 0.0}, {2, 1.0}});
    g.gradient = true;
    g.from = std::array<double, 3>{-10, -10, 0};
    g.to = std::array<double, 3>{10, 10, 20};
    MixChangeOptions o;
    o.first_layer_height = s.first_layer_height;
    o.layer_height = s.layer_height;
    o.extent = 20.0 * std::sqrt(2.0); // the cube across the diagonal
    CHECK(mix_changes_estimate(g, 20.0, o) == changes(gcode_of(sliced(p, s, {g}), s)));
    // Nearly vertical: as a gradient in Z.
    g.from = std::array<double, 3>{0, 0, 0};
    g.to = std::array<double, 3>{0, 0, 20};
    CHECK(mix_changes_estimate(g, 20.0, o) == changes(gcode_of(sliced(p, s, {g}), s)));
}

TEST_CASE("color mixing plus: the blend palette piece slices, each sample with its blend (CM8, MX18)", "[mixing][calibration]") {
    // As the interface loads it: one object, a part per piece of the mesh,
    // the base with filament 1 and each sample with its blend.
    const std::vector<std::string> four = {"#00ffff", "#ff00ff", "#ffff00", "#ffffff"};
    const std::vector<PaletteSample> palette = quick_palette(four, 2, 5);
    const PalettePiece piece = palette_piece(palette.size());
    Plate p;
    const auto add = [&](const TriangleMesh& m, int extruder) {
        const std::size_t first = p.mesh.triangle_count();
        for (std::size_t t = 0; t < m.triangle_count(); ++t) {
            Vec3f c[3];
            for (int k = 0; k < 3; ++k)
                c[k] = m.corner(t, k), c[k].x += 125.0f, c[k].y += 105.0f;
            p.mesh.add_triangle(c[0], c[1], c[2]);
        }
        p.objects.push_back({"palette", first, m.triangle_count(), 0, 0, extruder});
    };
    add(piece.base, 1);
    std::vector<ColorMix> mixes;
    for (std::size_t k = 0; k < palette.size(); ++k) {
        add(piece.samples[k], palette[k].mix.id);
        mixes.push_back(palette[k].mix); // a filament's sample is a blend of one
    }
    Settings s = base(4);
    const SlicedModel model = sliced(p, s, mixes);
    const std::string gcode = gcode_of(model, s); // checked
    // Every filament prints, on the samples' layers too.
    for (const char* tool : {"T0", "T1", "T2", "T3"})
        CHECK(gcode.find(std::string("\n") + tool) != std::string::npos);
    // Each filament's own sample prints with it (the samples' layers: 1 to 3 mm).
    for (std::size_t k = 0; k < 4; ++k) {
        const BoundingBox box = piece.samples[k].bounding_box();
        std::set<int> tools;
        for (std::size_t l = 0; l < model.layers.size(); ++l)
            if (model.layers[l].bottom > 1.2 && model.layers[l].top < 2.8)
                for (const Toolpath& t : model.walls[l].toolpaths)
                    for (const ToolpathPoint& q : t.points)
                        if (q.p.x > box.min.x + 125.5 && q.p.x < box.max.x + 124.5 && q.p.y > box.min.y + 105.5 &&
                            q.p.y < box.max.y + 104.5)
                            tools.insert(t.tool);
        INFO("sample " << k + 1);
        CHECK(tools == std::set<int>{int(k) + 1});
    }
    // No toolpath is left with a blend's number.
    for (const LayerWalls& w : model.walls)
        for (const Toolpath& t : w.toolpaths)
            CHECK(t.tool <= 4);
}

TEST_CASE("color mixing plus: a gradient is measured on each object (rule 3, MX32)", "[mixing]") {
    // Two 20 mm cubes 20 mm apart with one gradient in X, 1 to 2: each one
    // goes from 1 at its left to 2 at its right.
    Plate p;
    add_box(p.mesh, 90, 90, 0, 110, 110, 10);
    add_box(p.mesh, 130, 90, 0, 150, 110, 10);
    p.objects = {{"a", 0, 12, 0, 0, 3}, {"b", 12, 12, 1, 0, 3}};
    Settings s = base();
    ColorMix g = blend(3, {{1, 0.0}, {2, 1.0}});
    g.gradient = true;
    g.axis = 'x';
    const SlicedModel model = sliced(p, s, {g});
    const auto count = [&](double x0, double x1, int tool) {
        int n = 0;
        for (const LayerWalls& w : model.walls)
            for (const Toolpath& t : w.toolpaths)
                for (const ToolpathPoint& q : t.points)
                    n += q.p.x > x0 && q.p.x < x1 && t.tool == tool;
        return n;
    };
    for (const double left : {90.0, 130.0}) {
        INFO("cube from x " << left);
        CHECK(count(left, left + 2, 1) > 3 * count(left, left + 2, 2));
        CHECK(count(left + 18, left + 20, 2) > 3 * count(left + 18, left + 20, 1));
    }
    for (const LayerWalls& w : model.walls)
        for (const Toolpath& t : w.toolpaths)
            CHECK(t.tool <= 2); // no temporary number left
    gcode_of(model, s);
}

TEST_CASE("color mixing plus: two points nearly vertical make a gradient in Z whatever its axis was (MX31)", "[mixing]") {
    // Reported by the interface: the blend's axis 'x' stayed, and the
    // estimate and the slicing took it for a gradient in X.
    Settings s = base();
    ColorMix g = blend(3, {{1, 0.0}, {2, 1.0}});
    g.gradient = true;
    g.axis = 'x';
    g.from = std::array<double, 3>{0, 0, 0};
    g.to = std::array<double, 3>{0, 0, 10};
    ColorMix z = blend(3, {{1, 0.0}, {2, 1.0}});
    z.gradient = true;
    z.min_z = 0.0;
    z.max_z = 10.0;
    MixChangeOptions o;
    o.first_layer_height = s.first_layer_height;
    o.layer_height = s.layer_height;
    o.extent = 20.0;
    CHECK(mix_changes_estimate(g, 10.0, o) == mix_changes_estimate(z, 10.0, o));
    CHECK(sequence(sliced(box(3), s, {g})) == sequence(sliced(box(3), s, {z})));
    // Two points along X at one height span the part as a gradient in X
    // does: the same estimate (the ends on the part's edges add no band).
    ColorMix along = blend(3, {{1, 0.0}, {2, 1.0}});
    along.gradient = true;
    along.from = std::array<double, 3>{-10, 0, 5};
    along.to = std::array<double, 3>{10, 0, 5};
    ColorMix x = blend(3, {{1, 0.0}, {2, 1.0}});
    x.gradient = true;
    x.axis = 'x';
    CHECK(mix_changes_estimate(along, 10.0, o) == mix_changes_estimate(x, 10.0, o));
    CHECK(mix_changes_estimate(along, 10.0, o) == changes(gcode_of(sliced(box(3), s, {along}), s)));
}

TEST_CASE("color mixing plus: an object inside another one's outline keeps its own gradient (MX32)", "[mixing]") {
    // A square frame 60 mm wide (four bars, one object) and a 10 mm cube in
    // its middle (another object), one gradient in X: the cube 1 to 2 on its own.
    Plate p;
    add_box(p.mesh, 70, 70, 0, 130, 75, 10);
    add_box(p.mesh, 70, 125, 0, 130, 130, 10);
    add_box(p.mesh, 70, 75, 0, 75, 125, 10);
    add_box(p.mesh, 125, 75, 0, 130, 125, 10);
    add_box(p.mesh, 95, 95, 0, 105, 105, 10);
    p.objects = {{"frame", 0, 48, 0, 0, 3}, {"cube", 48, 12, 1, 0, 3}};
    Settings s = base();
    ColorMix g = blend(3, {{1, 0.0}, {2, 1.0}});
    g.gradient = true;
    g.axis = 'x';
    const SlicedModel model = sliced(p, s, {g});
    const auto count = [&](double x0, double x1, int tool) {
        int n = 0;
        for (const LayerWalls& w : model.walls)
            for (const Toolpath& t : w.toolpaths)
                for (const ToolpathPoint& q : t.points)
                    n += q.p.x > x0 && q.p.x < x1 && q.p.y > 94 && q.p.y < 106 && t.tool == tool;
        return n;
    };
    // A 10 mm cube has two bands, 3 : 7 and 7 : 3 (by 10 %); with the
    // frame's gradient its middle was nearly half and half (180 and 120).
    CHECK(count(95, 96.5, 1) > 2 * count(95, 96.5, 2));
    CHECK(count(103.5, 105, 2) > 2 * count(103.5, 105, 1));
}
