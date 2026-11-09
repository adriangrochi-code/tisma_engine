// Projects of other programs (docs/spec/features/project_import.md): which
// program saved a 3MF (rule 1), and a standard 3MF otherwise (rule 2).

#include "io/project_import.hpp"
#include "io/zip.hpp"

#include <catch2/catch_test_macros.hpp>

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
