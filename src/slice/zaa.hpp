#pragma once

#include "mesh/indexed_mesh.hpp"
#include "slice/layers.hpp"
#include "slice/local_layers.hpp"

#include <cstdint>
#include <vector>

// Z anti-aliasing (docs/ZAA.md): the extrusions seen on top follow the
// model's surface within their layer instead of its flat top. After the
// feature spec docs/spec/features/z_antialiasing.md, and Song, Ray, Sokolov
// and Lefebvre 2017, "Anti-aliasing for fused filament deposition"
// (Computer-Aided Design 89). The surface index and the splitting are own
// design; see PROVENANCE.md.
namespace tisma {

// The faces of the mesh that look up, by cells of XY: what lies over a point.
class SurfaceIndex {
public:
    SurfaceIndex() = default;
    explicit SurfaceIndex(const IndexedMesh& mesh);

    // The Z of the up-facing surface over (x, y) that lies in [low, high] and
    // is nearest to `near`; NaN when none does.
    double surface(double x, double y, double low, double high, double near) const;

    bool empty() const { return faces_.empty(); }

private:
    struct Face {
        float x[3], y[3], z[3];
        float z_low, z_high;
    };
    std::vector<Face> faces_;
    std::vector<std::uint32_t> start_, items_; // cell c holds items_[start_[c] .. start_[c + 1])
    double x0_ = 0, y0_ = 0, cell_ = 1;
    std::size_t nx_ = 0, ny_ = 0;
};

struct ZaaParams {
    double min_thickness = 0.25; // share of the layer height an extrusion keeps at least
    double step = 0.2;           // mm: paths are split this fine where they leave the layer's top
    bool raise = false;          // also up to the surface above the top (Song et al. 2017), up to half a layer
    // How far above the top the raise may go, mm; below 0, half the layer's
    // own height. antialias_layers sets it to half the thinner of this layer
    // and the next: past the next layer's slice plane, that layer prints the
    // same surface again.
    double above = -1.0;
};

// The heights of a path's points for a layer from bottom to top: each point's
// dz (mm, from the top) where the surface crosses the layer, the path split
// every `step` where it moves off the top. Points that keep the top keep dz 0.
Toolpath antialias_path(const Toolpath& path, const SurfaceIndex& surface, double bottom, double top,
                        const ZaaParams& params);

// Every visible extrusion of every layer: walls, top surfaces and gap fills,
// and the thick layers of layer heights by zone. Returns how many points
// left their layer's top.
std::size_t antialias_layers(const std::vector<Layer>& layers, std::vector<LayerWalls>& walls,
                             std::vector<LayerFill>& fills, std::vector<LayerGroup> const& groups,
                             std::vector<ThickLayer>& thick, const SurfaceIndex& surface, const ZaaParams& params);

} // namespace tisma
