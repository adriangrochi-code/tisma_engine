#include "ui/assistant.hpp"
#include "ui/knowledge.hpp"
#include "ui/guide.hpp"

#include "config/settings.hpp"
#include "ui/text.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

using namespace tisma;
using namespace tisma::ui;
using Catch::Matchers::WithinAbs;

namespace {

struct LanguageScope {
    Language before = language();
    explicit LanguageScope(Language l) { set_language(l); }
    ~LanguageScope() { set_language(before); }
};

SettingChange plan(const Settings& s, SettingAction a, const SettingRange& range = {}) {
    const std::optional<SettingChange> c = plan_setting(s, a, range);
    REQUIRE(c);
    return *c;
}

} // namespace

TEST_CASE("assistant: a setting set, shown before and after (AS4)", "[ui][assistant]") {
    const LanguageScope es(Language::Spanish);
    Settings s;
    s.fan_off_layers = 1;
    const SettingChange c = plan(s, {"cooling.fan_off_layers", SettingOp::Set, "4", {}}, parse_range("0–1000"));
    CHECK(c.before == "1");
    CHECK(c.after == "4");
    CHECK(c.profile == ProfileKind::Filament);
    CHECK_FALSE(c.clamped);
    const Settings after = apply_setting(s, c);
    CHECK(after.fan_off_layers == 4);
    // Undo is the settings before: nothing else moved.
    CHECK(apply_setting(after, plan(after, {"cooling.fan_off_layers", SettingOp::Set, "1", {}})).fan_off_layers == 1);
}

TEST_CASE("assistant: a temperature added to its variable (AS5)", "[ui][assistant]") {
    Settings s;
    s.variables["temperature"] = {"215"};
    const SettingChange c = plan(s, {"machine.variables.temperature", SettingOp::Add, "-5", {}});
    CHECK(c.before == "215");
    CHECK(c.after == "210");
    CHECK(c.profile == ProfileKind::Filament);
    CHECK(apply_setting(s, c).variables.at("temperature") == std::vector<std::string>{"210"});
    // One per extruder: each.
    s.variables["temperature"] = {"215", "230"};
    CHECK(apply_setting(s, plan(s, {"machine.variables.temperature", SettingOp::Add, "-5", {}})).variables.at("temperature") ==
          std::vector<std::string>{"210", "225"});
}

TEST_CASE("assistant: multiply and set_relative (AS6, AS7)", "[ui][assistant]") {
    Settings s;
    s.max_speed = 80.0;
    const SettingChange m = plan(s, {"speed.max", SettingOp::Multiply, "0.8", {}});
    CHECK(m.profile == ProfileKind::Printer);
    CHECK_THAT(apply_setting(s, m).max_speed, WithinAbs(64.0, 1e-9));
    s.nozzle_diameter = 0.4;
    const SettingChange r = plan(s, {"layers.height", SettingOp::SetRelative, "0.75", "machine.nozzle_diameter"});
    CHECK(r.profile == ProfileKind::Print);
    CHECK_THAT(apply_setting(s, r).layer_height, WithinAbs(0.3, 1e-9));
}

TEST_CASE("assistant: a value out of range takes the limit (AS8)", "[ui][assistant]") {
    Settings s;
    s.fan_off_layers = 3;
    const SettingChange c = plan(s, {"cooling.fan_off_layers", SettingOp::Add, "5000", {}}, parse_range("0–1000"));
    CHECK(c.clamped);
    CHECK(c.after == "1000");
    CHECK(apply_setting(s, c).fan_off_layers == 1000);
    const SettingChange low = plan(s, {"cooling.fan_off_layers", SettingOp::Add, "-10", {}}, parse_range("0–1000"));
    CHECK(low.clamped);
    CHECK(low.after == "0");
}

TEST_CASE("assistant: what an action cannot do", "[ui][assistant]") {
    Settings s;
    // A key the engine does not know: no button (rule 7).
    CHECK_FALSE(plan_setting(s, {"cooling.no_such_thing", SettingOp::Set, "1", {}}));
    CHECK_FALSE(known_setting(s, "cooling.no_such_thing"));
    CHECK(known_setting(s, "cooling.fan_off_layers"));
    CHECK_THROWS_AS(plan_setting(s, {"walls.seam", SettingOp::Multiply, "2", {}}), AssistantError);
    CHECK_THROWS_AS(plan_setting(s, {"walls.seam", SettingOp::Set, "3", {}}), AssistantError);
    CHECK_THROWS_AS(plan_setting(s, {"layers.height", SettingOp::Set, "not json", {}}), AssistantError);
    CHECK_THROWS_AS(plan_setting(s, {"layers.height", SettingOp::SetRelative, "1", "machine.nope"}), AssistantError);
    // A text set as text.
    const SettingChange seam = plan(s, {"walls.seam", SettingOp::Set, "\"rear\"", {}});
    CHECK(seam.after == "rear");
    CHECK(setting_op_from("set_relative") == SettingOp::SetRelative);
    CHECK_FALSE(setting_op_from("divide"));
}

TEST_CASE("assistant: ranges read from docs/CONFIG.md", "[ui][assistant]") {
    const SettingRange a = parse_range("0 a 100 000");
    CHECK(*a.low == 0.0);
    CHECK(*a.high == 100000.0);
    const SettingRange b = parse_range("\xE2\x88\x92" "360 a 360");
    CHECK(*b.low == -360.0);
    CHECK(*b.high == 360.0);
    const SettingRange c = parse_range("0,5 a 2");
    CHECK(*c.low == 0.5);
    const SettingRange d = parse_range("≥ 0");
    CHECK(*d.low == 0.0);
    CHECK_FALSE(d.high);
    CHECK(parse_range("más de 0 a 1000").low_open);
    CHECK(parse_range("> 0").low_open);
    CHECK_FALSE(parse_range("").low);
    CHECK_FALSE(parse_range("cualquiera").low);
}

TEST_CASE("assistant: last_resort causes go last (AS3)", "[ui][assistant]") {
    struct Cause {
        std::string id;
        bool last_resort = false;
    };
    const std::vector<Cause> causes = {{"a", false}, {"z", true}, {"b", false}, {"c", false}};
    std::string order;
    for (const Cause* c : causes_in_order(causes))
        order += c->id;
    CHECK(order == "abcz");
}

TEST_CASE("assistant: a material's card and warnings (AS9 to AS11)", "[ui][assistant]") {
    const LanguageScope es(Language::Spanish);
    std::vector<MaterialInfo> materials(4);
    materials[0].id = "pla", materials[0].name = "PLA";
    materials[1].id = "abs", materials[1].name = "ABS", materials[1].enclosure_needed = true;
    materials[1].bed_min = 90, materials[1].nozzle_min = 230;
    materials[2].id = "pla_cf", materials[2].name = "PLA-CF", materials[2].abrasive = true;
    materials[3].id = "pa", materials[3].name = "PA", materials[3].dry_before_printing = true;
    materials[3].dry_min = 70, materials[3].dry_max = 80, materials[3].dry_hours = 8;
    // By type, else by the profile's name, whatever the case.
    CHECK(find_material(materials, "ABS", "")->id == "abs");
    CHECK(find_material(materials, "", "Generic abs @0.4")->id == "abs");
    CHECK(find_material(materials, "", "Brand PLA-CF Black")->id == "pla_cf");
    CHECK(find_material(materials, "", "Brand PLA Black")->id == "pla");
    CHECK(find_material(materials, "", "Mystery") == nullptr);
    CHECK(find_material(materials, "", "Capable") == nullptr); // "pla" inside a word is not PLA
    Settings open;
    open.enclosed = false;
    // AS9: ABS on an open printer.
    std::vector<MaterialWarning> w = material_warnings(materials[1], open);
    REQUIRE(w == std::vector<MaterialWarning>{MaterialWarning::NeedsEnclosure});
    CHECK(material_warning_text(w.front(), materials[1]) == "Este material necesita una impresora cerrada");
    // The bed and the hot end only when the printer says how hot they go.
    open.max_bed_temp = 80, open.max_nozzle_temp = 300;
    CHECK(material_warnings(materials[1], open) ==
          std::vector<MaterialWarning>{MaterialWarning::NeedsEnclosure, MaterialWarning::BedTooCold});
    // AS10: abrasive without a hardened nozzle.
    CHECK(material_warnings(materials[2], open) == std::vector<MaterialWarning>{MaterialWarning::Abrasive});
    open.hardened_nozzle = true;
    CHECK(material_warnings(materials[2], open).empty());
    // AS11: drying.
    w = material_warnings(materials[3], open);
    REQUIRE(w == std::vector<MaterialWarning>{MaterialWarning::DryFirst});
    CHECK(material_warning_text(w.front(), materials[3]) == "Conviene secarlo antes de imprimir: 70 a 80 °C, 8 h");
    const LanguageScope en(Language::English);
    CHECK(material_warning_text(w.front(), materials[3]) == "Better dry it before printing: 70 to 80 °C, 8 h");
}

namespace {

const std::filesystem::path kKnowledge = std::filesystem::path(TISMA_TEST_DATA_DIR) / "knowledge";

} // namespace

TEST_CASE("assistant: the knowledge files read (docs/KNOWLEDGE.md)", "[ui][assistant]") {
    const Knowledge k = load_knowledge(kKnowledge, "es");
    CHECK(k.errors.empty());
    CHECK(k.language == "es");
    REQUIRE(k.materials.size() == 2);
    REQUIRE(k.problems.size() == 3);
    REQUIRE(k.procedures.size() == 1);
    REQUIRE(k.terms.size() == 5);
    REQUIRE(k.tutorials.size() == 1);
    CHECK_FALSE(k.sources.empty());
    // The glossary in alphabetical order, accents aside.
    CHECK(k.terms.front().term == "Altura de capa");
    CHECK(k.terms.back().term == "Retracción");
    // A material's details.
    const KnowledgeMaterial& abs = k.materials[1];
    CHECK(abs.base);
    CHECK(abs.base->chamber_temp == 45.0);
    CHECK_FALSE(abs.base->retraction_bowden);
    CHECK(abs.drying->hours_before_printing == 4);
    CHECK_FALSE(k.materials[0].drying);
    // The tutorial's steps.
    const KnowledgeTutorial& t = k.tutorials.front();
    REQUIRE(t.steps.size() == 3);
    CHECK(t.steps[0].wait_for == "printer_selected");
    CHECK(t.steps[2].open_type == "procedure");
    // A missing folder: nothing, no errors (AS1).
    const Knowledge none = load_knowledge(kKnowledge / "no_such_folder", "es");
    CHECK(none.empty());
    CHECK(none.errors.empty());
    // What is not such a file.
    Knowledge bad;
    CHECK_THROWS_AS(parse_knowledge_file("{}", bad), KnowledgeError);
    CHECK_THROWS_AS(parse_knowledge_file(R"({"format": "tisma-knowledge-terms", "version": 2})", bad), KnowledgeError);
    CHECK_THROWS_AS(parse_knowledge_file(R"({"format": "tisma-knowledge-glossary", "version": 1, "terms": [{"term": "x"}]})", bad),
                    KnowledgeError);
}

TEST_CASE("assistant: the search finds the problem and the term (AS2)", "[ui][assistant]") {
    const Knowledge k = load_knowledge(kKnowledge, "es");
    const std::vector<KnowledgeHit> hits = search_knowledge(k, "HILOS");
    bool problem = false, term = false;
    for (const KnowledgeHit& h : hits) {
        problem |= h.kind == KnowledgeHit::Kind::Problem && k.problems[h.index].id == "stringing";
        term |= h.kind == KnowledgeHit::Kind::Term && k.terms[h.index].term == "Hilos";
    }
    CHECK(problem);
    CHECK(term);
    CHECK(search_knowledge(k, "retraccion").size() >= 1); // accents aside
    CHECK(search_knowledge(k, "   ").empty());
}

TEST_CASE("assistant: the knowledge's causes, actions and materials at work (AS3 to AS9, AS15)", "[ui][assistant]") {
    const Knowledge k = load_knowledge(kKnowledge, "es");
    // AS3: the last_resort cause last.
    const std::vector<const KnowledgeCause*> causes = causes_in_order(k.problems[0].causes);
    REQUIRE(causes.size() == 3);
    CHECK(causes.back()->id == "wet");
    // AS4, AS5, AS7 from the file's actions.
    Settings s;
    s.fan_off_layers = 1;
    s.variables["temperature"] = {"215"};
    s.nozzle_diameter = 0.4;
    const auto& fan = k.problems[2].causes[0].actions;
    const std::optional<SettingAction> four = setting_action(fan[0]);
    REQUIRE(four);
    const std::optional<SettingChange> c = plan_setting(s, *four);
    REQUIRE(c);
    CHECK(c->before == "1");
    CHECK(apply_setting(s, *c).fan_off_layers == 4);
    const std::optional<SettingChange> first = plan_setting(s, *setting_action(fan[1]));
    REQUIRE(first);
    CHECK(std::abs(apply_setting(s, *first).first_layer_height - 0.3) < 1e-9);
    // A key the engine does not have: no button.
    CHECK_FALSE(plan_setting(s, *setting_action(fan[2])));
    CHECK_FALSE(setting_action(fan[3]));
    const std::optional<SettingChange> cooler = plan_setting(s, *setting_action(k.problems[0].causes[0].actions[0]));
    REQUIRE(cooler);
    CHECK(cooler->after == "210");
    // AS9: the ABS card on an open printer.
    const MaterialInfo abs = material_info(k.materials[1]);
    CHECK(abs.enclosure_needed);
    CHECK(abs.dry_before_printing);
    CHECK(abs.dry_hours == 4);
    Settings open;
    const std::vector<MaterialWarning> w = material_warnings(abs, open);
    CHECK(std::find(w.begin(), w.end(), MaterialWarning::NeedsEnclosure) != w.end());
    CHECK(std::find(w.begin(), w.end(), MaterialWarning::DryFirst) != w.end());
    std::vector<MaterialInfo> all;
    for (const KnowledgeMaterial& m : k.materials)
        all.push_back(material_info(m));
    CHECK(find_material(all, "", "Generic ABS @0.4")->name == "ABS");
    // AS15: the term for a setting, the first in alphabetical order.
    const KnowledgeTerm* walls = term_for_setting(k, "walls.count");
    REQUIRE(walls);
    CHECK(walls->term == "Pared");
    CHECK(term_for_setting(k, "retraction.length")->term == "Hilos");
    CHECK_FALSE(term_for_setting(k, "infill.density"));
}

TEST_CASE("assistant: the guided tutorials, offered, moved on and resumed (AS12 to AS14)", "[ui][assistant]") {
    const Knowledge k = load_knowledge(kKnowledge, "es");
    GuideMemory memory;
    // AS12: at the end of the first start, the first_print tutorial; its
    // first step points at the printers' list.
    const std::vector<std::size_t> offered = tutorials_to_offer(k, "first_start", memory);
    REQUIRE(offered.size() == 1);
    Guide g = start_guide(k, offered.front(), memory);
    CHECK(k.tutorials[g.tutorial].steps[std::size_t(g.step)].anchor == "printer_selector");
    CHECK(tutorials_to_offer(k, "on_request", memory).empty());
    CHECK(tutorials_to_offer(k, "new_filament", memory).empty());
    // A step that waits for an event moves on with it, not with another.
    CHECK_FALSE(guide_event(k, g, "model_added"));
    CHECK(g.step == 0);
    CHECK_FALSE(guide_event(k, g, "printer_selected"));
    CHECK(g.step == 1);
    // AS13: adding a model moves step 2 on by itself.
    CHECK_FALSE(guide_event(k, g, "model_added"));
    CHECK(g.step == 2);
    // AS14: left on that step, it starts there again.
    remember(memory, k, g, false);
    const Guide again = start_guide(k, g.tutorial, memory);
    CHECK(again.step == 2);
    guide_back(g);
    CHECK(g.step == 1);
    // The last step's Next ends it; a finished or turned down one is not offered.
    g.step = 2;
    CHECK(guide_next(k, g));
    remember(memory, k, g, true);
    CHECK(tutorials_to_offer(k, "first_start", memory).empty());
    CHECK(start_guide(k, g.tutorial, memory).step == 0);
    GuideMemory no;
    no.dismissed.insert("first_print");
    CHECK(tutorials_to_offer(k, "first_start", no).empty());
}
