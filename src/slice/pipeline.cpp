// Planar slicing pipeline. Own design; see PROVENANCE.md.

#include "slice/pipeline.hpp"

#include <oneapi/tbb/parallel_for.h>

#include <algorithm>

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
    std::vector<std::size_t> open(out.layers.size(), 0);
    tbb::parallel_for(std::size_t(0), out.layers.size(), [&](std::size_t l) {
        for (const Contour& c : contours[l])
            open[l] += c.closed ? 0 : 1;
        Polygons region =
            layer_region(simplify(to_polygons(contours[l]), settings.simplify_tolerance), settings.fill_mode);
        const double shrink = l == 0 ? settings.first_layer_shrink : 0.0;
        out.regions[l] = offset(region, settings.xy_offset - shrink);
    });
    for (std::size_t n : open)
        out.open_contours += n;
    return out;
}

} // namespace tisma
