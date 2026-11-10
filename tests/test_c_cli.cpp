#include <tisma/tisma_engine.h>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>

TEST_CASE("C API: the command line in the same process (tisma_run_cli, for iOS)", "[c_api]") {
    // An unknown command: the usage exit code and a message on the error stream.
    const char* bad[] = {"tisma-engine", "no-such-command"};
    char* out = nullptr;
    char* err = nullptr;
    size_t out_size = 99, err_size = 0;
    CHECK(tisma_run_cli(2, bad, &out, &out_size, &err, &err_size) == 1);
    REQUIRE(out != nullptr);
    REQUIRE(err != nullptr);
    CHECK(out_size == std::string(out).size());
    CHECK(err_size > 0);
    tisma_free(out);
    tisma_free(err);
    // A real command, its output in the buffer: info on a model.
    const std::string model = (std::filesystem::path(TISMA_TEST_DATA_DIR) / "cube_ascii.stl").string();
    const char* info[] = {"tisma-engine", "info", model.c_str()};
    char* text = nullptr;
    CHECK(tisma_run_cli(3, info, &text, nullptr, nullptr, nullptr) == 0);
    REQUIRE(text != nullptr);
    CHECK(std::string(text).find("triangles") != std::string::npos);
    tisma_free(text);
    // Nothing wanted back: still runs.
    CHECK(tisma_run_cli(3, info, nullptr, nullptr, nullptr, nullptr) == 0);
}
