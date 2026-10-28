#pragma once

#include <string>
#include <vector>

namespace tisma {

struct SlicedModel;
struct Settings;

// A slicing problem for Tisma to show (docs/spec/BEHAVIOR.md, "Errores y
// avisos"). layer is -1 (and z unused) for the whole model.
struct Warning {
    std::string kind; // "thin_detail", "layer_time", "open_contours", "gpu_fallback", "purge", "part_settings", and the machine's
                      // capabilities: "nozzle_temperature", "bed_temperature", "chamber", "enclosure", "abrasive"
    int layer = -1;
    double z = 0.0;
    std::string message;
    double value = 0.0; // mm2 for thin_detail, s for layer_time, a count for open_contours, g (mm without a
                        // density) for purge
    std::string severity = "warning"; // "critical", "warning" or "information" (print_review.md)
};

// Details too thin for any bead below this area per layer are not reported.
inline constexpr double kThinDetailMinArea = 0.01; // mm2

// The warnings the sliced model carries: contours left open and parts of a
// layer thinner than the thinnest printable bead.
std::vector<Warning> model_warnings(const SlicedModel& model);

// {"version": 1, "warnings": [{"kind", "layer", "z", "message", "value"}, ...]};
// layer and z are null for warnings about the whole model.
std::string warnings_json(const std::vector<Warning>& warnings);

} // namespace tisma
