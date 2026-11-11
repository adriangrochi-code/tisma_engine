#pragma once

// Projects of other slicing programs (docs/spec/features/project_import.md).
// Own design from the public 3MF specification and black-box samples; see
// PROVENANCE.md.

#include "config/settings.hpp"

#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

// The profile pack's translation table for one dialect (rule 11;
// profile_pack_format.md, «Tabla de traducción»): by kind (print, filament,
// printer...) and source key, the engine key and its group (rule 6). A key
// a row names in "with" goes with that row.
enum class SettingGroup { Translated, Similar, None };
struct TranslationRow {
    std::string engine; // "" when the row has none
    SettingGroup group = SettingGroup::None;
    // How the value becomes the engine's (profile_pack_format.md, «Tabla de
    // traducción», rules 2 and 3): `how` (number, flag, factor, map,
    // percent_of, absolute_only, number_strip_percent, first_item; another
    // is not converted yet), its factor, its map, its base key; in the
    // dialects other than prusaslicer, the values renamed first and what an
    // "N%" is of ("nozzle" or an engine key, PF12 and PF13).
    std::string how;
    double factor = 1.0;
    std::map<std::string, std::string> values, renamed;
    std::string base, percent_of;
};
struct TranslationTable {
    std::string dialect;
    std::map<std::string, std::map<std::string, TranslationRow>> kinds;
};
struct TranslationError : std::runtime_error {
    using std::runtime_error::runtime_error;
};
// Throws TranslationError: not a table, or of a newer version.
TranslationTable parse_translation(std::string_view json);

// The table's dialect for a project's program: prusaslicer, orcaslicer,
// crealityprint, elegooslicer or cura; "" for a standard 3MF.
std::string translation_dialect(const ProjectSource& source);

// A project's settings (source key and value) sorted into the rule 6 groups,
// as the open dialog shows them (rule 9): a key the table does not have is
// without equivalent. The detail is in the settings' order.
struct SettingSort {
    int translated = 0, similar = 0, none = 0;
    std::vector<std::pair<std::string, SettingGroup>> detail;
};
// The project's own settings, as saved (rule 6): for the PrusaSlicer family
// the "; key = value" lines of Metadata/Slic3r_PE.config, in order; for the
// OrcaSlicer family the keys of the JSON object in
// Metadata/project_settings.config, a list's items joined by commas. Empty
// for Cura until its samples are measured, or with no settings.
// Throws ZipError when the package is not a readable ZIP.
std::vector<std::pair<std::string, std::string>> project_settings(std::string_view package);

SettingSort sort_settings(const TranslationTable& table, const std::vector<std::pair<std::string, std::string>>& settings);

// A project's settings over `base` (rules 6 and 8: the translated and similar
// ones, converted by their row, as changes by hand). A value that does not
// convert, or that the engine refuses, is left out and named in `skipped`
// ("key = value"). An "N%" of another setting or of the nozzle takes the
// project's value of it, else `base`'s.
struct ProjectSettings {
    Settings settings;
    std::vector<std::string> skipped;
};
ProjectSettings translate_settings(const TranslationTable& table,
                                   const std::vector<std::pair<std::string, std::string>>& settings, const Settings& base);

} // namespace tisma
