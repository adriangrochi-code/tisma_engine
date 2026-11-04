#include "config/analysis_case.hpp"
#include "config/settings.hpp"
#include "fea/verdict.hpp"
#include "fea/analysis.hpp"
#include "slice/placement.hpp"
#include "fea/materials.hpp"
#include "fea/zone.hpp"
#include "io/ply.hpp"
#include "io/vtk.hpp"
#include "mesh/indexed_mesh.hpp"
#include "mesh/mesh.hpp"
#include "mesh/shapes.hpp"
#include "project/engineering_case.hpp"
#include "report/analysis_json.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <limits>
#include <random>
#include <utility>
#include <vector>
#include <sstream>
#include <string>

using namespace tisma;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

// The settings the analysis tests were measured with: 0.4 mm beads (walls.width
// is automatic otherwise, 1.125 nozzles).
Settings beads_04() {
    Settings s;
    s.wall_width = 0.4;
    s.wall_width_auto = false;
    return s;
}

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
        "resolution_check": false,
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
    CHECK(!c.resolution_check);
    CHECK(d.resolution_check);
}

TEST_CASE("analysis case: errors name the key", "[analysis]") {
    CHECK_THAT(error_of("[1]"), ContainsSubstring("top level"));
    CHECK_THAT(error_of("{\"x\": 1}"), ContainsSubstring("case.x: unknown key"));
    CHECK_THAT(error_of("{\"material\": \"steel\"}"), ContainsSubstring("unknown material 'steel'; known: song2017-pla"));
    CHECK_THAT(error_of("{\"material\": {\"zt\": -1}}"), ContainsSubstring("material.zt: must be greater than 0"));
    CHECK_THAT(error_of("{\"material\": {\"nu12\": 0.9, \"nu13\": 0.9, \"nu23\": 0.9}}"), ContainsSubstring("not a valid material"));
    CHECK_THAT(error_of("{\"quality\": \"ultra\"}"), ContainsSubstring("quality: must be one of extreme, high, normal"));
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
    CHECK_THAT(error_of("{\"resolution_check\": 1}"), ContainsSubstring("resolution_check: must be true or false"));
}

TEST_CASE("analysis: voxels from the quality and the line width", "[analysis]") {
    Settings s = beads_04(); // 0.4 mm lines, 0.2 mm layers
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
    const AnalysisReport r = run_analysis(column, beads_04(), c);
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
    // A uniform stress does not depend on the voxel size.
    REQUIRE(r.resolution.done);
    CHECK(r.resolution.voxel == 0.4);
    CHECK(r.resolution.rows_per_layer == 1);
    CHECK_THAT(r.resolution.change, WithinAbs(0.0, 1e-6));
    CHECK_THAT(r.resolution.extrapolated_min_safety, WithinRel(r.result.min_safety, 1e-6));

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
    CHECK(json["resolution"]["checked"] == true);
    CHECK(json["resolution"]["coarse_voxel"] == 0.4);

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
    const AnalysisReport r = run_analysis(plate, beads_04(), c);
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
        const AnalysisReport r = run_analysis(column, beads_04(), c);
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
        const AnalysisReport r = run_analysis(column, beads_04(), c);
        CHECK(!r.valid);
        REQUIRE(!r.warnings.empty());
        CHECK(r.warnings.front().code == "load_not_applied");
    }
    SECTION("a heavy load breaks the part") {
        const AnalysisCase c = parse_analysis_case(R"({"model": "solid", "fixtures": [{"face": "bottom"}],
            "loads": [{"face": "top", "force": [0, 0, 1000]}]})");
        const AnalysisReport r = run_analysis(column, beads_04(), c);
        CHECK(r.valid);
        CHECK(r.result.min_safety < 1.0);
        CHECK(r.below[0] > 0);
        bool unsafe = false;
        for (const AnalysisWarning& w : r.warnings)
            unsafe = unsafe || w.code == "unsafe";
        CHECK(unsafe);
    }
    SECTION("a force past small strains says so (the spec team's BH12)") {
        const AnalysisCase c = parse_analysis_case(R"({"model": "solid", "fixtures": [{"face": "bottom"}],
            "loads": [{"face": "top", "force": [1e30, 0, 0]}]})");
        const AnalysisReport r = run_analysis(column, beads_04(), c);
        bool large = false;
        for (const AnalysisWarning& w : r.warnings)
            large = large || w.code == "large_displacement";
        CHECK(large);
    }
    SECTION("a voxel as big as the part says to make it smaller (BH12)") {
        const AnalysisCase c = parse_analysis_case(R"({"model": "solid", "voxel": 50, "fixtures": [{"face": "bottom"}],
            "loads": [{"face": "top", "force": [0, 0, -1]}]})");
        const AnalysisReport r = run_analysis(column, beads_04(), c);
        std::string messages;
        for (const AnalysisWarning& w : r.warnings)
            messages += w.message + "\n";
        CHECK_THAT(messages, ContainsSubstring("touches no part of the model: the model is only 0 voxels of 50 mm, try a "
                                               "smaller voxel"));
    }
    SECTION("too many voxels") {
        const AnalysisCase c = parse_analysis_case(R"({"max_voxels": 1000, "fixtures": [{"face": "bottom"}],
            "loads": [{"face": "top", "force": [0, 0, -1]}]})");
        CHECK_THROWS_AS(run_analysis(column, beads_04(), c), AnalysisError);
    }
}

TEST_CASE("analysis: estimate, progress and the voxel mesh", "[analysis]") {
    const IndexedMesh column = box_mesh(4.0f, 4.0f, 10.0f);
    const AnalysisCase c = parse_analysis_case(R"({"model": "solid",
        "fixtures": [{"face": "bottom"}], "loads": [{"face": "top", "force": [0, 0, -10]}]})");

    const AnalysisEstimate e = estimate_analysis(column, beads_04(), c);
    // The cheap count of the box agrees with the grid the estimate built.
    CHECK(analysis_grid_voxels(column, beads_04(), c) == e.grid.solid.size());
    AnalysisCase as_printed = c;
    as_printed.model = AnalysisModel::AsPrinted;
    as_printed.quality = AnalysisQuality::High;
    CHECK(analysis_grid_voxels(column, beads_04(), as_printed) ==
          estimate_analysis(column, beads_04(), as_printed).grid.solid.size());
    CHECK(e.part_voxels == 20000);
    CHECK(e.printed_voxels == 20000);
    CHECK(e.rows_per_layer == 1);
    CHECK_THAT(e.memory_mb, WithinRel(10.0 + 540.0 * 20000 / 1e6, 1e-12));
    CHECK(e.seconds > 0.0);

    std::vector<std::pair<std::string, double>> steps;
    const AnalysisReport r = run_analysis(column, beads_04(), c, nullptr,
                                          [&](std::string_view stage, double done) { steps.emplace_back(stage, done); });
    REQUIRE(r.valid);
    REQUIRE(steps.size() >= 5);
    CHECK(steps.front() == std::pair<std::string, double>{"slicing", 0.0});
    CHECK(steps.back() == std::pair<std::string, double>{"done", 1.0});
    for (std::size_t i = 1; i < steps.size(); ++i)
        CHECK(steps[i].second >= steps[i - 1].second);
    std::vector<std::string> stages;
    for (const auto& [stage, done] : steps)
        if (stages.empty() || stages.back() != stage)
            stages.push_back(stage);
    CHECK(stages == std::vector<std::string>{"slicing", "voxels", "solving", "summary", "resolution", "done"});

    // The surface of a 20 x 20 x 50 block of voxels: 2 x 400 + 4 x 1000 faces.
    std::ostringstream ply;
    write_voxel_mesh_ply(ply, r);
    const std::string text = ply.str();
    CHECK(text.starts_with("ply\nformat binary_little_endian 1.0\n"));
    CHECK_THAT(text, ContainsSubstring("comment units mm\ncomment voxels_analysed 20000\ncomment grid_voxels 20 20 50\n"
                                       "comment voxel_size_mm 0.2 0.2\ncomment grid_origin_mm 0 0 0\n"
                                       "comment rows_per_layer 1\ncomment z_planes_mm 0 0.2 0.4 "));
    CHECK_THAT(text, ContainsSubstring("element vertex 19200\n"));
    CHECK_THAT(text, ContainsSubstring("element face 4800\n"));
    const std::size_t body = text.find("end_header\n") + 11;
    CHECK(text.size() - body == 19200 * 15 + 4800 * (1 + 16 + 4 + 1));

    CHECK(safety_colour(0.5) == std::array<std::uint8_t, 3>{220, 40, 40});
    CHECK(safety_colour(1.2) == std::array<std::uint8_t, 3>{245, 140, 30});
    CHECK(safety_colour(1.7) == std::array<std::uint8_t, 3>{240, 210, 40});
    CHECK(safety_colour(3.0) == std::array<std::uint8_t, 3>{60, 170, 75});
    CHECK(safety_colour(std::numeric_limits<double>::infinity()) == std::array<std::uint8_t, 3>{190, 190, 190});
}

TEST_CASE("analysis: how much the result depends on the voxel size", "[analysis]") {
    const IndexedMesh plate = box_mesh(12.0f, 4.0f, 2.0f);
    SECTION("a bent plate with coarse voxels: a warning and the extrapolated value") {
        const AnalysisCase c = parse_analysis_case(R"({"model": "solid", "voxel": 0.5, "rows_per_layer": 2,
            "fixtures": [{"face": "x_min"}], "loads": [{"face": "x_max", "force": [0, 0, -5]}]})");
        const AnalysisReport r = run_analysis(plate, beads_04(), c);
        REQUIRE(r.valid);
        REQUIRE(r.resolution.done);
        CHECK(r.resolution.voxel == 1.0);
        CHECK(r.resolution.rows_per_layer == 1);
        INFO("fine " << r.result.min_safety << ", coarse " << r.resolution.min_safety);
        CHECK(r.resolution.min_safety > r.result.min_safety);
        CHECK_THAT(r.resolution.change, WithinRel((r.resolution.min_safety - r.result.min_safety) / r.result.min_safety, 1e-12));
        CHECK_THAT(r.resolution.extrapolated_min_safety, WithinRel(2.0 * r.result.min_safety - r.resolution.min_safety, 1e-12));
        bool warned = false;
        for (const AnalysisWarning& w : r.warnings)
            warned = warned || w.code == "resolution_sensitive";
        CHECK(warned == (r.resolution.change > 0.05));
    }
    SECTION("as printed at normal quality the coarser voxels cannot hold the gaps") {
        const AnalysisCase c = parse_analysis_case(R"({"fixtures": [{"face": "x_min"}],
            "loads": [{"face": "x_max", "force": [0, 0, -5]}]})");
        const AnalysisReport r = run_analysis(plate, beads_04(), c);
        CHECK(r.valid);
        CHECK(!r.resolution.done);
        CHECK_THAT(r.resolution.reason, ContainsSubstring("gaps of sparse infill"));
    }
    SECTION("not asked for") {
        const AnalysisCase c = parse_analysis_case(R"({"model": "solid", "resolution_check": false,
            "fixtures": [{"face": "x_min"}], "loads": [{"face": "x_max", "force": [0, 0, -5]}]})");
        const AnalysisReport r = run_analysis(plate, beads_04(), c);
        CHECK(!r.resolution.done);
        CHECK(r.resolution.reason == "not asked for");
    }
}

TEST_CASE("analysis: the voxels stay in the model's coordinates", "[analysis]") {
    // A block away from the origin: the voxel grid and its mesh start where
    // the model does, with no move.
    IndexedMesh block = box_mesh(2.0f, 2.0f, 1.0f);
    for (Vec3f& v : block.vertices)
        v.x += 10.0f, v.y -= 5.0f, v.z += 3.0f;
    const AnalysisCase c = parse_analysis_case(R"({"model": "solid", "resolution_check": false,
        "fixtures": [{"face": "bottom"}], "loads": [{"face": "top", "force": [0, 0, -1]}]})");
    const AnalysisReport r = run_analysis(block, beads_04(), c);
    REQUIRE(r.valid);
    CHECK(r.grid.x0 == 10.0);
    CHECK(r.grid.y0 == -5.0);
    CHECK(r.grid.z.front() == 3.0);
    CHECK_THAT(r.grid.z.back(), WithinAbs(4.0, 1e-9));
    std::ostringstream ply;
    write_voxel_mesh_ply(ply, r);
    const std::string text = ply.str();
    CHECK_THAT(text, ContainsSubstring("comment grid_origin_mm 10 -5 3\n"));
    // Every vertex inside the model's box.
    const std::size_t body = text.find("end_header\n") + 11;
    const std::size_t vertices = std::size_t(std::stoul(text.substr(text.find("element vertex ") + 15)));
    float lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f};
    for (std::size_t i = 0; i < vertices; ++i)
        for (int a = 0; a < 3; ++a) {
            std::array<char, 4> bytes{};
            std::copy_n(text.begin() + std::ptrdiff_t(body + i * 15 + std::size_t(a) * 4), 4, bytes.begin());
            if constexpr (std::endian::native == std::endian::big)
                std::reverse(bytes.begin(), bytes.end());
            const float v = std::bit_cast<float>(bytes);
            lo[a] = std::min(lo[a], v), hi[a] = std::max(hi[a], v);
        }
    CHECK(lo[0] == 10.0f);
    CHECK(hi[0] == 12.0f);
    CHECK(lo[1] == -5.0f);
    CHECK(hi[1] == -3.0f);
    CHECK(lo[2] == 3.0f);
    CHECK_THAT(hi[2], WithinAbs(4.0, 1e-5));
}

TEST_CASE("analysis: conditions marked on the model hold and push as the part's faces do (formats.md, rule 11 bis)",
          "[analysis][engineering]") {
    const IndexedMesh plate = box_mesh(12.0f, 4.0f, 2.0f);
    TriangleMesh triangles;
    for (const auto& t : plate.triangles)
        triangles.add_triangle(plate.vertices[t[0]], plate.vertices[t[1]], plate.vertices[t[2]]);
    // The same plate held by its x_min face and pushed down on its x_max face,
    // once by the case's faces and once by the triangles marked on the mesh.
    const AnalysisCase by_faces = parse_analysis_case(R"({
        "fixtures": [{"name": "clamp", "face": "x_min"}],
        "loads": [{"name": "push", "face": "x_max", "force": [0, 0, -5]}]})");
    ProjectEngineering e;
    EngineeringCondition fixed, push;
    fixed.kind = ConditionKind::FixedZone;
    push.kind = ConditionKind::ForceZone;
    push.newtons = 5.0;
    push.direction = ForceDirection::Axis;
    push.axis = -3;
    for (std::uint32_t t = 0; t < triangles.triangle_count(); ++t) {
        bool at_min = true, at_max = true;
        for (int k = 0; k < 3; ++k)
            at_min &= triangles.corner(t, k).x == 0.0f, at_max &= triangles.corner(t, k).x == 12.0f;
        if (at_min)
            fixed.triangles.push_back(t);
        if (at_max)
            push.triangles.push_back(t);
    }
    REQUIRE(fixed.triangles.size() == 2);
    REQUIRE(push.triangles.size() == 2);
    e.conditions = {fixed, push};
    AnalysisCase by_marks = by_faces;
    by_marks.fixtures.clear();
    by_marks.loads.clear();
    CHECK(add_conditions(by_marks, e, triangles).empty());
    REQUIRE(by_marks.fixtures.size() == 1);
    REQUIRE(by_marks.loads.size() == 1);
    CHECK(by_marks.loads[0].force[2] == -5.0);
    const AnalysisReport a = run_analysis(plate, beads_04(), by_faces);
    const AnalysisReport b = run_analysis(plate, beads_04(), by_marks);
    REQUIRE(a.valid);
    REQUIRE(b.valid);
    INFO(a.result.elasticity.max_displacement << " " << b.result.elasticity.max_displacement);
    CHECK(b.result.elasticity.unapplied_loads == 0);
    CHECK(std::abs(b.result.elasticity.max_displacement / a.result.elasticity.max_displacement - 1.0) < 0.15);
    // A point pushed at the free end's middle, along its normal (into the
    // part, -x): the plate is squeezed, far less than bent.
    EngineeringCondition dot;
    dot.kind = ConditionKind::ForcePoint;
    dot.point = {12.0, 2.0, 1.0};
    dot.normal = {1.0, 0.0, 0.0};
    dot.radius = 1.0;
    dot.newtons = 5.0;
    e.conditions = {fixed, dot};
    AnalysisCase squeeze = by_faces;
    squeeze.fixtures.clear();
    squeeze.loads.clear();
    CHECK(add_conditions(squeeze, e, triangles).empty());
    REQUIRE(squeeze.loads.size() == 1);
    CHECK(squeeze.loads[0].force[0] == -5.0);
    const AnalysisReport s = run_analysis(plate, beads_04(), squeeze);
    REQUIRE(s.valid);
    CHECK(s.result.elasticity.unapplied_loads == 0);
    CHECK(s.result.elasticity.max_displacement < 0.2 * a.result.elasticity.max_displacement);
}

TEST_CASE("a marked zone is exactly the points within reach of its triangles, at any size", "[analysis]") {
    // A large face (two triangles of 200 mm) and a fine one (a fan of small
    // triangles): near() against the distance to each triangle, by brute force.
    ZoneShape big, fine;
    big.triangles.push_back({{{0, 0, 0}, {200, 0, 0}, {200, 200, 0}}});
    big.triangles.push_back({{{0, 0, 0}, {200, 200, 0}, {0, 200, 0}}});
    for (int i = 0; i < 60; ++i) {
        const double a0 = 2.0 * 3.14159265358979 * i / 60, a1 = 2.0 * 3.14159265358979 * (i + 1) / 60;
        fine.triangles.push_back({{{0, 0, 1}, {5 * std::cos(a0), 5 * std::sin(a0), 0}, {5 * std::cos(a1), 5 * std::sin(a1), 0}}});
    }
    std::mt19937 rng(7);
    for (const ZoneShape* shape : {&big, &fine}) {
        const SurfaceZone zone(*shape, 0.1, 0.2);
        const double span = shape == &big ? 210.0 : 12.0;
        std::uniform_real_distribution<double> xy(-5.0, span), z(-0.5, 1.5);
        int inside = 0;
        for (int n = 0; n < 20000; ++n) {
            const std::array<double, 3> p{xy(rng), xy(rng), z(rng)};
            bool expected = false;
            for (const Triangle3& t : shape->triangles) {
                const std::array<double, 3> c = closest_on_triangle(p, t);
                expected |= (p[0] - c[0]) * (p[0] - c[0]) + (p[1] - c[1]) * (p[1] - c[1]) + (p[2] - c[2]) * (p[2] - c[2]) <=
                            0.04;
            }
            inside += expected;
            REQUIRE(zone.near(p[0], p[1], p[2]) == expected);
        }
        CHECK(inside > 0);
    }
}

TEST_CASE("verdict: material families, the temperature factor, the verdict and what to try (E10)", "[analysis]") {
    REQUIRE(family_of("bambu-pla-basic"));
    CHECK(family_of("bambu-pla-basic")->name == "PLA");
    CHECK(family_of("PLA+")->name == "PLA");
    CHECK(family_of("bambu-pla-cf")->name == "PLA");
    CHECK(family_of("PETG-CF")->name == "PETG");
    CHECK(family_of("bambu-pa6-cf")->name == "PA-CF");
    CHECK(family_of("bambu-paht-cf")->name == "PA-CF");
    CHECK(family_of("PA12")->name == "PA");
    CHECK(family_of("PC")->name == "PC");
    CHECK(family_of("TPU 95A")->elastomer);
    CHECK(family_of("cnckitchen-abs-open")->name == "ABS");
    CHECK(family_of("wood") == nullptr);

    // f(T): 1 up to 23 °C, 0.5 at the service maximum, 0.02 fifteen degrees above.
    const MaterialFamily& pla = *family_of("PLA");
    CHECK(temperature_factor(pla, 20.0) == 1.0);
    CHECK_THAT(temperature_factor(pla, 55.0), Catch::Matchers::WithinAbs(0.5, 1e-12));
    CHECK_THAT(temperature_factor(pla, 39.0), Catch::Matchers::WithinAbs(0.75, 1e-12));
    CHECK_THAT(temperature_factor(pla, 70.0), Catch::Matchers::WithinAbs(0.02, 1e-12));
    CHECK(temperature_factor(pla, 130.0) == 0.02);

    VerdictInput in;
    in.family = &pla;
    in.required_safety = 2.0;
    in.min_safety = 3.0;
    CHECK(structural_verdict(in) == Verdict::Holds);
    in.min_safety = 1.5;
    CHECK(structural_verdict(in) == Verdict::LowMargin);
    in.min_safety = 0.8;
    CHECK(structural_verdict(in) == Verdict::Overloaded);
    in.min_safety = 3.0;
    in.limit_exceeded = true;
    CHECK(structural_verdict(in) == Verdict::TooFlexible);
    in.limit_exceeded = false;
    in.family = family_of("TPU");
    CHECK(structural_verdict(in) == Verdict::NotApplicable);
    CHECK(suggest_materials(in).empty());

    // E10: PLA at 130 °C is over temperature, and PEI is among what to try;
    // nothing suggested serves below 130 °C.
    in.family = &pla;
    in.working_temperature = 130.0;
    in.min_safety = 0.1; // 5 at 23 °C: f(130) is 0.02
    CHECK(structural_verdict(in) == Verdict::OverTemperature);
    const std::vector<MaterialSuggestion> try_these = suggest_materials(in);
    REQUIRE_FALSE(try_these.empty());
    CHECK(std::any_of(try_these.begin(), try_these.end(), [](const MaterialSuggestion& s) { return s.family->name == "PEI"; }));
    for (const MaterialSuggestion& s : try_these) {
        CHECK(s.family->max_service >= 130.0);
        CHECK(s.safety >= 2.0);
    }

    // The mass: a 10 mm cube of PLA, 1.24 g.
    VoxelGrid grid;
    grid.nx = grid.ny = grid.nz = 10;
    grid.z.resize(11);
    for (int k = 0; k <= 10; ++k)
        grid.z[std::size_t(k)] = double(k);
    grid.solid.assign(1000, 1);
    CHECK_THAT(voxel_mass_grams(grid, pla.density), Catch::Matchers::WithinRel(1.24, 1e-12));
}

TEST_CASE("structural report: EP4 and EP6, a bar fixed at one end, the verdict, the mass and the map", "[analysis][engineering]") {
    // A bar of 40 x 8 x 8 mm (EP4 scaled down to keep the test quick), PLA.
    const IndexedMesh bar = box_mesh(40.0f, 8.0f, 8.0f);
    TriangleMesh triangles;
    for (const auto& t : bar.triangles)
        triangles.add_triangle(bar.vertices[t[0]], bar.vertices[t[1]], bar.vertices[t[2]]);
    ProjectEngineering e;
    EngineeringCondition fixed, tip, spread;
    fixed.kind = ConditionKind::FixedZone;
    spread.kind = ConditionKind::ForceZone;
    for (std::uint32_t t = 0; t < triangles.triangle_count(); ++t) {
        bool at_min = true, top = true;
        for (int k = 0; k < 3; ++k)
            at_min &= triangles.corner(t, k).x == 0.0f, top &= triangles.corner(t, k).z == 8.0f;
        if (at_min)
            fixed.triangles.push_back(t);
        if (top)
            spread.triangles.push_back(t);
    }
    tip.kind = ConditionKind::ForcePoint;
    tip.point = {40.0, 4.0, 4.0};
    tip.normal = {1.0, 0.0, 0.0};
    tip.radius = 3.0;
    tip.kilograms = 5.0 / 9.81; // typed in kg: 5 N
    tip.newtons = 5.0;
    tip.direction = ForceDirection::Axis;
    tip.axis = -3;
    spread.newtons = 5.0;
    spread.direction = ForceDirection::Axis;
    spread.axis = -3;
    e.conditions = {fixed, tip};
    // Fast: the bar solid in a coarse grid (the normal grid took 26 minutes
    // under the address sanitizer); what is checked does not need more.
    e.structural.quality = "fast";
    std::vector<std::string> notes;
    const AnalysisCase c = engineering_analysis_case(e, triangles, {}, notes, "PLA");
    REQUIRE(c.fixtures.size() == 1);
    REQUIRE(c.loads.size() == 1);
    const AnalysisReport r = run_analysis(bar, beads_04(), c);
    REQUIRE(r.valid);
    const nlohmann::json j = nlohmann::json::parse(structural_report_json(r, c, e));
    REQUIRE(j.contains("verdict"));
    CHECK(j["verdict"]["family"] == "PLA");
    CHECK(j["verdict"]["name"] == "holds"); // 5 N on a 8 mm PLA bar
    CHECK(j["verdict"]["required_safety"] == 2.0);
    // Mass: no more than the solid bar, 40 x 8 x 8 mm of PLA = 3.17 g.
    const double mass = j["verdict"]["mass_g"].get<double>();
    CHECK(mass > 0.5);
    CHECK(mass <= 40.0 * 8.0 * 8.0 * 1.24 / 1000.0 * 1.001);
    // The map: the voxels' outside, two triangles per open face, each with its
    // voxel's factor; the smallest on the outside is the part's smallest (a
    // bent bar is weakest at its surface).
    const VoxelSurface map = voxel_surface(r.grid, r.result.safety);
    REQUIRE_FALSE(map.value.empty());
    CHECK(map.corners.size() == 3 * map.value.size());
    const double lowest = *std::min_element(map.value.begin(), map.value.end());
    CHECK_THAT(lowest, WithinRel(r.result.min_safety, 1e-9));
    // Two voxels side by side: ten open faces.
    VoxelGrid two;
    two.nx = 2, two.ny = 1, two.nz = 1;
    two.z = {0.0, 1.0};
    two.solid = {1, 1};
    CHECK(voxel_surface(two, {1.0, 2.0}).value.size() == 20);
    CHECK(voxel_surface(two, {1.0, 2.0}).displacement.empty());
    // With the nodes' displacement (here u = (0, 0, z)), each corner carries its own.
    std::vector<double> lift(3 * 3 * 2 * 2, 0.0);
    for (int k = 0; k < 2; ++k)
        for (int y = 0; y < 2; ++y)
            for (int i = 0; i < 3; ++i)
                lift[3 * std::size_t((k * 2 + y) * 3 + i) + 2] = double(k);
    const VoxelSurface lifted = voxel_surface(two, {1.0, 2.0}, &lift);
    REQUIRE(lifted.displacement.size() == lifted.corners.size());
    for (std::size_t n = 0; n < lifted.corners.size(); ++n)
        CHECK(lifted.displacement[n].z == lifted.corners[n].z);
    // The bar's corners: the largest movement on its outside is the bar's largest.
    const VoxelSurface bent = voxel_surface(r.grid, r.result.safety, &r.result.elasticity.displacement);
    REQUIRE(bent.displacement.size() == bent.corners.size());
    double most = 0.0;
    for (const Vec3f& d : bent.displacement)
        most = std::max(most, std::sqrt(double(d.x) * d.x + double(d.y) * d.y + double(d.z) * d.z));
    CHECK_THAT(most, WithinRel(r.result.elasticity.max_displacement, 1e-5));
    // EP6: the same force spread on the top face bends the bar less.
    e.conditions = {fixed, spread};
    const AnalysisCase c6 = engineering_analysis_case(e, triangles, {}, notes, "PLA");
    const AnalysisReport r6 = run_analysis(bar, beads_04(), c6);
    REQUIRE(r6.valid);
    CHECK(r6.result.elasticity.max_displacement < r.result.elasticity.max_displacement);
    // A displacement limit on the free end's face: tighter than what it
    // moves, «too flexible»; looser, it holds.
    EngineeringCondition end;
    end.kind = ConditionKind::Limit;
    for (std::uint32_t t = 0; t < triangles.triangle_count(); ++t) {
        bool at_max = true;
        for (int k = 0; k < 3; ++k)
            at_max &= triangles.corner(t, k).x == 40.0f;
        if (at_max)
            end.triangles.push_back(t);
    }
    REQUIRE(end.triangles.size() == 2);
    const double moved = r.result.elasticity.max_displacement;
    end.limit = 0.5 * moved;
    e.conditions = {fixed, tip, end};
    const std::vector<LimitCheck> checks = check_limits(e, triangles, r);
    REQUIRE(checks.size() == 1);
    CHECK(checks[0].exceeded);
    CHECK(checks[0].displacement_mm > 0.5 * moved);
    CHECK(checks[0].displacement_mm <= moved * 1.0001);
    nlohmann::json limited = nlohmann::json::parse(structural_report_json(r, c, e, {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}, &triangles));
    CHECK(limited["verdict"]["name"] == "too_flexible");
    CHECK(limited["verdict"]["limits_checked"] == true);
    e.conditions[2].limit = 2.0 * moved;
    limited = nlohmann::json::parse(structural_report_json(r, c, e, {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}, &triangles));
    CHECK(limited["verdict"]["name"] == "holds");
    // As a percentage of the largest size (40 mm).
    e.conditions[2].limit = 100.0 * 0.5 * moved / 40.0;
    e.conditions[2].limit_percent = true;
    CHECK(check_limits(e, triangles, r)[0].exceeded);
    // Hot: at 50 °C the same bar is less stiff (f(50) = 0.58 for PLA).
    e.conditions = {fixed, tip};
    e.working_temperature = 50.0;
    const AnalysisCase hot = engineering_analysis_case(e, triangles, {}, notes, "PLA");
    CHECK_THAT(hot.material.e1, WithinRel(c.material.e1 * temperature_factor(*family_of("PLA"), 50.0), 1e-12));
    // A suggested family tried (rule 12): PEI at 130 °C takes the table's
    // values times its own f(T), and the report names it.
    e.working_temperature = 130.0;
    e.overrides.material = family_material_id(*family_of("PEI"));
    const AnalysisCase pei = engineering_analysis_case(e, triangles, {}, notes, "PLA");
    CHECK(pei.material_id == "family:PEI");
    const double f130 = temperature_factor(*family_of("PEI"), 130.0);
    CHECK_THAT(pei.material.e1, WithinRel(2200.0 * f130, 1e-12));
    CHECK_THAT(pei.material.e3, WithinRel(2000.0 * f130, 1e-12));
    CHECK_THAT(pei.material.zt, WithinRel(40.0 * f130, 1e-12));
    const AnalysisReport rp = run_analysis(bar, beads_04(), pei);
    REQUIRE(rp.valid);
    const nlohmann::json jp = nlohmann::json::parse(structural_report_json(rp, pei, e));
    CHECK(jp["material"]["name"] == "PEI");
    CHECK(jp["verdict"]["family"] == "PEI");
    CHECK(jp["verdict"]["name"] == "holds"); // under its 150 °C service maximum
    // The adhesion it was given (at the reference: nothing changed) and the
    // band its uncertainty makes of the safety factor (±50 % for PEI).
    CHECK(jp["adhesion"]["modelled"] == true);
    CHECK(jp["adhesion"]["z_strength_factor"].get<double>() == 1.0);
    CHECK(jp["adhesion"]["weakest_mode"].is_string());
    if (jp["adhesion"]["weakest_mode"] == "between_layers")
        CHECK_THAT(jp["adhesion"]["min_safety_low"].get<double>(), WithinRel(0.5 * rp.result.min_safety, 1e-9));
    CHECK(family_of_material_id("bambu-pla-basic") == nullptr);
    // Rápida: the whole bar solid in about 20 000 voxels, and still the same
    // verdict at 23 °C.
    e.overrides.material.reset();
    e.working_temperature = 23.0;
    e.structural.quality = "fast";
    const AnalysisCase fast = engineering_analysis_case(e, triangles, {}, notes, "PLA");
    CHECK(fast.model == AnalysisModel::Solid);
    CHECK(fast.resolution_check); // the band takes its voxel error
    const std::size_t voxels = analysis_grid_voxels(bar, beads_04(), fast);
    INFO(voxels);
    CHECK(voxels > 10000);
    CHECK(voxels < 40000);
    const AnalysisReport rf = run_analysis(bar, beads_04(), fast);
    REQUIRE(rf.valid);
    CHECK(nlohmann::json::parse(structural_report_json(rf, fast, e))["verdict"]["name"] == "holds");
}

TEST_CASE("BH24: a quality finer than the voxel limit allows takes the finest voxels that fit", "[analysis]") {
    TriangleMesh box = make_box(20, 20, 10);
    const IndexedMesh mesh = weld_exact(box);
    Settings s;
    s.layer_height = 0.2;
    s.wall_width = 0.42;
    AnalysisCase c;
    c.quality = AnalysisQuality::Extreme;
    c.max_voxels = 200000;
    const std::size_t asked = analysis_grid_voxels(mesh, s, c);
    REQUIRE(asked > c.max_voxels);
    CHECK(fit_voxel_limit(c, mesh, s));
    const std::size_t fitted = analysis_grid_voxels(mesh, s, c);
    INFO(asked << " -> " << fitted << " at " << c.voxel << " mm, " << c.rows_per_layer << " rows");
    CHECK(fitted <= c.max_voxels);
    CHECK(fitted > c.max_voxels / 3); // not much coarser than it needs
    // Within the limit: nothing changes.
    AnalysisCase normal;
    CHECK_FALSE(fit_voxel_limit(normal, mesh, s));
    CHECK(normal.voxel == 0.0);
}


TEST_CASE("structural optimization rule 2: the lowest uniform infill that holds, by bisection", "[analysis][engineering]") {
    const IndexedMesh bar = box_mesh(16.0f, 4.0f, 4.0f);
    TriangleMesh triangles;
    for (const auto& t : bar.triangles)
        triangles.add_triangle(bar.vertices[t[0]], bar.vertices[t[1]], bar.vertices[t[2]]);
    ProjectEngineering e;
    EngineeringCondition fixed, tip;
    fixed.kind = ConditionKind::FixedZone;
    for (std::uint32_t t = 0; t < triangles.triangle_count(); ++t) {
        bool at_min = true;
        for (int k = 0; k < 3; ++k)
            at_min &= triangles.corner(t, k).x == 0.0f;
        if (at_min)
            fixed.triangles.push_back(t);
    }
    tip.kind = ConditionKind::ForcePoint;
    tip.point = {16.0, 2.0, 2.0};
    tip.normal = {1.0, 0.0, 0.0};
    tip.radius = 2.0;
    tip.newtons = 10.5;
    tip.direction = ForceDirection::Axis;
    tip.axis = -3;
    e.conditions = {fixed, tip};
    // Rápida analyses the part solid: nothing to search.
    e.structural.quality = "fast";
    std::vector<std::string> notes;
    const AnalysisCase solid = engineering_analysis_case(e, triangles, {}, notes, "PLA");
    CHECK(lightest_infill(bar, triangles, beads_04(), solid, e).solid);
    e.structural.quality = "normal";
    const AnalysisCase c = engineering_analysis_case(e, triangles, {}, notes, "PLA");
    const Settings s = beads_04();
    double last = -1.0;
    const LightestInfill best = lightest_infill(bar, triangles, s, c, e, [&](double done) {
        CHECK(done >= last); // the progress only grows
        last = done;
    });
    CHECK(best.analyses <= 8);
    CHECK(last == 1.0);
    REQUIRE(best.found);
    // It holds there and not one percent below.
    Settings at = s;
    at.infill_density = best.density;
    CHECK(report_verdict(run_analysis(bar, at, c), c, e, triangles) == Verdict::Holds);
    if (best.density > 0) {
        at.infill_density = best.density - 1;
        CHECK(report_verdict(run_analysis(bar, at, c), c, e, triangles) != Verdict::Holds);
    }
    // A load no infill carries: not found, with its factor.
    e.conditions[1].newtons = 20000.0;
    const AnalysisCase heavy = engineering_analysis_case(e, triangles, {}, notes, "PLA");
    const LightestInfill none = lightest_infill(bar, triangles, s, heavy, e);
    CHECK_FALSE(none.found);
    CHECK(none.analyses == 1);
    CHECK(none.min_safety < 2.0);
}

TEST_CASE("analysis with modifiers: a zone's own infill is printed and analysed (structural_optimization.md, rule 3)",
          "[analysis][engineering]") {
    const IndexedMesh bar = box_mesh(24.0f, 6.0f, 6.0f);
    TriangleMesh triangles;
    for (const auto& t : bar.triangles)
        triangles.add_triangle(bar.vertices[t[0]], bar.vertices[t[1]], bar.vertices[t[2]]);
    ProjectEngineering e;
    EngineeringCondition fixed, tip;
    fixed.kind = ConditionKind::FixedZone;
    for (std::uint32_t t = 0; t < triangles.triangle_count(); ++t) {
        bool at_min = true;
        for (int k = 0; k < 3; ++k)
            at_min &= triangles.corner(t, k).x == 0.0f;
        if (at_min)
            fixed.triangles.push_back(t);
    }
    tip.kind = ConditionKind::ForcePoint;
    tip.point = {24.0, 3.0, 3.0};
    tip.normal = {1.0, 0.0, 0.0};
    tip.radius = 3.0;
    tip.newtons = 20.0;
    tip.direction = ForceDirection::Axis;
    tip.axis = -3;
    e.conditions = {fixed, tip};
    e.structural.quality = "normal";
    std::vector<std::string> notes;
    const AnalysisCase c = engineering_analysis_case(e, triangles, {}, notes, "PLA");
    Settings s = beads_04();
    s.infill_density = 0.0;
    const AnalysisReport empty = run_analysis(bar, s, c);
    // The fixed half, where the bending is largest, filled by a modifier.
    const IndexedMesh half = box_mesh(12.0f, 6.0f, 6.0f);
    TriangleMesh zone;
    for (const auto& t : half.triangles)
        zone.add_triangle(half.vertices[t[0]], half.vertices[t[1]], half.vertices[t[2]]);
    const std::vector<MeshModifier> mods{{"zone", 0, zone, R"({"infill": {"density": 100}})"}};
    const AnalysisReport filled = run_analysis(bar, s, c, nullptr, {}, nullptr, &mods);
    REQUIRE(empty.valid);
    REQUIRE(filled.valid);
    CHECK(filled.result.min_safety > 1.2 * empty.result.min_safety);
    CHECK(filled.result.elasticity.max_displacement < empty.result.elasticity.max_displacement);
}

TEST_CASE("infill zones: denser cells where the stress is higher (structural_optimization.md, rule 3)",
          "[analysis][engineering]") {
    const IndexedMesh bar = box_mesh(24.0f, 6.0f, 6.0f);
    TriangleMesh triangles;
    for (const auto& t : bar.triangles)
        triangles.add_triangle(bar.vertices[t[0]], bar.vertices[t[1]], bar.vertices[t[2]]);
    ProjectEngineering e;
    EngineeringCondition fixed, tip;
    fixed.kind = ConditionKind::FixedZone;
    for (std::uint32_t t = 0; t < triangles.triangle_count(); ++t) {
        bool at_min = true;
        for (int k = 0; k < 3; ++k)
            at_min &= triangles.corner(t, k).x == 0.0f;
        if (at_min)
            fixed.triangles.push_back(t);
    }
    tip.kind = ConditionKind::ForcePoint;
    tip.point = {24.0, 3.0, 3.0};
    tip.normal = {1.0, 0.0, 0.0};
    tip.radius = 3.0;
    tip.newtons = 20.0;
    tip.direction = ForceDirection::Axis;
    tip.axis = -3;
    e.conditions = {fixed, tip};
    e.structural.quality = "normal";
    std::vector<std::string> notes;
    const AnalysisCase c = engineering_analysis_case(e, triangles, {}, notes, "PLA");
    Settings s = beads_04();
    s.infill_density = 50.0;
    const AnalysisReport uniform = run_analysis(bar, s, c);
    REQUIRE(uniform.valid);
    CHECK(infill_strength_exponent(InfillPattern::Gyroid) > infill_strength_exponent(InfillPattern::Rectilinear));
    const InfillZones z = infill_zones(uniform, 50.0, 2.0, InfillPattern::Rectilinear);
    CHECK(z.cells[0] + z.cells[1] + z.cells[2] > 0);
    REQUIRE_FALSE(z.modifiers.empty());
    for (int l = 0; l < 3; ++l) {
        CHECK(z.density[l] >= 0.0);
        CHECK(z.density[l] <= 100.0);
        if (l > 0)
            CHECK(z.density[l] >= z.density[l - 1]);
    }
    // The densest zone is near the fixed end, where the bending is largest.
    double densest_x = 1e9;
    for (const MeshModifier& m : z.modifiers)
        if (m.settings.find(std::to_string(static_cast<int>(std::lround(z.density[2])))) != std::string::npos) {
            CHECK_FALSE(m.mesh.empty());
            densest_x = std::min(densest_x, static_cast<double>(m.mesh.bounding_box().min.x));
        }
    CHECK(densest_x < 6.0);
    // A smaller scale never asks for more.
    const InfillZones half = infill_zones(uniform, 50.0, 2.0, InfillPattern::Rectilinear, 0.5);
    for (int l = 0; l < 3; ++l)
        CHECK(half.density[l] <= z.density[l] + 1e-9);
    // Rule 3's search: the zones scaled until they hold, lighter than 50 % everywhere.
    double last = -1.0;
    const ZonedInfill zoned = zoned_infill(bar, triangles, s, c, e, 50, [&](double done) {
        CHECK(done >= last);
        last = done;
    });
    CHECK(last == 1.0);
    CHECK(zoned.analyses <= 7);
    REQUIRE(zoned.found);
    CHECK(zoned.uniform_mass_g > 0.0);
    CHECK(zoned.mass_g > 0.0);
    CHECK(zoned.lighter == (zoned.mass_g < zoned.uniform_mass_g));
    CHECK(zoned.mass_g <= 1.05 * zoned.uniform_mass_g); // the voxels weigh beads coarsely: near, not always below
    for (const MeshModifier& m : zoned.zones.modifiers)
        CHECK(m.name != "zone 1"); // the lowest level is the object's own infill
    std::printf("zoned infill: levels %.0f/%.0f/%.0f %%, %.3f g vs %.3f g uniform, %d analyses\n", zoned.zones.density[0],
                zoned.zones.density[1], zoned.zones.density[2], zoned.mass_g, zoned.uniform_mass_g, zoned.analyses);
}

TEST_CASE("infill ribs: lines along the stress (structural_optimization.md, rule 4)", "[analysis][engineering]") {
    // A bar along X or along Y, held at one end and pulled at the other along itself.
    const auto ribs_of = [](bool along_y) {
        const float lx = along_y ? 6.0f : 24.0f, ly = along_y ? 24.0f : 6.0f;
        const IndexedMesh bar = box_mesh(lx, ly, 6.0f);
        TriangleMesh triangles;
        for (const auto& t : bar.triangles)
            triangles.add_triangle(bar.vertices[t[0]], bar.vertices[t[1]], bar.vertices[t[2]]);
        ProjectEngineering e;
        EngineeringCondition fixed, pull;
        fixed.kind = ConditionKind::FixedZone;
        for (std::uint32_t t = 0; t < triangles.triangle_count(); ++t) {
            bool at_min = true;
            for (int k = 0; k < 3; ++k)
                at_min &= (along_y ? triangles.corner(t, k).y : triangles.corner(t, k).x) == 0.0f;
            if (at_min)
                fixed.triangles.push_back(t);
        }
        pull.kind = ConditionKind::ForcePoint;
        pull.point = along_y ? std::array<double, 3>{3.0, 24.0, 3.0} : std::array<double, 3>{24.0, 3.0, 3.0};
        pull.normal = along_y ? std::array<double, 3>{0.0, 1.0, 0.0} : std::array<double, 3>{1.0, 0.0, 0.0};
        pull.radius = 3.0;
        pull.newtons = 50.0;
        pull.direction = ForceDirection::Axis;
        pull.axis = along_y ? 2 : 1;
        e.conditions = {fixed, pull};
        e.structural.quality = "normal";
        std::vector<std::string> notes;
        const AnalysisCase c = engineering_analysis_case(e, triangles, {}, notes, "PLA");
        Settings s = beads_04();
        s.infill_density = 30.0;
        const AnalysisReport r = run_analysis(bar, s, c);
        REQUIRE(r.valid);
        REQUIRE(r.result.layer_stress.size() == r.grid.solid.size());
        return infill_ribs(r);
    };
    // The angle of the group with the most cells.
    const auto main_angle = [](const InfillRibs& ribs) {
        std::size_t best = 0;
        for (std::size_t g = 1; g < ribs.angles.size(); ++g)
            if (ribs.group_cells[g] > ribs.group_cells[best])
                best = g;
        std::printf("infill ribs:");
        for (std::size_t g = 0; g < ribs.angles.size(); ++g)
            std::printf(" %.0f° × %zu", ribs.angles[g], ribs.group_cells[g]);
        std::printf("\n");
        return ribs.angles.empty() ? -1.0 : ribs.angles[best];
    };
    const InfillRibs x = ribs_of(false);
    REQUIRE(x.cells > 0);
    REQUIRE_FALSE(x.modifiers.empty());
    CHECK(main_angle(x) == 0.0); // pulled along X: lines along X
    REQUIRE(x.group_cells.size() == x.angles.size());
    CHECK(x.modifiers.front().settings.find("aligned_rectilinear") != std::string::npos);
    const InfillRibs y = ribs_of(true);
    REQUIRE(y.cells > 0);
    CHECK(main_angle(y) == 90.0);
}
