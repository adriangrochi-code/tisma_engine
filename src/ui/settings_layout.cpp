// The settings panel's sections, cards and levels (docs/spec/features/
// settings_panel.md). Own design on that spec; see PROVENANCE.md.

#include "ui/settings_layout.hpp"

#include <cmath>
#include <cstdio>

namespace tisma::ui {
namespace {

constexpr SettingLevel B = SettingLevel::Basic, N = SettingLevel::Normal, A = SettingLevel::Advanced;

const std::vector<SectionLayout>& print_layout() {
    static const std::vector<SectionLayout> layout = {
        {"quality", T::SecQuality, "section_quality.svg",
         {{"layer_height", T::SubLayerHeight, {{"layers.height", B}, {"layers.first_height", B}}},
          {"variable_height", T::SubVariableHeight, {{"layers.heights", N}, {"layers.cusp_height", A}}},
          {"fine_detail", T::SubFineDetail,
           {{"layers.detail_height", A}, {"layers.detail_tolerance", A}, {"layers.detail_margin", A}}},
          {"line_width", T::SubLineWidth, {{"walls.width", N}}},
          {"seam", T::SubSeam,
           {{"walls.seam", B},
            {"walls.scarf", N},
            {"walls.scarf_length", A},
            {"walls.scarf_entire_loop", N},
            {"walls.scarf_start_height", A},
            {"walls.scarf_max_segment", A},
            {"walls.scarf_only_smooth", A},
            {"walls.scarf_inner", A}}},
          {"precision", T::SubPrecision,
           {{"slicing.xy_offset", A},
            {"slicing.first_layer_shrink", A},
            {"compensation.scale_x", A},
            {"compensation.scale_y", A},
            {"compensation.scale_z", A},
            {"compensation.hole_offset", A}}},
          {"slopes", T::SubSlopes, {{"zaa.enabled", N}, {"zaa.min_thickness", A}, {"zaa.raise", A}}}}},
        {"strength", T::SecStrength, "section_strength.svg",
         {{"walls", T::SubWalls, {{"walls.count", B}}},
          {"top_bottom", T::SubTopBottom,
           {{"surfaces.top_layers", B},
            {"surfaces.bottom_layers", B},
            {"surfaces.top_min_thickness", B},
            {"surfaces.bottom_min_thickness", B},
            {"surfaces.top_pattern", B}}},
          {"infill", T::SubInfill, {{"infill.density", B}, {"infill.pattern", B}, {"infill.angle", N}}},
          {"infill_more", T::SubInfillMore,
           {{"infill.wall_overlap", A},
            {"infill.solid_every", A},
            {"infill.solid_below_area", A},
            {"infill.dense_under_top", N},
            {"infill.dense_density", N},
            {"infill.stress_density", A}}},
          {"combine", T::SubCombine,
           {{"infill.combine_layers", A}, {"infill.combine_auto", A}, {"infill.combine_max_height", A}}},
          {"wavy", T::SubWavy,
           {{"infill.nonplanar", N}, {"infill.nonplanar_amplitude", N}, {"infill.nonplanar_wavelength", N}}}}},
        {"speed", T::SecSpeed, "section_speed.svg",
         {{"first_layer", T::SubFirstLayerSpeed, {{"speed.first_layer", N}}},
          {"walls_surfaces", T::SubWallSpeed,
           {{"speed.walls", N}, {"speed.outer_wall", N}, {"speed.solid_infill", N}, {"speed.top_solid_infill", N}}},
          {"infill_support", T::SubInfillSpeed, {{"speed.infill", N}, {"speed.support", N}}},
          {"travel", T::SubTravelSpeed, {{"speed.travel", N}}},
          {"limits", T::SubSpeedCap, {{"speed.max", N}}}}},
        {"support", T::SecSupport, "section_support.svg",
         {{"support", T::SubSupport, {{"support.enabled", B}, {"support.automatic", B}, {"support.threshold_angle", N}}},
          {"gap", T::SubSupportGap, {{"support.z_gap", N}, {"support.xy_gap", N}}},
          {"interface", T::SubSupportInterface, {{"support.interface_layers", N}}},
          {"pattern", T::SubSupportPattern, {{"support.spacing", N}}}}},
        {"adhesion", T::SecAdhesion, "section_adhesion.svg",
         {{"skirt", T::SubSkirt, {{"adhesion.skirt_loops", B}, {"adhesion.skirt_distance", B}, {"adhesion.skirt_layers", N}}},
          {"brim", T::SubBrim, {{"adhesion.brim_width", B}}}}},
        {"other", T::SecOther, "section_other.svg",
         {{"special_modes", T::SubSpecialModes, {{"vase.enabled", N}}},
          {"fuzzy", T::SubFuzzy, {{"fuzzy.mode", N}, {"fuzzy.thickness", N}, {"fuzzy.point_distance", N}}},
          {"gcode_output", T::SubGcodeOut, {{"gcode.resolution", A}, {"changes", A}}},
          {"slicing", T::SubSlicing,
           {{"slicing.fill_mode", A}, {"slicing.gap_closing_radius", A}, {"slicing.simplify_tolerance", A}}}}},
    };
    return layout;
}

const std::vector<SectionLayout>& material_layout() {
    static const std::vector<SectionLayout> layout = {
        {"temperature", T::SecTemperature, "section_temperature.svg",
         {{"nozzle_temp", T::SubNozzleTemp, {{"machine.variables.temperature", B}}},
          {"bed_temp", T::SubBedTemp, {{"machine.variables.bed_temperature", B}}},
          {"chamber", T::SubChamber, {{"extrusion.chamber_temperature", N}}}}},
        {"cooling", T::SecCooling, "section_cooling.svg",
         {{"fan", T::SubFan,
           {{"cooling.fan_min", B},
            {"cooling.fan_max", B},
            {"cooling.fan_below_time", N},
            {"cooling.fan_always", N},
            {"cooling.fan_bridges", N},
            {"cooling.fan_off_layers", N}}},
          {"short_layers", T::SubShortLayers,
           {{"cooling.min_layer_time", N}, {"cooling.min_speed", N}, {"cooling.keep_outer_speed", A}}}}},
        {"flow", T::SecFlow, "section_flow.svg",
         {{"flow", T::SubFlow, {{"extrusion.flow", N}}},
          {"pressure", T::SubPressure, {{"extrusion.pressure_advance", A}}},
          {"max_flow", T::SubMaxFlow, {{"extrusion.max_flow", N}}}}},
        {"retraction", T::SecRetraction, "section_retraction.svg",
         {{"retraction", T::SubRetract, {{"retraction.length", B}, {"retraction.speed", N}}},
          {"retract_travel", T::SubRetractTravel, {{"retraction.lift", B}, {"retraction.min_travel", N}}}}},
        {"needs", T::SecNeeds, "section_needs.svg",
         {{"needs", T::SubNeeds, {{"extrusion.abrasive", N}, {"extrusion.needs_enclosure", N}}}}},
    };
    return layout;
}

const std::vector<SectionLayout>& printer_layout() {
    static const std::vector<SectionLayout> layout = {
        {"machine", T::SecMachine, "section_machine.svg",
         {{"bed_volume", T::SubBedVolume, {{"machine.bed", B}, {"machine.max_height", B}}},
          {"enclosure", T::SubEnclosure, {{"machine.enclosed", N}, {"machine.max_chamber_temp", N}}}}},
        {"nozzle", T::SecNozzle, "section_nozzle.svg",
         {{"nozzle_size", T::SubNozzle,
           {{"machine.nozzle_diameter", B},
            {"machine.hardened_nozzle", N},
            {"machine.min_layer_height", N},
            {"machine.max_layer_height", N}}},
          {"extruder", T::SubExtruder,
           {{"machine.filament_diameter", B}, {"machine.max_flow", N}, {"machine.relative_extrusion", N}}}}},
        {"limits", T::SecLimits, "section_limits.svg",
         {{"speeds", T::SubSpeedLimits,
           {{"machine.max_feed_x", N},
            {"machine.max_feed_y", N},
            {"machine.max_feed_z", N},
            {"machine.max_feed_e", N},
            {"machine.corner_speed", N}}},
          {"accelerations", T::SubAccel, {{"machine.acceleration", N}, {"machine.travel_acceleration", N}}},
          {"max_temps", T::SubMaxTemps, {{"machine.max_nozzle_temp", N}, {"machine.max_bed_temp", N}}}}},
        {"gcode", T::SecGcode, "section_gcode.svg",
         {{"firmware", T::SubFirmware, {{"machine.flavor", N}}},
          {"start_end", T::SubStartEnd, {{"machine.start_gcode", N}, {"machine.end_gcode", N}}},
          {"variables", T::SubVariables, {{"machine.variables", A}}}}},
    };
    return layout;
}

// A number as the interface writes it: up to `decimals` places, no trailing
// zeros, the decimal comma in Spanish.
std::string number(double v, int decimals = 2) {
    char text[64];
    std::snprintf(text, sizeof text, "%.*f", decimals, v);
    std::string out = text;
    if (out.find('.') != std::string::npos) {
        while (out.back() == '0')
            out.pop_back();
        if (out.back() == '.')
            out.pop_back();
    }
    if (out == "-0")
        out = "0";
    if (language() == Language::Spanish)
        for (char& c : out)
            if (c == '.')
                c = ',';
    return out;
}

std::string format(T id, auto... args) {
    char text[256];
    std::snprintf(text, sizeof text, tr(id), args...);
    return text;
}

const char* seam_title(SeamMode mode) {
    switch (mode) {
    case SeamMode::Aligned:
        return tr(T::SeamAligned);
    case SeamMode::Nearest:
        return tr(T::SeamNearest);
    case SeamMode::Random:
        return tr(T::SeamRandom);
    case SeamMode::Rear:
        return tr(T::SeamRear);
    }
    return "";
}

const char* pattern_title(InfillPattern pattern) {
    switch (pattern) {
    case InfillPattern::Rectilinear:
        return tr(T::Rectilinear);
    case InfillPattern::AlignedRectilinear:
        return tr(T::AlignedRectilinear);
    case InfillPattern::Zigzag:
        return tr(T::Zigzag);
    case InfillPattern::Line:
        return tr(T::LineInfill);
    case InfillPattern::Grid:
        return tr(T::Grid);
    case InfillPattern::Gyroid:
        return tr(T::Gyroid);
    case InfillPattern::Triangles:
        return tr(T::TrianglesInfill);
    case InfillPattern::Stars:
        return tr(T::Stars);
    case InfillPattern::Cubic:
        return tr(T::Cubic);
    case InfillPattern::AdaptiveCubic:
        return tr(T::AdaptiveCubic);
    case InfillPattern::SupportCubic:
        return tr(T::SupportCubic);
    case InfillPattern::Lightning:
        return tr(T::Lightning);
    case InfillPattern::Honeycomb:
        return tr(T::Honeycomb);
    case InfillPattern::Concentric:
        return tr(T::ConcentricInfill);
    case InfillPattern::Hilbert:
        return tr(T::HilbertInfill);
    case InfillPattern::Archimedean:
        return tr(T::ArchimedeanInfill);
    case InfillPattern::Octagram:
        return tr(T::OctagramInfill);
    }
    return "";
}

// A temperature from the printer's variables, or nothing.
std::optional<double> variable(const Settings& s, const char* name) {
    const auto it = s.variables.find(name);
    if (it == s.variables.end() || it->second.empty())
        return std::nullopt;
    char* end = nullptr;
    const double v = std::strtod(it->second.front().c_str(), &end);
    if (end == it->second.front().c_str())
        return std::nullopt;
    return v;
}

std::string speed_text(double v) { return v > 0.0 ? number(v, 0) : std::string("—"); }

} // namespace

const std::vector<SectionLayout>& settings_layout(SettingsTab tab) {
    switch (tab) {
    case SettingsTab::Print:
        return print_layout();
    case SettingsTab::Material:
        return material_layout();
    case SettingsTab::Printer:
        return printer_layout();
    }
    return print_layout();
}

std::optional<SettingPlace> setting_place(std::string_view key) {
    for (const SettingsTab tab : {SettingsTab::Print, SettingsTab::Material, SettingsTab::Printer})
        for (const SectionLayout& section : settings_layout(tab))
            for (const SubsectionLayout& sub : section.subsections)
                for (const SettingEntry& e : sub.settings)
                    if (e.key == key)
                        return SettingPlace{tab, &section, &sub, e.level};
    // The printer's other variables go with machine.variables.
    if (key.starts_with("machine.variables."))
        return setting_place("machine.variables");
    return std::nullopt;
}

bool level_shows(ViewLevel view, SettingLevel level) {
    switch (view) {
    case ViewLevel::Basic:
        return level == SettingLevel::Basic;
    case ViewLevel::Normal:
        return level != SettingLevel::Advanced;
    case ViewLevel::Advanced:
    case ViewLevel::Custom:
        return true;
    }
    return true;
}

std::string subsection_summary(std::string_view id, const Settings& s) {
    // Print
    if (id == "layer_height")
        return format(T::SumLayer, number(s.layer_height).c_str(), number(s.first_layer_height).c_str());
    if (id == "variable_height") {
        if (!s.layer_heights.empty())
            return format(T::SumHeightSteps, int(s.layer_heights.size()));
        return s.cusp_height > 0.0 ? tr(T::SumAdaptive) : tr(T::SumSameHeight);
    }
    if (id == "fine_detail")
        return s.detail_height > 0.0 ? format(T::SumMm, number(s.detail_height).c_str()) : tr(T::SumOff);
    if (id == "line_width")
        return format(T::SumMm, number(s.wall_width).c_str());
    if (id == "seam")
        return s.scarf != ScarfMode::Nowhere ? format(T::SumScarf, seam_title(s.seam)) : seam_title(s.seam);
    if (id == "precision")
        return s.xy_offset == 0.0 && s.first_layer_shrink == 0.0 && s.scale_x == 100.0 && s.scale_y == 100.0 &&
                       s.scale_z == 100.0 && s.hole_offset == 0.0
                   ? tr(T::SumNoCorrections)
                   : tr(T::SumCorrections);
    if (id == "slopes")
        return s.zaa ? format(T::SumZaaOn, number(s.zaa_min_thickness, 0).c_str()) : tr(T::SumZaaOff);
    if (id == "walls")
        return s.wall_count == 0 ? tr(T::SumNoWalls) : s.wall_count == 1 ? tr(T::SumOneWall) : format(T::SumWalls, s.wall_count);
    if (id == "top_bottom")
        return format(T::SumTopBottom, s.top_layers, s.bottom_layers);
    if (id == "infill")
        return format(T::SumInfill, number(s.infill_density, 1).c_str(), pattern_title(s.infill_pattern));
    if (id == "infill_more") {
        const Settings d;
        return s.infill_wall_overlap == d.infill_wall_overlap && s.infill_solid_every == d.infill_solid_every &&
                       s.infill_solid_below_area == d.infill_solid_below_area && !s.infill_dense_under_top &&
                       s.infill_stress_density == 0.0
                   ? tr(T::SumNoChanges)
                   : tr(T::SumCorrections);
    }
    if (id == "combine")
        return s.infill_combine_auto        ? tr(T::SumCombineAuto)
               : s.infill_combine_layers > 1 ? format(T::SumCombine, s.infill_combine_layers)
                                             : tr(T::SumNotCombined);
    if (id == "wavy")
        return s.infill_nonplanar ? format(T::SumMm, number(s.infill_nonplanar_wavelength).c_str()) : tr(T::SumOff);
    if (id == "first_layer")
        return format(T::SumSpeed, speed_text(s.first_layer_speed).c_str());
    if (id == "walls_surfaces")
        return format(T::SumWallSpeed, speed_text(s.wall_speed).c_str(),
                      s.outer_wall_speed.percent ? (number(s.outer_wall_speed.value, 0) + " %").c_str()
                                                 : speed_text(s.outer_wall_speed.value).c_str());
    if (id == "infill_support")
        return format(T::SumTwoSpeeds, speed_text(s.infill_speed).c_str(), speed_text(s.support_speed).c_str());
    if (id == "travel")
        return format(T::SumSpeed, speed_text(s.travel_speed).c_str());
    if (id == "limits")
        return format(T::SumSpeed, speed_text(s.max_speed).c_str());
    if (id == "support")
        return !s.support                ? tr(T::SumNoSupport)
               : !s.support_automatic    ? tr(T::SumPaintedSupport)
                                         : format(T::SumAutoSupport, number(s.support_angle, 0).c_str());
    if (id == "gap")
        return format(T::SumGaps, number(s.support_z_gap).c_str(), number(s.support_xy_gap).c_str());
    if (id == "interface")
        return format(T::SumLayersN, s.support_interface_layers);
    if (id == "pattern")
        return format(T::SumMm, number(s.support_spacing).c_str());
    if (id == "skirt")
        return s.skirt_loops > 0 ? format(T::SumSkirt, s.skirt_loops, number(s.skirt_distance).c_str()) : tr(T::SumNoSkirt);
    if (id == "brim")
        return s.brim_width > 0.0 ? format(T::SumMm, number(s.brim_width).c_str()) : tr(T::SumNoBrim);
    if (id == "special_modes")
        return s.vase ? tr(T::VaseMode) : tr(T::SumNormalPrint);
    if (id == "fuzzy")
        return s.fuzzy_mode == FuzzyMode::None       ? tr(T::FuzzyNone)
               : s.fuzzy_mode == FuzzyMode::External ? tr(T::FuzzyExternal)
                                                     : tr(T::FuzzyAll);
    if (id == "gcode_output")
        return format(T::SumResolution, number(s.gcode_resolution, 4).c_str());
    if (id == "slicing")
        return s.fill_mode == FillMode::Union ? "union" : s.fill_mode == FillMode::EvenOdd ? "even-odd" : "fill holes";
    // Material
    if (id == "nozzle_temp")
        return variable(s, "temperature") ? format(T::SumTemp, int(std::lround(*variable(s, "temperature")))) : "—";
    if (id == "bed_temp")
        return variable(s, "bed_temperature") ? format(T::SumTemp, int(std::lround(*variable(s, "bed_temperature")))) : "—";
    if (id == "chamber")
        return s.chamber_temperature > 0.0 ? format(T::SumTemp, int(std::lround(s.chamber_temperature))) : tr(T::SumNoChamber);
    if (id == "fan")
        return format(T::SumFan, number(s.fan_min, 0).c_str(), number(s.fan_max, 0).c_str());
    if (id == "short_layers")
        return format(T::SumMinLayer, number(s.min_layer_time, 1).c_str());
    if (id == "flow")
        return format(T::SumPercent, number(s.flow, 1).c_str());
    if (id == "pressure")
        return s.pressure_advance > 0.0 ? format(T::SumPa, number(s.pressure_advance, 3).c_str()) : tr(T::SumNoPa);
    if (id == "max_flow")
        return s.filament_max_flow > 0.0 ? format(T::SumCap, number(s.filament_max_flow, 1).c_str()) : tr(T::SumNoCap);
    if (id == "retraction")
        return s.retract_length > 0.0
                   ? format(T::SumRetract, number(s.retract_length).c_str(), speed_text(s.retract_speed).c_str())
                   : tr(T::SumNoRetract);
    if (id == "retract_travel")
        return s.retract_lift > 0.0 ? format(T::SumHop, number(s.retract_lift).c_str()) : tr(T::SumNoHop);
    if (id == "needs")
        return s.abrasive || s.needs_enclosure ? tr(T::SumNeeds) : tr(T::SumNoNeeds);
    // Printer
    if (id == "bed_volume") {
        double x0 = 0, x1 = 0, y0 = 0, y1 = 0;
        for (std::size_t k = 0; k < s.bed.size(); ++k) {
            const auto& p = s.bed[k];
            x0 = k ? std::min(x0, p[0]) : p[0], x1 = k ? std::max(x1, p[0]) : p[0];
            y0 = k ? std::min(y0, p[1]) : p[1], y1 = k ? std::max(y1, p[1]) : p[1];
        }
        return format(T::SumBed, number(x1 - x0, 0).c_str(), number(y1 - y0, 0).c_str(), number(s.max_height, 0).c_str());
    }
    if (id == "enclosure")
        return s.enclosed ? tr(T::SumClosed) : tr(T::SumOpen);
    if (id == "nozzle_size")
        return s.hardened_nozzle ? format(T::SumHardened, number(s.nozzle_diameter).c_str())
                                 : format(T::SumMm, number(s.nozzle_diameter).c_str());
    if (id == "extruder")
        return format(T::SumMm, number(s.filament_diameter).c_str());
    if (id == "speeds")
        return format(T::SumFeeds, speed_text(std::min(s.max_feed_x, s.max_feed_y)).c_str());
    if (id == "accelerations")
        return format(T::SumAccel, number(s.acceleration, 0).c_str());
    if (id == "max_temps")
        return format(T::SumMaxTemps, s.max_nozzle_temp > 0.0 ? number(s.max_nozzle_temp, 0).c_str() : "—",
                      s.max_bed_temp > 0.0 ? number(s.max_bed_temp, 0).c_str() : "—");
    if (id == "firmware")
        return s.flavor == GcodeFlavor::Marlin ? "Marlin" : s.flavor == GcodeFlavor::Klipper ? "Klipper" : "RepRap";
    if (id == "start_end") {
        const auto lines = [](const std::string& text) {
            int n = text.empty() ? 0 : 1;
            for (const char c : text)
                n += c == '\n';
            return text.ends_with('\n') ? n - 1 : n;
        };
        return format(T::SumLines, lines(s.start_gcode), lines(s.end_gcode));
    }
    if (id == "variables")
        return format(T::SumVariables, int(s.variables.size()));
    return {};
}

} // namespace tisma::ui
