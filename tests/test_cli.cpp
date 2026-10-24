#include "cli/cli.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <sstream>
#include <string>
#include <vector>

using namespace tisma;
using Catch::Matchers::ContainsSubstring;

namespace {

struct Run {
    int code;
    std::string out;
    std::string err;
};

Run run(std::vector<std::string> args) {
    std::vector<std::string_view> views(args.begin(), args.end());
    std::ostringstream out, err;
    const int code = run_cli(views, out, err);
    return {code, out.str(), err.str()};
}

const std::string kData = TISMA_TEST_DATA_DIR;

} // namespace

TEST_CASE("no arguments prints usage", "[cli]") {
    const Run r = run({});
    CHECK(r.code == kExitUsage);
    CHECK_THAT(r.err, ContainsSubstring("usage"));
}

TEST_CASE("--help and --version", "[cli]") {
    CHECK(run({"--help"}).code == kExitOk);
    const Run v = run({"--version"});
    CHECK(v.code == kExitOk);
    CHECK_THAT(v.out, ContainsSubstring("tisma-engine "));
}

TEST_CASE("unknown command", "[cli]") {
    const Run r = run({"print"});
    CHECK(r.code == kExitUsage);
    CHECK_THAT(r.err, ContainsSubstring("unknown command"));
}

TEST_CASE("slice reads the model and says it is not implemented", "[cli]") {
    const Run r = run({"slice", kData + "/cube_ascii.stl", "--config", kData + "/settings.json", "-o", "out.gcode"});
    CHECK(r.code == kExitNotImplemented);
    CHECK_THAT(r.out, ContainsSubstring("read 12 triangles"));
    CHECK_THAT(r.err, ContainsSubstring("not implemented"));
}

TEST_CASE("slice argument errors", "[cli]") {
    CHECK(run({"slice", kData + "/cube_ascii.stl"}).code == kExitUsage);
    CHECK(run({"slice", kData + "/cube_ascii.stl", "--config"}).code == kExitUsage);
    CHECK(run({"slice", "a.stl", "b.stl", "--config", "c", "-o", "d"}).code == kExitUsage);
    CHECK(run({"slice", "a.stl", "--bogus", "--config", "c", "-o", "d"}).code == kExitUsage);
}

TEST_CASE("slice input errors", "[cli]") {
    const Run no_config = run({"slice", kData + "/cube_ascii.stl", "--config", kData + "/nope.json", "-o", "x"});
    CHECK(no_config.code == kExitInput);
    CHECK_THAT(no_config.err, ContainsSubstring("config file not found"));

    const Run no_model = run({"slice", kData + "/nope.stl", "--config", kData + "/settings.json", "-o", "x"});
    CHECK(no_model.code == kExitInput);
    CHECK_THAT(no_model.err, ContainsSubstring("cannot open"));
}

TEST_CASE("info prints the mesh summary", "[cli]") {
    const Run r = run({"info", kData + "/cube_ascii.stl"});
    CHECK(r.code == kExitOk);
    CHECK_THAT(r.out, ContainsSubstring("format: ascii"));
    CHECK_THAT(r.out, ContainsSubstring("triangles: 12"));
    CHECK_THAT(r.out, ContainsSubstring("size: 10 x 10 x 10"));
    CHECK_THAT(r.out, ContainsSubstring("volume: 1000"));
    CHECK_THAT(r.out, ContainsSubstring("vertices: 8"));
    CHECK_THAT(r.out, ContainsSubstring("layers: 50"));
    CHECK_THAT(r.out, ContainsSubstring("contours: 50"));
    CHECK_THAT(r.out, ContainsSubstring("sliced_volume: 1000"));
}
