#include "slice/job.hpp"

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
