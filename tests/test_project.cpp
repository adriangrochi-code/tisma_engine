#include "project/project.hpp"

#include "calibration/calibration.hpp"
#include "config/profiles.hpp"
#include "io/stl.hpp"
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
