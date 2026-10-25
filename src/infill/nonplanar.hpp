#pragma once

#include "config/settings.hpp"
#include "geom/polygons.hpp"
#include "infill/adaptive.hpp"
#include "perimeters/sites.hpp"

#include <optional>
#include <vector>

// Wavy nonplanar infill (docs/NONPLANAR_INFILL.md, N1): the sparse infill's
// beads go up and down in Z as they run, after ZigZagZ (Allum et al. 2021).
// The height is a field fixed in space, the same in every layer, so each
// layer nests on the one below and the joints between layers lean instead of
// lying flat. It only makes sense where the beads stack, in patterns that
// repeat layer to layer. Near the walls and the solid layers the waves fade
// out, so the infill meets them flat. Own design; see PROVENANCE.md.
namespace tisma {

struct WaveParams {
    double amplitude = 0.5;  // mm, from trough to crest
    double wavelength = 2.0; // mm, along a line at 45 degrees to `direction`
    double direction = 45.0; // degrees: the waves run along this, 45 degrees off the infill's lines
    double fade = 1.0;       // mm of depth over which the waves grow from nothing
    double margin = 0.45;    // mm of depth with no waves at all, by the walls and the solid layers
    double step = 0.25;      // mm: the beads are cut this fine to follow the waves
    double cell = 0.4;       // mm: the voxels of the depth field the waves fade by
};

// The waves the settings ask for, or none: off, no sparse infill, a pattern
// whose beads do not stack (it changes from layer to layer), or lines too
// close for the nozzle to clear a crest beside a trough. The amplitude is
// what the settings ask, capped so that the nozzle's flat end (about 2.5
// orifices wide) and its 45-degree side clear a neighbouring line as high as
// a crest.
std::optional<WaveParams> wave_params(const Settings& settings);

// The wave's height at p (mm, from -amplitude / 2 to +amplitude / 2): a
// triangle wave along `direction`, rising amplitude over half a wavelength.
double wave_height(const WaveParams& params, Point2d p);

// How much of the wave a point at this depth in the sparse infill gets: none
// within `margin`, then growing over `fade` to all of it.
double wave_share(const WaveParams& params, double depth);

// Makes the sparse infill of one layer wavy: every path cut `step` fine, each
// point raised by the wave times its share at the layer's height (dz), and
// told where the bead below lies (below: the same at the height of the layer
// below; none for the first layer). The paths are marked wavy.
void make_wavy(std::vector<Toolpath>& paths, const DepthField& depth, double z, const double* z_below,
               const WaveParams& params);

} // namespace tisma
