// Projects of other programs (docs/spec/features/project_import.md): which
// program saved a 3MF (rule 1), and a standard 3MF otherwise (rule 2).

#include "io/model.hpp"
#include "io/project_import.hpp"
#include "io/zip.hpp"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace tisma;

namespace {

// A package with a root model naming `application` (none if empty) and
// these other files.
std::string package(const std::string& application, const std::vector<std::string>& files) {
    std::string model = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<model unit=\"millimeter\">\n";
    if (!application.empty())
        model += " <metadata name=\"Application\">" + application + "</metadata>\n";
    model += " <resources/>\n <build/>\n</model>\n";
    std::vector<ZipEntryToWrite> entries = {{"3D/3dmodel.model", model}};
    for (const std::string& f : files)
        entries.push_back({f, "x"});
    return write_zip_stored(entries);
}

} // namespace

TEST_CASE("project import: the program is recognized by its files and named by its metadata (rule 1)", "[import]") {
    const ProjectSource prusa = project_source(package("PrusaSlicer-2.8.1", {"Metadata/Slic3r_PE.config"}));
    CHECK(prusa.family == ProjectFamily::PrusaSlicer);
    CHECK(prusa.application == "PrusaSlicer-2.8.1");
    CHECK(prusa.program == "PrusaSlicer");

    const ProjectSource tisma = project_source(package("Tisma Slicer 1.2", {"Metadata/Slic3r_PE_model.config"}));
    CHECK(tisma.family == ProjectFamily::PrusaSlicer);
    CHECK(tisma.program == "Tisma Slicer");

    const ProjectSource bambu = project_source(
        package("BambuStudio-01.10.00", {"Metadata/project_settings.config", "Metadata/model_settings.config"}));
    CHECK(bambu.family == ProjectFamily::OrcaSlicer);
    CHECK(bambu.program == "Bambu Studio");

    const ProjectSource creality = project_source(package("Creality Print 6.0", {"Metadata/project_settings.config"}));
    CHECK(creality.family == ProjectFamily::OrcaSlicer);
    CHECK(creality.program == "Creality Print");

    // No name in the metadata: the family's.
    const ProjectSource orca = project_source(package("", {"Metadata/model_settings.config"}));
    CHECK(orca.family == ProjectFamily::OrcaSlicer);
    CHECK(orca.application.empty());
    CHECK(orca.program == "OrcaSlicer");

    const ProjectSource cura = project_source(package("Cura 5.8", {"Cura/creality_ender3.def.json"}));
    CHECK(cura.family == ProjectFamily::Cura);
    CHECK(cura.program == "Cura");
}

TEST_CASE("project import: a 3MF of an unknown program is a standard one (rule 2, PI1)", "[import]") {
    const ProjectSource plain = project_source(package("Some CAD 3", {"Metadata/thumbnail.png"}));
    CHECK(plain.family == ProjectFamily::Standard);
    CHECK(plain.application == "Some CAD 3");
    CHECK(plain.program.empty());
    // No model at all, and an escaped name.
    CHECK(project_source(write_zip_stored({{"Metadata/x.txt", "y"}})).family == ProjectFamily::Standard);
    CHECK(project_source(package("A &amp; B", {})).application == "A & B");
}

TEST_CASE("project import: a file that is not a ZIP is refused", "[import]") {
    CHECK_THROWS_AS(project_source("solid cube\nendsolid\n"), ZipError);
}

TEST_CASE("project import: an OrcaSlicer-family package, its meshes in 3D/Objects (rule 3)", "[import]") {
    // The layout of the 3MF Production Extension: the root model's object is
    // made of components whose mesh is in another model file of the package.
    const std::string cube =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<model unit=\"millimeter\" xmlns=\"http://schemas.microsoft.com/3dmanufacturing/core/2015/02\">\n"
        " <resources><object id=\"1\" type=\"model\"><mesh><vertices>"
        "<vertex x=\"0\" y=\"0\" z=\"0\"/><vertex x=\"10\" y=\"0\" z=\"0\"/><vertex x=\"10\" y=\"10\" z=\"0\"/>"
        "<vertex x=\"0\" y=\"10\" z=\"0\"/><vertex x=\"0\" y=\"0\" z=\"10\"/><vertex x=\"10\" y=\"0\" z=\"10\"/>"
        "<vertex x=\"10\" y=\"10\" z=\"10\"/><vertex x=\"0\" y=\"10\" z=\"10\"/></vertices><triangles>"
        "<triangle v1=\"0\" v2=\"2\" v3=\"1\"/><triangle v1=\"0\" v2=\"3\" v3=\"2\"/>"
        "<triangle v1=\"4\" v2=\"5\" v3=\"6\"/><triangle v1=\"4\" v2=\"6\" v3=\"7\"/>"
        "<triangle v1=\"0\" v2=\"1\" v3=\"5\"/><triangle v1=\"0\" v2=\"5\" v3=\"4\"/>"
        "<triangle v1=\"1\" v2=\"2\" v3=\"6\"/><triangle v1=\"1\" v2=\"6\" v3=\"5\"/>"
        "<triangle v1=\"2\" v2=\"3\" v3=\"7\"/><triangle v1=\"2\" v2=\"7\" v3=\"6\"/>"
        "<triangle v1=\"3\" v2=\"0\" v3=\"4\"/><triangle v1=\"3\" v2=\"4\" v3=\"7\"/>"
        "</triangles></mesh></object></resources>\n <build/>\n</model>\n";
    const std::string root =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<model unit=\"millimeter\" xmlns=\"http://schemas.microsoft.com/3dmanufacturing/core/2015/02\" "
        "xmlns:p=\"http://schemas.microsoft.com/3dmanufacturing/production/2015/06\">\n"
        " <metadata name=\"Application\">BambuStudio-01.10.00</metadata>\n"
        " <resources><object id=\"2\" type=\"model\"><components>"
        "<component p:path=\"/3D/Objects/object_1.model\" objectid=\"1\" transform=\"1 0 0 0 1 0 0 0 1 0 0 0\"/>"
        "</components></object></resources>\n"
        " <build><item objectid=\"2\" transform=\"1 0 0 0 1 0 0 0 1 100 50 0\"/></build>\n</model>\n";
    const std::string package = write_zip_stored({{"3D/3dmodel.model", root},
                                                  {"3D/Objects/object_1.model", cube},
                                                  {"Metadata/project_settings.config", "{}"},
                                                  {"Metadata/model_settings.config", "<config/>"}});
    CHECK(project_source(package).family == ProjectFamily::OrcaSlicer);
    const std::vector<ThreeMfItem> items = read_3mf_items(package);
    REQUIRE(items.size() == 1);
    CHECK(items[0].mesh.triangle_count() == 12);
    const BoundingBox box = items[0].mesh.bounding_box();
    CHECK(box.min.x == 100.0f); // placed by its build item
    CHECK(box.max.y == 60.0f);
}

TEST_CASE("project import: the spec team's PrusaSlicer 2.7.2 sample (PI2, its part so far)", "[import]") {
    // docs/spec/samples/project_import/pi2_prusaslicer.3mf (spec/next 2c763a7),
    // saved by PrusaSlicer's command line: one 30 x 10 x 15 mm box.
    std::ifstream in(std::filesystem::path(TISMA_TEST_DATA_DIR) / "formats" / "pi2_prusaslicer.3mf", std::ios::binary);
    REQUIRE(in);
    const std::string package((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const ProjectSource source = project_source(package);
    CHECK(source.family == ProjectFamily::PrusaSlicer);
    CHECK(source.application == "PrusaSlicer-2.7.2");
    CHECK(source.program == "PrusaSlicer");
    CHECK(source.items == 1);
    const std::vector<ThreeMfItem> items = read_3mf_items(package);
    REQUIRE(items.size() == 1);
    CHECK(items[0].mesh.triangle_count() == 12);
    const BoundingBox box = items[0].mesh.bounding_box();
    CHECK(box.max.x - box.min.x == 30.0f);
    CHECK(box.max.y - box.min.y == 10.0f);
    CHECK(box.max.z - box.min.z == 15.0f);
}

namespace {

// Rows as the profile pack writes them (profiles/next 8e2e5f0, given by the
// pack's session as data).
const char* kPrusaTable = R"({"format": "tisma-pack-translation", "version": 1, "dialect": "prusaslicer", "kinds": {
 "print": {
  "layer_height": {"engine": "layers.height", "group": "translated", "conversion": {"how": "number"}},
  "first_layer_height": {"engine": "layers.first_height", "group": "translated",
                         "conversion": {"how": "percent_of", "base": "layers.height"}},
  "external_perimeter_extrusion_width": {"engine": "walls.outer_width", "group": "similar",
                                         "conversion": {"how": "absolute_only"}, "note": "in mm"},
  "thin_walls": {"engine": null, "group": "none"}},
 "filament": {
  "extrusion_multiplier": {"engine": "extrusion.flow", "group": "translated", "conversion": {"how": "factor", "factor": 100}}},
 "printer": {
  "gcode_flavor": {"engine": "machine.flavor", "group": "similar", "conversion": {"how": "map", "values": {"marlin2": "marlin"}}},
  "start_gcode": {"engine": "machine.start_gcode, machine.end_gcode", "group": "translated",
                  "conversion": {"how": "rule", "rule": "gcode_template"}, "with": ["end_gcode"]}}}})";

} // namespace

TEST_CASE("project import: the pack's table sorts a project's settings into the three groups (rules 6, 9 and 11)",
          "[project_import]") {
    const TranslationTable table = parse_translation(kPrusaTable);
    CHECK(table.dialect == "prusaslicer");
    REQUIRE(table.kinds.size() == 3);
    CHECK(table.kinds.at("print").at("layer_height").engine == "layers.height");
    CHECK(table.kinds.at("print").at("thin_walls").group == SettingGroup::None);
    const SettingSort sort = sort_settings(table, {{"layer_height", "0.2"},
                                                   {"first_layer_height", "0.2"},
                                                   {"external_perimeter_extrusion_width", "0.45"},
                                                   {"thin_walls", "0"},
                                                   {"extrusion_multiplier", "1"},
                                                   {"gcode_flavor", "marlin2"},
                                                   {"start_gcode", "G28"},
                                                   {"end_gcode", "M84"},
                                                   {"post_process", "script.py"}});
    CHECK(sort.translated == 5); // layer heights, flow, start and end G-code
    CHECK(sort.similar == 2);    // outer wall width, flavor
    CHECK(sort.none == 2);       // thin walls, and a key the table does not have
    REQUIRE(sort.detail.size() == 9);
    CHECK(sort.detail.back() == std::pair<std::string, SettingGroup>{"post_process", SettingGroup::None});
    // Not a table, or a newer one.
    CHECK_THROWS_AS(parse_translation(R"({"format": "other", "version": 1})"), TranslationError);
    CHECK_THROWS_AS(parse_translation(R"({"format": "tisma-pack-translation", "version": 2})"), TranslationError);
    CHECK_THROWS_AS(parse_translation("not json"), TranslationError);
}

TEST_CASE("project import: each program's table", "[project_import]") {
    ProjectSource s;
    CHECK(translation_dialect(s).empty());
    s.family = ProjectFamily::PrusaSlicer;
    CHECK(translation_dialect(s) == "prusaslicer");
    s.family = ProjectFamily::OrcaSlicer;
    s.application = "BambuStudio-01.10";
    CHECK(translation_dialect(s) == "orcaslicer");
    s.application = "Creality Print 6.0";
    CHECK(translation_dialect(s) == "crealityprint");
    s.application = "ElegooSlicer-1.1";
    CHECK(translation_dialect(s) == "elegooslicer");
    s.family = ProjectFamily::Cura;
    CHECK(translation_dialect(s) == "cura");
}

TEST_CASE("project import: a PrusaSlicer project's own settings, and how many the table brings (rules 6 and 9, pi4)",
          "[project_import]") {
    std::ifstream in(std::filesystem::path(TISMA_TEST_DATA_DIR) / "formats" / "pi4_prusaslicer_parts.3mf", std::ios::binary);
    REQUIRE(in);
    const std::string package((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    CHECK(project_source(package).family == ProjectFamily::PrusaSlicer);
    const auto settings = project_settings(package);
    CHECK(settings.size() == 327);
    const auto find = [&](const std::string& key) {
        for (const auto& [k, v] : settings)
            if (k == key)
                return v;
        return std::string("(none)");
    };
    CHECK(find("layer_height") == "0.2");
    CHECK(find("first_layer_height") == "0.2");
    CHECK(find("end_gcode").starts_with("{if layer_z < max_print_height}"));
    // The generator's comment is not a setting.
    CHECK(find("generated") == "(none)");
    const SettingSort sort = sort_settings(parse_translation(kPrusaTable), settings);
    CHECK(sort.translated + sort.similar + sort.none == 327);
    // With the rows above: layer and first layer height, flow, start and end
    // G-code translated; outer wall width and flavor similar; the rest without.
    CHECK(sort.translated == 5);
    CHECK(sort.similar == 2);
    CHECK(sort.none == 320);
    // A standard 3MF has none.
    std::ifstream box(std::filesystem::path(TISMA_TEST_DATA_DIR) / "formats" / "box.3mf", std::ios::binary);
    CHECK(project_settings(std::string((std::istreambuf_iterator<char>(box)), std::istreambuf_iterator<char>())).empty());
}

TEST_CASE("project import: a PrusaSlicer project's kinds of part (rule 4, pi4)", "[project_import]") {
    std::ifstream in(std::filesystem::path(TISMA_TEST_DATA_DIR) / "formats" / "pi4_prusaslicer_parts.3mf", std::ios::binary);
    REQUIRE(in);
    const std::string package((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const ModelFile m = parse_3mf(package);
    // The cube (12 triangles), a negative sphere, a modifier cylinder and a
    // support blocker box, as the project's config lists them.
    CHECK(m.parts == 1);
    CHECK(m.mesh.triangle_count() == 12);
    CHECK(m.negative.triangle_count() == 1280);
    REQUIRE(m.modifiers.size() == 1);
    CHECK(m.modifiers[0].triangle_count() == 1440);
    CHECK(m.support_blockers.triangle_count() == 12);
    CHECK(m.support_enforcers.empty());
    // The blocker box where the project put it: centred at (22.5, 2.5, 2.5) of the object.
    const BoundingBox b = m.support_blockers.bounding_box();
    const BoundingBox cube = m.mesh.bounding_box();
    CHECK(b.max.x > cube.min.x); // it overlaps the cube's side, as placed
    CHECK(b.min.z < cube.max.z);
}

TEST_CASE("project import: a project's settings translated over the profile (rules 6, 8 and 11; PF12, PF13)",
          "[project_import]") {
    // OrcaSlicer rows (shape as profile_pack_format.md rule 3 describes them):
    // widths in % of the nozzle, an acceleration in % of the default one, a
    // renamed value, a factor, a flag.
    const TranslationTable orca = parse_translation(R"({"format": "tisma-pack-translation", "version": 1,
      "dialect": "orcaslicer", "kinds": {"print": {
       "outer_wall_line_width": {"engine": "walls.outer_width", "group": "similar", "prusaslicer": "external_perimeter_extrusion_width",
                                 "percent_of": "nozzle", "conversion": {"how": "absolute_only"}},
       "default_acceleration": {"engine": "acceleration.default", "group": "translated", "conversion": {"how": "number"}},
       "bridge_acceleration": {"engine": "acceleration.bridges", "group": "similar", "percent_of": "acceleration.default",
                               "conversion": {"how": "number"}},
       "layer_height": {"engine": "layers.height", "group": "translated", "conversion": {"how": "number"}},
       "seam_position": {"engine": "walls.seam", "group": "translated", "prusaslicer_values": {"back": "rear"},
                         "conversion": {"how": "map", "values": {}}},
       "thin_walls": {"engine": null, "group": "none"}},
      "filament": {
       "filament_flow_ratio": {"engine": "extrusion.flow", "group": "translated", "conversion": {"how": "factor", "factor": 100}}},
      "printer": {
       "nozzle_diameter": {"engine": "machine.nozzle_diameter", "group": "translated", "conversion": {"how": "first_item"}}}}})");
    Settings base;
    base.nozzle_diameter = 0.4;
    const ProjectSettings p = translate_settings(orca, {{"nozzle_diameter", "0.6,0.6"},
                                                        {"outer_wall_line_width", "105%"},
                                                        {"bridge_acceleration", "50%"},
                                                        {"default_acceleration", "5000"},
                                                        {"layer_height", "0.16"},
                                                        {"seam_position", "back"},
                                                        {"filament_flow_ratio", "0.98"},
                                                        {"thin_walls", "1"}},
                                                 base);
    CHECK_THAT(p.settings.nozzle_diameter, Catch::Matchers::WithinAbs(0.6, 1e-9));
    CHECK_THAT(p.settings.outer_wall_width, Catch::Matchers::WithinAbs(0.63, 1e-9)); // PF12: 105 % of 0.6, not of the layer
    CHECK_THAT(p.settings.accel_default, Catch::Matchers::WithinAbs(5000, 1e-9));
    CHECK_THAT(p.settings.accel_bridges, Catch::Matchers::WithinAbs(2500, 1e-9)); // PF13, the default given after it
    CHECK_THAT(p.settings.layer_height, Catch::Matchers::WithinAbs(0.16, 1e-9));
    CHECK_THAT(p.settings.flow, Catch::Matchers::WithinAbs(98.0, 1e-9));
    CHECK(settings_json(p.settings).find("\"rear\"") != std::string::npos);
    CHECK(p.skipped.empty()); // thin_walls has no equivalent: not tried
    // A value that does not convert, or that the engine refuses, is named.
    const ProjectSettings bad = translate_settings(orca, {{"layer_height", "thick"}, {"default_acceleration", "-5"}}, base);
    CHECK(bad.settings.layer_height == base.layer_height);
    CHECK(bad.skipped == std::vector<std::string>{"layer_height = thick", "default_acceleration = -5"});
    // An OrcaSlicer project's settings: a JSON object, lists joined.
    const std::string package = write_zip_stored(
        {{"3D/3dmodel.model", "<model/>"},
         {"Metadata/project_settings.config", R"({"layer_height": "0.16", "nozzle_diameter": ["0.6", "0.6"]})"}});
    const auto read = project_settings(package);
    REQUIRE(read.size() == 2);
    CHECK(read[1] == std::pair<std::string, std::string>{"nozzle_diameter", "0.6,0.6"});
}

TEST_CASE("project import: the bed's temperature by the project's plate, and the other conversions (PF14)",
          "[project_import]") {
    const TranslationTable orca = parse_translation(R"({"format": "tisma-pack-translation", "version": 1,
      "dialect": "orcaslicer",
      "plate_temperature": {
        "plates": {"High Temp Plate": ["hot_plate_temp", "hot_plate_temp_initial_layer"],
                   "Textured PEI Plate": ["textured_plate_temp", "textured_plate_temp_initial_layer"],
                   "Cool Plate": ["cool_plate_temp", "cool_plate_temp_initial_layer"]},
        "select": ["curr_bed_type", "default_bed_type"],
        "index": ["Default Plate", "Cool Plate", "Engineering Plate", "High Temp Plate", "Textured PEI Plate"],
        "order": ["High Temp Plate", "Textured PEI Plate", "Cool Plate"]},
      "kinds": {"filament": {
       "hot_plate_temp": {"engine": "machine.variables.bed_temperature", "group": "similar", "prusaslicer": "bed_temperature",
                          "conversion": {"how": "rule", "rule": "plate_temperature", "plate": "High Temp Plate"}},
       "textured_plate_temp": {"engine": "machine.variables.bed_temperature", "group": "similar", "prusaslicer": "bed_temperature",
                               "conversion": {"how": "rule", "rule": "plate_temperature", "plate": "Textured PEI Plate"}},
       "cool_plate_temp": {"engine": "machine.variables.bed_temperature", "group": "similar", "prusaslicer": "bed_temperature",
                           "conversion": {"how": "rule", "rule": "plate_temperature", "plate": "Cool Plate"}}},
      "print": {
       "skirt_loops": {"engine": "adhesion.skirt_loops", "group": "translated", "conversion": {"how": "at_least", "min": 0}},
       "seam_gap": {"engine": "walls.seam_gap", "group": "similar",
                    "conversion": {"how": "number_or_percent", "nonzero": true, "bare_percent_over": 5}},
       "wall_loops": {"engine": "walls.count", "group": "translated", "conversion": {"how": "number", "nonnegative": true, "max": 50}},
       "wall_generator": {"engine": "walls.generator", "group": "similar", "conversion": {"how": "rule", "rule": "role_width"}}},
      "printer": {
       "printable_area": {"engine": "machine.bed", "group": "translated", "conversion": {"how": "points"}}}}})");
    REQUIRE(orca.plate_temperature);
    Settings base;
    const auto bed = [&](const std::vector<std::pair<std::string, std::string>>& project) {
        const std::string json = settings_json(translate_settings(orca, project, base).settings);
        const auto at = json.find("\"bed_temperature\"");
        REQUIRE(at != std::string::npos);
        return json.substr(at, json.find(']', at) - at + 1);
    };
    const std::vector<std::pair<std::string, std::string>> temps = {
        {"hot_plate_temp", "55"}, {"textured_plate_temp", "65"}, {"cool_plate_temp", "35"}};
    // PF14: the named plate; by number; none named: High Temp first.
    auto named = temps;
    named.push_back({"curr_bed_type", "Textured PEI Plate"});
    CHECK(bed(named).find("\"65\"") != std::string::npos);
    auto numbered = temps;
    numbered.push_back({"default_bed_type", "1"});
    CHECK(bed(numbered).find("\"35\"") != std::string::npos);
    CHECK(bed(temps).find("\"55\"") != std::string::npos);
    // The other conversions.
    const ProjectSettings p = translate_settings(orca, {{"skirt_loops", "2"},
                                                        {"seam_gap", "10"},
                                                        {"wall_loops", "60"},
                                                        {"wall_generator", "arachne"},
                                                        {"printable_area", "0x0,220x0,220x220,0x220"}},
                                                 base);
    CHECK(p.settings.skirt_loops == 2);
    CHECK(p.settings.wall_seam_gap.value == 10.0); // over 5: a %
    CHECK(p.settings.wall_seam_gap.percent);
    CHECK(p.settings.wall_count == base.wall_count);                     // 60: over max, not written
    REQUIRE(p.settings.bed.size() == 4);
    CHECK(p.settings.bed[2][0] == 220.0);
    CHECK(p.skipped == std::vector<std::string>{"wall_loops = 60", "wall_generator = arachne"});
    // Two points are no bed.
    CHECK(translate_settings(orca, {{"printable_area", "0x0,220x0"}}, base).settings.bed == base.bed);
}

TEST_CASE("project import: the pack's own rows (profiles/next, PF12 to PF14)", "[project_import]") {
    // The rows as the pack's session gave them, as data.
    const TranslationTable t = parse_translation(R"({"format": "tisma-pack-translation", "version": 1,
      "dialect": "crealityprint", "source": "test",
      "plate_temperature": {"plates": {"Cool Plate": ["cool_plate_temp", "cool_plate_temp_initial_layer"],
        "Engineering Plate": ["eng_plate_temp", "eng_plate_temp_initial_layer"],
        "High Temp Plate": ["hot_plate_temp", "hot_plate_temp_initial_layer"],
        "Textured PEI Plate": ["textured_plate_temp", "textured_plate_temp_initial_layer"],
        "Textured Cool Plate": ["textured_cool_plate_temp", "textured_cool_plate_temp_initial_layer"],
        "Supertack Plate": ["supertack_plate_temp", "supertack_plate_temp_initial_layer"]},
       "select": ["curr_bed_type", "default_bed_type"],
       "index": ["Default Plate", "Cool Plate", "Engineering Plate", "High Temp Plate", "Textured PEI Plate",
                 "Textured Cool Plate", "Supertack Plate"],
       "order": ["hot_plate_temp", "textured_plate_temp", "eng_plate_temp", "cool_plate_temp", "textured_cool_plate_temp",
                 "supertack_plate_temp"]},
      "kinds": {"print": {
        "outer_wall_line_width": {"engine": "walls.outer_width", "group": "translated",
          "conversion": {"how": "rule", "rule": "role_width", "auto": null}, "note": "in mm",
          "with": ["extrusion_width"], "prusaslicer": "external_perimeter_extrusion_width", "percent_of": "nozzle"},
        "default_acceleration": {"engine": "acceleration.default", "group": "translated", "conversion": {"how": "number"}},
        "initial_layer_acceleration": {"engine": "acceleration.first_layer", "group": "translated",
          "conversion": {"how": "number"}, "prusaslicer": "first_layer_acceleration",
          "percent_of": "acceleration.default", "renamed_by": "converter"}},
       "filament": {
        "hot_plate_temp": {"engine": "machine.variables.bed_temperature", "group": "translated",
          "conversion": {"how": "first_item"}, "prusaslicer": "bed_temperature", "rule": "plate_temperature",
          "plate": "High Temp Plate"},
        "textured_plate_temp": {"engine": "machine.variables.bed_temperature", "group": "translated",
          "conversion": {"how": "first_item"}, "prusaslicer": "bed_temperature", "rule": "plate_temperature",
          "plate": "Textured PEI Plate"}},
       "printer": {
        "nozzle_diameter": {"engine": "machine.nozzle_diameter", "group": "translated", "conversion": {"how": "first_item"}}}}})");
    Settings base;
    const auto width = [&](std::vector<std::pair<std::string, std::string>> p) {
        p.push_back({"nozzle_diameter", "0.6"});
        return translate_settings(t, p, base).settings.outer_wall_width;
    };
    CHECK_THAT(width({{"outer_wall_line_width", "105%"}}), Catch::Matchers::WithinAbs(0.63, 1e-9)); // PF12
    CHECK_THAT(width({{"outer_wall_line_width", "0.5"}}), Catch::Matchers::WithinAbs(0.5, 1e-9));
    CHECK_THAT(width({{"outer_wall_line_width", "0"}, {"extrusion_width", "110%"}}), Catch::Matchers::WithinAbs(0.66, 1e-9));
    CHECK(width({{"outer_wall_line_width", "0"}, {"extrusion_width", "0"}}) == 0.0); // the engine's own
    // extrusion_width alone is not written anywhere by itself.
    CHECK(translate_settings(t, {{"extrusion_width", "0.7"}}, base).settings.outer_wall_width == base.outer_wall_width);
    // PF13 with Creality Print's row.
    const ProjectSettings a = translate_settings(t, {{"initial_layer_acceleration", "50%"}, {"default_acceleration", "5000"}}, base);
    CHECK(a.settings.accel_first_layer == 2500.0);
    // PF14: the named plate; "Default Plate" (0) goes to the order, High Temp first.
    const auto bed = [&](std::vector<std::pair<std::string, std::string>> p) {
        p.push_back({"hot_plate_temp", "55,55"});
        p.push_back({"textured_plate_temp", "65,65"});
        const std::string json = settings_json(translate_settings(t, p, base).settings);
        const auto at = json.find("\"bed_temperature\"");
        return at == std::string::npos ? std::string() : json.substr(at, json.find(']', at) - at + 1);
    };
    CHECK(bed({{"curr_bed_type", "Textured PEI Plate"}}).find("\"65\"") != std::string::npos);
    CHECK(bed({{"curr_bed_type", "0"}, {"default_bed_type", "Textured PEI Plate"}}).find("\"55\"") != std::string::npos);
    CHECK(bed({}).find("\"55\"") != std::string::npos);
}

TEST_CASE("project import: the named rules of the translation table (profile_pack_format.md)", "[project_import]") {
    // OrcaSlicer rows that name a rule; "with" and the rules use the keys'
    // PrusaSlicer names.
    const TranslationTable orca = parse_translation(R"({"format": "tisma-pack-translation", "version": 1,
      "dialect": "orcaslicer", "kinds": {"print": {
       "gap_infill_speed": {"engine": "speed.gap_fill, walls.gap_fill", "group": "translated", "prusaslicer": "gap_fill_speed",
                            "conversion": {"how": "rule", "rule": "gap_fill"}, "with": ["gap_fill_enabled"]},
       "inner_wall_line_width": {"engine": "walls.width", "group": "similar", "prusaslicer": "perimeter_extrusion_width",
                                 "percent_of": "nozzle", "conversion": {"how": "rule", "rule": "wall_width"}, "with": ["extrusion_width"]},
       "line_width": {"engine": "walls.width", "group": "similar", "prusaslicer": "extrusion_width",
                      "percent_of": "nozzle", "conversion": {"how": "rule", "rule": "wall_width"}, "with": ["perimeter_extrusion_width"]},
       "support_object_xy_distance": {"engine": "support.xy_gap", "group": "translated", "prusaslicer": "support_material_xy_spacing",
                                      "conversion": {"how": "rule", "rule": "xy_gap"}}},
      "filament": {
       "fan_min_speed": {"engine": "cooling.fan_min, cooling.fan_max", "group": "translated", "prusaslicer": "min_fan_speed",
                         "conversion": {"how": "rule", "rule": "fans"}, "with": ["cooling", "fan_always_on", "max_fan_speed"]},
       "fan_max_speed": {"engine": "cooling.fan_min, cooling.fan_max", "group": "translated", "prusaslicer": "max_fan_speed",
                         "conversion": {"how": "rule", "rule": "fans"}, "with": ["cooling", "fan_always_on", "min_fan_speed"]},
       "filament_retraction_length": {"engine": "extrusion.retraction, extrusion.travel", "group": "translated",
                                      "prusaslicer": "filament_retract_length", "conversion": {"how": "rule", "rule": "filament_overrides"}},
       "filament_z_hop": {"engine": "extrusion.retraction, extrusion.travel", "group": "translated",
                          "prusaslicer": "filament_retract_lift", "conversion": {"how": "rule", "rule": "filament_overrides"}}},
      "printer": {
       "machine_max_acceleration_extruding": {"engine": "machine.acceleration, machine.travel_acceleration", "group": "translated",
                                              "conversion": {"how": "rule", "rule": "acceleration"}, "with": ["machine_max_acceleration_travel"]},
       "machine_max_junction_deviation": {"engine": "machine.junction_deviation", "group": "translated",
                                          "conversion": {"how": "rule", "rule": "junction_deviation"}, "with": ["gcode_flavor"]},
       "extruder_offset": {"engine": "machine.extruders, machine.nozzle_diameter, machine.extruder_offset", "group": "translated",
                           "conversion": {"how": "rule", "rule": "extruders"}},
       "machine_start_gcode": {"engine": "machine.start_gcode, machine.end_gcode", "group": "translated", "prusaslicer": "start_gcode",
                               "conversion": {"how": "rule", "rule": "gcode_template"}}}}})");
    Settings base;
    base.nozzle_diameter = 0.4;
    const auto json_of = [](const ProjectSettings& p) { return nlohmann::json::parse(settings_json(p.settings)); };
    const ProjectSettings p = translate_settings(orca,
                                                 {{"nozzle_diameter", "0.4,0.6"},
                                                  {"extruder_offset", "0x0,25x0"},
                                                  {"gap_infill_speed", "40"},
                                                  {"inner_wall_line_width", "110%"},
                                                  {"line_width", "0.42"},
                                                  {"support_object_xy_distance", "0.35"},
                                                  {"fan_min_speed", "20,20"},
                                                  {"fan_max_speed", "80,80"},
                                                  {"filament_retraction_length", "1.2"},
                                                  {"filament_z_hop", "nil"},
                                                  {"machine_max_acceleration_extruding", "4000,1000"},
                                                  {"machine_max_acceleration_travel", "0,0"},
                                                  {"machine_max_junction_deviation", "0.02,0.01"},
                                                  {"gcode_flavor", "marlin2"},
                                                  {"machine_start_gcode", "G28 ; {if x}"}},
                                                 base);
    const nlohmann::json j = json_of(p);
    CHECK(j["machine"]["extruders"] == 2);
    CHECK(j["machine"]["nozzle_diameter"] == nlohmann::json::array({0.4, 0.6})); // they differ: a list
    CHECK(j["machine"]["extruder_offset"] == nlohmann::json::array({{0.0, 0.0}, {25.0, 0.0}}));
    CHECK(j["walls"]["gap_fill"] == true);
    CHECK(j["speed"]["gap_fill"] == 40.0);
    CHECK_THAT(j["walls"]["width"].get<double>(), Catch::Matchers::WithinAbs(0.44, 1e-9)); // 110 % of the first nozzle
    CHECK(j["support"]["xy_gap"] == 0.35);
    CHECK(j["cooling"]["fan_min"] == 20.0);
    CHECK(j["cooling"]["fan_max"] == 80.0);
    CHECK(j["extrusion"]["retraction"] == nlohmann::json{{"length", 1.2}}); // "nil": the printer's lift
    CHECK(j["machine"]["acceleration"] == 4000.0);
    CHECK(j["machine"]["travel_acceleration"] == 4000.0); // travel 0 falls back to extruding
    CHECK(j["machine"]["junction_deviation"] == 0.02);
    // gcode_template is not done yet: named, once per key.
    CHECK(p.skipped == std::vector<std::string>{"machine_start_gcode = G28 ; {if x}"});

    // Cooling off and fan not always on: both 0; no gap fill; no wall width
    // given: 1.125 nozzles; the deviation only with marlin2.
    const TranslationTable prusa = parse_translation(R"({"format": "tisma-pack-translation", "version": 1,
      "dialect": "prusaslicer", "kinds": {"print": {
       "gap_fill_enabled": {"engine": "speed.gap_fill, walls.gap_fill", "group": "translated",
                            "conversion": {"how": "rule", "rule": "gap_fill"}, "with": ["gap_fill_speed"]},
       "perimeter_extrusion_width": {"engine": "walls.width", "group": "similar",
                                     "conversion": {"how": "rule", "rule": "wall_width"}, "with": ["extrusion_width"]},
       "support_material_xy_spacing": {"engine": "support.xy_gap", "group": "translated", "conversion": {"how": "rule", "rule": "xy_gap"}}},
      "filament": {
       "cooling": {"engine": "cooling.fan_min, cooling.fan_max", "group": "translated", "conversion": {"how": "rule", "rule": "fans"},
                   "with": ["fan_always_on", "min_fan_speed", "max_fan_speed"]}},
      "printer": {
       "machine_max_junction_deviation": {"engine": "machine.junction_deviation", "group": "translated",
                                          "conversion": {"how": "rule", "rule": "junction_deviation"}, "with": ["gcode_flavor"]},
       "thumbnails": {"engine": "gcode.thumbnails", "group": "translated", "conversion": {"how": "rule", "rule": "thumbnails"},
                      "with": ["thumbnails_format"]}}}})");
    const ProjectSettings q = translate_settings(prusa,
                                                 {{"gap_fill_enabled", "0"},
                                                  {"perimeter_extrusion_width", "0"},
                                                  {"extrusion_width", "150%"},
                                                  {"layer_height", "0.2"},
                                                  {"support_material_xy_spacing", "60%"},
                                                  {"cooling", "0"},
                                                  {"fan_always_on", "0"},
                                                  {"min_fan_speed", "35"},
                                                  {"machine_max_junction_deviation", "0.02"},
                                                  {"gcode_flavor", "klipper"},
                                                  {"thumbnails", "16x16/QOI, 220x124"},
                                                  {"thumbnails_format", "png"}},
                                                 base);
    const nlohmann::json k = json_of(q);
    CHECK(k["walls"]["gap_fill"] == false);
    CHECK_THAT(k["walls"]["width"].get<double>(), Catch::Matchers::WithinAbs(0.3, 1e-9)); // 150 % of the layer height
    CHECK(k["support"]["xy_gap"] == "60.0%"); // the engine writes its own way
    CHECK(k["cooling"]["fan_min"] == 0.0);
    CHECK(k["cooling"]["fan_max"] == 0.0);
    CHECK(k["machine"]["junction_deviation"] == 0.0); // klipper: not sent
    CHECK(k["gcode"]["thumbnails"] == "16x16/QOI,220x124/PNG");
    CHECK(q.skipped.empty());
    // role_width in PrusaSlicer: "N%" of the layer height.
    const TranslationTable outer = parse_translation(R"({"format": "tisma-pack-translation", "version": 1,
      "dialect": "prusaslicer", "kinds": {"print": {
       "external_perimeter_extrusion_width": {"engine": "walls.outer_width", "group": "translated",
                                              "conversion": {"how": "rule", "rule": "role_width"}, "with": ["extrusion_width"]}}}})");
    const ProjectSettings o = translate_settings(outer, {{"external_perimeter_extrusion_width", "200%"}, {"layer_height", "0.2"}}, base);
    CHECK_THAT(o.settings.outer_wall_width, Catch::Matchers::WithinAbs(0.4, 1e-9));
    CHECK(o.skipped.empty());
    const ProjectSettings none = translate_settings(prusa, {{"perimeter_extrusion_width", "0"}}, base);
    CHECK_THAT(json_of(none)["walls"]["width"].get<double>(), Catch::Matchers::WithinAbs(0.45, 1e-9));
}
