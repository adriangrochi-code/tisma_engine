#include "ui/printer_card.hpp"

#include "ui/settings_view.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>

namespace tisma::ui {

PrinterKind printer_kind(const Settings& s) {
    if (s.machine_technology == MachineTechnology::Msla)
        return PrinterKind::Resin;
    if (s.belt_enabled)
        return PrinterKind::Belt;
    if (round_bed(s.bed))
        return PrinterKind::Delta;
    return s.enclosed ? PrinterKind::Enclosed : PrinterKind::Open;
}

bool round_bed(const std::vector<std::array<double, 2>>& bed) {
    if (bed.size() < 12)
        return false;
    double cx = 0.0, cy = 0.0;
    for (const auto& p : bed)
        cx += p[0], cy += p[1];
    cx /= double(bed.size()), cy /= double(bed.size());
    double lo = 1e300, hi = 0.0;
    for (const auto& p : bed) {
        const double r = std::hypot(p[0] - cx, p[1] - cy);
        lo = std::min(lo, r), hi = std::max(hi, r);
    }
    return hi > 0.0 && lo >= 0.98 * hi;
}

BedFamily bed_family(std::string_view name) {
    const std::string n = fold(name);
    const auto has = [&](std::string_view w) { return n.find(w) != std::string::npos; };
    if (has("textur") || has("satin"))
        return BedFamily::Textured;
    if (has("glass") || has("vidrio"))
        return BedFamily::Glass;
    if (has("cool"))
        return BedFamily::Cool;
    if (has("engineering") || has("high temp"))
        return BedFamily::Engineering;
    if (has("smooth") || has("pei"))
        return BedFamily::Smooth;
    return BedFamily::Other;
}

NozzleLook nozzle_look(double diameter, bool hardened, std::string_view name) {
    NozzleLook look;
    look.diameter = diameter;
    look.hardened = hardened;
    const std::string n = fold(name);
    // "HF" as a word of its own.
    bool hf = false;
    for (std::size_t at = n.find("hf"); at != std::string::npos && !hf; at = n.find("hf", at + 1))
        hf = (at == 0 || !std::isalnum(static_cast<unsigned char>(n[at - 1]))) &&
             (at + 2 == n.size() || !std::isalnum(static_cast<unsigned char>(n[at + 2])));
    look.high_flow = hf || n.find("high flow") != std::string::npos;
    look.size = std::clamp((diameter - 0.2) / 1.0, 0.0, 1.0);
    return look;
}

std::optional<CatalogMatch> catalog_model_of(const Catalog& catalog, std::string_view name) {
    // A copy's " (2)" off.
    if (name.ends_with(")"))
        if (const std::size_t open = name.rfind(" ("); open != std::string_view::npos)
            name = name.substr(0, open);
    if (!name.ends_with(" mm"))
        return std::nullopt;
    for (const PrinterModel& p : catalog.printers) {
        const std::string head = p.vendor + " " + p.model + " ";
        if (!name.starts_with(head))
            continue;
        const std::string_view number = name.substr(head.size(), name.size() - head.size() - 3);
        double d = 0.0;
        const auto [end, ec] = std::from_chars(number.data(), number.data() + number.size(), d);
        if (ec == std::errc() && end == number.data() + number.size() && d > 0.0)
            return CatalogMatch{&p, d};
    }
    return std::nullopt;
}

std::vector<NozzleChoice> nozzle_choices(const Catalog& catalog, std::string_view profile_name, double nozzle) {
    std::vector<NozzleChoice> out;
    if (const std::optional<CatalogMatch> match = catalog_model_of(catalog, profile_name)) {
        std::vector<double> sizes = match->model->nozzles;
        if (std::none_of(sizes.begin(), sizes.end(), [&](double d) { return std::abs(d - match->nozzle) < 1e-9; }))
            sizes.push_back(match->nozzle);
        std::sort(sizes.begin(), sizes.end());
        for (const double d : sizes)
            out.push_back({d, printer_profile_name(*match->model, d)});
        return out;
    }
    out.push_back({nozzle, {}});
    return out;
}

CalibrationState calibration_state(const CalibrationRecord* record, double nozzle) {
    if (!record)
        return CalibrationState::Missing;
    return std::abs(record->nozzle - nozzle) < 1e-6 ? CalibrationState::Done : CalibrationState::OtherNozzle;
}

std::vector<CalibrationKind> printer_calibrations(const Settings& s) {
    const bool resin = s.machine_technology == MachineTechnology::Msla;
    // The order to make them in: the machine first, then the filament.
    static const CalibrationKind order[] = {
        CalibrationKind::Dimensions,   CalibrationKind::ElephantFoot,    CalibrationKind::FirstLayer,
        CalibrationKind::Temperature,  CalibrationKind::Flow,            CalibrationKind::PressureAdvance,
        CalibrationKind::Retraction,   CalibrationKind::RetractionSpeed, CalibrationKind::MaxFlow,
        CalibrationKind::Speed,        CalibrationKind::Acceleration,    CalibrationKind::Fit,
        CalibrationKind::ResinExposure, CalibrationKind::ResinMeasure};
    std::vector<CalibrationKind> out;
    for (const CalibrationKind k : order)
        if (is_resin_calibration(k) == resin)
            out.push_back(k);
    return out;
}

std::string_view calibration_key(CalibrationKind kind) {
    switch (kind) {
    case CalibrationKind::Dimensions: return "compensation.scale_x";
    case CalibrationKind::Temperature: return "machine.variables.temperature";
    case CalibrationKind::Retraction: return "retraction.length";
    case CalibrationKind::Flow: return "extrusion.flow";
    case CalibrationKind::Speed: return "speed.max";
    case CalibrationKind::RetractionSpeed: return "retraction.speed";
    case CalibrationKind::PressureAdvance: return "extrusion.pressure_advance";
    case CalibrationKind::MaxFlow: return "machine.max_flow";
    case CalibrationKind::Acceleration: return "machine.acceleration";
    case CalibrationKind::ElephantFoot: return "slicing.first_layer_shrink";
    case CalibrationKind::ResinExposure: return "resin.exposure";
    case CalibrationKind::ResinMeasure: return "resin.scale_x";
    case CalibrationKind::FirstLayer:
    case CalibrationKind::Fit: return {};
    }
    return {};
}

bool filament_calibration(CalibrationKind kind) {
    return kind == CalibrationKind::Temperature || kind == CalibrationKind::Flow ||
           kind == CalibrationKind::PressureAdvance || kind == CalibrationKind::Retraction ||
           kind == CalibrationKind::RetractionSpeed || kind == CalibrationKind::ResinExposure;
}

std::optional<CalibrationRecord> calibration_record(const Settings& s, std::string_view name, std::string_view printer,
                                                    std::string_view filament) {
    const std::string key(name);
    if (const auto f = s.filament_calibrations.find(key);
        f != s.filament_calibrations.end() && !f->second.date.empty() &&
        (f->second.printer.empty() || f->second.printer == printer))
        return CalibrationRecord{f->second.date, f->second.nozzle, std::string(filament)};
    if (const auto p = s.printer_calibrations.find(key); p != s.printer_calibrations.end() && !p->second.date.empty())
        return CalibrationRecord{p->second.date, p->second.nozzle, {}};
    return std::nullopt;
}

void date_calibration(Settings& s, std::string_view name, bool filament, std::string_view date,
                      std::string_view printer) {
    const std::string key(name);
    if (filament)
        s.filament_calibrations[key] = {std::string(date), s.nozzle_diameter, std::string(printer)};
    else
        s.printer_calibrations[key] = {std::string(date), s.nozzle_diameter, {}};
}

std::set<std::string> calibrations_dated(const Settings& s, std::string_view printer) {
    std::set<std::string> out;
    for (const auto& [name, d] : s.printer_calibrations)
        if (!d.date.empty())
            out.insert(name);
    for (const auto& [name, d] : s.filament_calibrations)
        if (!d.date.empty() && (d.printer.empty() || d.printer == printer))
            out.insert(name);
    return out;
}

} // namespace tisma::ui
