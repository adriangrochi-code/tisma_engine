// Planar slicing pipeline. Own design; see PROVENANCE.md.

#include "slice/pipeline.hpp"

#include "adhesion/adhesion.hpp"
#include "infill/adaptive.hpp"
#include "infill/combine.hpp"
#include "support/support.hpp"
#include "surfaces/bridges.hpp"

#include <oneapi/tbb/parallel_for.h>

#include <algorithm>
#include <cmath>

namespace tisma {

IndexedMesh compensated(const IndexedMesh& mesh, const Settings& settings) {
    if (mesh.vertices.empty() || (settings.scale_x == 100.0 && settings.scale_y == 100.0 && settings.scale_z == 100.0))
        return mesh;
    float x0 = mesh.vertices.front().x, x1 = x0, y0 = mesh.vertices.front().y, y1 = y0, z0 = mesh.vertices.front().z;
    for (const Vec3f& v : mesh.vertices) {
        x0 = std::min(x0, v.x), x1 = std::max(x1, v.x);
        y0 = std::min(y0, v.y), y1 = std::max(y1, v.y);
        z0 = std::min(z0, v.z);
    }
    const double cx = (double(x0) + double(x1)) / 2.0, cy = (double(y0) + double(y1)) / 2.0;
    const double sx = settings.scale_x / 100.0, sy = settings.scale_y / 100.0, sz = settings.scale_z / 100.0;
    IndexedMesh out = mesh;
    for (Vec3f& v : out.vertices) {
        v.x = float(cx + (double(v.x) - cx) * sx);
        v.y = float(cy + (double(v.y) - cy) * sy);
        v.z = float(double(z0) + (double(v.z) - double(z0)) * sz);
    }
    return out;
}

SlicedModel slice_model(const IndexedMesh& input, const Settings& settings, gpu::Context* gpu,
                        const StressField* stress) {
    SlicedModel out;
    if (input.vertices.empty())
        return out;
    const IndexedMesh mesh = compensated(input, settings);
    float min_z = mesh.vertices.front().z, max_z = min_z;
    for (const Vec3f& v : mesh.vertices) {
        min_z = std::min(min_z, v.z);
        max_z = std::max(max_z, v.z);
    }
    // With layer heights by zone, the stack is the fine one (docs/LOCAL_LAYERS.md);
    // with variable heights, planned layer by layer (docs/VARIABLE_LAYERS.md).
    const int ratio = detail_ratio(settings);
    const double stack_height = settings.layer_height / double(ratio); // nominal, for the support's gap
    const LayerStack stack = plan_layer_stack(mesh, min_z, max_z, settings);
    out.layers = stack.layers;
    std::vector<float> heights;
    heights.reserve(out.layers.size());
    for (const Layer& l : out.layers)
        heights.push_back(float(l.slice_z()));

    const std::vector<std::vector<Contour>> contours =
        slice_levels(mesh, height_field(mesh), heights, settings.gap_closing_radius, gpu);

    out.regions.resize(out.layers.size());
    out.walls.resize(out.layers.size());
    BeadingParams beading;
    beading.preferred_width = settings.wall_width;
    beading.wall_count = settings.wall_count;
    std::vector<std::size_t> open(out.layers.size(), 0);
    out.thin_area.assign(out.layers.size(), 0.0);
    // The thinnest feature that gets a bead (bead_count grows with the thickness).
    double lo = 0.0, hi = 4.0 * beading.preferred_width;
    for (int i = 0; i < 40; ++i)
        (bead_count(beading, (lo + hi) / 2.0) > 0 ? hi : lo) = (lo + hi) / 2.0;
    const double thinnest = hi;
    tbb::parallel_for(std::size_t(0), out.layers.size(), [&](std::size_t l) {
        for (const Contour& c : contours[l])
            open[l] += c.closed ? 0 : 1;
        Polygons region =
            layer_region(simplify(to_polygons(contours[l]), settings.simplify_tolerance), settings.fill_mode);
        const double shrink = l == 0 ? settings.first_layer_shrink : 0.0;
        out.regions[l] = offset_holes(offset(region, settings.xy_offset - shrink), settings.hole_offset);
        // What an opening by the thinnest bead removes is too thin to print.
        const Polygons printable = offset(offset(out.regions[l], -thinnest / 2.0), thinnest / 2.0);
        out.thin_area[l] = area_mm2(Clipper2Lib::Difference(out.regions[l], printable, Clipper2Lib::FillRule::NonZero));
        if (settings.wall_count > 0)
            out.walls[l] = layer_walls(out.regions[l], beading);
        else
            out.walls[l].infill = out.regions[l];
    });
    for (std::size_t n : open)
        out.open_contours += n;
    out.open_by_layer = std::move(open);
    std::vector<Polygons> infill(out.walls.size());
    for (std::size_t l = 0; l < out.walls.size(); ++l)
        infill[l] = out.walls[l].infill;
    out.surfaces = variable_layers(settings)
                       ? classify_surfaces(out.regions, infill, skin_counts(stack.share, settings.top_layers, +1),
                                           skin_counts(stack.share, settings.bottom_layers, -1), settings.wall_width)
                       : classify_surfaces(out.regions, infill, settings.top_layers * ratio,
                                           settings.bottom_layers * ratio, settings.wall_width);
    find_bridges(out.surfaces, out.regions, out.walls, settings.infill_density > 0.0, settings.wall_width);
    // Adaptive and support cubic infill (docs/INFILL.md): how deep each point
    // lies in the whole sparse infill, from all its surfaces or from the ones
    // it holds up, before stress bands and the rest take parts of it.
    DepthField depth;
    if ((settings.infill_pattern == InfillPattern::AdaptiveCubic ||
         settings.infill_pattern == InfillPattern::SupportCubic) &&
        settings.infill_density > 0.0) {
        std::vector<Polygons> sparse(out.surfaces.size());
        for (std::size_t l = 0; l < sparse.size(); ++l)
            sparse[l] = out.surfaces[l].sparse;
        // A quarter of the finest line spacing: the levels start 1.4, 2.8...
        // spacings deep.
        depth = sparse_depth(sparse, out.layers, 0.75 * settings.wall_width / (settings.infill_density / 100.0),
                             settings.infill_pattern == InfillPattern::SupportCubic ? DepthFrom::TopsAndWalls
                                                                                    : DepthFrom::Surfaces);
    }
    // Stress-guided infill (docs/INFILL.md): bands of each layer's sparse
    // infill where the part works hardest, each at its density.
    const std::vector<DensityBand> bands =
        stress && !stress->empty() && settings.infill_density > 0.0
            ? density_bands(settings.infill_density / 100.0, settings.infill_stress_density / 100.0)
            : std::vector<DensityBand>{};
    std::vector<std::vector<Polygons>> banded(bands.empty() ? 0 : out.layers.size());
    if (!bands.empty())
        tbb::parallel_for(std::size_t(0), out.layers.size(), [&](std::size_t l) {
            Polygons left = out.surfaces[l].sparse;
            banded[l].resize(bands.size());
            for (std::size_t b = bands.size(); b-- > 0 && !left.empty();) {
                Polygons r = Clipper2Lib::Intersect(loaded_region(*stress, out.layers[l].slice_z(), bands[b].level), left,
                                                    Clipper2Lib::FillRule::NonZero);
                r = offset(offset(r, -settings.wall_width), settings.wall_width);
                if (r.empty())
                    continue;
                left = Clipper2Lib::Difference(left, r, Clipper2Lib::FillRule::NonZero);
                banded[l][b] = std::move(r);
            }
            out.surfaces[l].sparse = std::move(left);
        });
    // Dense infill under the top surfaces (docs/INFILL.md, dense_infill.md):
    // the sparse infill right under a top skin, denser, so the skin does not
    // sag on a light infill.
    std::vector<Polygons> dense_region(out.layers.size());
    if (settings.infill_dense_under_top && settings.infill_density > 0.0 && out.layers.size() > 1)
        tbb::parallel_for(std::size_t(0), out.layers.size() - 1, [&](std::size_t l) {
            Polygons under = Clipper2Lib::Intersect(out.surfaces[l].sparse, out.surfaces[l + 1].top,
                                                    Clipper2Lib::FillRule::NonZero);
            under = offset(offset(under, -settings.wall_width / 2.0), settings.wall_width / 2.0);
            if (under.empty())
                return;
            out.surfaces[l].sparse =
                Clipper2Lib::Difference(out.surfaces[l].sparse, under, Clipper2Lib::FillRule::NonZero);
            dense_region[l] = std::move(under);
        });
    // Combined sparse infill (docs/INFILL.md): what every layer of a run has
    // as sparse infill prints once, at the run's top layer, as thick as the
    // run. Layer heights by zone already print it thick: not with them.
    const std::vector<std::array<std::size_t, 2>> combined =
        stack.groups.empty() && settings.infill_density > 0.0 ? combine_runs(out.layers, settings)
                                                              : std::vector<std::array<std::size_t, 2>>{};
    std::vector<Polygons> combined_region(combined.size());
    tbb::parallel_for(std::size_t(0), combined.size(), [&](std::size_t r) {
        const auto [first, last] = combined[r];
        Polygons common = out.surfaces[first].sparse;
        for (std::size_t l = first + 1; l <= last && !common.empty(); ++l)
            common = Clipper2Lib::Intersect(common, out.surfaces[l].sparse, Clipper2Lib::FillRule::NonZero);
        // Pieces narrower than a bead are not worth a thick pass.
        common = offset(offset(common, -settings.wall_width), settings.wall_width);
        if (common.empty())
            return;
        for (std::size_t l = first; l <= last; ++l)
            out.surfaces[l].sparse =
                Clipper2Lib::Difference(out.surfaces[l].sparse, common, Clipper2Lib::FillRule::NonZero);
        combined_region[r] = std::move(common);
    });
    FillParams fill{settings.wall_width, settings.infill_density / 100.0, settings.infill_angle, settings.infill_pattern,
                    settings.monotonic_top};
    fill.wall_overlap = settings.infill_wall_overlap / 100.0;
    fill.depth = depth.empty() ? nullptr : &depth;
    out.support_areas.resize(out.layers.size());
    if (settings.support) {
        std::vector<double> thickness;
        for (const Layer& layer : out.layers)
            thickness.push_back(layer.top - layer.bottom);
        SupportParams sp;
        sp.threshold_angle = settings.support_angle;
        sp.z_gap_layers = int(std::lround(settings.support_z_gap / stack_height));
        sp.xy_gap = settings.support_xy_gap;
        sp.bead = settings.wall_width;
        out.support_areas = support_areas(out.regions, thickness, sp);
    }
    if (!out.regions.empty()) {
        out.brim = brim_loops(out.regions.front(), settings.brim_width, settings.wall_width);
        out.skirt = skirt_loops(out.regions.front(), settings.brim_width, settings.skirt_layers > 0 ? settings.skirt_loops : 0,
                                settings.skirt_distance, settings.wall_width);
    }
    out.fills.resize(out.layers.size());
    tbb::parallel_for(std::size_t(0), out.layers.size(),
                      [&](std::size_t l) {
                          out.fills[l] = fill_layer(out.surfaces[l], l, fill, out.layers[l].slice_z());
                          for (std::size_t b = 0; l < banded.size() && b < banded[l].size(); ++b) {
                              if (banded[l][b].empty())
                                  continue;
                              FillParams denser = fill;
                              denser.density = bands[b].density;
                              denser.depth = nullptr; // as dense all through
                              LayerSurfaces only;
                              only.sparse = banded[l][b];
                              for (Toolpath& t : fill_layer(only, l, denser, out.layers[l].slice_z()).sparse)
                                  out.fills[l].sparse.push_back(std::move(t));
                          }
                          if (!dense_region[l].empty()) {
                              // Same pattern and direction, closer lines.
                              FillParams dense = fill;
                              dense.density = settings.infill_dense_density / 100.0;
                              dense.depth = nullptr;
                              LayerSurfaces only;
                              only.sparse = dense_region[l];
                              for (Toolpath& t : fill_layer(only, l, dense, out.layers[l].slice_z()).sparse)
                                  out.fills[l].sparse.push_back(std::move(t));
                          }
                          if (out.support_areas[l].empty())
                              return;
                          // Interface: the support with the part above it within the next
                          // interface_layers layers past the Z gap; the rest is the base.
                          const std::size_t gap = std::size_t(std::max(0L, std::lround(settings.support_z_gap / stack_height)));
                          Polygons above;
                          for (int k = 1; k <= settings.support_interface_layers; ++k)
                              if (const std::size_t a = l + gap + std::size_t(k); a < out.regions.size())
                                  above = Clipper2Lib::Union(above, out.regions[a], Clipper2Lib::FillRule::NonZero);
                          const Polygons interface = Clipper2Lib::Intersect(out.support_areas[l], above, Clipper2Lib::FillRule::NonZero);
                          const Polygons base = Clipper2Lib::Difference(out.support_areas[l], above, Clipper2Lib::FillRule::NonZero);
                          const double angle = (l % 2 == 0) ? 0.0 : 90.0;
                          out.fills[l].support = rectilinear_fill(base, settings.support_spacing, angle, settings.wall_width, true);
                          out.fills[l].support_interface =
                              rectilinear_fill(interface, settings.wall_width, angle + 45.0, settings.wall_width, true);
                      });
    if (!combined.empty()) {
        out.thick.resize(out.layers.size());
        tbb::parallel_for(std::size_t(0), combined.size(), [&](std::size_t r) {
            const auto [first, last] = combined[r];
            LayerSurfaces only;
            only.sparse = combined_region[r];
            // The runs take turns in the infill's direction, as layers do.
            const double z = 0.5 * (out.layers[first].bottom + out.layers[last].top);
            ThickLayer& t = out.thick[last];
            t.height = out.layers[last].top - out.layers[first].bottom;
            t.fill.sparse = fill_layer(only, r, fill, z).sparse;
        });
    }
    if (!stack.groups.empty()) {
        LocalLayerParams lp;
        lp.ratio = ratio;
        lp.tolerance = settings.detail_tolerance;
        lp.margin = settings.detail_margin > 0.0 ? settings.detail_margin : 3.0 * settings.wall_width;
        lp.min_coarse = 2.0 * settings.wall_width;
        out.groups = plan_layer_groups(out.regions, stack.groups, lp);
        std::vector<double> group_heights;
        for (const LayerGroup& g : out.groups)
            group_heights.push_back(out.layers[g.last].top - out.layers[g.first].bottom);
        out.thick = split_thick_zones(out.groups, out.walls, out.fills, group_heights, 0.25 * settings.wall_width);
    }
    if (settings.zaa) {
        ZaaParams zp;
        zp.min_thickness = settings.zaa_min_thickness / 100.0;
        zp.raise = settings.zaa_raise;
        zp.step = std::min(0.2, 0.5 * settings.wall_width);
        out.zaa_points = antialias_layers(out.layers, out.walls, out.fills, out.groups, out.thick, SurfaceIndex(mesh), zp);
    }
    return out;
}

} // namespace tisma
