// A pack printer's bed picture as coloured triangles (profile_pack.md, rule
// 14). Own code; see bed_art.hpp and PROVENANCE.md.

#include "ui/bed_art.hpp"

#include "slice/transform.hpp" // triangulate_polygon

#include <clipper2/clipper.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <string>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702) // unreachable code inside nanosvg
#endif
#include <nanosvg.h> // the implementation is compiled in icons.cpp
#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace tisma::ui {
namespace {

constexpr double kScale = 1000.0; // Clipper2's integer units per SVG unit
constexpr int kMaxCurveSteps = 16;

using Clipper2Lib::Path64;
using Clipper2Lib::Paths64;
using Point2 = std::array<double, 2>;

// A path's cubic Béziers as straight steps, in Clipper2's units; a straight
// segment (nanosvg writes lines as flat curves) is one step.
Path64 flatten(const NSVGpath& path, double tolerance) {
    Path64 out;
    if (path.npts < 1)
        return out;
    const auto at = [&](int i) { return Point2{path.pts[2 * i], path.pts[2 * i + 1]}; };
    const auto add = [&](Point2 p) {
        const Clipper2Lib::Point64 q(std::llround(p[0] * kScale), std::llround(p[1] * kScale));
        if (out.empty() || out.back() != q)
            out.push_back(q);
    };
    add(at(0));
    for (int i = 0; i + 3 < path.npts; i += 3) {
        const Point2 p0 = at(i), c1 = at(i + 1), c2 = at(i + 2), p1 = at(i + 3);
        // How far the control points stray from the chord sets the steps.
        const double dx = p1[0] - p0[0], dy = p1[1] - p0[1];
        const double chord = std::hypot(dx, dy);
        const auto off = [&](Point2 c) {
            return chord > 0 ? std::abs((c[0] - p0[0]) * dy - (c[1] - p0[1]) * dx) / chord
                             : std::hypot(c[0] - p0[0], c[1] - p0[1]);
        };
        const double bend = std::max(off(c1), off(c2));
        const int steps = bend <= tolerance ? 1 : std::clamp(int(std::ceil(std::sqrt(bend / tolerance) * 2.0)), 2, kMaxCurveSteps);
        for (int s = 1; s <= steps; ++s) {
            const double t = double(s) / steps, u = 1.0 - t;
            const double a = u * u * u, b = 3 * u * u * t, c = 3 * u * t * t, d = t * t * t;
            add({a * p0[0] + b * c1[0] + c * c2[0] + d * p1[0], a * p0[1] + b * c1[1] + c * c2[1] + d * p1[1]});
        }
    }
    return out;
}

double area(const std::vector<Point2>& ring) {
    double a = 0.0;
    for (std::size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++)
        a += ring[j][0] * ring[i][1] - ring[i][0] * ring[j][1];
    return 0.5 * a;
}

struct Mapper {
    double sx, sy, ox, oy, z;
    Point2 operator()(const Clipper2Lib::Point64& p) const {
        return {ox + double(p.x) / kScale * sx, oy - double(p.y) / kScale * sy};
    }
};

std::vector<Point2> ring_of(const Path64& path, const Mapper& map, bool outer) {
    std::vector<Point2> ring;
    ring.reserve(path.size());
    for (const auto& p : path)
        ring.push_back(map(p));
    // triangulate_polygon wants the outer counter-clockwise, the holes clockwise.
    if ((area(ring) > 0) != outer)
        std::reverse(ring.begin(), ring.end());
    return ring;
}

void fill_node(const Clipper2Lib::PolyPath64& outer, const Mapper& map, std::vector<float>& vertices) {
    std::vector<Point2> ring = ring_of(outer.Polygon(), map, true);
    std::vector<std::vector<Point2>> holes;
    for (const auto& hole : outer)
        holes.push_back(ring_of(hole->Polygon(), map, false));
    if (ring.size() >= 3) {
        std::vector<Point2> all = ring;
        for (const auto& h : holes)
            all.insert(all.end(), h.begin(), h.end());
        for (const auto& t : triangulate_polygon(ring, holes))
            for (const std::size_t k : t)
                vertices.insert(vertices.end(), {float(all[k][0]), float(all[k][1]), float(map.z), 0.0f, 0.0f, 1.0f});
    }
    // Islands inside the holes.
    for (const auto& hole : outer)
        for (const auto& island : *hole)
            fill_node(*island, map, vertices);
}

// One painted region: its colour (nanosvg's 0xAABBGGRR) and its outline.
struct Region {
    std::uint32_t abgr;
    Paths64 area;
};

void add_group(BedArt& art, std::uint32_t abgr, const Clipper2Lib::PolyTree64& tree, const Mapper& map) {
    const std::uint32_t first = std::uint32_t(art.vertices.size() / 6);
    for (const auto& outer : tree)
        fill_node(*outer, map, art.vertices);
    const std::uint32_t count = std::uint32_t(art.vertices.size() / 6) - first;
    if (count == 0)
        return;
    const std::uint32_t rgb = ((abgr & 0xFF) << 16) | (abgr & 0xFF00) | ((abgr >> 16) & 0xFF);
    if (!art.groups.empty() && art.groups.back().rgb == rgb)
        art.groups.back().count += count; // the same colour, drawn together
    else
        art.groups.push_back({rgb, first, count});
}

// The regions as triangles that never overlap: each one without what is
// painted after it, so they all lie at one height without fighting in depth.
void add_regions(BedArt& art, const std::vector<Region>& regions, const Mapper& map) {
    std::vector<Clipper2Lib::PolyTree64> seen(regions.size());
    Paths64 above;
    for (std::size_t i = regions.size(); i-- > 0;) {
        Clipper2Lib::Clipper64 c;
        c.AddSubject(regions[i].area);
        c.AddClip(above);
        c.Execute(Clipper2Lib::ClipType::Difference, Clipper2Lib::FillRule::NonZero, seen[i]);
        above = Clipper2Lib::Union(above, regions[i].area, Clipper2Lib::FillRule::NonZero);
    }
    for (std::size_t i = 0; i < regions.size(); ++i)
        add_group(art, regions[i].abgr, seen[i], map);
}

bool solid(const NSVGpaint& paint, float opacity) {
    return paint.type == NSVG_PAINT_COLOR && float(paint.color >> 24) * opacity >= 1.0f;
}

} // namespace

BedModelPlace bed_model_place(const double lo[3], const double hi[3], double min_x, double min_y, double max_x, double max_y) {
    BedModelPlace out;
    out.dx = 0.5 * (min_x + max_x) - 0.5 * (lo[0] + hi[0]);
    out.dy = 0.5 * (min_y + max_y) - 0.5 * (lo[1] + hi[1]);
    out.dz = -hi[2];
    out.small = hi[0] - lo[0] < 0.5 * (max_x - min_x) || hi[1] - lo[1] < 0.5 * (max_y - min_y);
    return out;
}

BedArt bed_svg_art(std::string_view svg, double min_x, double min_y, double max_x, double max_y, double z) {
    BedArt art;
    if (!(max_x > min_x) || !(max_y > min_y))
        return art;
    std::string text(svg); // nanosvg parses in place
    NSVGimage* image = nsvgParse(text.data(), "px", 96.0f);
    if (!image)
        return art;
    if (image->width > 0 && image->height > 0) {
        // The largest rectangle of the picture's shape inside the bed's box,
        // centred: never stretched (as the phone app draws it).
        const double scale = std::min((max_x - min_x) / image->width, (max_y - min_y) / image->height);
        const double left = min_x + 0.5 * ((max_x - min_x) - image->width * scale);
        const double top = max_y - 0.5 * ((max_y - min_y) - image->height * scale);
        const Mapper map{scale, scale, left, top, z};
        // About a tenth of a millimetre on the bed.
        const double tolerance = 0.1 / std::max(map.sx, map.sy);
        std::vector<Region> regions;
        for (const NSVGshape* shape = image->shapes; shape; shape = shape->next) {
            if (!(shape->flags & NSVG_FLAGS_VISIBLE))
                continue;
            Paths64 paths;
            std::vector<bool> closed;
            for (const NSVGpath* p = shape->paths; p; p = p->next) {
                Path64 flat = flatten(*p, tolerance);
                if (flat.size() >= 2) {
                    paths.push_back(std::move(flat));
                    closed.push_back(p->closed != 0);
                }
            }
            if (paths.empty())
                continue;
            if (solid(shape->fill, shape->opacity)) {
                regions.push_back({shape->fill.color,
                                   Clipper2Lib::Union(paths, shape->fillRule == NSVG_FILLRULE_EVENODD
                                                                 ? Clipper2Lib::FillRule::EvenOdd
                                                                 : Clipper2Lib::FillRule::NonZero)});
            }
            if (solid(shape->stroke, shape->opacity) && shape->strokeWidth > 0) {
                const auto join = shape->strokeLineJoin == NSVG_JOIN_ROUND   ? Clipper2Lib::JoinType::Round
                                  : shape->strokeLineJoin == NSVG_JOIN_BEVEL ? Clipper2Lib::JoinType::Square
                                                                             : Clipper2Lib::JoinType::Miter;
                const auto cap = shape->strokeLineCap == NSVG_CAP_ROUND    ? Clipper2Lib::EndType::Round
                                 : shape->strokeLineCap == NSVG_CAP_SQUARE ? Clipper2Lib::EndType::Square
                                                                           : Clipper2Lib::EndType::Butt;
                Paths64 open, shut;
                for (std::size_t i = 0; i < paths.size(); ++i)
                    (closed[i] ? shut : open).push_back(paths[i]);
                const double half = 0.5 * shape->strokeWidth * kScale;
                Paths64 band = Clipper2Lib::InflatePaths(shut, half, join, Clipper2Lib::EndType::Joined);
                const Paths64 lines = Clipper2Lib::InflatePaths(open, half, join, cap);
                band.insert(band.end(), lines.begin(), lines.end());
                regions.push_back({shape->stroke.color, Clipper2Lib::Union(band, Clipper2Lib::FillRule::NonZero)});
            }
        }
        add_regions(art, regions, map);
    }
    nsvgDelete(image);
    return art;
}

BedArt bed_image_art(int width, int height, const std::vector<std::uint8_t>& rgba, double min_x, double min_y,
                     double max_x, double max_y, double z, int cells) {
    BedArt art;
    const struct {
        int width, height;
        const std::vector<std::uint8_t>& rgba;
    } image{width, height, rgba};
    if (image.width <= 0 || image.height <= 0 || image.rgba.size() < std::size_t(image.width) * std::size_t(image.height) * 4 ||
        max_x <= min_x || max_y <= min_y || cells <= 0)
        return art;
    // The cells: the picture cut evenly, never more cells than pixels.
    const int step = std::max(1, (std::max(image.width, image.height) + cells - 1) / cells);
    const int cw = (image.width + step - 1) / step, ch = (image.height + step - 1) / step;
    // Each cell's colour (0xRRGGBB, 8 levels a channel), or -1 when mostly clear.
    std::vector<std::int64_t> colour(std::size_t(cw) * ch, -1);
    for (int cy = 0; cy < ch; ++cy)
        for (int cx = 0; cx < cw; ++cx) {
            double sum[4] = {0, 0, 0, 0};
            int n = 0;
            for (int y = cy * step; y < std::min(image.height, (cy + 1) * step); ++y)
                for (int x = cx * step; x < std::min(image.width, (cx + 1) * step); ++x) {
                    const std::uint8_t* px = &image.rgba[(std::size_t(y) * image.width + x) * 4];
                    const double a = px[3];
                    for (int c = 0; c < 3; ++c)
                        sum[c] += px[c] * a; // weighted by opacity, so clear pixels add no colour
                    sum[3] += a;
                    ++n;
                }
            if (n == 0 || sum[3] < 0.5 * 255.0 * n)
                continue;
            std::uint32_t rgb = 0;
            for (int c = 0; c < 3; ++c) {
                const double v = sum[c] / sum[3];                       // 0 to 255
                const int level = std::clamp(int(std::lround(v / 255.0 * 7.0)), 0, 7);
                rgb = (rgb << 8) | std::uint32_t(std::lround(level * 255.0 / 7.0));
            }
            colour[std::size_t(cy) * cw + cx] = rgb;
        }
    // Fitted as the SVG: the largest rectangle of the picture's shape, centred,
    // its top row towards max_y.
    const double scale = std::min((max_x - min_x) / image.width, (max_y - min_y) / image.height);
    const double left = min_x + 0.5 * ((max_x - min_x) - image.width * scale);
    const double top = max_y - 0.5 * ((max_y - min_y) - image.height * scale);
    const double cell = step * scale;
    const double right_edge = left + image.width * scale, bottom_edge = top - image.height * scale;
    // The runs, gathered by colour so that each colour is one group.
    std::map<std::uint32_t, std::vector<float>> by_colour;
    for (int cy = 0; cy < ch; ++cy)
        for (int cx = 0; cx < cw;) {
            const std::int64_t c = colour[std::size_t(cy) * cw + cx];
            int end = cx + 1;
            while (end < cw && colour[std::size_t(cy) * cw + end] == c)
                ++end;
            if (c >= 0) {
                const float x0 = float(left + cx * cell), x1 = float(std::min(right_edge, left + end * cell));
                const float y0 = float(top - cy * cell), y1 = float(std::max(bottom_edge, top - (cy + 1) * cell));
                std::vector<float>& v = by_colour[std::uint32_t(c)];
                const float zf = float(z);
                // Two triangles, counter-clockwise seen from above.
                for (const auto& [x, y] : {std::pair{x0, y1}, std::pair{x1, y1}, std::pair{x1, y0}, std::pair{x0, y1},
                                           std::pair{x1, y0}, std::pair{x0, y0}})
                    v.insert(v.end(), {x, y, zf, 0.0f, 0.0f, 1.0f});
            }
            cx = end;
        }
    for (const auto& [rgb, v] : by_colour) {
        BedArt::Group g;
        g.rgb = rgb;
        g.first = std::uint32_t(art.vertices.size() / 6);
        g.count = std::uint32_t(v.size() / 6);
        art.vertices.insert(art.vertices.end(), v.begin(), v.end());
        art.groups.push_back(g);
    }
    return art;
}

} // namespace tisma::ui
