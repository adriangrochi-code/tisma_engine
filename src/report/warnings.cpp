// Slicing warnings as JSON (docs/spec/BEHAVIOR.md, "Errores y avisos"). Own
// design; see PROVENANCE.md.

#include "report/warnings.hpp"

#include "slice/pipeline.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace tisma {

namespace {

// The longest stretch of a layer's bridge lines with nothing below: neither
// the layer below, grown by half a bead where the bridge rests on it, nor the
// support (print_review.md, rule 2).
double longest_free_bridge(const SlicedModel& model, std::size_t l) {
    if (l == 0 || l >= model.fills.size() || model.fills[l].bridge.empty())
        return 0.0;
    double half = 0.0;
    Clipper2Lib::Paths64 lines;
    for (const Toolpath& t : model.fills[l].bridge) {
        Clipper2Lib::Path64 line;
        for (const ToolpathPoint& p : t.points) {
            line.push_back({to_units(p.p.x), to_units(p.p.y)});
            half = std::max(half, p.width / 2.0);
        }
        if (line.size() > 1)
            lines.push_back(std::move(line));
    }
    Polygons below = l - 1 < model.regions.size() ? model.regions[l - 1] : Polygons{};
    if (l - 1 < model.support_areas.size())
        below = Clipper2Lib::Union(below, model.support_areas[l - 1], Clipper2Lib::FillRule::NonZero);
    Clipper2Lib::Clipper64 clip;
    clip.AddOpenSubject(lines);
    clip.AddClip(offset(below, half));
    Clipper2Lib::Paths64 closed, open;
    clip.Execute(Clipper2Lib::ClipType::Difference, Clipper2Lib::FillRule::NonZero, closed, open);
    double longest = 0.0;
    for (const Clipper2Lib::Path64& piece : open)
        longest = std::max(longest, Clipper2Lib::Length(piece) / kUnitsPerMm);
    return longest;
}

} // namespace

std::vector<Warning> model_warnings(const SlicedModel& model, double free_bridge_length) {
    std::vector<Warning> out;
    for (std::size_t l = 0; l < model.layers.size(); ++l) {
        const double z = model.layers[l].top;
        if (l < model.open_by_layer.size() && model.open_by_layer[l] > 0)
            out.push_back({"open_contours", int(l), z,
                           std::to_string(model.open_by_layer[l]) +
                               " open contours dropped (holes in the mesh wider than slicing.gap_closing_radius)",
                           double(model.open_by_layer[l])});
        if (l < model.thin_area.size() && model.thin_area[l] >= kThinDetailMinArea) {
            char text[128];
            std::snprintf(text, sizeof text, "%.3f mm2 of details thinner than the thinnest bead are not printed",
                          model.thin_area[l]);
            out.push_back({"thin_detail", int(l), z, text, model.thin_area[l]});
        }
    }
    // Long bridges (print_review.md, rule 2), a warning for each run of layers.
    if (free_bridge_length > 0.0) {
        std::size_t first = 0;
        double longest = 0.0;
        for (std::size_t l = 1; l <= model.layers.size(); ++l) {
            const double free = l < model.layers.size() ? longest_free_bridge(model, l) : 0.0;
            if (free > free_bridge_length) {
                if (longest == 0.0)
                    first = l;
                longest = std::max(longest, free);
                continue;
            }
            if (longest > 0.0) {
                char text[200];
                std::snprintf(text, sizeof text,
                              "long bridge: %.1f mm over nothing between Z %.2f and %.2f, more than "
                              "support.free_bridge_length (%.1f mm)",
                              longest, model.layers[first].bottom, model.layers[l - 1].top, free_bridge_length);
                out.push_back({"long_bridge", int(first), model.layers[first].top, text, longest});
                longest = 0.0;
            }
        }
    }
    // Settings of a part left out (model_tools.md, rules 4 and 9).
    for (const std::string& notice : model.notices)
        out.push_back({"part_settings", -1, 0.0, notice, 0.0, "information"});
    return out;
}

std::string warnings_json(const std::vector<Warning>& warnings) {
    nlohmann::json list = nlohmann::json::array();
    for (const Warning& w : warnings) {
        nlohmann::json item;
        item["kind"] = w.kind;
        item["layer"] = w.layer >= 0 ? nlohmann::json(w.layer) : nlohmann::json(nullptr);
        item["z"] = w.layer >= 0 ? nlohmann::json(w.z) : nlohmann::json(nullptr);
        item["message"] = w.message;
        item["value"] = w.value;
        item["severity"] = w.severity;
        list.push_back(std::move(item));
    }
    return nlohmann::json{{"version", 1}, {"warnings", std::move(list)}}.dump(2) + "\n";
}

} // namespace tisma
