#pragma once

#include "slice/layers.hpp"

#include <array>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// Mixed colours with one nozzle (docs/spec/features/color_mixing.md): a
// virtual extruder, numbered after the machine's, whose layers take the
// filaments of its blend in turn, so that from afar the eye sees the colour
// between them. Own design; see PROVENANCE.md.
namespace tisma {

class ColorMixError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// A blend of up to three filaments (rule "Datos del proyecto"): its number
// (the one after the real filaments: with 2, the first blend is 3), the
// colour shown for it, and its components, each a filament (from 1) and its
// share (0 to 1). A gradient (rule 10) gives each component a position
// along its range (0 to 1) instead, the range from min_z to max_z, or else
// each run of layers it is in.
struct ColorMix {
    int id = 0;
    std::string color; // "#rrggbb"
    bool gradient = false;
    std::optional<double> min_z, max_z; // mm
    struct Component {
        int extruder = 1;
        double share = 0.0; // a ratio, or a gradient's position
        friend bool operator==(const Component&, const Component&) = default;
    };
    std::vector<Component> components;
    // color_mixing_plus.md (rules 1, 2, 6, 15, 30): a written sequence of
    // filaments (from 1), printed as it is, repeated; whether it is offered;
    // its visible name; its bias, from -1 to 1.
    std::vector<int> pattern;
    bool enabled = true;
    std::string name;
    double bias = 0.0;
    friend bool operator==(const ColorMix&, const ColorMix&) = default;
};

// A written sequence (color_mixing_plus.md, rule 2): filament numbers, or
// letters for the blend's filaments in order (A the first of `letters`, B
// the second; with none, A is filament 1). Spaces, commas, / and - only
// separate: "1,1,2", "1 1 2" and "AAB" are the same; a run of digits is one
// number. Throws ColorMixError for a filament the machine (`extruders`)
// does not have, or an empty sequence.
std::vector<int> parse_mix_pattern(std::string_view text, const std::vector<int>& letters, int extruders);

// A blend of a written sequence: its filaments in the order they first
// appear, each with its share of the sequence (1,1,2: 1 at 2/3, 2 at 1/3).
ColorMix pattern_mix(int id, const std::vector<int>& pattern);

// The automatic pairs (rule 5): every 50 : 50 blend of two of the filaments,
// numbered after them (4 filaments: 6 blends, 5 to 10), each with its
// colour from `filament_colors` (one per filament).
std::vector<ColorMix> auto_pair_mixes(const std::vector<std::string>& filament_colors);

// The cycle of layers of a blend (rule 7): for each share (all above 0,
// together 1), how many layers it gets in the shortest cycle, from 2 to 64
// layers, in which every share is less than 3 points off and each gets at
// least one; then the least such counts (2 and 2 are 1 and 1).
std::vector<int> mix_cycle(const std::vector<double>& shares);

// The order of a cycle (rule 8): at each layer the component furthest
// behind its share so far, the first of the blend on a tie. Indices into
// `counts`. 1 : 3 gives 1, 0, 1, 1.
std::vector<int> mix_order(const std::vector<int>& counts);

// The filament (from 1) each layer prints a blend with: 0 where `present`
// says the blend has nothing. The cycle starts at the first layer it is in
// (rule 9). A gradient (rule 10) is cut into bands, one every 10 layers of
// its range (2 to 11), each with the blend of its middle rounded to 10 %,
// and a cycle of its own from its first layer; under its first point all
// the first filament, over its last all the last. A layer's band goes by its
// top, the layers' heights added up from the bed to the micrometre.
// With `step` above 0 (color_mixing_plus.md, rule 9), a blend that is not a
// gradient goes by height: the layer whose middle is z above the first
// layer's bottom takes element floor(z / step) of its cycle.
std::vector<int> mix_layers(const ColorMix& mix, const std::vector<Layer>& layers, const std::vector<char>& present,
                            double step = 0.0);

// A blend's cycle as the filaments in order: its written sequence, or the
// cycle of its shares (rules 2, 7 and 8). Empty for a gradient.
std::vector<int> mix_sequence(const ColorMix& mix);

// Whether a blend can print on a machine with `extruders` filaments: its
// number after theirs and its filaments among them (rule 5), and it is
// enabled (color_mixing_plus.md, rule 6).
bool mix_usable(const ColorMix& mix, int extruders);

// The project's blends in their JSON (rules 14 to 16): {"version": 1,
// "physical_extruders": [{"id", "color"}], "virtual_extruders": [{"id",
// "kind": "fullspectrum" or "gradient", "color", "components": [{"extruder",
// "ratio"}] or, for a gradient, [{"extruder", "position"}], "minz", "maxz"}]}.
// color_mixing_plus.md (rule 30) adds "kind": "pattern" with "pattern": [1,
// 1, 2], and "enabled", "name" and "bias", written only when they are not
// the default, so a project of ratio blends is written as before.
// Ratios that do not add up to 1 are taken in proportion (1 and 3 are 0.25
// and 0.75). Throws ColorMixError.
std::vector<ColorMix> parse_color_mixes(std::string_view json);
std::string color_mixes_json(const std::vector<ColorMix>& mixes, const std::vector<std::string>& filament_colors);

// How many filament changes a part `height` mm tall printed with `mix`
// asks for, before slicing (color_mixing_plus.md, rules 12 and 32): its
// layers (the first `first_layer_height`, the rest `layer_height`) in the
// blend's cycle; with `subdivide`, each layer past the first (unless
// `sub_first_layer`) one pass per filament, so k filaments make k - 1 changes
// a layer, the next layer starting with the filament the last one ended
// with. A layer whose thinnest pass would be under `sub_min_height` goes by
// the cycle, as the slicing does; a gradient always does.
struct MixChangeOptions {
    double first_layer_height = 0.2, layer_height = 0.2;
    bool subdivide = false, sub_first_layer = false;
    double sub_min_height = 0.04;
    // Each filament of a blend of two with its own height (rule 8): the
    // first layer, then height_b and height_a in turn (0: the layer height),
    // as the slicing lays them, when nothing else on the plate stops it.
    bool cadence_heights = false;
    double height_a = 0.0, height_b = 0.0;
    // The height the cycle is counted in (`mixing.step`, rule 9), on even
    // layers: 0, or the layer height, by layer; finer than the layers
    // without `subdivide`, by layer too, as the slicing does.
    double step = 0.0;
};
int mix_changes_estimate(const ColorMix& mix, double height, const MixChangeOptions& options);

// What lowers a blend's purge when it passes half the filament
// (color_mixing_plus.md, rule 38), each with its estimate before slicing:
// "subdivide" (sublayers off), "cadence_heights" (a blend of two with the
// larger share's layers twice as high, at most `max_height` when above 0) and
// "surface_only" (the same changes, their purge into the hidden infill).
// `purge` is mm3 a change, `part` the part's own plastic, mm3. Empty when
// the purge is half the filament or less.
struct MixSuggestion {
    std::string key;
    int changes = 0;
    double purge_share = 0.0; // of all the filament, 0 to 1
};
std::vector<MixSuggestion> mix_purge_suggestions(const ColorMix& mix, double height, const MixChangeOptions& options,
                                                 double purge, double part, double max_height = 0.0);

// The colour a blend shows from afar (rule 8): its filaments' colours mixed
// by their shares, by the Yule-Nielsen relation (n = 2) on each channel.
// An approximation: the colours real filaments give were not measured.
std::string mix_color(const std::vector<std::string>& colors, const std::vector<double>& shares);

// The spectral prediction (color_mixing_plus.md, rule 21): Kubelka and Munk,
// each filament's K/S per channel from its colour, mixed by its share times
// 1 + TD / 10 (`transmission_distance`, mm: a more translucent filament shows
// more than its share; own design), back to a colour. With every TD at 0, the
// simple mixer (mix_color), MX17.
std::string mix_color_spectral(const std::vector<std::string>& colors, const std::vector<double>& shares,
                               const std::vector<double>& transmission);

// The CIEDE2000 colour difference between two "#rrggbb" colours (CIELAB,
// D65), after G. Sharma, W. Wu and E. N. Dalal, "The CIEDE2000
// color-difference formula: implementation notes, supplementary test data,
// and mathematical observations", Color Research & Application 30 (2005).
double ciede2000(const std::string& a, const std::string& b);
// The same between two CIELAB colours (L*, a*, b*).
double ciede2000_lab(const std::array<double, 3>& a, const std::array<double, 3>& b);

// The colour sought (color_mixing_plus.md, rule 4): every blend of 2 and of
// 3 of the filaments with a cycle of up to `max_cycle` layers and no share
// under `min_part` (0 to 1), its colour by mix_color, the closest first by
// CIEDE2000; the best and the next 3 at most. `id` numbers the blends.
// Reached when the best is 10 or less apart. With `transmission` (each
// filament's TD, mm) the colours come from the spectral prediction
// (mix_color_spectral, `mixing.predictor = "spectral"`, rule 21); empty, from
// the simple mixer (rule 20).
struct MixMatch {
    ColorMix mix;
    std::string color;
    double distance = 0.0;
};
std::vector<MixMatch> match_color(const std::string& target, const std::vector<std::string>& filament_colors, int id,
                                  int max_cycle = 6, double min_part = 0.0,
                                  const std::vector<double>& transmission = {});
inline bool color_reached(const std::vector<MixMatch>& m) { return !m.empty() && m.front().distance <= 10.0; }

// A colour's name (color_mixing_plus.md, rules 6 and 24): the engine's own
// list, in English and Spanish. Own design on CIELAB (D65): chroma under 10
// is white, light grey, grey, dark grey or black by L*; otherwise a hue band
// of h_ab (red, orange, yellow, green, cyan, blue, purple, pink), brown for a
// dark red, orange or yellow, and "Light" or "Dark" by L*.
struct ColorName {
    std::string en, es;
};
ColorName color_name(const std::string& hex);

// The quick palette (color_mixing_plus.md, rule 22, MX18): one sample per
// filament, then every blend of 2 and of 3 filaments whose cycle has at most
// `max_cycle` layers (a shorter cycle once), each with its colour (the
// mixer, or the spectral prediction with `transmission`) and its name. With
// 4 filaments: 10 samples with cycles of up to 2 layers, 26 with up to 3.
// Blends are numbered from `first_id`; a filament's sample is a blend of one.
struct PaletteSample {
    ColorMix mix;
    std::string color;
    ColorName name;
};
std::vector<PaletteSample> quick_palette(const std::vector<std::string>& filament_colors, int max_cycle, int first_id,
                                         const std::vector<double>& transmission = {});

// The purge between two filaments by their colours ("#rrggbb"), mm3
// (color_mixing_plus.md, rule 37; own design): 0 for the same colour, else
// 60 plus 1.6 a point of lightness gained (CIELAB L*, D65) plus 0.6 a point
// of colour apart (a*b*), at most 300. More from dark to light than back.
// An unreadable colour counts as mid grey.
double purge_from_colors(const std::string& from, const std::string& to);

// The table of every pair (row: from, column: to), as wipe_tower.purge_volumes takes it.
std::vector<std::vector<double>> purge_table(const std::vector<std::string>& colors);

} // namespace tisma
