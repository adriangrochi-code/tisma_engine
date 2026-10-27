#include "resin/resin.hpp"

#include "calibration/calibration.hpp"

#include "slice/layers.hpp"
#include "slice/level_set.hpp"

#include <tbb/combinable.h>
#include <tbb/parallel_for.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <map>
#include <numeric>
#include <string>

namespace tisma {
namespace {

// The mesh scaled by resin.scale_* about its box's centre in x and y and
// from its base in z (rule 5), its base moved to z = 0 (on the platform).
IndexedMesh scaled_mesh(const IndexedMesh& mesh, const Settings& s) {
    IndexedMesh out = mesh;
    if (out.vertices.empty())
        return out;
    double lo[3] = {1e300, 1e300, 1e300}, hi[3] = {-1e300, -1e300, -1e300};
    for (const Vec3f& v : out.vertices) {
        const double p[3] = {v.x, v.y, v.z};
        for (int a = 0; a < 3; ++a)
            lo[a] = std::min(lo[a], p[a]), hi[a] = std::max(hi[a], p[a]);
    }
    const double cx = 0.5 * (lo[0] + hi[0]), cy = 0.5 * (lo[1] + hi[1]);
    for (Vec3f& v : out.vertices) {
        v.x = float(cx + (double(v.x) - cx) * s.resin_scale_x);
        v.y = float(cy + (double(v.y) - cy) * s.resin_scale_y);
        v.z = float((double(v.z) - lo[2]) * s.resin_scale_z);
    }
    orient_consistently(out);
    return out;
}

std::vector<Layer> resin_layers(const IndexedMesh& mesh, const Settings& s) {
    double top = 0.0;
    for (const Vec3f& v : mesh.vertices)
        top = std::max(top, double(v.z));
    if (mesh.vertices.empty())
        return {};
    return plan_layers(0.0, top, {s.resin_layer_height, s.resin_layer_height});
}

std::vector<Polygons> sections_of(const IndexedMesh& mesh, const Settings& s, const std::vector<Layer>& layers) {
    std::vector<float> heights;
    for (const Layer& l : layers)
        heights.push_back(float(l.slice_z()));
    std::vector<char> same;
    const std::vector<std::vector<Contour>> contours =
        slice_levels(mesh, height_field(mesh), heights, s.gap_closing_radius, nullptr, &same);
    std::vector<std::size_t> source(layers.size());
    for (std::size_t l = 0; l < layers.size(); ++l)
        source[l] = l > 0 && same[l] ? source[l - 1] : l;
    std::vector<Polygons> regions(layers.size());
    tbb::parallel_for(std::size_t(0), layers.size(), [&](std::size_t l) {
        regions[l] = layer_region(to_polygons(contours[source[l]]), s.fill_mode);
    });
    return regions;
}

struct Edge {
    double ax, ay, bx, by; // in pixels: x along the columns, y along the rows
    double lo, hi;         // its rows' range
};

std::uint16_t rgb565(double r, double g, double b) {
    auto c = [](double v, int bits) {
        const int top = (1 << bits) - 1;
        return std::clamp(int(std::lround(v * top)), 0, top);
    };
    return std::uint16_t(c(r, 5) << 11 | c(g, 6) << 5 | c(b, 5));
}

// The part seen from above on an n by n preview (row 0 at the back): the
// highest layer over each pixel, shaded by the slope of those heights.
std::vector<std::uint16_t> preview(const std::vector<Polygons>& sections, const std::vector<Layer>& layers, int n,
                                   const double box[4]) {
    const double side = std::max({box[2] - box[0], box[3] - box[1], 1.0}) * 1.1;
    PixelGrid grid;
    grid.width = grid.height = n;
    grid.pixel_x = grid.pixel_y = side / n;
    grid.left = 0.5 * (box[0] + box[2]) - 0.5 * side;
    grid.top = 0.5 * (box[1] + box[3]) + 0.5 * side;
    tbb::combinable<std::vector<float>> heights([&] { return std::vector<float>(std::size_t(n) * n, -1.0f); });
    tbb::parallel_for(std::size_t(0), sections.size(), [&](std::size_t l) {
        std::vector<float>& h = heights.local();
        const float z = float(layers[l].top);
        coverage_rows(sections[l], grid, [&](int row, int x0, std::span<const double> share) {
            for (std::size_t i = 0; i < share.size(); ++i)
                if (std::abs(share[i]) >= 0.5) {
                    float& cell = h[std::size_t(row) * n + std::size_t(x0) + i];
                    cell = std::max(cell, z);
                }
        });
    });
    std::vector<float> h(std::size_t(n) * n, -1.0f);
    heights.combine_each([&](const std::vector<float>& part) {
        for (std::size_t i = 0; i < h.size(); ++i)
            h[i] = std::max(h[i], part[i]);
    });
    std::vector<std::uint16_t> out(h.size());
    // Light from the front left, above.
    const double light[3] = {-0.40, -0.45, 0.80};
    const double norm = std::sqrt(light[0] * light[0] + light[1] * light[1] + light[2] * light[2]);
    auto at = [&](int x, int y, float own) {
        if (x < 0 || y < 0 || x >= n || y >= n)
            return own;
        const float v = h[std::size_t(y) * n + std::size_t(x)];
        return v < 0.0f ? own : v;
    };
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x) {
            const float z = h[std::size_t(y) * n + std::size_t(x)];
            if (z < 0.0f) {
                out[std::size_t(y) * n + std::size_t(x)] = rgb565(0.10, 0.10, 0.12);
                continue;
            }
            // Rows go down in y: the slope in y is the one from the row below.
            const double dzdx = (at(x + 1, y, z) - at(x - 1, y, z)) / (2.0 * grid.pixel_x);
            const double dzdy = (at(x, y - 1, z) - at(x, y + 1, z)) / (2.0 * grid.pixel_y);
            const double nl = std::sqrt(dzdx * dzdx + dzdy * dzdy + 1.0);
            const double lambert =
                std::max(0.0, (-dzdx * light[0] - dzdy * light[1] + light[2]) / (nl * norm));
            const double b = 0.30 + 0.70 * lambert;
            out[std::size_t(y) * n + std::size_t(x)] = rgb565(0.66 * b, 0.73 * b, 0.80 * b);
        }
    return out;
}

// A box from (x0, y0, z0) to (x1, y1, z1), wound outwards.
void add_box(TriangleMesh& m, double x0, double y0, double z0, double x1, double y1, double z1) {
    const Vec3f v[8] = {{float(x0), float(y0), float(z0)}, {float(x1), float(y0), float(z0)},
                        {float(x1), float(y1), float(z0)}, {float(x0), float(y1), float(z0)},
                        {float(x0), float(y0), float(z1)}, {float(x1), float(y0), float(z1)},
                        {float(x1), float(y1), float(z1)}, {float(x0), float(y1), float(z1)}};
    const auto quad = [&](int a, int b, int c, int d) {
        m.add_triangle(v[a], v[b], v[c]);
        m.add_triangle(v[a], v[c], v[d]);
    };
    quad(0, 3, 2, 1);
    quad(4, 5, 6, 7);
    quad(0, 1, 5, 4);
    quad(1, 2, 6, 5);
    quad(2, 3, 7, 6);
    quad(3, 0, 4, 7);
}

// The holes of a section, each a piece of the plane the part closes around
// (with the islands standing in it taken out).
std::vector<Polygons> holes_of(const Polygons& section) {
    Polygons outers;
    for (const Polygon& p : section)
        if (Clipper2Lib::Area(p) > 0.0)
            outers.push_back(p);
    Clipper2Lib::Clipper64 clipper;
    clipper.AddSubject(outers);
    clipper.AddClip(section);
    Clipper2Lib::PolyTree64 tree;
    clipper.Execute(Clipper2Lib::ClipType::Difference, Clipper2Lib::FillRule::NonZero, tree);
    std::vector<Polygons> out;
    std::vector<const Clipper2Lib::PolyPath64*> pieces;
    for (const auto& piece : tree)
        pieces.push_back(piece.get());
    for (std::size_t k = 0; k < pieces.size(); ++k) {
        Polygons hole{pieces[k]->Polygon()};
        for (const auto& island : *pieces[k]) {
            hole.push_back(island->Polygon());
            for (const auto& inner : *island)
                pieces.push_back(inner.get());
        }
        out.push_back(std::move(hole));
    }
    return out;
}

} // namespace

PixelGrid display_grid(const Settings& s) {
    PixelGrid g;
    g.width = s.display_pixels_x;
    g.height = s.display_pixels_y;
    g.left = 0.0;
    g.top = s.display_height;
    g.pixel_x = s.display_width / s.display_pixels_x;
    g.pixel_y = s.display_height / s.display_pixels_y;
    g.mirror_x = s.display_mirror_x;
    g.mirror_y = s.display_mirror_y;
    return g;
}

void coverage_rows(const Polygons& region_in, const PixelGrid& grid,
                   const std::function<void(int row, int x0, std::span<const double> share)>& row) {
    if (region_in.empty() || grid.width <= 0 || grid.height <= 0)
        return;
    // What lies off the grid is cut away first: every edge then lies on it.
    const double right = grid.left + grid.width * grid.pixel_x, bottom = grid.top - grid.height * grid.pixel_y;
    const Clipper2Lib::Rect64 bounds = Clipper2Lib::GetBounds(region_in);
    Polygons clipped;
    const Polygons* region = &region_in;
    if (to_mm(bounds.left) < grid.left || to_mm(bounds.right) > right || to_mm(bounds.top) < bottom ||
        to_mm(bounds.bottom) > grid.top) {
        const Polygon rect = {{to_units(grid.left), to_units(bottom)},
                              {to_units(right), to_units(bottom)},
                              {to_units(right), to_units(grid.top)},
                              {to_units(grid.left), to_units(grid.top)}};
        clipped = Clipper2Lib::Intersect(region_in, Polygons{rect}, Clipper2Lib::FillRule::NonZero);
        region = &clipped;
    }
    std::vector<Edge> edges;
    double umin = 1e300, umax = -1e300, vmin = 1e300, vmax = -1e300;
    for (const Polygon& poly : *region) {
        const std::size_t m = poly.size();
        if (m < 3)
            continue;
        auto pixel = [&](const Clipper2Lib::Point64& p) {
            double u = (to_mm(p.x) - grid.left) / grid.pixel_x, v = (grid.top - to_mm(p.y)) / grid.pixel_y;
            if (grid.mirror_x)
                u = grid.width - u;
            if (grid.mirror_y)
                v = grid.height - v;
            return std::array<double, 2>{u, v};
        };
        std::array<double, 2> a = pixel(poly.back());
        for (std::size_t i = 0; i < m; ++i) {
            const std::array<double, 2> b = pixel(poly[i]);
            umin = std::min(umin, b[0]), umax = std::max(umax, b[0]);
            vmin = std::min(vmin, b[1]), vmax = std::max(vmax, b[1]);
            if (a[1] != b[1])
                edges.push_back({a[0], a[1], b[0], b[1], std::min(a[1], b[1]), std::max(a[1], b[1])});
            a = b;
        }
    }
    if (edges.empty())
        return;
    const int x0 = std::clamp(int(std::floor(umin)), 0, grid.width), x1 = std::clamp(int(std::ceil(umax)), 0, grid.width);
    const int y0 = std::clamp(int(std::floor(vmin)), 0, grid.height), y1 = std::clamp(int(std::ceil(vmax)), 0, grid.height);
    if (x1 <= x0 || y1 <= y0)
        return;
    const int w = x1 - x0;
    // Each edge adds, to every pixel of its rows, the signed area between it
    // and the pixel's right side; every pixel past it in the row gets the
    // edge's whole height in that row. Kept as differences along the row
    // (d[c] for column c on), summed at the end: the outer contours and the
    // holes go round opposite ways, so inside the region it comes to the
    // pixel's covered share.
    std::sort(edges.begin(), edges.end(), [](const Edge& a, const Edge& b) { return a.lo < b.lo; });
    constexpr int kBand = 32;
    std::vector<double> d(std::size_t(kBand) * std::size_t(w + 1));
    std::vector<double> share(static_cast<std::size_t>(w), 0.0);
    std::vector<std::size_t> active;
    std::size_t next = 0;
    for (int r0 = y0; r0 < y1; r0 += kBand) {
        const int r1 = std::min(y1, r0 + kBand);
        while (next < edges.size() && edges[next].lo < r1)
            active.push_back(next++);
        std::erase_if(active, [&](std::size_t e) { return edges[e].hi <= r0; });
        std::fill(d.begin(), d.end(), 0.0);
        for (const std::size_t e : active) {
            const Edge& g = edges[e];
            const double dxdy = (g.bx - g.ax) / (g.by - g.ay);
            const bool down = g.by > g.ay;
            const int first = std::max(r0, int(std::floor(g.lo))), last = std::min(r1, int(std::ceil(g.hi)));
            for (int r = first; r < last; ++r) {
                const double ya = std::max(g.lo, double(r)), yb = std::min(g.hi, double(r + 1));
                if (yb <= ya)
                    continue;
                const double ys = down ? ya : yb, ye = down ? yb : ya;
                const double xs = g.ax + (ys - g.ay) * dxdy, xe = g.ax + (ye - g.ay) * dxdy;
                double* cells = d.data() + std::size_t(r - r0) * std::size_t(w + 1);
                auto piece = [&](double xa, double xb, double dy) {
                    const double xm = 0.5 * (xa + xb);
                    const int c = std::clamp(int(std::floor(xm)), x0, x1 - 1);
                    const double inside = dy * ((c + 1) - xm);
                    cells[c - x0] += inside;
                    cells[c - x0 + 1] += dy - inside;
                };
                if (xs == xe) {
                    piece(xs, xe, ye - ys);
                    continue;
                }
                const double dy_dx = (ye - ys) / (xe - xs);
                if (xe > xs) {
                    for (double x = xs; x < xe;) {
                        const double xn = std::min(std::floor(x) + 1.0, xe);
                        piece(x, xn, (xn - x) * dy_dx);
                        x = xn;
                    }
                } else {
                    for (double x = xs; x > xe;) {
                        const double xn = std::max(std::ceil(x) - 1.0, xe);
                        piece(x, xn, (xn - x) * dy_dx);
                        x = xn;
                    }
                }
            }
        }
        for (int r = r0; r < r1; ++r) {
            const double* cells = d.data() + std::size_t(r - r0) * std::size_t(w + 1);
            double sum = 0.0;
            for (int c = 0; c < w; ++c) {
                sum += cells[c];
                share[std::size_t(c)] = std::min(1.0, std::abs(sum));
            }
            row(r, x0, share);
        }
    }
}

std::uint8_t resin_grey(double share, double gamma) {
    share = std::min(1.0, std::abs(share));
    if (gamma <= 0.0)
        return share >= 0.5 - 1e-9 ? 255 : 0;
    double g = std::ceil(255.0 * share - 0.05);
    if (g <= 0.0)
        return 0;
    if (g >= 255.0)
        return 255;
    if (gamma != 1.0)
        g = std::clamp(std::round(255.0 * std::pow(g / 255.0, gamma)), 1.0, 255.0);
    return std::uint8_t(g);
}

void GooImageEncoder::add(std::uint8_t value, std::size_t count) {
    if (count == 0)
        return;
    if (count_ > 0 && value == value_) {
        count_ += count;
        return;
    }
    flush();
    value_ = value;
    count_ = count;
}

void GooImageEncoder::flush() {
    // byte 0: the kind (0x00 pixels, a grey, 0xff pixels) in bits 7 and 6,
    // how many more bytes the length takes in bits 5 and 4, the length's
    // low 4 bits; a grey's value next; then the rest of the length, high
    // byte first.
    while (count_ > 0) {
        const std::size_t length = std::min<std::size_t>(count_, (std::size_t(1) << 28) - 1);
        count_ -= length;
        const unsigned kind = value_ == 0 ? 0u : value_ == 255 ? 3u : 1u;
        const unsigned more = length < 16 ? 0u : length < (1u << 12) ? 1u : length < (1u << 20) ? 2u : 3u;
        bytes_.push_back(std::uint8_t(kind << 6 | more << 4 | (length & 0xf)));
        if (kind == 1)
            bytes_.push_back(value_);
        for (int k = int(more) - 1; k >= 0; --k)
            bytes_.push_back(std::uint8_t(length >> (4 + 8 * k)));
    }
}

std::vector<std::uint8_t> GooImageEncoder::finish() {
    flush();
    std::uint8_t sum = 0;
    for (std::size_t i = 1; i < bytes_.size(); ++i)
        sum = std::uint8_t(sum + bytes_[i]);
    bytes_.push_back(std::uint8_t(~sum));
    std::vector<std::uint8_t> out = std::move(bytes_);
    bytes_ = {0x55};
    count_ = 0;
    return out;
}

std::vector<std::uint8_t> decode_goo_image(std::span<const std::uint8_t> data, int width, int height) {
    if (data.size() < 2 || data[0] != 0x55)
        throw ResinError("image: no 0x55 at its start");
    std::uint8_t sum = 0;
    for (std::size_t i = 1; i + 1 < data.size(); ++i)
        sum = std::uint8_t(sum + data[i]);
    if (std::uint8_t(~sum) != data.back())
        throw ResinError("image: wrong checksum");
    const std::size_t total = std::size_t(width) * std::size_t(height), end = data.size() - 1;
    std::vector<std::uint8_t> out;
    out.reserve(total);
    std::size_t pos = 1;
    int previous = 0;
    auto next = [&]() {
        if (pos >= end)
            throw ResinError("image: a chunk runs past the end");
        return data[pos++];
    };
    auto put = [&](int value, std::size_t count) {
        if (count > total - out.size())
            throw ResinError("image: more pixels than the display has");
        out.insert(out.end(), count, std::uint8_t(value));
        previous = value;
    };
    while (pos < end) {
        const std::uint8_t b0 = data[pos++];
        const unsigned kind = b0 >> 6;
        if (kind == 2) {
            // A difference from the previous pixel, down with bit 5, for
            // one pixel or for byte 1 of them with bit 4.
            const int diff = (b0 & 0x20) ? -int(b0 & 0xf) : int(b0 & 0xf);
            const std::size_t count = (b0 & 0x10) ? next() : 1;
            if (previous + diff < 0 || previous + diff > 255)
                throw ResinError("image: a difference out of 0 to 255");
            put(previous + diff, count);
            continue;
        }
        const int value = kind == 0 ? 0 : kind == 3 ? 255 : next();
        std::size_t length = 0;
        for (unsigned k = 0; k < ((b0 >> 4) & 3u); ++k)
            length = length << 8 | next();
        put(value, length << 4 | (b0 & 0xfu));
    }
    if (out.size() != total)
        throw ResinError("image: " + std::to_string(out.size()) + " pixels, the display has " + std::to_string(total));
    return out;
}

std::vector<Polygons> resin_sections(const IndexedMesh& mesh, const Settings& s) {
    const IndexedMesh scaled = scaled_mesh(mesh, s);
    return sections_of(scaled, s, resin_layers(scaled, s));
}

ResinPrint slice_resin(const IndexedMesh& input, const Settings& s, const std::function<double(double z)>& exposure) {
    if (input.triangles.empty())
        throw ResinError("the model is empty");
    const IndexedMesh mesh = scaled_mesh(input, s);
    ResinPrint print;
    print.max_height = s.max_height > 0.0 ? s.max_height : 150.0;
    double box[4] = {1e300, 1e300, -1e300, -1e300}, top = 0.0;
    for (const Vec3f& v : mesh.vertices) {
        box[0] = std::min(box[0], double(v.x)), box[1] = std::min(box[1], double(v.y));
        box[2] = std::max(box[2], double(v.x)), box[3] = std::max(box[3], double(v.y));
        top = std::max(top, double(v.z));
    }
    constexpr double kSlack = 1e-6;
    if (box[0] < -kSlack || box[1] < -kSlack || box[2] > s.display_width + kSlack || box[3] > s.display_height + kSlack) {
        char what[200];
        std::snprintf(what, sizeof what,
                      "the part (x %.2f to %.2f, y %.2f to %.2f mm) does not fit on the display (0 to %.2f by 0 to %.2f mm)",
                      box[0], box[2], box[1], box[3], s.display_width, s.display_height);
        throw ResinError(what);
    }
    if (top > print.max_height + kSlack) {
        char what[160];
        std::snprintf(what, sizeof what, "the part is %.2f mm tall: the printer prints up to %.2f mm (machine.max_height)",
                      top, print.max_height);
        throw ResinError(what);
    }
    const std::vector<Layer> layers = resin_layers(mesh, s);
    if (layers.empty())
        throw ResinError("the part is thinner than half a layer");
    for (const auto& t : mesh.triangles) {
        const Vec3f& a = mesh.vertices[t[0]];
        const Vec3f& b = mesh.vertices[t[1]];
        const Vec3f& c = mesh.vertices[t[2]];
        print.volume += (double(a.x) * (double(b.y) * c.z - double(b.z) * c.y) -
                         double(a.y) * (double(b.x) * c.z - double(b.z) * c.x) +
                         double(a.z) * (double(b.x) * c.y - double(b.y) * c.x)) /
                        6.0;
    }
    print.volume = std::abs(print.volume);
    print.weight = print.volume / 1000.0 * s.resin_density;

    // The sections grown by the absolute correction (rule 22); the bottom
    // layers shrunk (rule 6), the first by the whole elephant's foot, the
    // next ones less, in a straight line; and of each, only what stands on
    // the last resin.z_correction_layers sections (rule 21).
    const std::vector<Polygons> sections = sections_of(mesh, s, layers);
    const std::size_t count = layers.size();
    std::vector<Polygons> grown(count);
    tbb::parallel_for(std::size_t(0), count, [&](std::size_t l) {
        grown[l] = s.resin_absolute_correction != 0.0 ? offset(sections[l], s.resin_absolute_correction) : sections[l];
    });
    print.bottom_layers = std::max(1, s.resin_bottom_layers);
    const PixelGrid grid = display_grid(s);
    print.layers.resize(count);
    tbb::parallel_for(std::size_t(0), count, [&](std::size_t l) {
        Polygons lit = grown[l];
        const bool bottom = l < std::size_t(print.bottom_layers);
        if (bottom && s.resin_elephant_foot > 0.0) {
            const double shrink = s.resin_elephant_foot * (1.0 - double(l) / print.bottom_layers);
            if (shrink > 0.0)
                lit = offset(lit, -shrink);
        }
        for (std::size_t j = 1; j <= std::size_t(s.resin_z_correction_layers) && j <= l && !lit.empty(); ++j)
            lit = Clipper2Lib::Intersect(lit, grown[l - j], Clipper2Lib::FillRule::NonZero);

        ResinLayer& out = print.layers[l];
        GooImageEncoder image;
        int next_row = 0;
        coverage_rows(lit, grid, [&](int row, int x0, std::span<const double> share) {
            image.add(0, std::size_t(row - next_row) * std::size_t(grid.width) + std::size_t(x0));
            for (const double a : share) {
                const std::uint8_t g = resin_grey(a, s.resin_gamma);
                out.lit += g > 0;
                image.add(g);
            }
            image.add(0, std::size_t(grid.width) - std::size_t(x0) - share.size());
            next_row = row + 1;
        });
        image.add(0, std::size_t(grid.height - next_row) * std::size_t(grid.width));
        out.image = image.finish();

        out.z = layers[l].top;
        out.bottom = bottom;
        out.exposure = bottom ? s.resin_bottom_exposure : exposure ? exposure(layers[l].slice_z()) : s.resin_exposure;
        out.lift_distance = bottom ? s.resin_bottom_lift_distance : s.resin_lift_distance;
        out.lift_speed = bottom ? s.resin_bottom_lift_speed : s.resin_lift_speed;
        out.retract_distance = out.lift_distance;
        out.retract_speed = s.resin_retract_speed;
        out.wait_after_cure = s.resin_wait_after_cure;
        out.wait_after_lift = s.resin_wait_after_lift;
        out.wait_before_exposure = s.resin_wait_before_exposure;
        out.pwm = bottom ? s.resin_bottom_light_pwm : s.resin_light_pwm;
    });
    for (const ResinLayer& l : print.layers)
        print.seconds += l.exposure + l.wait_after_cure + l.lift_distance / l.lift_speed + l.wait_after_lift +
                         l.retract_distance / l.retract_speed + l.wait_before_exposure;
    // The part stands on the platform (resin supports are not made yet).
    print.cups = find_cups(sections, s.resin_layer_height, false);
    print.small_preview = preview(sections, layers, 116, box);
    print.big_preview = preview(sections, layers, 290, box);
    return print;
}

std::vector<ResinCup> find_cups(const std::vector<Polygons>& sections, double layer_height, bool raised) {
    const std::size_t count = sections.size();
    // Each layer's holes, and for each, how much of it lies over the layer
    // below's section and over each of the layer below's holes. The section
    // and its holes do not overlap, so what is left is over the air around
    // the part. Every layer at once.
    struct Hole {
        Polygons shape;
        double area = 0.0, under = 0.0;
        std::vector<std::pair<std::size_t, double>> over; // the holes below it overlaps, and by how much
    };
    std::vector<std::vector<Hole>> holes(count);
    tbb::parallel_for(std::size_t(0), count, [&](std::size_t l) {
        for (Polygons& shape : holes_of(sections[l])) {
            Hole h;
            h.area = area_mm2(shape);
            h.shape = std::move(shape);
            holes[l].push_back(std::move(h));
        }
    });
    auto overlap = [](const Polygons& a, const Polygons& b) {
        const Clipper2Lib::Rect64 ra = Clipper2Lib::GetBounds(a), rb = Clipper2Lib::GetBounds(b);
        if (ra.right <= rb.left || rb.right <= ra.left || ra.bottom <= rb.top || rb.bottom <= ra.top)
            return 0.0;
        return area_mm2(Clipper2Lib::Intersect(Clipper2Lib::RectClip(ra, b), a, Clipper2Lib::FillRule::NonZero));
    };
    tbb::parallel_for(std::size_t(1), std::max<std::size_t>(count, 1), [&](std::size_t l) {
        for (Hole& h : holes[l]) {
            h.under = overlap(h.shape, sections[l - 1]);
            for (std::size_t g = 0; g < holes[l - 1].size(); ++g)
                if (const double a = overlap(h.shape, holes[l - 1][g].shape); a > 1e-9)
                    h.over.push_back({g, a});
        }
    });
    // Layer by layer from the platform: a hole whose way down (over air, or
    // over a hole that is vented) is under kCupExit is closed. Closed holes
    // that overlap from one layer to the next are one cup.
    struct Closed {
        std::size_t layer;
        double area;
    };
    std::vector<Closed> closed;
    std::vector<std::size_t> parent;
    auto root = [&](std::size_t i) {
        while (parent[i] != i)
            i = parent[i] = parent[parent[i]];
        return i;
    };
    constexpr std::size_t kVented = std::size_t(-1);
    std::vector<std::size_t> below; // the cup piece of each hole of the layer below, or kVented
    for (std::size_t l = 0; l < count; ++l) {
        std::vector<std::size_t> here(holes[l].size(), kVented);
        for (std::size_t k = 0; k < holes[l].size(); ++k) {
            const Hole& h = holes[l][k];
            double open = l == 0 ? (raised ? h.area : 0.0) : h.area - h.under;
            for (const auto& [g, a] : h.over)
                if (below[g] != kVented)
                    open -= a;
            if (open >= kCupExit - 1e-9)
                continue;
            const std::size_t id = closed.size();
            parent.push_back(id);
            for (const auto& [g, a] : h.over)
                if (below[g] != kVented)
                    parent[root(below[g])] = id;
            closed.push_back({l, h.area});
            here[k] = id;
        }
        below = std::move(here);
    }
    // Each cup: its layers and its largest section (the sum of its holes on a layer).
    std::map<std::size_t, ResinCup> cups;
    std::map<std::pair<std::size_t, std::size_t>, double> per_layer;
    for (std::size_t i = 0; i < closed.size(); ++i) {
        const std::size_t r = root(i);
        auto [it, fresh] = cups.try_emplace(r);
        ResinCup& c = it->second;
        const int layer = int(closed[i].layer);
        c.first = fresh ? layer : std::min(c.first, layer);
        c.last = fresh ? layer : std::max(c.last, layer);
        c.area = std::max(c.area, per_layer[{r, closed[i].layer}] += closed[i].area);
    }
    std::vector<ResinCup> out;
    for (auto& [r, c] : cups) {
        if (c.area < kCupReport - 1e-9)
            continue;
        c.z0 = c.first * layer_height;
        c.z1 = (c.last + 1) * layer_height;
        out.push_back(c);
    }
    std::sort(out.begin(), out.end(), [](const ResinCup& a, const ResinCup& b) {
        return a.first != b.first ? a.first < b.first : a.area > b.area;
    });
    return out;
}

std::vector<Warning> cup_warnings(const ResinPrint& print, const std::string& part) {
    std::vector<Warning> out;
    for (const ResinCup& c : print.cups) {
        char text[512];
        std::snprintf(text, sizeof text,
                      "%s: a cup from z %.2f to %.2f mm (layers %d to %d), up to %.0f mm2: the film seals it as each "
                      "layer peels off, and the suction can tear or bend the print. Add a drain hole of 3 to 5 mm in "
                      "its floor, or turn the part so that the hollow opens towards the platform",
                      part.c_str(), c.z0, c.z1, c.first, c.last, c.area);
        out.push_back({"cupping", c.first, c.z0, text, c.area, "warning"});
    }
    return out;
}

std::vector<double> tower_exposures(const ResinTower& t) {
    return tower_values(t.start, t.end, t.step);
}

double tower_exposure(const ResinTower& t, double base, double z) {
    const std::vector<double> values = tower_exposures(t);
    const double band = t.band > 0.0 ? std::floor(z / t.band + 1e-9) : 0.0;
    if (band < 1.0 || values.empty())
        return base;
    return values[std::min(std::size_t(band) - 1, values.size() - 1)];
}

TriangleMesh resin_tower_mesh(const ResinTower& t) {
    if (!(t.band >= 2.0 && t.band <= 20.0))
        throw ResinError("the exposure tower's bands must be 2 to 20 mm tall");
    constexpr double kHalfWidth = 12.0, kHalfDepth = 4.0;
    constexpr double kFront = 0.5;   // the front strip, where the grooves are
    constexpr double kGroove = 0.15; // half the groove's height
    constexpr double kHoles[5] = {0.4, 0.6, 0.8, 1.0, 1.4}, kHoleX[5] = {-8.0, -4.0, 0.0, 4.0, 8.0};
    constexpr double kFins[4] = {0.15, 0.25, 0.35, 0.5}, kFinX[4] = {-7.5, -2.5, 2.5, 7.5};
    const std::size_t bands = tower_exposures(t).size() + 1;
    TriangleMesh m;
    for (std::size_t k = 0; k < bands; ++k) {
        const double z0 = double(k) * t.band, z1 = z0 + t.band, mid = 0.5 * (z0 + z1);
        // The front strip leaves a groove at each border between bands.
        const double f0 = k > 0 ? z0 + kGroove : z0, f1 = k + 1 < bands ? z1 - kGroove : z1;
        // Columns across X, those of the holes open from mid - d/2 to mid + d/2.
        double x = -kHalfWidth;
        auto column = [&](double x0, double x1, double hole) {
            const double ranges[2][2] = {{z0, hole > 0.0 ? mid - 0.5 * hole : z1}, {mid + 0.5 * hole, z1}};
            for (int r = 0; r < (hole > 0.0 ? 2 : 1); ++r) {
                add_box(m, x0, -kHalfDepth + kFront, ranges[r][0], x1, kHalfDepth, ranges[r][1]);
                const double a = std::max(ranges[r][0], f0), b = std::min(ranges[r][1], f1);
                if (b > a)
                    add_box(m, x0, -kHalfDepth, a, x1, -kHalfDepth + kFront, b);
            }
        };
        for (int h = 0; h < 5 && k > 0; ++h) {
            const double x0 = kHoleX[h] - 0.5 * kHoles[h], x1 = kHoleX[h] + 0.5 * kHoles[h];
            column(x, x0, 0.0);
            column(x0, x1, kHoles[h]);
            x = x1;
        }
        column(x, kHalfWidth, 0.0);
        // The fins at the back.
        for (int f = 0; f < 4 && k > 0; ++f)
            add_box(m, kFinX[f] - 0.5 * kFins[f], kHalfDepth, z0 + 0.25, kFinX[f] + 0.5 * kFins[f], kHalfDepth + 2.0,
                    z1 - 0.25);
    }
    return m;
}

TriangleMesh resin_measure_block() {
    TriangleMesh m;
    add_box(m, -15.0, -15.0, 0.0, 15.0, 15.0, 10.0);
    return m;
}

std::array<double, 3> resin_scale_from(const Settings& s, double x, double y, double z) {
    if (!(x > 0.0 && y > 0.0 && z > 0.0))
        throw ResinError("the measured sizes must be above 0");
    return {s.resin_scale_x * 30.0 / x, s.resin_scale_y * 30.0 / y, s.resin_scale_z * 10.0 / z};
}

} // namespace tisma
