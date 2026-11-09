#pragma once

// Projects of other slicing programs (docs/spec/features/project_import.md).
// Own design from the public 3MF specification and black-box samples; see
// PROVENANCE.md.

#include <string>
#include <string_view>

namespace tisma {

// Which kind of program saved a 3MF package (rule 1), by the files it
// carries: the PrusaSlicer family's Metadata/Slic3r_PE*.config, the
// OrcaSlicer family's Metadata/project_settings.config with
// model_settings.config, Cura's Cura/ folder. Anything else is a standard
// 3MF (rule 2).
enum class ProjectFamily { Standard, PrusaSlicer, OrcaSlicer, Cura };

struct ProjectSource {
    ProjectFamily family = ProjectFamily::Standard;
    // The model's Application metadata as written ("" if none), and the
    // program's name for the dialog (rule 9): a known name found in it, or
    // the family's when the metadata names none.
    std::string application, program;
    // The build items of the root model: the parts the dialog counts (rule 9).
    int items = 0;
};

// Throws ZipError when the package is not a readable ZIP.
ProjectSource project_source(std::string_view package);

} // namespace tisma
