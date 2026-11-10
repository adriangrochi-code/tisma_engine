// The adhesion between layers and between lines (fea/adhesion.hpp; the
// owner's request, 2026-11-03): the factors against the numbers worked out
// by hand from docs/research/interlayer_adhesion.md §5 and
// docs/research/bead_adhesion.md §5.2.

#include "fea/adhesion.hpp"
#include "fea/verdict.hpp"
#include "project/engineering_case.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

using namespace tisma;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

PrintConditions at(double nozzle, double layer = 0.2, double width = 0.42) {
    PrintConditions p;
    p.nozzle_temperature = nozzle;
    p.layer_height = layer;
    p.line_width = width;
    return p;
}

} // namespace

TEST_CASE("adhesion: at the sources' settings nothing changes", "[adhesion]") {
    const MaterialFamily* pla = family_of("PLA");
    REQUIRE(pla);
    const Adhesion a = adhesion(pla, {});
    CHECK(a.modelled);
    CHECK_THAT(a.reference_temperature, WithinAbs(205.0, 1e-12)); // the table's 190 to 220 °C
    CHECK_THAT(a.temperature, WithinAbs(205.0, 1e-12));
    CHECK_THAT(a.z_strength, WithinAbs(1.0, 1e-12));
    CHECK_THAT(a.z_stiffness, WithinAbs(1.0, 1e-12));
    CHECK_THAT(a.across_strength, WithinAbs(1.0, 1e-12));
    CHECK_THAT(a.z_ratio, WithinAbs(0.80, 1e-12));
    CHECK_THAT(a.z_uncertainty, WithinAbs(0.25, 1e-12));
    CHECK_FALSE(a.too_cold);
    CHECK_FALSE(a.too_hot);
    // A family without data: not modelled, nothing changes, a wide band.
    const Adhesion hips = adhesion(family_of("HIPS"), at(240));
    CHECK_FALSE(hips.modelled);
    CHECK(hips.z_strength == 1.0);
    CHECK(adhesion(nullptr, {}).modelled == false);
    OrthotropicMaterial m, same;
    apply_adhesion(m, hips);
    CHECK(m.zt == same.zt);
    CHECK(m.e3 == same.e3);
}

TEST_CASE("adhesion: the nozzle temperature, as the measured curves", "[adhesion]") {
    const MaterialFamily* pla = family_of("PLA");
    // Colder: M_T(190) / M_T(205) = (15/55)^(1/4) / (30/55)^(1/4).
    const Adhesion cold = adhesion(pla, at(190));
    CHECK_THAT(cold.z_strength, WithinRel(std::pow(0.5, 0.25), 1e-9));
    CHECK(cold.z_stiffness < 1.0);
    CHECK(cold.z_stiffness > 0.95);
    CHECK_THAT(cold.across_strength, WithinAbs(1.0 - 0.015 * 15.0, 1e-12));
    // Hotter, up to the optimum: stronger, but never past r_max.
    const Adhesion warm = adhesion(pla, at(230));
    CHECK_THAT(warm.z_strength, WithinRel(std::pow(55.0 / 30.0, 0.25), 1e-9));
    CHECK(warm.z_ratio <= 0.95);
    CHECK_THAT(adhesion(pla, at(220)).across_strength, WithinAbs(1.0, 1e-12)); // flat for 20 °C above
    CHECK_THAT(warm.across_strength, WithinAbs(0.95, 1e-12));                  // then 1 % per °C
    // Too hot: the polymer degrades past T_hot (245 °C).
    const Adhesion hot = adhesion(pla, at(270));
    CHECK(hot.too_hot);
    CHECK(hot.z_strength < warm.z_strength);
    CHECK_THAT(hot.z_strength, WithinRel(0.8 / std::pow(30.0 / 55.0, 0.25), 1e-9));
    CHECK_THAT(hot.across_strength, WithinAbs(0.55, 1e-12));
    CHECK(hot.z_uncertainty == 0.5);
    // Too cold to bond: the floor r_min.
    const Adhesion frozen = adhesion(pla, at(160));
    CHECK(frozen.too_cold);
    CHECK_THAT(frozen.z_ratio, WithinAbs(0.20, 1e-12));
    CHECK_THAT(frozen.across_strength, WithinAbs(0.6, 1e-12));
    // The relative curve against Rivera-López's PLA (180 °C at 0.53 of
    // 240 °C): the model's 0.55.
    const double ratio = adhesion(pla, at(180)).z_strength / adhesion(pla, at(240)).z_strength;
    CHECK_THAT(ratio, WithinAbs(0.53, 0.05));
    // PETG against CNC Kitchen: 200 °C at 0.56 and 230 °C at 0.94 of the best.
    const MaterialFamily* petg = family_of("PETG");
    const double petg_ratio = adhesion(petg, at(200)).z_strength / adhesion(petg, at(230)).z_strength;
    CHECK_THAT(petg_ratio, WithinAbs(0.56 / 0.94, 0.07));
}

TEST_CASE("adhesion: layer height, single walls, layer time, fan and chamber", "[adhesion]") {
    const MaterialFamily* pla = family_of("PLA");
    const double ref = 1.0 - 0.25 * std::pow(0.2 / 0.42, 1.5);
    // Thicker layers on the same line: less bonded width.
    const Adhesion thick = adhesion(pla, at(205, 0.3, 0.42));
    CHECK_THAT(thick.z_strength, WithinRel((1.0 - 0.25 * std::pow(0.3 / 0.42, 1.5)) / ref, 1e-9));
    CHECK(thick.z_stiffness < 1.0);
    PrintConditions walls = at(205);
    walls.single_walls = true;
    CHECK_THAT(adhesion(pla, walls).z_strength, WithinRel((1.0 - 0.78 * std::pow(0.2 / 0.42, 1.5)) / ref, 1e-9));
    // A long layer with the fan on: the floor 0.6 (and the fan's own loss is
    // the reference's for PLA, printed with the fan).
    PrintConditions slow = at(205);
    slow.layer_time = 60.0;
    slow.fan = 1.0;
    CHECK_THAT(adhesion(pla, slow).z_strength, WithinRel(0.6, 1e-9));
    slow.layer_time = 10.0; // past the 8 s the fan allows: 1 - 0.25 ln(10 / 8)
    CHECK_THAT(adhesion(pla, slow).z_strength, WithinRel(1.0 - 0.25 * std::log(10.0 / 8.0), 1e-9));
    // ABS in a 65 °C chamber against the data sheets' 47.5 °C.
    PrintConditions enclosed = at(245);
    enclosed.chamber = 65.0;
    const Adhesion abs = adhesion(family_of("ABS"), enclosed);
    CHECK_THAT(abs.z_strength, WithinRel((1.0 + 0.75 * 0.5) / (1.0 + 0.75 * 22.5 / 80.0), 1e-9));
    // Nylon is semicrystalline: no chamber law.
    PrintConditions nylon = at(260);
    nylon.chamber = 65.0;
    CHECK_THAT(adhesion(family_of("PA"), nylon).z_strength, WithinAbs(1.0, 1e-12));
    // PEEK above 200 °C: the hot chamber's row.
    PrintConditions peek = at(390);
    CHECK_THAT(adhesion(family_of("PEEK"), peek).z_ratio, WithinAbs(0.20, 1e-12));
    peek.chamber = 220.0;
    CHECK_THAT(adhesion(family_of("PEEK"), peek).z_ratio, WithinAbs(0.5, 1e-12));
}

TEST_CASE("adhesion: the material and the safety factor's band", "[adhesion]") {
    const Adhesion a = adhesion(family_of("PLA"), at(190));
    OrthotropicMaterial m;
    const OrthotropicMaterial before = m;
    apply_adhesion(m, a);
    CHECK_THAT(m.zt, WithinRel(before.zt * a.z_strength, 1e-12));
    CHECK_THAT(m.s13, WithinRel(before.s13 * a.z_strength, 1e-12));
    CHECK_THAT(m.e3, WithinRel(before.e3 * a.z_stiffness, 1e-12));
    CHECK_THAT(m.g23, WithinRel(before.g23 * a.z_stiffness, 1e-12));
    CHECK_THAT(m.yt, WithinRel(before.yt * a.across_strength, 1e-12));
    CHECK(m.xt == before.xt);
    CHECK(m.zc == before.zc);
    CHECK(m.e1 == before.e1);
    const SafetyBand layers = safety_band(2.0, FailureMode::BetweenLayers, a);
    CHECK_THAT(layers.low, WithinAbs(1.5, 1e-12));
    CHECK_THAT(layers.high, WithinAbs(2.5, 1e-12));
    const SafetyBand lines = safety_band(2.0, FailureMode::BetweenLines, a);
    CHECK_THAT(lines.low, WithinAbs(1.4, 1e-12));
    const SafetyBand line = safety_band(2.0, FailureMode::Line, a);
    CHECK_THAT(line.low, WithinAbs(1.8, 1e-12));
    CHECK(std::isinf(safety_band(INFINITY, FailureMode::None, a).low));
}

TEST_CASE("adhesion: the analysis case reads the profile and the hand changes", "[adhesion]") {
    Settings s;
    s.variables["temperature"] = {"190"};
    s.layer_height = 0.2;
    s.wall_width = 0.42;
    s.fan_max = 100.0;
    ProjectEngineering e;
    PrintConditions p = print_conditions(s, e);
    CHECK(p.nozzle_temperature == 190.0);
    CHECK(p.fan == 1.0);
    CHECK(std::isnan(p.chamber)); // no heated chamber
    e.overrides.extrusion_temperature = 230.0;
    e.overrides.layer_height = 0.28;
    p = print_conditions(s, e);
    CHECK(p.nozzle_temperature == 230.0);
    CHECK(p.layer_height == 0.28);
    s.max_chamber_temp = 60.0;
    s.chamber_temperature = 80.0;
    CHECK(print_conditions(s, e).chamber == 60.0);
    // The case: PLA's measured record at 190 °C bonds less between layers.
    TriangleMesh none;
    std::vector<std::string> notes;
    ProjectEngineering bare;
    const AnalysisCase ref = engineering_analysis_case(bare, none, {}, notes, "PLA");
    const AnalysisCase cold = engineering_analysis_case(bare, none, {}, notes, "PLA", 0.2, at(190));
    CHECK(ref.adhesion.modelled);
    CHECK_THAT(cold.material.zt, WithinRel(ref.material.zt * std::pow(0.5, 0.25), 1e-9));
    CHECK(cold.material.xt == ref.material.xt);
}

TEST_CASE("adhesion: the band adds the voxels' error in quadrature", "[adhesion]") {
    const Adhesion a = adhesion(family_of("PLA"), {});
    const SafetyBand b = safety_band(2.0, FailureMode::BetweenLayers, a, 0.2);
    CHECK_THAT(b.sigma, WithinAbs(std::sqrt(0.25 * 0.25 + 0.2 * 0.2), 1e-12));
    CHECK_THAT(b.low, WithinAbs(2.0 * (1.0 - b.sigma), 1e-12));
    // Never below a tenth of the value.
    CHECK(safety_band(2.0, FailureMode::BetweenLayers, a, 5.0).low > 0.19);
}
