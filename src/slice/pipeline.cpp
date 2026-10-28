// Planar slicing pipeline. Own design; see PROVENANCE.md.

#include "slice/pipeline.hpp"

#include "adhesion/adhesion.hpp"
#include "gcode/order.hpp"
#include "infill/adaptive.hpp"
#include "infill/arcs.hpp"
#include "infill/combine.hpp"
#include "infill/lightning.hpp"
#include "infill/nonplanar.hpp"
#include "mesh/indexed_mesh.hpp"
#include "perimeters/fuzzy.hpp"
#include "slice/belt.hpp"
#include "slice/painted.hpp"
#include "support/support.hpp"
#include "support/tree.hpp"
#include "surfaces/bridges.hpp"
#include "perimeters/fixed.hpp"

#include <oneapi/tbb/parallel_for.h>

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <limits>
#include <numbers>
#include <optional>

namespace tisma {
namespace {

// mm the grid support reaches past what overhangs (supports_skirt.md, rule 2:
// about 1.6 in test S4).
constexpr double kSupportMargin = 1.6;
// mm the bridge over the support reaches over the part around it
// (supports_skirt.md, rule 4: about 0.6 mm in the test S4).
constexpr double kBridgeAnchor = 0.6;

// The bridge lines over the support fill the Z gap under them
// (supports_skirt.md, rule 4): the ones with most of their length within
// `reach` of where the layer rests on the support.
void rest_on_support(std::vector<Toolpath>& bridges, const Polygons& on, double gap, double reach) {
    const Polygons near = offset(on, reach);
    for (Toolpath& t : bridges) {
        double inside = 0.0, total = 0.0;
        for (std::size_t i = 1; i < t.points.size(); ++i) {
            const Point2d a = t.points[i - 1].p, b = t.points[i].p;
            const double length = std::hypot(b.x - a.x, b.y - a.y);
            total += length;
            if (region_contains(near, (a.x + b.x) / 2.0, (a.y + b.y) / 2.0))
                inside += length;
        }
        if (total > 0.0 ? inside > total / 2.0
                        : !t.points.empty() && region_contains(near, t.points.front().p.x, t.points.front().p.y))
            t.gap = gap;
    }
}

// The raft (skirt_brim_raft.md, rules 3 and 9 to 14) under a part sliced
// without it: raft_layers layers of support put before the part's, which go
// up by their height and raft_gap. The raft follows the part's first layer
// grown by raft_expansion (the exact value, rule 13), its first layer
// raft_first_layer_expansion more. Its first layer is as thick as the first
// layer, the others three quarters of the nozzle (0.3 mm with a 0.4 nozzle,
// rule 11). From the top: support.interface_layers of dense interface at 135
// and 45 degrees in turn, at most all but the first (R1, R2; a raft of one
// layer is interface), then the base, support.spacing apart, at
// support.line_angle and turned 90 degrees in turn; the first layer at
// raft_first_layer_density. The skirt goes round the raft's first layer.
void add_raft(SlicedModel& out, const Settings& settings) {
    const auto n = std::size_t(settings.raft_layers);
    if (n == 0 || out.layers.empty())
        return;
    Polygons footprint;
    for (const Polygons& r : out.regions)
        if (!r.empty()) {
            footprint = r;
            break;
        }
    if (footprint.empty())
        return;
    const auto Union = [](const Polygons& p) { return Clipper2Lib::Union(p, Clipper2Lib::FillRule::NonZero); };
    const Polygons area = Union(offset(footprint, settings.raft_expansion));
    const Polygons first_area = Union(offset(area, settings.raft_first_layer_expansion));
    const std::size_t interfaces =
        n == 1 ? 1 : std::min(n - 1, std::size_t(std::max(settings.support_interface_layers, 0)));
    const double width = settings.support_width.value <= 0.0 ? settings.wall_width
                         : settings.support_width.percent   ? settings.support_width.value / 100.0 * settings.layer_height
                                                            : settings.support_width.value;
    std::vector<Layer> layers;
    std::vector<LayerFill> fills(n);
    std::vector<Polygons> regions(n);
    double z = out.layers.front().bottom;
    for (std::size_t k = 0; k < n; ++k) {
        const double h = k == 0 ? settings.first_layer_height : 0.75 * settings.nozzle_diameter;
        layers.push_back({z, z + h});
        z += h;
        regions[k] = k == 0 ? first_area : area;
        const double bead = loop_spacing(width, h);
        if (k + interfaces >= n) { // interface, the top one at 135
            const double angle = (n - 1 - k) % 2 == 0 ? 135.0 : 45.0;
            fills[k].support_interface = rectilinear_fill(regions[k], bead, angle, width, true);
        } else { // base, the top one at support.line_angle
            const double angle = settings.support_line_angle + ((n - interfaces - 1 - k) % 2 == 1 ? 90.0 : 0.0);
            const double spacing =
                k == 0 ? bead / (settings.raft_first_layer_density / 100.0) : settings.support_spacing + bead;
            fills[k].support = rectilinear_fill(regions[k], spacing, angle, width, true);
        }
    }
    // The part goes up.
    const double shift = z + settings.raft_gap - out.layers.front().bottom;
    for (Layer& l : out.layers)
        l.bottom += shift, l.top += shift;
    // Every per-layer list gets the raft's layers in front.
    const std::size_t count = out.layers.size();
    const auto prepend = [&](auto& list, auto&& value) {
        if (list.size() == count)
            list.insert(list.begin(), n, value);
    };
    prepend(out.walls, LayerWalls{});
    prepend(out.surfaces, LayerSurfaces{});
    prepend(out.support_areas, Polygons{});
    prepend(out.hanging, Polygons{});
    prepend(out.on_support, Polygons{});
    prepend(out.support_gap, 0.0);
    prepend(out.air_bridges, Polygons{});
    prepend(out.seam_enforce, Polygons{});
    prepend(out.seam_block, Polygons{});
    prepend(out.open_by_layer, std::size_t(0));
    prepend(out.thin_area, 0.0);
    prepend(out.thick, ThickLayer{});
    out.regions.insert(out.regions.begin(), regions.begin(), regions.end());
    out.fills.insert(out.fills.begin(), std::make_move_iterator(fills.begin()), std::make_move_iterator(fills.end()));
    out.layers.insert(out.layers.begin(), layers.begin(), layers.end());
    for (LayerGroup& g : out.groups)
        g.first += n, g.last += n;
    out.raft_layers = n;
    // No brim on a raft (rule 9); the skirt round its first layer (rule 3).
    out.brim.clear();
    AdhesionShape shape = adhesion_shape(settings);
    shape.brim_width = 0.0;
    out.skirt = skirt_loops(first_area, shape, settings.wall_width, settings.first_layer_height);
}

// The bridges' beads are round (gcode_motion.md, rule 1): as thick as they
// are wide, their section pi/4 of the width squared, the height past the
// layer's in each path's gap. The ones resting on the support get its gap after.
void round_bridges(std::vector<Toolpath>& bridges, double width, double layer_height) {
    const double gap = std::max(0.0, std::numbers::pi / 4.0 * width - layer_height);
    for (Toolpath& t : bridges)
        t.gap = gap;
}

} // namespace

double bridge_bead(const Settings& settings) {
    return settings.nozzle_diameter * std::sqrt(std::max(settings.bridge_flow_ratio, 0.0));
}

// The walls of a region, `count` of them, by the settings' generator.
LayerWalls walls_of(const Polygons& region, int count, const Settings& settings, BeadingParams beading) {
    if (settings.wall_generator == WallGenerator::Fixed)
        return fixed_walls(region, settings.wall_width, count, settings.wall_gap_fill, settings.outer_wall_width);
    beading.wall_count = count;
    return layer_walls(region, beading);
}

// Only the outer wall over a top surface (walls_extra.md, rule 10): the
// part of the region the layer above leaves open (wider than a bead) keeps
// the outer wall alone; the inner walls go round what goes on up, as if it
// were a region of its own, and the top surface takes the room they leave.
// None when nothing is open above.
std::optional<LayerWalls> one_wall_on_top(const Polygons& region, const Polygons& above, const Settings& settings,
                                          const BeadingParams& beading) {
    const double w = settings.wall_width;
    const Polygons open = offset(offset(Clipper2Lib::Difference(region, above, Clipper2Lib::FillRule::NonZero), -w / 2.0), w / 2.0);
    if (open.empty())
        return std::nullopt;
    LayerWalls out = walls_of(region, 1, settings, beading);
    // What goes on up, without the slivers the difference leaves along the
    // open part (narrower than half a bead: no inner wall fits there).
    const Polygons on = offset(offset(Clipper2Lib::Difference(region, open, Clipper2Lib::FillRule::NonZero), -w / 4.0), w / 4.0);
    if (on.empty() || settings.wall_count < 2)
        return out;
    LayerWalls inner = walls_of(on, settings.wall_count, settings, beading);
    for (Toolpath& t : inner.toolpaths)
        if (t.index > 0)
            out.toolpaths.push_back(std::move(t));
    out.gaps.insert(out.gaps.end(), inner.gaps.begin(), inner.gaps.end());
    // The inner walls' band, from a bead inside what goes on up to its infill.
    const Polygons band = Clipper2Lib::Difference(offset(on, -w), inner.infill, Clipper2Lib::FillRule::NonZero);
    out.infill = Clipper2Lib::Difference(out.infill, band, Clipper2Lib::FillRule::NonZero);
    return out;
}

IndexedMesh compensated(const IndexedMesh& mesh, const Settings& settings) {
    return compensated_like(mesh, mesh, settings);
}

IndexedMesh compensated_like(const IndexedMesh& mesh, const IndexedMesh& part, const Settings& settings) {
    // The filament's shrinkage (gcode_extras.md, rule 4): the part grows so
    // that, shrunk, it measures what was asked.
    const double grow_xy = 100.0 / (100.0 - settings.shrinkage_xy), grow_z = 100.0 / (100.0 - settings.shrinkage_z);
    const double sx = settings.scale_x / 100.0 * grow_xy, sy = settings.scale_y / 100.0 * grow_xy,
                 sz = settings.scale_z / 100.0 * grow_z;
    if (mesh.vertices.empty() || part.vertices.empty() || (sx == 1.0 && sy == 1.0 && sz == 1.0))
        return mesh;
    // About the part's centre and its bottom, whatever mesh moves.
    float x0 = part.vertices.front().x, x1 = x0, y0 = part.vertices.front().y, y1 = y0, z0 = part.vertices.front().z;
    for (const Vec3f& v : part.vertices) {
        x0 = std::min(x0, v.x), x1 = std::max(x1, v.x);
        y0 = std::min(y0, v.y), y1 = std::max(y1, v.y);
        z0 = std::min(z0, v.z);
    }
    const double cx = (double(x0) + double(x1)) / 2.0, cy = (double(y0) + double(y1)) / 2.0;
    IndexedMesh out = mesh;
    for (Vec3f& v : out.vertices) {
        v.x = float(cx + (double(v.x) - cx) * sx);
        v.y = float(cy + (double(v.y) - cy) * sy);
        v.z = float(double(z0) + (double(v.z) - double(z0)) * sz);
    }
    return out;
}

// The triangles of one object of a mesh, with the vertices they use, and their painting.
IndexedMesh object_mesh(const IndexedMesh& mesh, const MeshObject& object) {
    IndexedMesh out;
    std::vector<std::uint32_t> index(mesh.vertices.size(), UINT32_MAX);
    const std::size_t end = std::min(mesh.triangles.size(), object.first + object.count);
    for (std::size_t t = object.first; t < end; ++t) {
        std::array<std::uint32_t, 3> tri{};
        for (int k = 0; k < 3; ++k) {
            std::uint32_t& v = index[mesh.triangles[t][std::size_t(k)]];
            if (v == UINT32_MAX) {
                v = std::uint32_t(out.vertices.size());
                out.vertices.push_back(mesh.vertices[mesh.triangles[t][std::size_t(k)]]);
            }
            tri[std::size_t(k)] = v;
        }
        out.triangles.push_back(tri);
    }
    return out;
}

Painting object_painting(const Painting& painting, const MeshObject& object) {
    Painting out;
    for (std::size_t c = 0; c < kPaintChannels; ++c) {
        const auto channel = PaintChannel(c);
        for (std::size_t t = 0; t < object.count && object.first + t < painting.size(channel); ++t)
            if (const Painting::Tree& tree = painting.tree(channel, object.first + t); !tree.empty())
                out.set_tree(channel, t, tree);
    }
    return out;
}

// The convex hull of a mesh's vertices on the bed, counter-clockwise.
Polygon outline_of(const IndexedMesh& mesh) {
    std::vector<Clipper2Lib::Point64> p;
    p.reserve(mesh.vertices.size());
    for (const Vec3f& v : mesh.vertices)
        p.push_back({to_units(v.x), to_units(v.y)});
    std::sort(p.begin(), p.end(), [](const Clipper2Lib::Point64& a, const Clipper2Lib::Point64& b) { return a.x < b.x || (a.x == b.x && a.y < b.y); });
    p.erase(std::unique(p.begin(), p.end()), p.end());
    if (p.size() < 3)
        return Polygon(p.begin(), p.end());
    const auto turn = [](const Clipper2Lib::Point64& o, const Clipper2Lib::Point64& a, const Clipper2Lib::Point64& b) {
        return double(a.x - o.x) * double(b.y - o.y) - double(a.y - o.y) * double(b.x - o.x);
    };
    Polygon hull(2 * p.size());
    std::size_t k = 0;
    for (std::size_t i = 0; i < p.size(); ++i) { // lower, then upper (Andrew's monotone chain)
        while (k >= 2 && turn(hull[k - 2], hull[k - 1], p[i]) <= 0.0)
            --k;
        hull[k++] = p[i];
    }
    for (std::size_t i = p.size() - 1, low = k + 1; i > 0; --i) {
        while (k >= low && turn(hull[k - 2], hull[k - 1], p[i - 1]) <= 0.0)
            --k;
        hull[k++] = p[i - 1];
    }
    hull.resize(k - 1);
    return hull;
}

// Appends an object sliced apart after the layers of `out` (sequential.md, rule 1).
void append_object(SlicedModel& out, SlicedModel&& part, SlicedObject object) {
    const std::size_t offset = out.layers.size();
    object.first_layer = offset;
    object.raft_layers = part.raft_layers;
    object.skirt = std::move(part.skirt);
    object.brim = std::move(part.brim);
    const auto add = [](auto& to, auto& from) { to.insert(to.end(), std::make_move_iterator(from.begin()), std::make_move_iterator(from.end())); };
    add(out.layers, part.layers);
    add(out.regions, part.regions);
    add(out.walls, part.walls);
    add(out.surfaces, part.surfaces);
    add(out.fills, part.fills);
    add(out.support_areas, part.support_areas);
    add(out.hanging, part.hanging);
    add(out.on_support, part.on_support);
    add(out.support_gap, part.support_gap);
    add(out.air_bridges, part.air_bridges);
    add(out.seam_enforce, part.seam_enforce);
    add(out.seam_block, part.seam_block);
    add(out.open_by_layer, part.open_by_layer);
    add(out.thin_area, part.thin_area);
    add(out.thick, part.thick);
    for (LayerGroup& g : part.groups) {
        g.first += offset, g.last += offset;
        out.groups.push_back(std::move(g));
    }
    out.open_contours += part.open_contours;
    out.cut_away += part.cut_away;
    out.zaa_points += part.zaa_points;
    out.objects.push_back(std::move(object));
}

SlicedModel object_part(const SlicedModel& model, std::size_t k) {
    SlicedModel out;
    if (k >= model.objects.size())
        return out;
    const SlicedObject& o = model.objects[k];
    const std::size_t from = o.first_layer;
    const std::size_t to = k + 1 < model.objects.size() ? model.objects[k + 1].first_layer : model.layers.size();
    const auto take = [&](auto& dst, const auto& src) {
        if (from < src.size())
            dst.assign(src.begin() + std::ptrdiff_t(from), src.begin() + std::ptrdiff_t(std::min(to, src.size())));
    };
    take(out.layers, model.layers);
    take(out.regions, model.regions);
    take(out.walls, model.walls);
    take(out.surfaces, model.surfaces);
    take(out.fills, model.fills);
    take(out.support_areas, model.support_areas);
    take(out.hanging, model.hanging);
    take(out.on_support, model.on_support);
    take(out.support_gap, model.support_gap);
    take(out.air_bridges, model.air_bridges);
    take(out.seam_enforce, model.seam_enforce);
    take(out.seam_block, model.seam_block);
    take(out.open_by_layer, model.open_by_layer);
    take(out.thin_area, model.thin_area);
    take(out.thick, model.thick);
    for (LayerGroup g : model.groups)
        if (g.first >= from && g.last < to) {
            g.first -= from, g.last -= from;
            out.groups.push_back(std::move(g));
        }
    out.skirt = o.skirt;
    out.brim = o.brim;
    out.raft_layers = o.raft_layers;
    return out;
}

namespace {

// One extruder's part of a plate printed with several (multi_extruder.md,
// rule 1), sliced on the plate's layers. What every part lays (`cover`)
// holds up the walls, bridges and overhangs; the skins see it too, unless
// surfaces.interface_shells keeps each part's own. What the parts before
// it took (`taken`) is not this one's. The support is the plate's: the part
// rests on it but lays none, nor skirt or brim. Its fills are centred where
// the plate's are.
// A modifier that changes only the infill (model_tools.md, rule 3, MT4):
// where it reaches, the sparse infill takes its pattern, density and angle;
// the walls go round the whole part.
struct InfillZone {
    std::vector<Polygons> reach; // per layer of the frame
    Settings settings;
};

struct PartFrame {
    LayerStack stack;
    const std::vector<InfillZone>* infill_zones = nullptr;
    const std::vector<Polygons>* cover = nullptr;
    const std::vector<Polygons>* taken = nullptr;
    const std::vector<Polygons>* support = nullptr;
    Point2d centre;
    // The part's regions when the beams between materials changed them
    // (multi_extruder.md, rule 17); null: its own.
    const std::vector<Polygons>* regions = nullptr;
};

// `tool`: the extruder of the part's lines, for their widths (the first
// with none forced).
// `negative`: the volumes that take material away (model_tools.md, rule 2),
// compensated as the part; null when there are none.
SlicedModel slice_layers(const IndexedMesh& compensated_mesh, const Settings& settings, gpu::Context* gpu,
                         const StressField* stress, const Painting* painting, const PartFrame* frame, int tool = 1,
                         const IndexedMesh* negative = nullptr);
SlicedModel slice_plate(const IndexedMesh& input, const Settings& settings, gpu::Context* gpu, const StressField* stress,
                        const Painting* painting, const std::vector<MeshObject>* tools, int tool = 1,
                        const IndexedMesh* negative = nullptr, const std::vector<MeshModifier>* modifiers = nullptr);
// `uncompensated`: the plate before compensated(), for the modifiers to be
// compensated as it was.
SlicedModel slice_tools(const IndexedMesh& mesh, const Settings& settings, gpu::Context* gpu, const StressField* stress,
                        const Painting* painting, const std::vector<MeshObject>& objects,
                        const IndexedMesh* negative = nullptr, const std::vector<MeshModifier>* modifiers = nullptr,
                        const IndexedMesh* uncompensated = nullptr);

// Each list of a part's paths (walls and fills, not the support), with its layer.
template <class F>
void part_paths(SlicedModel& part, F&& f) {
    for (std::size_t l = 0; l < part.walls.size(); ++l)
        f(l, part.walls[l].toolpaths), f(l, part.walls[l].gaps);
    for (std::size_t l = 0; l < part.fills.size(); ++l) {
        LayerFill& fill = part.fills[l];
        for (std::vector<Toolpath>* list : {&fill.top, &fill.solid, &fill.sparse, &fill.bridge, &fill.gaps, &fill.arcs,
                                            &fill.overhang_walls, &fill.ironing})
            f(l, *list);
    }
    for (std::size_t l = 0; l < part.thick.size(); ++l) {
        ThickLayer& t = part.thick[l];
        for (std::vector<Toolpath>* list : {&t.walls, &t.fill.top, &t.fill.solid, &t.fill.sparse, &t.fill.bridge, &t.fill.gaps})
            f(l, *list);
    }
}

// Every path of a part's walls and fills (not the support) printed by `tool`.
void tag_tool(SlicedModel& part, int tool) {
    part_paths(part, [&](std::size_t, std::vector<Toolpath>& paths) {
        for (Toolpath& t : paths)
            t.tool = std::uint8_t(tool);
    });
}

// An object's extruder, as slice_model resolved it: one the machine has, or
// a blend's number.
int object_tool(const MeshObject& o, const Settings&) {
    return std::max(o.extruder, 1);
}

// Mixed colours (color_mixing.md, rules 6 to 10): the paths a blend's
// objects lay (marked with its number) each take the filament of their
// layer, whole layers at a time.
void apply_mixes(SlicedModel& model, const std::vector<ColorMix>& mixes) {
    for (const ColorMix& mix : mixes) {
        std::vector<char> present(model.layers.size(), 0);
        part_paths(model, [&](std::size_t l, std::vector<Toolpath>& paths) {
            if (l < present.size() && !present[l])
                present[l] = std::any_of(paths.begin(), paths.end(), [&](const Toolpath& t) { return t.tool == mix.id; });
        });
        if (std::none_of(present.begin(), present.end(), [](char c) { return c != 0; }))
            continue;
        const std::vector<int> filament = mix_layers(mix, model.layers, present);
        part_paths(model, [&](std::size_t l, std::vector<Toolpath>& paths) {
            for (Toolpath& t : paths)
                if (t.tool == mix.id && l < filament.size() && filament[l] > 0)
                    t.tool = std::uint8_t(filament[l]);
        });
    }
}

// Different nozzles (multimaterial.md, rule 3; own design): the line widths
// are for machine.nozzle_diameter, the first extruder's, and each line
// takes its extruder's nozzle in proportion. The walls and what is laid
// with their width (the solid infill, the bridges, the support) the walls'
// extruder's, the sparse infill the infill's; with
// multi.fine_walls_coarse_infill, the sparse infill combined over as many
// layers as 75 % of its nozzle holds (rule 2).
struct PartNozzles {
    Settings settings;   // with the walls' width and nozzle
    double sparse_width; // mm
    double infill_nozzle; // mm
};
PartNozzles part_nozzles(const Settings& s, int tool) {
    // The sparse infill's width: its own, or 1.125 nozzles (surfaces_infill.md, I9).
    const double sparse = s.infill_width > 0.0 ? s.infill_width : 1.125 * s.nozzle_diameter;
    PartNozzles p{s, sparse, s.nozzle_diameter};
    if (s.extruders < 2 || s.nozzle_diameters.empty() || s.single_extruder_mmu)
        return p;
    const KindExtruders kinds = kind_extruders(s);
    const double walls = nozzle_of(s, kinds.walls > 0 ? kinds.walls : tool);
    const double infill = nozzle_of(s, kinds.infill > 0 ? kinds.infill : tool);
    p.settings.wall_width = s.wall_width * walls / s.nozzle_diameter;
    p.settings.outer_wall_width = s.outer_wall_width * walls / s.nozzle_diameter;
    p.settings.nozzle_diameter = walls;
    p.sparse_width = sparse * infill / s.nozzle_diameter;
    p.infill_nozzle = infill;
    if (s.fine_walls_coarse_infill && infill != walls) {
        p.settings.infill_combine_auto = true;
        p.settings.infill_combine_max_height = {75.0, true};
    }
    return p;
}

// Whether the plate's objects print with more than one extruder, or with
// settings of their own that differ (model_tools.md, rules 5 and 7).
bool several_tools(const std::vector<MeshObject>& objects, const Settings& settings) {
    return std::any_of(objects.begin(), objects.end(), [&](const MeshObject& o) {
        return object_tool(o, settings) != object_tool(objects.front(), settings) ||
               o.settings != objects.front().settings;
    });
}

} // namespace

SlicedModel slice_model(const IndexedMesh& input, const Settings& settings, gpu::Context* gpu,
                        const StressField* stress, const Painting* painting, const std::vector<MeshObject>* given,
                        const std::vector<ColorMix>* mixes, const IndexedMesh* negative_volumes,
                        const std::vector<MeshModifier>* given_modifiers) {
    SlicedModel out;
    if (input.vertices.empty())
        return out;
    // Height ranges (model_tools.md, rule 6): their layer heights as tramos
    // of layers.heights, each starting a layer, and their other settings as a
    // modifier as wide as the object between their heights, under the parts'
    // own settings and the modifiers.
    std::vector<MeshModifier> with_ranges;
    std::optional<Settings> ranged;
    if (given)
        for (const MeshObject& o : *given) {
            if (o.ranges.empty())
                continue;
            const IndexedMesh piece = object_mesh(input, o);
            if (piece.vertices.empty())
                continue;
            float x0 = piece.vertices.front().x, x1 = x0, y0 = piece.vertices.front().y, y1 = y0, z0 = piece.vertices.front().z;
            for (const Vec3f& v : piece.vertices)
                x0 = std::min(x0, v.x), x1 = std::max(x1, v.x), y0 = std::min(y0, v.y), y1 = std::max(y1, v.y),
                z0 = std::min(z0, v.z);
            for (const MeshObject::HeightRange& r : o.ranges) {
                if (!(r.to > r.from))
                    continue;
                char span[64];
                std::snprintf(span, sizeof span, " %g-%g mm", r.from, r.to);
                const std::string where = o.name + span;
                const Settings own = own_settings(settings, r.settings, where, true); // and its errors said
                bool layers = false;
                const std::string rest = own_without(r.settings, "layers", &layers);
                if (layers) {
                    if (!ranged)
                        ranged = settings;
                    if (!settings.layer_heights.empty()) {
                        out.notices.push_back(o.name + ": layers.heights is given, so the height ranges' layer heights are left out");
                    } else {
                        // Past the range, the height that held there before it.
                        auto& h = ranged->layer_heights;
                        double after = settings.layer_height;
                        for (const auto& t : h)
                            if (t[0] <= z0 + r.to + 1e-9)
                                after = t[1];
                        h.erase(std::remove_if(h.begin(), h.end(), [&](const auto& t) {
                                    return t[0] >= z0 + r.from - 1e-9 && t[0] < z0 + r.to - 1e-9;
                                }),
                                h.end());
                        h.push_back({double(z0) + r.from, own.layer_height});
                        if (std::none_of(h.begin(), h.end(), [&](const auto& t) { return std::abs(t[0] - (z0 + r.to)) < 1e-9; }))
                            h.push_back({double(z0) + r.to, after});
                        std::sort(h.begin(), h.end());
                    }
                }
                if (rest.empty())
                    continue;
                MeshModifier m{where, o.object, {}, rest, true};
                const Vec3f lo{x0 - 1.0f, y0 - 1.0f, float(double(z0) + r.from)}, hi{x1 + 1.0f, y1 + 1.0f, float(double(z0) + r.to)};
                const Vec3f c[8] = {{lo.x, lo.y, lo.z}, {hi.x, lo.y, lo.z}, {hi.x, hi.y, lo.z}, {lo.x, hi.y, lo.z},
                                    {lo.x, lo.y, hi.z}, {hi.x, lo.y, hi.z}, {hi.x, hi.y, hi.z}, {lo.x, hi.y, hi.z}};
                static constexpr int f[12][3] = {{0, 2, 1}, {0, 3, 2}, {4, 5, 6}, {4, 6, 7}, {0, 1, 5}, {0, 5, 4},
                                                 {1, 2, 6}, {1, 6, 5}, {2, 3, 7}, {2, 7, 6}, {3, 0, 4}, {3, 4, 7}};
                for (const auto& t : f)
                    m.mesh.add_triangle(c[t[0]], c[t[1]], c[t[2]]);
                with_ranges.push_back(std::move(m));
            }
        }
    if (!with_ranges.empty() && given_modifiers)
        with_ranges.insert(with_ranges.end(), given_modifiers->begin(), given_modifiers->end());
    if (ranged || !with_ranges.empty()) {
        // The ranges' layers, the ranges as modifiers (first in the list: the
        // modifiers, later, win), and the objects without their ranges.
        std::vector<MeshObject> plain = *given;
        for (MeshObject& o : plain)
            o.ranges.clear();
        SlicedModel ranged_out = slice_model(input, ranged ? *ranged : settings, gpu, stress, painting, &plain, mixes,
                                             negative_volumes, with_ranges.empty() ? given_modifiers : &with_ranges);
        ranged_out.notices.insert(ranged_out.notices.begin(), out.notices.begin(), out.notices.end());
        return ranged_out;
    }
    const std::vector<MeshModifier>* modifiers =
        given_modifiers && !given_modifiers->empty() ? given_modifiers : nullptr;
    // Modifiers change parts of objects: without a list, the plate is object 0.
    std::vector<MeshObject> whole;
    if (modifiers && (!given || given->empty())) {
        whole.push_back({"object", 0, input.triangles.size(), 0, 0});
        given = &whole;
    }
    const IndexedMesh* negative =
        negative_volumes && !negative_volumes->triangles.empty() ? negative_volumes : nullptr;
    // A belt printer (belt_printer.md): the part turned so that the planes
    // along the gantry lie flat, sliced as on a bed; the writer puts the
    // G-code on the machine's axes. No skirt (rule 4).
    if (settings.belt_enabled) {
        Settings flat = settings;
        flat.belt_enabled = false;
        flat.skirt_loops = 0;
        if (modifiers)
            throw std::invalid_argument("belt printers do not take modifiers yet");
        if (negative) {
            const IndexedMesh turned = belt_mesh(*negative, settings);
            return slice_model(belt_mesh(input, settings), flat, gpu, stress, painting, given, mixes, &turned);
        }
        return slice_model(belt_mesh(input, settings), flat, gpu, stress, painting, given, mixes);
    }
    // Curved layers (nonplanar.md): the mesh, split fine enough to bend,
    // moved to z - D and sliced flat; the G-code bends it back. The objects
    // keep their triangles, each one's pieces in its place.
    if (settings.curved_mode != CurvedMode::Disabled) {
        if (painting && !painting->empty())
            throw CurvedError("curved layers do not take painted faces yet (curved.mode)");
        if (modifiers)
            throw CurvedError("curved layers do not take modifiers yet (curved.mode)");
        if (negative)
            throw CurvedError("curved layers do not take negative volumes yet (curved.mode)");
        std::optional<CurvedField> field = curved_field(settings, input);
        if (!field) { // nothing to bend: flat layers (nonplanar_modes.md, CO1)
            Settings flat = settings;
            flat.curved_mode = CurvedMode::Disabled;
            return slice_model(input, flat, gpu, stress, painting, given, mixes, negative);
        }
        const double longest =
            field->mode == CurvedMode::Wave ? std::clamp(2.0 * std::numbers::pi / field->k / 20.0, 0.25, 1.0) : 1.0;
        std::vector<std::uint32_t> origin;
        IndexedMesh bent = refined_mesh(input, longest, field->flat_below, &origin);
        flatten_mesh(bent, *field);
        std::vector<MeshObject> pieces;
        if (given) {
            // Where each object's first triangle's pieces start, and how many.
            std::vector<std::size_t> first(input.triangles.size() + 1, origin.size());
            for (std::size_t t = origin.size(); t-- > 0;)
                first[origin[t]] = t;
            for (std::size_t t = input.triangles.size(); t-- > 0;)
                first[t] = std::min(first[t], first[t + 1]);
            pieces = *given;
            for (MeshObject& o : pieces) {
                const std::size_t a = first[std::min(o.first, input.triangles.size())];
                const std::size_t b = first[std::min(o.first + o.count, input.triangles.size())];
                o.first = a, o.count = b - a;
            }
        }
        Settings flat = settings;
        flat.curved_mode = CurvedMode::Disabled;
        out = slice_model(bent, flat, gpu, stress, nullptr, given ? &pieces : nullptr, mixes);
        out.curved = std::move(field);
        return out;
    }
    // Each object's extruder: one the machine has (the first with one), or
    // the number of a blend it can print (color_mixing.md, rule 5).
    std::vector<ColorMix> blends;
    if (mixes && settings.extruders > 1)
        for (const ColorMix& m : *mixes)
            if (mix_usable(m, settings.extruders))
                blends.push_back(m);
    std::vector<MeshObject> resolved;
    if (given) {
        resolved = *given;
        for (MeshObject& o : resolved)
            if (std::none_of(blends.begin(), blends.end(), [&](const ColorMix& m) { return m.id == o.extruder; }))
                o.extruder = std::clamp(o.extruder, 1, std::max(settings.extruders, 1));
    }
    const std::vector<MeshObject>* objects = given ? &resolved : nullptr;
    // The plate's objects: each one sliced apart when printed one after
    // another (sequential.md, rule 1), else the plate as one, with their
    // outlines kept.
    if (objects && !objects->empty()) {
        if (settings.sequential && objects->size() > 1) {
            out.sequential = true;
            for (const MeshObject& o : *objects) {
                const IndexedMesh mesh = object_mesh(input, o);
                if (mesh.triangles.empty())
                    continue;
                const Painting paint = painting ? object_painting(*painting, o) : Painting{};
                const Settings own = own_settings(settings, o.settings, o.name);
                SlicedModel part = slice_plate(mesh, own, gpu, stress, painting ? &paint : nullptr, nullptr,
                                               object_tool(o, settings), negative);
                if (part.layers.empty())
                    continue;
                if (object_tool(o, settings) != 1)
                    tag_tool(part, object_tool(o, settings));
                apply_mixes(part, blends);
                SlicedObject object{o.name, o.object, o.instance, outline_of(mesh),
                                    part.layers.back().top - part.layers.front().bottom, 0, 0, {}, {},
                                    o.wipe_into_infill, o.wipe_into_object};
                append_object(out, std::move(part), std::move(object));
            }
            if (!out.objects.empty()) { // the skirt and brim of the first, for what reads them from the model
                out.skirt = out.objects.front().skirt;
                out.brim = out.objects.front().brim;
                out.raft_layers = out.objects.front().raft_layers;
            }
            return out;
        }
        // Each extruder's objects apart on the plate's layers
        // (multi_extruder.md, rule 1); one other than the first, every path its.
        if (several_tools(*objects, settings) || modifiers) {
            out = slice_plate(input, settings, gpu, stress, painting, objects, 1, negative, modifiers);
        } else {
            out = slice_plate(input, own_settings(settings, objects->front().settings, objects->front().name), gpu,
                              stress, painting, nullptr, object_tool(objects->front(), settings), negative);
            if (object_tool(objects->front(), settings) != 1)
                tag_tool(out, object_tool(objects->front(), settings));
        }
        apply_mixes(out, blends);
        for (const MeshObject& o : *objects) {
            const IndexedMesh mesh = object_mesh(input, o);
            if (mesh.triangles.empty())
                continue;
            float lo = mesh.vertices.front().z, hi = lo;
            for (const Vec3f& v : mesh.vertices)
                lo = std::min(lo, v.z), hi = std::max(hi, v.z);
            out.objects.push_back({o.name, o.object, o.instance, outline_of(mesh), double(hi) - double(lo), 0, out.raft_layers, {}, {},
                                   o.wipe_into_infill, o.wipe_into_object});
        }
        return out;
    }
    return slice_plate(input, settings, gpu, stress, painting, nullptr, 1, negative);
}

namespace {

SlicedModel slice_plate(const IndexedMesh& input, const Settings& settings, gpu::Context* gpu, const StressField* stress,
                        const Painting* painting, const std::vector<MeshObject>* tools, int tool,
                        const IndexedMesh* negative, const std::vector<MeshModifier>* modifiers) {
    // Several extruders print layer by layer, every extruder on every layer:
    // no zones of layer heights (multi_extruder.md; docs/CONFIG.md).
    if (settings.extruders > 1 && settings.detail_height > 0.0) {
        Settings flat = settings;
        flat.detail_height = 0.0;
        return slice_plate(input, flat, gpu, stress, painting, tools, tool, negative, modifiers);
    }
    // Vase mode (vase_mode.md, rule 1): one wall, no infill and no top
    // layers, whatever those settings say.
    if (settings.vase && (settings.wall_count != 1 || settings.infill_density != 0.0 || settings.top_layers != 0 ||
                          settings.top_min_thickness != 0.0)) {
        Settings vase = settings;
        vase.wall_count = 1;
        vase.infill_density = 0.0;
        vase.top_layers = 0;
        vase.top_min_thickness = 0.0;
        return slice_plate(input, vase, gpu, stress, painting, tools, tool, negative, modifiers);
    }
    // A raft (skirt_brim_raft.md, rules 9 to 14): the part sliced as if on
    // the bed, its first layer as thick as the others, without the first
    // layer's shrink or a brim, and the raft put under it.
    if (settings.raft_layers > 0) {
        Settings part = settings;
        part.raft_layers = 0;
        part.first_layer_height = settings.layer_height;
        part.first_layer_shrink = 0.0;
        part.brim_width = 0.0;
        SlicedModel raft = slice_plate(input, part, gpu, stress, painting, tools, tool, negative, modifiers);
        add_raft(raft, settings);
        return raft;
    }
    const IndexedMesh mesh = compensated(input, settings);
    std::optional<IndexedMesh> cut;
    if (negative)
        cut = compensated_like(*negative, input, settings);
    if (!tools)
        return slice_layers(mesh, settings, gpu, stress, painting, nullptr, tool, cut ? &*cut : nullptr);
    return slice_tools(mesh, settings, gpu, stress, painting, *tools, cut ? &*cut : nullptr, modifiers,
                       &input);
}

// A part's regions on given layers, as slice_layers makes them before its
// walls: its level contours, the fill mode, the XY offset, the first layer's
// shrink and the holes' offset.
std::vector<Polygons> part_regions(IndexedMesh mesh, const Settings& settings, const std::vector<Layer>& layers,
                                   gpu::Context* gpu) {
    orient_consistently(mesh);
    std::vector<float> heights;
    for (const Layer& l : layers)
        heights.push_back(float(l.slice_z()));
    std::vector<char> same;
    const std::vector<std::vector<Contour>> contours =
        slice_levels(mesh, height_field(mesh), heights, settings.gap_closing_radius, gpu, &same);
    std::vector<std::size_t> source(layers.size());
    for (std::size_t l = 0; l < layers.size(); ++l)
        source[l] = l > 0 && same[l] ? source[l - 1] : l;
    std::vector<Polygons> regions(layers.size());
    tbb::parallel_for(std::size_t(0), layers.size(), [&](std::size_t l) {
        const Polygons region = layer_region(simplify(to_polygons(contours[source[l]]), settings.simplify_tolerance),
                                             settings.fill_mode);
        const double shrink = l == 0 ? settings.first_layer_shrink : 0.0;
        regions[l] = offset_holes(offset(region, settings.xy_offset - shrink), settings.hole_offset);
    });
    return regions;
}

// Beams between two materials (multi_extruder.md, rule 17; own design): on
// each layer, what lies within interlocking.depth beam widths of the
// border between two parts, and that far from the outside, is cut in
// stripes interlocking.beam_width wide at interlocking.orientation, turned
// 90 degrees every interlocking.beam_layers layers; the stripes go to the
// two parts in turn, so each part's beams go into the other.
void interlock(std::vector<std::vector<Polygons>>& regions, const std::vector<Layer>& layers, const Settings& s) {
    const double w = s.interlock_width;
    const double depth = s.interlock_depth * w, avoid = s.interlock_avoidance * w;
    tbb::parallel_for(std::size_t(0), layers.size(), [&](std::size_t l) {
        const double angle = (s.interlock_orientation + (l / std::size_t(std::max(s.interlock_layers, 1)) % 2 ? 90.0 : 0.0)) *
                             std::numbers::pi / 180.0;
        const double c = std::cos(angle), sn = std::sin(angle);
        for (std::size_t a = 0; a < regions.size(); ++a)
            for (std::size_t b = a + 1; b < regions.size(); ++b) {
                Polygons& ra = regions[a][l];
                Polygons& rb = regions[b][l];
                if (ra.empty() || rb.empty())
                    continue;
                Polygons zone = Clipper2Lib::Union(Clipper2Lib::Intersect(ra, offset(rb, depth), Clipper2Lib::FillRule::NonZero),
                                                   Clipper2Lib::Intersect(rb, offset(ra, depth), Clipper2Lib::FillRule::NonZero),
                                                   Clipper2Lib::FillRule::NonZero);
                if (avoid > 0.0)
                    zone = Clipper2Lib::Intersect(zone, offset(Clipper2Lib::Union(ra, rb, Clipper2Lib::FillRule::NonZero), -avoid),
                                                  Clipper2Lib::FillRule::NonZero);
                if (zone.empty())
                    continue;
                // The zone's box across the stripes: v along c, s turned.
                double lo_u = 1e300, hi_u = -1e300, lo_v = 1e300, hi_v = -1e300;
                for (const Polygon& p : zone)
                    for (const auto& q : p) {
                        const double x = to_mm(q.x), y = to_mm(q.y);
                        const double u = c * x + sn * y, v = -sn * x + c * y;
                        lo_u = std::min(lo_u, u), hi_u = std::max(hi_u, u), lo_v = std::min(lo_v, v), hi_v = std::max(hi_v, v);
                    }
                Polygons even, odd;
                for (auto k = static_cast<long long>(std::floor(lo_v / w)); double(k) * w < hi_v; ++k) {
                    const double v0 = double(k) * w, v1 = v0 + w;
                    Polygon stripe;
                    for (const auto& [u, v] : {std::pair{lo_u - w, v0}, std::pair{hi_u + w, v0}, std::pair{hi_u + w, v1},
                                               std::pair{lo_u - w, v1}})
                        stripe.push_back({to_units(c * u - sn * v), to_units(sn * u + c * v)});
                    (k % 2 == 0 ? even : odd).push_back(std::move(stripe));
                }
                const Polygons to_a = Clipper2Lib::Intersect(zone, even, Clipper2Lib::FillRule::NonZero);
                const Polygons to_b = Clipper2Lib::Intersect(zone, odd, Clipper2Lib::FillRule::NonZero);
                ra = Clipper2Lib::Union(Clipper2Lib::Difference(ra, zone, Clipper2Lib::FillRule::NonZero), to_a,
                                        Clipper2Lib::FillRule::NonZero);
                rb = Clipper2Lib::Union(Clipper2Lib::Difference(rb, zone, Clipper2Lib::FillRule::NonZero), to_b,
                                        Clipper2Lib::FillRule::NonZero);
            }
    });
}

// Several extruders (multi_extruder.md, rule 1): the whole plate sliced
// once for its outline on every layer, its support, skirt and brim; then
// each extruder's objects (the first extruder first) on the same layers,
// and their walls and fills, each path marked with its extruder, in place
// of the plate's.
SlicedModel slice_tools(const IndexedMesh& mesh, const Settings& settings, gpu::Context* gpu, const StressField* stress,
                        const Painting* painting, const std::vector<MeshObject>& objects, const IndexedMesh* negative,
                        const std::vector<MeshModifier>* modifiers, const IndexedMesh* uncompensated) {
    SlicedModel out = slice_layers(mesh, settings, gpu, stress, painting, nullptr, 1, negative);
    if (out.layers.empty())
        return out;
    PartFrame frame;
    {
        IndexedMesh oriented = mesh;
        orient_consistently(oriented); // as slice_layers plans the plate's layers
        float min_z = oriented.vertices.front().z, max_z = min_z;
        float min_x = oriented.vertices.front().x, max_x = min_x, min_y = oriented.vertices.front().y, max_y = min_y;
        for (const Vec3f& v : oriented.vertices) {
            min_z = std::min(min_z, v.z), max_z = std::max(max_z, v.z);
            min_x = std::min(min_x, v.x), max_x = std::max(max_x, v.x);
            min_y = std::min(min_y, v.y), max_y = std::max(max_y, v.y);
        }
        frame.stack = plan_layer_stack(oriented, min_z, max_z, settings);
        frame.centre = {0.5 * (double(min_x) + double(max_x)), 0.5 * (double(min_y) + double(max_y))};
    }
    const std::size_t n = out.layers.size();
    std::vector<Polygons> taken(n);
    frame.cover = &out.regions;
    frame.taken = &taken;
    frame.support = &out.support_areas;
    Settings part_settings = settings;
    part_settings.support = false;
    // The parts to slice apart: the objects of each extruder with the same
    // settings of their own (model_tools.md, rules 5, 7 and 8). The first
    // extruder first; with one extruder, the group whose part comes later in
    // the list first, so that it keeps what it shares with an earlier one
    // (MT11).
    struct Group {
        int tool = 1;
        std::string settings, part; // the merged settings, and the part's own (for the ranges)
        std::size_t last = 0;
    };
    std::vector<Group> groups;
    for (std::size_t i = 0; i < objects.size(); ++i) {
        const int tool = object_tool(objects[i], settings);
        const auto g = std::find_if(groups.begin(), groups.end(), [&](const Group& x) {
            return x.tool == tool && x.settings == objects[i].settings && x.part == objects[i].part_settings;
        });
        if (g == groups.end())
            groups.push_back({tool, objects[i].settings, objects[i].part_settings, i});
        else
            g->last = i;
    }
    std::sort(groups.begin(), groups.end(), [](const Group& a, const Group& b) {
        return a.tool != b.tool ? a.tool < b.tool : a.last > b.last;
    });
    std::vector<int> tools;
    std::vector<Settings> group_settings;
    for (const Group& g : groups) {
        tools.push_back(g.tool);
        const MeshObject& named = objects[g.last];
        Settings own = own_settings(part_settings, g.settings, named.name);
        group_settings.push_back(std::move(own));
    }
    std::vector<int> distinct = tools;
    std::sort(distinct.begin(), distinct.end());
    distinct.erase(std::unique(distinct.begin(), distinct.end()), distinct.end());
    // The combined infill (docs/INFILL.md) in the same runs for every part,
    // as thick as the finest of their infill nozzles holds.
    if (settings.infill_combine_auto || settings.infill_combine_layers > 1 || settings.fine_walls_coarse_infill) {
        double most = std::numeric_limits<double>::infinity();
        for (const int tool : distinct) {
            const PartNozzles nozzles = part_nozzles(settings, tool);
            if (nozzles.settings.infill_combine_auto || nozzles.settings.infill_combine_layers > 1)
                most = std::min(most, combine_max_height(nozzles.settings, nozzles.infill_nozzle));
        }
        if (std::isfinite(most)) {
            part_settings.infill_combine_max_height = {most, false};
            for (Settings& g : group_settings)
                g.infill_combine_max_height = {most, false};
        }
    }
    std::vector<LayerWalls> walls(n);
    std::vector<LayerFill> fills(n);
    std::vector<ThickLayer> thick;
    std::vector<LayerSurfaces> surfaces(n);
    std::vector<Polygons> hanging(n), on_support(n), air_bridges(n);
    std::vector<double> support_gap(n, 0.0);
    std::size_t zaa_points = 0;
    const auto add = [](std::vector<Toolpath>& to, std::vector<Toolpath>& from) {
        to.insert(to.end(), std::make_move_iterator(from.begin()), std::make_move_iterator(from.end()));
    };
    const auto unite = [](Polygons& to, const Polygons& from) {
        if (!from.empty())
            to = to.empty() ? from : Clipper2Lib::Union(to, from, Clipper2Lib::FillRule::NonZero);
    };
    std::vector<IndexedMesh> part_meshes(tools.size());
    std::vector<Painting> part_paints(tools.size());
    for (std::size_t p = 0; p < tools.size(); ++p)
        for (const MeshObject& o : objects) {
            if (object_tool(o, settings) != tools[p] || o.settings != groups[p].settings ||
                o.part_settings != groups[p].part)
                continue;
            const IndexedMesh piece = object_mesh(mesh, o);
            if (painting)
                part_paints[p].append(object_painting(*painting, o), part_meshes[p].triangles.size());
            const auto base = std::uint32_t(part_meshes[p].vertices.size());
            part_meshes[p].vertices.insert(part_meshes[p].vertices.end(), piece.vertices.begin(), piece.vertices.end());
            for (const auto& t : piece.triangles)
                part_meshes[p].triangles.push_back({t[0] + base, t[1] + base, t[2] + base});
        }
    // The modifiers (model_tools.md, rules 3 and 5): where one meets the
    // parts of its object in a group, a group of its own with its settings
    // over the group's, its regions that zone. They go first, the later
    // modifier first, so that they keep their zone (MT12).
    std::vector<std::vector<Polygons>> zone(tools.size());
    std::vector<std::vector<InfillZone>> infill_zones(tools.size()); // by the parts' groups
    // A zone's settings (rule 5): a modifier's over the part's, a height
    // range's between the object's and the part's own.
    const auto zone_own = [](const Group& g, const MeshModifier& m) {
        return m.under_parts ? merged_own(merged_own(g.settings, m.settings), g.part) : merged_own(g.settings, m.settings);
    };
    if (modifiers && uncompensated) {
        Settings bare = settings; // the modifier's outline as it is, no offsets
        bare.xy_offset = 0.0, bare.hole_offset = 0.0, bare.first_layer_shrink = 0.0;
        std::vector<int> zone_tools;
        std::vector<Settings> zone_settings;
        std::vector<IndexedMesh> zone_meshes;
        std::vector<std::vector<Polygons>> zones;
        for (std::size_t mi = modifiers->size(); mi-- > 0;) {
            MeshModifier m = (*modifiers)[mi];
            // The layer height is the object's (rule 4, MT6): left out, and said so.
            bool layers = false;
            m.settings = own_without(m.settings, "layers", &layers);
            if (layers)
                out.notices.push_back(m.name + ": the layer height is the object's, not a modifier's (model_tools.md, rule 4)");
            const IndexedMesh modifier = compensated_like(weld_exact(m.mesh), *uncompensated, settings);
            if (modifier.triangles.empty())
                continue;
            const std::vector<Polygons> reach = part_regions(modifier, bare, frame.stack.layers, gpu);
            // Only the infill: no zone of its own, the groups' infill changes there (MT4).
            bool other = false;
            const std::string infill_only = own_without(m.settings, "infill", &other);
            if (other && infill_only.empty()) {
                for (std::size_t p = 0; p < groups.size(); ++p)
                    if (std::any_of(objects.begin(), objects.end(), [&](const MeshObject& o) {
                            return o.object == m.object && object_tool(o, settings) == tools[p] &&
                                   o.settings == groups[p].settings && o.part_settings == groups[p].part;
                        }))
                        infill_zones[p].push_back(
                            {reach, own_settings(part_settings, zone_own(groups[p], m), m.name)});
                continue;
            }
            for (std::size_t p = 0; p < groups.size(); ++p) {
                // This group's parts of the modifier's object.
                IndexedMesh parts;
                for (const MeshObject& o : objects) {
                    if (o.object != m.object || object_tool(o, settings) != tools[p] || o.settings != groups[p].settings ||
                        o.part_settings != groups[p].part)
                        continue;
                    const IndexedMesh piece = object_mesh(mesh, o);
                    const auto base = std::uint32_t(parts.vertices.size());
                    parts.vertices.insert(parts.vertices.end(), piece.vertices.begin(), piece.vertices.end());
                    for (const auto& t : piece.triangles)
                        parts.triangles.push_back({t[0] + base, t[1] + base, t[2] + base});
                }
                if (parts.triangles.empty())
                    continue;
                std::vector<Polygons> shared = part_regions(parts, group_settings[p], frame.stack.layers, gpu);
                bool any = false;
                for (std::size_t l = 0; l < n; ++l) {
                    shared[l] = l < reach.size() && !reach[l].empty() && !shared[l].empty()
                                    ? Clipper2Lib::Intersect(shared[l], reach[l], Clipper2Lib::FillRule::NonZero)
                                    : Polygons{};
                    any = any || !shared[l].empty();
                }
                if (!any)
                    continue;
                zone_tools.push_back(tools[p]);
                zone_settings.push_back(own_settings(part_settings, zone_own(groups[p], m), m.name));
                zone_settings.back().infill_combine_max_height = group_settings[p].infill_combine_max_height;
                zone_meshes.push_back(std::move(parts));
                zones.push_back(std::move(shared));
            }
        }
        // In front of the parts' groups.
        tools.insert(tools.begin(), zone_tools.begin(), zone_tools.end());
        group_settings.insert(group_settings.begin(), std::make_move_iterator(zone_settings.begin()),
                              std::make_move_iterator(zone_settings.end()));
        part_meshes.insert(part_meshes.begin(), std::make_move_iterator(zone_meshes.begin()),
                           std::make_move_iterator(zone_meshes.end()));
        part_paints.insert(part_paints.begin(), zone_tools.size(), Painting{});
        zone.insert(zone.begin(), std::make_move_iterator(zones.begin()), std::make_move_iterator(zones.end()));
        infill_zones.insert(infill_zones.begin(), zone_tools.size(), std::vector<InfillZone>{});
    }
    // Beams between the materials (rule 17): every part's regions first.
    std::vector<std::vector<Polygons>> beamed;
    // Only between materials, each extruder one part.
    if (settings.interlock_beams && distinct.size() > 1 && distinct.size() == tools.size()) {
        beamed.resize(tools.size());
        for (std::size_t p = 0; p < tools.size(); ++p)
            beamed[p] = part_meshes[p].triangles.empty() ? std::vector<Polygons>(n)
                                                         : part_regions(part_meshes[p], settings, frame.stack.layers, gpu);
        interlock(beamed, frame.stack.layers, settings);
    }
    for (std::size_t p = 0; p < tools.size(); ++p) {
        const int tool = tools[p];
        const IndexedMesh& part_mesh = part_meshes[p];
        if (part_mesh.triangles.empty())
            continue;
        frame.regions = !zone[p].empty() ? &zone[p] : beamed.empty() ? nullptr : &beamed[p];
        frame.infill_zones = infill_zones[p].empty() ? nullptr : &infill_zones[p];
        SlicedModel part =
            slice_layers(part_mesh, group_settings[p], gpu, stress,
                         painting && !part_paints[p].empty() ? &part_paints[p] : nullptr, &frame, tool, negative);
        tag_tool(part, tool);
        zaa_points += part.zaa_points;
        // Their combined infill, in the same runs (above).
        if (!part.thick.empty()) {
            thick.resize(n);
            for (std::size_t l = 0; l < std::min(n, part.thick.size()); ++l) {
                ThickLayer& to = thick[l];
                ThickLayer& from = part.thick[l];
                if (from.empty())
                    continue;
                to.height = from.height;
                add(to.walls, from.walls);
                add(to.fill.sparse, from.fill.sparse), add(to.fill.solid, from.fill.solid), add(to.fill.top, from.fill.top);
                add(to.fill.bridge, from.fill.bridge), add(to.fill.gaps, from.fill.gaps);
            }
        }
        tbb::parallel_for(std::size_t(0), std::min(n, part.layers.size()), [&](std::size_t l) {
            unite(taken[l], part.regions[l]);
            LayerWalls& w = part.walls[l];
            add(walls[l].toolpaths, w.toolpaths);
            add(walls[l].gaps, w.gaps);
            unite(walls[l].infill, w.infill);
            LayerFill& f = part.fills[l];
            for (auto [to, from] : {std::pair{&fills[l].top, &f.top}, {&fills[l].solid, &f.solid}, {&fills[l].sparse, &f.sparse},
                                    {&fills[l].bridge, &f.bridge}, {&fills[l].gaps, &f.gaps}, {&fills[l].arcs, &f.arcs},
                                    {&fills[l].overhang_walls, &f.overhang_walls}, {&fills[l].ironing, &f.ironing}})
                add(*to, *from);
            const LayerSurfaces& s = part.surfaces[l];
            if (surfaces[l].bridge.empty() && !s.bridge.empty())
                surfaces[l].bridge_angle = s.bridge_angle;
            unite(surfaces[l].top, s.top);
            unite(surfaces[l].top_under, s.top_under);
            unite(surfaces[l].bottom, s.bottom);
            unite(surfaces[l].sparse, s.sparse);
            unite(surfaces[l].bridge, s.bridge);
            unite(hanging[l], part.hanging[l]);
            unite(on_support[l], part.on_support[l]);
            unite(air_bridges[l], part.air_bridges[l]);
            support_gap[l] = std::max(support_gap[l], part.support_gap[l]);
        });
    }
    // The plate's support stays, printed by its own extruder.
    for (std::size_t l = 0; l < n; ++l) {
        fills[l].support = std::move(out.fills[l].support);
        fills[l].support_interface = std::move(out.fills[l].support_interface);
    }
    out.walls = std::move(walls);
    out.fills = std::move(fills);
    out.thick = std::move(thick);
    out.surfaces = std::move(surfaces);
    out.hanging = std::move(hanging);
    out.on_support = std::move(on_support);
    out.air_bridges = std::move(air_bridges);
    out.support_gap = std::move(support_gap);
    out.zaa_points = zaa_points;
    return out;
}

SlicedModel slice_layers(const IndexedMesh& compensated_mesh, const Settings& given, gpu::Context* gpu,
                         const StressField* stress, const Painting* painting, const PartFrame* frame, int tool,
                         const IndexedMesh* negative) {
    const PartNozzles nozzles = part_nozzles(given, tool);
    const Settings& settings = nozzles.settings;
    SlicedModel out;
    IndexedMesh mesh = compensated_mesh;
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
    const LayerStack stack = frame ? frame->stack : plan_layer_stack(mesh, min_z, max_z, settings);
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
    // The negative volumes' sections on the same levels (model_tools.md, rule
    // 2): taken away from every layer they cross; the layers stay the part's.
    std::vector<Polygons> cut;
    if (negative) {
        IndexedMesh oriented = *negative;
        orient_consistently(oriented);
        std::vector<char> cut_same;
        const std::vector<std::vector<Contour>> cut_contours =
            slice_levels(oriented, height_field(oriented), heights, settings.gap_closing_radius, gpu, &cut_same);
        // A level like the one below comes back empty: it takes that one's.
        std::vector<std::size_t> from(heights.size());
        for (std::size_t l = 0; l < heights.size(); ++l)
            from[l] = l > 0 && cut_same[l] ? from[l - 1] : l;
        cut.resize(heights.size());
        tbb::parallel_for(std::size_t(0), heights.size(), [&](std::size_t l) {
            cut[l] = layer_region(to_polygons(cut_contours[from[l]]), FillMode::Union);
        });
    }

    out.regions.resize(out.layers.size());
    out.walls.resize(out.layers.size());
    BeadingParams beading;
    beading.preferred_width = settings.wall_width;
    beading.outer_width = settings.outer_wall_width;
    beading.wall_count = settings.wall_count;
    beading.min_detail = settings.wall_min_detail.of(settings.nozzle_diameter);
    beading.min_width = settings.wall_min_line_width.of(settings.nozzle_diameter);
    std::vector<std::size_t> open(out.layers.size(), 0);
    std::vector<char> cut_away(out.layers.size(), 0);
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
        copied[l] = same && shrink_at(l) == shrink_at(l - 1) && !fuzzed(l) && !fuzzed(l - 1) &&
                    (cut.empty() || cut[l] == cut[l - 1]) &&
                    !(l == 1 && settings.first_layer_one_wall) &&
                    !(frame && frame->taken && (*frame->taken)[l] != (*frame->taken)[l - 1]) &&
                    !(frame && frame->regions && (*frame->regions)[l] != (*frame->regions)[l - 1]);
    }
    tbb::parallel_for(std::size_t(0), out.layers.size(), [&](std::size_t l) {
        if (copied[l])
            return;
        for (const Contour& c : contours[source[l]])
            open[l] += c.closed ? 0 : 1;
        Polygons region = layer_region(simplify(to_polygons(contours[source[l]]), settings.simplify_tolerance),
                                       settings.fill_mode);
        if (!cut.empty() && !cut[l].empty() && !region.empty()) {
            region = Clipper2Lib::Difference(region, cut[l], Clipper2Lib::FillRule::NonZero);
            cut_away[l] = region.empty();
        }
        const double shrink = shrink_at(l);
        out.regions[l] = offset_holes(offset(region, settings.xy_offset - shrink), settings.hole_offset);
        if (frame && frame->regions) {
            out.regions[l] = (*frame->regions)[l];
            // Regions given from outside (a modifier's zone) lose the negative volumes too.
            if (!cut.empty() && !cut[l].empty() && !out.regions[l].empty())
                out.regions[l] = Clipper2Lib::Difference(out.regions[l], cut[l], Clipper2Lib::FillRule::NonZero);
        }
        // Where an earlier extruder's part is, this one is not.
        if (frame && frame->taken && !(*frame->taken)[l].empty())
            out.regions[l] = Clipper2Lib::Difference(out.regions[l], (*frame->taken)[l], Clipper2Lib::FillRule::NonZero);
        // What an opening by the thinnest bead removes is too thin to print.
        const Polygons printable = offset(offset(out.regions[l], -thinnest / 2.0), thinnest / 2.0);
        out.thin_area[l] = area_mm2(Clipper2Lib::Difference(out.regions[l], printable, Clipper2Lib::FillRule::NonZero));
        if (settings.wall_count > 0) {
            // Only the outer wall on the first layer (walls_extra.md, rule 11).
            out.walls[l] = walls_of(out.regions[l], l == 0 && settings.first_layer_one_wall ? 1 : settings.wall_count,
                                    settings, beading);
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
    // What holds a layer up, and what its skins see: with several extruders,
    // every part's (the skins their own with surfaces.interface_shells).
    const std::vector<Polygons>& held = frame && frame->cover ? *frame->cover : out.regions;
    const std::vector<Polygons>& skins = settings.interface_shells ? out.regions : held;
    // Only the outer wall over the top surfaces (walls_extra.md, rule 10):
    // every one, or the part's topmost (its last layer).
    if (settings.top_one_wall != TopOneWall::None && settings.wall_count > 1 && !out.layers.empty()) {
        const std::size_t last = out.layers.size() - 1;
        const Polygons nothing;
        tbb::parallel_for(settings.top_one_wall == TopOneWall::Topmost ? last : std::size_t(0), last + 1, [&](std::size_t l) {
            if (out.regions[l].empty() || (l == 0 && settings.first_layer_one_wall))
                return;
            std::optional<LayerWalls> walls =
                one_wall_on_top(out.regions[l], l < last ? skins[l + 1] : nothing, settings, beading);
            if (!walls)
                return;
            walls->infill = simplify(walls->infill, settings.simplify_tolerance);
            fuzz_walls(walls->toolpaths, out.regions[l], fuzzy,
                       l < fuzzy_painted.size() && !fuzzy_painted[l].empty() ? &fuzzy_painted[l] : nullptr, l);
            out.walls[l] = std::move(*walls);
        });
    }
    for (std::size_t n : open)
        out.open_contours += n;
    out.cut_away = std::size_t(std::count(cut_away.begin(), cut_away.end(), 1));
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
                       ? classify_surfaces(skins, infill, skin_counts(stack.share, top_count, +1),
                                           skin_counts(stack.share, bottom_count, -1), settings.wall_width)
                       : classify_surfaces(skins, infill, top_count * ratio, bottom_count * ratio,
                                           settings.wall_width);
    // What the other extruders' parts lay counts as printed below.
    std::vector<Polygons> others;
    if (frame && frame->cover) {
        others.resize(out.layers.size());
        tbb::parallel_for(std::size_t(0), out.layers.size(), [&](std::size_t l) {
            others[l] = Clipper2Lib::Difference(held[l], out.regions[l], Clipper2Lib::FillRule::NonZero);
        });
    }
    find_bridges(out.surfaces, out.regions, out.walls, settings.infill_density > 0.0, settings.wall_width,
                 others.empty() ? nullptr : &others);
    out.air_bridges.resize(out.surfaces.size());
    for (std::size_t l = 0; l < out.surfaces.size(); ++l)
        out.air_bridges[l] = out.surfaces[l].bridge;
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
    // Arcs over the overhangs held from one side (arc_overhangs.md): they take
    // the place of the zone's fill, and of the bite of what holds it.
    std::vector<std::vector<Toolpath>> arc_paths(out.layers.size());
    std::vector<Polygons> arc_areas(out.layers.size());
    if (settings.arcs) {
        ArcParams ap;
        ap.width = settings.wall_width;
        ap.overlap = settings.arcs_overlap;
        ap.max_distance = settings.arcs_max_distance;
        tbb::parallel_for(std::size_t(1), out.layers.size(), [&](std::size_t l) {
            if (out.regions[l].empty() || held[l - 1].empty())
                return;
            // Round beads, as the bridges' (rule 3): pi/4 of the width squared.
            const double round = std::numbers::pi / 4.0 * settings.wall_width - (out.layers[l].top - out.layers[l].bottom);
            for (ArcZone& z : overhang_arcs(out.regions[l], held[l - 1], ap)) {
                for (Toolpath& t : z.arcs) {
                    t.gap = round;
                    t.retract = true;
                    arc_paths[l].push_back(std::move(t));
                }
                arc_areas[l].insert(arc_areas[l].end(), z.area.begin(), z.area.end());
            }
            if (arc_areas[l].empty())
                return;
            LayerSurfaces& surface = out.surfaces[l];
            for (Polygons* part : {&surface.top, &surface.top_under, &surface.bottom, &surface.sparse, &surface.bridge})
                *part = Clipper2Lib::Difference(*part, arc_areas[l], Clipper2Lib::FillRule::NonZero);
        });
    }
    // Extra perimeters on overhangs (walls_extra.md, rule 5): a zone held from
    // one side only, where no arcs go, is filled with loops from its outline
    // in, inside the walls, as overhang perimeters on round beads; they take
    // the place of its fill.
    std::vector<std::vector<Toolpath>> overhang_walls(out.layers.size());
    if (settings.extra_on_overhangs && !settings.arcs) {
        const double w = settings.wall_width;
        tbb::parallel_for(std::size_t(1), out.layers.size(), [&](std::size_t l) {
            if (out.regions[l].empty() || held[l - 1].empty())
                return;
            const double round = std::max(0.0, std::numbers::pi / 4.0 * w - (out.layers[l].top - out.layers[l].bottom));
            Polygons taken;
            for (const HeldZone& z : held_zones(out.regions[l], held[l - 1], w)) {
                const Polygons area = Clipper2Lib::Intersect(z.zone, out.walls[l].infill, Clipper2Lib::FillRule::NonZero);
                if (area.empty())
                    continue;
                for (double d = w / 2.0;; d += w) {
                    const Polygons rings = offset(area, -d);
                    if (rings.empty())
                        break;
                    for (const Polygon& ring : rings) {
                        if (ring.size() < 3)
                            continue;
                        Toolpath t;
                        t.closed = true;
                        t.gap = round;
                        for (const auto& q : ring)
                            t.points.push_back({{to_mm(q.x), to_mm(q.y)}, w});
                        overhang_walls[l].push_back(std::move(t));
                    }
                }
                taken.insert(taken.end(), area.begin(), area.end());
            }
            if (taken.empty())
                return;
            LayerSurfaces& surface = out.surfaces[l];
            for (Polygons* part : {&surface.top, &surface.top_under, &surface.bottom, &surface.sparse, &surface.bridge})
                *part = Clipper2Lib::Difference(*part, taken, Clipper2Lib::FillRule::NonZero);
        });
    }
    out.support_areas.resize(out.layers.size());
    if (settings.support && !frame) {
        std::vector<double> thickness;
        for (const Layer& layer : out.layers)
            thickness.push_back(layer.top - layer.bottom);
        SupportParams sp;
        sp.threshold_angle = settings.support_angle;
        sp.z_gap_layers = int(std::lround(settings.support_z_gap / stack_height));
        sp.bottom_gap_layers = settings.support_bottom_z_gap > 0.0
                                   ? int(std::lround(settings.support_bottom_z_gap / stack_height))
                                   : sp.z_gap_layers;
        sp.buildplate_only = settings.support_buildplate_only;
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
    // Where the walls hang (walls_extra.md, rule 8): past the layer below's
    // region grown by half a bead, a bead has nothing of it under it. And the
    // part's first layer over the support (supports_skirt.md, rule 4): what
    // hangs over the top of the support, a Z gap under the layer's bottom,
    // rests on it; its bridge reaches kBridgeAnchor over the part around it,
    // taken from the sparse infill, and the writer lays its walls and bridge
    // there as thick as the layer and the gap.
    // A part rests on the plate's support; it lays none itself.
    const std::vector<Polygons>& support_at = frame && frame->support ? *frame->support : out.support_areas;
    out.hanging.resize(out.layers.size());
    out.on_support.resize(out.layers.size());
    out.support_gap.assign(out.layers.size(), 0.0);
    const std::size_t support_gap_layers =
        std::size_t(std::max(0L, std::lround(settings.support_z_gap / stack_height)));
    tbb::parallel_for(std::size_t(1), out.layers.size(), [&](std::size_t l) {
        if (out.regions[l].empty())
            return;
        out.hanging[l] = Clipper2Lib::Difference(out.regions[l], offset(held[l - 1], settings.wall_width / 2.0),
                                                 Clipper2Lib::FillRule::NonZero);
        if (l < support_gap_layers + 1)
            return;
        const std::size_t top = l - 1 - support_gap_layers; // the support's last layer under this one
        if (support_at[top].empty())
            return;
        // The support keeps support.xy_gap off the part: that much and a bead
        // more of what hangs still lies over it.
        const double xy_gap = settings.support_xy_gap.of(settings.wall_width);
        Polygons on = Clipper2Lib::Intersect(
            Clipper2Lib::Difference(out.regions[l], held[l - 1], Clipper2Lib::FillRule::NonZero),
            offset(support_at[top], xy_gap + settings.wall_width), Clipper2Lib::FillRule::NonZero);
        // Arcs instead, where there are (arc_overhangs.md, rule 11).
        if (!arc_areas[l].empty())
            on = Clipper2Lib::Difference(on, arc_areas[l], Clipper2Lib::FillRule::NonZero);
        on = offset(offset(on, -settings.wall_width / 4.0), settings.wall_width / 4.0);
        if (on.empty())
            return;
        out.support_gap[l] = std::max(0.0, out.layers[l].bottom - out.layers[top].top);
        LayerSurfaces& surface = out.surfaces[l];
        const Polygons resting = Clipper2Lib::Intersect(surface.bridge, offset(on, settings.wall_width),
                                                        Clipper2Lib::FillRule::NonZero);
        if (!resting.empty() && !surface.sparse.empty()) {
            // The lines' ends, half a bead inside, kBridgeAnchor past the edge of the part below.
            const Polygons anchor = Clipper2Lib::Intersect(offset(on, kBridgeAnchor + settings.wall_width / 2.0),
                                                           surface.sparse, Clipper2Lib::FillRule::NonZero);
            surface.sparse = Clipper2Lib::Difference(surface.sparse, anchor, Clipper2Lib::FillRule::NonZero);
            surface.bridge = Clipper2Lib::Union(surface.bridge, anchor, Clipper2Lib::FillRule::NonZero);
        }
        out.on_support[l] = std::move(on);
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
    // The infill-only modifiers' zones (model_tools.md, rule 3, MT4): their
    // part of the sparse infill, the later modifier's first.
    const std::size_t zones = frame && frame->infill_zones ? frame->infill_zones->size() : 0;
    std::vector<std::vector<Polygons>> zone_region(zones, std::vector<Polygons>(out.layers.size()));
    if (zones > 0)
        tbb::parallel_for(std::size_t(0), out.layers.size(), [&](std::size_t l) {
            for (std::size_t z = 0; z < zones && !out.surfaces[l].sparse.empty(); ++z) {
                const InfillZone& zone = (*frame->infill_zones)[z];
                if (l >= zone.reach.size() || zone.reach[l].empty())
                    continue;
                Polygons r = Clipper2Lib::Intersect(out.surfaces[l].sparse, zone.reach[l], Clipper2Lib::FillRule::NonZero);
                if (r.empty())
                    continue;
                out.surfaces[l].sparse = Clipper2Lib::Difference(out.surfaces[l].sparse, r, Clipper2Lib::FillRule::NonZero);
                zone_region[z][l] = std::move(r);
            }
        });
    // Combined sparse infill (docs/INFILL.md): what every layer of a run has
    // as sparse infill prints once, at the run's top layer, as thick as the
    // run. Layer heights by zone already print it thick: not with them; nor
    // lightning, whose branches each layer draws from the one above.
    const std::vector<std::array<std::size_t, 2>> combined =
        stack.groups.empty() && settings.infill_density > 0.0 && !lightning
            ? combine_runs(out.layers, settings, nozzles.infill_nozzle)
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
    fill.bridge_width = bridge_bead(settings);
    fill.sparse_width = nozzles.sparse_width;
    // At 0 from the nozzle, not the walls (surfaces_infill.md, I9): the top
    // surface one nozzle wide, the other solid infill 1.125.
    fill.top_width = settings.top_width > 0.0 ? settings.top_width : settings.nozzle_diameter;
    fill.solid_width = settings.solid_width > 0.0 ? settings.solid_width : 1.125 * settings.nozzle_diameter;
    fill.wall_overlap = settings.infill_wall_overlap / 100.0;
    fill.depth = depth.empty() ? nullptr : &depth;
    fill.centre = frame ? frame->centre : Point2d{0.5 * (double(min_x) + double(max_x)), 0.5 * (double(min_y) + double(max_y))};
    // Lightning (docs/INFILL.md): from the top down, in what the stress
    // bands and the dense infill left of the sparse infill.
    std::vector<std::vector<Toolpath>> branches;
    if (lightning) {
        std::vector<Polygons> left(out.surfaces.size());
        for (std::size_t l = 0; l < left.size(); ++l)
            left[l] = out.surfaces[l].sparse;
        LightningParams lp;
        lp.width = nozzles.sparse_width;
        lp.reach = lightning_reach(nozzles.sparse_width, settings.infill_density / 100.0);
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
    if (!out.regions.empty() && !frame) {
        const double first = out.layers.front().top - out.layers.front().bottom;
        const AdhesionShape shape = adhesion_shape(settings);
        out.brim = brim_loops(out.regions.front(), shape, settings.wall_width, first);
        out.skirt = skirt_loops(out.regions.front(), shape, settings.wall_width, first);
    }
    out.fills.resize(out.layers.size());
    // A layer with nothing but its surfaces to fill, the same as the layer a
    // fill period below (fill_period), has its fill (prismatic parts; the
    // speed report's V10).
    const std::size_t period = fill_period(fill);
    const auto plain = [&](std::size_t l) {
        return l >= branches.size() && (l >= banded.size() || std::all_of(banded[l].begin(), banded[l].end(),
                                                                         [](const Polygons& b) { return b.empty(); })) &&
               dense_region[l].empty() && out.support_areas[l].empty() && out.on_support[l].empty() &&
               std::all_of(zone_region.begin(), zone_region.end(), [&](const std::vector<Polygons>& z) { return z[l].empty(); }) &&
               arc_paths[l].empty() && overhang_walls[l].empty();
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
                          round_bridges(out.fills[l].bridge, fill.bridge_width, out.layers[l].top - out.layers[l].bottom);
                          if (!out.on_support[l].empty())
                              rest_on_support(out.fills[l].bridge, out.on_support[l], out.support_gap[l],
                                              kBridgeAnchor + settings.wall_width);
                          out.fills[l].arcs = std::move(arc_paths[l]);
                          out.fills[l].overhang_walls = std::move(overhang_walls[l]);
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
                          for (std::size_t z = 0; z < zones; ++z) {
                              if (zone_region[z][l].empty())
                                  continue;
                              const Settings& own = (*frame->infill_zones)[z].settings;
                              FillParams zoned = fill;
                              zoned.density = own.infill_density / 100.0;
                              zoned.angle = own.infill_angle;
                              zoned.pattern = own.infill_pattern;
                              zoned.depth = nullptr;
                              LayerSurfaces only;
                              only.sparse = zone_region[z][l];
                              for (Toolpath& t : fill_layer(only, l, zoned, out.layers[l].slice_z()).sparse)
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
                          // And the support with the part below it within the next
                          // bottom_interface_layers layers under the bottom Z gap
                          // (supports_more.md, rule 7; grid support).
                          if (settings.support_style != SupportStyle::Organic) {
                              const std::size_t bottom_gap =
                                  settings.support_bottom_z_gap > 0.0
                                      ? std::size_t(std::max(0L, std::lround(settings.support_bottom_z_gap / stack_height)))
                                      : gap;
                              const int below_layers = settings.support_bottom_interface_layers < 0
                                                           ? settings.support_interface_layers
                                                           : settings.support_bottom_interface_layers;
                              for (int k = 1; k <= below_layers; ++k)
                                  if (const std::size_t b = bottom_gap + std::size_t(k); b <= l)
                                      above = Clipper2Lib::Union(above, out.regions[l - b], Clipper2Lib::FillRule::NonZero);
                          }
                          const Polygons interface = Clipper2Lib::Intersect(out.support_areas[l], above, Clipper2Lib::FillRule::NonZero);
                          const Polygons base = Clipper2Lib::Difference(out.support_areas[l], above, Clipper2Lib::FillRule::NonZero);
                          // Its own bead width (supports_more.md, rule 18): a
                          // percentage is of the layer height; 0, the walls'.
                          const double h = out.layers[l].top - out.layers[l].bottom;
                          const double width = settings.support_width.value <= 0.0 ? settings.wall_width
                                               : settings.support_width.percent ? settings.support_width.value / 100.0 * h
                                                                                : settings.support_width.value;
                          if (settings.support_style == SupportStyle::Organic) {
                              // Interface lines at 45 and 135 degrees in turn (supports_more.md, rule 17).
                              TreeFill tf = tree_fill(out.support_areas[l], above, width, settings.support_spacing,
                                                      settings.tree_double_wall_diameter, (l % 2 == 0) ? 0.0 : 90.0);
                              out.fills[l].support = std::move(tf.support);
                              out.fills[l].support_interface = std::move(tf.interface);
                              return;
                          }
                          // The base in lines along support.line_angle, support.spacing
                          // of free space apart; the interface across them, a
                          // bead's spacing apart (supports_skirt.md, rules 5 and 6).
                          const double bead = loop_spacing(width, h);
                          out.fills[l].support = rectilinear_fill(base, settings.support_spacing + bead, settings.support_line_angle,
                                                                  width, true);
                          out.fills[l].support_interface =
                              rectilinear_fill(interface, bead, settings.support_line_angle + 90.0, width, true);
                      });
    for (std::size_t l = period; l < out.layers.size(); ++l)
        if (repeats[l]) {
            out.fills[l] = out.fills[l - period];
            round_bridges(out.fills[l].bridge, fill.bridge_width, out.layers[l].top - out.layers[l].bottom);
        }
    // Ironing (ironing.md): lines `spacing` apart at 45 degrees to the layer's
    // top fill, as wide as spacing times flow so that each mm lays that share
    // of a layer's material (rule 3), over the whole exposed top, walls
    // included, half a nozzle from the edge (rule 4: 0.2 to 0.35 mm). Top:
    // what the layer above does not cover; topmost: what nothing above covers;
    // solid: every solid surface.
    if (settings.ironing) {
        const std::size_t n = out.layers.size();
        std::vector<Polygons> above; // topmost: everything above each layer
        if (settings.ironing_surfaces == IroningSurfaces::Topmost && n > 0) {
            above.resize(n);
            for (std::size_t l = n - 1; l-- > 0;)
                above[l] = Clipper2Lib::Union(above[l + 1], skins[l + 1], Clipper2Lib::FillRule::NonZero);
        }
        const double spacing = settings.ironing_spacing;
        // As ;WIDTH writes it (three decimals), so the material matches what
        // the G-code says: a tiny width loses much in the rounding otherwise.
        const double width = std::max(0.001, std::round(spacing * settings.ironing_flow / 100.0 * 1000.0) / 1000.0);
        const Polygons nothing;
        tbb::parallel_for(std::size_t(0), n, [&](std::size_t l) {
            Polygons area;
            if (settings.ironing_surfaces == IroningSurfaces::Solid) {
                const LayerSurfaces& s = out.surfaces[l];
                area = Clipper2Lib::Union(Clipper2Lib::Union(s.top, s.bottom, Clipper2Lib::FillRule::NonZero), s.bridge,
                                          Clipper2Lib::FillRule::NonZero);
            } else {
                const Polygons& cover = settings.ironing_surfaces == IroningSurfaces::Topmost ? above[l]
                                        : l + 1 < n                                         ? skins[l + 1]
                                                                                            : nothing;
                area = Clipper2Lib::Intersect(Clipper2Lib::Difference(out.regions[l], cover, Clipper2Lib::FillRule::NonZero),
                                              offset(out.regions[l], -settings.nozzle_diameter / 2.0),
                                              Clipper2Lib::FillRule::NonZero);
            }
            area = offset(offset(area, -spacing), spacing); // nothing narrower than two lines
            if (area.empty())
                return;
            const double angle = settings.infill_angle + (l % 2 == 1 ? 90.0 : 0.0) + 45.0;
            out.fills[l].ironing = rectilinear_fill(area, spacing, angle, width, true);
        });
    }
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

} // namespace
} // namespace tisma
