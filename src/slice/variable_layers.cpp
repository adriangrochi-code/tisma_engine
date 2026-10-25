// Variable layer height by Z. The cusp height is from Dolenc and Mäkelä 1994;
// the planning, the bands of Z and the sub-layers by slope are own design.
// See PROVENANCE.md.

#include "slice/variable_layers.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace tisma {

double SlopeLimit::allowed(double bottom, double top) const {
    if (limit.empty() || top <= bottom)
        return std::numeric_limits<double>::infinity();
    const double last = double(limit.size() - 1);
    const auto band = [&](double z) { return std::size_t(std::clamp(std::floor((z - z0) / bin), 0.0, last)); };
    // A layer that only touches a band at its edge does not take its limit.
    const std::size_t lo = band(bottom), hi = band(std::nextafter(top, bottom));
    float least = std::numeric_limits<float>::infinity();
    for (std::size_t b = lo; b <= hi; ++b)
        least = std::min(least, limit[b]);
    return double(least);
}

double SlopeLimit::tallest(double bottom, double most, int parts) const {
    if (limit.empty())
        return most;
    double least = std::numeric_limits<double>::infinity(), best = 0.0;
    for (auto b = std::ptrdiff_t(std::floor((bottom - z0) / bin));; ++b) {
        const double from = z0 + double(b) * bin - bottom, to = from + bin; // the band, from the bottom
        if (b >= 0 && b < std::ptrdiff_t(limit.size()))
            least = std::min(least, double(limit[std::size_t(b)]) * parts);
        if (least <= from)
            break; // the layer cannot reach into this band
        best = std::max(best, std::min({to, least, most}));
        if (to >= most || b >= std::ptrdiff_t(limit.size()))
            break;
    }
    return best;
}

SlopeLimit slope_limit(const IndexedMesh& mesh, double cusp_height, double max_height, double bin) {
    SlopeLimit out;
    out.bin = bin;
    if (mesh.vertices.empty() || !(cusp_height > 0.0))
        return out;
    float z0 = mesh.vertices.front().z, z1 = z0;
    for (const Vec3f& v : mesh.vertices)
        z0 = std::min(z0, v.z), z1 = std::max(z1, v.z);
    out.z0 = z0;
    out.limit.assign(std::size_t(std::ceil((double(z1) - double(z0)) / bin)) + 1, std::numeric_limits<float>::infinity());
    for (const auto& t : mesh.triangles) {
        const Vec3f& a = mesh.vertices[t[0]];
        const Vec3f& b = mesh.vertices[t[1]];
        const Vec3f& c = mesh.vertices[t[2]];
        const double lo = std::min({a.z, b.z, c.z}), hi = std::max({a.z, b.z, c.z});
        if (lo == hi) { // horizontal: a layer should end here
            out.flats.push_back(lo);
            continue;
        }
        const double ux = double(b.x) - a.x, uy = double(b.y) - a.y, uz = double(b.z) - a.z;
        const double vx = double(c.x) - a.x, vy = double(c.y) - a.y, vz = double(c.z) - a.z;
        const double nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
        const double length = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (!(length > 0.0))
            continue;
        const double tilt = std::abs(nz) / length; // |n_z| of the unit normal
        if (cusp_height >= tilt * max_height)
            continue; // steep enough for any layer
        const float allows = float(cusp_height / tilt);
        const std::size_t first = std::size_t(std::floor((lo - out.z0) / bin));
        const std::size_t last = std::min(out.limit.size() - 1, std::size_t(std::floor((hi - out.z0) / bin)));
        for (std::size_t i = first; i <= last; ++i)
            out.limit[i] = std::min(out.limit[i], allows);
    }
    std::sort(out.flats.begin(), out.flats.end());
    out.flats.erase(std::unique(out.flats.begin(), out.flats.end()), out.flats.end());
    return out;
}

HeightBounds height_bounds(const Settings& s) {
    HeightBounds b;
    b.max = s.max_layer_height > 0.0 ? std::min(s.max_layer_height, s.nozzle_diameter) : s.nozzle_diameter;
    b.min = std::min(s.min_layer_height > 0.0 ? s.min_layer_height : 0.04, b.max);
    return b;
}

double listed_height(const Settings& s, double z) {
    double h = s.layer_height, at = -1.0;
    for (const auto& step : s.layer_heights)
        if (step[0] <= z + 1e-9 && step[0] > at)
            at = step[0], h = step[1];
    return h;
}

bool variable_layers(const Settings& s) { return !s.layer_heights.empty() || s.cusp_height > 0.0; }

int sub_layers(double h, double detail_height, double min_height) {
    if (!(detail_height > 0.0) || detail_height >= h)
        return 1;
    int k = std::clamp(int(std::lround(h / detail_height)), 2, 4);
    while (k > 1 && h / k < min_height - 1e-12)
        --k;
    return k < 2 ? 1 : k;
}

namespace {

// The uniform stack: every thick layer layer_height, split alike.
LayerStack uniform_stack(double min_z, double max_z, const Settings& s, int ratio) {
    LayerStack out;
    out.layers = plan_layers(min_z, max_z, {s.first_layer_height, s.layer_height / double(ratio)});
    out.share.assign(out.layers.size(), 1.0 / double(ratio));
    if (!out.share.empty())
        out.share.front() = 1.0;
    if (ratio > 1)
        for (std::size_t first = 1; first + std::size_t(ratio) <= out.layers.size(); first += std::size_t(ratio))
            out.groups.push_back({first, first + std::size_t(ratio) - 1});
    return out;
}

} // namespace

LayerStack plan_layer_stack(const IndexedMesh& mesh, double min_z, double max_z, const Settings& s) {
    const double kThinnest = 0.04; // as kThinnestSubLayer: ;HEIGHT: carries three decimals
    if (!variable_layers(s))
        return uniform_stack(min_z, max_z, s, sub_layers(s.layer_height, s.detail_height, kThinnest));
    if (!(s.first_layer_height > 0.0) || !(s.layer_height > 0.0))
        throw std::invalid_argument("layer heights must be positive");
    const HeightBounds bounds = height_bounds(s);
    const bool zones = s.detail_height > 0.0;
    const SlopeLimit slope = slope_limit(mesh, s.cusp_height, bounds.max);
    const double fine_min = std::max(kThinnest, bounds.min);

    LayerStack out;
    const auto add = [&](double bottom, double top, double share) {
        if (0.5 * (bottom + top) >= max_z)
            return false;
        out.layers.push_back({bottom, top});
        out.share.push_back(share);
        return true;
    };
    if (!add(min_z, min_z + s.first_layer_height, 1.0))
        return out;
    double z = min_z + s.first_layer_height;
    for (;;) {
        const double above_bed = z - min_z;
        double h = std::clamp(listed_height(s, above_bed), bounds.min, bounds.max);
        // A step of the list starts a layer.
        double next = std::numeric_limits<double>::infinity();
        for (const auto& step : s.layer_heights)
            if (step[0] > above_bed + 1e-9)
                next = std::min(next, step[0]);
        if (const double to_step = next - above_bed; to_step < h && to_step >= bounds.min)
            h = to_step;
        int k = 1;
        if (s.cusp_height > 0.0) {
            if (zones) {
                // The slope picks the thick layer's sub-layers; only where even
                // the most sub-layers are not fine enough, the thick layer thins.
                int most = sub_layers(h, s.detail_height, fine_min);
                const double reach = slope.tallest(z, h, most);
                if (reach < h - 1e-12) {
                    h = std::max(reach, bounds.min);
                    while (most > 1 && h / most < fine_min - 1e-12)
                        --most;
                }
                const double allows = slope.allowed(z, z + h);
                k = allows >= h ? 1 : std::clamp(int(std::ceil(h / allows - 1e-9)), 1, most);
            } else {
                // The whole layer as tall as the slope allows.
                h = std::max(slope.tallest(z, h), bounds.min);
            }
            // A flat face ends a layer: the next one within reach, or just
            // past it when the layer can stretch to it.
            const auto flat = std::upper_bound(slope.flats.begin(), slope.flats.end(), z + bounds.min - 1e-9);
            if (flat != slope.flats.end()) {
                const double to_flat = *flat - z;
                if (to_flat <= h || (to_flat < h + bounds.min && to_flat <= bounds.max))
                    h = to_flat;
            }
        } else if (zones) {
            k = sub_layers(h, s.detail_height, fine_min);
        }
        const std::size_t first = out.layers.size();
        bool whole = true;
        for (int i = 0; i < k && whole; ++i)
            whole = add(z + h * i / k, i + 1 == k ? z + h : z + h * (i + 1) / k, 1.0 / k);
        if (out.layers.size() == first)
            break;
        if (k > 1 && whole)
            out.groups.push_back({first, out.layers.size() - 1});
        if (!whole)
            break;
        z += h;
    }
    return out;
}

std::vector<int> skin_counts(const std::vector<double>& share, int count, int step) {
    std::vector<int> out(share.size(), 0);
    if (count <= 0)
        return out;
    const auto n = std::ptrdiff_t(share.size());
    for (std::ptrdiff_t i = 0; i < n; ++i) {
        double sum = 0.0;
        int layers = 0;
        for (std::ptrdiff_t j = i + step; sum < double(count) - 1e-9; j += step) {
            ++layers;
            if (j < 0 || j >= n)
                break; // past the end: no layer covers it, the skin reaches out
            sum += share[std::size_t(j)];
        }
        out[std::size_t(i)] = layers;
    }
    return out;
}

} // namespace tisma
