// Slicing warnings as JSON (docs/spec/BEHAVIOR.md, "Errores y avisos"). Own
// design; see PROVENANCE.md.

#include "report/warnings.hpp"

#include "slice/pipeline.hpp"

#include <nlohmann/json.hpp>

#include <cstdio>

namespace tisma {

std::vector<Warning> model_warnings(const SlicedModel& model) {
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
        list.push_back(std::move(item));
    }
    return nlohmann::json{{"version", 1}, {"warnings", std::move(list)}}.dump(2) + "\n";
}

} // namespace tisma
