#pragma once

#include "config/settings.hpp"
#include "mesh/indexed_mesh.hpp"
#include "slice/layers.hpp"

#include <array>
#include <cstddef>
#include <vector>

// Variable layer height by Z (docs/VARIABLE_LAYERS.md): the user's list of
// (z, height) steps, the slope of the surface by the cusp height of Dolenc and
// Mäkelä 1994 ("Slicing procedures for layered manufacturing techniques"),
// and the fine sub-layers of layer heights by zone (docs/LOCAL_LAYERS.md), in
// one stack of layers. The planning is own design; see PROVENANCE.md.
namespace tisma {

// The tallest layer the surface allows over each band of Z for a cusp height
// c: a facet with unit normal n leaves a step c = h |n_z| on a layer of height
// h, so it allows c / |n_z|. Horizontal facets bound nothing: a layer ends at
// their height instead (flats).
struct SlopeLimit {
    double z0 = 0.0;
    double bin = 0.005;        // mm of Z per band
    std::vector<float> limit;  // per band, mm; infinity where nothing limits it
    std::vector<double> flats; // Z of the horizontal facets, sorted, without repeats

    // The tallest a layer from bottom to top may be: the least limit of the
    // bands it overlaps.
    double allowed(double bottom, double top) const;
    // The tallest layer from bottom, up to `most`, that is no taller than the
    // bands it overlaps allow (a layer may stop short of a flatter band); with
    // `parts`, a layer split into that many, each no taller than they allow.
    double tallest(double bottom, double most, int parts = 1) const;
};

SlopeLimit slope_limit(const IndexedMesh& mesh, double cusp_height, double max_height, double bin = 0.005);

// The layer heights the machine prints: its own limits, 0.04 mm and the
// nozzle diameter where it gives none, never above the nozzle diameter.
struct HeightBounds {
    double min = 0.04, max = 0.4;
};
HeightBounds height_bounds(const Settings& settings);

// The height the list asks from z (mm above the bed) up: its last step at or
// below z; layer_height below the first step.
double listed_height(const Settings& settings, double z);

// The list or the cusp height is set: the layers are planned one by one.
bool variable_layers(const Settings& settings);

// The fine sub-layers a thick layer of height h splits into for layer heights
// by zone: h over detail_height, rounded, from 2 to 4, none thinner than
// min_height; 1 when detail_height is 0 or not below h.
int sub_layers(double h, double detail_height, double min_height);

// The layers to slice, and which runs of them print as one thick layer.
struct LayerStack {
    std::vector<Layer> layers;
    std::vector<std::array<std::size_t, 2>> groups; // [first, last] layers of each thick layer, k >= 2
    std::vector<double> share; // per layer: the part of its thick layer it is (1 / sub-layers)
};

// Layers from min_z: the first first_layer_height thick, then each thick
// layer from the list (or layer_height), lowered by the slope when the cusp
// height is set, ended at flats and at the list's steps, and split into
// sub_layers() fine layers. Without zones the slope lowers the whole layer;
// with zones it sets how many fine sub-layers the thick layer splits into
// (more where the surface is flatter), and the zones keep the rest thick.
// The uniform stack (no list, no cusp height) is plan_layers() with the
// groups of detail_ratio().
LayerStack plan_layer_stack(const IndexedMesh& mesh, double min_z, double max_z, const Settings& settings);

// For each layer, how many layers above (+1) or below (-1) make `count` thick
// layers by their shares: the count for the skins.
std::vector<int> skin_counts(const std::vector<double>& share, int count, int step);

} // namespace tisma
