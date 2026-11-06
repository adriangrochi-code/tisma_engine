// The free profile pack (profile_pack.md, profile_pack_format.md), with a
// small pack of invented data that follows the format.
#include "catalog/pack.hpp"
#include "cli/cli.hpp"
#include "config/settings.hpp"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace tisma;
using Catch::Matchers::ContainsSubstring;

namespace {

const char* kIndex = R"({
 "format": "tisma-profile-pack", "version": 2, "pack_version": "2026.11.01-abc1234",
 "printers": ["acme/models/A1.json#0.4", "acme/models/A1.json#0.6", "bolt/models/B2.json#0.4"],
 "compatible": {"name": "compatible.json", "size": 120, "sha256": "AA11"},
 "community": {"name": "community.tar.xz", "size": 900, "sha256": "bb22"},
 "vendors": [
  {"id": "acme", "name": "Acme", "license": "AGPL-3.0-or-later", "origin": "Invented",
   "printers": ["acme/models/A1.json#0.4", "acme/models/A1.json#0.6"],
   "filaments": ["vendors/acme/filaments/PLA.json"], "prints": ["vendors/acme/prints/Standard.json"],
   "archive": {"name": "acme.tar.xz", "size": 4096, "sha256": "cc33"},
   "models": [{"id": "A1", "name": "Acme One", "technology": "fff", "family": "One", "file": "vendors/acme/models/A1.json",
               "default_variant": "0.4", "variants": [{"id": "0.4", "nozzle": 0.4}, {"id": "0.6", "nozzle": 0.6}],
               "assets": {"thumbnail": "vendors/acme/A1.png"}}]},
  {"id": "bolt", "name": "Bolt", "printers": ["bolt/models/B2.json#0.4"],
   "archive": {"name": "bolt.tar.xz", "size": 2048, "sha256": "dd44"}, "models": []}
 ]})";

const char* kModel = R"({
 "format": "tisma-pack-model", "version": 2, "technology": "fff", "vendor": "Acme",
 "model": {"id": "A1", "name": "Acme One", "family": "One"},
 "default_variant": "0.4",
 "common": {"settings": {"machine": {"max_bed_temp": 110, "nozzle_diameter": 0.4},
                         "speed": {"travel": 150}, "gizmo": {"warp": 9}}},
 "variants": [{"id": "0.4", "nozzle": 0.4, "name": "Acme One 0.4", "settings": {}},
              {"id": "0.6", "nozzle": 0.6, "name": "Acme One 0.6", "settings": {"machine": {"nozzle_diameter": 0.6}}}],
 "original_common": {"machine": {"max_bed_temp": 999}},
 "license": "AGPL-3.0-or-later", "origin": "Invented"})";

} // namespace

TEST_CASE("pack: SHA-256 against FIPS 180-4's examples", "[pack]") {
    CHECK(sha256_hex("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(sha256_hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(sha256_hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

TEST_CASE("pack: the index, a newer version refused (PF1), and the plan of downloads", "[pack]") {
    const PackIndex index = parse_pack_index(kIndex);
    CHECK(index.pack_version == "2026.11.01-abc1234");
    REQUIRE(index.vendors.size() == 2);
    CHECK(index.vendors[0].models[0].variants.size() == 2);
    CHECK(index.vendors[0].archive.sha256 == "cc33");
    CHECK(index.compatible.sha256 == "aa11"); // lowercase
    std::string newer = kIndex;
    newer.replace(newer.find("\"version\": 2"), 12, "\"version\": 3");
    CHECK_THROWS_WITH(parse_pack_index(newer), ContainsSubstring("newer version"));
    CHECK_THROWS_AS(parse_pack_index(R"({"format": "other", "version": 1})"), PackError);
    // Without an index: the index. With it: compatible, community, knowledge and the vendors asked.
    const auto first = pack_plan(nullptr, "https://example.org/pack", {});
    REQUIRE(first.size() == 1);
    CHECK(first[0].url == "https://example.org/pack/index.json");
    const auto plan = pack_plan(&index, "https://example.org/pack/", {"acme"});
    REQUIRE(plan.size() == 4);
    CHECK(plan[0].name == "compatible.json");
    CHECK(plan[0].size == 120);
    CHECK(plan[3].name == "acme.tar.xz");
    CHECK(plan[3].sha256 == "cc33");
    CHECK(pack_plan(&index, "", {}).size() == 5); // every vendor
    // An update: only the vendor whose archive changed (rule 20).
    PackIndex installed = index;
    installed.vendors[1].archive.sha256 = "old";
    const auto update = pack_plan(&index, "", {}, &installed);
    CHECK(update.back().name == "bolt.tar.xz");
    CHECK(std::none_of(update.begin(), update.end(), [](const PackDownload& d) { return d.name == "acme.tar.xz"; }));
}

TEST_CASE("pack: only safe paths are unpacked (rule 5, PF3)", "[pack]") {
    CHECK(safe_pack_path("vendors/acme/models/A1.json"));
    CHECK(safe_pack_path("community/printers/Acme/One.json"));
    CHECK(safe_pack_path("knowledge/problems.json"));
    CHECK_FALSE(safe_pack_path("../x.json"));
    CHECK_FALSE(safe_pack_path("vendors/acme/../../x.json"));
    CHECK_FALSE(safe_pack_path("/etc/passwd"));
    CHECK_FALSE(safe_pack_path("vendors//x.json"));
    CHECK_FALSE(safe_pack_path("other/x.json"));
    CHECK_FALSE(safe_pack_path("vendors\\acme\\x.json"));
    CHECK_FALSE(safe_pack_path("C:/x.json"));
    CHECK_FALSE(safe_pack_path("knowledge/./x.json"));
}

TEST_CASE("pack: a variant's printer profile is common with the variant on top (PF4); unknown keys and original left out (PF6)",
          "[pack]") {
    std::vector<std::string> warnings;
    const Profile p06 = pack_printer_profile(kModel, "0.6", warnings);
    CHECK(p06.kind == ProfileKind::Printer);
    CHECK(p06.name == "Acme One 0.6");
    CHECK(p06.settings.nozzle_diameter == 0.6);  // the variant's
    CHECK(p06.settings.max_bed_temp == 110.0);     // common's
    CHECK(p06.settings.travel_speed == 150.0);
    REQUIRE_FALSE(warnings.empty());
    CHECK_THAT(warnings.front(), ContainsSubstring("gizmo.warp"));
    warnings.clear();
    const Profile p04 = pack_printer_profile(kModel, "", warnings); // the default variant
    CHECK(p04.settings.nozzle_diameter == 0.4);
    CHECK(p04.settings.max_bed_temp == 110.0); // original_common's 999 never read
    CHECK_THROWS_AS(pack_printer_profile(kModel, "0.8", warnings), PackError);
}

TEST_CASE("pack: entries and compatible printers (PF5)", "[pack]") {
    std::vector<std::string> warnings;
    const PackEntry pla = parse_pack_entry(R"({"format": "tisma-pack-entry", "version": 2, "technology": "fff",
        "profile": {"format": "tisma-profile", "version": 1, "kind": "filament", "name": "Acme PLA",
                    "settings": {"temperature": {"nozzle": 205}}},
        "compatible": 1, "generic": false, "original": {"temperature": 999}, "license": "AGPL-3.0-or-later"})",
                                           warnings);
    CHECK(pla.profile.kind == ProfileKind::Filament);
    CHECK(pla.profile.name == "Acme PLA");
    REQUIRE(pla.compatible);
    CHECK(*pla.compatible == 1);
    const auto sets = parse_pack_compatible(R"({"format": "tisma-pack-compatible", "version": 1, "sets": [[0], [2]]})");
    CHECK_FALSE(pack_compatible(pla.compatible, sets, 0));
    CHECK(pack_compatible(pla.compatible, sets, 2)); // PF5: only printer 2
    CHECK(pack_compatible(std::nullopt, sets, 0));   // compatible null: every printer
    CHECK_FALSE(pack_compatible(7, sets, 2));         // a set that is not there
    // An empty set is taken as null (rule 12, BH29): every printer.
    const auto with_empty = parse_pack_compatible(R"({"format": "tisma-pack-compatible", "version": 1, "sets": [[0], []]})");
    CHECK(pack_compatible(1, with_empty, 5));
    // A filament's material gives its density and price per kg (entries,
    // rule 4); the profile's own win. The Android app saw a cost of 0.
    const PackEntry petg = parse_pack_entry(R"({"format": "tisma-pack-entry", "version": 2, "technology": "fff",
        "profile": {"format": "tisma-profile", "version": 1, "kind": "filament", "name": "Acme PETG", "settings": {}},
        "material": {"type": "PETG", "density": 1.27, "cost_per_kg": 30.0}, "compatible": null})",
                                            warnings);
    CHECK(petg.profile.settings.filament_density == 1.27);
    CHECK(petg.profile.settings.filament_cost == 30.0);
    const PackEntry own = parse_pack_entry(R"({"format": "tisma-pack-entry", "version": 2, "technology": "fff",
        "profile": {"format": "tisma-profile", "version": 1, "kind": "filament", "name": "Acme PETG+",
                    "settings": {"extrusion": {"cost": 45}}},
        "material": {"density": 1.27, "cost_per_kg": 30.0}})",
                                           warnings);
    CHECK(own.profile.settings.filament_cost == 45.0);
    CHECK(own.profile.settings.filament_density == 1.27);
}

// ---------------------------------------------------------------------------
// Installing (rules 4, 5, 7, 20, 21) from tests/data/pack, made by
// tests/data/pack/make_pack_data.py: release1 (two vendors), release2 (only
// Acme changed) and evil (a path out of the folder, a link).

namespace {

namespace fs = std::filesystem;

const fs::path kPackData = fs::path(TISMA_TEST_DATA_DIR) / "pack";

fs::path fresh_dir(const char* name) {
    const fs::path dir = fs::temp_directory_path() / name;
    fs::remove_all(dir);
    fs::create_directories(dir);
    return dir;
}

fs::path staging_from(const char* release, const char* name) {
    const fs::path dir = fresh_dir(name);
    for (const auto& e : fs::directory_iterator(kPackData / release))
        fs::copy_file(e.path(), dir / e.path().filename());
    return dir;
}

std::string text_of(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::ostringstream s;
    s << in.rdbuf();
    return s.str();
}

} // namespace

TEST_CASE("pack install: a first install writes every archive under the pack folder", "[pack]") {
    const fs::path root = fresh_dir("tisma_pack_first");
    const fs::path pack = root / "pack";
    std::vector<std::string> lines;
    const PackInstallReport r = pack_install(pack, staging_from("release1", "tisma_pack_stage1"),
                                             [&](const std::string& l) { lines.push_back(l); });
    CHECK(r.pack_version == "2026.11.01-aaaaaaa");
    CHECK(r.discarded.empty());
    CHECK(r.installed.size() == 5); // compatible, community, knowledge, acme, bolt
    CHECK(fs::exists(pack / "vendors/acme/models/A1.json"));
    CHECK(fs::exists(pack / "vendors/bolt/models/B2.json"));
    CHECK(fs::exists(pack / "community/printers/Acme/Acme One.json"));
    CHECK(fs::exists(pack / "knowledge/glossary.json"));
    CHECK(fs::exists(pack / "compatible.json"));
    CHECK(text_of(pack / "index.json") == text_of(kPackData / "release1/index.json")); // kept as published
    CHECK(!fs::exists(root / "pack.new"));
    CHECK(!lines.empty());
    // What is installed is read back with the archives' SHA-256 for the next plan.
    const PackIndex installed = pack_installed(pack);
    REQUIRE(installed.vendors.size() == 2);
    CHECK(installed.vendors[0].archive.sha256 == sha256_hex(text_of(kPackData / "release1/acme.tar.xz")));
    // PF4 on the installed model.
    std::vector<std::string> warnings;
    const Profile p06 = pack_printer_profile(text_of(pack / "vendors/acme/models/A1.json"), "0.6", warnings);
    CHECK(p06.name == "Acme One 0.6");
    CHECK(p06.settings.nozzle_diameter == 0.6);
}

TEST_CASE("pack install: an update downloads and replaces only the changed vendor (rule 20, PK12)", "[pack]") {
    const fs::path pack = fresh_dir("tisma_pack_update") / "pack";
    pack_install(pack, staging_from("release1", "tisma_pack_stage2a"));
    const std::string bolt_before = text_of(pack / "vendors/bolt/models/B2.json");
    const PackIndex published = parse_pack_index(text_of(kPackData / "release2/index.json"));
    const PackIndex installed = pack_installed(pack);
    const auto plan = pack_plan(&published, "", {"acme", "bolt"}, &installed);
    CHECK(std::any_of(plan.begin(), plan.end(), [](const PackDownload& d) { return d.name == "acme.tar.xz"; }));
    CHECK(std::none_of(plan.begin(), plan.end(), [](const PackDownload& d) { return d.name == "bolt.tar.xz"; }));
    // release2 holds only the new index and Acme's archive.
    const PackInstallReport r = pack_install(pack, staging_from("release2", "tisma_pack_stage2b"));
    CHECK(r.pack_version == "2026.11.04-bbbbbbb");
    CHECK(r.installed == std::vector<std::string>{"acme.tar.xz"});
    CHECK(std::find(r.kept.begin(), r.kept.end(), "bolt.tar.xz") != r.kept.end());
    CHECK(text_of(pack / "vendors/acme/models/A1.json").find("Acme One Plus") != std::string::npos);
    CHECK(text_of(pack / "vendors/bolt/models/B2.json") == bolt_before);
    CHECK(fs::exists(pack / "knowledge/glossary.json")); // kept too
    CHECK(pack_installed(pack).vendors[1].archive.sha256 == sha256_hex(text_of(kPackData / "release1/bolt.tar.xz")));
}

TEST_CASE("pack install: a changed byte or a cut download is discarded, the earlier one stays (PF2, PK3, PK14)", "[pack]") {
    const fs::path pack = fresh_dir("tisma_pack_bad") / "pack";
    pack_install(pack, staging_from("release1", "tisma_pack_stage3a"));
    const fs::path stage = staging_from("release2", "tisma_pack_stage3b");
    std::string acme = text_of(stage / "acme.tar.xz");
    acme[acme.size() / 2] = char(acme[acme.size() / 2] ^ 1);
    std::ofstream(stage / "acme.tar.xz", std::ios::binary) << acme;
    PackInstallReport r = pack_install(pack, stage);
    CHECK(r.discarded == std::vector<std::string>{"acme.tar.xz"});
    REQUIRE(r.warnings.size() == 1);
    CHECK_THAT(r.warnings[0], ContainsSubstring("SHA-256"));
    CHECK(text_of(pack / "vendors/acme/models/A1.json").find("Acme One Plus") == std::string::npos);
    // Still the earlier archive's SHA-256, so the next plan downloads it again.
    CHECK(pack_installed(pack).vendors[0].archive.sha256 == sha256_hex(text_of(kPackData / "release1/acme.tar.xz")));
    // Cut to half.
    std::ofstream(stage / "acme.tar.xz", std::ios::binary) << text_of(kPackData / "release2/acme.tar.xz").substr(0, 300);
    r = pack_install(pack, stage);
    CHECK(r.discarded == std::vector<std::string>{"acme.tar.xz"});
    CHECK_THAT(r.warnings[0], ContainsSubstring("size"));
    CHECK(fs::exists(pack / "vendors/acme/filaments/PLA.json"));
}

TEST_CASE("pack install: a path out of the folder or a link discards the whole archive (rule 5, PF3, PK4)", "[pack]") {
    const fs::path root = fresh_dir("tisma_pack_evil");
    const PackInstallReport r = pack_install(root / "pack", staging_from("evil", "tisma_pack_stage4"));
    CHECK(r.discarded.size() == 2);
    CHECK(r.installed.empty());
    CHECK(!fs::exists(root / "pack/vendors/acme"));
    CHECK(!fs::exists(root / "pack/vendors/bolt"));
    CHECK(!fs::exists(root / "x.json"));
    CHECK(!fs::exists(root / "pack/x.json"));
}

TEST_CASE("pack install: a newer index changes nothing (PF1, PK5)", "[pack]") {
    const fs::path pack = fresh_dir("tisma_pack_newer") / "pack";
    pack_install(pack, staging_from("release1", "tisma_pack_stage5a"));
    const fs::path stage = staging_from("release2", "tisma_pack_stage5b");
    std::string index = text_of(stage / "index.json");
    index.replace(index.find("\"version\": 2"), 12, "\"version\": 3");
    std::ofstream(stage / "index.json", std::ios::binary) << index;
    CHECK_THROWS_AS(pack_install(pack, stage), PackError);
    CHECK(pack_installed(pack).pack_version == "2026.11.01-aaaaaaa");
}

// The command line with `args`, its output in `out` and `err`.
int run(std::vector<std::string> args, std::string& out, std::string& err) {
    std::vector<std::string_view> views(args.begin(), args.end());
    std::ostringstream o, e;
    const int code = run_cli(views, o, e);
    out = o.str();
    err = e.str();
    return code;
}

TEST_CASE("pack on the command line: plan, install, list, and a pack printer's settings (PK6, PK7)", "[pack]") {
    const fs::path data = fresh_dir("tisma_pack_cli");
    std::string out, err;
    const std::string d = data.string(), stage = (kPackData / "release1").string();
    REQUIRE(run({"pack", "plan", "--data", d}, out, err) == 0);
    CHECK_THAT(out, ContainsSubstring("index.json"));
    REQUIRE(run({"pack", "plan", "--data", d, "--index", stage + "/index.json", "--vendors", "acme"}, out, err) == 0);
    CHECK_THAT(out, ContainsSubstring("acme.tar.xz"));
    CHECK_THAT(out, !ContainsSubstring("bolt.tar.xz"));
    REQUIRE(run({"pack", "install", "--data", d, "--from", stage}, out, err) == 0);
    CHECK_THAT(err, ContainsSubstring("progress: pack 2026.11.01-aaaaaaa ready"));
    REQUIRE(run({"pack", "list", "--data", d}, out, err) == 0);
    CHECK_THAT(out, ContainsSubstring("vendors/acme/models/A1.json#0.6"));
    // PK6: the variant's own filament and print profile by default.
    REQUIRE(run({"profile", "--pack", d, "--printer", "vendors/acme/models/A1.json#0.4"}, out, err) == 0);
    const Settings s = parse_settings(out);
    CHECK(s.nozzle_diameter == 0.4);
    CHECK(s.travel_speed == 300.0); // the print profile's, over the printer's 150 (BH27)
    CHECK(s.wall_speed == 170.0);
    CHECK(s.min_print_speed == 12.0); // the filament's cooling.min_speed
    CHECK(s.layer_height == 0.16); // the print profile, not the default
    REQUIRE(s.variables.count("temperature"));
    CHECK(s.variables.at("temperature") == std::vector<std::string>{"210"});
    // PK7: a filament whose set has not the printer.
    CHECK(run({"profile", "--pack", d, "--printer", "vendors/bolt/models/B2.json#default", "--filament",
               "vendors/acme/filaments/PLA.json"}, out, err) == 2);
    CHECK_THAT(err, ContainsSubstring("is not for the Bolt Two"));
    // Not in the pack.
    CHECK(run({"profile", "--pack", d, "--printer", "vendors/acme/models/A1.json#0.8"}, out, err) == 2);
    CHECK(run({"pack", "list", "--data", (data / "none").string()}, out, err) == 2);
    REQUIRE(run({"pack", "community", "--data", d, "--printer", "Acme/Acme One"}, out, err) == 0);
    CHECK_THAT(out, ContainsSubstring("\"kind\": \"measured\""));
    CHECK_THAT(out, ContainsSubstring("18.5"));
}

TEST_CASE("pack: the models a search finds, and a printer's profiles with its defaults (rules 2, 13)", "[pack]") {
    const fs::path pack = fresh_dir("tisma_pack_find") / "pack";
    pack_install(pack, staging_from("release1", "tisma_pack_stage_find"));
    const PackIndex index = pack_installed(pack);
    CHECK(find_pack_models(index, "").size() == 2);
    const auto one = find_pack_models(index, "acme ONE");
    REQUIRE(one.size() == 1);
    CHECK(one[0].model->name == "Acme One");
    CHECK(find_pack_models(index, "bolt one").empty());
    const PackProfiles p = pack_profiles(pack, index, "vendors/acme/models/A1.json#0.4");
    CHECK(p.number == 0);
    CHECK(p.printer.name == "Acme One 0.4");
    REQUIRE(p.filament);
    CHECK(p.filament->name == "Acme PLA");
    REQUIRE(p.print);
    CHECK(p.print->settings.layer_height == 0.16);
    // BH27: the speeds a print profile of the pack carries (a version 1 profile,
    // as other programs keep them) go to the printer profile, not lost.
    CHECK(p.printer.settings.wall_speed == 170.0);
    CHECK(p.printer.settings.travel_speed == 300.0);
    CHECK(p.printer.settings.min_print_speed == 12.0);
    std::vector<std::string> warnings;
    const PackEntry print = read_pack_entry(pack, "vendors/acme/prints/Standard.json", warnings);
    CHECK(nlohmann::json::parse(print.printer_settings) == nlohmann::json::parse("{\"speed\":{\"walls\":170,\"travel\":300}}"));
    CHECK(print.profile.kind == ProfileKind::Print);
    CHECK_THROWS_AS(pack_profiles(pack, index, "vendors/bolt/models/B2.json#default", "vendors/acme/filaments/PLA.json"),
                    PackError);
}

TEST_CASE("pack: the filaments offered for a printer (rule 12, PK7)", "[pack]") {
    const fs::path pack = fresh_dir("tisma_pack_filaments") / "pack";
    pack_install(pack, staging_from("release1", "tisma_pack_stage_filaments"));
    const PackIndex index = pack_installed(pack);
    // Acme PLA is in set 0, the Acme One's two printers (0 and 1); not the Bolt Two (2).
    const auto for_acme = pack_filaments_for(pack, index, 1);
    REQUIRE(for_acme.size() == 1);
    CHECK(for_acme[0].name == "Acme PLA");
    CHECK(for_acme[0].path == "vendors/acme/filaments/PLA.json");
    for (const PackFilamentRef& f : pack_filaments_for(pack, index, 2)) // the resin printer: its resins only
        CHECK(f.name != "Acme PLA");
    std::vector<std::string> warnings;
    CHECK(pack_entry_profile(pack, "vendors/acme/filaments/PLA.json", warnings).kind == ProfileKind::Filament);
    CHECK_THROWS_AS(pack_entry_profile(pack, "../x.json", warnings), PackError);
}

TEST_CASE("pack: a variant's defaults always valid for it, an empty set for every printer (rule 12, BH29)", "[pack]") {
    const fs::path pack = fresh_dir("tisma_pack_bh29") / "pack";
    pack_install(pack, staging_from("release1", "tisma_pack_stage_bh29"));
    const PackIndex index = pack_installed(pack);
    // Acme Standard is in set 1 (the Bolt Two only), but it is the Acme One
    // 0.4's default print: chosen and offered for it, not for the 0.6.
    const PackProfiles p = pack_profiles(pack, index, "vendors/acme/models/A1.json#0.4");
    REQUIRE(p.print);
    CHECK(p.print->name == "Acme Standard");
    const auto names = [](const std::vector<PackFilamentRef>& refs) {
        std::vector<std::string> out;
        for (const PackFilamentRef& r : refs)
            out.push_back(r.name);
        return out;
    };
    CHECK(names(pack_prints_for(pack, index, 0)) == std::vector<std::string>{"Acme Standard", "Acme Fine"});
    // Valid for every printer but offered only where they fit (BH32): Acme
    // Draft is for a 0.6 mm nozzle, Acme Coarse's 0.5 mm layers too thick
    // for a 0.4 mm one.
    CHECK(names(pack_prints_for(pack, index, 1)) ==
          std::vector<std::string>{"Acme Fine", "Acme Draft", "Acme Coarse", "Acme Wide"});
    // Without a nozzle, Acme Wide's 0.82 mm walls say it is not for 0.4 mm (BH35).
    CHECK_THROWS_WITH(pack_profiles(pack, index, "vendors/acme/models/A1.json#0.4", "", "vendors/acme/prints/Wide.json"),
                      Catch::Matchers::ContainsSubstring("made for another nozzle"));
    CHECK_THROWS_WITH(pack_profiles(pack, index, "vendors/acme/models/A1.json#0.4", "", "vendors/acme/prints/Draft.json"),
                      Catch::Matchers::ContainsSubstring("does not fit the") && Catch::Matchers::ContainsSubstring("0.6 mm nozzle"));
    CHECK_THROWS_WITH(pack_profiles(pack, index, "vendors/acme/models/A1.json#0.4", "", "vendors/acme/prints/Coarse.json"),
                      Catch::Matchers::ContainsSubstring("does not fit the") && Catch::Matchers::ContainsSubstring("thicker"));
    CHECK(pack_profiles(pack, index, "vendors/acme/models/A1.json#0.6", "", "vendors/acme/prints/Coarse.json").print);
    CHECK_THROWS_AS(pack_profiles(pack, index, "vendors/acme/models/A1.json#0.6", "", "vendors/acme/prints/Standard.json"),
                    PackError);
    // Acme Fine points to set 2, empty: offered with every printer, and
    // logged as a pack error when it is chosen.
    const PackProfiles fine = pack_profiles(pack, index, "vendors/acme/models/A1.json#0.6", "", "vendors/acme/prints/Fine.json");
    REQUIRE(fine.print);
    CHECK(std::any_of(fine.warnings.begin(), fine.warnings.end(), [](const std::string& w) {
        return w.find("compatible set #2, which is empty") != std::string::npos;
    }));
}

TEST_CASE("pack: a resin printer's resin and resin print profile (rules 10, 12, 13)", "[pack]") {
    const fs::path pack = fresh_dir("tisma_pack_resin") / "pack";
    pack_install(pack, staging_from("release1", "tisma_pack_stage_resin"));
    const PackIndex index = pack_installed(pack);
    CHECK(pack_printer_is_resin(index, 2));
    CHECK(!pack_printer_is_resin(index, 0));
    CHECK(!pack_printer_is_resin(index, 7));
    // Its defaults: the first resin and the resin print profile.
    const PackProfiles p = pack_profiles(pack, index, "vendors/bolt/models/B2.json#default");
    CHECK(p.resin);
    CHECK(p.printer.settings.machine_technology == MachineTechnology::Msla); // the model's "sla"
    REQUIRE(p.filament);
    CHECK(p.filament->name == "Bolt Grey");
    CHECK(p.filament->settings.resin_exposure == 3.5);
    CHECK(p.filament_vendor == "Bolt");
    REQUIRE(p.print);
    CHECK(p.print->settings.resin_layer_height == 0.03);
    CHECK(!pack_profiles(pack, index, "vendors/acme/models/A1.json#0.4").resin);
    // A resin is offered only for the resin printer; a filament never for it.
    const auto offered = pack_filaments_for(pack, index, 2);
    REQUIRE(offered.size() == 1);
    CHECK(offered[0].name == "Bolt Grey");
    CHECK(offered[0].vendor == "Bolt");
    for (const PackFilamentRef& f : pack_filaments_for(pack, index, 0))
        CHECK(f.name != "Bolt Grey");
    // A resin on a filament printer is refused (even with every printer compatible).
    std::vector<std::string> warnings;
    CHECK(read_pack_entry(pack, "vendors/bolt/resins/Grey.json", warnings).resin);
    CHECK_THROWS_AS(pack_profiles(pack, index, "vendors/acme/models/A1.json#0.4", "vendors/bolt/resins/Grey.json"),
                    PackError);
    // Its community data (rule 23).
    const auto values = pack_community(pack, "resins", "Bolt", "Bolt Grey");
    REQUIRE(values.size() == 2);
    CHECK(values[0].key == "resin.exposure");
    CHECK(values[1].kind == CommunityKind::Community);
}

TEST_CASE("pack plan on the command line: a broken install is planned whole, so it can be repaired", "[pack]") {
    const fs::path data = fresh_dir("tisma_pack_broken_plan");
    fs::create_directories(data / "pack");
    std::ofstream(data / "pack/index.json") << "garbage";
    std::string out, err;
    REQUIRE(run({"pack", "plan", "--data", data.string(), "--index", (kPackData / "release1/index.json").string()}, out,
                err) == 0);
    CHECK_THAT(err, ContainsSubstring("cannot be read"));
    CHECK_THAT(out, ContainsSubstring("acme.tar.xz"));
    CHECK_THAT(out, ContainsSubstring("bolt.tar.xz"));
    CHECK_THAT(out, ContainsSubstring("\"installed\": null"));
    CHECK(run({"pack"}, out, err) == 1);
    CHECK_THAT(err, ContainsSubstring("list or community"));
}

TEST_CASE("pack on the command line: a resin printer's settings (rule 13)", "[pack]") {
    const fs::path data = fresh_dir("tisma_pack_resin_cli");
    pack_install(data / "pack", staging_from("release1", "tisma_pack_stage_resin_cli"));
    const std::string d = data.string();
    std::string out, err;
    REQUIRE(run({"profile", "--pack", d, "--printer", "vendors/bolt/models/B2.json#default"}, out, err) == 0);
    const Settings s = parse_settings(out);
    CHECK(s.resin_exposure == 3.5);
    CHECK(s.resin_layer_height == 0.03);
    REQUIRE(run({"pack", "community", "--data", d, "--resin", "Bolt/Bolt Grey"}, out, err) == 0);
    CHECK_THAT(out, ContainsSubstring("resin.viscosity"));
}

TEST_CASE("pack: community data by printer, with its kind and source (rules 22 to 24, PK13)", "[pack]") {
    const fs::path pack = fresh_dir("tisma_pack_community") / "pack";
    pack_install(pack, staging_from("release1", "tisma_pack_stage_community"));
    const auto values = pack_community(pack, "printers", "Acme", "Acme One");
    REQUIRE(values.size() == 3); // an unknown key and an unknown kind are left out
    CHECK(values[0].key == "machine.max_flow");
    CHECK(values[0].kind == CommunityKind::Official);
    CHECK(values[0].value == "15");
    CHECK(values[0].source.url == "https://example.org/acme-one");
    CHECK(values[1].kind == CommunityKind::Measured);
    CHECK(values[1].value == "18.5");
    CHECK(values[1].source.at == "3:20");
    CHECK(values[2].value == "\"mzv\"");
    // Rule 24: only an official value fills an empty engine setting by itself.
    CHECK(community_fills_alone(values[0], true));
    CHECK_FALSE(community_fills_alone(values[0], false));
    CHECK_FALSE(community_fills_alone(values[1], true)); // PK13: measured needs the person
    CHECK_FALSE(community_fills_alone(values[2], true));
    CHECK(pack_community(pack, "printers", "Acme", "Nobody").empty());
    CHECK(pack_community(pack, "printers", "..", "x").empty());
    CHECK(pack_community(pack, "secrets", "Acme", "Acme One").empty());
}
