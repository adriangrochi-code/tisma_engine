#include "slice/job.hpp"

#include <tisma/tisma_engine.h>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

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

    // The preview and the analysis are not there yet, and say so.
    size_t segments = 1;
    CHECK(tisma_preview_segments(ctx, 0, 1, 0, nullptr, 0, &segments).code == TISMA_ERROR);
    CHECK(segments == 0);
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
