// The free profile pack (profile_pack.md, profile_pack_format.md), with a
// small pack of invented data that follows the format.
#include "catalog/pack.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

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
}
