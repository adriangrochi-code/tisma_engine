// Planar slicing pipeline. Own design; see PROVENANCE.md.

#include "slice/pipeline.hpp"

#include "adhesion/adhesion.hpp"
#include "gcode/order.hpp"
#include "infill/adaptive.hpp"
#include "infill/combine.hpp"
#include "infill/lightning.hpp"
#include "infill/nonplanar.hpp"
#include "mesh/indexed_mesh.hpp"
#include "perimeters/fuzzy.hpp"
#include "slice/painted.hpp"
#include "support/support.hpp"
#include "support/tree.hpp"
#include "surfaces/bridges.hpp"

#include <oneapi/tbb/parallel_for.h>

#include <algorithm>
#include <cmath>

namespace tisma {
namespace {

// mm the grid support reaches past what overhangs (supports_skirt.md, rule 2:
// about 1.6 in test S4).
constexpr double kSupportMargin = 1.6;

} // namespace

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
                        const StressField* stress, const Painting* painting) {
    SlicedModel out;
    if (input.vertices.empty())
        return out;
    // Vase mode (vase_mode.md, rule 1): one wall, no infill and no top
    // layers, whatever those settings say.
    if (settings.vase && (settings.wall_count != 1 || settings.infill_density != 0.0 || settings.top_layers != 0 ||
                          settings.top_min_thickness != 0.0)) {
        Settings vase = settings;
        vase.wall_count = 1;
        vase.infill_density = 0.0;
        vase.top_layers = 0;
        vase.top_min_thickness = 0.0;
        return slice_model(input, vase, gpu, stress, painting);
    }
    IndexedMesh mesh = compensated(input, settings);
    // The painted pieces before any triangle is turned: a tree follows the
    // order of its triangle's corners.
    std::array<std::vector<PaintedPiece>, 5> painted; // support enforce, block, seam enforce, block, fuzzy skin
    if (painting && !painting->empty()) {
        painted[0] = painted_pieces(mesh, *painting, PaintChannel::Support, kPaintEnforce);
        painted[1] = painted_pieces(mesh, *painting, PaintChannel::Support, kPaintBlock);
        painted[2] = painted_pieces(mesh, *painting, PaintChannel::Seam, kPaintEnforce);
        painted[3] = painted_pieces(mesh, *painting, PaintChannel::Seam, kPaintBlock);
        painted[4] = painted_pieces(mesh, *painting, PaintChannel::Fuzzy, kPaintEnforce);
    }
    orient_consistently(mesh); // a facet wound the wrong way dropped whole loops
    float min_z = mesh.vertices.front().z, max_z = min_z;
    float min_x = mesh.vertices.front().x, max_x = min_x, min_y = mesh.vertices.front().y, max_y = min_y;
    for (const Vec3f& v : mesh.vertices) {
        min_z = std::min(min_z, v.z);
        max_z = std::max(max_z, v.z);
        min_x = std::min(min_x, v.x), max_x = std::max(max_x, v.x);
        min_y = std::min(min_y, v.y), max_y = std::max(max_y, v.y);
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

    // Levels with the section of the level below come back empty
    // (same_sections; the speed report's V10).
    std::vector<char> same_section;
    const std::vector<std::vector<Contour>> contours =
        slice_levels(mesh, height_field(mesh), heights, settings.gap_closing_radius, gpu, &same_section);

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
    // Fuzzy skin (fuzzy_skin.md): on the outer walls the mode chooses, and
    // where it was painted (a band a bead wide each side of the surface holds
    // the outer wall).
    FuzzyParams fuzzy;
    fuzzy.mode = settings.fuzzy_mode;
    fuzzy.thickness = settings.fuzzy_thickness;
    fuzzy.point_distance = settings.fuzzy_point_distance;
    const std::vector<Polygons> fuzzy_painted = painted_areas(painted[4], out.layers, settings.wall_width);
    // A level the cut left empty takes the contours of the nearest level below
    // that has them, and, when the layer's shrink and fuzz are the level
    // below's too, its region and walls.
    const auto shrink_at = [&](std::size_t l) { return l == 0 ? settings.first_layer_shrink : 0.0; };
    const auto fuzzed = [&](std::size_t l) {
        return fuzzy.mode != FuzzyMode::None || (l < fuzzy_painted.size() && !fuzzy_painted[l].empty());
    };
    std::vector<std::size_t> source(out.layers.size());
    std::vector<char> copied(out.layers.size(), 0);
    for (std::size_t l = 0; l < out.layers.size(); ++l) {
        const bool same = l > 0 && same_section[l];
        source[l] = same ? source[l - 1] : l;
        copied[l] = same && shrink_at(l) == shrink_at(l - 1) && !fuzzed(l) && !fuzzed(l - 1);
    }
    tbb::parallel_for(std::size_t(0), out.layers.size(), [&](std::size_t l) {
        if (copied[l])
            return;
        for (const Contour& c : contours[source[l]])
            open[l] += c.closed ? 0 : 1;
        Polygons region = layer_region(simplify(to_polygons(contours[source[l]]), settings.simplify_tolerance),
                                       settings.fill_mode);
        const double shrink = shrink_at(l);
        out.regions[l] = offset_holes(offset(region, settings.xy_offset - shrink), settings.hole_offset);
        // What an opening by the thinnest bead removes is too thin to print.
        const Polygons printable = offset(offset(out.regions[l], -thinnest / 2.0), thinnest / 2.0);
        out.thin_area[l] = area_mm2(Clipper2Lib::Difference(out.regions[l], printable, Clipper2Lib::FillRule::NonZero));
        if (settings.wall_count > 0) {
            out.walls[l] = layer_walls(out.regions[l], beading);
            // What the walls leave, simplified as the contours are: each wall's
            // rounded corners add vertices that every later step would carry
            // (the speed report's V9).
            out.walls[l].infill = simplify(out.walls[l].infill, settings.simplify_tolerance);
            fuzz_walls(out.walls[l].toolpaths, out.regions[l], fuzzy,
                       l < fuzzy_painted.size() && !fuzzy_painted[l].empty() ? &fuzzy_painted[l] : nullptr, l);
        } else
            out.walls[l].infill = out.regions[l];
    });
    for (std::size_t l = 1; l < out.layers.size(); ++l)
        if (copied[l]) {
            open[l] = open[l - 1];
            out.regions[l] = out.regions[l - 1];
            out.thin_area[l] = out.thin_area[l - 1];
            out.walls[l] = out.walls[l - 1];
        }
    for (std::size_t n : open)
        out.open_contours += n;
    out.open_by_layer = std::move(open);
    std::vector<Polygons> infill(out.walls.size());
    for (std::size_t l = 0; l < out.walls.size(); ++l)
        infill[l] = out.walls[l].infill;
    // The minimum shell thickness (infill_patterns.md, rule 22): more solid
    // layers, of the nominal height, until they add up to it.
    const auto shell = [&](int count, double thickness) {
        return std::max(count, int(std::ceil(thickness / settings.layer_height - 1e-9)));
    };
    const int top_count = shell(settings.top_layers, settings.top_min_thickness);
    const int bottom_count = shell(settings.bottom_layers, settings.bottom_min_thickness);
    out.surfaces = variable_layers(settings)
                       ? classify_surfaces(out.regions, infill, skin_counts(stack.share, top_count, +1),
                                           skin_counts(stack.share, bottom_count, -1), settings.wall_width)
                       : classify_surfaces(out.regions, infill, top_count * ratio, bottom_count * ratio,
                                           settings.wall_width);
    find_bridges(out.surfaces, out.regions, out.walls, settings.infill_density > 0.0, settings.wall_width);
    // A solid layer every N (infill_patterns.md, rule 21): layers N, 2N...
    // fill all their sparse infill solid, as bridges over the sparse infill
    // below.
    if (settings.infill_solid_every > 0)
        for (auto l = std::size_t(settings.infill_solid_every); l < out.surfaces.size();
             l += std::size_t(settings.infill_solid_every)) {
            LayerSurfaces& s = out.surfaces[l];
            if (s.sparse.empty())
                continue;
            if (s.bridge.empty())
                s.bridge_angle = settings.infill_angle + (l % 2 == 1 ? 90.0 : 0.0);
            s.bridge = Clipper2Lib::Union(s.bridge, s.sparse, Clipper2Lib::FillRule::NonZero);
            s.sparse.clear();
        }
    // Small infill zones (surfaces_infill.md, rule 9): an island of a layer's
    // infill region smaller than infill.solid_below_area (a thin leg, a pin)
    // is solid, where sparse lines would hardly hold anything. Not with no
    // infill asked for.
    if (settings.infill_solid_below_area > 0.0 && settings.infill_density > 0.0)
        tbb::parallel_for(std::size_t(0), out.surfaces.size(), [&](std::size_t l) {
            LayerSurfaces& s = out.surfaces[l];
            if (s.sparse.empty())
                return;
            Polygons small;
            for (const Polygons& island : split_islands(infill[l]))
                if (area_mm2(island) < settings.infill_solid_below_area)
                    small.insert(small.end(), island.begin(), island.end());
            if (small.empty())
                return;
            const Polygons solid = Clipper2Lib::Intersect(s.sparse, small, Clipper2Lib::FillRule::NonZero);
            if (solid.empty())
                return;
            s.sparse = Clipper2Lib::Difference(s.sparse, small, Clipper2Lib::FillRule::NonZero);
            s.bottom = Clipper2Lib::Union(s.bottom, solid, Clipper2Lib::FillRule::NonZero);
        });
    // Adaptive and support cubic infill (docs/INFILL.md): how deep each point
    // lies in the whole sparse infill, from all its surfaces or from the ones
    // it holds up, before stress bands and the rest take parts of it.
    DepthField depth;
    const bool lightning = settings.infill_pattern == InfillPattern::Lightning && settings.infill_density > 0.0;
    // The lightning branches hold up what lies over all of it, and the waves of
    // the nonplanar infill fade by the walls and the solid layers around it.
    std::vector<Polygons> whole_sparse;
    if (lightning || wave_params(settings))
        for (const LayerSurfaces& s : out.surfaces)
            whole_sparse.push_back(s.sparse);
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
    // run. Layer heights by zone already print it thick: not with them; nor
    // lightning, whose branches each layer draws from the one above.
    const std::vector<std::array<std::size_t, 2>> combined =
        stack.groups.empty() && settings.infill_density > 0.0 && !lightning ? combine_runs(out.layers, settings)
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
    fill.centre = {0.5 * (double(min_x) + double(max_x)), 0.5 * (double(min_y) + double(max_y))};
    // Lightning (docs/INFILL.md): from the top down, in what the stress
    // bands and the dense infill left of the sparse infill.
    std::vector<std::vector<Toolpath>> branches;
    if (lightning) {
        std::vector<Polygons> left(out.surfaces.size());
        for (std::size_t l = 0; l < left.size(); ++l)
            left[l] = out.surfaces[l].sparse;
        LightningParams lp;
        lp.width = settings.wall_width;
        lp.reach = lightning_reach(settings.wall_width, settings.infill_density / 100.0);
        branches = lightning_layers(whole_sparse, out.regions, out.layers, lp, &left);
    }
    // The stress bands and the dense infill hold up loads and skins all
    // through: with lightning, they are cubic.
    const InfillPattern banded_pattern = lightning ? InfillPattern::Cubic : settings.infill_pattern;
    // The outer wall lies half a bead inside the surface: a painted strip of
    // the surface, grown by a bead, holds it.
    if (!painted[2].empty())
        out.seam_enforce = painted_areas(painted[2], out.layers, settings.wall_width);
    if (!painted[3].empty())
        out.seam_block = painted_areas(painted[3], out.layers, settings.wall_width);
    out.support_areas.resize(out.layers.size());
    if (settings.support) {
        std::vector<double> thickness;
        for (const Layer& layer : out.layers)
            thickness.push_back(layer.top - layer.bottom);
        SupportParams sp;
        sp.threshold_angle = settings.support_angle;
        sp.z_gap_layers = int(std::lround(settings.support_z_gap / stack_height));
        sp.xy_gap = settings.support_xy_gap.of(settings.wall_width);
        sp.margin = kSupportMargin;
        sp.bead = settings.wall_width;
        sp.automatic = settings.support_automatic;
        sp.gpu = gpu;
        // A painted face's own layer, a little wider: its overhang lies under it.
        const std::vector<Polygons> enforce = painted_areas(painted[0], out.layers, settings.wall_width / 2.0);
        const std::vector<Polygons> block = painted_areas(painted[1], out.layers, settings.wall_width / 2.0);
        if (!painted[0].empty())
            sp.enforce = &enforce;
        if (!painted[1].empty())
            sp.block = &block;
        if (settings.support_style == SupportStyle::Organic) {
            TreeParams tp;
            tp.angle = settings.tree_angle;
            tp.angle_preferred = settings.tree_angle_preferred;
            tp.tip_diameter = settings.tree_tip_diameter;
            tp.branch_diameter = settings.tree_branch_diameter;
            tp.widen_angle = settings.tree_branch_diameter_angle;
            tp.tip_spacing = tree_tip_spacing(settings.tree_tip_diameter, settings.tree_top_rate, settings.tree_branch_distance);
            tp.xy_gap = sp.xy_gap;
            tp.bead = settings.wall_width;
            tp.z_gap_layers = sp.z_gap_layers;
            tp.interface_layers = settings.support_interface_layers;
            const std::vector<Polygons> over = supported_overhangs(out.regions, thickness, sp);
            out.support_areas = tree_support_areas(out.regions, tree_nodes(out.regions, thickness, over, tp), over, tp);
        } else {
            out.support_areas = support_areas(out.regions, thickness, sp);
        }
    }
    if (!out.regions.empty()) {
        const double first = out.layers.front().top - out.layers.front().bottom;
        out.brim = brim_loops(out.regions.front(), settings.brim_width, settings.wall_width, first);
        out.skirt = skirt_loops(out.regions.front(), settings.brim_width, settings.skirt_layers > 0 ? settings.skirt_loops : 0,
                                settings.skirt_distance, settings.wall_width, first);
    }
    out.fills.resize(out.layers.size());
    // A layer with nothing but its surfaces to fill, the same as the layer a
    // fill period below (fill_period), has its fill (prismatic parts; the
    // speed report's V10).
    const std::size_t period = fill_period(fill);
    const auto plain = [&](std::size_t l) {
        return l >= branches.size() && (l >= banded.size() || std::all_of(banded[l].begin(), banded[l].end(),
                                                                         [](const Polygons& b) { return b.empty(); })) &&
               dense_region[l].empty() && out.support_areas[l].empty();
    };
    std::vector<char> repeats(out.layers.size(), 0);
    if (period > 0)
        tbb::parallel_for(period, out.layers.size(), [&](std::size_t l) {
            const LayerSurfaces &a = out.surfaces[l], &b = out.surfaces[l - period];
            repeats[l] = plain(l) && plain(l - period) && a.bridge_angle == b.bridge_angle && a.top == b.top &&
                         a.top_under == b.top_under && a.bottom == b.bottom && a.sparse == b.sparse &&
                         a.bridge == b.bridge;
        });
    tbb::parallel_for(std::size_t(0), out.layers.size(),
                      [&](std::size_t l) {
                          if (repeats[l])
                              return;
                          out.fills[l] = fill_layer(out.surfaces[l], l, fill, out.layers[l].slice_z());
                          if (l < branches.size())
                              out.fills[l].sparse = std::move(branches[l]);
                          for (std::size_t b = 0; l < banded.size() && b < banded[l].size(); ++b) {
                              if (banded[l][b].empty())
                                  continue;
                              FillParams denser = fill;
                              denser.density = bands[b].density;
                              denser.depth = nullptr; // as dense all through
                              denser.pattern = banded_pattern;
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
                              dense.pattern = banded_pattern;
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
                          if (settings.support_style == SupportStyle::Organic) {
                              // Interface lines at 45 and 135 degrees in turn (supports_more.md, rule 17).
                              TreeFill tf = tree_fill(out.support_areas[l], above, settings.wall_width, settings.support_spacing,
                                                      settings.tree_double_wall_diameter, (l % 2 == 0) ? 0.0 : 90.0);
                              out.fills[l].support = std::move(tf.support);
                              out.fills[l].support_interface = std::move(tf.interface);
                              return;
                          }
                          // The base in lines along support.line_angle, support.spacing
                          // of free space apart; the interface across them, a
                          // bead's spacing apart (supports_skirt.md, rules 5 and 6).
                          const double bead = loop_spacing(settings.wall_width, out.layers[l].top - out.layers[l].bottom);
                          out.fills[l].support = rectilinear_fill(base, settings.support_spacing + bead, settings.support_line_angle,
                                                                  settings.wall_width, true);
                          out.fills[l].support_interface =
                              rectilinear_fill(interface, bead, settings.support_line_angle + 90.0, settings.wall_width, true);
                      });
    for (std::size_t l = period; l < out.layers.size(); ++l)
        if (repeats[l])
            out.fills[l] = out.fills[l - period];
    // Wavy nonplanar infill (docs/NONPLANAR_INFILL.md, N1): only where the
    // beads stack, in patterns that repeat layer to layer, and not with thick
    // layers or stress bands.
    if (const std::optional<WaveParams> waves = wave_params(settings);
        waves && bands.empty() && combined.empty() && stack.groups.empty()) {
        std::vector<Polygons> sparse(out.layers.size());
        for (std::size_t l = 0; l < sparse.size(); ++l)
            sparse[l] = l < whole_sparse.size() ? whole_sparse[l] : Polygons{};
        const DepthField field = sparse_depth(sparse, out.layers, waves->cell);
        tbb::parallel_for(std::size_t(0), out.layers.size(), [&](std::size_t l) {
            const double below = l > 0 ? out.layers[l - 1].slice_z() : 0.0;
            make_wavy(out.fills[l].sparse, field, out.layers[l].slice_z(), l > 0 ? &below : nullptr, *waves);
        });
    }
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
    // Vase mode (rules 3 and 5): solid only the bottom layers, the last of
    // them the top surface; above them, only the wall, which the G-code lays
    // in a spiral.
    if (settings.vase) {
        const std::size_t bottom = std::size_t(std::max(settings.bottom_layers, 0));
        for (std::size_t l = 0; l < out.fills.size(); ++l) {
            LayerFill& f = out.fills[l];
            if (l + 1 == bottom) {
                f.top.insert(f.top.end(), std::make_move_iterator(f.solid.begin()), std::make_move_iterator(f.solid.end()));
                f.solid.clear();
            } else if (l >= bottom) {
                f = LayerFill{};
            }
        }
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
