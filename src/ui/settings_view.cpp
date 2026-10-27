#include "ui/settings_view.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <optional>

namespace tisma::ui {
namespace {

using nlohmann::json;

std::vector<std::string> words_of(std::string_view text) {
    std::vector<std::string> words;
    std::string word;
    for (const char c : fold(text)) {
        if (c == ' ' || c == '\t' || c == ',' || c == '.' || c == '_') {
            if (!word.empty())
                words.push_back(std::move(word)), word.clear();
        } else {
            word.push_back(c);
        }
    }
    if (!word.empty())
        words.push_back(std::move(word));
    return words;
}

} // namespace

std::string fold(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        const auto c = static_cast<unsigned char>(text[i]);
        if (c == 0xC3 && i + 1 < text.size()) {
            // Latin-1's letters with marks: the lower and upper case alike.
            const unsigned char d = static_cast<unsigned char>(text[i + 1]) | 0x20;
            const char plain = d >= 0xA0 && d <= 0xA5   ? 'a'
                               : d == 0xA7              ? 'c'
                               : d >= 0xA8 && d <= 0xAB ? 'e'
                               : d >= 0xAC && d <= 0xAF ? 'i'
                               : d == 0xB1              ? 'n'
                               : d >= 0xB2 && d <= 0xB6 ? 'o'
                               : d >= 0xB9 && d <= 0xBC ? 'u'
                                                        : '\0';
            if (plain) {
                out.push_back(plain);
                ++i;
                continue;
            }
        }
        out.push_back(c < 0x80 ? char(std::tolower(c)) : char(c));
    }
    return out;
}

const std::vector<std::string>& panel_keys() {
    static const std::vector<std::string> keys = [] {
        std::vector<std::string> out;
        for (const SettingsTab tab : {SettingsTab::Print, SettingsTab::Material, SettingsTab::Printer})
            for (const SectionLayout& section : settings_layout(tab))
                for (const SubsectionLayout& sub : section.subsections)
                    for (const SettingEntry& e : sub.settings)
                        if (std::find(out.begin(), out.end(), e.key) == out.end())
                            out.emplace_back(e.key);
        return out;
    }();
    return keys;
}

std::vector<SettingHit> search_settings(std::string_view query,
                                        const std::function<SettingText(std::string_view key)>& text,
                                        const std::function<bool(std::string_view key)>& wanted, std::size_t limit) {
    const std::vector<std::string> words = words_of(query);
    std::vector<SettingHit> hits;
    if (words.empty())
        return hits;
    const std::string whole = fold(query);
    for (const std::string& key : panel_keys()) {
        if (wanted && !wanted(key))
            continue;
        const std::optional<SettingPlace> place = setting_place(key);
        if (!place)
            continue;
        const SettingText t = text(key);
        // The fields, by how much a word found in them counts.
        const std::string label = fold(t.label);
        const std::array<std::pair<std::string, int>, 5> fields = {{{label, 8},
                                                                     {fold(tr(place->subsection->title)), 5},
                                                                     {fold(key), 4},
                                                                     {fold(tr(place->section->title)), 2},
                                                                     {fold(t.about), 1}}};
        int score = 0;
        bool all = true;
        for (const std::string& w : words) {
            int best = 0;
            for (const auto& [field, weight] : fields)
                if (field.find(w) != std::string::npos)
                    best = std::max(best, weight);
            if (best == 0) {
                all = false;
                break;
            }
            score += best;
        }
        if (!all)
            continue;
        if (label.starts_with(whole))
            score += 4;
        hits.push_back({key, *place, score});
    }
    std::stable_sort(hits.begin(), hits.end(), [](const SettingHit& a, const SettingHit& b) { return a.score > b.score; });
    if (hits.size() > limit)
        hits.resize(limit);
    return hits;
}

CustomView custom_view_from(ViewLevel level) {
    CustomView view;
    view.base = level == ViewLevel::Custom ? ViewLevel::Normal : level;
    for (const std::string& key : panel_keys()) {
        view.known.insert(key);
        if (const std::optional<SettingPlace> place = setting_place(key); place && level_shows(view.base, place->level))
            view.visible.insert(key);
    }
    return view;
}

bool custom_shows(const CustomView& view, std::string_view key) {
    const std::string k(key);
    if (view.visible.contains(k))
        return true;
    if (view.known.contains(k))
        return false;
    // A setting newer than the list: by its level (rule 18).
    const std::optional<SettingPlace> place = setting_place(key);
    return place && level_shows(view.base, place->level);
}

void custom_set(CustomView& view, const std::vector<SettingEntry>& settings, bool shown) {
    for (const SettingEntry& e : settings) {
        if (shown)
            view.visible.insert(std::string(e.key));
        else
            view.visible.erase(std::string(e.key));
        view.known.insert(std::string(e.key));
    }
}

std::string custom_view_json(const CustomView& view) {
    json keys = json::array();
    for (const std::string& key : panel_keys()) // in the panel's order
        if (view.visible.contains(key))
            keys.push_back(key);
    const nlohmann::ordered_json out = {{"format", "tisma-ui-view"},
                                        {"version", 1},
                                        {"base", std::string(view_level_name(view.base))},
                                        {"keys", keys}};
    return out.dump(2) + "\n";
}

CustomView parse_custom_view(std::string_view text) {
    json root;
    try {
        root = json::parse(text);
    } catch (const json::exception&) {
        throw ViewError("not a JSON file");
    }
    if (!root.is_object() || !root.contains("format") || root.at("format") != "tisma-ui-view")
        throw ViewError("not a view of the settings (format tisma-ui-view)");
    if (!root.contains("version") || !root.at("version").is_number_integer() || root.at("version").get<long>() != 1)
        throw ViewError("a view of another version");
    if (!root.contains("keys") || !root.at("keys").is_array())
        throw ViewError("the view has no list of keys");
    CustomView view;
    view.base = root.contains("base") && root.at("base").is_string() ? view_level_from(root.at("base").get<std::string>())
                                                                      : ViewLevel::Normal;
    if (view.base == ViewLevel::Custom)
        view.base = ViewLevel::Normal;
    const std::vector<std::string>& all = panel_keys();
    view.known.insert(all.begin(), all.end());
    for (const json& key : root.at("keys"))
        if (key.is_string() && view.known.contains(key.get<std::string>()))
            view.visible.insert(key.get<std::string>());
    return view;
}

std::string_view view_level_name(ViewLevel level) {
    switch (level) {
    case ViewLevel::Basic:
        return "basic";
    case ViewLevel::Normal:
        return "normal";
    case ViewLevel::Advanced:
        return "advanced";
    case ViewLevel::Custom:
        return "custom";
    }
    return "normal";
}

ViewLevel view_level_from(std::string_view name) {
    return name == "basic"      ? ViewLevel::Basic
           : name == "advanced" ? ViewLevel::Advanced
           : name == "custom"   ? ViewLevel::Custom
                                : ViewLevel::Normal;
}

} // namespace tisma::ui
