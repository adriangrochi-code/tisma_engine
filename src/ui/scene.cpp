// Draw data of the 3D view. Own design; see PROVENANCE.md.

#include "ui/scene.hpp"

#include <oneapi/tbb/blocked_range.h>
#include <oneapi/tbb/parallel_for.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace tisma::ui {
namespace {

constexpr float kLineLift = 0.02f; // mm: grid lines sit this far above the plate

// The bed polygon's box; a unit box at the origin when there is none.
void bed_box(const BedPolygon& bed, double& x0, double& y0, double& x1, double& y1) {
    if (bed.empty()) {
        x0 = y0 = x1 = y1 = 0.0;
        return;
    }
    x0 = x1 = bed[0][0];
    y0 = y1 = bed[0][1];
    for (const auto& p : bed) {
        x0 = std::min(x0, p[0]), x1 = std::max(x1, p[0]);
        y0 = std::min(y0, p[1]), y1 = std::max(y1, p[1]);
    }
}

// Where the line x = c (axis 0) or y = c (axis 1) is inside the polygon: the
// crossings with its edges along the other axis, sorted, in pairs.
std::vector<double> crossings(const BedPolygon& bed, int axis_index, double c) {
    std::vector<double> t;
    const auto axis = static_cast<std::size_t>(axis_index), other = 1 - axis;
    for (std::size_t i = 0; i < bed.size(); ++i) {
        const auto& a = bed[i];
        const auto& b = bed[(i + 1) % bed.size()];
        // Half-open in the axis, so a line through a vertex counts it once.
        if ((a[axis] <= c) != (b[axis] <= c)) {
            const double s = (c - a[axis]) / (b[axis] - a[axis]);
            t.push_back(a[other] + s * (b[other] - a[other]));
        }
    }
    std::sort(t.begin(), t.end());
    return t;
}

void add_line(std::vector<LineVertex>& out, float x0, float y0, float x1, float y1, std::uint32_t argb) {
    out.push_back({x0, y0, kLineLift, argb});
    out.push_back({x1, y1, kLineLift, argb});
}

} // namespace

Mat4 shadow_view_projection(Vec3 towards, Vec3 lo, Vec3 hi) {
    const Vec3 f = normalize(towards * -1.0f); // the light looks this way
    const Vec3 up = std::fabs(f.z) < 0.99f ? Vec3{0, 0, 1} : Vec3{0, 1, 0};
    const Vec3 r = normalize(cross(f, up)), u = cross(r, f);
    float x0 = 1e30f, x1 = -1e30f, y0 = 1e30f, y1 = -1e30f, d0 = 1e30f, d1 = -1e30f;
    for (int k = 0; k < 8; ++k) {
        const Vec3 p{(k & 1) ? hi.x : lo.x, (k & 2) ? hi.y : lo.y, (k & 4) ? hi.z : lo.z};
        x0 = std::min(x0, dot(r, p)), x1 = std::max(x1, dot(r, p));
        y0 = std::min(y0, dot(u, p)), y1 = std::max(y1, dot(u, p));
        d0 = std::min(d0, dot(f, p)), d1 = std::max(d1, dot(f, p));
    }
    const float sx = 2.0f / std::max(x1 - x0, 1e-6f), sy = 2.0f / std::max(y1 - y0, 1e-6f);
    const float sd = 2.0f / std::max(d1 - d0, 1e-6f);
    Mat4 m{};
    const Vec3 rows[3] = {r * sx, u * sy, f * sd};
    const float offsets[3] = {-(x1 + x0) * sx / 2.0f, -(y1 + y0) * sy / 2.0f, -(d1 + d0) * sd / 2.0f};
    for (std::size_t row = 0; row < 3; ++row) {
        m[0 * 4 + row] = rows[row].x;
        m[1 * 4 + row] = rows[row].y;
        m[2 * 4 + row] = rows[row].z;
        m[3 * 4 + row] = offsets[row];
    }
    m[15] = 1.0f;
    return m;
}

Vec3 centre_on_bed(const BoundingBox& box, const BedPolygon& bed) {
    if (box.empty)
        return {};
    double x0, y0, x1, y1;
    bed_box(bed, x0, y0, x1, y1);
    const double cx = (x0 + x1) * 0.5, cy = (y0 + y1) * 0.5;
    return {float(cx - (double(box.min.x) + double(box.max.x)) * 0.5),
            float(cy - (double(box.min.y) + double(box.max.y)) * 0.5), -box.min.z};
}

TriangleMesh translated(const TriangleMesh& mesh, Vec3 offset) {
    TriangleMesh out;
    out.reserve(mesh.triangle_count());
    const auto moved = [&](const Vec3f& p) { return Vec3f{p.x + offset.x, p.y + offset.y, p.z + offset.z}; };
    for (std::size_t t = 0; t < mesh.triangle_count(); ++t)
        out.add_triangle(moved(mesh.corner(t, 0)), moved(mesh.corner(t, 1)), moved(mesh.corner(t, 2)));
    return out;
}

std::vector<float> mesh_vertices(const TriangleMesh& mesh, Vec3 offset) {
    // Each triangle's 18 floats in their own place, the triangles in
    // parallel: a model of millions of triangles (the owner's large-model item).
    std::vector<float> out(mesh.triangle_count() * 18);
    tbb::parallel_for(tbb::blocked_range<std::size_t>(0, mesh.triangle_count(), 4096),
                      [&](const tbb::blocked_range<std::size_t>& range) {
                          for (std::size_t t = range.begin(); t != range.end(); ++t) {
                              Vec3 p[3];
                              for (int k = 0; k < 3; ++k) {
                                  const Vec3f& c = mesh.corner(t, k);
                                  p[k] = Vec3{c.x, c.y, c.z} + offset;
                              }
                              const Vec3 n = normalize(cross(p[1] - p[0], p[2] - p[0]));
                              float* at = out.data() + 18 * t;
                              for (const Vec3& v : p) {
                                  const float row[6] = {v.x, v.y, v.z, n.x, n.y, n.z};
                                  at = std::copy(row, row + 6, at);
                              }
                          }
                      });
    return out;
}

std::vector<LineVertex> bed_lines(const BedPolygon& bed, const BedLook& look) {
    std::vector<LineVertex> out;
    if (bed.size() < 3 || look.step <= 0.0)
        return out;
    double x0, y0, x1, y1;
    bed_box(bed, x0, y0, x1, y1);
    for (int axis = 0; axis < 2; ++axis) {
        const double lo = axis == 0 ? x0 : y0, hi = axis == 0 ? x1 : y1;
        // Lines at whole multiples of the step, so the grid lines up with the
        // machine's coordinates whatever the bed's origin.
        const auto first = static_cast<long long>(std::ceil(lo / look.step));
        const auto last = static_cast<long long>(std::floor(hi / look.step));
        for (long long i = first; i <= last; ++i) {
            const double c = double(i) * look.step;
            if (std::abs(c - lo) < 1e-9 || std::abs(c - hi) < 1e-9)
                continue; // on the box's edge: the outline is there
            const std::uint32_t argb = i % look.major_every == 0 ? look.major : look.minor;
            const std::vector<double> t = crossings(bed, axis, c);
            for (std::size_t k = 0; k + 1 < t.size(); k += 2) {
                if (axis == 0)
                    add_line(out, float(c), float(t[k]), float(c), float(t[k + 1]), argb);
                else
                    add_line(out, float(t[k]), float(c), float(t[k + 1]), float(c), argb);
            }
        }
    }
    for (std::size_t i = 0; i < bed.size(); ++i) {
        const auto& a = bed[i];
        const auto& b = bed[(i + 1) % bed.size()];
        add_line(out, float(a[0]), float(a[1]), float(b[0]), float(b[1]), look.outline);
    }
    return out;
}

std::vector<float> bed_triangles(const BedPolygon& bed) {
    std::vector<float> out;
    for (std::size_t i = 1; i + 1 < bed.size(); ++i)
        for (const auto* p : {&bed[0], &bed[i], &bed[i + 1]})
            out.insert(out.end(), {float((*p)[0]), float((*p)[1]), 0.0f});
    return out;
}

float preview_value(const GcodePreview& preview, const PreviewMove& m, PreviewValue value) {
    const auto actual = [&] {
        const double length = std::hypot(double(m.x1) - m.x0, double(m.y1) - m.y0, double(m.z1) - m.z0);
        return m.seconds > 0.0f ? float(length / m.seconds) : m.feed;
    };
    const auto layer_seconds = [&] { return m.layer < preview.layers.size() ? preview.layers[m.layer].seconds : 0.0; };
    switch (value) {
    case PreviewValue::Feed: return m.feed;
    case PreviewValue::ActualFeed: return actual();
    case PreviewValue::Fan: return float(m.fan);
    case PreviewValue::Temperature: return float(m.temperature);
    case PreviewValue::Flow: return m.width * m.height * m.feed;
    case PreviewValue::ActualFlow: return m.width * m.height * actual();
    case PreviewValue::LayerTime: return float(layer_seconds());
    case PreviewValue::LayerTimeLog: return float(std::log10(std::max(layer_seconds(), 0.01)));
    case PreviewValue::Tool: return float(m.tool);
    case PreviewValue::ColorPrint: return float(m.color);
    }
    return m.feed;
}

std::vector<double> legend_steps(double lo, double hi, bool log) {
    if (!(hi > lo))
        return {log ? std::pow(10.0, lo) : lo};
    std::vector<double> out;
    for (int i = 0; i <= 10; ++i) {
        const double v = lo + (hi - lo) * i / 10.0;
        out.push_back(log ? std::pow(10.0, v) : v);
    }
    return out;
}

PreviewBuffers preview_buffers(const GcodePreview& preview, std::uint32_t travel_argb, PreviewValue value) {
    PreviewBuffers out;
    out.extrusions.reserve(preview.extrusions.size());
    out.layer_extrusions.reserve(preview.layers.size() + 1);
    out.layer_travels.reserve(preview.layers.size() + 1);
    bool first = true;
    // The bead goes on from the move before into this one: it ends where this
    // one starts, with no travel between, and turns by no more than
    // kPreviewJoinTurn. Both then end on the plane halfway between their
    // directions, so the corner closes with no ends sticking out and no faces
    // laid over each other.
    const float min_cos = std::cos(kPreviewJoinTurn * 3.14159265f / 180.0f);
    const auto join = [&](PreviewInstance& before, PreviewInstance& after) {
        if (before.x1 != after.x0 || before.y1 != after.y0 || before.z1 != after.z0)
            return;
        float ax = before.x1 - before.x0, ay = before.y1 - before.y0;
        float bx = after.x1 - after.x0, by = after.y1 - after.y0;
        const float la = std::hypot(ax, ay), lb = std::hypot(bx, by);
        if (!(la > 1e-6f) || !(lb > 1e-6f))
            return;
        ax /= la, ay /= la, bx /= lb, by /= lb;
        if (ax * bx + ay * by < min_cos)
            return;
        const float nx = ax + bx, ny = ay + by, ln = std::hypot(nx, ny);
        before.join1_x = after.join0_x = nx / ln;
        before.join1_y = after.join0_y = ny / ln;
    };
    for (const PreviewLayer& layer : preview.layers) {
        out.layer_extrusions.push_back(out.extrusions.size());
        out.layer_travels.push_back(out.travels.size());
        bool joinable = false; // the last instance is the move just before this one
        for (std::size_t i = layer.first; i < layer.first + layer.count; ++i) {
            const PreviewMove& m = preview.moves[i];
            if (m.kind == LineKind::Travel) {
                out.travels.push_back({m.x0, m.y0, m.z0, travel_argb});
                out.travels.push_back({m.x1, m.y1, m.z1, travel_argb});
                joinable = false;
                continue;
            }
            PreviewInstance p;
            p.x0 = m.x0, p.y0 = m.y0, p.z0 = m.z0;
            p.x1 = m.x1, p.y1 = m.y1, p.z1 = m.z1;
            p.width = m.width, p.height = m.height, p.feed = preview_value(preview, m, value);
            p.kind_layer = static_cast<std::uint32_t>(m.kind) | (m.layer << 8);
            if (joinable)
                join(out.extrusions.back(), p);
            out.extrusions.push_back(p);
            joinable = true;
            out.min_feed = first ? p.feed : std::min(out.min_feed, p.feed);
            out.max_feed = first ? p.feed : std::max(out.max_feed, p.feed);
            out.min_width = first ? m.width : std::min(out.min_width, m.width);
            out.max_width = first ? m.width : std::max(out.max_width, m.width);
            out.min_height = first ? m.height : std::min(out.min_height, m.height);
            out.max_height = first ? m.height : std::max(out.max_height, m.height);
            first = false;
        }
    }
    out.layer_extrusions.push_back(out.extrusions.size());
    out.layer_travels.push_back(out.travels.size());
    out.hidden = hidden_inside(out);
    return out;
}

namespace {

struct Edge {
    double x0, y0, x1, y1;
};

// A grid over part of the bed, one byte per cell.
struct Grid {
    double x0 = 0, y0 = 0, cell = 1;
    int cols = 0, rows = 0;
    std::vector<std::uint8_t> on;
    std::uint8_t at(int c, int r) const {
        return c < 0 || r < 0 || c >= cols || r >= rows ? 0 : on[std::size_t(r) * std::size_t(cols) + std::size_t(c)];
    }
    int col(double x) const { return int(std::floor((x - x0) / cell)); }
    int row(double y) const { return int(std::floor((y - y0) / cell)); }
};

// What the edges enclose, by the even-odd rule along each row of cell
// centres. A row they cross an odd number of times (a wall not closed) stays
// outside: not known.
void fill_enclosed(const std::vector<Edge>& edges, Grid& g) {
    g.on.assign(std::size_t(g.rows) * std::size_t(g.cols), 0);
    std::vector<std::vector<double>> crossings(static_cast<std::size_t>(g.rows));
    for (const Edge& e : edges) {
        if (e.y0 == e.y1)
            continue;
        const double ya = std::min(e.y0, e.y1), yb = std::max(e.y0, e.y1);
        // The rows whose centre lies in [ya, yb): each edge counted once where two meet.
        const int r0 = std::max(0, int(std::ceil((ya - g.y0) / g.cell - 0.5)));
        const int r1 = std::min(g.rows - 1, int(std::ceil((yb - g.y0) / g.cell - 0.5)) - 1);
        for (int r = r0; r <= r1; ++r) {
            const double y = g.y0 + (r + 0.5) * g.cell;
            crossings[std::size_t(r)].push_back(e.x0 + (y - e.y0) * (e.x1 - e.x0) / (e.y1 - e.y0));
        }
    }
    for (int r = 0; r < g.rows; ++r) {
        std::vector<double>& xs = crossings[std::size_t(r)];
        if (xs.size() % 2 != 0)
            continue;
        std::sort(xs.begin(), xs.end());
        for (std::size_t k = 0; k + 1 < xs.size(); k += 2) {
            const int c0 = std::max(0, int(std::ceil((xs[k] - g.x0) / g.cell - 0.5)));
            const int c1 = std::min(g.cols - 1, int(std::ceil((xs[k + 1] - g.x0) / g.cell - 0.5)) - 1);
            if (c0 <= c1)
                std::fill_n(g.on.begin() + std::ptrdiff_t(std::size_t(r) * std::size_t(g.cols) + std::size_t(c0)),
                            c1 - c0 + 1, std::uint8_t(1));
        }
    }
}

double distance_to(const Edge& e, double x, double y) {
    const double dx = e.x1 - e.x0, dy = e.y1 - e.y0, l2 = dx * dx + dy * dy;
    const double t = l2 > 0.0 ? std::clamp(((x - e.x0) * dx + (y - e.y0) * dy) / l2, 0.0, 1.0) : 0.0;
    return std::hypot(x - (e.x0 + t * dx), y - (e.y0 + t * dy));
}

// Calls step(x, y) at points every `spacing` along `from` that no edge of
// `to` passes within `tolerance` of: where one layer's outer wall steps away
// from the other's.
template <class Step>
void steps_between(const std::vector<Edge>& from, const std::vector<Edge>& to, double tolerance, double spacing,
                   Step step) {
    // The edges of `to` in bins of 1 mm (or more), for the nearby ones only.
    double bx0 = 1e300, by0 = 1e300, bx1 = -1e300, by1 = -1e300;
    for (const Edge& e : to)
        bx0 = std::min({bx0, e.x0, e.x1}), bx1 = std::max({bx1, e.x0, e.x1}), by0 = std::min({by0, e.y0, e.y1}),
        by1 = std::max({by1, e.y0, e.y1});
    const double bin = std::max({1.0, (bx1 - bx0) / 256.0, (by1 - by0) / 256.0});
    const int cols = to.empty() ? 0 : int((bx1 - bx0) / bin) + 1, rows = to.empty() ? 0 : int((by1 - by0) / bin) + 1;
    std::vector<std::vector<std::uint32_t>> bins(std::size_t(cols) * std::size_t(rows));
    for (std::uint32_t i = 0; i < to.size(); ++i) {
        const Edge& e = to[i];
        const int c0 = int((std::min(e.x0, e.x1) - tolerance - bx0) / bin), c1 = int((std::max(e.x0, e.x1) + tolerance - bx0) / bin);
        const int r0 = int((std::min(e.y0, e.y1) - tolerance - by0) / bin), r1 = int((std::max(e.y0, e.y1) + tolerance - by0) / bin);
        for (int r = std::max(r0, 0); r <= std::min(r1, rows - 1); ++r)
            for (int c = std::max(c0, 0); c <= std::min(c1, cols - 1); ++c)
                bins[std::size_t(r) * std::size_t(cols) + std::size_t(c)].push_back(i);
    }
    const auto near = [&](double x, double y) {
        const int c = int(std::floor((x - bx0) / bin)), r = int(std::floor((y - by0) / bin));
        if (c < 0 || r < 0 || c >= cols || r >= rows)
            return false;
        for (const std::uint32_t i : bins[std::size_t(r) * std::size_t(cols) + std::size_t(c)])
            if (distance_to(to[i], x, y) <= tolerance)
                return true;
        return false;
    };
    for (const Edge& e : from) {
        const double length = std::hypot(e.x1 - e.x0, e.y1 - e.y0);
        const int n = std::max(1, int(std::ceil(length / spacing)));
        for (int k = 0; k <= n; ++k) {
            const double f = double(k) / n, x = e.x0 + f * (e.x1 - e.x0), y = e.y0 + f * (e.y1 - e.y0);
            if (!near(x, y))
                step(x, y);
        }
    }
}

} // namespace

std::vector<std::uint8_t> hidden_inside(const PreviewBuffers& p, double cell) {
    std::vector<std::uint8_t> hidden(p.extrusions.size(), 0);
    if (p.layer_extrusions.size() < 4) // fewer than 3 layers: none between a first and a last
        return hidden;
    const std::size_t layers = p.layer_extrusions.size() - 1;
    const auto kind = [](const PreviewInstance& e) { return LineKind(e.kind_layer & 0xFFu); };
    // Each layer's outer walls.
    std::vector<std::vector<Edge>> walls(layers);
    double x0 = 1e300, y0 = 1e300, x1 = -1e300, y1 = -1e300;
    for (std::size_t l = 0; l < layers; ++l)
        for (std::size_t i = p.layer_extrusions[l]; i < p.layer_extrusions[l + 1]; ++i) {
            const PreviewInstance& e = p.extrusions[i];
            if (kind(e) != LineKind::OuterWall && kind(e) != LineKind::OverhangWall)
                continue;
            walls[l].push_back({e.x0, e.y0, e.x1, e.y1});
            x0 = std::min({x0, double(e.x0), double(e.x1)}), x1 = std::max({x1, double(e.x0), double(e.x1)});
            y0 = std::min({y0, double(e.y0), double(e.y1)}), y1 = std::max({y1, double(e.y0), double(e.y1)});
        }
    if (!(x1 > x0) || !(y1 > y0))
        return hidden;
    if (!(cell > 0.0))
        cell = std::max(0.2, std::max(x1 - x0, y1 - y0) / 1024.0);
    // Where one layer's outer wall steps in or out from the next, the groove
    // between their beads opens onto the beads inside: about two beads in.
    const double width = std::max(double(p.max_width), 0.1);
    const double reach = 2.0 * width + cell;
    const double tolerance = 0.05; // mm two walls may differ by and still close the groove
    const int stamp = int(std::ceil(reach / cell));
    tbb::parallel_for(std::size_t(1), layers - 1, [&](std::size_t l) {
        const std::vector<Edge>&below = walls[l - 1], &here = walls[l], &above = walls[l + 1];
        if (below.empty() || here.empty() || above.empty())
            return;
        // A grid over the three layers' walls, with room for the reach.
        double gx0 = 1e300, gy0 = 1e300, gx1 = -1e300, gy1 = -1e300;
        for (const std::vector<Edge>* w : {&below, &here, &above})
            for (const Edge& e : *w)
                gx0 = std::min({gx0, e.x0, e.x1}), gx1 = std::max({gx1, e.x0, e.x1}), gy0 = std::min({gy0, e.y0, e.y1}),
                gy1 = std::max({gy1, e.y0, e.y1});
        Grid g;
        g.cell = cell;
        g.x0 = gx0 - reach - 2.0 * cell, g.y0 = gy0 - reach - 2.0 * cell;
        g.cols = int(std::ceil((gx1 - g.x0 + reach) / cell)) + 3;
        g.rows = int(std::ceil((gy1 - g.y0 + reach) / cell)) + 3;
        Grid under = g, over = g, steps = g;
        fill_enclosed(below, under);
        fill_enclosed(above, over);
        steps.on.assign(std::size_t(g.rows) * std::size_t(g.cols), 0);
        const auto mark = [&](double x, double y) {
            const int c = steps.col(x), r = steps.row(y);
            for (int rr = std::max(r - stamp, 0); rr <= std::min(r + stamp, g.rows - 1); ++rr)
                for (int cc = std::max(c - stamp, 0); cc <= std::min(c + stamp, g.cols - 1); ++cc)
                    steps.on[std::size_t(rr) * std::size_t(g.cols) + std::size_t(cc)] = 1;
        };
        for (const std::vector<Edge>* other : {&below, &above}) {
            steps_between(here, *other, tolerance, cell, mark);
            steps_between(*other, here, tolerance, cell, mark);
        }
        // Inside with a cell to spare: the cell and the eight around it.
        const auto inside = [](const Grid& in, int c, int r) {
            for (int dr = -1; dr <= 1; ++dr)
                for (int dc = -1; dc <= 1; ++dc)
                    if (!in.at(c + dc, r + dr))
                        return false;
            return true;
        };
        for (std::size_t i = p.layer_extrusions[l]; i < p.layer_extrusions[l + 1]; ++i) {
            const PreviewInstance& e = p.extrusions[i];
            if (kind(e) != LineKind::InnerWall && kind(e) != LineKind::SparseInfill)
                continue;
            const double length = std::hypot(double(e.x1) - e.x0, double(e.y1) - e.y0);
            const int n = std::max(2, int(std::ceil(length / cell)));
            bool covered = true;
            for (int k = 0; k <= n && covered; ++k) {
                const double f = double(k) / n;
                const int c = g.col(e.x0 + f * (double(e.x1) - e.x0)), r = g.row(e.y0 + f * (double(e.y1) - e.y0));
                covered = inside(under, c, r) && inside(over, c, r) && !steps.at(c, r);
            }
            hidden[i] = covered ? 1 : 0;
        }
    });
    return hidden;
}

PreviewDrawOrder preview_draw_order(const PreviewBuffers& p) {
    PreviewDrawOrder out;
    const std::size_t layers = p.layer_extrusions.empty() ? 0 : p.layer_extrusions.size() - 1;
    out.instances.reserve(p.extrusions.size());
    out.layer_shown.assign(1, 0);
    for (const bool hidden : {false, true}) {
        if (hidden)
            out.layer_hidden.push_back(out.instances.size());
        for (std::size_t l = 0; l < layers; ++l) {
            for (std::size_t i = p.layer_extrusions[l]; i < p.layer_extrusions[l + 1]; ++i)
                if ((i < p.hidden.size() && p.hidden[i] != 0) == hidden)
                    out.instances.push_back(p.extrusions[i]);
            (hidden ? out.layer_hidden : out.layer_shown).push_back(out.instances.size());
        }
    }
    for (std::size_t i = 0; i < p.hidden.size(); ++i)
        if (p.hidden[i] != 0)
            out.hidden_kinds |= 1u << (p.extrusions[i].kind_layer & 0xFFu);
    out.layer_z.assign(layers, 0.0f);
    for (std::size_t l = 0; l < layers; ++l) {
        float z = l > 0 ? out.layer_z[l - 1] : 0.0f;
        bool any = false;
        for (std::size_t i = p.layer_extrusions[l]; i < p.layer_extrusions[l + 1]; ++i) {
            const PreviewInstance& e = p.extrusions[i];
            z = any ? std::max({z, e.z0, e.z1}) : std::max(e.z0, e.z1);
            any = true;
        }
        out.layer_z[l] = z;
    }
    // A bead reaches half its width out, its free ends as far, and a mitre
    // up to its width past the point; it hangs one height under the path.
    out.layer_box.assign(layers, {1e30f, 1e30f, 1e30f, -1e30f, -1e30f, -1e30f});
    for (std::size_t l = 0; l < layers; ++l) {
        std::array<float, 6>& b = out.layer_box[l];
        for (std::size_t i = p.layer_extrusions[l]; i < p.layer_extrusions[l + 1]; ++i) {
            const PreviewInstance& e = p.extrusions[i];
            const float r = e.width + e.height;
            b = {std::min({b[0], e.x0 - r, e.x1 - r}), std::min({b[1], e.y0 - r, e.y1 - r}),
                 std::min({b[2], e.z0 - r, e.z1 - r}), std::max({b[3], e.x0 + r, e.x1 + r}),
                 std::max({b[4], e.y0 + r, e.y1 + r}), std::max({b[5], e.z0 + r, e.z1 + r})};
        }
    }
    return out;
}

bool box_outside(const Mat4& m, const std::array<float, 6>& box) {
    if (box[0] > box[3])
        return true; // empty
    // Per plane of the volume (-w <= x, y, z <= w), whether every corner is beyond it.
    std::array<bool, 6> beyond;
    beyond.fill(true);
    for (int c = 0; c < 8; ++c) {
        const float x = box[c & 1 ? 3 : 0], y = box[c & 2 ? 4 : 1], z = box[c & 4 ? 5 : 2];
        const float cx = m[0] * x + m[4] * y + m[8] * z + m[12];
        const float cy = m[1] * x + m[5] * y + m[9] * z + m[13];
        const float cz = m[2] * x + m[6] * y + m[10] * z + m[14];
        const float cw = m[3] * x + m[7] * y + m[11] * z + m[15];
        beyond[0] = beyond[0] && cx < -cw, beyond[1] = beyond[1] && cx > cw;
        beyond[2] = beyond[2] && cy < -cw, beyond[3] = beyond[3] && cy > cw;
        beyond[4] = beyond[4] && cz < -cw, beyond[5] = beyond[5] && cz > cw;
    }
    return std::any_of(beyond.begin(), beyond.end(), [](bool b) { return b; });
}

std::array<PreviewRange, 2> preview_ranges(const PreviewDrawOrder& order, std::size_t lo, std::size_t hi,
                                           std::uint32_t kind_mask, std::size_t top_shown, std::size_t top_hidden) {
    if (order.layer_shown.size() < 2 || order.layer_hidden.size() != order.layer_shown.size())
        return {};
    const std::size_t layers = order.layer_shown.size() - 1;
    lo = std::min(lo, layers - 1);
    hi = std::clamp(hi, lo, layers - 1);
    const bool cut = top_shown != kWholeLayer || top_hidden != kWholeLayer;
    const std::uint32_t kinds_drawn = kind_mask | order.hidden_kinds | (1u << unsigned(LineKind::Travel));
    const bool whole = lo == 0 && hi == layers - 1 && kinds_drawn == 0xFFFFFFFFu && !cut;
    const std::size_t shown_end = std::min(order.layer_shown[hi + 1],
                                           top_shown == kWholeLayer ? order.layer_shown[hi + 1] : order.layer_shown[hi] + top_shown);
    const std::size_t hidden_end =
        std::min(order.layer_hidden[hi + 1],
                 top_hidden == kWholeLayer ? order.layer_hidden[hi + 1] : order.layer_hidden[hi] + top_hidden);
    return {PreviewRange{order.layer_shown[lo], shown_end - order.layer_shown[lo]},
            PreviewRange{order.layer_hidden[lo], whole ? 0 : hidden_end - order.layer_hidden[lo]}};
}

std::vector<PreviewRange> preview_draws(const PreviewDrawOrder& order, std::size_t lo, std::size_t hi,
                                        std::uint32_t kind_mask, float eye_z, std::size_t top_shown,
                                        std::size_t top_hidden, const Mat4* clip) {
    std::vector<PreviewRange> out;
    const std::array<PreviewRange, 2> ranges = preview_ranges(order, lo, hi, kind_mask, top_shown, top_hidden);
    if (ranges[0].count + ranges[1].count == 0)
        return out;
    const std::size_t layers = order.layer_shown.size() - 1;
    lo = std::min(lo, layers - 1);
    hi = std::clamp(hi, lo, layers - 1);
    // The first layer above the eye.
    std::size_t split = lo;
    while (split <= hi && split < order.layer_z.size() && order.layer_z[split] <= eye_z)
        ++split;
    const auto add = [&](const PreviewRange& range, const std::vector<std::size_t>& starts, std::size_t l) {
        if (clip && l < order.layer_box.size() && box_outside(*clip, order.layer_box[l]))
            return;
        const std::size_t b = std::max(range.first, starts[l]);
        const std::size_t e = std::min(range.first + range.count, starts[l + 1]);
        if (e <= b)
            return;
        if (!out.empty() && out.back().first + out.back().count == b)
            out.back().count += e - b; // the next layer up, right after this one
        else
            out.push_back({b, e - b});
    };
    for (int k = 0; k < 2; ++k) {
        if (ranges[std::size_t(k)].count == 0)
            continue;
        const std::vector<std::size_t>& starts = k == 0 ? order.layer_shown : order.layer_hidden;
        for (std::size_t l = split; l-- > lo;)
            add(ranges[std::size_t(k)], starts, l);
        for (std::size_t l = split; l <= hi; ++l)
            add(ranges[std::size_t(k)], starts, l);
    }
    return out;
}

std::uint32_t kind_colour(LineKind kind) {
    switch (kind) {
    case LineKind::Travel: return 0x4FA3FF;
    case LineKind::OuterWall: return 0xF28C28;
    case LineKind::InnerWall: return 0xF5CF47;
    case LineKind::OverhangWall: return 0x3A86FF;
    case LineKind::SparseInfill: return 0xC2263E;
    case LineKind::SolidInfill: return 0x9D40E9;
    case LineKind::TopSolidInfill: return 0xFF6F91;
    case LineKind::BridgeInfill: return 0x5BC0EB;
    case LineKind::GapFill: return 0xF2F2F4;
    case LineKind::Ironing: return 0x9AE6B4;
    case LineKind::SkirtBrim: return 0x2EC4B6;
    case LineKind::Support: return 0x43AA8B;
    case LineKind::SupportInterface: return 0x90BE6D;
    case LineKind::WipeTower: return 0xB2B2BC;
    case LineKind::Custom: return 0x808080;
    case LineKind::Other: return 0x6C6C78;
    }
    return 0x808080;
}

TriangleMesh clustered(const TriangleMesh& mesh, std::size_t target) {
    const std::size_t n = mesh.triangle_count();
    if (n <= target || target == 0)
        return mesh;
    // A regular surface mesh has about two triangles per vertex: cells of
    // side h hold one vertex each over the area A, so h = sqrt(2 A / target).
    double area = 0.0;
    for (std::size_t t = 0; t < n; ++t) {
        const Vec3f &a = mesh.corner(t, 0), &b = mesh.corner(t, 1), &c = mesh.corner(t, 2);
        const Vec3 u{b.x - a.x, b.y - a.y, b.z - a.z}, v{c.x - a.x, c.y - a.y, c.z - a.z};
        area += 0.5 * double(length(cross(u, v)));
    }
    const BoundingBox box = mesh.bounding_box();
    // No finer than 2^21 cells along an axis, the key's room.
    const double extent = std::max({double(box.max.x - box.min.x), double(box.max.y - box.min.y),
                                    double(box.max.z - box.min.z), 1e-3});
    const double h = std::max(std::sqrt(2.0 * area / double(target)), extent / 2.0e6);
    const auto cell_of = [&](const Vec3f& p) {
        const auto at = [&](float x, float lo) { return std::uint64_t(std::max(0.0, std::floor((double(x) - lo) / h))); };
        return at(p.x, box.min.x) | at(p.y, box.min.y) << 21 | at(p.z, box.min.z) << 42;
    };
    // Each cell's index, its corners' sum and count: an open-addressing table
    // twice as large as the cells there may be.
    std::size_t size = 1;
    while (size < 4 * target)
        size <<= 1;
    std::vector<std::uint64_t> keys(size, ~std::uint64_t(0));
    std::vector<std::uint32_t> slot_cell(size, 0);
    std::vector<std::array<double, 3>> sum;
    std::vector<std::uint32_t> count;
    std::vector<std::uint32_t> corner_cell(3 * n);
    for (std::size_t i = 0; i < 3 * n; ++i) {
        const Vec3f& p = mesh.corners()[i];
        const std::uint64_t key = cell_of(p);
        std::size_t at = std::size_t((key * 0x9E3779B97F4A7C15ull) >> 20) & (size - 1);
        while (keys[at] != key && keys[at] != ~std::uint64_t(0))
            at = (at + 1) & (size - 1);
        if (keys[at] != key) {
            if (sum.size() * 2 >= size) // a full table: the grid was too fine for this shape
                return mesh;
            keys[at] = key;
            slot_cell[at] = std::uint32_t(sum.size());
            sum.push_back({0.0, 0.0, 0.0});
            count.push_back(0);
        }
        const std::uint32_t c = slot_cell[at];
        sum[c][0] += p.x, sum[c][1] += p.y, sum[c][2] += p.z;
        ++count[c];
        corner_cell[i] = c;
    }
    // The surviving triangles over their cells, each once, wound as they
    // were (the smallest cell first, the order kept).
    std::vector<std::array<std::uint32_t, 3>> kept;
    kept.reserve(target * 2);
    for (std::size_t t = 0; t < n; ++t) {
        std::array<std::uint32_t, 3> c{corner_cell[3 * t], corner_cell[3 * t + 1], corner_cell[3 * t + 2]};
        if (c[0] == c[1] || c[1] == c[2] || c[0] == c[2])
            continue;
        std::rotate(c.begin(), std::min_element(c.begin(), c.end()), c.end());
        kept.push_back(c);
    }
    std::sort(kept.begin(), kept.end());
    kept.erase(std::unique(kept.begin(), kept.end()), kept.end());
    std::vector<Vec3f> corners;
    corners.reserve(3 * kept.size());
    for (const auto& c : kept)
        for (const std::uint32_t k : c)
            corners.push_back(Vec3f{float(sum[k][0] / count[k]), float(sum[k][1] / count[k]), float(sum[k][2] / count[k])});
    return TriangleMesh(std::move(corners));
}

} // namespace tisma::ui
