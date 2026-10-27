#pragma once

#include "slice/layers.hpp"

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
    friend bool operator==(const ColorMix&, const ColorMix&) = default;
};

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
std::vector<int> mix_layers(const ColorMix& mix, const std::vector<Layer>& layers, const std::vector<char>& present);

// Whether a blend can print on a machine with `extruders` filaments: its
// number after theirs and its filaments among them (rule 5).
bool mix_usable(const ColorMix& mix, int extruders);

// The project's blends in their JSON (rules 14 to 16): {"version": 1,
// "physical_extruders": [{"id", "color"}], "virtual_extruders": [{"id",
// "kind": "fullspectrum" or "gradient", "color", "components": [{"extruder",
// "ratio"}] or, for a gradient, [{"extruder", "position"}], "minz", "maxz"}]}.
// Ratios that do not add up to 1 are taken in proportion (1 and 3 are 0.25
// and 0.75). Throws ColorMixError.
std::vector<ColorMix> parse_color_mixes(std::string_view json);
std::string color_mixes_json(const std::vector<ColorMix>& mixes, const std::vector<std::string>& filament_colors);

// The colour a blend shows from afar (rule 8): its filaments' colours mixed
// by their shares, by the Yule-Nielsen relation (n = 2) on each channel.
// An approximation: the colours real filaments give were not measured.
std::string mix_color(const std::vector<std::string>& colors, const std::vector<double>& shares);

} // namespace tisma
