#pragma once

#include "ui/assistant.hpp"

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// The assistant's data (docs/spec/features/assistant.md): the files of
// knowledge/ the profile pack brings, in the format docs/KNOWLEDGE.md of the
// profiles' repository describes. Only the format was read, never the data.
// Own design on that description.
namespace tisma::ui {

class KnowledgeError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// [min, max]; not known when the file leaves it out.
struct KnowledgeRange {
    double min = 0.0, max = 0.0;
    bool known = false;
};

struct KnowledgeSource {
    std::string id, title, author, url;
};

struct KnowledgeMaterial {
    std::string name, family, category, nozzle_material;
    bool abrasive = false;
    KnowledgeRange nozzle_temp, bed_temp, fan;
    std::string bed_required, enclosure; // "necesaria", "recomendada", "opcional"...
    std::vector<std::string> surfaces;
    // The base parameters' table, when the file has it.
    struct Base {
        KnowledgeRange nozzle_temp, bed_temp;
        std::optional<double> chamber_temp, fan, retraction_direct, retraction_bowden, nozzle_diameter_min;
        std::string enclosure;
    };
    std::optional<Base> base;
    struct Drying {
        std::string before_printing; // "si" or "recomendado"
        KnowledgeRange temp;
        std::optional<int> hours_before_printing, hours_mild, hours_severe;
    };
    std::optional<Drying> drying;
};

// An action of a problem's cause or of a tutorial: `setting` (key, op,
// value as JSON text, unit, of, why), `calibration`, `procedure` and
// `problem` (id), `hardware` and `model` (text).
struct KnowledgeAction {
    std::string type;
    std::string key, op, value, unit, of, why;
    std::string id, text;
};

struct KnowledgeCause {
    std::string id, name, explain, check;
    bool last_resort = false;
    std::vector<KnowledgeAction> actions;
};

struct KnowledgeProblem {
    std::string id, name, symptoms;
    std::vector<KnowledgeCause> causes;
};

struct KnowledgeStep {
    std::string title, text;
};

struct KnowledgeProcedure {
    std::string id, name, for_cases, applies_to;
    std::vector<KnowledgeStep> steps;
    std::vector<std::string> warnings, prevention, notes;
};

struct KnowledgeTerm {
    std::string term, definition;
    std::vector<std::string> settings;
};

struct KnowledgeTutorial {
    std::string id, name, when;
    int minutes = 0;
    struct Step {
        std::string anchor, title, text, wait_for;
        std::string open_type, open_id; // a procedure or a calibration to open from the step
    };
    std::vector<Step> steps;
};

// Everything the folder has. A file that is not there leaves its list
// empty; one that does not read is named in `errors` and left out.
struct Knowledge {
    std::string language;
    std::vector<KnowledgeSource> sources;
    std::vector<KnowledgeMaterial> materials;
    std::vector<KnowledgeProblem> problems;
    std::vector<KnowledgeProcedure> procedures;
    std::vector<KnowledgeTerm> terms; // in alphabetical order
    std::vector<KnowledgeTutorial> tutorials;
    std::vector<std::string> errors;
    bool empty() const {
        return materials.empty() && problems.empty() && procedures.empty() && terms.empty() && tutorials.empty();
    }
};

// One file's text read into `into` (its kind by its "format"). Throws
// KnowledgeError when it is not such a file.
void parse_knowledge_file(std::string_view json, Knowledge& into);

// The folder's files, in the interface's language when there are some in
// it (rule 19: `<folder>/<language>/` first, then the folder itself, whose
// files are in Spanish today).
Knowledge load_knowledge(const std::filesystem::path& folder, std::string_view language);

// What the search finds (rule 2): problems, procedures, terms and materials
// whose name or text holds every word of the query, whatever the case and the
// accents; names count more, best first.
struct KnowledgeHit {
    enum class Kind { Problem, Procedure, Term, Material, Tutorial } kind = Kind::Problem;
    std::size_t index = 0;
    int score = 0;
};
std::vector<KnowledgeHit> search_knowledge(const Knowledge& knowledge, std::string_view query);

// The term the glossary has for a setting (rule 12): the first in
// alphabetical order that names it; none when no term does.
const KnowledgeTerm* term_for_setting(const Knowledge& knowledge, std::string_view key);

// A material of the file as the warnings take it (rules 13 and 14).
MaterialInfo material_info(const KnowledgeMaterial& material);

// The setting action of a knowledge action, for plan_setting (rule 7).
std::optional<SettingAction> setting_action(const KnowledgeAction& action);

} // namespace tisma::ui
