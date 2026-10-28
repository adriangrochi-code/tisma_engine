#include "ui/knowledge.hpp"

#include "ui/settings_view.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <sstream>

namespace tisma::ui {
namespace {

using nlohmann::json;

// A text field: required ones throw when missing; optional ones (missing or
// null) are empty.
std::string text(const json& o, const char* key, bool required = false) {
    const auto it = o.find(key);
    if (it == o.end() || it->is_null()) {
        if (required)
            throw KnowledgeError(std::string("\"") + key + "\" is missing");
        return {};
    }
    if (!it->is_string())
        throw KnowledgeError(std::string("\"") + key + "\" is not a text");
    return it->get<std::string>();
}

std::vector<std::string> texts(const json& o, const char* key) {
    std::vector<std::string> out;
    const auto it = o.find(key);
    if (it == o.end() || it->is_null())
        return out;
    if (!it->is_array())
        throw KnowledgeError(std::string("\"") + key + "\" is not a list");
    for (const json& t : *it) {
        if (!t.is_string())
            throw KnowledgeError(std::string("\"") + key + "\" holds something that is not a text");
        out.push_back(t.get<std::string>());
    }
    return out;
}

std::optional<double> number(const json& o, const char* key) {
    const auto it = o.find(key);
    if (it == o.end() || it->is_null())
        return std::nullopt;
    if (!it->is_number())
        throw KnowledgeError(std::string("\"") + key + "\" is not a number");
    return it->get<double>();
}

KnowledgeRange range(const json& o, const char* key) {
    KnowledgeRange r;
    const auto it = o.find(key);
    if (it == o.end() || it->is_null())
        return r;
    if (!it->is_array() || it->size() != 2 || !(*it)[0].is_number() || !(*it)[1].is_number())
        throw KnowledgeError(std::string("\"") + key + "\" is not a range [min, max]");
    r.min = (*it)[0].get<double>(), r.max = (*it)[1].get<double>(), r.known = true;
    return r;
}

const json& list(const json& root, const char* key) {
    const auto it = root.find(key);
    if (it == root.end() || !it->is_array())
        throw KnowledgeError(std::string("no list \"") + key + "\"");
    return *it;
}

KnowledgeAction action_of(const json& a) {
    KnowledgeAction out;
    out.type = text(a, "type", true);
    if (out.type == "setting") {
        out.key = text(a, "key", true);
        out.op = text(a, "op", true);
        if (!a.contains("value"))
            throw KnowledgeError("a setting action without its value");
        out.value = a.at("value").dump();
        out.unit = text(a, "unit");
        out.of = text(a, "of");
    } else if (out.type == "calibration" || out.type == "procedure" || out.type == "problem") {
        out.id = text(a, "id", true);
    } else {
        out.text = text(a, "text");
    }
    out.why = text(a, "why");
    return out;
}

// A source as a file's: an object with its fields, or an id naming one of
// the file's sources.
KnowledgeSource source_of(const json& s, const std::vector<KnowledgeSource>& file_sources) {
    if (s.is_string()) {
        for (const KnowledgeSource& f : file_sources)
            if (f.id == s.get<std::string>())
                return f;
        return {s.get<std::string>(), s.get<std::string>(), {}, {}};
    }
    if (!s.is_object())
        throw KnowledgeError("a source that is neither an object nor an id");
    return {text(s, "id"), text(s, "title"), text(s, "author"), text(s, "url")};
}

// A text, or a list of texts, as a list.
std::vector<std::string> one_or_more(const json& o, const char* key) {
    const auto it = o.find(key);
    if (it != o.end() && it->is_string())
        return {it->get<std::string>()};
    return texts(o, key);
}

KnowledgeCalibration calibration_of(const json& c, const std::vector<KnowledgeSource>& file_sources) {
    KnowledgeCalibration k;
    k.id = text(c, "id", true);
    k.name = text(c, "name", true);
    k.group = text(c, "group");
    k.order = int(number(c, "order").value_or(0.0));
    k.before = one_or_more(c, "before");
    k.why = text(c, "why");
    k.repeat = text(c, "repeat");
    if (const auto st = c.find("steps"); st != c.end() && !st->is_null())
        for (const json& s : list(c, "steps"))
            k.steps.push_back(s.is_string() ? KnowledgeStep{{}, s.get<std::string>()}
                                            : KnowledgeStep{text(s, "title"), text(s, "text")});
    if (const auto r = c.find("read"); r != c.end() && !r->is_null())
        for (const json& x : list(c, "read"))
            k.read.push_back({text(x, "look"), text(x, "means")});
    if (const auto r = c.find("result"); r != c.end() && r->is_object()) {
        k.result_key = text(*r, "key");
        k.result_where = text(*r, "where");
        k.result_text = text(*r, "text");
    }
    k.tips = one_or_more(c, "tips");
    if (const auto o = c.find("open"); o != c.end() && o->is_object())
        k.open = text(*o, "id");
    else
        k.open = text(c, "open");
    if (const auto s = c.find("sources"); s != c.end() && s->is_array())
        for (const json& x : *s)
            k.sources.push_back(source_of(x, file_sources));
    return k;
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream s;
    s << in.rdbuf();
    return s.str();
}

} // namespace

void parse_knowledge_file(std::string_view source, Knowledge& into) {
    json root;
    try {
        root = json::parse(source);
    } catch (const json::exception&) {
        throw KnowledgeError("not a JSON file");
    }
    if (!root.is_object())
        throw KnowledgeError("not a knowledge file");
    const std::string format = text(root, "format", true);
    if (!format.starts_with("tisma-knowledge-"))
        throw KnowledgeError("not a knowledge file (format tisma-knowledge-...)");
    if (!root.contains("version") || !root.at("version").is_number_integer() || root.at("version").get<long>() != 1)
        throw KnowledgeError("a knowledge file of another version");
    try {
        if (into.language.empty())
            into.language = text(root, "language");
        std::vector<KnowledgeSource> file_sources;
        if (root.contains("sources") && root.at("sources").is_array())
            for (const json& s : root.at("sources"))
                file_sources.push_back({text(s, "id"), text(s, "title"), text(s, "author"), text(s, "url")});
        into.sources.insert(into.sources.end(), file_sources.begin(), file_sources.end());
        const std::string kind = format.substr(16);
        if (kind == "materials") {
            for (const json& m : list(root, "materials")) {
                KnowledgeMaterial k;
                k.name = text(m, "name", true);
                k.family = text(m, "family");
                k.category = text(m, "category");
                k.nozzle_material = text(m, "nozzle_material");
                k.abrasive = m.contains("abrasive") && m.at("abrasive").is_boolean() && m.at("abrasive").get<bool>();
                k.nozzle_temp = range(m, "nozzle_temp");
                k.bed_temp = range(m, "bed_temp");
                k.fan = range(m, "fan");
                k.bed_required = text(m, "bed_required");
                k.enclosure = text(m, "enclosure");
                k.surfaces = texts(m, "surfaces");
                if (const auto b = m.find("base_table"); b != m.end() && b->is_object()) {
                    KnowledgeMaterial::Base base;
                    base.nozzle_temp = range(*b, "nozzle_temp");
                    base.bed_temp = range(*b, "bed_temp");
                    base.chamber_temp = number(*b, "chamber_temp");
                    base.fan = number(*b, "fan");
                    base.enclosure = text(*b, "enclosure");
                    base.nozzle_diameter_min = number(*b, "nozzle_diameter_min");
                    if (const auto r = b->find("retraction"); r != b->end() && r->is_object()) {
                        base.retraction_direct = number(*r, "direct");
                        base.retraction_bowden = number(*r, "bowden");
                    }
                    k.base = base;
                }
                if (const auto d = m.find("drying"); d != m.end() && d->is_object()) {
                    KnowledgeMaterial::Drying dry;
                    dry.before_printing = text(*d, "before_printing");
                    dry.temp = range(*d, "temp");
                    const auto hours = [&](const char* key) -> std::optional<int> {
                        const std::optional<double> h = number(*d, key);
                        return h ? std::optional<int>(int(*h)) : std::nullopt;
                    };
                    dry.hours_before_printing = hours("hours_before_printing");
                    dry.hours_mild = hours("hours_mild");
                    dry.hours_severe = hours("hours_severe");
                    k.drying = dry;
                }
                into.materials.push_back(std::move(k));
            }
        } else if (kind == "problems") {
            for (const json& p : list(root, "problems")) {
                KnowledgeProblem k{text(p, "id", true), text(p, "name", true), text(p, "symptoms"), {}};
                for (const json& c : list(p, "causes")) {
                    KnowledgeCause cause{text(c, "id"), text(c, "name", true), text(c, "explain"), text(c, "check"),
                                         c.contains("last_resort") && c.at("last_resort").is_boolean() &&
                                             c.at("last_resort").get<bool>(),
                                         {}};
                    if (c.contains("actions"))
                        for (const json& a : list(c, "actions"))
                            cause.actions.push_back(action_of(a));
                    k.causes.push_back(std::move(cause));
                }
                into.problems.push_back(std::move(k));
            }
        } else if (kind == "procedures") {
            for (const json& p : list(root, "procedures")) {
                KnowledgeProcedure k;
                k.id = text(p, "id", true);
                k.name = text(p, "name", true);
                k.for_cases = text(p, "for");
                k.applies_to = text(p, "applies_to");
                for (const json& s : list(p, "steps"))
                    k.steps.push_back({text(s, "title"), text(s, "text")});
                k.warnings = texts(p, "warnings");
                k.prevention = texts(p, "prevention");
                k.notes = texts(p, "notes");
                into.procedures.push_back(std::move(k));
            }
        } else if (kind == "glossary") {
            for (const json& t : list(root, "terms"))
                into.terms.push_back({text(t, "term", true), text(t, "definition", true), texts(t, "settings")});
            std::stable_sort(into.terms.begin(), into.terms.end(),
                             [](const KnowledgeTerm& a, const KnowledgeTerm& b) { return fold(a.term) < fold(b.term); });
        } else if (kind == "tutorials") {
            for (const json& t : list(root, "tutorials")) {
                KnowledgeTutorial k;
                k.id = text(t, "id", true);
                k.name = text(t, "name", true);
                k.when = text(t, "when");
                k.minutes = int(number(t, "minutes").value_or(0.0));
                for (const json& s : list(t, "steps")) {
                    KnowledgeTutorial::Step step{text(s, "anchor"), text(s, "title"), text(s, "text"), text(s, "wait_for"),
                                                 {}, {}};
                    if (const auto o = s.find("open"); o != s.end() && o->is_object())
                        step.open_type = text(*o, "type"), step.open_id = text(*o, "id");
                    k.steps.push_back(std::move(step));
                }
                into.tutorials.push_back(std::move(k));
            }
        } else if (kind == "calibrations") {
            for (const json& c : list(root, "calibrations")) {
                KnowledgeCalibration k = calibration_of(c, file_sources);
                // A guide without sources of its own names the file's.
                if (k.sources.empty())
                    k.sources = file_sources;
                into.calibrations.push_back(std::move(k));
            }
            std::stable_sort(into.calibrations.begin(), into.calibrations.end(),
                             [](const KnowledgeCalibration& a, const KnowledgeCalibration& b) { return a.order < b.order; });
        }
    } catch (const json::exception& e) {
        throw KnowledgeError(e.what());
    }
}

Knowledge load_knowledge(const std::filesystem::path& folder, std::string_view language) {
    Knowledge k;
    static constexpr const char* files[] = {"materials.json", "problems.json", "procedures.json", "glossary.json",
                                            "tutorials.json", "calibrations.json"};
    std::error_code error;
    for (const char* name : files) {
        std::filesystem::path path = folder / std::string(language) / name;
        if (!std::filesystem::is_regular_file(path, error))
            path = folder / name;
        if (!std::filesystem::is_regular_file(path, error))
            continue;
        try {
            parse_knowledge_file(read_file(path), k);
        } catch (const KnowledgeError& e) {
            k.errors.push_back(std::string(name) + ": " + e.what());
        }
    }
    return k;
}

std::vector<KnowledgeHit> search_knowledge(const Knowledge& k, std::string_view query) {
    std::vector<std::string> words;
    {
        std::istringstream in(fold(query));
        for (std::string w; in >> w;)
            words.push_back(w);
    }
    std::vector<KnowledgeHit> hits;
    if (words.empty())
        return hits;
    // A word in the name counts 3, in the rest 1; every word must be somewhere.
    const auto score = [&](const std::string& name, const std::string& rest) {
        const std::string n = fold(name), r = fold(rest);
        int s = 0;
        for (const std::string& w : words) {
            if (n.find(w) != std::string::npos)
                s += 3;
            else if (r.find(w) != std::string::npos)
                s += 1;
            else
                return 0;
        }
        return s;
    };
    using Kind = KnowledgeHit::Kind;
    const auto add = [&](Kind kind, std::size_t i, int s) {
        if (s > 0)
            hits.push_back({kind, i, s});
    };
    for (std::size_t i = 0; i < k.problems.size(); ++i) {
        std::string rest = k.problems[i].symptoms;
        for (const KnowledgeCause& c : k.problems[i].causes)
            rest += " " + c.name + " " + c.explain;
        add(Kind::Problem, i, score(k.problems[i].name, rest));
    }
    for (std::size_t i = 0; i < k.procedures.size(); ++i) {
        std::string rest = k.procedures[i].for_cases;
        for (const KnowledgeStep& s : k.procedures[i].steps)
            rest += " " + s.title + " " + s.text;
        add(Kind::Procedure, i, score(k.procedures[i].name, rest));
    }
    for (std::size_t i = 0; i < k.terms.size(); ++i)
        add(Kind::Term, i, score(k.terms[i].term, k.terms[i].definition));
    for (std::size_t i = 0; i < k.materials.size(); ++i)
        add(Kind::Material, i, score(k.materials[i].name, k.materials[i].family + " " + k.materials[i].category));
    for (std::size_t i = 0; i < k.tutorials.size(); ++i)
        add(Kind::Tutorial, i, score(k.tutorials[i].name, {}));
    for (std::size_t i = 0; i < k.calibrations.size(); ++i)
        add(Kind::Calibration, i, score(k.calibrations[i].name, k.calibrations[i].why));
    std::stable_sort(hits.begin(), hits.end(), [](const KnowledgeHit& a, const KnowledgeHit& b) { return a.score > b.score; });
    return hits;
}

const KnowledgeCalibration* find_guide(const Knowledge& k, std::string_view id) {
    for (const KnowledgeCalibration& c : k.calibrations)
        if (c.id == id)
            return &c;
    return nullptr;
}

const KnowledgeTerm* term_for_setting(const Knowledge& k, std::string_view key) {
    for (const KnowledgeTerm& t : k.terms) // already in alphabetical order
        if (std::find(t.settings.begin(), t.settings.end(), key) != t.settings.end())
            return &t;
    return nullptr;
}

MaterialInfo material_info(const KnowledgeMaterial& m) {
    MaterialInfo info;
    info.id = m.family;
    info.name = m.name;
    const KnowledgeRange nozzle = m.nozzle_temp.known ? m.nozzle_temp : m.base ? m.base->nozzle_temp : KnowledgeRange{};
    const KnowledgeRange bed = m.bed_temp.known ? m.bed_temp : m.base ? m.base->bed_temp : KnowledgeRange{};
    if (nozzle.known)
        info.nozzle_min = nozzle.min, info.nozzle_max = nozzle.max;
    if (bed.known)
        info.bed_min = bed.min, info.bed_max = bed.max;
    info.enclosure_needed = m.enclosure == "necesaria" || (m.base && m.base->enclosure == "necesaria");
    info.abrasive = m.abrasive;
    if (m.drying) {
        info.dry_before_printing = m.drying->before_printing == "si" || m.drying->before_printing == "sí";
        if (m.drying->temp.known)
            info.dry_min = m.drying->temp.min, info.dry_max = m.drying->temp.max;
        info.dry_hours = m.drying->hours_before_printing.value_or(0);
    }
    return info;
}

std::optional<SettingAction> setting_action(const KnowledgeAction& a) {
    if (a.type != "setting")
        return std::nullopt;
    const std::optional<SettingOp> op = setting_op_from(a.op);
    if (!op)
        return std::nullopt;
    return SettingAction{a.key, *op, a.value, a.of};
}

} // namespace tisma::ui
