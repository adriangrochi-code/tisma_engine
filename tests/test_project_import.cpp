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
