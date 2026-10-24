// The interface's built-in profiles (docs/PROFILES.md). Own choice of generic
// values; see PROVENANCE.md.

#include "ui/presets.hpp"

#include "ui/slicing.hpp"

#include <string>
#include <utility>

namespace tisma::ui {
namespace {

Profile make(ProfileKind kind, std::string name, Settings settings) {
    Profile p;
    p.kind = kind;
    p.name = std::move(name);
    p.settings = std::move(settings);
    return p;
}

Settings filament(int nozzle, int bed, double fan_min, double fan_max, int fan_off_layers) {
    Settings s = default_settings();
    s.variables["temperature"] = {std::to_string(nozzle)};
    s.variables["bed_temperature"] = {std::to_string(bed)};
    s.fan_min = fan_min;
    s.fan_max = fan_max;
    s.fan_off_layers = fan_off_layers;
    return s;
}

} // namespace

std::vector<Profile> builtin_profiles(ProfileKind kind) {
    const Settings base = default_settings();
    switch (kind) {
    case ProfileKind::Printer:
        return {make(kind, "Marlin 256 × 256 mm", base)};
    case ProfileKind::Filament:
        return {make(kind, "PLA", base),
                make(kind, "PETG", filament(240, 80, 20.0, 50.0, 1)),
                make(kind, "ABS", filament(250, 100, 0.0, 20.0, 3)),
                make(kind, "TPU", filament(225, 50, 50.0, 100.0, 1))};
    case ProfileKind::Print: {
        Settings fine = base;
        fine.layer_height = 0.12;
        fine.top_layers = 5;
        fine.bottom_layers = 4;
        Settings fast = base;
        fast.layer_height = 0.28;
        fast.first_layer_height = 0.28;
        fast.wall_count = 2;
        fast.infill_density = 15.0;
        return {make(kind, "0.20 mm", base), make(kind, "0.12 mm", fine), make(kind, "0.28 mm", fast)};
    }
    }
    return {};
}

} // namespace tisma::ui
