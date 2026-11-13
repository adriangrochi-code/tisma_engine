#include "mesh/shapes.hpp"
#include "project/project.hpp"
#include "slice/job.hpp"
#include "test_temp.hpp"
#include "ui/slicing.hpp"

#include <tisma/tisma_engine.h>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <cmath>

#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

const std::string kData = TISMA_TEST_DATA_DIR;

std::string read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

// Polls until the job ends (at most a minute).
tisma_progress wait(tisma_ctx* ctx) {
    const auto until = std::chrono::steady_clock::now() + std::chrono::minutes(1);
    tisma_progress p = tisma_poll(ctx);
    while (p.state == TISMA_RUNNING && std::chrono::steady_clock::now() < until) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        p = tisma_poll(ctx);
    }
    return p;
}

} // namespace

TEST_CASE("Job hook: steps, progress that never goes back, cancel", "[c_api]") {
    tisma::job_step("free", 50); // no hook: nothing
    tisma::JobHook hook;
    {
        tisma::JobScope scope(hook);
        tisma::job_step("walls", 15);
        tisma::job_step("again", 10);
        CHECK(hook.percent.load() == 15);
        CHECK(std::string(hook.stage.load()) == "again");
        hook.cancel.store(true);
        CHECK_THROWS_AS(tisma::job_step("infill", 50), tisma::JobCancelled);
        CHECK_THROWS_AS(tisma::job_check(), tisma::JobCancelled);
    }
    tisma::job_check(); // the scope is gone: no hook
}

TEST_CASE("C API: wrong calls give their codes", "[c_api]") {
    CHECK(std::string(tisma_version()) == TISMA_VERSION);
    CHECK(tisma_open_path(nullptr, "x.stl").code == TISMA_BAD_ARGUMENT);
    tisma_ctx* ctx = tisma_create();
    REQUIRE(ctx);
    CHECK(tisma_slice_start(ctx).code == TISMA_NOTHING_TO_SLICE);
    CHECK(tisma_open_path(ctx, (kData + "/missing.stl").c_str()).code == TISMA_CANNOT_READ);
    const char junk[] = "solid nothing\nendsolid nothing\n";
    CHECK(tisma_open_bytes(ctx, junk, sizeof junk - 1, "empty.stl").code == TISMA_BAD_MODEL);
    const tisma_status bad = tisma_set_settings_json(ctx, "{ not json");
    CHECK(bad.code == TISMA_BAD_SETTINGS);
    CHECK(bad.message != nullptr);
    CHECK(tisma_estimate_memory(ctx) == 0);
    REQUIRE(tisma_open_path(ctx, (kData + "/cube_ascii.stl").c_str()).code == TISMA_OK);
    CHECK(tisma_slice_start(ctx).code == TISMA_BAD_SETTINGS); // none set yet
    CHECK(tisma_summary_json(ctx) == nullptr);
    CHECK(tisma_write_gcode(ctx, "x.gcode").code == TISMA_NOT_READY);
    CHECK(tisma_poll(ctx).state == TISMA_IDLE);
    tisma_cancel(ctx); // nothing running: nothing happens
    tisma_destroy(ctx);
    tisma_destroy(nullptr);
}

TEST_CASE("C API: open, settings, memory, slice with progress, summary and G-code", "[c_api]") {
    tisma_ctx* ctx = tisma_create();
    REQUIRE(ctx);
    const std::string cube = read_file(kData + "/cube_ascii.stl");
    REQUIRE(tisma_open_bytes(ctx, cube.data(), cube.size(), "cube.stl").code == TISMA_OK);
    REQUIRE(tisma_set_settings_json(ctx, read_file(kData + "/settings.json").c_str()).code == TISMA_OK);
    CHECK(std::string(tisma_get_settings_json(ctx)).find("\"layer_height\"") != std::string::npos);

    // The estimate is above what the slice was measured to take, and a
    // limit below it refuses.
    const uint64_t estimate = tisma_estimate_memory(ctx);
    CHECK(estimate > 64ull * 1024 * 1024);
    tisma_set_memory_limit(ctx, estimate - 1);
    CHECK(tisma_slice_start(ctx).code == TISMA_TOO_LARGE);
    tisma_set_memory_limit(ctx, 0);

    REQUIRE(tisma_slice_start(ctx).code == TISMA_OK);
    // Busy while it runs (a slice that ended already starts again: the same result).
    const tisma_code again = tisma_slice_start(ctx).code;
    CHECK((again == TISMA_BUSY || again == TISMA_OK));
    const tisma_progress done = wait(ctx);
    REQUIRE(done.state == TISMA_DONE);
    CHECK(done.percent == 100);
    CHECK(std::string(done.stage) == "done");
    const std::string summary = tisma_summary_json(ctx);
    CHECK(summary.find("\"layers\":50") != std::string::npos);
    CHECK(summary.find("\"print_time_s\":") != std::string::npos);
    CHECK(summary.find("\"cost\":null") != std::string::npos);
    CHECK(summary.find("\"warnings\":[") != std::string::npos);

    const std::filesystem::path out = std::filesystem::temp_directory_path() / "tisma_c_api_test.gcode";
    REQUIRE(tisma_write_gcode(ctx, out.string().c_str()).code == TISMA_OK);
    const std::string written = read_file(out.string());
    CHECK(written.find("G1") != std::string::npos);
    std::string pieces;
    char buffer[4096];
    size_t got = 0;
    do {
        REQUIRE(tisma_read_gcode(ctx, pieces.size(), buffer, sizeof buffer, &got).code == TISMA_OK);
        pieces.append(buffer, got);
    } while (got > 0);
    CHECK(pieces == written);
    std::filesystem::remove(out);

    // The preview by layers: the layers go up, and the segments come in
    // pieces that add up to the same as all at once.
    const int layers = tisma_layer_count(ctx);
    CHECK(layers >= 50);
    for (int l = 1; l < layers; ++l)
        CHECK(tisma_layer_z(ctx, l) > tisma_layer_z(ctx, l - 1));
    CHECK(tisma_layer_z(ctx, layers) == 0.0);
    std::vector<tisma_segment> all(200000);
    size_t total = 0;
    REQUIRE(tisma_preview_segments(ctx, 0, layers, 0, all.data(), all.size(), &total).code == TISMA_OK);
    REQUIRE(total > 0);
    REQUIRE(total < all.size());
    size_t handed = 0, piece = 0, outer = 0;
    tisma_segment chunk[37];
    do {
        REQUIRE(tisma_preview_segments(ctx, 0, layers, handed, chunk, 37, &piece).code == TISMA_OK);
        for (size_t i = 0; i < piece; ++i) {
            const tisma_segment& x = chunk[i];
            const tisma_segment& y = all[handed + i];
            CHECK((x.x0 == y.x0 && x.y0 == y.y0 && x.z0 == y.z0 && x.x1 == y.x1 && x.y1 == y.y1 && x.z1 == y.z1 &&
                   x.width == y.width && x.height == y.height && x.speed == y.speed && x.type == y.type));
            outer += chunk[i].type == TISMA_PATH_OUTER_WALL ? 1 : 0;
        }
        handed += piece;
    } while (piece > 0);
    CHECK(handed == total);
    CHECK(outer > 0);
    // One layer: its segments at its height.
    size_t one = 0;
    REQUIRE(tisma_preview_segments(ctx, 10, 1, 0, all.data(), all.size(), &one).code == TISMA_OK);
    REQUIRE(one > 0);
    for (size_t i = 0; i < one; ++i)
        if (all[i].type != TISMA_PATH_TRAVEL)
            CHECK(std::abs(all[i].z1 - tisma_layer_z(ctx, 10)) < 1e-3);
    CHECK(tisma_preview_segments(ctx, -1, 1, 0, all.data(), all.size(), &one).code == TISMA_BAD_ARGUMENT);
    tisma_destroy(ctx);
}

TEST_CASE("C API: the summary's cost from the grams and extrusion.cost", "[c_api]") {
    tisma_ctx* ctx = tisma_create();
    REQUIRE(ctx);
    REQUIRE(tisma_open_path(ctx, (kData + "/cube_ascii.stl").c_str()).code == TISMA_OK);
    REQUIRE(tisma_set_settings_json(ctx, R"({"extrusion": {"density": 1.24, "cost": 20}})").code == TISMA_OK);
    REQUIRE(tisma_slice_start(ctx).code == TISMA_OK);
    REQUIRE(wait(ctx).state == TISMA_DONE);
    const nlohmann::json summary = nlohmann::json::parse(tisma_summary_json(ctx));
    REQUIRE(summary["filament_g"].is_number());
    REQUIRE(summary["cost"].is_number());
    const double grams = summary["filament_g"].get<double>();
    CHECK(grams > 0.0);
    CHECK(std::abs(summary["cost"].get<double>() - std::round(grams * 20.0 / 10.0) / 100.0) < 1e-9);
    // Without a price the cost is unknown.
    REQUIRE(tisma_set_settings_json(ctx, R"({"extrusion": {"density": 1.24}})").code == TISMA_OK);
    REQUIRE(tisma_slice_start(ctx).code == TISMA_OK);
    REQUIRE(wait(ctx).state == TISMA_DONE);
    CHECK(nlohmann::json::parse(tisma_summary_json(ctx))["cost"].is_null());
    tisma_destroy(ctx);
}

TEST_CASE("C API: a cancelled slice stops, and the context slices again", "[c_api]") {
    tisma_ctx* ctx = tisma_create();
    REQUIRE(ctx);
    REQUIRE(tisma_open_path(ctx, (kData + "/sphere.stl").c_str()).code == TISMA_OK);
    REQUIRE(tisma_set_settings_json(ctx, read_file(kData + "/settings.json").c_str()).code == TISMA_OK);
    REQUIRE(tisma_slice_start(ctx).code == TISMA_OK);
    tisma_cancel(ctx);
    const tisma_progress stopped = wait(ctx);
    // A slice faster than the cancel ends done; otherwise it stops at its next stage.
    REQUIRE((stopped.state == TISMA_STOPPED || stopped.state == TISMA_DONE));
    if (stopped.state == TISMA_STOPPED) {
        CHECK(stopped.error.code == TISMA_CANCELLED);
        CHECK(tisma_summary_json(ctx) == nullptr);
        CHECK(tisma_write_gcode(ctx, "x.gcode").code == TISMA_NOT_READY);
    }
    REQUIRE(tisma_slice_start(ctx).code == TISMA_OK);
    CHECK(wait(ctx).state == TISMA_DONE);
    // Destroying a context with a job running stops it first.
    REQUIRE(tisma_slice_start(ctx).code == TISMA_OK);
    tisma_destroy(ctx);
}

TEST_CASE("C API: the structural analysis of a project's object, with progress, and its errors", "[c_api]") {
    using namespace tisma;
    // A 20 mm PLA block held at its bottom, 50 N down on its top.
    Project p;
    p.settings = ui::default_settings();
    p.settings.gpu = GpuMode::Off;
    ProjectModel m;
    m.name = "box.stl";
    m.mesh = make_box(20.0, 20.0, 20.0);
    m.offset = {100.0, 100.0, 0.0};
    EngineeringCondition fixed, push;
    fixed.kind = ConditionKind::FixedZone;
    push.kind = ConditionKind::ForceZone;
    push.newtons = 50.0;
    push.direction = ForceDirection::Axis;
    push.axis = -3;
    for (std::uint32_t t = 0; t < m.mesh.triangle_count(); ++t) {
        bool bottom = true, top = true;
        for (int k = 0; k < 3; ++k)
            bottom &= m.mesh.corner(t, k).z == 0.0f, top &= m.mesh.corner(t, k).z == 20.0f;
        if (bottom)
            fixed.triangles.push_back(t);
        if (top)
            push.triangles.push_back(t);
    }
    const auto dir = test_temp_dir();
    const auto bare = dir / "tisma_c_api_bare.3mf", loaded = dir / "tisma_c_api_loaded.3mf";
    p.models.push_back(m);
    save_project(bare, p);
    p.models[0].engineering.conditions = {fixed, push};
    save_project(loaded, p);

    tisma_ctx* ctx = tisma_create();
    REQUIRE(ctx);
    CHECK(tisma_analyze_start(ctx, 0, "fast").code == TISMA_NOTHING_TO_SLICE);
    REQUIRE(tisma_open_path(ctx, loaded.string().c_str()).code == TISMA_OK);
    CHECK(tisma_analyze_start(ctx, 0, "best").code == TISMA_BAD_ARGUMENT);
    CHECK(tisma_analyze_start(ctx, -1, "fast").code == TISMA_BAD_ARGUMENT);
    REQUIRE(tisma_analyze_start(ctx, 0, "fast").code == TISMA_OK);
    const tisma_progress done = wait(ctx);
    INFO((done.error.message ? done.error.message : ""));
    REQUIRE(done.state == TISMA_DONE);
    CHECK(done.percent == 100);
    const nlohmann::json report = nlohmann::json::parse(tisma_analysis_json(ctx));
    CHECK(report["verdict"] == "holds");
    CHECK(report["min_safety"].get<double>() > 2.0);
    CHECK(report["min_safety_low"].get<double>() <= report["min_safety"].get<double>());
    CHECK(report["max_displacement_mm"].get<double>() > 0.0);
    CHECK(report["mass_g"].get<double>() > 0.0);
    // An analysis is not a slice.
    CHECK(tisma_summary_json(ctx) == nullptr);
    CHECK(tisma_layer_count(ctx) == 0);

    // Another object, or no conditions: the codes say it.
    REQUIRE(tisma_analyze_start(ctx, 3, "fast").code == TISMA_OK);
    CHECK(wait(ctx).state == TISMA_FAILED);
    REQUIRE(tisma_open_path(ctx, bare.string().c_str()).code == TISMA_OK);
    REQUIRE(tisma_analyze_start(ctx, 0, "fast").code == TISMA_OK);
    const tisma_progress none = wait(ctx);
    CHECK(none.state == TISMA_FAILED);
    CHECK(none.error.code == TISMA_NEEDS_CONDITIONS);
    CHECK(tisma_analysis_json(ctx) == nullptr);

    // Cancelled: it stops at its next progress line (or ends done first).
    REQUIRE(tisma_open_path(ctx, loaded.string().c_str()).code == TISMA_OK);
    REQUIRE(tisma_analyze_start(ctx, 0, "normal").code == TISMA_OK);
    tisma_cancel(ctx);
    const tisma_progress stopped = wait(ctx);
    REQUIRE((stopped.state == TISMA_STOPPED || stopped.state == TISMA_DONE));
    if (stopped.state == TISMA_STOPPED)
        CHECK(stopped.error.code == TISMA_CANCELLED);
    tisma_destroy(ctx);
    std::filesystem::remove(bare);
    std::filesystem::remove(loaded);
}


TEST_CASE("C API: the analysis of a bare model with a case, its report and voxel surface", "[c_api]") {
    tisma_ctx* ctx = tisma_create();
    REQUIRE(ctx);
    const char* push = R"({"material": "bambu-pla-basic", "model": "as_printed", "voxel": 1.0, "rows_per_layer": 1,
                            "fixtures": [{"name": "bed", "face": "bottom"}],
                            "loads": [{"name": "push", "face": "top", "force": [0, 0, -100]}]})";
    CHECK(tisma_analyze_case_start(ctx, push).code == TISMA_NOTHING_TO_SLICE);
    REQUIRE(tisma_open_path(ctx, (kData + "/cube_ascii.stl").c_str()).code == TISMA_OK);
    REQUIRE(tisma_set_settings_json(ctx, read_file(kData + "/settings.json").c_str()).code == TISMA_OK);
    CHECK(tisma_analyze_case_start(ctx, nullptr).code == TISMA_BAD_ARGUMENT);
    CHECK(tisma_analyze_case_start(ctx, "{not json").code == TISMA_BAD_ARGUMENT);
    size_t size = 1;
    CHECK(tisma_analysis_mesh(ctx, &size) == nullptr);
    CHECK(size == 0);

    REQUIRE(tisma_analyze_case_start(ctx, push).code == TISMA_OK);
    const tisma_progress done = wait(ctx);
    INFO((done.error.message ? done.error.message : ""));
    REQUIRE(done.state == TISMA_DONE);
    CHECK(done.percent == 100);
    const char* text = tisma_analysis_json(ctx);
    REQUIRE(text);
    const nlohmann::json report = nlohmann::json::parse(text);
    CHECK(report.is_object());
    const unsigned char* mesh = tisma_analysis_mesh(ctx, &size);
    REQUIRE(mesh);
    REQUIRE(size > 3);
    CHECK(std::string(reinterpret_cast<const char*>(mesh), 3) == "ply");
    CHECK(tisma_summary_json(ctx) == nullptr);

    // A case the analysis cannot take fails with its reason.
    REQUIRE(tisma_analyze_case_start(ctx, R"({"fixtures": [], "loads": []})").code == TISMA_OK);
    const tisma_progress bad = wait(ctx);
    CHECK(bad.state == TISMA_FAILED);
    CHECK(tisma_analysis_mesh(ctx, &size) == nullptr);

    // Cancelled: it stops at its next progress line (or ends done first).
    REQUIRE(tisma_analyze_case_start(ctx, push).code == TISMA_OK);
    tisma_cancel(ctx);
    const tisma_progress stopped = wait(ctx);
    REQUIRE((stopped.state == TISMA_STOPPED || stopped.state == TISMA_DONE));
    if (stopped.state == TISMA_STOPPED)
        CHECK(stopped.error.code == TISMA_CANCELLED);
    tisma_destroy(ctx);
}

TEST_CASE("C API: a project slices with its own settings when none are given", "[c_api]") {
    using namespace tisma;
    Project p;
    p.settings = ui::default_settings();
    p.settings.gpu = GpuMode::Off;
    p.settings.filament_density = 1.24;
    ProjectModel m;
    m.name = "box.stl";
    m.mesh = make_box(20.0, 20.0, 20.0);
    m.offset = {100.0, 100.0, 0.0};
    p.models.push_back(m);
    const auto project = test_temp_dir() / "tisma_c_api_project.3mf";
    save_project(project, p);

    tisma_ctx* ctx = tisma_create();
    REQUIRE(ctx);
    // A model file still needs settings.
    REQUIRE(tisma_open_path(ctx, (kData + "/cube_ascii.stl").c_str()).code == TISMA_OK);
    CHECK(tisma_slice_start(ctx).code == TISMA_BAD_SETTINGS);
    REQUIRE(tisma_open_path(ctx, project.string().c_str()).code == TISMA_OK);
    REQUIRE(tisma_slice_start(ctx).code == TISMA_OK);
    const tisma_progress done = wait(ctx);
    INFO((done.error.message ? done.error.message : ""));
    REQUIRE(done.state == TISMA_DONE);
    const nlohmann::json summary = nlohmann::json::parse(tisma_summary_json(ctx));
    CHECK(summary["layers"].get<int>() == 100); // 20 mm at the default 0.2 mm
    CHECK(summary["filament_g"].get<double>() > 0.0);
    CHECK(tisma_layer_count(ctx) == 100);
    // Cancelled at its next progress line.
    REQUIRE(tisma_slice_start(ctx).code == TISMA_OK);
    tisma_cancel(ctx);
    const tisma_progress stopped = wait(ctx);
    REQUIRE((stopped.state == TISMA_STOPPED || stopped.state == TISMA_DONE));
    tisma_destroy(ctx);
    std::filesystem::remove(project);
}
