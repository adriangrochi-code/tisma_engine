#include "cli/cli.hpp"
#include "config/settings.hpp"
#include "io/zip.hpp"
#include "library/library.hpp"
#include "project/project.hpp"
#include "test_printer.hpp"
#include "test_temp.hpp"

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace tisma;
namespace fs = std::filesystem;
using nlohmann::json;

namespace {

fs::path fresh(const std::string& name) {
    const fs::path d = test_temp_dir() / name;
    fs::remove_all(d);
    fs::create_directories(d);
    return d;
}

fs::path put(const fs::path& p, const std::string& data) {
    std::ofstream(p, std::ios::binary) << data;
    return p;
}

std::string slurp(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), {});
}

int run(std::vector<std::string> args, std::string& out) {
    std::vector<std::string_view> views(args.begin(), args.end());
    std::ostringstream o, e;
    const int code = run_cli(views, o, e);
    out = o.str();
    return code;
}

struct Files {
    fs::path model, settings, recipe, summary, analysis, thumb;
};

Files files(const fs::path& in) {
    Settings s = test_printer();
    s.infill_density = 20.0;
    return {put(in / "p.3mf", write_zip_stored({{"3D/3dmodel.model", "fake project"}})),
            put(in / "s.json", settings_json(s)),
            put(in / "r.json", R"({"printer":"Acme One","nozzle":0.4,"filament":"PLA","print":"0.2 mm","label":"Acme One"})"),
            put(in / "sum.json", R"({"print_time_s":720,"filament_mm":2500,"filament_g":7.4,"layers":50})"),
            put(in / "a.tea", "TEA fake analysis"),
            put(in / "t.png", "PNG fake")};
}

} // namespace

TEST_CASE("library: save, the same recipe replaced, list and show; no G-code kept (local_library.md, ML10)",
          "[library]") {
    const fs::path lib = fresh("lib1"), in = fresh("lib1_in");
    const Files f = files(in);
    LibrarySave r{std::nullopt, f.model, f.settings, f.recipe, f.summary, f.analysis, std::nullopt, f.thumb, "Cube", "1.0.0"};
    const json e = library_save(lib, r);
    CHECK(e.at("format") == "tisma-library-entry");
    CHECK(e.at("name") == "Cube");
    REQUIRE(e.at("recipes").size() == 1);
    CHECK(e.at("recipes")[0].at("summary").at("filament_m") == 2.5);
    CHECK(e.at("analyses")[0].at("kind") == "fea");
    const std::string id = e.at("id");
    CHECK(slurp(lib / id / "project.3mf") == slurp(f.model)); // a 3MF is kept as it is
    CHECK(fs::exists(lib / id / "recipes" / "1" / "settings.json"));
    // The same printer, nozzle, filament and print: the recipe again, not a second.
    r.entry = id;
    r.analysis.reset();
    r.thumb.reset();
    const json again = library_save(lib, r);
    CHECK(again.at("recipes").size() == 1);
    CHECK(again.at("analyses").size() == 1);
    const json list = library_list(lib);
    REQUIRE(list.at("entries").size() == 1);
    CHECK(list.at("entries")[0].at("thumb") == "thumb.png");
    CHECK(list.at("entries")[0].at("printers") == json::array({"Acme One"}));
    CHECK(library_show(lib, id) == again);
    for (const auto& file : fs::recursive_directory_iterator(lib))
        CHECK(file.path().extension() != ".gcode");
    CHECK_THROWS_AS(library_show(lib, "../x"), LibraryError);
}

TEST_CASE("library: check says when the analysis no longer holds (ML3, ML4) and an older engine (ML8)", "[library]") {
    const fs::path lib = fresh("lib2"), in = fresh("lib2_in");
    const Files f = files(in);
    const json e = library_save(lib, {std::nullopt, f.model, f.settings, f.recipe, std::nullopt, f.analysis,
                                      std::nullopt, std::nullopt, std::nullopt, "1.0.0"});
    const std::string id = e.at("id");
    json c = library_check(lib, id, "1", f.settings, "1.0.0");
    CHECK(c.at("same_settings") == true);
    CHECK(c.at("older_engine") == false);
    CHECK(c.at("analyses")[0].at("valid") == true);
    Settings travel = test_printer();
    travel.infill_density = 20.0;
    travel.travel_speed += 50.0;
    c = library_check(lib, id, "1", put(in / "travel.json", settings_json(travel)), "1.1.0");
    CHECK(c.at("same_settings") == false);
    CHECK(c.at("older_engine") == true);
    CHECK(c.at("analyses")[0].at("valid") == true); // ML4
    Settings infill = test_printer();
    infill.infill_density = 40.0;
    c = library_check(lib, id, "1", put(in / "infill.json", settings_json(infill)), "1.0.0");
    CHECK(c.at("analyses")[0].at("valid") == false); // ML3
    CHECK(c.at("analyses")[0].at("changed") == json::array({"infill.density"}));
}

TEST_CASE("library: printed, delete, export and import; unsafe files refused whole (ML5, ML6)", "[library]") {
    const fs::path lib = fresh("lib3"), in = fresh("lib3_in"), other = fresh("lib3_other");
    const Files f = files(in);
    const json e = library_save(lib, {std::nullopt, f.model, f.settings, f.recipe, f.summary, f.analysis,
                                      std::nullopt, f.thumb, std::string("Hook"), "1.0.0"});
    const std::string id = e.at("id");
    const json p = library_printed(lib, id, "2026-11-06", put(in / "ph.jpg", "JPG"), "nice");
    CHECK(p.at("printed")[0].at("photo") == "photos/1.jpg");
    CHECK(slurp(lib / id / "photos" / "1.jpg") == "JPG");
    library_export(lib, id, in / "hook.tismalib");
    // A bare file name, in the current folder (BH57: "cannot create directories").
    {
        const fs::path was = fs::current_path();
        fs::current_path(in);
        CHECK_NOTHROW(library_export(lib, id, "bare.tismalib"));
        fs::current_path(was);
        CHECK(fs::exists(in / "bare.tismalib"));
    }
    const json imported = library_import(in / "hook.tismalib", other, LibraryImport::Ask);
    CHECK(imported.at("name") == "Hook");
    CHECK(slurp(other / id / "photos" / "1.jpg") == "JPG");
    CHECK(slurp(other / id / "analyses" / "1.tea") == "TEA fake analysis");
    // Already there: asked, then as a new one.
    std::string out;
    CHECK(run({"library", "import", (in / "hook.tismalib").string(), "--dir", other.string()}, out) == 3);
    CHECK(json::parse(out).at("exists") == id);
    CHECK(library_import(in / "hook.tismalib", other, LibraryImport::AsNew).at("id") == id + "_1");
    // A path out of the folder, or a program: refused, nothing written.
    for (const std::string& bad : std::vector<std::string>{id + "/../x", id + "/run.exe", "../evil/entry.json", id + "/photos/a.sh"}) {
        const fs::path target = fresh("lib3_bad");
        put(in / "bad.tismalib", write_zip_stored({{id + "/entry.json", slurp(lib / id / "entry.json")}, {bad, "x"}}));
        CHECK(run({"library", "import", (in / "bad.tismalib").string(), "--dir", target.string()}, out) == 2);
        CHECK(json::parse(out).at("code") == "rejected");
        CHECK(fs::is_empty(target));
    }
    library_delete(lib, id, std::nullopt, std::string("1"));
    CHECK(library_show(lib, id).at("analyses").empty());
    CHECK_FALSE(fs::exists(lib / id / "analyses" / "1.tea"));
    library_delete(lib, id, std::nullopt, std::nullopt);
    CHECK_FALSE(fs::exists(lib / id));
}

TEST_CASE("library on the command line: save, list and errors as JSON", "[library]") {
    const fs::path lib = fresh("lib4"), in = fresh("lib4_in");
    const Files f = files(in);
    std::string out;
    REQUIRE(run({"library", "save", "--dir", lib.string(), "--new", "--model", f.model.string(), "--config",
                 f.settings.string(), "--recipe", f.recipe.string(), "--thumb", f.thumb.string(), "--name", "Cube"},
                out) == 0);
    CHECK(json::parse(out).at("recipes")[0].at("engine") == TISMA_VERSION);
    // A bare STL becomes a 3MF project (rule 1) that opens as one.
    REQUIRE(run({"library", "save", "--dir", lib.string(), "--new", "--model",
                 (fs::path(TISMA_TEST_DATA_DIR) / "cube_ascii.stl").string(), "--config", f.settings.string(),
                 "--recipe", f.recipe.string()},
                out) == 0);
    const std::string id = json::parse(out).at("id");
    const Project p = read_project(slurp(lib / id / "project.3mf"));
    REQUIRE(p.models.size() == 1);
    CHECK(p.models[0].name == "cube_ascii");
    CHECK(p.models[0].mesh.triangle_count() == 12);
    REQUIRE(run({"library", "list", "--dir", lib.string()}, out) == 0);
    CHECK(json::parse(out).at("entries")[0].at("name") == "Cube");
    CHECK(run({"library", "show", "--dir", lib.string(), "--entry", "nope"}, out) == 2);
    const json error = json::parse(out);
    CHECK(error.at("code") == "entry");
    CHECK(error.contains("error_es"));
}
