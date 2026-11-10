// Projects of other programs (docs/spec/features/project_import.md): which
// program saved a 3MF (rule 1), and a standard 3MF otherwise (rule 2).

#include "io/model.hpp"
#include "io/project_import.hpp"
#include "io/zip.hpp"

#include <catch2/catch_test_macros.hpp>

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
