// Planar slicing pipeline. Own design; see PROVENANCE.md.

#include "slice/pipeline.hpp"

#include "adhesion/adhesion.hpp"
#include "support/support.hpp"
#include "surfaces/bridges.hpp"

#include <oneapi/tbb/parallel_for.h>

#include <algorithm>
#include <cmath>

namespace tisma {

SlicedModel slice_model(const IndexedMesh& mesh, const Settings& settings, gpu::Context* gpu) {
    SlicedModel out;
    if (mesh.vertices.empty())
        return out;
    float min_z = mesh.vertices.front().z, max_z = min_z;
    for (const Vec3f& v : mesh.vertices) {
        min_z = std::min(min_z, v.z);
        max_z = std::max(max_z, v.z);
    }
    out.layers = plan_layers(min_z, max_z, {settings.first_layer_height, settings.layer_height});
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
        out.regions[l] = offset(region, settings.xy_offset - shrink);
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
    out.surfaces =
        classify_surfaces(out.regions, infill, settings.top_layers, settings.bottom_layers, settings.wall_width);
    find_bridges(out.surfaces, out.regions, out.walls, settings.infill_density > 0.0, settings.wall_width);
    FillParams fill{settings.wall_width, settings.infill_density / 100.0, settings.infill_angle, settings.infill_pattern,
                    settings.monotonic_top};
    fill.wall_overlap = settings.infill_wall_overlap / 100.0;
    out.support_areas.resize(out.layers.size());
    if (settings.support) {
        std::vector<double> thickness;
        for (const Layer& layer : out.layers)
            thickness.push_back(layer.top - layer.bottom);
        SupportParams sp;
        sp.threshold_angle = settings.support_angle;
        sp.z_gap_layers = int(std::lround(settings.support_z_gap / settings.layer_height));
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
                          if (out.support_areas[l].empty())
                              return;
                          // Interface: the support with the part above it within the next
                          // interface_layers layers past the Z gap; the rest is the base.
                          const std::size_t gap = std::size_t(std::max(0L, std::lround(settings.support_z_gap / settings.layer_height)));
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
    return out;
}

} // namespace tisma
