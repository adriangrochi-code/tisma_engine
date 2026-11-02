// The Design tab's engine side (docs/spec/features/cad_design.md, stage D1):
// the design as a SolidCraft script, the bundled tool driven through its
// stand-in (tests/fake_solidcraft_cli.cpp), and the 3MF written and read.

#include "cad/design.hpp"
#include "cad/process.hpp"
#include "cad/solidcraft.hpp"
#include "io/model.hpp"
#include "mesh/shapes.hpp"

#include <nlohmann/json.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cmath>
#include <cstdlib>
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

Design box_design(double l = 40.0, double w = 30.0, double h = 20.0) {
    Design d;
    d.name = "Test";
    d.features.push_back({"Box", Primitive::Box, {l, w, h}});
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
    tube.features.push_back({"Tube", Primitive::Tube, {20.0, 12.0, 10.0}});
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
        one.features.push_back({"A", p, default_sizes(p)});
        const std::vector<std::string> own = build_commands(one);
        REQUIRE(own.size() == 1);
        CHECK(nlohmann::json::parse(own[0]).at("command") == command_of(p));
    }
    // A suppressed feature (the eye in Historial) and bad sizes are left out.
    Design mixed = box_design();
    mixed.features.push_back({"Hidden", Primitive::Sphere, {10.0, 0.0, 0.0}, true});
    mixed.features.push_back({"Flat", Primitive::Cylinder, {10.0, 0.0, 0.0}});
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
    d.features.push_back({"Ring", Primitive::Torus, {30.0, 8.0, 0.0}, true});
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
    CHECK_FALSE(info.has("solid.fillet")); // «Próximamente»
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
    more.features.push_back({"Tube", Primitive::Tube, {20.0, 12.0, 20.0}});
    more.features.push_back({"Ball", Primitive::Sphere, {10.0, 0.0, 0.0}});
    more.features.push_back({"Cone", Primitive::Cone, {20.0, 0.0, 20.0}});
    more.features.push_back({"Ring", Primitive::Torus, {30.0, 8.0, 0.0}});
    const BuildResult four = build_design(kFake, more, MeshQuality::Fine);
    INFO(four.error);
    REQUIRE(four.ok);
    const double pi = std::numbers::pi;
    const double expected = pi * (100.0 - 36.0) * 20.0 + 4.0 / 3.0 * pi * 125.0 + pi * 100.0 * 20.0 / 3.0 + 2.0 * pi * pi * 15.0 * 16.0;
    CHECK_THAT(four.volume_mm3, WithinRel(expected, 0.01));
    // Nothing to build: no run, an empty mesh.
    Design empty;
    empty.features.push_back({"Hidden", Primitive::Box, {10.0, 10.0, 10.0}, true});
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
