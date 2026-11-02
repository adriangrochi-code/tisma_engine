// The engine as the program of Tisma Cloud's nodes (cloud.md, rules 18 and
// 20; CC10 to CC12).

#include "cli/cli.hpp"
#include "mesh/shapes.hpp"
#include "project/project.hpp"
#include "test_temp.hpp"
#include "ui/slicing.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <regex>
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

std::string read_text(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

// A project as the interface saves it: the generic printer, PLA, and a 20 mm box,
// moved to (x, y) on the bed.
Project box_project(double x, double y) {
    Project p;
    p.settings = ui::default_settings();
    p.settings.gpu = GpuMode::Off;
    p.settings.filament_density = 1.24; // PLA: the summary's grams
    ProjectModel m;
    m.name = "box.stl";
    m.mesh = make_box(20.0, 20.0, 20.0);
    m.offset = {x, y, 0.0};
    p.models.push_back(std::move(m));
    return p;
}

std::filesystem::path fresh_dir(const char* name) {
    const auto dir = test_temp_dir() / name;
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

// Every line of the output is "progress N", N going up, from 0 to 100.
void check_progress(const std::string& out) {
    std::istringstream lines(out);
    int last = -1;
    std::size_t count = 0;
    for (std::string line; std::getline(lines, line); ++count) {
        INFO(line);
        REQUIRE(line.starts_with("progress "));
        const int n = std::stoi(line.substr(9));
        CHECK(n > last);
        CHECK(n <= 100);
        last = n;
    }
    CHECK(count >= 2);
    CHECK(last == 100);
}

} // namespace

TEST_CASE("CC10: slice of a project with --sandbox: progress lines, G-code and the summary's five keys",
          "[cli][cloud]") {
    const auto dir = fresh_dir("tisma_test_node_cc10");
    const auto project = dir / "plate.3mf";
    save_project(project, box_project(100.0, 100.0));
    const auto gcode = dir / "plate.gcode", summary = dir / "summary.json";
    const Run r = run({"slice", project.string(), "-o", gcode.string(), "--summary", summary.string(), "--sandbox",
                       "--no-gpu"});
    INFO(r.err);
    REQUIRE(r.code == kExitOk);
    check_progress(r.out);
    const std::string text = read_text(gcode);
    CHECK_THAT(text, ContainsSubstring("; estimated printing time"));
    const nlohmann::json j = nlohmann::json::parse(read_text(summary));
    CHECK_FALSE(j.contains("error"));
    REQUIRE(j.contains("print_time"));
    CHECK(std::regex_match(j["print_time"].get<std::string>(), std::regex(R"(((\d+h )?\d+m )?\d+s)")));
    CHECK(j["print_time_s"].get<long long>() > 0);
    CHECK(j["filament_mm"].get<double>() > 100.0);
    CHECK(j["filament_g"].get<double>() > 0.0);
    CHECK(j["layers"].get<int>() == 100); // 20 mm in layers of 0.2 mm
    std::filesystem::remove_all(dir);
}

TEST_CASE("slice of a project: the G-code binary when the profile says so (cloud.md, rule 18)", "[cli][cloud]") {
    const auto dir = fresh_dir("tisma_test_node_binary");
    Project p = box_project(100.0, 100.0);
    p.settings.gcode_binary = true;
    const auto project = dir / "plate.tisma";
    save_project(project, p);
    const auto gcode = dir / "plate.bgcode", summary = dir / "summary.json";
    const Run r = run({"slice", project.string(), "-o", gcode.string(), "--summary", summary.string(), "--sandbox"});
    INFO(r.err);
    REQUIRE(r.code == kExitOk);
    CHECK(read_text(gcode).starts_with("GCDE"));
    const nlohmann::json j = nlohmann::json::parse(read_text(summary));
    CHECK(j["print_time_s"].get<long long>() > 0);
    CHECK(j["filament_g"].get<double>() > 0.0);
    std::filesystem::remove_all(dir);
}

TEST_CASE("CC11: a project with a post-processing script: --sandbox does not run it", "[cli][cloud]") {
    const auto dir = fresh_dir("tisma_test_node_cc11");
    const auto marker = dir / "ran.txt";
#ifdef _WIN32
    const auto script = dir / "post.cmd";
    std::ofstream(script) << "@echo ran> \"" << marker.string() << "\"\r\n";
#else
    const auto script = dir / "post.sh";
    std::ofstream(script) << "#!/bin/sh\necho ran > '" << marker.string() << "'\n";
    std::filesystem::permissions(script, std::filesystem::perms::owner_all);
#endif
    Project p = box_project(100.0, 100.0);
    p.settings.post_process = {script.string()};
    const auto project = dir / "plate.3mf";
    save_project(project, p);
    const auto gcode = dir / "plate.gcode", summary = dir / "summary.json";
    const Run r = run({"slice", project.string(), "-o", gcode.string(), "--summary", summary.string(), "--sandbox"});
    INFO(r.err);
    CHECK(r.code == kExitOk);
    CHECK_THAT(r.err, ContainsSubstring("not run"));
    CHECK_FALSE(std::filesystem::exists(marker));
    CHECK(std::filesystem::exists(gcode));
#ifndef _WIN32
    // Without the sandbox, once confirmed, it runs (as `slice` runs it).
    const Run ran = run({"slice", project.string(), "-o", gcode.string(), "--summary", summary.string(), "--yes"});
    CHECK(ran.code == kExitOk);
    CHECK(std::filesystem::exists(marker));
#endif
    std::filesystem::remove_all(dir);
}

TEST_CASE("CC12: a project with a part off the bed: exit code not 0 and `error` in the summary", "[cli][cloud]") {
    const auto dir = fresh_dir("tisma_test_node_cc12");
    const auto project = dir / "plate.3mf";
    save_project(project, box_project(400.0, 100.0));
    const auto gcode = dir / "plate.gcode", summary = dir / "summary.json";
    const Run r = run({"slice", project.string(), "-o", gcode.string(), "--summary", summary.string(), "--sandbox"});
    CHECK(r.code != kExitOk);
    const nlohmann::json j = nlohmann::json::parse(read_text(summary));
    REQUIRE(j.contains("error"));
    CHECK_THAT(j["error"].get<std::string>(), ContainsSubstring("bed"));
    CHECK_FALSE(j.contains("print_time"));
    CHECK_FALSE(std::filesystem::exists(gcode));
    std::filesystem::remove_all(dir);
}

TEST_CASE("slice of a project: what is not a project, and a bad command line, say so in the summary",
          "[cli][cloud]") {
    const auto dir = fresh_dir("tisma_test_node_bad");
    const auto summary = dir / "summary.json", gcode = dir / "out.gcode";
    const auto model = dir / "box.stl";
    std::ofstream(model) << "solid nothing\nendsolid nothing\n";
    const Run stl = run({"slice", model.string(), "-o", gcode.string(), "--summary", summary.string()});
    CHECK(stl.code == kExitInput);
    CHECK_THAT(nlohmann::json::parse(read_text(summary))["error"].get<std::string>(),
               ContainsSubstring("not a project"));
    // A project without --summary: the project's way, which needs it.
    const auto project = dir / "plate.3mf";
    save_project(project, box_project(100.0, 100.0));
    const Run bare = run({"slice", project.string(), "-o", gcode.string()});
    CHECK(bare.code == kExitUsage);
    CHECK_THAT(bare.err, ContainsSubstring("--summary"));
    const Run unknown = run({"slice", project.string(), "-o", gcode.string(), "--summary", summary.string(), "--fast"});
    CHECK(unknown.code == kExitUsage);
    CHECK_THAT(nlohmann::json::parse(read_text(summary))["error"].get<std::string>(), ContainsSubstring("--fast"));
    // Never over the project.
    const Run over = run({"slice", project.string(), "-o", project.string(), "--summary", summary.string()});
    CHECK(over.code == kExitInput);
    CHECK(looks_like_project(read_text(project)));
    std::filesystem::remove_all(dir);
}

TEST_CASE("slice of a project: the same G-code from the engine's own file and from its 3MF package", "[cli][cloud]") {
    const auto dir = fresh_dir("tisma_test_node_3mf");
    Project p = box_project(100.0, 100.0);
    p.models.push_back(p.models.front());
    p.models.back().offset = {140.0, 90.0, 0.0};
    save_project(dir / "plate.tisma", p);
    save_project(dir / "plate.3mf", p);
    CHECK(read_text(dir / "plate.3mf").starts_with("PK"));
    for (const char* name : {"plate.tisma", "plate.3mf"}) {
        const Run r = run({"slice", (dir / name).string(), "-o", (dir / (std::string(name) + ".gcode")).string(), "--summary",
                           (dir / (std::string(name) + ".json")).string(), "--sandbox"});
        INFO(name << ": " << r.err);
        REQUIRE(r.code == kExitOk);
    }
    CHECK(read_text(dir / "plate.tisma.gcode") == read_text(dir / "plate.3mf.gcode"));
    std::filesystem::remove_all(dir);
}

