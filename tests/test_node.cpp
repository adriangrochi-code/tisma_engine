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

TEST_CASE("CC19: --threads 1 gives the same G-code; an unknown density gives no grams", "[cli][cloud]") {
    const auto dir = fresh_dir("tisma_test_node_threads");
    Project p = box_project(100.0, 100.0);
    p.settings.filament_density = 0.0; // not known
    save_project(dir / "plate.3mf", p);
    const Run all = run({"slice", (dir / "plate.3mf").string(), "-o", (dir / "all.gcode").string(), "--summary",
                         (dir / "all.json").string(), "--sandbox"});
    const Run one = run({"slice", (dir / "plate.3mf").string(), "-o", (dir / "one.gcode").string(), "--summary",
                         (dir / "one.json").string(), "--sandbox", "--threads", "1"});
    INFO(all.err << one.err);
    REQUIRE(all.code == kExitOk);
    REQUIRE(one.code == kExitOk);
    CHECK(read_text(dir / "all.gcode") == read_text(dir / "one.gcode"));
    const nlohmann::json j = nlohmann::json::parse(read_text(dir / "one.json"));
    CHECK_FALSE(j.contains("filament_g"));
    CHECK(j["filament_mm"].get<double>() > 100.0);
    CHECK(run({"slice", (dir / "plate.3mf").string(), "--threads", "0"}).code == kExitUsage);
    CHECK(run({"slice", (dir / "plate.3mf").string(), "--threads"}).code == kExitUsage);
    CHECK_THAT(run({"--help"}).out, ContainsSubstring("--threads <n>"));
    std::filesystem::remove_all(dir);
}


namespace {
// box_project with Engineering conditions on its box: held at its bottom,
// 50 N down on its top, at fast quality.
Project loaded_box_project() {
    Project p = box_project(100.0, 100.0);
    ProjectModel& m = p.models[0];
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
    m.engineering.conditions = {fixed, push};
    return p;
}
} // namespace

TEST_CASE("analyze of a project the nodes' way: progress, the .tea result and the verdict (cloud.md, rule 19)",
          "[cli][cloud]") {
    const auto dir = fresh_dir("tisma_test_node_analyze");
    const auto project = dir / "plate.3mf";
    save_project(project, loaded_box_project());
    const auto tea = dir / "result.tea", summary = dir / "summary.json";
    const Run r = run({"analyze", project.string(), "-o", tea.string(), "--summary", summary.string(), "--sandbox", "--mode",
                       "analyze", "--quality", "fast", "--object", "0", "--instance", "0", "--no-gpu", "--threads", "2"});
    INFO(r.err);
    REQUIRE(r.code == kExitOk);
    check_progress(r.out);
    const nlohmann::json s = nlohmann::json::parse(read_text(summary));
    CHECK_FALSE(s.contains("error"));
    CHECK(s["verdict"] == "holds"); // 50 N on a 20 mm PLA block
    CHECK(s["min_safety"].get<double>() > 2.0);
    CHECK(s["required_safety"] == 2.0);
    CHECK(s["max_displacement_mm"].get<double>() > 0.0);
    CHECK(s["family"] == "PLA");
    CHECK(s["mass_g"].get<double>() > 0.0);
    CHECK(s["voxels"].get<long long>() > 100);
    const nlohmann::json t = nlohmann::json::parse(read_text(tea));
    CHECK(t["format"] == "tisma-engineering-result");
    CHECK(t["version"] == 1);
    CHECK(t["kind"] == "structural");
    CHECK(t["object"] == 0);
    CHECK_FALSE(t["fingerprint"].get<std::string>().empty());
    CHECK(t["report"]["verdict"]["name"] == "holds");
    {
        const nlohmann::json sum = nlohmann::json::parse(read_text(summary));
        REQUIRE(sum["min_safety_low"].is_number());
        CHECK(sum["min_safety_low"].get<double>() <= sum["min_safety"].get<double>());
        CHECK(sum["min_safety_high"].get<double>() >= sum["min_safety"].get<double>());
    }

    // --solid: the whole part solid, still held.
    const Run solid = run({"analyze", project.string(), "-o", tea.string(), "--summary", summary.string(), "--sandbox",
                           "--quality", "fast", "--object", "0", "--instance", "0", "--solid"});
    REQUIRE(solid.code == kExitOk);
    // Errors: the summary says them, the exit code is not 0.
    const auto error_of = [&](std::vector<std::string> args, int code) {
        const Run e = run(std::move(args));
        INFO(e.err);
        CHECK(e.code == code);
        const nlohmann::json j = nlohmann::json::parse(read_text(summary));
        REQUIRE(j.contains("error"));
        return j["error"].get<std::string>();
    };
    CHECK_THAT(error_of({"analyze", project.string(), "-o", tea.string(), "--summary", summary.string(), "--sandbox",
                         "--object", "3", "--instance", "0"},
                        kExitInput),
               ContainsSubstring("no object 3"));
    CHECK_THAT(error_of({"analyze", project.string(), "-o", tea.string(), "--summary", summary.string(), "--sandbox",
                         "--object", "0", "--instance", "1"},
                        kExitInput),
               ContainsSubstring("no instance 1"));
    CHECK_THAT(error_of({"analyze", project.string(), "-o", tea.string(), "--summary", summary.string(), "--sandbox",
                         "--mode", "lightest-infill", "--object", "0", "--instance", "0"},
                        kExitNotImplemented),
               ContainsSubstring("lightest-infill"));
    CHECK_THAT(error_of({"analyze", project.string(), "-o", tea.string(), "--summary", summary.string(), "--sandbox",
                         "--quality", "best"},
                        kExitUsage),
               ContainsSubstring("--quality"));
    // TR29: a stable code and the Spanish text for the cloud to show; BH23:
    // extreme (and ultra, its earlier name) accepted, so the mode is what fails.
    for (const char* q : {"extreme", "ultra"}) {
        CHECK_THAT(error_of({"analyze", project.string(), "-o", tea.string(), "--summary", summary.string(), "--sandbox",
                             "--mode", "lightest-infill", "--quality", q},
                            kExitNotImplemented),
                   ContainsSubstring("does not run in the cloud"));
        const nlohmann::json j = nlohmann::json::parse(read_text(summary));
        CHECK(j["code"] == "not_in_cloud");
        CHECK_THAT(j["error_es"].get<std::string>(), ContainsSubstring("todavía no corre en la nube"));
    }
    error_of({"analyze", project.string(), "-o", tea.string(), "--summary", summary.string(), "--sandbox", "--quality", "best"},
             kExitUsage);
    CHECK(nlohmann::json::parse(read_text(summary))["code"] == "bad_quality");
    CHECK_THAT(nlohmann::json::parse(read_text(summary))["error_es"].get<std::string>(), ContainsSubstring("extrema"));
    // A part with no conditions.
    const auto bare = dir / "bare.3mf";
    save_project(bare, box_project(100.0, 100.0));
    CHECK_THAT(error_of({"analyze", bare.string(), "-o", tea.string(), "--summary", summary.string(), "--sandbox", "--object",
                         "0", "--instance", "0"},
                        kExitInput),
               ContainsSubstring("fixed point or zone and a force"));
    CHECK(nlohmann::json::parse(read_text(summary))["code"] == "needs_conditions");
    std::filesystem::remove_all(dir);
}

TEST_CASE("aero of a project the nodes' way: progress, the .taero result and the summary (cloud.md, rule 19b)",
          "[cli][cloud]") {
    const auto dir = fresh_dir("tisma_test_node_aero");
    const auto project = dir / "plate.3mf";
    save_project(project, box_project(100.0, 100.0));
    const auto taero = dir / "result.taero", summary = dir / "summary.json";
    const Run r = run({"aero", project.string(), "-o", taero.string(), "--summary", summary.string(), "--sandbox", "--object", "0",
                       "--instance", "0", "--quality", "friction", "--speed", "20", "--direction", "-y", "--threads", "2"});
    INFO(r.err);
    REQUIRE(r.code == kExitOk);
    check_progress(r.out);
    const nlohmann::json s = nlohmann::json::parse(read_text(summary));
    CHECK_FALSE(s.contains("error"));
    for (const char* key : {"drag_n", "cd", "cd_pressure", "cd_friction", "reynolds", "reynolds_simulated"})
        CHECK(s.contains(key));
    CHECK(s["drag_n"].get<double>() > 0.0);
    CHECK(s["cd_pressure"].get<double>() == 0.0); // friction only: nothing simulated
    CHECK(s["reynolds_simulated"].get<double>() == 0.0);
    // 20 m/s across 20 mm: Re = 26 000.
    CHECK(std::abs(s["reynolds"].get<double>() / 26385.0 - 1.0) < 0.01);
    const nlohmann::json t = nlohmann::json::parse(read_text(taero));
    CHECK(t["format"] == "tisma-aero-result");
    CHECK(t["object"] == 0);
    CHECK(t["part"] == "box.stl");
    CHECK(t["inputs"]["direction"] == nlohmann::json::array({0.0, -1.0, 0.0}));
    CHECK(t["cp"].size() == 12);
    // Three numbers as the direction.
    CHECK(run({"aero", project.string(), "-o", taero.string(), "--summary", summary.string(), "--sandbox", "--quality", "friction",
               "--direction", "1,1,0"})
              .code == kExitOk);
    const auto error_of = [&](std::vector<std::string> args, int code) {
        const Run e = run(std::move(args));
        INFO(e.err);
        CHECK(e.code == code);
        const nlohmann::json j = nlohmann::json::parse(read_text(summary));
        REQUIRE(j.contains("error"));
        return j["error"].get<std::string>();
    };
    using Catch::Matchers::ContainsSubstring;
    CHECK_THAT(error_of({"aero", project.string(), "-o", taero.string(), "--summary", summary.string(), "--speed", "150"}, kExitUsage),
               ContainsSubstring("--speed"));
    CHECK_THAT(error_of({"aero", project.string(), "-o", taero.string(), "--summary", summary.string(), "--direction", "up"}, kExitUsage),
               ContainsSubstring("--direction"));
    CHECK_THAT(error_of({"aero", project.string(), "-o", taero.string(), "--summary", summary.string(), "--quality", "ultra"}, kExitUsage),
               ContainsSubstring("--quality"));
    CHECK_THAT(error_of({"aero", project.string(), "-o", taero.string(), "--summary", summary.string(), "--object", "2"}, kExitInput),
               ContainsSubstring("no object 2"));
    std::filesystem::remove_all(dir);
}

TEST_CASE("orient of a project the nodes' way (cloud.md, rules 10c and 19c)", "[cli][cloud]") {
    const auto dir = fresh_dir("node_orient");
    const auto project = dir / "bar.3mf";
    // A standing bar (OR2): lying down is cheaper.
    Project bar = box_project(100.0, 100.0);
    bar.models[0].mesh = make_box(5.0, 5.0, 60.0);
    save_project(project, bar);
    const auto json = dir / "orientation.json", summary = dir / "summary.json";
    const Run r = run({"orient", project.string(), "-o", json.string(), "--summary", summary.string(), "--sandbox", "--object", "0",
                       "--instance", "0", "--threads", "2"});
    INFO(r.err);
    REQUIRE(r.code == kExitOk);
    const nlohmann::json j = nlohmann::json::parse(read_text(json));
    CHECK(j["format"] == "tisma-orientation");
    REQUIRE(j["rotation"].size() == 3);
    CHECK(std::abs(j["rotation"][2][2].get<double>()) < 0.01); // the bar's Z no longer up
    CHECK(j["cost"].get<double>() < j["cost_before"].get<double>());
    const nlohmann::json s = nlohmann::json::parse(read_text(summary));
    CHECK(s["candidates"].get<int>() >= 2);
    // Errors: in the summary, with a code.
    const Run bad = run({"orient", project.string(), "-o", json.string(), "--summary", summary.string(), "--object", "4"});
    CHECK(bad.code == kExitInput);
    CHECK_THAT(nlohmann::json::parse(read_text(summary))["error"].get<std::string>(), ContainsSubstring("no object 4"));
    std::filesystem::remove_all(dir);
}

