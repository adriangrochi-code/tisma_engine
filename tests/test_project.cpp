#include "project/project.hpp"
#include "project/engineering_values.hpp"
#include "ui/project_slice.hpp"
#include "mesh/face_region.hpp"

#include "calibration/calibration.hpp"
#include "config/profiles.hpp"
#include "io/model.hpp"
#include "io/stl.hpp"
#include "io/zip.hpp"
#include "test_temp.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cmath>
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
    // A support blocker and an enforcer too (BH47).
    painted.models[1].painting.set_tree(PaintChannel::Support, 1, {kPaintBlock});
    painted.models[1].painting.set_tree(PaintChannel::Support, 3, {Painting::kSplit, kPaintEnforce, 0, 0, kPaintBlock});
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
    // A lattice (structural_optimization.md, rule 7) keeps its cell and bars.
    Project q = sample();
    ProjectVolume lattice;
    lattice.lattice = true;
    lattice.lattice_cell = 5.0;
    lattice.lattice_diameter = 1.25;
    lattice.settings = R"({"infill":{"density":100}})";
    q.models.front().volumes.push_back(lattice);
    const Project qb = read_project(write_project(q));
    REQUIRE(qb.models.front().volumes.size() == 1);
    CHECK(qb.models.front().volumes[0].lattice);
    CHECK(qb.models.front().volumes[0].lattice_cell == 5.0);
    CHECK(qb.models.front().volumes[0].lattice_diameter == 1.25);
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

    // The engine: everything back, each mesh in its own coordinates and its
    // place on the plate as its offset (formats.md, rules 8 and 11 bis).
    CHECK(looks_like_project(bytes));
    const Project back = read_project(bytes);
    CHECK(settings_json(back.settings) == settings_json(p.settings));
    CHECK(back.profiles == p.profiles);
    REQUIRE(back.models.size() == 2);
    for (std::size_t i = 0; i < 2; ++i) {
        CHECK(back.models[i].name == p.models[i].name);
        CHECK(back.models[i].offset == p.models[i].offset);
        CHECK(back.models[i].turn == p.models[i].turn);
        REQUIRE(back.models[i].mesh.triangle_count() == p.models[i].mesh.triangle_count());
        for (std::size_t c = 0; c < p.models[i].mesh.corners().size(); ++c)
            CHECK(back.models[i].mesh.corners()[c] == p.models[i].mesh.corners()[c]);
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
    // The last structural result, with what it was computed from (rule 11 bis).
    const std::string settings = settings_json(p.settings);
    e.results.push_back({"structural", engineering_fingerprint(p.models[0].mesh, e, settings, "structural"),
                         R"({"verdict": "holds", "max_displacement_mm": 0.31})"});
    CHECK_FALSE(result_stale(e.results[0], p.models[0].mesh, e, settings));
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
        // On the same place of the part (a 3MF comes back with the mesh placed).
        const BoundingBox before = p.models[0].mesh.bounding_box(), after = again.models[0].mesh.bounding_box();
        CHECK(std::abs((r.conditions[1].point[0] - after.min.x) - (force.point[0] - before.min.x)) < 1e-4);
        CHECK(std::abs((r.conditions[1].point[1] - after.min.y) - (force.point[1] - before.min.y)) < 1e-4);
        CHECK(std::abs((r.conditions[1].point[2] - after.min.z) - (force.point[2] - before.min.z)) < 1e-4);
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
        // The result comes back the same and not stale (FM23).
        REQUIRE(r.results.size() == 1);
        CHECK(r.results[0] == e.results[0]);
        CHECK_FALSE(result_stale(r.results[0], again.models[0].mesh, r, settings_json(again.settings)));
        // A changed condition makes it stale («Desactualizado»).
        ProjectEngineering changed = r;
        changed.conditions[1].newtons = 60.0;
        CHECK(result_stale(r.results[0], again.models[0].mesh, changed, settings_json(again.settings)));
    }
}

TEST_CASE("an Engineering result is its own entry of the 3MF, never starting with a program's bytes",
          "[project][engineering]") {
    Project p = sample();
    p.models[1].engineering.results.push_back({"aerodynamics", "0123456789abcdef", R"({"cd": 1.05})"});
    const std::string package = write_project_3mf(p);
    const ZipArchive zip(package);
    const ZipArchive::Entry* entry = zip.find("Metadata/tisma_result_0.json");
    REQUIRE(entry);
    const std::string bytes = zip.read(*entry);
    CHECK(bytes == R"({"cd": 1.05})");
    for (const char* magic : {"MZ", "\x7f" "ELF", "#!"})
        CHECK_FALSE(bytes.starts_with(magic));
    CHECK(read_project(package).models[1].engineering.results.size() == 1);
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

TEST_CASE("EP1, EP3, EP5, EP10: the Structural panel's values come from the profile, by hand when changed", "[project]") {
    Settings profile;
    profile.filament_type = "PLA";
    profile.variables["temperature"] = {"215"};
    profile.variables["bed_temperature"] = {"60"};
    profile.layer_height = 0.2;
    profile.first_layer_height = 0.24;
    profile.wall_width = 0.45;
    ProjectEngineering e;
    // EP1: everything from the profile.
    EngineeringValues v = engineering_values(profile, e);
    CHECK(v.material_id == "bambu-pla-basic");
    CHECK_FALSE(v.material_by_hand);
    CHECK(v.extrusion_temperature.value == 215.0);
    CHECK_FALSE(v.extrusion_temperature.by_hand);
    CHECK(v.bed_temperature == 60.0);
    CHECK(v.layer_height.value == 0.2);
    CHECK(v.first_layer_height.value == 0.24);
    CHECK(v.line_width.value == 0.45);
    CHECK(v.working_temperature == 23.0);
    // EP2: another filament profile, nothing touched in the panel.
    profile.filament_type = "petg";
    profile.variables["temperature"] = {"240"};
    v = engineering_values(profile, e);
    CHECK(v.material_id == "bambu-petg-basic");
    CHECK(v.extrusion_temperature.value == 240.0);
    // EP3: 230 C by hand stays over a change of profile, with its dot.
    e.overrides.extrusion_temperature = 230.0;
    profile.variables["temperature"] = {"250"};
    v = engineering_values(profile, e);
    CHECK(v.extrusion_temperature.value == 230.0);
    CHECK(v.extrusion_temperature.by_hand);
    e.overrides.extrusion_temperature.reset(); // «Volver al del perfil»
    CHECK(engineering_values(profile, e).extrusion_temperature.value == 250.0);
    // EP5: a fixed point and no force.
    EngineeringCondition fixed;
    fixed.kind = ConditionKind::FixedPoint;
    e.conditions = {fixed};
    CHECK(analyze_block(e, v) == AnalyzeBlock::Force);
    EngineeringCondition push;
    push.kind = ConditionKind::ForcePoint;
    push.newtons = 50.0;
    e.conditions.push_back(push);
    CHECK(analyze_block(e, v) == AnalyzeBlock::None);
    e.conditions[0].hidden = true; // hidden: not used
    CHECK(analyze_block(e, v) == AnalyzeBlock::Fixed);
    e.conditions[0].hidden = false;
    // EP10: a type the table does not have: no material until one is chosen.
    profile.filament_type = "Wood Fill";
    v = engineering_values(profile, e);
    CHECK(v.material_id.empty());
    CHECK(analyze_block(e, v) == AnalyzeBlock::Material);
    e.overrides.material = "bambu-pla-basic";
    v = engineering_values(profile, e);
    CHECK(v.material_by_hand);
    CHECK(analyze_block(e, v) == AnalyzeBlock::None);
    CHECK(material_for_filament("PA-CF") == "bambu-pa6-cf");
    CHECK(material_for_filament("pla+") == "esun-pla-plus");
    CHECK(material_for_filament("") == "");
    // A filament profile that names no type: its name, or its first word.
    profile.filament_type.clear();
    e.overrides.material.reset();
    CHECK(engineering_values(profile, e, "PLA Genérico").material_id == "bambu-pla-basic");
    CHECK(engineering_values(profile, e, "PETG").material_id == "bambu-petg-basic");
    CHECK(engineering_values(profile, e, "Mi filamento").material_id.empty());
}

TEST_CASE("Engineering conditions follow the part onto the plate, placed and mirrored", "[project]") {
    ProjectEngineering e;
    EngineeringCondition dot;
    dot.kind = ConditionKind::ForcePoint;
    dot.point = {1.0, 2.0, 3.0};
    dot.normal = {1.0, 0.0, 0.0};
    e.conditions = {dot};
    // Moved by an offset: the point moves, the normal stays.
    ProjectEngineering moved = e;
    move_engineering(moved, {1, 0, 0, 0, 1, 0, 0, 0, 1}, {10.0, 20.0, 0.0});
    CHECK(moved.conditions[0].point == std::array<double, 3>{11.0, 22.0, 3.0});
    CHECK(moved.conditions[0].normal == std::array<double, 3>{1.0, 0.0, 0.0});
    // Mirrored in X about 5: the point to 9, the normal still outward (-x).
    ProjectEngineering mirrored = e;
    move_engineering(mirrored, {-1, 0, 0, 0, 1, 0, 0, 0, 1}, {10.0, 0.0, 0.0});
    CHECK(mirrored.conditions[0].point[0] == 9.0);
    CHECK(mirrored.conditions[0].normal[0] == -1.0);
    // Stretched twice in X, turned a quarter about Z: a normal along x stays
    // normal to the turned face (along y).
    ProjectEngineering turned = e;
    move_engineering(turned, {0, -1, 0, 2, 0, 0, 0, 0, 1}, {});
    CHECK(std::abs(turned.conditions[0].normal[1] - 1.0) < 1e-12);
    CHECK(std::abs(turned.conditions[0].normal[0]) < 1e-12);
    // project_objects keeps them, moved with the mesh by the model's offset.
    std::vector<ProjectModel> models(1);
    models[0].mesh.add_triangle({0, 0, 0}, {1, 0, 0}, {0, 1, 0});
    models[0].offset = {5.0, 0.0, 0.0};
    models[0].engineering = e;
    const std::vector<ui::PlateObject> objects = ui::project_objects(models);
    REQUIRE(objects[0].engineering.conditions.size() == 1);
    CHECK(objects[0].engineering.conditions[0].point[0] == 6.0);
}

TEST_CASE("a click picks the whole face: flat up to the edges, smooth around a cylinder", "[project]") {
    // A 10 mm cube: each side is its two triangles.
    TriangleMesh cube = read_stl(std::string(TISMA_TEST_DATA_DIR) + "/cube_ascii.stl").mesh;
    REQUIRE(cube.triangle_count() == 12);
    for (std::uint32_t t = 0; t < 12; ++t)
        CHECK(face_region(cube, t).size() == 2);
    // A cylinder of 32 sides: the wall (64 triangles, 11.25 degrees from one
    // side to the next) is one face; each cap is its own.
    TriangleMesh cylinder;
    const int n = 32;
    const auto at = [&](int i, float z) {
        const double a = 2.0 * 3.14159265358979 * (i % n) / n;
        return Vec3f{float(10.0 * std::cos(a)), float(10.0 * std::sin(a)), z};
    };
    for (int i = 0; i < n; ++i) {
        cylinder.add_triangle(at(i, 0), at(i + 1, 0), at(i + 1, 20)); // wall: triangles 0..2n-1
        cylinder.add_triangle(at(i, 0), at(i + 1, 20), at(i, 20));
    }
    for (int i = 0; i < n; ++i) {
        cylinder.add_triangle({0, 0, 0}, at(i + 1, 0), at(i, 0));    // bottom cap
        cylinder.add_triangle({0, 0, 20}, at(i, 20), at(i + 1, 20)); // top cap
    }
    CHECK(face_region(cylinder, 5).size() == std::size_t(2 * n));
    CHECK(face_region(cylinder, std::uint32_t(2 * n)).size() == std::size_t(n));
    // A sharper bend allowed: wall and caps stay apart (90 degrees).
    CHECK(face_region(cylinder, 5, 45.0).size() == std::size_t(2 * n));
    // Nothing for a triangle the mesh does not have.
    CHECK(face_region(cube, 99).empty());
}

TEST_CASE("FM24: a part moved and turned keeps its conditions on the same places of it", "[project]") {
    // A 100 x 10 x 10 mm bar, a force point at its tip, turned 90 degrees
    // about Z and moved 30 mm on the plate, as the interface saves it: the
    // mesh in its own coordinates, its place as the build transform.
    TriangleMesh bar;
    const auto quad = [&](Vec3f a, Vec3f b, Vec3f c, Vec3f d) {
        bar.add_triangle(a, b, c);
        bar.add_triangle(a, c, d);
    };
    const float L = 100, W = 10;
    quad({0, 0, 0}, {0, W, 0}, {L, W, 0}, {L, 0, 0});
    quad({0, 0, W}, {L, 0, W}, {L, W, W}, {0, W, W});
    quad({0, 0, 0}, {L, 0, 0}, {L, 0, W}, {0, 0, W});
    quad({0, W, 0}, {0, W, W}, {L, W, W}, {L, W, 0});
    quad({0, 0, 0}, {0, 0, W}, {0, W, W}, {0, W, 0});
    quad({L, 0, 0}, {L, W, 0}, {L, W, W}, {L, 0, W});
    ui::PlateObject o{"bar.stl", bar, {}, {}};
    o.place.x = 130.0;
    o.place.y = 100.0;
    o.place.rotation = {0, -1, 0, 1, 0, 0, 0, 0, 1};
    EngineeringCondition tip;
    tip.kind = ConditionKind::ForcePoint;
    tip.point = {100.0, 5.0, 5.0};
    tip.normal = {1.0, 0.0, 0.0};
    tip.newtons = 50.0;
    o.engineering.conditions = {tip};
    const TriangleMesh placed = ui::placed_mesh(o);
    const ui::Affine to_bed = ui::placing(o, placed);
    Project p;
    p.models.push_back({o.name, o.mesh, to_bed.b, {}});
    p.models.back().turn = to_bed.a;
    p.models.back().engineering = o.engineering;
    for (const bool three_mf : {true, false}) {
        INFO((three_mf ? "3MF" : "own file"));
        Project back = read_project(three_mf ? write_project_3mf(p) : write_project(p));
        REQUIRE(back.models.size() == 1);
        // The part's own frame: the same mesh, the same point on it.
        CHECK(back.models[0].mesh.corners() == bar.corners());
        REQUIRE(back.models[0].engineering.conditions.size() == 1);
        CHECK(back.models[0].engineering.conditions[0].point == tip.point);
        // On the plate again: the same turn and place, the tip where it was.
        const std::vector<ui::PlateObject> objects = ui::project_objects(back.models);
        REQUIRE(objects.size() == 1);
        CHECK(objects[0].mesh.corners() == bar.corners());
        for (std::size_t k = 0; k < 9; ++k)
            CHECK(std::abs(objects[0].place.rotation[k] - o.place.rotation[k]) < 1e-9);
        CHECK(std::abs(objects[0].place.x - o.place.x) < 1e-4);
        CHECK(std::abs(objects[0].place.y - o.place.y) < 1e-4);
        const ui::Affine again = ui::placing(objects[0], ui::placed_mesh(objects[0]));
        for (std::size_t r = 0; r < 3; ++r) {
            const double was = to_bed.a[3 * r] * tip.point[0] + to_bed.a[3 * r + 1] * tip.point[1] +
                               to_bed.a[3 * r + 2] * tip.point[2] + to_bed.b[r];
            const auto& q = objects[0].engineering.conditions[0].point;
            const double now = again.a[3 * r] * q[0] + again.a[3 * r + 1] * q[1] + again.a[3 * r + 2] * q[2] + again.b[r];
            CHECK(std::abs(now - was) < 1e-4);
        }
    }
}

TEST_CASE("rule 8: each new condition is numbered by its kind and listed by its line", "[project]") {
    TriangleMesh cube = read_stl(std::string(TISMA_TEST_DATA_DIR) + "/cube_ascii.stl").mesh;
    ProjectEngineering e;
    add_condition(e, ConditionKind::FixedZone, {}, {0, 0, -1}, face_region(cube, 0));
    add_condition(e, ConditionKind::FixedZone, {}, {0, 0, 1}, {});
    EngineeringCondition& push = add_condition(e, ConditionKind::ForcePoint, {5, 5, 10}, {0, 0, 1}, {});
    CHECK(e.conditions[0].number == 1);
    CHECK(e.conditions[1].number == 2);
    CHECK(push.number == 1);
    CHECK(push.newtons == 10.0);
    CHECK(push.radius == 5.0);
    CHECK(condition_label(e.conditions[0], cube, "es") == "Zona fija 1 · 1 cara");
    CHECK(condition_label(e.conditions[0], cube, "en") == "Fixed zone 1 · 1 face");
    push.newtons = 50.0;
    push.direction = ForceDirection::Axis;
    push.axis = -3;
    CHECK(condition_label(push, cube, "es") == "Punto de fuerza 1 · r 5 mm · 50 N hacia −Z");
    // Two sides of the cube in one zone: two faces.
    std::vector<std::uint32_t> two = face_region(cube, 0);
    for (const std::uint32_t t : face_region(cube, 2))
        two.push_back(t);
    e.conditions[1].triangles = two;
    CHECK(condition_label(e.conditions[1], cube, "es") == "Zona fija 2 · 2 caras");
}

// Printable (interface.md, «Menús»): a model that is not printed keeps that
// in the project, in both the engine's file and the 3MF package; a printed
// one writes nothing for it.
TEST_CASE("a project keeps which models are not printable", "[project]") {
    Project p = sample();
    REQUIRE(p.models.size() == 2);
    p.models[1].printable = false;
    const std::string json = write_project(p);
    CHECK(json.find("\"printable\"") != std::string::npos);
    const Project back = read_project(json);
    REQUIRE(back.models.size() == 2);
    CHECK(back.models[0].printable);
    CHECK_FALSE(back.models[1].printable);
    const Project back_3mf = read_project(write_project_3mf(p));
    REQUIRE(back_3mf.models.size() == 2);
    CHECK(back_3mf.models[0].printable);
    CHECK_FALSE(back_3mf.models[1].printable);
    CHECK(write_project(sample()).find("\"printable\"") == std::string::npos);
}
