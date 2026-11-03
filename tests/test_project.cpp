#include "project/project.hpp"

#include "calibration/calibration.hpp"
#include "config/profiles.hpp"
#include "io/model.hpp"
#include "io/stl.hpp"
#include "io/zip.hpp"
#include "test_temp.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <fstream>

using namespace tisma;
using Catch::Matchers::ContainsSubstring;

namespace {

TriangleMesh tetrahedron(float size) {
    TriangleMesh m;
    const Vec3f a{0, 0, 0}, b{size, 0, 0}, c{0, size, 0}, d{0, 0, size};
    m.add_triangle(a, c, b);
    m.add_triangle(a, b, d);
    m.add_triangle(a, d, c);
    m.add_triangle(b, c, d);
    return m;
}

Project sample() {
    Project p;
    p.settings.layer_height = 0.16;
    p.settings.variables["temperature"] = {"215"};
    HeightChange change;
    change.z = 10.0;
    change.temperature = 205.0;
    p.settings.changes.push_back(change);
    p.profiles[std::size_t(ProfileKind::Filament)] = "PLA";
    p.profiles[std::size_t(ProfileKind::Print)] = "0.16 mm — fino";
    p.models.push_back({"pieza.stl", tetrahedron(10.0f), {128.0, 128.0, 0.0}, {}});
    p.models.push_back({"otra.stl", tetrahedron(2.5f), {10.0, -3.5, 0.25}, {}});
    p.calibration = ProjectCalibration{"temperature", "C", {{0.0, 10.0, 215.0}, {10.0, 20.0, 205.0}}};
    return p;
}

void same_mesh(const TriangleMesh& a, const TriangleMesh& b) {
    REQUIRE(a.triangle_count() == b.triangle_count());
    CHECK(a.corners() == b.corners());
}

} // namespace

TEST_CASE("binary STL is written as the format says and reads back exactly", "[project][stl]") {
    const TriangleMesh m = tetrahedron(3.25f);
    const std::string bytes = stl_binary(m, "name");
    REQUIRE(bytes.size() == 84 + 50 * m.triangle_count());
    CHECK(bytes.substr(0, 4) == "name");
    CHECK(bytes[4] == '\0');
    CHECK(static_cast<unsigned char>(bytes[80]) == m.triangle_count());
    const StlFile back = parse_stl(bytes);
    CHECK(back.format == StlFormat::Binary);
    same_mesh(back.mesh, m);
    // The header never reads as ASCII, whatever the name.
    CHECK(parse_stl(stl_binary(m, "solid trap")).format == StlFormat::Binary);
}

TEST_CASE("a project reads back as it was written", "[project]") {
    const Project p = sample();
    const std::string bytes = write_project(p);
    CHECK(bytes.substr(0, 8) == "TISMAPRJ");
    CHECK(bytes.substr(12, 4) == "JSON");

    const Project back = read_project(bytes);
    CHECK(settings_json(back.settings) == settings_json(p.settings));
    CHECK(back.profiles == p.profiles);
    REQUIRE(back.models.size() == 2);
    for (std::size_t i = 0; i < 2; ++i) {
        CHECK(back.models[i].name == p.models[i].name);
        CHECK(back.models[i].offset == p.models[i].offset);
        same_mesh(back.models[i].mesh, p.models[i].mesh);
    }
    REQUIRE(back.calibration);
    CHECK(back.calibration->kind == "temperature");
    CHECK(back.calibration->unit == "C");
    REQUIRE(back.calibration->segments.size() == 2);
    CHECK(back.calibration->segments[1].z0 == 10.0);
    CHECK(back.calibration->segments[1].value == 205.0);

    // Through a file, saved over an older one.
    const auto path = test_temp_dir() / "tisma_test_project.tisma";
    std::ofstream(path) << "old";
    save_project(path, p);
    CHECK_FALSE(std::filesystem::exists(path.string() + ".tmp"));
    CHECK(settings_json(load_project(path).settings) == settings_json(p.settings));
    std::filesystem::remove(path);

    // Painted faces come back with their model; an older reader skips them.
    Project painted = p;
    painted.models[1].painting.set_tree(PaintChannel::Seam, 2, {Painting::kSplit, 0, 1, 2, 1});
    painted.models[1].painting.set_tree(PaintChannel::Color, 0, {3});
    const Project again = read_project(write_project(painted));
    CHECK(again.models[0].painting.empty());
    CHECK(again.models[1].painting == painted.models[1].painting);
    // A painting of more triangles than the mesh has is refused.
    Project wrong = p;
    wrong.models[0].painting.set_tree(PaintChannel::Support, 4, {kPaintBlock}); // a tetrahedron has 4
    CHECK_THROWS_WITH(read_project(write_project(wrong)), Catch::Matchers::ContainsSubstring("does not have"));

    // An empty project is a valid one.
    const Project empty = read_project(write_project(Project{}));
    CHECK(empty.models.empty());
    CHECK_FALSE(empty.calibration);
}

TEST_CASE("a calibration tower's project keeps the tower", "[project]") {
    const CalibrationPrint tower = temperature_tower(220.0, 200.0, 10.0);
    Project p;
    p.settings.changes = tower.changes;
    p.models.push_back({"tisma_calibration_temperature.stl", tower.mesh, {}, {}});
    p.calibration = ProjectCalibration{tower.kind, tower.unit, tower.segments};
    const Project back = read_project(write_project(p));
    same_mesh(back.models[0].mesh, tower.mesh);
    CHECK(back.settings.changes.size() == tower.changes.size());
    REQUIRE(back.calibration->segments.size() == tower.segments.size());
    CHECK(back.calibration->segments.back().value == tower.segments.back().value);
}

TEST_CASE("damaged or newer projects are rejected with a reason", "[project]") {
    const std::string good = write_project(sample());
    const auto fails = [](const std::string& bytes, const std::string& message) {
        try {
            read_project(bytes);
            FAIL("accepted");
        } catch (const ProjectError& e) {
            CHECK_THAT(e.what(), ContainsSubstring(message));
        }
    };
    fails("", "not a project file");
    fails("TISMAPRX" + good.substr(8), "not a project file");
    fails(good.substr(0, good.size() - 7), "cut short");
    fails(good.substr(0, 20), "cut short");
    std::string newer = good;
    newer[8] = 2;
    fails(newer, "newer");
    // A MESH chunk before the JSON.
    std::string swapped = good.substr(0, 12) + std::string("MESH") + good.substr(16);
    fails(swapped, "first chunk must be JSON");

    // Unknown chunks are skipped.
    std::string extra = good + "THMB" + std::string("\x03\0\0\0\0\0\0\0", 8) + "abc";
    CHECK(read_project(extra).models.size() == 2);

    // A model whose mesh chunk is missing.
    const std::size_t first_mesh = good.find("MESH");
    REQUIRE(first_mesh != std::string::npos);
    fails(good.substr(0, first_mesh), "no MESH chunk 0");

    // Bad settings name their key.
    Project p;
    std::string bytes = write_project(p);
    const std::size_t at = bytes.find("\"height\": 0.2");
    REQUIRE(at != std::string::npos);
    bytes.replace(at, 13, "\"height\": 0.0");
    fails(bytes, "layers.height");
}

TEST_CASE("a project keeps a model's own settings, volumes and height ranges (model_tools.md)", "[project]") {
    Project p = sample();
    ProjectModel& m = p.models.front();
    m.settings = R"({"walls":{"count":3}})";
    m.volumes.push_back({false, true, {5.0, 0.0, 2.0}, {8.0, 8.0, 4.0}, R"({"infill":{"density":60}})"});
    m.volumes.push_back({true, false, {0.0, 0.0, -1.0}, {4.0, 4.0, 20.0}, "", {0, -1, 0, 1, 0, 0, 0, 0, 1}});
    m.ranges.push_back({4.0, 6.0, R"({"layers":{"height":0.1}})"});
    const Project back = read_project(write_project(p));
    const ProjectModel& b = back.models.front();
    CHECK(b.settings == m.settings);
    REQUIRE(b.volumes.size() == 2);
    CHECK_FALSE(b.volumes[0].negative);
    CHECK(b.volumes[0].cylinder);
    CHECK(b.volumes[0].offset == m.volumes[0].offset);
    CHECK(b.volumes[0].size == m.volumes[0].size);
    CHECK(b.volumes[0].settings == m.volumes[0].settings);
    CHECK(b.volumes[1].negative);
    CHECK(b.volumes[1].settings.empty());
    CHECK(b.volumes[0].turn == m.volumes[0].turn); // upright: not written
    CHECK(b.volumes[1].turn == m.volumes[1].turn); // a quarter turn about Z
    REQUIRE(b.ranges.size() == 1);
    CHECK(b.ranges[0].from == 4.0);
    CHECK(b.ranges[0].to == 6.0);
    CHECK(b.ranges[0].settings == m.ranges[0].settings);
    CHECK(back.models[1].volumes.empty());
}

TEST_CASE("a project as a 3MF package: any reader sees the plate, the engine reads it all back (formats.md, rule 11)",
          "[project]") {
    Project p = sample();
    p.models[1].painting.set_tree(PaintChannel::Seam, 2, {Painting::kSplit, 0, 1, 2, 1});
    p.models[0].settings = R"({"walls":{"count":3}})";
    p.models[0].volumes.push_back({false, true, {5.0, 0.0, 2.0}, {8.0, 8.0, 4.0}, R"({"infill":{"density":60}})"});
    p.models[0].ranges.push_back({4.0, 6.0, R"({"layers":{"height":0.1}})"});
    p.models[1].extruder = 2;
    const std::string bytes = write_project_3mf(p);

    // A standard 3MF, as the server checks it: a ZIP with 3D/3dmodel.model and its triangles.
    REQUIRE(looks_like_zip(bytes));
    const ZipArchive zip(bytes);
    const ZipArchive::Entry* model = zip.find("3D/3dmodel.model");
    REQUIRE(model);
    const std::string xml = zip.read(*model);
    std::size_t triangles = 0;
    for (std::size_t at = 0; (at = xml.find("<triangle ", at)) != std::string::npos; ++at)
        ++triangles;
    CHECK(triangles == 8);
    REQUIRE(zip.find("Metadata/tisma_project.json"));
    // The painting in its own file, with a header that is no program's (the
    // server refuses entries starting with MZ, ELF, Mach-O or #!), and no
    // DOCTYPE in the XML parts.
    const ZipArchive::Entry* paint = zip.find("Metadata/tisma_paint_0.bin");
    REQUIRE(paint);
    CHECK(zip.read(*paint).starts_with("TPNT"));
    CHECK_FALSE(zip.find("Metadata/tisma_paint_1.bin"));
    for (const ZipArchive::Entry& e : zip.entries()) {
        const std::string entry = zip.read(e);
        INFO(e.name);
        for (const char* magic : {"MZ", "\x7f" "ELF", "#!", "\xca\xfe\xba\xbe", "\xfe\xed\xfa\xce", "\xfe\xed\xfa\xcf",
                                  "\xce\xfa\xed\xfe", "\xcf\xfa\xed\xfe"})
            CHECK_FALSE(entry.starts_with(magic));
        CHECK(entry.find("<!DOCTYPE") == std::string::npos);
        CHECK(entry.find("<!ENTITY") == std::string::npos);
    }
    // Any 3MF reader: the two objects, where they stand on the plate.
    const ModelFile plate = parse_3mf(bytes);
    CHECK(plate.objects == 2);
    CHECK(plate.mesh.triangle_count() == 8);
    CHECK(plate.mesh.bounding_box().max.x == 138.0f); // the first tetrahedron moved to 128

    // The engine: everything back, the models placed (offsets applied).
    CHECK(looks_like_project(bytes));
    const Project back = read_project(bytes);
    CHECK(settings_json(back.settings) == settings_json(p.settings));
    CHECK(back.profiles == p.profiles);
    REQUIRE(back.models.size() == 2);
    for (std::size_t i = 0; i < 2; ++i) {
        CHECK(back.models[i].name == p.models[i].name);
        CHECK(back.models[i].offset == std::array<double, 3>{0.0, 0.0, 0.0});
        REQUIRE(back.models[i].mesh.triangle_count() == p.models[i].mesh.triangle_count());
        for (std::size_t c = 0; c < p.models[i].mesh.corners().size(); ++c) {
            const Vec3f a = p.models[i].mesh.corners()[c], b = back.models[i].mesh.corners()[c];
            CHECK(b.x == float(double(a.x) + p.models[i].offset[0]));
            CHECK(b.y == float(double(a.y) + p.models[i].offset[1]));
            CHECK(b.z == float(double(a.z) + p.models[i].offset[2]));
        }
    }
    CHECK(back.models[0].painting.empty());
    CHECK(back.models[1].painting == p.models[1].painting);
    CHECK(back.models[0].settings == p.models[0].settings);
    REQUIRE(back.models[0].volumes.size() == 1);
    CHECK(back.models[0].volumes[0].offset == p.models[0].volumes[0].offset);
    REQUIRE(back.models[0].ranges.size() == 1);
    CHECK(back.models[1].extruder == 2);
    REQUIRE(back.calibration);
    CHECK(back.calibration->segments.size() == 2);

    // Saved by its name: .3mf is the package, .tisma the engine's own file.
    const auto dir = test_temp_dir();
    save_project(dir / "tisma_test_project.3mf", p);
    save_project(dir / "tisma_test_project.tisma", p);
    CHECK(is_project_file(dir / "tisma_test_project.3mf"));
    CHECK(is_project_file(dir / "tisma_test_project.tisma"));
    CHECK(looks_like_zip(write_project_3mf(Project{})));
    CHECK(load_project(dir / "tisma_test_project.3mf").models.size() == 2);
    std::filesystem::remove(dir / "tisma_test_project.3mf");
    std::filesystem::remove(dir / "tisma_test_project.tisma");

    // A 3MF of another program is a model, not a project.
    const std::string plain = write_3mf(tetrahedron(5.0f), "plain");
    CHECK_FALSE(looks_like_project(plain));
    CHECK_THROWS_WITH(read_project(plain), Catch::Matchers::ContainsSubstring("no Tisma project data"));
}


TEST_CASE("FM23, FM24: a part's Engineering conditions, values by hand and options travel with the project",
          "[project][engineering]") {
    Project p = sample();
    ProjectEngineering& e = p.models[0].engineering;
    EngineeringCondition fixed;
    fixed.kind = ConditionKind::FixedZone;
    fixed.triangles = {0, 1};
    EngineeringCondition force;
    force.kind = ConditionKind::ForcePoint;
    force.point = {9.5, 5.0, 10.0};
    force.normal = {0.0, 0.0, 1.0};
    force.newtons = 5.1 * 9.81; // typed as 5.1 kg
    force.kilograms = 5.1;
    force.direction = ForceDirection::Axis;
    force.axis = -3;
    EngineeringCondition limit;
    limit.kind = ConditionKind::Limit;
    limit.triangles = {2};
    limit.limit = 5.0;
    limit.limit_percent = true;
    limit.number = 1;
    e.conditions = {fixed, force, limit};
    e.working_temperature = 40.0;
    e.overrides.extrusion_temperature = 230.0;
    e.structural.safety_factor = 3.0;
    e.aerodynamics.speed = 12.5;
    // The second model has nothing: it opens as before.
    REQUIRE(p.models[1].engineering.empty());
    for (const bool package : {false, true}) {
        INFO((package ? "3MF" : "own file"));
        // FM24: moved on the bed; the conditions stay on the part.
        Project moved = p;
        moved.models[0].offset = {130.0, 100.0, 0.0};
        const Project again = read_project(package ? write_project_3mf(moved) : write_project(moved));
        CHECK(again.notices.empty());
        const ProjectEngineering& r = again.models[0].engineering;
        REQUIRE(r.conditions.size() == 3);
        CHECK(r.conditions[0].kind == ConditionKind::FixedZone);
        CHECK(r.conditions[0].triangles == std::vector<std::uint32_t>{0, 1});
        CHECK(r.conditions[1].kind == ConditionKind::ForcePoint);
        CHECK(r.conditions[1].point == force.point);
        CHECK(r.conditions[1].newtons == force.newtons);
        CHECK(r.conditions[1].kilograms == 5.1); // shown again as 5.1 kg
        CHECK(r.conditions[1].direction == ForceDirection::Axis);
        CHECK(r.conditions[1].axis == -3);
        CHECK(r.conditions[2].limit_percent);
        CHECK(r.working_temperature == 40.0);
        CHECK(r.overrides.extrusion_temperature == 230.0);
        CHECK_FALSE(r.overrides.material); // from the profile: not copied
        CHECK(r.structural.safety_factor == 3.0);
        CHECK(r.aerodynamics.speed == 12.5);
        CHECK(again.models[1].engineering.empty());
    }
}

TEST_CASE("FM26 and newer data: conditions that no longer fit are dropped with a notice, the part kept",
          "[project][engineering]") {
    Project p = sample();
    EngineeringCondition zone;
    zone.triangles = {0, 999999}; // a triangle the mesh does not have
    EngineeringCondition point;
    point.kind = ConditionKind::FixedPoint;
    p.models[0].engineering.conditions = {zone, point};
    const Project again = read_project(write_project(p));
    REQUIRE(again.notices.size() == 1);
    CHECK_THAT(again.notices[0], ContainsSubstring("1 Engineering condition(s)"));
    REQUIRE(again.models[0].engineering.conditions.size() == 1);
    CHECK(again.models[0].engineering.conditions[0].kind == ConditionKind::FixedPoint);
    // Data from a newer version: the project opens without it, and says so.
    CHECK_THROWS_AS(engineering_from_json(R"({"version": 99})"), EngineeringError);
    CHECK_THROWS_AS(engineering_from_json(R"({"version": 1, "conditions": [{"kind": "glue", "number": 1}]})"),
                    EngineeringError);
    const ProjectEngineering back = engineering_from_json(engineering_json(again.models[0].engineering));
    CHECK(back.conditions.size() == 1);
}
