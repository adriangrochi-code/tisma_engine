#include "config/analysis_case.hpp"
#include "config/settings.hpp"
#include "fea/analysis.hpp"
#include "fea/materials.hpp"
#include "io/ply.hpp"
#include "io/vtk.hpp"
#include "mesh/indexed_mesh.hpp"
#include "mesh/mesh.hpp"
#include "report/analysis_json.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <nlohmann/json.hpp>

#include <array>
#include <bit>
#include <cmath>
#include <limits>
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
