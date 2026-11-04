// The Design tab's engine side (docs/spec/features/cad_design.md, stage D1):
// the design as a SolidCraft script, the bundled tool driven through its
// stand-in (tests/fake_solidcraft_cli.cpp), and the 3MF written and read.

#include "cad/constraints.hpp"
#include "cad/design.hpp"
#include "cad/parameters.hpp"
#include "cad/process.hpp"
#include "cad/sketch.hpp"
#include "cad/solidcraft.hpp"
#include "cad/sketch_io.hpp"
#include "cad/view_dxf.hpp"
#include "io/model.hpp"
#include "mesh/shapes.hpp"

#include <nlohmann/json.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <chrono>
#include <cmath>
#include <random>
#include <map>
#include <cstdlib>
#include <fstream>
#include <numbers>

using namespace tisma;
using namespace tisma::cad;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

#ifndef TISMA_FAKE_SOLIDCRAFT
#define TISMA_FAKE_SOLIDCRAFT ""
#endif
const std::filesystem::path kFake = TISMA_FAKE_SOLIDCRAFT;

void set_env(const char* name, const char* value) {
#ifdef _WIN32
    _putenv_s(name, value ? value : "");
#else
    if (value)
        setenv(name, value, 1);
    else
        unsetenv(name);
#endif
}

Feature prim(const char* name, Primitive kind, std::array<double, kMaxSizes> sizes, bool suppressed = false) {
    Feature f;
    f.name = name;
    f.kind = kind;
    f.sizes = sizes;
    f.suppressed = suppressed;
    return f;
}

Design box_design(double l = 40.0, double w = 30.0, double h = 20.0) {
    Design d;
    d.name = "Test";
    d.features.push_back(prim("Box", Primitive::Box, {l, w, h}));
    return d;
}

} // namespace

TEST_CASE("cad: a primitive is SolidCraft's command, standing on the bed", "[cad]") {
    const Design d = box_design();
    const std::vector<std::string> commands = build_commands(d);
    REQUIRE(commands.size() == 1);
    const nlohmann::json c = nlohmann::json::parse(commands[0]);
    CHECK(c.at("command") == "solid.box");
    CHECK(c.at("params").at("length") == 40.0);
    CHECK(c.at("params").at("width") == 30.0);
    CHECK(c.at("params").at("height") == 20.0);
    CHECK(c.at("params").at("corner") == nlohmann::json::array({-20.0, -15.0, 0.0}));
    CHECK(c.at("params").at("operation") == "new");
    CHECK(c.at("params").at("body_name") == "Box");
    // The script with its export at the end, at the quality asked for.
    const nlohmann::json script = nlohmann::json::parse(script_json(d, "out.3mf", MeshQuality::Fine));
    REQUIRE(script.at("commands").size() == 2);
    CHECK(script.at("commands")[1].at("command") == "file.export_3mf");
    CHECK(script.at("commands")[1].at("params").at("path") == "out.3mf");
    CHECK(script.at("commands")[1].at("params").at("quality") == "fine");
    // A tube is a cylinder with a longer cylinder cut out of it alone.
    Design tube;
    tube.features.push_back(prim("Tube", Primitive::Tube, {20.0, 12.0, 10.0}));
    const std::vector<std::string> cut = build_commands(tube);
    REQUIRE(cut.size() == 2);
    const nlohmann::json hole = nlohmann::json::parse(cut[1]);
    CHECK(hole.at("command") == "solid.cylinder");
    CHECK(hole.at("params").at("operation") == "cut");
    CHECK(hole.at("params").at("diameter") == 12.0);
    CHECK(hole.at("params").at("height") == 12.0);
    CHECK(hole.at("params").at("targets") == nlohmann::json::array({"Tube"}));
    // Every other primitive names its command.
    for (const Primitive p : {Primitive::Cylinder, Primitive::Sphere, Primitive::Cone, Primitive::Torus}) {
        Design one;
        one.features.push_back(prim("A", p, default_sizes(p)));
        const std::vector<std::string> own = build_commands(one);
        REQUIRE(own.size() == 1);
        CHECK(nlohmann::json::parse(own[0]).at("command") == command_of(p));
    }
    // A suppressed feature (the eye in Historial) and bad sizes are left out.
    Design mixed = box_design();
    mixed.features.push_back(prim("Hidden", Primitive::Sphere, {10.0, 0.0, 0.0}, true));
    mixed.features.push_back(prim("Flat", Primitive::Cylinder, {10.0, 0.0, 0.0}));
    CHECK(build_commands(mixed).size() == 1);
    CHECK(mixed.has_body());
    CHECK_FALSE(Design{}.has_body());
    CHECK(mixed.free_name("Box") == "Box 2");
    CHECK(mixed.free_name("Cone") == "Cone");
    // Sizes that make a solid.
    CHECK(sizes_ok(Primitive::Cone, {20.0, 0.0, 10.0}));
    CHECK_FALSE(sizes_ok(Primitive::Cone, {20.0, 25.0, 10.0}));
    CHECK_FALSE(sizes_ok(Primitive::Tube, {20.0, 20.0, 10.0}));
    CHECK_FALSE(sizes_ok(Primitive::Torus, {10.0, 12.0, 0.0}));
    CHECK_FALSE(sizes_ok(Primitive::Box, {10.0, -1.0, 10.0}));
}

TEST_CASE("cad: the mesh quality for a chord tolerance, and the design round trip", "[cad]") {
    CHECK(quality_for(0.01, 20.0) == MeshQuality::Standard);
    CHECK(quality_for(0.01, 100.0) == MeshQuality::Fine);
    CHECK(quality_for(0.01, 1000.0) == MeshQuality::Finest);
    CHECK(quality_for(0.0, 1000.0) == MeshQuality::Standard);
    Design d = box_design();
    d.features.push_back(prim("Ring", Primitive::Torus, {30.0, 8.0, 0.0}, true));
    CHECK_THAT(nominal_size(d), WithinAbs(40.0, 1e-9));
    const Design back = parse_design(design_json(d));
    REQUIRE(back.features.size() == 2);
    CHECK(back.name == "Test");
    CHECK(back.features[0].kind == Primitive::Box);
    CHECK(back.features[0].sizes == d.features[0].sizes);
    CHECK(back.features[1].kind == Primitive::Torus);
    CHECK(back.features[1].suppressed);
    CHECK(back.features[1].sizes[0] == 30.0);
    CHECK_THROWS_WITH(parse_design("{}"), ContainsSubstring("tisma-design-1"));
    CHECK_THROWS_WITH(parse_design(R"({"format":"tisma-design-1","features":[{"name":"x","kind":"cube","sizes":[1,1,1]}]})"),
                      ContainsSubstring("unknown feature kind"));
}

TEST_CASE("cad: the shapes of D1 and the 3MF written and read back", "[cad]") {
    const double pi = std::numbers::pi;
    CHECK_THAT(make_cone(10.0, 0.0, 30.0, 256).signed_volume(), WithinRel(pi * 100.0 * 30.0 / 3.0, 0.002));
    CHECK_THAT(make_cone(10.0, 5.0, 30.0, 256).signed_volume(), WithinRel(pi * 30.0 / 3.0 * (100.0 + 50.0 + 25.0), 0.002));
    CHECK_THAT(make_tube(10.0, 6.0, 20.0, 256).signed_volume(), WithinRel(pi * (100.0 - 36.0) * 20.0, 0.002));
    CHECK_THAT(make_torus(15.0, 4.0, 256, 128).signed_volume(), WithinRel(2.0 * pi * pi * 15.0 * 16.0, 0.002));
    const BoundingBox ring = make_torus(15.0, 4.0).bounding_box();
    CHECK_THAT(double(ring.min.z), WithinAbs(0.0, 1e-5));
    CHECK_THAT(double(ring.max.z), WithinAbs(8.0, 1e-5));
    CHECK_THAT(double(ring.max.x), WithinAbs(19.0, 1e-5));
    const TriangleMesh box = make_box(40.0, 30.0, 20.0);
    const ModelFile back = parse_model(write_3mf(box, "Caja <1>"), ".3mf");
    CHECK(back.format == ModelFormat::ThreeMf);
    CHECK(back.mesh.triangle_count() == 12);
    CHECK_THAT(back.mesh.signed_volume(), WithinAbs(24000.0, 1e-3));
    CHECK(back.objects == 1);
}

TEST_CASE("cad: the bundled tool probed and a design built through it (CD2's box)", "[cad]") {
    if (kFake.empty() || !std::filesystem::is_regular_file(kFake))
        SKIP("the stand-in solidcraft-cli was not built");
    set_env("TISMA_FAKE_SOLIDCRAFT_HANG", nullptr);
    set_env("TISMA_FAKE_SOLIDCRAFT_BROKEN", nullptr);
    const SolidCraftInfo info = probe_solidcraft(kFake);
    INFO(info.error);
    REQUIRE(info.available);
    CHECK_THAT(info.version, ContainsSubstring(kSolidCraftVersion));
    CHECK(info.has("solid.box"));
    CHECK(info.has("solid.torus"));
    CHECK(info.has("solid.fillet"));
    CHECK_FALSE(info.has("solid.loft")); // «Próximamente»
    // A 40 x 30 x 20 box: 24 000 mm3 and that box, standing on the bed.
    const BuildResult built = build_design(kFake, box_design(), MeshQuality::Standard);
    INFO(built.error);
    REQUIRE(built.ok);
    CHECK_THAT(built.volume_mm3, WithinAbs(24000.0, 1e-3));
    const BoundingBox b = built.mesh.bounding_box();
    CHECK_THAT(double(b.max.x - b.min.x), WithinAbs(40.0, 1e-4));
    CHECK_THAT(double(b.max.y - b.min.y), WithinAbs(30.0, 1e-4));
    CHECK_THAT(double(b.min.z), WithinAbs(0.0, 1e-4));
    CHECK_THAT(double(b.max.z), WithinAbs(20.0, 1e-4));
    CHECK_FALSE(built.design_file.empty());
    CHECK_THAT(built.measure, ContainsSubstring("volume_mm3"));
    // A tube, through the cut; a sphere, a cone and a torus.
    Design more;
    more.features.push_back(prim("Tube", Primitive::Tube, {20.0, 12.0, 20.0}));
    more.features.push_back(prim("Ball", Primitive::Sphere, {10.0, 0.0, 0.0}));
    more.features.push_back(prim("Cone", Primitive::Cone, {20.0, 0.0, 20.0}));
    more.features.push_back(prim("Ring", Primitive::Torus, {30.0, 8.0, 0.0}));
    const BuildResult four = build_design(kFake, more, MeshQuality::Fine);
    INFO(four.error);
    REQUIRE(four.ok);
    const double pi = std::numbers::pi;
    const double expected = pi * (100.0 - 36.0) * 20.0 + 4.0 / 3.0 * pi * 125.0 + pi * 100.0 * 20.0 / 3.0 + 2.0 * pi * pi * 15.0 * 16.0;
    CHECK_THAT(four.volume_mm3, WithinRel(expected, 0.01));
    // Nothing to build: no run, an empty mesh.
    Design empty;
    empty.features.push_back(prim("Hidden", Primitive::Box, {10.0, 10.0, 10.0}, true));
    const BuildResult none = build_design(kFake, empty, MeshQuality::Standard);
    CHECK(none.ok);
    CHECK(none.mesh.empty());
}

TEST_CASE("cad: the tool missing, damaged or not answering (CD21)", "[cad]") {
    const SolidCraftInfo none = probe_solidcraft({});
    CHECK_FALSE(none.available);
    CHECK_THAT(none.error, ContainsSubstring("not here"));
    CHECK_FALSE(build_design({}, box_design(), MeshQuality::Standard).ok);
    const SolidCraftInfo gone = probe_solidcraft(std::filesystem::path("/no/such/solidcraft-cli"));
    CHECK_FALSE(gone.available);
    CHECK_FALSE(gone.error.empty());
    if (kFake.empty() || !std::filesystem::is_regular_file(kFake))
        SKIP("the stand-in solidcraft-cli was not built");
    set_env("TISMA_FAKE_SOLIDCRAFT_BROKEN", "1");
    const SolidCraftInfo broken = probe_solidcraft(kFake);
    set_env("TISMA_FAKE_SOLIDCRAFT_BROKEN", nullptr);
    CHECK_FALSE(broken.available);
    CHECK_THAT(broken.error, ContainsSubstring("failed to start"));
    set_env("TISMA_FAKE_SOLIDCRAFT_HANG", "1");
    const ProcessResult hung = run_process(kFake, {"--version"}, 1.0);
    set_env("TISMA_FAKE_SOLIDCRAFT_HANG", nullptr);
    CHECK(hung.started);
    CHECK(hung.timed_out);
    CHECK_FALSE(hung.ok());
    // The tool refuses a script: its complaint reaches the person.
    const ProcessResult bad = run_process(kFake, {"run"}, 10.0);
    CHECK(bad.started);
    CHECK(bad.status == 2);
    CHECK_THAT(bad.err, ContainsSubstring("usage"));
}

TEST_CASE("cad: points typed as the drafting programs take them (rule 13, CD13, CD14)", "[cad]") {
    const Pt last{10.0, 5.0};
    REQUIRE(parse_typed_point("0,0", last));
    CHECK(*parse_typed_point("0,0", last) == Pt{0.0, 0.0});
    CHECK(*parse_typed_point("40.5, 30", last) == Pt{40.5, 30.0});
    CHECK(*parse_typed_point("@40,0", last) == Pt{50.0, 5.0});
    CHECK(*parse_typed_point("@-40,0", last) == Pt{-30.0, 5.0});
    // CD14: @50<30 from the origin ends at (43.301, 25).
    const Pt polar = *parse_typed_point("@50<30", Pt{});
    CHECK_THAT(polar.x, WithinAbs(43.301, 1e-3));
    CHECK_THAT(polar.y, WithinAbs(25.0, 1e-9));
    CHECK_FALSE(parse_typed_point("40", last));
    CHECK_FALSE(parse_typed_point("50<30", last)); // polar is always relative
    CHECK_FALSE(parse_typed_point("a,b", last));
    CHECK(parse_typed_value("12,5") == 12.5);
    CHECK(parse_typed_value(" 7 ") == 7.0);
    CHECK_FALSE(parse_typed_value("x"));
}

TEST_CASE("cad: ortho and polar tracking (rule 16, CD16)", "[cad]") {
    const Pt from{0.0, 0.0};
    // F8: a segment at 10 degrees from the horizontal comes out horizontal.
    const Pt c{30.0 * std::cos(10.0 * std::numbers::pi / 180.0), 30.0 * std::sin(10.0 * std::numbers::pi / 180.0)};
    const Pt ortho = tracked_point(from, c, true, 0.0);
    CHECK_THAT(ortho.y, WithinAbs(0.0, 1e-12));
    CHECK_THAT(ortho.x, WithinAbs(c.x, 1e-9));
    CHECK(tracked_point(from, {2.0, 30.0}, true, 0.0) == Pt{0.0, 30.0});
    // F10 at 45 degrees: 43 degrees snaps to 45, the length kept; 20 does not.
    const Pt near45{30.0 * std::cos(43.0 * std::numbers::pi / 180.0), 30.0 * std::sin(43.0 * std::numbers::pi / 180.0)};
    const Pt polar = tracked_point(from, near45, false, 45.0);
    CHECK_THAT(polar.x, WithinAbs(polar.y, 1e-9));
    CHECK_THAT(std::hypot(polar.x, polar.y), WithinAbs(30.0, 1e-9));
    const Pt off{30.0 * std::cos(20.0 * std::numbers::pi / 180.0), 30.0 * std::sin(20.0 * std::numbers::pi / 180.0)};
    CHECK(tracked_point(from, off, false, 45.0) == off);
}

TEST_CASE("cad: the object snaps (rule 15, CD15)", "[cad]") {
    Sketch s;
    s.entities.push_back({Entity::Kind::Chain, {{0.0, 0.0}, {40.0, 0.0}}}); // l1
    s.entities.push_back({Entity::Kind::Circle, {{20.0, 20.0}}, 5.0});       // c1
    SnapOptions o;
    // Endpoint before midpoint before nearest; the midpoint of l1 is (20, 0).
    Snap snap = snap_point(s, {20.5, 0.3}, o);
    CHECK(snap.kind == SnapKind::Midpoint);
    CHECK(snap.at == Pt{20.0, 0.0});
    CHECK(snap.entity == 0);
    snap = snap_point(s, {39.5, 0.5}, o);
    CHECK(snap.kind == SnapKind::Endpoint);
    CHECK(snap.at == Pt{40.0, 0.0});
    snap = snap_point(s, {20.5, 19.5}, o);
    CHECK(snap.kind == SnapKind::Center);
    snap = snap_point(s, {25.3, 20.2}, o);
    CHECK(snap.kind == SnapKind::Quadrant);
    CHECK(snap.at == Pt{25.0, 20.0});
    snap = snap_point(s, {10.0, 0.6}, o);
    CHECK(snap.kind == SnapKind::Nearest);
    CHECK_THAT(snap.at.y, WithinAbs(0.0, 1e-12));
    CHECK(snap_point(s, {10.0, 10.0}, o).kind == SnapKind::None);
    // Perpendicular and tangent from a start point.
    o.from = Pt{10.0, 8.0};
    snap = snap_point(s, {10.2, 0.4}, o);
    CHECK(snap.kind == SnapKind::Perpendicular);
    CHECK(snap.at == Pt{10.0, 0.0});
    o.from = Pt{20.0, 0.0};
    const double t = std::acos(5.0 / 20.0);
    const Pt tangent{20.0 + 5.0 * std::cos(-std::numbers::pi / 2.0 + t), 20.0 + 5.0 * std::sin(-std::numbers::pi / 2.0 + t)};
    o.radius = 1.0; // the quadrant (25, 20) lies 1.3 mm from the tangent point, and would win
    snap = snap_point(s, {tangent.x + 0.3, tangent.y - 0.2}, o);
    CHECK(snap.kind == SnapKind::Tangent);
    o.radius = 2.0;
    CHECK_THAT(std::hypot(snap.at.x - 20.0, snap.at.y - 20.0), WithinAbs(5.0, 1e-9));
    // An intersection of two lines wins over the nearest.
    s.entities.push_back({Entity::Kind::Chain, {{10.0, -5.0}, {10.0, 15.0}}}); // l2 (its midpoint away from the crossing)
    o.from.reset();
    snap = snap_point(s, {10.4, 0.4}, o);
    CHECK(snap.kind == SnapKind::Intersection);
    CHECK_THAT(snap.at.x, WithinAbs(10.0, 1e-9));
    CHECK_THAT(snap.at.y, WithinAbs(0.0, 1e-9));
    // F3 off: only the grid, when it is on.
    o.enabled = false;
    CHECK(snap_point(s, {20.5, 0.3}, o).kind == SnapKind::None);
    o.grid = true;
    snap = snap_point(s, {20.6, 0.3}, o);
    CHECK(snap.kind == SnapKind::Grid);
    CHECK(snap.at == Pt{21.0, 0.0});
    // CD15: a line from the midpoint of another leaves the midpoint constraint, by SolidCraft's ids.
    Sketch two;
    two.entities.push_back({Entity::Kind::Chain, {{0.0, 0.0}, {40.0, 0.0}}});
    two.entities.push_back({Entity::Kind::Chain, {{20.0, 0.0}, {20.0, 30.0}}});
    two.constraints.push_back({Constraint::Kind::Midpoint, {1, 0}, 0, 0});
    const std::vector<std::string> commands = sketch_commands(two, {}, "Part");
    bool found = false;
    for (const std::string& c : commands) {
        const nlohmann::json j = nlohmann::json::parse(c);
        if (j.at("command") == "sketch.constraint.midpoint") {
            found = true;
            CHECK(j.at("params").at("point") == "l2.start");
            CHECK(j.at("params").at("line") == "l1");
        }
    }
    CHECK(found);
}

TEST_CASE("cad: a sketch's script, its ids and its round trip (rules 4, 7; CD13)", "[cad]") {
    Sketch s;
    // CD13: L, 0,0, @40,0, @0,30, @-40,0, C: a closed 40 x 30 rectangle with its dimensions.
    Entity chain{Entity::Kind::Chain, {{0.0, 0.0}, {40.0, 0.0}, {40.0, 30.0}, {0.0, 30.0}}, 0.0, true};
    s.entities.push_back(chain);
    s.dimensions = {{0, 0, 40.0}, {0, 1, 30.0}, {0, 2, 40.0}};
    s.entities.push_back({Entity::Kind::Circle, {{20.0, 15.0}}, 5.0});
    s.entities.push_back({Entity::Kind::Rectangle, {{50.0, 0.0}, {60.0, 10.0}}});
    s.entities.push_back({Entity::Kind::Arc, {{70.0, 0.0}, {75.0, 0.0}, {70.0, 5.0}}});
    CHECK(s.has_region());
    CHECK(point_id(s, {0, 0}) == "l1.start");
    CHECK(point_id(s, {0, 3}) == "l4.start");
    CHECK(line_id(s, 0, 3) == "l4");
    CHECK(point_id(s, {1, 0}) == "c1.center");
    CHECK(point_id(s, {2, 1}) == "l5.end"); // the rectangle's bottom-right corner: the fifth line's end
    CHECK(line_id(s, 2, 2) == "l7");
    CHECK(point_id(s, {3, 2}) == "a1.end");
    SketchOperation op;
    op.distance = 20.0;
    const std::vector<std::string> commands = sketch_commands(s, op, "Plate");
    REQUIRE(commands.size() >= 9);
    nlohmann::json first = nlohmann::json::parse(commands.front());
    CHECK(first.at("command") == "sketch.create");
    CHECK(first.at("params").at("plane") == "XY");
    nlohmann::json line = nlohmann::json::parse(commands[1]);
    CHECK(line.at("command") == "sketch.line");
    CHECK(line.at("params").at("points").size() == 4);
    CHECK(line.at("params").at("closed") == true);
    nlohmann::json dim = nlohmann::json::parse(commands[5]);
    CHECK(dim.at("command") == "sketch.dimension");
    CHECK(dim.at("params").at("entities") == nlohmann::json::array({"l1"}));
    CHECK(dim.at("params").at("value") == 40.0);
    CHECK(nlohmann::json::parse(commands[commands.size() - 2]).at("command") == "sketch.finish");
    nlohmann::json last = nlohmann::json::parse(commands.back());
    CHECK(last.at("command") == "solid.extrude");
    CHECK(last.at("params").at("distance") == 20.0);
    CHECK(last.at("params").at("body_name") == "Plate");
    op.kind = SketchOperation::Kind::Revolve;
    op.axis = 'x';
    op.angle = 180.0;
    last = nlohmann::json::parse(sketch_commands(s, op, "Knob").back());
    CHECK(last.at("command") == "solid.revolve");
    CHECK(last.at("params").at("axis") == "x");
    CHECK(last.at("params").at("angle") == 180.0);
    // The round trip, inside a design too.
    Sketch back;
    SketchOperation back_op;
    parse_sketch(sketch_json(s, op), back, back_op);
    CHECK(back == s);
    CHECK(back_op == op);
    Design d;
    Feature f;
    f.name = "Plate";
    f.type = Feature::Type::Sketch;
    f.sketch = s;
    f.operation = op;
    d.features.push_back(f);
    const Design parsed = parse_design(design_json(d));
    REQUIRE(parsed.features.size() == 1);
    CHECK(parsed.features[0].type == Feature::Type::Sketch);
    CHECK(parsed.features[0].sketch == s);
    CHECK(parsed.features[0].operation == op);
    CHECK(d.has_body());
    CHECK_THAT(nominal_size(d), WithinAbs(75.0, 1e-9));
    // A sketch with nothing closed makes no body.
    Design open;
    Feature line_only;
    line_only.type = Feature::Type::Sketch;
    line_only.sketch.entities.push_back({Entity::Kind::Chain, {{0.0, 0.0}, {10.0, 0.0}}});
    open.features.push_back(line_only);
    CHECK_FALSE(open.has_body());
}

TEST_CASE("cad: a rectangle extruded and a profile revolved through the tool (CD2)", "[cad]") {
    if (kFake.empty() || !std::filesystem::is_regular_file(kFake))
        SKIP("the stand-in solidcraft-cli was not built");
    set_env("TISMA_FAKE_SOLIDCRAFT_HANG", nullptr);
    set_env("TISMA_FAKE_SOLIDCRAFT_BROKEN", nullptr);
    // CD2: a 40 x 30 rectangle on XY extruded 20 mm: 24 000 mm3, box 40 x 30 x 20.
    Design d;
    Feature f;
    f.name = "Plate";
    f.type = Feature::Type::Sketch;
    f.sketch.entities.push_back({Entity::Kind::Rectangle, {{0.0, 0.0}, {40.0, 30.0}}});
    f.operation.distance = 20.0;
    d.features.push_back(f);
    BuildResult built = build_design(kFake, d, MeshQuality::Standard);
    INFO(built.error);
    REQUIRE(built.ok);
    CHECK_THAT(built.volume_mm3, WithinAbs(24000.0, 1e-3));
    BoundingBox b = built.mesh.bounding_box();
    CHECK_THAT(double(b.max.x - b.min.x), WithinAbs(40.0, 1e-4));
    CHECK_THAT(double(b.max.y - b.min.y), WithinAbs(30.0, 1e-4));
    CHECK_THAT(double(b.max.z - b.min.z), WithinAbs(20.0, 1e-4));
    // The same outline as CD13's closed chain, on XZ: the box stands along -Z... the tool lays XZ as +X, -Z.
    d.features[0].sketch = {};
    d.features[0].sketch.plane = SketchPlane::XZ;
    d.features[0].sketch.entities.push_back({Entity::Kind::Chain, {{0.0, 0.0}, {40.0, 0.0}, {40.0, 30.0}, {0.0, 30.0}}, 0.0, true});
    built = build_design(kFake, d, MeshQuality::Standard);
    REQUIRE(built.ok);
    CHECK_THAT(built.volume_mm3, WithinAbs(24000.0, 1e-3));
    b = built.mesh.bounding_box();
    CHECK_THAT(double(b.max.y - b.min.y), WithinAbs(20.0, 1e-4));
    CHECK_THAT(double(b.max.z - b.min.z), WithinAbs(30.0, 1e-4));
    // A 10 x 20 rectangle 5 mm off the y axis revolved about it: a tube of
    // radii 5 and 15, 20 tall.
    d.features[0].sketch = {};
    d.features[0].sketch.entities.push_back({Entity::Kind::Rectangle, {{5.0, 0.0}, {15.0, 20.0}}});
    d.features[0].operation.kind = SketchOperation::Kind::Revolve;
    d.features[0].operation.axis = 'y';
    built = build_design(kFake, d, MeshQuality::Standard);
    REQUIRE(built.ok);
    CHECK_THAT(built.volume_mm3, WithinRel(std::numbers::pi * (225.0 - 25.0) * 20.0, 0.01));
    // A circle revolved about the x axis: a torus.
    d.features[0].sketch = {};
    d.features[0].sketch.entities.push_back({Entity::Kind::Circle, {{0.0, 15.0}}, 4.0});
    d.features[0].operation.axis = 'x';
    built = build_design(kFake, d, MeshQuality::Standard);
    REQUIRE(built.ok);
    CHECK_THAT(built.volume_mm3, WithinRel(2.0 * std::numbers::pi * std::numbers::pi * 15.0 * 16.0, 0.01));
}


TEST_CASE("cad: parameters with units, their order and their errors (rule 9)", "[cad]") {
    std::map<std::string, Quantity> v{{"ancho", {40.0, Measure::Length}}, {"angulo", {60.0, Measure::Angle}}};
    const auto value = [&](const std::string& e) {
        const Evaluation r = evaluate(e, v);
        INFO(e << ": " << r.error);
        REQUIRE(r.value);
        return *r.value;
    };
    CHECK(value("2 * ancho + 5 mm").value == 85.0);
    CHECK(value("2 * ancho + 5 mm").measure == Measure::Length);
    CHECK(value("angulo / 2").value == 30.0);
    CHECK(value("angulo / 2").measure == Measure::Angle);
    CHECK(value("2,5 cm").value == 25.0);
    CHECK(value("1 in").value == 25.4);
    CHECK_THAT(value("1 rad").value, WithinRel(57.29578, 1e-6));
    CHECK(value("-(ancho - 10) * 2").value == -60.0);
    CHECK(value("ancho / 4 mm").measure == Measure::None); // a length over a length
    CHECK(value("30°").measure == Measure::Angle);
    // What is not a value says why.
    for (const char* bad : {"ancho + angulo", "ancho * ancho", "2 *", "(1 + 2", "largo + 1", "1 / 0", "5 mm mm"})
        CHECK_FALSE(evaluate(bad, v).value);
    // Parameters in any order, each using others; a cycle and a missing name fail.
    const ParameterValues p = evaluate_parameters({{"total", "2 * ancho + borde"},
                                                   {"ancho", "40 mm"},
                                                   {"borde", "5"},
                                                   {"a", "b + 1"},
                                                   {"b", "a + 1"},
                                                   {"c", "nada * 2"},
                                                   {"mm", "1"}});
    CHECK(p.values.at("total").value == 85.0);
    CHECK(p.values.at("total").measure == Measure::Length);
    CHECK(p.errors.count("a"));
    CHECK(p.errors.count("b"));
    CHECK(p.errors.count("c"));
    CHECK(p.errors.count("mm")); // a unit is not a name
    CHECK(valid_parameter_name("ancho_2"));
    CHECK_FALSE(valid_parameter_name("2ancho"));
}

TEST_CASE("cad: CD4, a parameter changes the part without redoing it by hand", "[cad]") {
    // CD2's rectangle, its width tied to «ancho», its extrusion to «alto».
    Design d;
    d.parameters = {{"ancho", "40 mm"}, {"alto", "ancho / 2"}};
    Feature f;
    f.name = "Plate";
    f.type = Feature::Type::Sketch;
    f.sketch.entities.push_back({Entity::Kind::Rectangle, {{0.0, 0.0}, {40.0, 30.0}}});
    f.bindings = {{"e0.width", "ancho"}, {"distance", "alto"}};
    d.features.push_back(f);
    CHECK(apply_parameters(d).empty());
    CHECK(d.features[0].operation.distance == 20.0);
    d.parameters[0].expression = "50 mm";
    CHECK(apply_parameters(d).empty());
    CHECK(d.features[0].sketch.entities[0].points[1].x == 50.0);
    CHECK(d.features[0].operation.distance == 25.0);
    // A primitive's size, and a wrong measure or target said, the size kept.
    Feature box;
    box.name = "Box";
    box.bindings = {{"size0", "ancho"}, {"size1", "30 deg"}, {"size7", "1"}};
    d.features.push_back(box);
    const std::vector<std::string> problems = apply_parameters(d);
    CHECK(d.features[1].sizes[0] == 50.0);
    CHECK(d.features[1].sizes[1] == 20.0);
    REQUIRE(problems.size() == 2);
    // Kept with the design.
    const Design back = parse_design(design_json(d));
    CHECK(back.parameters == d.parameters);
    CHECK(back.features[0].bindings == d.features[0].bindings);
    CHECK(back.features[1].bindings == d.features[1].bindings);
    // Through the tool: 40 wide gives 24 000 mm3 (20 high), 50 wide 50 x 30 x 25.
    if (kFake.empty() || !std::filesystem::is_regular_file(kFake))
        SKIP("the stand-in solidcraft-cli was not built");
    set_env("TISMA_FAKE_SOLIDCRAFT_HANG", nullptr);
    set_env("TISMA_FAKE_SOLIDCRAFT_BROKEN", nullptr);
    d.features.pop_back();
    d.parameters[0].expression = "40 mm";
    BuildResult built = build_design(kFake, d, MeshQuality::Standard);
    REQUIRE(built.ok);
    CHECK_THAT(built.volume_mm3, WithinAbs(24000.0, 1e-3));
    d.parameters[0].expression = "50 mm";
    built = build_design(kFake, d, MeshQuality::Standard);
    REQUIRE(built.ok);
    CHECK_THAT(built.volume_mm3, WithinAbs(50.0 * 30.0 * 25.0, 1e-3));
}

TEST_CASE("cad: CD3, a rectangle's degrees of freedom, its dimensions and a fixed corner (rule 5)", "[cad]") {
    using K = GeoConstraint::Kind;
    Sketch s;
    Entity rect;
    rect.kind = Entity::Kind::Rectangle;
    rect.points = {{0.0, 0.0}, {40.0, 30.0}};
    s.entities.push_back(rect);
    SketchSolve a = analyze_sketch(s);
    CHECK(a.solved);
    CHECK(a.dof == 4); // x, y, width and height
    CHECK_FALSE(a.entity_fixed[0]);

    // The width and the height: the rectangle can still slide.
    CHECK(add_rule(s, {K::Length, {0, 0}, {}, 40.0, {}}) == AddRule::Added);
    CHECK(add_rule(s, {K::Length, {0, 1}, {}, 30.0, {}}) == AddRule::Added);
    a = analyze_sketch(s);
    CHECK(a.dof == 2);
    CHECK_FALSE(a.entity_fixed[0]);

    // Fixed at a corner: nothing left, the sketch is fully fixed.
    CHECK(add_rule(s, {K::Fixed, {0, 0}, {}, 0.0, {0.0, 0.0}}) == AddRule::Added);
    a = analyze_sketch(s);
    CHECK(a.dof == 0);
    CHECK(a.entity_fixed[0]);

    // A side already horizontal, or the width again: redundant; another
    // width: contradicts. None is added and the sketch stays.
    const Sketch before = s;
    CHECK(add_rule(s, {K::Horizontal, {0, 0}, {}, 0.0, {}}) == AddRule::Redundant);
    CHECK(add_rule(s, {K::Length, {0, 2}, {}, 40.0, {}}) == AddRule::Redundant);
    CHECK(add_rule(s, {K::Length, {0, 0}, {}, 50.0, {}}) == AddRule::Conflicting);
    CHECK(add_rule(s, {K::Length, {3, 0}, {}, 50.0, {}}) == AddRule::Invalid);
    CHECK(add_rule(s, {K::Length, {0, 0}, {}, -5.0, {}}) == AddRule::Invalid);
    CHECK(s == before);

    // A width changed by hand: solving moves the far side.
    s.rules[0].value = 50.0;
    a = solve_sketch(s);
    CHECK(a.solved);
    CHECK_THAT(s.entities[0].points[1].x - s.entities[0].points[0].x, Catch::Matchers::WithinAbs(50.0, 1e-6));
    CHECK_THAT(s.entities[0].points[0].x, Catch::Matchers::WithinAbs(0.0, 1e-6));

    // The width set from its field or a parameter: the dimension follows, so
    // the two never disagree, and the fixed corner stays.
    CHECK(set_entity_size(s, 0, "width", 60.0));
    CHECK(s.rules[0].value == 60.0);
    a = analyze_sketch(s);
    CHECK(a.solved);
    CHECK(a.dof == 0);
    CHECK_THAT(s.entities[0].points[1].x, Catch::Matchers::WithinAbs(60.0, 1e-6));
    CHECK_THAT(s.entities[0].points[0].x, Catch::Matchers::WithinAbs(0.0, 1e-6));
    CHECK_FALSE(set_entity_size(s, 0, "diameter", 5.0));
}

TEST_CASE("cad: constraints move a chain and a circle as little as they can (rule 5)", "[cad]") {
    using K = GeoConstraint::Kind;
    Sketch s;
    Entity chain;
    chain.kind = Entity::Kind::Chain;
    chain.points = {{0.0, 0.0}, {20.0, 1.0}, {21.0, 15.0}};
    s.entities.push_back(chain);
    CHECK(analyze_sketch(s).dof == 6);
    CHECK(add_rule(s, {K::Horizontal, {0, 0}, {}, 0.0, {}}) == AddRule::Added);
    CHECK(add_rule(s, {K::Perpendicular, {0, 0}, {0, 1}, 0.0, {}}) == AddRule::Added);
    const std::vector<Pt>* points = &s.entities[0].points;
    const auto& p = *points;
    CHECK_THAT(p[1].y - p[0].y, Catch::Matchers::WithinAbs(0.0, 1e-6));
    CHECK_THAT(p[2].x - p[1].x, Catch::Matchers::WithinAbs(0.0, 1e-6));
    // Little moved: the points stay within a mm of where they were drawn.
    CHECK(std::abs(p[1].x - 20.0) < 1.0);
    CHECK(std::abs(p[2].y - 15.0) < 1.0);
    CHECK(analyze_sketch(s).dof == 4);
    // Vertical on the second segment says nothing new.
    CHECK(add_rule(s, {K::Vertical, {0, 1}, {}, 0.0, {}}) == AddRule::Redundant);
    CHECK(add_rule(s, {K::Equal, {0, 0}, {0, 1}, 0.0, {}}) == AddRule::Added);
    const auto& q = s.entities[0].points; // the sketch was replaced
    CHECK_THAT(std::hypot(q[1].x - q[0].x, q[1].y - q[0].y),
               Catch::Matchers::WithinAbs(std::hypot(q[2].x - q[1].x, q[2].y - q[1].y), 1e-6));
    CHECK(add_rule(s, {K::Angle, {0, 0}, {0, 1}, 90.0, {}}) == AddRule::Redundant);

    // A circle: its diameter, then concentric with an arc.
    Entity circle;
    circle.kind = Entity::Kind::Circle;
    circle.points = {{50.0, 50.0}};
    circle.radius = 5.0;
    s.entities.push_back(circle);
    Entity arc;
    arc.kind = Entity::Kind::Arc;
    arc.points = {{52.0, 49.0}, {60.0, 49.0}, {52.0, 57.0}};
    s.entities.push_back(arc);
    CHECK(analyze_sketch(s).dof == 3 + 3 + 5); // the chain keeps 3 after equal
    CHECK(add_rule(s, {K::Diameter, {1, 0}, {}, 12.0, {}}) == AddRule::Added);
    CHECK_THAT(s.entities[1].radius, Catch::Matchers::WithinAbs(6.0, 1e-6));
    CHECK(add_rule(s, {K::Concentric, {1, 0}, {2, 0}, 0.0, {}}) == AddRule::Added);
    CHECK_THAT(s.entities[1].points[0].x, Catch::Matchers::WithinAbs(s.entities[2].points[0].x, 1e-6));
    CHECK(add_rule(s, {K::Radius, {2, 0}, {}, 10.0, {}}) == AddRule::Added);
    const Pt c = s.entities[2].points[0];
    CHECK_THAT(std::hypot(s.entities[2].points[2].x - c.x, s.entities[2].points[2].y - c.y),
               Catch::Matchers::WithinAbs(10.0, 1e-6));
    CHECK(add_rule(s, {K::Equal, {1, 0}, {2, 0}, 0.0, {}}) == AddRule::Conflicting);
    CHECK(analyze_sketch(s).dof == 3 + 4); // circle and arc: 8, less diameter, concentric (2) and radius

    // The rules are kept with the sketch.
    Sketch back;
    SketchOperation op;
    parse_sketch(sketch_json(s, {}), back, op);
    CHECK(back == s);
}

TEST_CASE("cad: the timeline marks the first feature that fails and keeps the ones before (rule 8)", "[cad]") {
    if (kFake.empty() || !std::filesystem::is_regular_file(kFake))
        SKIP("the stand-in solidcraft-cli was not built");
    set_env("TISMA_FAKE_SOLIDCRAFT_HANG", nullptr);
    set_env("TISMA_FAKE_SOLIDCRAFT_BROKEN", nullptr);
    // Four boxes of 10, 20, 30 and 40 mm on a side; the tool refuses the third.
    Design d;
    for (int k = 1; k <= 4; ++k) {
        Feature f;
        f.name = "Caja " + std::to_string(k);
        f.kind = Primitive::Box;
        f.sizes = {10.0 * k, 10.0 * k, 10.0 * k};
        d.features.push_back(f);
    }
    set_env("TISMA_FAKE_SOLIDCRAFT_REFUSE", "Caja 3");
    BuildResult built = build_timeline(kFake, d, MeshQuality::Standard);
    INFO(built.error);
    REQUIRE(built.ok);
    REQUIRE(built.failed_feature);
    CHECK(*built.failed_feature == 2);
    CHECK_THAT(built.failure, ContainsSubstring("Caja 3"));
    CHECK_THAT(built.volume_mm3, WithinRel(1000.0 + 8000.0, 1e-6)); // the two before it; the fourth waits
    // Suppressed, it no longer fails: the fourth is built again.
    d.features[2].suppressed = true;
    built = build_timeline(kFake, d, MeshQuality::Standard);
    REQUIRE(built.ok);
    CHECK_FALSE(built.failed_feature);
    CHECK_THAT(built.volume_mm3, WithinRel(1000.0 + 8000.0 + 64000.0, 1e-6));
    // The first one refused: nothing before it.
    d.features[2].suppressed = false;
    set_env("TISMA_FAKE_SOLIDCRAFT_REFUSE", "Caja 1");
    built = build_timeline(kFake, d, MeshQuality::Standard);
    REQUIRE(built.ok);
    REQUIRE(built.failed_feature);
    CHECK(*built.failed_feature == 0);
    CHECK(built.mesh.empty());
    set_env("TISMA_FAKE_SOLIDCRAFT_REFUSE", nullptr);
    built = build_timeline(kFake, d, MeshQuality::Standard);
    CHECK(built.ok);
    CHECK_FALSE(built.failed_feature);
}

TEST_CASE("cad: random sketches and constraints never hang, and what is added holds (maintenance)", "[cad]") {
    using K = GeoConstraint::Kind;
    std::mt19937 rng(20261103);
    std::uniform_real_distribution<double> coord(-50.0, 50.0), size(1.0, 30.0);
    std::uniform_int_distribution<int> pick(0, 1000);
    int added = 0, refused = 0;
    const auto start = std::chrono::steady_clock::now();
    for (int round = 0; round < 60; ++round) {
        Sketch s;
        const int entities = 1 + pick(rng) % 4;
        for (int i = 0; i < entities; ++i) {
            Entity e;
            switch (pick(rng) % 4) {
            case 0:
                e.kind = Entity::Kind::Chain;
                for (int k = 0, n = 2 + pick(rng) % 4; k < n; ++k)
                    e.points.push_back({coord(rng), coord(rng)});
                e.closed = e.points.size() > 2 && pick(rng) % 2;
                break;
            case 1: {
                e.kind = Entity::Kind::Rectangle;
                const Pt a{coord(rng), coord(rng)};
                e.points = {a, {a.x + size(rng), a.y + size(rng)}};
                break;
            }
            case 2:
                e.kind = Entity::Kind::Circle;
                e.points = {{coord(rng), coord(rng)}};
                e.radius = size(rng);
                break;
            default: {
                e.kind = Entity::Kind::Arc;
                const Pt c{coord(rng), coord(rng)};
                const double r = size(rng);
                e.points = {c, {c.x + r, c.y}, {c.x, c.y + r}};
                break;
            }
            }
            s.entities.push_back(e);
        }
        for (int t = 0; t < 8; ++t) {
            GeoConstraint r;
            r.kind = K(pick(rng) % 16);
            r.a = {std::size_t(pick(rng) % entities), std::size_t(pick(rng) % 5)};
            r.b = {std::size_t(pick(rng) % entities), std::size_t(pick(rng) % 5)};
            r.value = r.kind == K::Angle ? double(pick(rng) % 180) : size(rng);
            r.at = {coord(rng), coord(rng)};
            const AddRule result = add_rule(s, r);
            if (result == AddRule::Added) {
                ++added;
                const SketchSolve now = analyze_sketch(s);
                INFO("round " << round << " rule " << t << " kind " << geo_kind_name(r.kind));
                CHECK(now.solved);
                CHECK(now.dof >= 0);
                CHECK(now.dof <= now.unknowns);
            } else {
                ++refused;
            }
        }
    }
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    INFO(added << " added, " << refused << " refused, " << seconds << " s");
    CHECK(added > 50);
    CHECK(seconds < 60.0);
}

TEST_CASE("cad: any text as an expression gives a value or an error, never a hang (maintenance)", "[cad]") {
    std::mt19937 rng(20261103);
    const std::string alphabet = "0123456789.+-*/()  mmcinrad°degpabc_,e";
    std::uniform_int_distribution<std::size_t> letter(0, alphabet.size() - 1), length(0, 40);
    const std::map<std::string, Quantity> known{{"a", {2.0, Measure::Length}}, {"b", {30.0, Measure::Angle}}};
    int values = 0;
    for (int i = 0; i < 20000; ++i) {
        std::string text;
        for (std::size_t k = 0, n = length(rng); k < n; ++k)
            text += alphabet[letter(rng)];
        const Evaluation e = evaluate(text, known);
        CHECK((e.value.has_value() != !e.error.empty()));
        if (e.value) {
            ++values;
            CHECK(std::isfinite(e.value->value));
        }
    }
    // Deep nesting is refused or read, not a crash.
    // Deep nesting is refused, not a crash (it ran the stack out under the
    // address sanitizer).
    const Evaluation deep = evaluate(std::string(5000, '(') + "1" + std::string(5000, ')'), known);
    CHECK_FALSE(deep.value);
    CHECK_FALSE(deep.error.empty());
    CHECK_FALSE(evaluate(std::string(5000, '-') + "1", known).value);
    const Evaluation fine = evaluate(std::string(50, '(') + "1 mm" + std::string(50, ')'), known);
    REQUIRE(fine.value);
    CHECK(fine.value->value == 1.0);
    INFO(values);
    CHECK(values > 100);
}

namespace {
Feature operation(const std::string& name, OperationKind kind, const std::string& target) {
    Feature f;
    f.name = name;
    f.type = Feature::Type::Operation;
    f.op.kind = kind;
    f.op.target = target;
    return f;
}
Feature box_feature(const std::string& name, double side) {
    Feature f;
    f.name = name;
    f.kind = Primitive::Box;
    f.sizes = {side, side, side};
    return f;
}
} // namespace

TEST_CASE("cad: D4 operations as SolidCraft commands, kept with the design (rule 7)", "[cad]") {
    Design d;
    d.features.push_back(box_feature("Caja", 20.0));
    Feature fillet = operation("Redondeo", OperationKind::Fillet, "Caja");
    fillet.op.refs = {{10.0, 10.0, 5.0}};
    fillet.op.value = 3.0;
    d.features.push_back(fillet);
    Feature hole = operation("Agujero", OperationKind::Hole, "Caja");
    hole.op.refs = {{0.0, 0.0, 20.0}, {5.0, 5.0, 20.0}};
    hole.op.standard = "M5";
    d.features.push_back(hole);
    const std::vector<std::string> cmds = build_commands(d);
    REQUIRE(cmds.size() == 4); // the box, the fillet, a hole at each point
    const nlohmann::json f = nlohmann::json::parse(cmds[1]);
    CHECK(f["command"] == "solid.fillet");
    CHECK(f["params"]["radius"] == 3.0);
    CHECK(f["params"]["body"] == "Caja");
    CHECK(f["params"]["edges"][0] == nlohmann::json::array({10.0, 10.0, 5.0}));
    const nlohmann::json h = nlohmann::json::parse(cmds[3]);
    CHECK(h["command"] == "solid.hole");
    CHECK(h["params"]["clearance"] == "M5");
    CHECK(h["params"]["fit"] == "normal");
    CHECK(h["params"]["targets"][0] == "Caja");
    CHECK_FALSE(h["params"].contains("depth")); // through all
    // Not buildable without what it needs: no edges, no tools, one copy.
    Feature bare = operation("Chaflán", OperationKind::Chamfer, "Caja");
    CHECK_FALSE(bare.buildable());
    Feature combine = operation("Combinar", OperationKind::Combine, "Caja");
    CHECK_FALSE(combine.buildable());
    Feature pattern = operation("Patrón", OperationKind::PatternCircular, "Caja");
    pattern.op.count = 1;
    CHECK_FALSE(pattern.buildable());
    // An operation alone makes no body.
    Design only;
    only.features.push_back(fillet);
    CHECK_FALSE(only.has_body());
    // Its size tied to a parameter.
    d.parameters = {{"r", "4 mm"}};
    d.features[1].bindings = {{"value", "r"}};
    CHECK(target_measure(d.features[1], "value") == Measure::Length);
    CHECK(apply_parameters(d).empty());
    CHECK(d.features[1].op.value == 4.0);
    // Kept and read back the same.
    const Design back = parse_design(design_json(d));
    REQUIRE(back.features.size() == 3);
    CHECK(back.features[1].type == Feature::Type::Operation);
    CHECK(back.features[1].op == d.features[1].op);
    CHECK(back.features[2].op == d.features[2].op);
    CHECK(back.features[1].bindings == d.features[1].bindings);
}

TEST_CASE("cad: a thread and a tapped hole as SolidCraft commands", "[cad]") {
    Design d;
    Feature box = box_feature("Caja", 20);
    d.features.push_back(box);
    Feature thread = operation("Rosca", OperationKind::Thread, "Caja");
    CHECK_FALSE(thread.buildable()); // no face yet
    thread.op.refs = {{10.0, 0.0, 5.0}, {-10.0, 0.0, 5.0}};
    thread.op.standard = "M8";
    thread.op.hand = "left";
    thread.op.modeled = true;
    thread.op.depth = 12;
    CHECK(thread.buildable());
    d.features.push_back(thread);
    Feature hole = operation("Agujero", OperationKind::Hole, "Caja");
    hole.op.refs = {{0.0, 0.0, 20.0}};
    hole.op.standard = "M6";
    hole.op.tapped = true;
    d.features.push_back(hole);
    std::vector<nlohmann::json> threads, holes;
    for (const std::string& c : build_commands(d)) {
        const nlohmann::json j = nlohmann::json::parse(c);
        if (j.value("command", "") == "solid.thread")
            threads.push_back(j.at("params"));
        if (j.value("command", "") == "solid.hole")
            holes.push_back(j.at("params"));
    }
    REQUIRE(threads.size() == 2);
    CHECK(threads[0].at("designation") == "M8");
    CHECK(threads[0].at("hand") == "left");
    CHECK(threads[0].at("modeled") == true);
    CHECK(threads[0].at("length") == 12.0);
    REQUIRE(holes.size() == 1);
    CHECK(holes[0].at("thread") == "M6");
    CHECK_FALSE(holes[0].contains("clearance"));
    // Kept with the design.
    const Design back = parse_design(design_json(d));
    CHECK(back.features[1].op == d.features[1].op);
    CHECK(back.features[2].op == d.features[2].op);
}

TEST_CASE("cad: move, copy, split and draft through the tool", "[cad]") {
    if (kFake.empty() || !std::filesystem::is_regular_file(kFake))
        SKIP("the stand-in solidcraft-cli was not built");
    set_env("TISMA_FAKE_SOLIDCRAFT_HANG", nullptr);
    set_env("TISMA_FAKE_SOLIDCRAFT_BROKEN", nullptr);
    set_env("TISMA_FAKE_SOLIDCRAFT_REFUSE", nullptr);
    Design d;
    d.features.push_back(box_feature("Caja", 20));
    Feature move = operation("Mover", OperationKind::Move, "Caja");
    move.op.angle = 0.0;
    CHECK_FALSE(move.buildable()); // nothing to do
    move.op.offset = {30.0, 0.0, 0.0};
    move.op.copy = true;
    move.op.angle = 90.0;
    move.op.axis = 'Z';
    REQUIRE(move.buildable());
    d.features.push_back(move);
    BuildResult b = build_timeline(kFake, d, MeshQuality::Standard);
    REQUIRE(b.ok);
    REQUIRE_FALSE(b.failed_feature);
    CHECK_THAT(b.volume_mm3, WithinRel(16000.0, 1e-6)); // the copy beside it
    Feature split = operation("Partir", OperationKind::Split, "Caja");
    split.op.plane = "XY";
    d.features.push_back(split);
    b = build_timeline(kFake, d, MeshQuality::Standard);
    REQUIRE(b.failed_feature); // the XY plane is the box's bottom face: nothing to split
    CHECK(*b.failed_feature == 2);
    d.features.back().op.plane_offset = 10.0;
    b = build_timeline(kFake, d, MeshQuality::Standard);
    CHECK_FALSE(b.failed_feature);
    CHECK_THAT(b.volume_mm3, WithinRel(16000.0, 1e-6));
    Feature draft = operation("Desmoldeo", OperationKind::Draft, "Caja");
    draft.op.value = 3.0;
    draft.op.plane = "XY";
    CHECK_FALSE(draft.buildable()); // no face
    draft.op.refs = {{10.0, 0.0, 10.0}};
    d.features.push_back(draft);
    b = build_timeline(kFake, d, MeshQuality::Standard);
    CHECK_FALSE(b.failed_feature);
    // As commands: the plane moved along its normal.
    bool found = false;
    for (const std::string& c : build_commands(d)) {
        const nlohmann::json j = nlohmann::json::parse(c);
        if (j.value("command", "") == "solid.split_body") {
            found = true;
            CHECK(j["params"]["plane"]["origin"] == nlohmann::json::array({0.0, 0.0, 10.0}));
        }
    }
    CHECK(found);
    const Design back = parse_design(design_json(d));
    for (std::size_t i = 1; i < d.features.size(); ++i)
        CHECK(back.features[i].op == d.features[i].op);
}

TEST_CASE("cad: CD5 and CD6, a standard hole and a fillet through the tool (rule 7)", "[cad]") {
    if (kFake.empty() || !std::filesystem::is_regular_file(kFake))
        SKIP("the stand-in solidcraft-cli was not built");
    set_env("TISMA_FAKE_SOLIDCRAFT_HANG", nullptr);
    set_env("TISMA_FAKE_SOLIDCRAFT_BROKEN", nullptr);
    set_env("TISMA_FAKE_SOLIDCRAFT_REFUSE", nullptr);
    const auto volume = [&](const Design& d) {
        const BuildResult b = build_timeline(kFake, d, MeshQuality::Standard);
        INFO(b.error << " " << b.failure);
        REQUIRE(b.ok);
        REQUIRE_FALSE(b.failed_feature);
        return b.volume_mm3;
    };
    // CD5: a cylinder 20 across and 10 high, an M5 clearance hole through it
    // from the top: 5.5 mm (ISO 273, medium).
    Design cd5;
    Feature cyl;
    cyl.name = "Cilindro";
    cyl.kind = Primitive::Cylinder;
    cyl.sizes = {20.0, 10.0, 0.0};
    cd5.features.push_back(cyl);
    const double solid = volume(cd5);
    Feature hole = operation("Agujero", OperationKind::Hole, "Cilindro");
    hole.op.refs = {{0.0, 0.0, 10.0}};
    hole.op.standard = "M5";
    cd5.features.push_back(hole);
    const double holed = volume(cd5);
    CHECK_THAT(solid - holed, WithinRel(std::numbers::pi * 2.75 * 2.75 * 10.0, 0.01));
    // Tapped instead: the M5 tap drill, 5 - 0.8 = 4.2 mm (ISO 261 coarse pitch).
    cd5.features.back().op.tapped = true;
    const double tapped = volume(cd5);
    CHECK_THAT(solid - tapped, WithinRel(std::numbers::pi * 2.1 * 2.1 * 10.0, 0.01));
    // A thread on the outside of the cylinder: drawn on its face, the body
    // as it was (the stand-in does not cut it).
    cd5.features.pop_back();
    Feature thread = operation("Rosca", OperationKind::Thread, "Cilindro");
    thread.op.refs = {{10.0, 0.0, 5.0}};
    thread.op.standard = "M20";
    cd5.features.push_back(thread);
    {
        const BuildResult b = build_timeline(kFake, cd5, MeshQuality::Standard);
        // M20 is not in the stand-in's table: the thread is refused and
        // marked (rule 8), the cylinder before it still built.
        REQUIRE(b.failed_feature);
        CHECK(*b.failed_feature == 1);
        CHECK_THAT(b.volume_mm3, WithinRel(solid, 1e-9));
    }
    cd5.features.back().op.standard = "M16";
    cd5.features.back().op.depth = 6.0;
    CHECK_THAT(volume(cd5), WithinRel(solid, 1e-9));

    // CD6: CD2's 40 x 30 x 20 block, a 3 mm fillet on one edge; deleted, it
    // is 24 000 mm3 again.
    Design cd6;
    Feature plate;
    plate.name = "Bloque";
    plate.type = Feature::Type::Sketch;
    plate.sketch.entities.push_back({Entity::Kind::Rectangle, {{0.0, 0.0}, {40.0, 30.0}}});
    plate.operation.distance = 20.0;
    cd6.features.push_back(plate);
    CHECK_THAT(volume(cd6), WithinAbs(24000.0, 1e-3));
    Feature round = operation("Redondeo", OperationKind::Fillet, "Bloque");
    round.op.refs = {{40.0, 30.0, 10.0}};
    round.op.value = 3.0;
    cd6.features.push_back(round);
    CHECK_THAT(24000.0 - volume(cd6), WithinRel((1.0 - std::numbers::pi / 4.0) * 9.0 * 20.0, 0.01));
    cd6.features.pop_back();
    CHECK_THAT(volume(cd6), WithinAbs(24000.0, 1e-3));
    // A 2 mm chamfer there takes 2 x 2 / 2 x 20.
    Feature cut = operation("Chaflán", OperationKind::Chamfer, "Bloque");
    cut.op.refs = {{40.0, 30.0, 10.0}};
    cut.op.value = 2.0;
    cd6.features.push_back(cut);
    CHECK_THAT(volume(cd6), WithinAbs(24000.0 - 40.0, 1e-3));

    // Shell, mirror, patterns and combine on a 20 mm box (8000 mm3).
    const auto with = [&](Feature op) {
        Design d;
        d.features.push_back(box_feature("Caja", 20.0));
        d.features.push_back(std::move(op));
        return volume(d);
    };
    Feature shell = operation("Vaciado", OperationKind::Shell, "Caja");
    shell.op.value = 2.0;
    shell.op.refs = {{0.0, 0.0, 20.0}}; // the top left open
    CHECK_THAT(with(shell), WithinAbs(8000.0 - 16.0 * 16.0 * 18.0, 1e-3));
    Feature mirror = operation("Simetría", OperationKind::Mirror, "Caja");
    CHECK_THAT(with(mirror), WithinAbs(16000.0, 1e-3));
    Feature ring = operation("Patrón", OperationKind::PatternCircular, "Caja");
    ring.op.count = 6;
    CHECK_THAT(with(ring), WithinRel(48000.0, 1e-4)); // float corners turned
    Feature row = operation("Fila", OperationKind::PatternRectangular, "Caja");
    row.op.count = 3;
    row.op.spacing = 30.0;
    CHECK_THAT(with(row), WithinAbs(24000.0, 1e-3));
    Design cut_box;
    cut_box.features.push_back(box_feature("Caja", 20.0));
    cut_box.features.push_back(box_feature("Cubo", 10.0));
    Feature combine = operation("Combinar", OperationKind::Combine, "Caja");
    combine.op.tools = {"Cubo"};
    cut_box.features.push_back(combine);
    CHECK_THAT(volume(cut_box), WithinAbs(7000.0, 1e-3));
    // An edge that is not there: the operation fails, the box is built.
    Design wrong;
    wrong.features.push_back(box_feature("Caja", 20.0));
    Feature miss = operation("Redondeo", OperationKind::Fillet, "Caja");
    miss.op.refs = {{3.0, 3.0, 3.0}};
    wrong.features.push_back(miss);
    const BuildResult b = build_timeline(kFake, wrong, MeshQuality::Standard);
    REQUIRE(b.ok);
    REQUIRE(b.failed_feature);
    CHECK(*b.failed_feature == 1);
    CHECK_THAT(b.volume_mm3, WithinAbs(8000.0, 1e-3));
}

TEST_CASE("CD25: one design session for twenty builds, and opened again when it dies", "[cad]") {
    const auto starts = std::filesystem::temp_directory_path() / "tisma_cd25_starts.txt";
    const auto die = std::filesystem::temp_directory_path() / "tisma_cd25_die";
    std::filesystem::remove(starts);
    std::filesystem::remove(die);
#ifdef _WIN32
    _putenv_s("TISMA_FAKE_SOLIDCRAFT_STARTS", starts.string().c_str());
    _putenv_s("TISMA_FAKE_SOLIDCRAFT_MCP_DIE", die.string().c_str());
#else
    setenv("TISMA_FAKE_SOLIDCRAFT_STARTS", starts.string().c_str(), 1);
    setenv("TISMA_FAKE_SOLIDCRAFT_MCP_DIE", die.string().c_str(), 1);
#endif
    {
        SolidCraftSession session(kFake);
        for (int i = 0; i < 20; ++i) {
            const BuildResult built = build_design(kFake, box_design(), MeshQuality::Standard, 60.0, &session);
            INFO(built.error);
            REQUIRE(built.ok);
            CHECK_THAT(built.volume_mm3, WithinAbs(24000.0, 1e-3));
            CHECK_FALSE(built.session_restarted);
            CHECK_THAT(built.measure, ContainsSubstring("volume_mm3"));
            CHECK_FALSE(built.design_file.empty());
        }
        CHECK(session.starts() == 1);
        CHECK(session.open());
        // Killed: the next build opens it again and gives the same design.
        { std::ofstream(die) << "x"; }
        const BuildResult again = build_design(kFake, box_design(), MeshQuality::Standard, 60.0, &session);
        INFO(again.error);
        REQUIRE(again.ok);
        CHECK(again.session_restarted);
        CHECK_THAT(again.volume_mm3, WithinAbs(24000.0, 1e-3));
        CHECK(session.starts() == 2);
        CHECK(session.restarts() == 1);
        // Idle: closed after the time, opened again by the next build.
        session.close_if_idle(0.0);
        CHECK_FALSE(session.open());
    }
    std::ifstream f(starts);
    int lines = 0;
    for (std::string l; std::getline(f, l);)
        ++lines;
    CHECK(lines == 2);
#ifdef _WIN32
    _putenv_s("TISMA_FAKE_SOLIDCRAFT_STARTS", "");
    _putenv_s("TISMA_FAKE_SOLIDCRAFT_MCP_DIE", "");
#else
    unsetenv("TISMA_FAKE_SOLIDCRAFT_STARTS");
    unsetenv("TISMA_FAKE_SOLIDCRAFT_MCP_DIE");
#endif
}

#ifndef _WIN32
TEST_CASE("CD25: a stopped child whose own child keeps the pipes open does not hang", "[cad]") {
    cad::ChildProcess child;
    std::string error;
    REQUIRE(child.start("/bin/sh", {"-c", "sleep 30 & echo ready; sleep 30"}, error));
    const std::optional<std::string> line = child.read_line(10.0);
    REQUIRE(line);
    CHECK(*line == "ready");
    const auto t0 = std::chrono::steady_clock::now();
    child.stop();
    CHECK(std::chrono::steady_clock::now() - t0 < std::chrono::seconds(5));
    CHECK_FALSE(child.running());
}
#endif

namespace {
std::pair<double, double> view_lengths(const std::vector<cad::ViewLine>& lines) {
    double shown = 0, hidden = 0;
    for (const cad::ViewLine& l : lines)
        (l.hidden ? hidden : shown) += std::hypot(l.x1 - l.x0, l.y1 - l.y0);
    return {shown, hidden};
}
} // namespace

TEST_CASE("view as DXF: a cube from the front and from a corner, hidden edges dashed or left out", "[cad]") {
    const TriangleMesh cube = make_box(10, 10, 10);
    cad::ViewDxfOptions front;
    front.look = {0, 1, 0};
    front.up = {0, 0, 1};
    auto [shown, hidden] = view_lengths(cad::view_lines(cube, front));
    CHECK_THAT(shown, Catch::Matchers::WithinAbs(40.0, 0.01)); // the front square
    CHECK_THAT(hidden, Catch::Matchers::WithinAbs(40.0, 0.01)); // the back one, behind it

    // From the corner (+,+,+): nine edges seen, the three at the far corner hidden.
    cad::ViewDxfOptions corner;
    corner.look = {-1, -1, -1};
    corner.up = {0, 0, 1};
    const double edge = 10.0 * std::sqrt(2.0 / 3.0);
    std::tie(shown, hidden) = view_lengths(cad::view_lines(cube, corner));
    CHECK_THAT(shown, Catch::Matchers::WithinRel(9 * edge, 0.02));
    CHECK_THAT(hidden, Catch::Matchers::WithinRel(3 * edge, 0.05));

    corner.hidden = false;
    const std::vector<cad::ViewLine> seen = cad::view_lines(cube, corner);
    CHECK(std::none_of(seen.begin(), seen.end(), [](const cad::ViewLine& l) { return l.hidden; }));

    const std::string dxf = cad::write_view_dxf(cad::view_lines(cube, front));
    CHECK_THAT(dxf, Catch::Matchers::ContainsSubstring("DASHED"));
    CHECK_THAT(dxf, Catch::Matchers::ContainsSubstring("8\nHIDDEN\n"));
    CHECK_THAT(dxf, Catch::Matchers::ContainsSubstring("8\nVISIBLE\n"));
    // Our own reader takes it back as lines.
    const cad::SketchImport back = cad::read_dxf(dxf);
    REQUIRE(back.ok());
    CHECK(back.entities.size() == cad::view_lines(cube, front).size());
}

TEST_CASE("view as DXF: a cylinder from above, its rim seen and the bottom one hidden, no facet lines", "[cad]") {
    const TriangleMesh cylinder = make_cylinder(10, 20, 64);
    cad::ViewDxfOptions top;
    const double rim = 64 * 2 * 10 * std::sin(3.14159265358979323846 / 64);
    auto [shown, hidden] = view_lengths(cad::view_lines(cylinder, top));
    CHECK_THAT(shown, Catch::Matchers::WithinRel(rim, 0.01));
    CHECK_THAT(hidden, Catch::Matchers::WithinRel(rim, 0.01));

    // From the side: two silhouettes, the front halves of the rims seen, the
    // back halves behind them.
    cad::ViewDxfOptions side;
    side.look = {0, 1, 0};
    side.up = {0, 0, 1};
    std::tie(shown, hidden) = view_lengths(cad::view_lines(cylinder, side));
    CHECK_THAT(shown, Catch::Matchers::WithinRel(2 * 20.0 + 2 * 20.0, 0.02));
    CHECK_THAT(hidden, Catch::Matchers::WithinRel(2 * 20.0, 0.02));
}
