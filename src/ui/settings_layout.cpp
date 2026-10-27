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
          {"slopes", T::SubSlopes, {{"zaa.enabled", N}, {"zaa.min_thickness", A}, {"zaa.raise", A}}},
          {"ironing", T::SubIroning,
           {{"ironing.enabled", N},
            {"ironing.surfaces", A},
            {"ironing.flow", A},
            {"ironing.spacing", A},
            {"ironing.speed", A}}}}},
        {"strength", T::SecStrength, "section_strength.svg",
         {{"walls", T::SubWalls,
           {{"walls.count", B},
            {"walls.detect_overhangs", A},
            {"walls.min_line_width", A},
            {"walls.min_detail", A},
            {"walls.generator", A},
            {"walls.gap_fill", A},
            {"walls.clockwise", A}}},
          {"walls_more", T::SubWallsMore,
           {{"walls.staggered_inner_seams", A},
            {"walls.top_one_wall", A},
            {"walls.first_layer_one_wall", A},
            {"walls.extra_if_needed", A},
            {"walls.extra_on_overhangs", A},
            {"walls.avoid_curled_overhangs", A}}},
          {"top_bottom", T::SubTopBottom,
           {{"surfaces.top_layers", B},
            {"surfaces.bottom_layers", B},
            {"surfaces.top_min_thickness", B},
            {"surfaces.bottom_min_thickness", B},
            {"surfaces.top_pattern", B},
            {"surfaces.interface_shells", A}}},
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
         {{"first_layer", T::SubFirstLayerSpeed,
           {{"speed.first_layer", N}, {"speed.first_layer_infill", A}, {"speed.first_layer_over_raft", A}}},
          {"walls_surfaces", T::SubWallSpeed,
           {{"speed.walls", N},
            {"speed.outer_wall", N},
            {"speed.solid_infill", N},
            {"speed.top_solid_infill", N},
            {"speed.bridges", N},
            {"speed.over_bridges", A},
            {"speed.gap_fill", A}}},
          {"infill_support", T::SubInfillSpeed, {{"speed.infill", N}, {"speed.support", N}}},
          {"travel", T::SubTravelSpeed,
           {{"speed.travel", N}, {"speed.travel_z", A}, {"travel.avoid_crossing_walls", N}, {"travel.max_detour", A}}},
          {"limits", T::SubSpeedCap, {{"speed.max", N}, {"speed.flow_slope_up", A}, {"speed.flow_slope_down", A}}},
          {"overhang_speeds", T::SubOverhangSpeeds,
           {{"speed.dynamic_overhangs", N},
            {"speed.overhang_0", A},
            {"speed.overhang_1", A},
            {"speed.overhang_2", A},
            {"speed.overhang_3", A}}},
          {"accel_kinds", T::SubAccelerations,
           {{"acceleration.default", A},
            {"acceleration.walls", A},
            {"acceleration.outer_wall", A},
            {"acceleration.infill", A},
            {"acceleration.solid_infill", A},
            {"acceleration.top_solid_infill", A},
            {"acceleration.bridges", A}}},
          {"accel_travel", T::SubAccelTravel,
           {{"acceleration.first_layer", A},
            {"acceleration.first_layer_over_raft", A},
            {"acceleration.travel", A},
            {"acceleration.short_travel", A}}}}},
        {"support", T::SecSupport, "section_support.svg",
         {{"support", T::SubSupport,
           {{"support.enabled", B}, {"support.automatic", B}, {"support.style", N}, {"support.threshold_angle", N}}},
          {"tree", T::SubSupportTree,
           {{"support.tree_branch_diameter", N},
            {"support.tree_tip_diameter", N},
            {"support.tree_top_rate", N},
            {"support.tree_branch_distance", A},
            {"support.tree_angle_preferred", A},
            {"support.tree_angle", A},
            {"support.tree_branch_diameter_angle", A},
            {"support.tree_double_wall_diameter", A}}},
          {"gap", T::SubSupportGap, {{"support.z_gap", N}, {"support.xy_gap", N}}},
          {"interface", T::SubSupportInterface, {{"support.interface_layers", N}, {"support.synchronize_layers", A}}},
          {"pattern", T::SubSupportPattern, {{"support.spacing", N}, {"support.line_angle", A}, {"support.width", A}}},
          {"arcs", T::SubArcs,
           {{"arcs.enabled", N}, {"arcs.overlap", A}, {"arcs.speed", A}, {"arcs.max_distance", A}}}}},
        {"adhesion", T::SecAdhesion, "section_adhesion.svg",
         {{"skirt", T::SubSkirt,
           {{"adhesion.skirt_loops", B},
            {"adhesion.skirt_distance", B},
            {"adhesion.skirt_layers", N},
            {"adhesion.skirt_min_filament", A},
            {"adhesion.draft_shield", N}}},
          {"brim", T::SubBrim, {{"adhesion.brim_width", B}, {"adhesion.brim_type", N}, {"adhesion.brim_gap", A}}},
          {"raft", T::SubRaft,
           {{"adhesion.raft_layers", N},
            {"adhesion.raft_gap", A},
            {"adhesion.raft_expansion", A},
            {"adhesion.raft_first_layer_density", A},
            {"adhesion.raft_first_layer_expansion", A}}}}},
        {"other", T::SecOther, "section_other.svg",
         {{"special_modes", T::SubSpecialModes, {{"vase.enabled", N}, {"sequential.enabled", N}}},
          {"multi_material", T::SubMultiMaterial,
           {{"extruders.walls", N},
            {"extruders.infill", N},
            {"extruders.solid_infill", N},
            {"extruders.support", N},
            {"extruders.support_interface", N},
            {"extruders.bed_temperature_from", A},
            {"extruders.ooze_prevention", A},
            {"extruders.standby_delta", A}}},
          {"multi_nozzle", T::SubMultiNozzle, {{"multi.fine_walls_coarse_infill", N}, {"multi.color_mixing", N}}},
          {"resin_layers", T::SubResinLayers, {{"resin.layer_height", N}}},
          {"resin_supports", T::SubResinSupports,
           {{"resin.supports", N},
            {"resin.support_elevation", N},
            {"resin.support_density", N},
            {"resin.support_tip_diameter", A},
            {"resin.support_tip_penetration", A},
            {"resin.support_pillar_diameter", A},
            {"resin.support_critical_angle", A}}},
          {"resin_pad", T::SubResinPad,
           {{"resin.pad", N},
            {"resin.pad_around_object", N},
            {"resin.pad_thickness", A},
            {"resin.pad_margin", A},
            {"resin.pad_wall_slope", A}}},
          {"resin_hollow", T::SubResinHollow,
           {{"resin.hollow", N},
            {"resin.hollow_wall", N},
            {"resin.hollow_closing", A},
            {"resin.hollow_infill", A},
            {"resin.hollow_infill_cell", A}}},
          {"curved", T::SubCurved,
           {{"curved.mode", N},
            {"curved.pattern", N},
            {"curved.fit_head", N},
            {"curved.amplitude", A},
            {"curved.wavelength", A},
            {"curved.angle", A},
            {"curved.twist", A},
            {"curved.cone_angle", A}}},
          {"curved_more", T::SubCurvedMore,
           {{"curved.flat_below", A},
            {"curved.ramp_height", A},
            {"curved.flat_top", A},
            {"curved.segment_length", A},
            {"curved.max_slope", A},
            {"curved.flow_policy", A},
            {"curved.uniform_flow", A},
            {"curved.overhang_threshold", A}}},
          {"interlocking", T::SubInterlocking,
           {{"interlocking.beams", N},
            {"interlocking.beam_width", A},
            {"interlocking.beam_layers", A},
            {"interlocking.orientation", A},
            {"interlocking.depth", A},
            {"interlocking.boundary_avoidance", A},
            {"painting.max_region_width", A},
            {"painting.interlocking_depth", A}}},
          {"wipe_tower", T::SubWipeTower,
           {{"wipe_tower.enabled", N},
            {"wipe_tower.width", N},
            {"wipe_tower.brim_width", N},
            {"wipe_tower.x", A},
            {"wipe_tower.y", A},
            {"wipe_tower.rotation", A},
            {"wipe_tower.bridging", A},
            {"extruders.wipe_tower", A}}},
          {"wipe_tower_more", T::SubWipeTowerMore,
           {{"wipe_tower.purge_volumes", A},
            {"wipe_tower.extra_spacing", A},
            {"wipe_tower.extra_flow", A},
            {"wipe_tower.cone_angle", A},
            {"wipe_tower.no_sparse_layers", A}}},
          {"fuzzy", T::SubFuzzy, {{"fuzzy.mode", N}, {"fuzzy.thickness", N}, {"fuzzy.point_distance", N}}},
          {"gcode_output", T::SubGcodeOut,
           {{"gcode.resolution", A}, {"changes", A}, {"gcode.volumetric_e", A}, {"gcode.verbose_comments", A},
            {"gcode.arcs", A}, {"gcode.binary", A}}},
          {"output_file", T::SubOutputFile, {{"output.filename_format", A}, {"output.post_process", A}}},
          {"gcode_templates", T::SubGcodeTemplates,
           {{"gcode.auto_temperatures", A},
            {"extrusion.start_gcode", A},
            {"extrusion.end_gcode", A},
            {"gcode.color_change", A},
            {"gcode.pause", A},
            {"gcode.before_layer", A},
            {"gcode.after_layer", A},
            {"gcode.between_objects", A}}},
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
          {"chamber", T::SubChamber, {{"extrusion.chamber_temperature", N}, {"extrusion.chamber_min_temperature", A}}},
          {"price", T::SubPrice, {{"extrusion.density", N}, {"extrusion.cost", N}, {"extrusion.spool_weight", A}}},
          {"shrinkage", T::SubShrinkage, {{"extrusion.shrinkage_xy", A}, {"extrusion.shrinkage_z", A}}}}},
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
         {{"flow", T::SubFlow, {{"extrusion.flow", N}, {"extrusion.bridge_flow_ratio", A}, {"extrusion.purge_multiplier", A},
                    {"extrusion.min_purge_on_tower", A}}},
          {"pressure", T::SubPressure, {{"extrusion.pressure_advance", A}}},
          {"max_flow", T::SubMaxFlow,
           {{"extrusion.max_flow", N}, {"extrusion.infill_max_speed", A}, {"extrusion.infill_max_crossing_speed", A}}}}},
        {"retraction", T::SecRetraction, "section_retraction.svg",
         {{"retraction", T::SubRetract,
           {{"retraction.length", B},
            {"retraction.speed", N},
            {"retraction.unretract_speed", A},
            {"retraction.restart_extra", A},
            {"retraction.on_layer_change", N},
            {"retraction.wipe", N},
            {"retraction.before_wipe", A},
            {"retraction.firmware", A}}},
          {"retract_travel", T::SubRetractTravel,
           {{"retraction.lift", B},
            {"retraction.min_travel", N},
            {"retraction.lift_above", A},
            {"retraction.lift_below", A},
            {"travel.ramping_lift", A},
            {"travel.slope", A},
            {"travel.max_lift", A},
            {"travel.lift_before_obstacle", A}}},
          {"material_retraction", T::SubMaterialRetraction, {{"extrusion.retraction", A}}},
          {"mmu_load", T::SubMmuLoad,
           {{"extrusion.loading_speed", A},
            {"extrusion.loading_speed_start", A},
            {"extrusion.unloading_speed", A},
            {"extrusion.unloading_speed_start", A},
            {"extrusion.cooling_moves", A},
            {"extrusion.cooling_initial_speed", A},
            {"extrusion.cooling_final_speed", A},
            {"extrusion.ramming", A}}},
          {"mmu_times", T::SubMmuTimes,
           {{"extrusion.toolchange_delay", A}, {"extrusion.load_time", A}, {"extrusion.unload_time", A}}}}},
        // Resin (resin.md): the resin's exposures, moves and corrections.
        {"resin", T::SecResin, "section_flow.svg",
         {{"resin_exposure", T::SubResinExposure,
           {{"resin.exposure", N},
            {"resin.bottom_exposure", N},
            {"resin.bottom_layers", N},
            {"resin.light_pwm", A},
            {"resin.bottom_light_pwm", A}}},
          {"resin_moves", T::SubResinMoves,
           {{"resin.lift_distance", N},
            {"resin.lift_speed", N},
            {"resin.bottom_lift_distance", A},
            {"resin.bottom_lift_speed", A},
            {"resin.retract_speed", N},
            {"resin.wait_before_exposure", N},
            {"resin.wait_after_cure", A},
            {"resin.wait_after_lift", A}}},
          {"resin_corrections", T::SubResinCorrections,
           {{"resin.elephant_foot", N},
            {"resin.scale_x", A},
            {"resin.scale_y", A},
            {"resin.scale_z", A},
            {"resin.absolute_correction", A},
            {"resin.z_correction_layers", A},
            {"resin.gamma", A},
            {"resin.antialiasing", A}}},
          {"resin_material", T::SubResinMaterial,
           {{"resin.density", N},
            {"resin.bottle_cost", N},
            {"resin.bottle_volume", N},
            {"resin.bottle_weight", A},
            {"resin.profile_name", A}}}}},
        {"needs", T::SecNeeds, "section_needs.svg",
         {{"needs", T::SubNeeds, {{"extrusion.abrasive", N}, {"extrusion.needs_enclosure", N}}}}},
    };
    return layout;
}

const std::vector<SectionLayout>& printer_layout() {
    static const std::vector<SectionLayout> layout = {
        {"machine", T::SecMachine, "section_machine.svg",
         {{"bed_volume", T::SubBedVolume,
           {{"machine.technology", N},
            {"machine.bed", B},
            {"machine.max_height", B},
            {"machine.z_offset", A},
            {"sequential.clearance_radius", A},
            {"sequential.clearance_height", A}}},
          {"enclosure", T::SubEnclosure, {{"machine.enclosed", N}, {"machine.max_chamber_temp", N}}},
          {"head", T::SubHead,
           {{"head.free_angle_x", N},
            {"head.free_angle_y", N},
            {"head.clearance_height", N},
            {"head.clearance_radius", N},
            {"head.profile", A}}},
          {"resin_display", T::SubResinDisplay,
           {{"machine.display_pixels_x", N},
            {"machine.display_pixels_y", N},
            {"machine.display_width", N},
            {"machine.display_height", N},
            {"machine.display_mirror_x", N},
            {"machine.display_mirror_y", N},
            {"resin.printer_name", A}}},
          {"stability", T::SubStability,
           {{"review.nozzle_force", A}, {"review.bed_adhesion", A}, {"review.max_deflection", A}}}}},
        {"nozzle", T::SecNozzle, "section_nozzle.svg",
         {{"nozzle_size", T::SubNozzle,
           {{"machine.nozzle_diameter", B},
            {"machine.hardened_nozzle", N},
            {"machine.min_layer_height", N},
            {"machine.max_layer_height", N}}},
          {"extruder", T::SubExtruder,
           {{"machine.filament_diameter", B}, {"machine.max_flow", N}, {"machine.relative_extrusion", N}}},
          {"extruders", T::SubExtruders,
           {{"machine.extruders", N},
            {"machine.extruder_offset", A},
            {"retraction.toolchange_length", A},
            {"retraction.toolchange_restart_extra", A},
            {"gcode.toolchange", A},
            {"machine.single_extruder_mmu", A}}},
          {"mmu", T::SubMmu,
           {{"mmu.cooling_tube_length", A},
            {"mmu.cooling_tube_position", A},
            {"mmu.parking_position", A},
            {"mmu.extra_loading_move", A},
            {"mmu.high_current_on_swap", A}}}}},
        {"limits", T::SecLimits, "section_limits.svg",
         {{"speeds", T::SubSpeedLimits,
           {{"machine.max_feed_x", N},
            {"machine.max_feed_y", N},
            {"machine.max_feed_z", N},
            {"machine.max_feed_e", N},
            {"machine.corner_speed", N}}},
          {"accelerations", T::SubAccel,
           {{"machine.acceleration", N},
            {"machine.travel_acceleration", N},
            {"machine.retract_acceleration", A},
            {"machine.max_accel_x", A},
            {"machine.max_accel_y", A},
            {"machine.max_accel_z", A},
            {"machine.max_accel_e", A}}},
          {"jerk", T::SubJerk,
           {{"machine.jerk_x", A},
            {"machine.jerk_y", A},
            {"machine.jerk_z", A},
            {"machine.jerk_e", A},
            {"machine.junction_deviation", A},
            {"machine.min_extrude_feed", A},
            {"machine.min_travel_feed", A}}},
          {"silent", T::SubSilent, {{"machine.silent_mode", A}, {"machine.silent", A}}},
          {"max_temps", T::SubMaxTemps, {{"machine.max_nozzle_temp", N}, {"machine.max_bed_temp", N}}}}},
        {"gcode", T::SecGcode, "section_gcode.svg",
         {{"firmware", T::SubFirmware,
           {{"machine.flavor", N}, {"machine.limits_usage", N}, {"gcode.progress", N}, {"gcode.object_labels", N}}},
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
    if (id == "walls_more")
        return format(T::SumWallsMore, int(s.staggered_inner_seams) + int(s.top_one_wall != TopOneWall::None) +
                                           int(s.first_layer_one_wall) + int(s.extra_if_needed) +
                                           int(s.extra_on_overhangs) + int(s.avoid_curled_overhangs));
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
    if (id == "ironing")
        return s.ironing ? format(T::SumIroning, number(s.ironing_speed, 0).c_str()) : tr(T::SumOff);
    if (id == "overhang_speeds")
        return s.dynamic_overhangs ? tr(T::SumBySupport) : tr(T::SumOff);
    if (id == "accel_kinds")
        return s.accel_default > 0.0 ? format(T::SumAccel, number(s.accel_default, 0).c_str()) : tr(T::SumOff);
    if (id == "accel_travel")
        return s.accel_travel > 0.0 ? format(T::SumAccel, number(s.accel_travel, 0).c_str()) : tr(T::SumOff);
    if (id == "support")
        return !s.support                ? tr(T::SumNoSupport)
               : !s.support_automatic    ? tr(T::SumPaintedSupport)
                                         : format(T::SumAutoSupport, number(s.support_angle, 0).c_str());
    if (id == "tree")
        return format(T::SumTree, number(s.tree_branch_diameter).c_str(), number(s.tree_tip_diameter).c_str());
    if (id == "gap")
        return format(T::SumGaps, number(s.support_z_gap).c_str(),
                      s.support_xy_gap.percent ? (number(s.support_xy_gap.value, 0) + " %").c_str()
                                               : (number(s.support_xy_gap.value) + " mm").c_str());
    if (id == "interface")
        return format(T::SumLayersN, s.support_interface_layers);
    if (id == "pattern")
        return format(T::SumMm, number(s.support_spacing).c_str());
    if (id == "arcs")
        return s.arcs ? format(T::SumArcs, number(s.arcs_speed, 0).c_str()) : tr(T::SumNoArcs);
    if (id == "skirt")
        return s.skirt_loops > 0 ? format(T::SumSkirt, s.skirt_loops, number(s.skirt_distance).c_str()) : tr(T::SumNoSkirt);
    if (id == "gcode_templates")
        return s.gcode_color_change.substr(0, s.gcode_color_change.find('\n')) + " / " +
               s.gcode_pause.substr(0, s.gcode_pause.find('\n'));
    if (id == "raft")
        return s.raft_layers > 0 ? format(T::SumRaft, s.raft_layers) : tr(T::SumOff);
    if (id == "brim")
        return s.brim_width > 0.0 ? format(T::SumMm, number(s.brim_width).c_str()) : tr(T::SumNoBrim);
    if (id == "special_modes")
        return s.vase && s.sequential ? std::string(tr(T::VaseMode)) + " · " + tr(T::SequentialRow)
               : s.vase               ? std::string(tr(T::VaseMode))
               : s.sequential         ? std::string(tr(T::SequentialRow))
                                      : std::string(tr(T::SumNormalPrint));
    if (id == "multi_material")
        return s.extruder_walls == 0 && s.extruder_infill == 0 && s.extruder_solid_infill == 0 &&
                       s.extruder_support == 0 && s.extruder_support_interface == 0
                   ? tr(T::SumObjectExtruder)
                   : tr(T::SumByKind);
    if (id == "multi_nozzle")
        return s.fine_walls_coarse_infill ? tr(T::FineWallsCoarseInfill)
               : color_mixing_offered(s)  ? tr(T::ColorMixingRow)
                                          : tr(T::SumOff);
    if (id == "extruders")
        return s.extruders > 1 ? format(T::SumExtrudersN, s.extruders) : tr(T::SumOneExtruder);
    if (id == "interlocking")
        return s.interlock_beams ? format(T::SumMm, number(s.interlock_width).c_str()) : tr(T::SumOff);
    if (id == "mmu")
        return s.single_extruder_mmu ? format(T::SumMm, number(s.mmu_parking_position).c_str()) : tr(T::SumOff);
    if (id == "mmu_load")
        return number(s.loading_speed, 0) + " / " + number(s.unloading_speed, 0) + " mm/s";
    if (id == "mmu_times")
        return number(s.toolchange_delay + s.load_time + s.unload_time, 1) + " s";
    if (id == "wipe_tower")
        return s.wipe_tower ? format(T::SumMm, number(s.wipe_tower_width).c_str()) : tr(T::SumOff);
    if (id == "wipe_tower_more")
        return number(s.wipe_tower_extra_spacing, 0) + " % · " + number(s.wipe_tower_extra_flow, 0) + " %";
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
    if (id == "price")
        return s.filament_density > 0.0 ? format(T::SumDensity, number(s.filament_density, 2).c_str()) : "—";
    if (id == "shrinkage")
        return s.shrinkage_xy == 0.0 && s.shrinkage_z == 0.0
                   ? tr(T::SumNoShrinkage)
                   : format(T::SumShrinkage, number(s.shrinkage_xy, 2).c_str(), number(s.shrinkage_z, 2).c_str());
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
    if (id == "material_retraction")
        return s.filament_retraction.empty() ? tr(T::SumPrinterValues)
                                             : format(T::SumMaterialValues, int(s.filament_retraction.size()));
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
    if (id == "curved")
        return s.curved_mode == CurvedMode::Disabled ? tr(T::SumOff)
               : s.curved_mode == CurvedMode::Wave   ? tr(T::CurvedWave)
                                                     : tr(T::CurvedCone);
    if (id == "curved_more")
        return format(T::SumMm, number(s.curved_ramp_height).c_str());
    if (id == "head")
        return format(T::SumMm, number(s.head_clearance_height).c_str());
    if (id == "resin_layers")
        return format(T::SumMm, number(s.resin_layer_height, 3).c_str());
    if (id == "resin_supports")
        return s.resin_supports ? format(T::SumMm, number(s.resin_support_elevation).c_str()) : tr(T::SumOff);
    if (id == "resin_pad")
        return s.resin_pad ? format(T::SumMm, number(s.resin_pad_thickness).c_str()) : tr(T::SumOff);
    if (id == "resin_hollow")
        return s.resin_hollow ? format(T::SumMm, number(s.resin_hollow_wall).c_str()) : tr(T::SumOff);
    if (id == "resin_display")
        return format(T::SumPixels, s.display_pixels_x, s.display_pixels_y);
    if (id == "resin_exposure")
        return format(T::SumResinExposure, number(s.resin_exposure).c_str(), number(s.resin_bottom_exposure).c_str());
    if (id == "resin_moves")
        return format(T::SumLift, number(s.resin_lift_distance).c_str(), number(s.resin_lift_speed).c_str());
    if (id == "resin_corrections")
        return format(T::SumMm, number(s.resin_elephant_foot).c_str());
    if (id == "resin_material")
        return format(T::SumResinDensity, number(s.resin_density).c_str());
    if (id == "stability")
        return s.review_nozzle_force > 0.0 ? format(T::SumNozzleForce, number(s.review_nozzle_force, 3).c_str())
                                           : tr(T::SumNotMeasured);
    if (id == "nozzle_size") {
        // One per extruder: "different nozzles" while they differ (multimaterial.md, rule 1).
        for (int e = 2; e <= s.extruders; ++e)
            if (nozzle_of(s, e) != nozzle_of(s, 1))
                return tr(T::DifferentNozzles);
        return s.hardened_nozzle ? format(T::SumHardened, number(s.nozzle_diameter).c_str())
                                 : format(T::SumMm, number(s.nozzle_diameter).c_str());
    }
    if (id == "extruder")
        return format(T::SumMm, number(s.filament_diameter).c_str());
    if (id == "speeds")
        return format(T::SumFeeds, speed_text(std::min(s.max_feed_x, s.max_feed_y)).c_str());
    if (id == "accelerations")
        return format(T::SumAccel, number(s.acceleration, 0).c_str());
    if (id == "max_temps")
        return format(T::SumMaxTemps, s.max_nozzle_temp > 0.0 ? number(s.max_nozzle_temp, 0).c_str() : "—",
                      s.max_bed_temp > 0.0 ? number(s.max_bed_temp, 0).c_str() : "—");
    if (id == "firmware") {
        switch (s.flavor) {
        case GcodeFlavor::Marlin:
            return "Marlin";
        case GcodeFlavor::Klipper:
            return "Klipper";
        case GcodeFlavor::RepRap:
            return "RepRap";
        case GcodeFlavor::MarlinLegacy:
            return "Marlin 1";
        case GcodeFlavor::RepRapFirmware:
            return "RepRapFirmware";
        case GcodeFlavor::Repetier:
            return "Repetier";
        case GcodeFlavor::Smoothie:
            return "Smoothieware";
        case GcodeFlavor::Teacup:
            return "Teacup";
        case GcodeFlavor::MakerWare:
            return "MakerWare";
        case GcodeFlavor::Sailfish:
            return "Sailfish";
        case GcodeFlavor::Mach3:
            return "Mach3";
        case GcodeFlavor::Machinekit:
            return "Machinekit";
        case GcodeFlavor::NoExtrusion:
            return tr(T::FlavorNoExtrusion);
        }
        return "RepRap";
    }
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
    if (id == "output_file")
        return s.filename_format;
    if (id == "jerk")
        return s.junction_deviation > 0.0 ? format(T::SumJunction, number(s.junction_deviation, 3).c_str())
               : s.jerk_x > 0.0          ? format(T::SumJerk, number(s.jerk_x, 1).c_str())
                                         : std::string("—");
    if (id == "silent")
        return s.silent_mode ? format(T::SumSilent, int(s.silent_limits.size())) : tr(T::SumOff);
    return {};
}

} // namespace tisma::ui
