#include "config/analysis_case.hpp"
#include "config/settings.hpp"
#include "fea/analysis.hpp"
#include "fea/materials.hpp"
#include "io/vtk.hpp"
#include "mesh/indexed_mesh.hpp"
#include "report/analysis_json.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <nlohmann/json.hpp>

#include <array>
#include <bit>
#include <cmath>
#include <sstream>
#include <string>

using namespace tisma;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

// A box from (0, 0, 0) to (x, y, z), facing out.
IndexedMesh box_mesh(float x, float y, float z) {
    IndexedMesh m;
    for (float h : {0.0f, z})
        for (const auto& p : {std::array<float, 2>{0.0f, 0.0f}, {x, 0.0f}, {x, y}, {0.0f, y}})
            m.vertices.push_back({p[0], p[1], h});
    auto quad = [&](std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d) {
        m.triangles.push_back({a, b, c});
        m.triangles.push_back({a, c, d});
    };
    quad(0, 3, 2, 1);
    quad(4, 5, 6, 7);
    for (std::uint32_t i = 0; i < 4; ++i)
        quad(i, (i + 1) % 4, 4 + (i + 1) % 4, 4 + i);
    return m;
}

std::string error_of(const std::string& json) {
    try {
        parse_analysis_case(json);
    } catch (const SettingsError& e) {
        return e.what();
    }
    return "";
}

} // namespace

TEST_CASE("analysis case: defaults and every key", "[analysis]") {
    const AnalysisCase d = parse_analysis_case("{}");
    CHECK(d.material_id == "bambu-pla-basic");
    CHECK(d.material.zt == 31.0);
    CHECK(d.model == AnalysisModel::AsPrinted);
    CHECK(d.quality == AnalysisQuality::Normal);
    CHECK(d.fixtures.empty());
    CHECK(d.loads.empty());
    CHECK(d.solver.preconditioner == Preconditioner::Multigrid);

    const AnalysisCase c = parse_analysis_case(R"({
        "material": {"base": "esun-pla-plus", "zt": 20, "s13": 15},
        "model": "solid", "quality": "high", "voxel": 0.05, "rows_per_layer": 4, "max_voxels": 1000,
        "fixtures": [{"name": "bed", "face": "bottom", "axes": "z"},
                     {"box": {"min": [null, -1, 0], "max": [5, null, null]}}],
        "loads": [{"name": "hook", "face": "x_max", "force": [0, 1.5, -20]}],
        "solver": {"tolerance": 1e-6, "max_iterations": 300, "preconditioner": "jacobi"}})");
    CHECK(c.material_id == "esun-pla-plus");
    CHECK(c.material.xt == 70.37);
    CHECK(c.material.zt == 20.0);
    CHECK(c.material.s13 == 15.0);
    CHECK(c.model == AnalysisModel::Solid);
    CHECK(c.quality == AnalysisQuality::High);
    CHECK(c.voxel == 0.05);
    CHECK(c.rows_per_layer == 4);
    CHECK(c.max_voxels == 1000);
    REQUIRE(c.fixtures.size() == 2);
    CHECK(c.fixtures[0].name == "bed");
    CHECK(c.fixtures[0].region.face == PartFace::Bottom);
    CHECK((!c.fixtures[0].x && !c.fixtures[0].y && c.fixtures[0].z));
    CHECK(c.fixtures[1].name == "fixture 2");
    CHECK(c.fixtures[1].region.box.min[0] < -1e299);
    CHECK(c.fixtures[1].region.box.min[1] == -1.0);
    CHECK(c.fixtures[1].region.box.max[0] == 5.0);
    CHECK(c.fixtures[1].region.box.max[2] > 1e299);
    REQUIRE(c.loads.size() == 1);
    CHECK(c.loads[0].region.face == PartFace::XMax);
    CHECK(c.loads[0].force == std::array<double, 3>{0.0, 1.5, -20.0});
    CHECK(c.solver.tolerance == 1e-6);
    CHECK(c.solver.max_iterations == 300);
    CHECK(c.solver.preconditioner == Preconditioner::Jacobi);
}

TEST_CASE("analysis case: errors name the key", "[analysis]") {
    CHECK_THAT(error_of("[1]"), ContainsSubstring("top level"));
    CHECK_THAT(error_of("{\"x\": 1}"), ContainsSubstring("case.x: unknown key"));
    CHECK_THAT(error_of("{\"material\": \"steel\"}"), ContainsSubstring("unknown material 'steel'; known: song2017-pla"));
    CHECK_THAT(error_of("{\"material\": {\"zt\": -1}}"), ContainsSubstring("material.zt: must be greater than 0"));
    CHECK_THAT(error_of("{\"material\": {\"nu12\": 0.9, \"nu13\": 0.9, \"nu23\": 0.9}}"), ContainsSubstring("not a valid material"));
    CHECK_THAT(error_of("{\"quality\": \"ultra\"}"), ContainsSubstring("quality: must be one of high, normal"));
    CHECK_THAT(error_of("{\"rows_per_layer\": 0}"), ContainsSubstring("rows_per_layer"));
    CHECK_THAT(error_of("{\"fixtures\": [{\"face\": \"bottom\", \"box\": {}}]}"), ContainsSubstring("fixtures[0]: needs either"));
    CHECK_THAT(error_of("{\"fixtures\": [{\"face\": \"side\"}]}"), ContainsSubstring("fixtures[0].face: must be one of"));
    CHECK_THAT(error_of("{\"fixtures\": [{\"face\": \"top\", \"axes\": \"xw\"}]}"), ContainsSubstring("fixtures[0].axes"));
    CHECK_THAT(error_of("{\"fixtures\": [{\"box\": {\"min\": [1, 2]}}]}"), ContainsSubstring("fixtures[0].box.min: must be [x, y, z]"));
    CHECK_THAT(error_of("{\"loads\": [{\"face\": \"top\"}]}"), ContainsSubstring("loads[0]: needs \"force\""));
    CHECK_THAT(error_of("{\"loads\": [{\"face\": \"top\", \"force\": [1, \"a\", 0]}]}"), ContainsSubstring("loads[0].force[1]: must be a number"));
    CHECK_THAT(error_of("{\"loads\": [{\"name\": 3, \"face\": \"top\", \"force\": [0, 0, 1]}]}"), ContainsSubstring("loads[0].name: must be a string"));
    CHECK_THAT(error_of("{\"solver\": {\"preconditioner\": \"lu\"}}"), ContainsSubstring("solver.preconditioner"));
    CHECK_THAT(error_of("{oops"), ContainsSubstring("not valid JSON"));
}

TEST_CASE("analysis: voxels from the quality and the line width", "[analysis]") {
    Settings s; // 0.4 mm lines, 0.2 mm layers
    AnalysisCase c;
    CHECK(analysis_resolution(c, s) == std::pair<double, int>{0.2, 1});
    c.quality = AnalysisQuality::High;
    CHECK(analysis_resolution(c, s) == std::pair<double, int>{0.1, 2});
    c.voxel = 0.05;
    CHECK(analysis_resolution(c, s) == std::pair<double, int>{0.05, 4});
    c.rows_per_layer = 1;
    CHECK(analysis_resolution(c, s) == std::pair<double, int>{0.05, 1});
}

TEST_CASE("analysis: a solid column pulled up, exact stress and safety", "[analysis]") {
    // 4 x 4 x 10 mm on rollers (bottom in z, one side in x, one in y), 160 N
    // up on the top: 10 MPa across the layers everywhere.
    const IndexedMesh column = box_mesh(4.0f, 4.0f, 10.0f);
    AnalysisCase c = parse_analysis_case(R"({
        "model": "solid",
        "fixtures": [{"face": "bottom", "axes": "z"}, {"face": "x_min", "axes": "x"}, {"face": "y_min", "axes": "y"}],
        "loads": [{"face": "top", "force": [0, 0, 160]}],
        "solver": {"tolerance": 1e-10}})");
    const AnalysisReport r = run_analysis(column, Settings{}, c);
    REQUIRE(r.valid);
    CHECK(r.warnings.empty());
    CHECK(r.grid.nx == 20);
    CHECK(r.grid.nz == 50);
    CHECK(r.part_voxels == 20 * 20 * 50);
    CHECK(r.layers.size() == 50);
    // Every direction in the layer sees the same 10 MPa across the layers.
    const double expected = tsai_wu_safety(c.material, Voigt{0, 0, 10.0, 0, 0, 0});
    CHECK_THAT(expected, WithinRel(31.0 / 10.0, 1e-12));
    CHECK_THAT(r.result.min_safety, WithinRel(expected, 1e-6));
    CHECK_THAT(r.result.min_peak_safety, WithinRel(expected, 1e-6)); // a uniform stress: the average is the same
    CHECK(r.result.mode[r.result.weakest] == FailureMode::BetweenLayers);
    for (const LayerSafety& l : r.layer_safety) {
        CHECK_THAT(l.min_safety, WithinRel(expected, 1e-6));
        CHECK(l.mode == FailureMode::BetweenLayers);
    }
    CHECK(r.below == std::array<std::size_t, 3>{0, 0, 0});

    // The report as JSON.
    const auto json = nlohmann::ordered_json::parse(analysis_json(r, c));
    CHECK(json.begin().key() == "version");
    CHECK(json["valid"] == true);
    CHECK(json["model"] == "solid");
    CHECK(json["material"]["id"] == "bambu-pla-basic");
    CHECK(json["voxels"]["analysed"] == 20000);
    CHECK_THAT(json["result"]["min_safety"].get<double>(), WithinRel(expected, 1e-6));
    CHECK(json["result"]["weakest"]["mode"] == "between_layers");
    CHECK(json["layers"].size() == 50);
    CHECK(json["warnings"].empty());

    // The map as VTK: the header, then the blocks of the right sizes.
    std::ostringstream vtk;
    write_analysis_vtk(vtk, r);
    const std::string text = vtk.str();
    CHECK(text.starts_with("# vtk DataFile Version 3.0\n"));
    CHECK_THAT(text, ContainsSubstring("DATASET RECTILINEAR_GRID\nDIMENSIONS 21 21 51\n"));
    const std::size_t z = text.find("Z_COORDINATES 51 double\n");
    REQUIRE(z != std::string::npos);
    std::array<char, 8> bytes{};
    std::copy_n(text.begin() + std::ptrdiff_t(z + 24 + 8), 8, bytes.begin()); // the second plane, big-endian
    if constexpr (std::endian::native == std::endian::little)
        std::reverse(bytes.begin(), bytes.end());
    CHECK_THAT(std::bit_cast<double>(bytes), WithinAbs(0.2, 1e-12));
    for (const char* field : {"SCALARS solid unsigned_char 1", "SCALARS safety float 1", "SCALARS peak_safety float 1",
                              "SCALARS mode unsigned_char 1",
                              "SCALARS von_mises float 1", "SCALARS direction float 1", "VECTORS displacement float"})
        CHECK_THAT(text, ContainsSubstring(field));
    const std::size_t header = text.find("CELL_DATA 20000\n");
    const std::size_t points = text.find("POINT_DATA 22491\nVECTORS displacement float\n");
    REQUIRE(header != std::string::npos);
    REQUIRE(points != std::string::npos);
    CHECK(text.size() == points + std::string("POINT_DATA 22491\nVECTORS displacement float\n").size() + 22491 * 12 + 1);
}

TEST_CASE("analysis: as printed, a plate held at one end and pushed at the other", "[analysis]") {
    const IndexedMesh plate = box_mesh(12.0f, 4.0f, 2.0f);
    const AnalysisCase c = parse_analysis_case(R"({
        "fixtures": [{"name": "clamp", "face": "x_min"}],
        "loads": [{"name": "push", "face": "x_max", "force": [0, 0, -5]}]})");
    const AnalysisReport r = run_analysis(plate, Settings{}, c);
    REQUIRE(r.valid);
    CHECK(r.layers.size() == 10);
    REQUIRE(r.direction.size() == r.grid.solid.size());
    // The sparse layers in the middle have gaps.
    CHECK(r.printed_voxels < r.part_voxels);
    std::size_t known = 0;
    for (float a : r.direction)
        known += std::isfinite(a);
    CHECK(known == r.printed_voxels);
    // Bending is largest at the clamp, where the stress of single voxels
    // peaks above the average over a line width and a layer.
    REQUIRE(std::isfinite(r.result.min_safety));
    CHECK(r.weakest[0] < 3.0);
    CHECK(r.result.min_peak_safety < r.result.min_safety);
    for (std::size_t v = 0; v < r.result.safety.size(); ++v)
        if (std::isfinite(r.result.safety[v]))
            CHECK(std::isfinite(r.result.peak_safety[v]));
    CHECK(r.result.elasticity.max_displacement > 0.0);
}

TEST_CASE("analysis: warnings and errors", "[analysis]") {
    const IndexedMesh column = box_mesh(4.0f, 4.0f, 4.0f);
    SECTION("a fixture beside the part holds nothing") {
        const AnalysisCase c = parse_analysis_case(R"({"model": "solid",
            "fixtures": [{"name": "far", "box": {"min": [50, 50, 0], "max": [60, 60, 1]}}],
            "loads": [{"face": "top", "force": [0, 0, -1]}]})");
        const AnalysisReport r = run_analysis(column, Settings{}, c);
        CHECK(!r.valid);
        std::string codes;
        for (const AnalysisWarning& w : r.warnings)
            codes += w.code + " ";
        CHECK_THAT(codes, ContainsSubstring("fixture_unused"));
        CHECK_THAT(codes, ContainsSubstring("loose_voxels"));
        CHECK_THAT(codes, ContainsSubstring("nothing_held"));
    }
    SECTION("a load beside the part finds nothing") {
        const AnalysisCase c = parse_analysis_case(R"({"model": "solid", "fixtures": [{"face": "bottom"}],
            "loads": [{"box": {"min": [50, 50, 0], "max": [60, 60, 1]}, "force": [0, 0, -1]}]})");
        const AnalysisReport r = run_analysis(column, Settings{}, c);
        CHECK(!r.valid);
        REQUIRE(!r.warnings.empty());
        CHECK(r.warnings.front().code == "load_not_applied");
    }
    SECTION("a heavy load breaks the part") {
        const AnalysisCase c = parse_analysis_case(R"({"model": "solid", "fixtures": [{"face": "bottom"}],
            "loads": [{"face": "top", "force": [0, 0, 1000]}]})");
        const AnalysisReport r = run_analysis(column, Settings{}, c);
        CHECK(r.valid);
        CHECK(r.result.min_safety < 1.0);
        CHECK(r.below[0] > 0);
        REQUIRE(!r.warnings.empty());
        CHECK(r.warnings.back().code == "unsafe");
    }
    SECTION("too many voxels") {
        const AnalysisCase c = parse_analysis_case(R"({"max_voxels": 1000, "fixtures": [{"face": "bottom"}],
            "loads": [{"face": "top", "force": [0, 0, -1]}]})");
        CHECK_THROWS_AS(run_analysis(column, Settings{}, c), AnalysisError);
    }
}
