// G-code output (docs/spec/GCODE_OUTPUT.md). Own design; see PROVENANCE.md.

#include "gcode/writer.hpp"

#include "gcode/arcs.hpp"
#include "gcode/cooling.hpp"
#include "gcode/dialect.hpp"
#include "gcode/order.hpp"
#include "gcode/print_time.hpp"
#include "gcode/resolution.hpp"
#include "gcode/template.hpp"
#include "gcode/scarf.hpp"
#include "gcode/travel.hpp"
#include "infill/nonplanar.hpp"

#include <oneapi/tbb/parallel_for.h>
#include <oneapi/tbb/parallel_pipeline.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <limits>
#include <map>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <unordered_map>
#include <set>
#include <numbers>
#include <optional>
#include <sstream>
#include <string_view>
#include <thread>

namespace tisma {
namespace {

// A number with fixed decimals, as printf's "%.*f" writes it (std::to_chars
// is specified to round the same way), without a negative zero. Returns the
// length written to buffer.
//
// Most numbers take a fast path that gives the same text: the value times
// 10^decimals, rounded to a whole number, with the point put back. The
// product's own rounding error is under 2^-12 below 2^40, so a fraction more
// than 0.001 away from one half rounds the same way as the exact value; the
// rest (near a half, huge, or more than 9 decimals) go to std::to_chars.
constexpr std::size_t kNumberCapacity = 400; // any finite double with a few decimals
std::size_t format_fixed(char* buffer, double v, int decimals) {
    static constexpr double kScale[] = {1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9};
    static constexpr std::uint64_t kDivisor[] = {1,         10,         100,         1000,         10000,
                                                 100000,    1000000,    10000000,    100000000,    1000000000};
    if (decimals >= 0 && decimals <= 9) {
        const double scaled = std::abs(v) * kScale[decimals];
        if (scaled < 1099511627776.0) { // 2^40; false for NaN
            const double whole = std::floor(scaled);
            const double fraction = scaled - whole;
            if (std::abs(fraction - 0.5) > 1e-3) {
                const std::uint64_t n = std::uint64_t(whole) + (fraction > 0.5 ? 1u : 0u);
                const std::uint64_t divisor = kDivisor[decimals];
                char* p = buffer;
                if (v < 0.0 && n != 0)
                    *p++ = '-';
                p = std::to_chars(p, buffer + kNumberCapacity, n / divisor).ptr;
                if (decimals > 0) {
                    *p++ = '.';
                    std::uint64_t rest = n % divisor;
                    for (int i = decimals - 1; i >= 0; --i) {
                        p[i] = char('0' + rest % 10);
                        rest /= 10;
                    }
                    p += decimals;
                }
                return std::size_t(p - buffer);
            }
        }
    }
    const auto result = std::to_chars(buffer, buffer + kNumberCapacity, v, std::chars_format::fixed, decimals);
    std::size_t n = std::size_t(result.ptr - buffer);
    if (n > 1 && buffer[0] == '-' && std::all_of(buffer + 1, buffer + n, [](char c) { return c == '0' || c == '.'; })) {
        std::memmove(buffer, buffer + 1, n - 1);
        --n;
    }
    return n;
}

std::string fixed(double v, int decimals) {
    char buffer[kNumberCapacity];
    return std::string(buffer, format_fixed(buffer, v, decimals));
}

// The start of a line that holds an M73's place (Emitter::progress_mark):
// no G-code has this byte.
constexpr char kProgressMark[] = "\x01M73 ";

// v with up to `decimals` decimals and no zeros at the end ("252.45", "255").
std::string trimmed(double v, int decimals) {
    std::string t = fixed(v, decimals);
    if (t.find('.') != std::string::npos) {
        t.erase(t.find_last_not_of('0') + 1);
        if (t.back() == '.')
            t.pop_back();
    }
    return t;
}

} // namespace

std::string fixed_number(double v, int decimals) { return fixed(v, decimals); }

namespace {

// The stretches of a wall whose bead lies wholly off the layer below
// (walls_extra.md, rule 8), by each piece's middle in `hanging`, and of those
// the ones in `on_support`, which fill the support's Z gap `gap`
// (supports_skirt.md, rule 4). Without `detect`, only the latter. The wall is
// cut where its stretches change, keeping its order from its start (a loop's
// seam), into open pieces; a wall all of one kind stays whole. Each piece says
// whether it is an overhang perimeter; over the air, its bead is the bridges'
// round one (`round`, when given: walls_extra.md, rule 8). The same cuts split
// the solid infill right over a bridge (speeds_accels.md, rule 2).
struct WallPiece {
    Toolpath path;
    bool overhang = false;
    bool air = false; // over the air
};
struct RoundBead {
    double width = 0.0; // mm, the bead's diameter
    double gap = 0.0;   // mm the bead is thicker than the layer
};
// A region prepared for many tests along a layer's walls: each polygon's box
// for the point tests, and the edges in a grid of cells for the crossings.
class AreaIndex {
public:
    explicit AreaIndex(const Polygons& region) : region_(region) {
        box_ = {std::numeric_limits<double>::max(), std::numeric_limits<double>::max(),
                std::numeric_limits<double>::lowest(), std::numeric_limits<double>::lowest()};
        for (const Polygon& poly : region) {
            std::array<double, 4> b = {std::numeric_limits<double>::max(), std::numeric_limits<double>::max(),
                                       std::numeric_limits<double>::lowest(), std::numeric_limits<double>::lowest()};
            const auto first = std::uint32_t(edges_.size());
            for (std::size_t i = 0; i < poly.size(); ++i) {
                const Point2d p{to_mm(poly[i].x), to_mm(poly[i].y)};
                const Point2d q{to_mm(poly[(i + 1) % poly.size()].x), to_mm(poly[(i + 1) % poly.size()].y)};
                const auto k = std::uint32_t(edges_.size());
                prev_.push_back(i == 0 ? first + std::uint32_t(poly.size()) - 1 : k - 1);
                next_.push_back(i + 1 == poly.size() ? first : k + 1);
                edges_.push_back({p, q});
                b = {std::min(b[0], p.x), std::min(b[1], p.y), std::max(b[2], p.x), std::max(b[3], p.y)};
            }
            boxes_.push_back(b);
            box_ = {std::min(box_[0], b[0]), std::min(box_[1], b[1]), std::max(box_[2], b[2]), std::max(box_[3], b[3])};
        }
        if (edges_.empty())
            return;
        // About as many cells as edges, at most 512 a side.
        const double w = std::max(box_[2] - box_[0], 1e-6), h = std::max(box_[3] - box_[1], 1e-6);
        cell_ = std::max(std::sqrt(w * h / double(edges_.size())), std::max(w, h) / 512.0);
        nx_ = std::size_t(w / cell_) + 1, ny_ = std::size_t(h / cell_) + 1;
        cells_.resize(nx_ * ny_);
        for (std::uint32_t e = 0; e < edges_.size(); ++e) {
            const auto [x0, y0, x1, y1] = cell_box(edges_[e].a, edges_[e].b);
            for (std::size_t y = y0; y <= y1; ++y)
                for (std::size_t x = x0; x <= x1; ++x)
                    cells_[y * nx_ + x].push_back(e);
        }
        seen_.assign(edges_.size(), 0);
    }

    // Whether the segment from a to b can meet the region at all.
    bool near(const Point2d& a, const Point2d& b) const {
        return !edges_.empty() && std::max(a.x, b.x) >= box_[0] && std::min(a.x, b.x) <= box_[2] &&
               std::max(a.y, b.y) >= box_[1] && std::min(a.y, b.y) <= box_[3];
    }

    // As region_contains, looking only at the polygons whose box holds the point.
    bool contains(double x, double y) const {
        if (edges_.empty() || x < box_[0] || x > box_[2] || y < box_[1] || y > box_[3])
            return false;
        const Clipper2Lib::Point64 q{to_units(x), to_units(y)};
        int winding = 0;
        for (std::size_t k = 0; k < region_.size(); ++k) {
            const auto& b = boxes_[k];
            if (x < b[0] || x > b[2] || y < b[1] || y > b[3])
                continue;
            const Clipper2Lib::PointInPolygonResult r = Clipper2Lib::PointInPolygon(q, region_[k]);
            if (r == Clipper2Lib::PointInPolygonResult::IsOn)
                return true;
            if (r == Clipper2Lib::PointInPolygonResult::IsInside)
                winding += Clipper2Lib::IsPositive(region_[k]) ? 1 : -1;
        }
        return winding != 0;
    }

    // The outline's nearest point to p within `reach`: how far, and whether p
    // is inside. The region lies left of its edges (Clipper's orientation);
    // nearest to a corner, p is outside a convex corner and inside a reflex one.
    struct Nearest {
        double distance;
        bool inside;
    };
    std::optional<Nearest> nearest(const Point2d& p, double reach) const {
        if (edges_.empty())
            return std::nullopt;
        double best = reach;
        std::optional<Nearest> out;
        const auto [x0, y0, x1, y1] = cell_box({p.x - reach, p.y - reach}, {p.x + reach, p.y + reach});
        const auto turn = [&](std::uint32_t in, std::uint32_t out_edge) { // left turn: convex
            const Edge &a = edges_[in], &b = edges_[out_edge];
            return (a.b.x - a.a.x) * (b.b.y - b.a.y) - (a.b.y - a.a.y) * (b.b.x - b.a.x) > 0.0;
        };
        for (std::size_t y = y0; y <= y1; ++y)
            for (std::size_t x = x0; x <= x1; ++x)
                for (const std::uint32_t k : cells_[y * nx_ + x]) {
                    const Edge& e = edges_[k];
                    const double ex = e.b.x - e.a.x, ey = e.b.y - e.a.y;
                    const double l2 = ex * ex + ey * ey;
                    if (!(l2 > 0.0))
                        continue;
                    const double u = ((p.x - e.a.x) * ex + (p.y - e.a.y) * ey) / l2;
                    const double c = std::clamp(u, 0.0, 1.0);
                    const double d = std::hypot(e.a.x + c * ex - p.x, e.a.y + c * ey - p.y);
                    if (out ? d >= best : d > best)
                        continue;
                    best = d;
                    const bool inside = u <= 0.0   ? !turn(prev_[k], k)
                                        : u >= 1.0 ? !turn(k, next_[k])
                                                   : ex * (p.y - e.a.y) - ey * (p.x - e.a.x) > 0.0;
                    out = Nearest{d, inside};
                }
        return out;
    }

    // The distance from p to the region's outline when it is under `reach`;
    // else `reach`.
    double distance(const Point2d& p, double reach) const {
        double best = reach;
        if (edges_.empty())
            return best;
        const auto [x0, y0, x1, y1] = cell_box({p.x - reach, p.y - reach}, {p.x + reach, p.y + reach});
        for (std::size_t y = y0; y <= y1; ++y)
            for (std::size_t x = x0; x <= x1; ++x)
                for (const std::uint32_t k : cells_[y * nx_ + x]) {
                    const Edge& e = edges_[k];
                    const double ex = e.b.x - e.a.x, ey = e.b.y - e.a.y;
                    const double l2 = ex * ex + ey * ey;
                    const double u = l2 > 0.0 ? std::clamp(((p.x - e.a.x) * ex + (p.y - e.a.y) * ey) / l2, 0.0, 1.0) : 0.0;
                    best = std::min(best, std::hypot(e.a.x + u * ex - p.x, e.a.y + u * ey - p.y));
                }
        return best;
    }

    // Where along the segment from a to b (0 to 1, ends left out) it crosses
    // the region's outline, added to `at`.
    void crossings(const Point2d& a, const Point2d& b, std::vector<double>& at) {
        if (!near(a, b))
            return;
        ++stamp_;
        const double dx = b.x - a.x, dy = b.y - a.y;
        const auto [x0, y0, x1, y1] = cell_box(a, b);
        for (std::size_t y = y0; y <= y1; ++y)
            for (std::size_t x = x0; x <= x1; ++x)
                for (const std::uint32_t k : cells_[y * nx_ + x]) {
                    if (seen_[k] == stamp_)
                        continue;
                    seen_[k] = stamp_;
                    const Edge& e = edges_[k];
                    if (std::max(e.a.x, e.b.x) < std::min(a.x, b.x) || std::min(e.a.x, e.b.x) > std::max(a.x, b.x) ||
                        std::max(e.a.y, e.b.y) < std::min(a.y, b.y) || std::min(e.a.y, e.b.y) > std::max(a.y, b.y))
                        continue;
                    const double ex = e.b.x - e.a.x, ey = e.b.y - e.a.y;
                    const double d = dx * ey - dy * ex;
                    if (d == 0.0)
                        continue;
                    const double u = ((e.a.x - a.x) * ey - (e.a.y - a.y) * ex) / d; // along the segment
                    const double v = ((e.a.x - a.x) * dy - (e.a.y - a.y) * dx) / d; // along the edge
                    if (u > 1e-9 && u < 1.0 - 1e-9 && v >= 0.0 && v <= 1.0)
                        at.push_back(u);
                }
    }

private:
    struct Edge {
        Point2d a, b;
    };
    std::array<std::size_t, 4> cell_box(const Point2d& a, const Point2d& b) const {
        const auto cx = [&](double x) { return std::size_t(std::clamp((x - box_[0]) / cell_, 0.0, double(nx_ - 1))); };
        const auto cy = [&](double y) { return std::size_t(std::clamp((y - box_[1]) / cell_, 0.0, double(ny_ - 1))); };
        return {cx(std::min(a.x, b.x)), cy(std::min(a.y, b.y)), cx(std::max(a.x, b.x)), cy(std::max(a.y, b.y))};
    }
    const Polygons& region_;
    std::vector<std::array<double, 4>> boxes_;
    std::array<double, 4> box_{};
    std::vector<Edge> edges_;
    std::vector<std::uint32_t> prev_, next_; // each edge's neighbours on its polygon
    double cell_ = 1.0;
    std::size_t nx_ = 1, ny_ = 1;
    std::vector<std::vector<std::uint32_t>> cells_;
    std::vector<std::uint32_t> seen_;
    std::uint32_t stamp_ = 0;
};

// The wall with a point added wherever it crosses the outline of `areas`, so
// each piece between two points lies on one side of it. The added points take
// the width and heights in between.
Toolpath cut_at(const Toolpath& t, AreaIndex& areas) {
    const std::size_t n = t.points.size();
    const std::size_t segments = t.closed && n > 2 ? n : n - 1;
    Toolpath out = t;
    out.points.clear();
    std::vector<double> at;
    for (std::size_t i = 0; i < segments; ++i) {
        const ToolpathPoint& a = t.points[i];
        const ToolpathPoint& b = t.points[(i + 1) % n];
        out.points.push_back(a);
        at.clear();
        areas.crossings(a.p, b.p, at);
        std::sort(at.begin(), at.end());
        const double dx = b.p.x - a.p.x, dy = b.p.y - a.p.y;
        // Not within a micron of either end: no piece of nothing.
        const double near_end = 1e-3 / std::max(std::hypot(dx, dy), 1e-9);
        for (const double u : at) {
            if (u < near_end || u > 1.0 - near_end)
                continue;
            ToolpathPoint p = a;
            p.p = {a.p.x + u * dx, a.p.y + u * dy};
            p.width = a.width + u * (b.width - a.width);
            p.dz = a.dz + u * (b.dz - a.dz);
            p.below = a.below + u * (b.below - a.below);
            out.points.push_back(p);
        }
    }
    if (!(t.closed && n > 2))
        out.points.push_back(t.points.back());
    return out;
}

std::vector<WallPiece> wall_pieces(const Toolpath& wall, AreaIndex& hanging, const AreaIndex& on_support, double gap,
                                   bool detect, const RoundBead* round = nullptr) {
    if (wall.points.size() < 2)
        return {{wall, false}};
    // Most walls are nowhere near what hangs.
    bool near = false;
    for (std::size_t i = 0; i < wall.points.size() && !near; ++i)
        near = hanging.near(wall.points[i].p, wall.points[(i + 1) % wall.points.size()].p);
    if (!near)
        return {{wall, false}};
    const Toolpath t = cut_at(wall, hanging);
    const std::size_t n = t.points.size();
    const std::size_t segments = t.closed && n > 2 ? n : n - 1;
    // 0: held, 1: over the air, 2: over the support.
    std::vector<unsigned char> kind(segments, 0);
    for (std::size_t i = 0; i < segments; ++i) {
        const Point2d& a = t.points[i].p;
        const Point2d& b = t.points[(i + 1) % n].p;
        const double x = (a.x + b.x) / 2.0, y = (a.y + b.y) / 2.0;
        if (!hanging.contains(x, y))
            continue;
        if (on_support.contains(x, y))
            kind[i] = 2;
        else if (detect)
            kind[i] = 1;
    }
    if (std::all_of(kind.begin(), kind.end(), [](unsigned char k) { return k == 0; }))
        return {{wall, false}};
    const auto piece_of = [&](unsigned char k) {
        WallPiece piece;
        piece.path.index = t.index;
        piece.path.gap = k == 2 ? gap : k == 1 && round ? round->gap : 0.0;
        piece.overhang = detect && k != 0;
        piece.air = k == 1;
        return piece;
    };
    // Over the air, the round bead all along.
    const auto rounded = [&](std::vector<WallPiece>& pieces) {
        if (round)
            for (WallPiece& piece : pieces)
                if (piece.air)
                    for (ToolpathPoint& p : piece.path.points)
                        p.width = round->width;
        return std::move(pieces);
    };
    if (std::all_of(kind.begin(), kind.end(), [&](unsigned char k) { return k == kind.front(); })) {
        std::vector<WallPiece> whole(1, piece_of(kind.front()));
        whole.front().path.points = t.points;
        whole.front().path.closed = t.closed;
        whole.front().path.wavy = t.wavy;
        return rounded(whole);
    }
    std::vector<WallPiece> out;
    WallPiece piece = piece_of(kind.front());
    piece.path.points.push_back(t.points.front());
    for (std::size_t i = 0; i < segments; ++i) {
        if (kind[i] != (i == 0 ? kind.front() : kind[i - 1])) {
            out.push_back(std::move(piece));
            piece = piece_of(kind[i]);
            piece.path.points.push_back(t.points[i]);
        }
        piece.path.points.push_back(t.points[(i + 1) % n]);
    }
    out.push_back(std::move(piece));
    return rounded(out);
}

// A point between a and b, at u from a: width and heights in between.
ToolpathPoint between(const ToolpathPoint& a, const ToolpathPoint& b, double u) {
    ToolpathPoint p = a;
    p.p = {a.p.x + u * (b.p.x - a.p.x), a.p.y + u * (b.p.y - a.p.y)};
    p.width = a.width + u * (b.width - a.width);
    p.dz = a.dz + u * (b.dz - a.dz);
    p.below = a.below + u * (b.below - a.below);
    return p;
}

// A wall by how much of its bead rests on the layer below (`below`, its
// region; speeds_accels.md, rules 6 and 7): every stretch's share, from 0 (in
// the air) to 1, from how far its middle lies inside or out of that region
// against its width; the wall cut every 0.5 mm near the region's outline, and
// the stretches whose feeds (feed_of the share) are within 1 % of their run's
// joined, at the run's mean by length (an organic surface would otherwise
// change speed every half millimetre). Away from the outline a stretch rests
// all or nothing: `in_air` says which. A wall at one feed all along stays
// whole.
template <typename InAir, typename FeedOf>
std::vector<std::pair<Toolpath, double>> by_support(const Toolpath& wall, const AreaIndex& below, InAir in_air,
                                                    FeedOf feed_of) {
    const std::size_t n = wall.points.size();
    if (n < 2)
        return {{wall, feed_of(1.0)}};
    const std::size_t segments = wall.closed && n > 2 ? n : n - 1;
    constexpr double kPiece = 0.5; // mm
    constexpr double kChunk = 4.0; // mm looked at for the outline at once
    std::vector<std::pair<Toolpath, double>> out;
    Toolpath run; // the stretch being joined, at run_feed
    run.index = wall.index;
    run.gap = wall.gap;
    double run_feed = 0.0, run_length = 0.0;
    constexpr double kJoin = 0.01; // of the run's feed
    for (std::size_t i = 0; i < segments; ++i) {
        const ToolpathPoint& a = wall.points[i];
        const ToolpathPoint& b = wall.points[(i + 1) % n];
        const double length = std::hypot(b.p.x - a.p.x, b.p.y - a.p.y);
        const double w = std::max((a.width + b.width) / 2.0, 1e-3);
        const auto at = [&](double u) { return Point2d{a.p.x + u * (b.p.x - a.p.x), a.p.y + u * (b.p.y - a.p.y)}; };
        const std::size_t chunks = std::max<std::size_t>(1, std::size_t(std::ceil(length / kChunk)));
        for (std::size_t ch = 0; ch < chunks; ++ch) {
            const double ua = double(ch) / double(chunks), ub = double(ch + 1) / double(chunks);
            const double reach = length / double(chunks) / 2.0 + w;
            const bool near = below.distance(at((ua + ub) / 2.0), reach) < reach;
            const std::size_t parts =
                near ? std::clamp<std::size_t>(std::size_t(std::ceil(length / double(chunks) / kPiece)), 1, 400) : 1;
            for (std::size_t j = 0; j < parts; ++j) {
                const double u0 = ua + (ub - ua) * double(j) / double(parts);
                const double u1 = ua + (ub - ua) * double(j + 1) / double(parts);
                const Point2d c = at((u0 + u1) / 2.0);
                double share = 1.0;
                if (const auto hit = near ? below.nearest(c, w) : std::nullopt)
                    share = std::clamp((w / 2.0 + (hit->inside ? hit->distance : -hit->distance)) / w, 0.0, 1.0);
                else
                    share = in_air(c) ? 0.0 : 1.0;
                const double feed = feed_of(share);
                const double piece = (u1 - u0) * length;
                if (run.points.empty() || std::abs(feed - run_feed) > kJoin * run_feed) {
                    if (run.points.size() > 1)
                        out.push_back({run, run_feed});
                    run.points.assign(1, between(a, b, u0));
                    run_feed = feed, run_length = 0.0;
                }
                if (run_length + piece > 0.0)
                    run_feed = (run_feed * run_length + feed * piece) / (run_length + piece);
                run_length += piece;
                run.points.push_back(between(a, b, u1));
            }
        }
    }
    if (out.empty())
        return {{wall, run_feed}};
    if (run.points.size() > 1)
        out.push_back({std::move(run), run_feed});
    return out;
}

// The part of an open path from `from` to `to` mm along it.
Toolpath stretch(const Toolpath& t, double from, double to) {
    Toolpath out;
    out.index = t.index;
    out.gap = t.gap;
    out.wavy = t.wavy;
    double walked = 0.0;
    for (std::size_t i = 1; i < t.points.size(); ++i) {
        const ToolpathPoint& a = t.points[i - 1];
        const ToolpathPoint& b = t.points[i];
        const double length = std::hypot(b.p.x - a.p.x, b.p.y - a.p.y);
        const double end = walked + length;
        if (end > from && walked < to && length > 0.0) {
            if (out.points.empty())
                out.points.push_back(between(a, b, std::max(0.0, (from - walked) / length)));
            out.points.push_back(end <= to ? b : between(a, b, (to - walked) / length));
        }
        walked = end;
        if (walked >= to)
            break;
    }
    return out;
}

// Z anti-aliasing (docs/ZAA.md): the pieces of a path where its points go from
// below the layer's top to on it, or above it; each piece lies on one side,
// and the piece after a change starts at the last point before it.
std::vector<Toolpath> split_by_height(const Toolpath& t) {
    constexpr double kSide = 0.005; // mm
    const auto side = [](const ToolpathPoint& p) { return p.dz < -kSide ? -1 : p.dz > kSide ? 1 : 0; };
    const std::size_t n = t.points.size();
    std::size_t change = n;
    for (std::size_t i = 1; i < n && change == n; ++i)
        if (side(t.points[i]) != side(t.points[i - 1]))
            change = i;
    if (n < 2 || (change == n && (!t.closed || side(t.points.front()) == side(t.points.back()))))
        return {t};
    // A loop runs from a change around to it again, as an open path.
    std::vector<ToolpathPoint> run;
    if (t.closed) {
        const std::size_t from = change == n ? 0 : change - 1;
        for (std::size_t k = 0; k <= n; ++k)
            run.push_back(t.points[(from + k) % n]);
    } else {
        run = t.points;
    }
    std::vector<Toolpath> out;
    Toolpath piece;
    piece.index = t.index;
    piece.points.push_back(run.front());
    for (std::size_t i = 1; i < run.size(); ++i) {
        if (side(run[i]) != side(run[i - 1]) && piece.points.size() > 1) {
            out.push_back(piece);
            piece.points.assign(1, run[i - 1]);
        }
        piece.points.push_back(run[i]);
    }
    if (piece.points.size() > 1)
        out.push_back(std::move(piece));
    return out;
}

// The order to lay paths off the layer's top in (Z anti-aliasing), after the
// height constraint graph of Song et al. 2017, Section 4.1: where two paths
// run beside each other (within `beside`), the lower there goes first, by the
// majority of their nearest points; among those free to go, the one first in
// the given order. A cycle is broken by the given order.
std::vector<std::size_t> height_order(const std::vector<const Toolpath*>& paths, double beside) {
    const double reach = beside;
    constexpr double kStep = 0.2, kSlack = 0.002; // mm
    struct Sample {
        double x, y, dz;
        std::uint32_t path;
    };
    std::vector<Sample> samples;
    for (std::uint32_t k = 0; k < paths.size(); ++k) {
        const std::vector<ToolpathPoint>& p = paths[k]->points;
        const std::size_t segments = paths[k]->closed ? p.size() : p.size() - 1;
        for (std::size_t i = 0; i < segments && p.size() > 1; ++i) {
            const ToolpathPoint& a = p[i];
            const ToolpathPoint& b = p[(i + 1) % p.size()];
            const int n = std::max(1, int(std::ceil(std::hypot(b.p.x - a.p.x, b.p.y - a.p.y) / kStep)));
            for (int j = 0; j < n; ++j) {
                const double f = double(j) / n;
                samples.push_back({a.p.x + f * (b.p.x - a.p.x), a.p.y + f * (b.p.y - a.p.y), a.dz + f * (b.dz - a.dz), k});
            }
        }
        if (!paths[k]->closed && !p.empty())
            samples.push_back({p.back().p.x, p.back().p.y, p.back().dz, k});
    }
    std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> cells;
    const auto cell = [&](double x, double y) {
        return (std::uint64_t(std::uint32_t(std::int32_t(std::floor(x / reach)))) << 32) |
               std::uint32_t(std::int32_t(std::floor(y / reach)));
    };
    for (std::uint32_t s = 0; s < samples.size(); ++s)
        cells[cell(samples[s].x, samples[s].y)].push_back(s);
    // For each sample, the nearest sample of every other path within reach:
    // whether it is higher there. Each pair of paths goes by the majority.
    std::map<std::uint64_t, double> votes; // (a, b), a < b: positive when b is higher than a beside it
    std::map<std::uint32_t, std::pair<double, double>> nearest; // other path: (distance2, dz)
    for (const Sample& s : samples) {
        nearest.clear();
        for (int dx = -1; dx <= 1; ++dx)
            for (int dy = -1; dy <= 1; ++dy) {
                const auto it = cells.find(cell(s.x + dx * reach, s.y + dy * reach));
                if (it == cells.end())
                    continue;
                for (const std::uint32_t o : it->second) {
                    const Sample& q = samples[o];
                    const double d2 = (q.x - s.x) * (q.x - s.x) + (q.y - s.y) * (q.y - s.y);
                    if (q.path == s.path || d2 > reach * reach)
                        continue;
                    const auto [at, fresh] = nearest.try_emplace(q.path, d2, q.dz);
                    if (!fresh && d2 < at->second.first)
                        at->second = {d2, q.dz};
                }
            }
        for (const auto& [other, near] : nearest) {
            const double diff = near.second - s.dz; // the other path's height over this one's, beside it
            if (std::abs(diff) <= kSlack)
                continue;
            const bool ascending = s.path < other;
            const std::uint64_t key = ascending ? (std::uint64_t(s.path) << 32) | other : (std::uint64_t(other) << 32) | s.path;
            votes[key] += (diff > 0.0) == ascending ? 1.0 : -1.0;
        }
    }
    std::vector<std::vector<std::uint32_t>> after(paths.size()); // after[a]: paths that go after a
    for (const auto& [key, vote] : votes) {
        if (vote == 0.0)
            continue;
        const auto a = std::uint32_t(key >> 32), b = std::uint32_t(key & 0xFFFFFFFFu);
        if (vote > 0.0)
            after[a].push_back(b); // b higher: a first
        else
            after[b].push_back(a);
    }
    std::vector<std::uint32_t> waiting(paths.size(), 0);
    for (const auto& list : after)
        for (const std::uint32_t b : list)
            ++waiting[b];
    std::vector<std::size_t> order;
    std::vector<bool> done(paths.size(), false);
    std::set<std::size_t> ready;
    for (std::size_t k = 0; k < paths.size(); ++k)
        if (waiting[k] == 0)
            ready.insert(k);
    std::size_t next_unlaid = 0;
    while (order.size() < paths.size()) {
        std::size_t k;
        if (!ready.empty()) {
            k = *ready.begin();
            ready.erase(ready.begin());
        } else { // a cycle: the first not laid yet
            while (done[next_unlaid])
                ++next_unlaid;
            k = next_unlaid;
        }
        if (done[k])
            continue;
        done[k] = true;
        order.push_back(k);
        for (const std::uint32_t b : after[k])
            if (!done[b] && --waiting[b] == 0)
                ready.insert(b);
    }
    return order;
}

void append_integer(std::string& text, long v) {
    char buffer[24];
    const auto result = std::to_chars(buffer, buffer + sizeof buffer, v);
    text.append(buffer, result.ptr);
}

// Keeps the machine's position and writes only the axes that change. The
// text goes to a buffer that flush() hands to the stream.
// What the lines of each kind do, for gcode.verbose_comments (gcode_extras.md,
// rule 12): its extrusions, and the travel to its first point.
struct KindNotes {
    const char* type;
    const char* extrude;
    const char* travel;
};
constexpr KindNotes kKindNotes[] = {
    {"External perimeter", "perimeter", "move to first perimeter point"},
    {"Perimeter", "perimeter", "move to first perimeter point"},
    {"Overhang perimeter", "perimeter", "move to first perimeter point"},
    {"Internal infill", "infill", "move to first infill point"},
    {"Solid infill", "infill", "move to first infill point"},
    {"Top solid infill", "infill", "move to first infill point"},
    {"Bridge infill", "infill (bridge)", "move to first infill point"},
    {"Gap fill", "gap fill", "move to first gap fill point"},
    {"Ironing", "ironing", "move to first ironing point"},
    {"Skirt/Brim", "skirt", "move to first skirt point"},
    {"Support material", "support material", "move to first support material point"},
    {"Support material interface", "support material interface", "move to first support material point"},
};
constexpr KindNotes kOtherNotes{"", "extrude", "travel"};

const KindNotes& notes_of(const char* type) {
    if (type)
        for (const KindNotes& k : kKindNotes)
            if (std::strcmp(k.type, type) == 0)
                return k;
    return kOtherNotes;
}

// The machine's limits for the time estimate (print_time.md, rule 2): with
// machine.limits_usage ignore, only the feeds, accelerations and corner speed.
Kinematics kinematics_of(const Settings& s) {
    Kinematics k{{s.max_feed_x, s.max_feed_y, s.max_feed_z, s.max_feed_e},
                 s.acceleration,
                 s.travel_acceleration,
                 s.corner_speed};
    if (s.limits_usage == LimitsUsage::Ignore)
        return k;
    k.retract_acceleration = s.retract_acceleration;
    const double accel[4] = {s.max_accel_x, s.max_accel_y, s.max_accel_z, s.max_accel_e};
    const double jerk[4] = {s.jerk_x, s.jerk_y, s.jerk_z, s.jerk_e};
    std::copy(accel, accel + 4, k.max_accel);
    std::copy(jerk, jerk + 4, k.jerk);
    k.junction_deviation = s.junction_deviation;
    k.min_extrude_feed = s.min_extrude_feed;
    k.min_travel_feed = s.min_travel_feed;
    return k;
}

class Emitter {
public:
    explicit Emitter(const Settings& s)
        : relative_(relative_e(s)), dialect_(dialect_of(s.flavor)), time_(kinematics_of(s)),
          retract_(s.retract_length), retract_feed_(s.retract_speed), lift_(s.retract_lift),
          min_travel_(s.retract_min_travel), z_feed_(s.travel_z_speed), marlin_(dialect_of(s.flavor).m204_print_travel),
          accel_print_(s.acceleration), accel_travel_(s.travel_acceleration), travel_accel_(s.accel_travel),
          short_travel_accel_(s.accel_short_travel > 0.0 ? s.accel_short_travel : s.accel_travel) {
        const double r = s.filament_diameter / 2.0;
        filament_area_ = std::numbers::pi * r * r;
        verbose_ = s.verbose_comments;
        // A silent mode: a second estimate with its limits (gcode_extras.md, rule 7).
        if (s.silent_mode)
            silent_.emplace(kinematics_of(silent_settings(s)));
        // With the limits written, no acceleration passes them (speeds_accels.md, V9).
        if (s.limits_usage == LimitsUsage::Emit && dialect_.limits != Dialect::Limits::None) {
            cap_print_ = s.acceleration;
            cap_travel_ = s.travel_acceleration;
        }
        // Volumetric E (gcode_extras.md, rule 11): every E in mm3 of
        // plastic, retractions too; the engine counts filament as ever.
        if (s.volumetric_e)
            e_scale_ = filament_area_;
        // The melt limit: the lower of the hot end's and the filament's (0: none).
        for (const double limit : {s.machine_max_flow, s.filament_max_flow})
            if (limit > 0.0)
                max_flow_ = max_flow_ > 0.0 ? std::min(max_flow_, limit) : limit;
        feed_limit_ = std::max(s.max_feed_x, s.max_feed_y);
        if (!(feed_limit_ > 0.0)) // not known: no cap
            feed_limit_ = std::numeric_limits<double>::infinity();
    }

    // Plans the time of the moves since the last call (they end at rest), and
    // returns it; the silent mode's too.
    double end_layer() {
        const double seconds = time_.flush();
        last_silent_ = silent_ ? silent_->flush() : seconds;
        return seconds;
    }
    double last_silent_layer() const { return last_silent_; }
    double print_time() const { return time_.total(); }
    double silent_time() const { return silent_ ? silent_->total() : time_.total(); }

    // A mark for M73 (gcode_firmware.md, rule 5): a line that holds its
    // place in the G-code until the times are known (resolve_progress).
    void progress_mark() {
        const std::size_t id = time_.mark();
        if (silent_)
            silent_->mark();
        text_ += kProgressMark;
        append_integer(text_, long(id));
        text_ += '\n';
    }
    double time_at(std::size_t mark) const { return time_.time_at(mark); }
    double silent_time_at(std::size_t mark) const { return silent_ ? silent_->time_at(mark) : time_.time_at(mark); }

    void flush(std::ostream& out) {
        out.write(text_.data(), std::streamsize(text_.size()));
        text_.clear();
        fan_marks_.clear(); // past the last layer: its fan stays
    }

    // Extrusion speeds of the layer being written go through this.
    void pace(const Slowdown& slowdown) { pace_ = slowdown; }

    // What the layer's moves take: extruded length per feed, and the time of
    // everything else (travels, retractions, lifts).
    void reset_times() {
        extruded_.clear();
        outer_extruded_.clear();
        other_time_ = 0.0;
    }
    const std::map<double, double>& extruded() const { return extruded_; }
    const std::map<double, double>& outer_extruded() const { return outer_extruded_; } // the outer walls' part
    double other_time() const { return other_time_; }

    // The part-cooling fan (docs/spec/features/cooling.md, rules 3 and 4). A
    // layer's fan hangs on its time, known only once the layer is built: the
    // layer opens with a mark where its fan goes, before its ;LAYER_CHANGE,
    // each bridge with a mark for the bridge fan and its end with a mark for
    // the layer's again. settle_fan() writes the commands at the marks.
    void open_layer_fan() {
        fan_marks_.clear();
        fan_marks_.push_back({text_.size(), false});
        marked_bridge_ = false;
    }
    void settle_fan(double layer, double bridge) {
        if (fan_marks_.empty())
            return;
        const double percents[2] = {layer, bridge};
        const long values[2] = {fan_key(layer), fan_key(bridge)};
        std::string out;
        out.reserve(text_.size() + 12 * fan_marks_.size());
        std::size_t from = 0;
        for (const auto& [at, on_bridge] : fan_marks_) {
            out.append(text_, from, at - from);
            from = at;
            const long value = values[on_bridge];
            if (value == fan_)
                continue;
            append_fan(out, percents[on_bridge], value);
            fan_ = value;
        }
        out.append(text_, from);
        text_ = std::move(out);
        fan_marks_.clear();
    }

    // Values a height change sets (docs/CONFIG.md, "changes").
    void set_retraction(double length, double speed) {
        retract_ = length;
        retract_feed_ = speed;
    }
    void set_flow(double factor) { flow_ = factor; }
    void set_forced_flow(double flow) { forced_flow_ = flow; }

    // Lines written as they are (a temperature, the user's G-code of a change),
    // as user G-code; then the type before comes back, so the engine's own
    // moves that follow (a travel that retracts) are not taken for the user's.
    void custom(const std::string& lines, bool user = false) {
        if (lines.empty())
            return;
        const char* before = type_;
        type("Custom");
        text_ += lines;
        if (lines.back() != '\n')
            text_ += '\n';
        if (user)
            resync();
        if (before && std::string_view(before) != "Custom")
            type(before);
    }

    // After a change's own G-code (a pause that parks the head, a relative
    // move): the engine no longer knows where the head is, nor the modes or
    // the extruder's position. The modes and E are set again, and the next
    // travel retracts and writes X, Y, Z and F in full.
    void resync() {
        text_ += "G90";
        line_end("use absolute coordinates");
        if (dialect_.e_mode) {
            text_ += relative_ ? "M83" : "M82";
            line_end(relative_ ? "use relative distances for extrusion" : "use absolute distances for extrusion");
        }
        if (!relative_ && dialect_.resets_e) {
            char buffer[kNumberCapacity];
            text_ += "G92 ";
            text_ += dialect_.e_axis;
            text_.append(buffer, format_fixed(buffer, e_ * e_scale_, 5));
            line_end("reset extrusion distance");
        }
        x_.clear(), y_.clear(), z_.clear(), e_text_.clear();
        f_ = std::numeric_limits<long>::min();
        resync_ = true;
    }

    // Another bead height within the layer (a thick zone, docs/LOCAL_LAYERS.md).
    void set_height(double height) {
        if (height == height_)
            return;
        text_ += ";HEIGHT:" + fixed(height, 3) + "\n";
        height_ = height;
    }

    // Above the layer's top the nozzle travels this high (Z anti-aliasing that
    // raises paths, docs/ZAA.md), so it clears them.
    void set_clearance(double clearance) { clearance_ = clearance; }
    double clearance() const { return clearance_; }

    // A wavy path (docs/NONPLANAR_INFILL.md) between nonplanar(true) and
    // nonplanar(false): each move's ;HEIGHT is the bead's own thickness, the
    // gap from the bead below up to the nozzle, and the layer's comes back after.
    void nonplanar(bool on) {
        if (on == nonplanar_)
            return;
        nonplanar_ = on;
        text_ += on ? ";NONPLANAR:1\n" : ";NONPLANAR:0\n";
        if (on) {
            shown_height_ = fixed(height_, 3);
        } else if (shown_height_ != fixed(height_, 3)) {
            text_ += ";HEIGHT:" + fixed(height_, 3) + "\n";
        }
    }

    // keep_z: a vase's turn goes on from where the turn below ended, at its
    // height (vase_mode.md, rule 4): no move up to the layer's Z.
    // before and after: the user's G-code around the move in Z
    // (gcode_templates.md, rule 11); one that moves anything makes the next
    // travel write every axis again.
    void new_layer(double z, double height, double travel_feed, bool keep_z = false, const std::string& before = {},
                   const std::string& after = {}) {
        text_ += ";LAYER_CHANGE\n;Z:" + fixed(z, 3) + "\n;HEIGHT:" + fixed(height, 3) + "\n";
        if (!relative_ && dialect_.resets_e) {
            // A retraction carries over the layer change: E restarts at minus its length.
            e_ = retracted_ ? -retracted_amount_ : 0.0;
            text_ += "G92 ";
            text_ += dialect_.e_axis;
            text_ += fixed(e_ * e_scale_, 5);
            line_end("reset extrusion distance");
            position_[3] = e_;
            e_text_.clear();
        }
        // The user's text as their own G-code (;TYPE:Custom), the type before it back after.
        const auto user = [&](const std::string& text) {
            if (text.empty())
                return;
            const char* was = type_;
            type("Custom");
            text_ += text;
            if (text.back() != '\n')
                text_ += '\n';
            if (was && std::string_view(was) != "Custom")
                type(was);
        };
        user(before);
        if (!keep_z)
            move("G1", std::nullopt, std::nullopt, z, std::nullopt, z_feed(travel_feed), {}, {}, "simple layer change");
        first_travel_ = true;
        height_ = height;
        z_value_ = z;
        if (!keep_z)
            z_now_ = written(z);
        clearance_ = 0.0;
        // Marlin with relative E: the extruder's position restarts at each
        // layer, so it keeps its precision (gcode_firmware.md, rule 3).
        if (relative_ && dialect_.g92_relative) {
            text_ += "G92 E0";
            line_end("reset extrusion distance");
            position_[3] = 0.0;
        }
        user(after);
        if (commands(before) || commands(after))
            resync();
    }

    // Whether the user's G-code left the head where the engine does not know.
    bool lost() const { return resync_; }

    // Whether a user's text has anything but comments and blank lines.
    static bool commands(const std::string& text) {
        std::size_t at = 0;
        while (at < text.size()) {
            const std::size_t end = std::min(text.find('\n', at), text.size());
            const std::size_t first = text.find_first_not_of(" \t\r", at);
            if (first < end && text[first] != ';')
                return true;
            at = end + 1;
        }
        return false;
    }

    // After a kind's extrusions: back to `a` (0: none). With M204 S and an
    // acceleration of the travels, the next travel sets its own anyway.
    void reset_acceleration(double a) {
        if (marlin_ || !(travel_accel_ > 0.0))
            print_acceleration(a);
    }

    // Before an extrusion: its kind's acceleration; 0 leaves it as it is.
    void print_acceleration(double a) {
        if (cap_print_ > 0.0)
            a = std::min(a, cap_print_);
        if (!dialect_.accelerations || !(a > 0.0) || (a == accel_print_ && (marlin_ || a == accel_travel_)))
            return;
        text_ += marlin_ ? "M204 P" : "M204 S";
        append_integer(text_, std::lround(a));
        line_end("set acceleration");
        accel_print_ = a;
        if (!marlin_)
            accel_travel_ = a;
        time_.set_accelerations(accel_print_, accel_travel_);
        if (silent_)
            silent_->set_accelerations(accel_print_, accel_travel_);
    }

    void type(const char* t) {
        if (type_ != t) {
            text_ += ";TYPE:";
            text_ += t;
            text_ += '\n';
            type_ = t;
            note_ = notes_of(t).extrude;
            outer_ = std::strcmp(t, "External perimeter") == 0;
            // Overhang perimeters get the bridges' fan (walls_extra.md, rule 8).
            bridge_ = std::strcmp(t, "Bridge infill") == 0 || std::strcmp(t, "Overhang perimeter") == 0;
        }
    }

    // A travel along the planner's route. It retracts first (and lifts Z by
    // lift) when it leaves the part or is longer than min_travel. A route
    // planned beforehand from exactly where the head is stands for the
    // planner's.
    // next_type: the kind printed after it, for the verbose comments.
    void travel(const Point2d& p, double feed, const TravelPlanner& planner, const TravelPlan* planned = nullptr,
                const Point2d* planned_from = nullptr, bool force_retract = false, const char* next_type = nullptr) {
        const char* note = first_travel_ ? "travel to first layer point" : notes_of(next_type).travel;
        first_travel_ = false;
        extruder_off();
        if (resync_ && known_) {
            // From wherever the change's G-code left the head: retracted, up at
            // the cruise height, then straight over, every axis written.
            travel_acceleration(std::numeric_limits<double>::infinity());
            if (retract_ > 0.0 && !retracted_) {
                e_ = relative_ ? -retract_ : e_ - retract_;
                move("G1", std::nullopt, std::nullopt, std::nullopt, e_, retract_feed_, {}, {}, "retract");
                retracted_ = true;
                retracted_amount_ = retract_;
                other_time_ += retract_ / retract_feed_;
            }
            const double cruise = written(z_value_ + clearance_);
            move("G1", std::nullopt, std::nullopt, cruise, std::nullopt, z_feed(feed), {}, {}, "move to layer height");
            z_now_ = cruise;
            move("G1", p.x, p.y, std::nullopt, std::nullopt, feed, {}, {}, note);
            other_time_ += std::hypot(p.x - x_value_, p.y - y_value_) / feed;
            x_value_ = p.x, y_value_ = p.y;
            resync_ = false;
            return;
        }
        resync_ = false;
        if (!known_) {
            travel_acceleration(std::numeric_limits<double>::infinity());
            move("G1", p.x, p.y, std::nullopt, std::nullopt, feed, {}, {}, note);
            x_value_ = p.x, y_value_ = p.y, known_ = true;
            return;
        }
        const bool ready = planned && planned_from && planned_from->x == x_value_ && planned_from->y == y_value_;
        const TravelPlan own = ready ? TravelPlan{} : planner.plan({x_value_, y_value_}, p);
        const TravelPlan& plan = ready ? *planned : own;
        const bool retract = force_retract || plan.leaves_part || plan.length > min_travel_;
        if (plan.length > 0.0)
            travel_acceleration(plan.length);
        if (retract && retract_ > 0.0 && !retracted_) {
            e_ = relative_ ? -retract_ : e_ - retract_;
            move("G1", std::nullopt, std::nullopt, std::nullopt, e_, retract_feed_, {}, {}, "retract");
            retracted_ = true;
            retracted_amount_ = retract_;
            other_time_ += retract_ / retract_feed_;
        }
        // Off the layer's top (Z anti-aliasing): back up, and over what was raised.
        const double cruise = written(z_value_ + clearance_);
        if (z_now_ != cruise) {
            move("G1", std::nullopt, std::nullopt, cruise, std::nullopt, z_feed(feed), {}, {},
                 cruise > z_now_ ? "lift" : "lower");
            z_now_ = cruise;
        }
        const bool lift = retract && lift_ > 0.0;
        const double level = clearance_ > 0.0 ? cruise : z_value_;
        other_time_ += plan.length / feed + (lift ? 2.0 * lift_ / z_feed(feed) : 0.0);
        if (lift)
            move("G1", std::nullopt, std::nullopt, level + lift_, std::nullopt, z_feed(feed), {}, {}, "lift");
        for (const Point2d& q : plan.points)
            move("G1", q.x, q.y, std::nullopt, std::nullopt, feed, {}, {}, note);
        if (lift)
            move("G1", std::nullopt, std::nullopt, level, std::nullopt, z_feed(feed), {}, {}, "lower");
        x_value_ = p.x, y_value_ = p.y;
    }

    void extrude(const ToolpathPoint& from, const ToolpathPoint& to, double feed) {
        start_extrusion();
        const double width = (from.width + to.width) / 2.0;
        const double length = std::hypot(to.p.x - from.p.x, to.p.y - from.p.y);
        // Z anti-aliasing (docs/ZAA.md): the nozzle at the surface, and the bead
        // as thick as what is left down to the layer's bottom, by the Z written.
        const double z_layer = written(z_value_), z_from = written(z_value_ + from.dz), z_to = written(z_value_ + to.dz);
        if (z_now_ != z_from) {
            move("G1", std::nullopt, std::nullopt, z_from, std::nullopt, feed, {}, {}, z_from > z_now_ ? "lift" : "lower");
            z_now_ = z_from;
        }
        double height = height_ + ((z_from - z_layer) + (z_to - z_layer)) / 2.0;
        if (nonplanar_) {
            // On the wavy bead below, not on the layer below's top; a scarf
            // ramp's foot, as the Z written rounds it, may come to nothing.
            height = std::max(0.0, height - (from.below + to.below) / 2.0);
            const std::string shown = fixed(height, 3);
            if (shown != shown_height_) {
                text_ += ";HEIGHT:" + shown + "\n";
                shown_height_ = shown;
            }
        }
        const double e = width * height * length / filament_area_ * flow_;
        feed = melt_feed(feed, width, height);
        extruded_[feed] += length;
        if (outer_)
            outer_extruded_[feed] += length;
        total_e_ += e;
        pending_ += e;
        // A piece that rounds to no move at 3 decimals is not written; its E
        // (next to nothing) goes with the next piece.
        char tx[kNumberCapacity], ty[kNumberCapacity];
        const std::string_view x_text(tx, format_fixed(tx, to.p.x, 3)), y_text(ty, format_fixed(ty, to.p.y, 3));
        if (x_text == x_ && y_text == y_)
            return;
        // The width of the moves that follow, written only before one (the
        // tip of a bead that tapers to nothing has no move of its own).
        show_width(width);
        e_ = relative_ ? pending_ : e_ + pending_;
        pending_ = 0.0;
        move("G1", to.p.x, to.p.y, z_to != z_now_ ? std::optional<double>(z_to) : std::nullopt, e_, paced(feed), x_text,
             y_text, note_);
        x_value_ = to.p.x, y_value_ = to.p.y, known_ = true;
        z_now_ = z_to;
    }

    // An arc of a flat path of one width (gcode_extras.md, rule 10): G2
    // clockwise or G3 counter-clockwise to points[a.last], its centre (I, J)
    // from where it starts as written, and the E of its length. The time
    // estimate follows the path's own moves, which the arc stays within
    // gcode.resolution of.
    void arc(std::span<const ToolpathPoint> points, const ArcRun& a, double feed) {
        start_extrusion();
        const double z_layer = written(z_value_);
        if (z_now_ != z_layer) {
            move("G1", std::nullopt, std::nullopt, z_layer, std::nullopt, feed, {}, {}, z_layer > z_now_ ? "lift" : "lower");
            z_now_ = z_layer;
        }
        const double width = points[a.first].width, length = a.length();
        const double e = width * height_ * length / filament_area_ * flow_;
        feed = melt_feed(feed, width, height_);
        extruded_[feed] += length;
        if (outer_)
            outer_extruded_[feed] += length;
        total_e_ += e;
        pending_ += e;
        show_width(width);
        e_ = relative_ ? pending_ : e_ + pending_;
        pending_ = 0.0;
        const Point2d& end = points[a.last].p;
        char tx[kNumberCapacity], ty[kNumberCapacity], buffer[kNumberCapacity];
        const std::string_view x_text(tx, format_fixed(tx, end.x, 3)), y_text(ty, format_fixed(ty, end.y, 3));
        text_ += a.ccw ? "G3 X" : "G2 X";
        text_ += x_text;
        text_ += " Y";
        text_ += y_text;
        text_ += " I";
        text_.append(buffer, format_fixed(buffer, a.center.x - written(x_value_), 3));
        text_ += " J";
        text_.append(buffer, format_fixed(buffer, a.center.y - written(y_value_), 3));
        if (dialect_.e_axis != 0) {
            const std::string_view e_text(buffer, format_fixed(buffer, e_ * e_scale_, 5));
            text_ += ' ';
            text_ += dialect_.e_axis;
            text_ += e_text;
            e_text_.assign(e_text);
        }
        const double f = paced(feed);
        if (std::lround(f * 60.0) != f_) {
            f_ = std::lround(f * 60.0);
            text_ += " F";
            append_integer(text_, f_);
        }
        line_end(note_);
        x_.assign(x_text);
        y_.assign(y_text);
        // The time, along the path's moves, each with its share of E.
        const double e_to = relative_ ? position_[3] + e_ : e_, de = e_to - position_[3];
        double path = 0.0;
        for (std::size_t k = a.first; k < a.last; ++k)
            path += std::hypot(points[k + 1].p.x - points[k].p.x, points[k + 1].p.y - points[k].p.y);
        for (std::size_t k = a.first; k < a.last; ++k) {
            const double dx = points[k + 1].p.x - points[k].p.x, dy = points[k + 1].p.y - points[k].p.y;
            clock(dx, dy, 0.0, path > 0.0 ? de * std::hypot(dx, dy) / path : 0.0, double(f_) / 60.0, true);
        }
        position_[0] = end.x, position_[1] = end.y, position_[3] = e_to;
        x_value_ = end.x, y_value_ = end.y, known_ = true;
    }

    double total_e() const { return total_e_; }

    // MakerWare's M103 after the extrusions (before a travel, and at the end).
    void extruder_off() {
        if (extruder_on_) {
            text_ += "M103 ; extruder off\n";
            extruder_on_ = false;
        }
    }

    // The print's progress, at each layer on the dialects that show it (M73 P).
    void progress(long percent) {
        if (!dialect_.progress)
            return;
        text_ += "M73 P";
        append_integer(text_, percent);
        line_end("update progress");
    }

private:
    // A Z as the G-code writes it (three decimals), to compare and to measure with.
    static double written(double z) { return std::round(z * 1000.0) / 1000.0; }

    // Before an extrusion: a bridge's fan mark, MakerWare's M101, and the
    // retraction undone (back to where E was; a length changed since stays the old one).
    void start_extrusion() {
        if (bridge_ != marked_bridge_ && !fan_marks_.empty()) { // a bridge starts or ends: its fan
            fan_marks_.push_back({text_.size(), bridge_});
            marked_bridge_ = bridge_;
        }
        if (dialect_.extruder_on_off && !extruder_on_) {
            text_ += "M101 ; extruder on\n";
            extruder_on_ = true;
        }
        if (retracted_) {
            e_ = relative_ ? retracted_amount_ : e_ + retracted_amount_;
            move("G1", std::nullopt, std::nullopt, std::nullopt, e_, retract_feed_, {}, {}, "unretract");
            retracted_ = false;
            other_time_ += retracted_amount_ / retract_feed_;
        }
    }

    // The feed of a bead of this section: a flow tower's exact flow, past every
    // cap (it looks for the limit) as far as the machine moves; else no faster
    // than the hot end and the filament melt, rounded down to whole mm/min.
    double melt_feed(double feed, double width, double height) const {
        if (forced_flow_ > 0.0 && width * height * flow_ > 0.0)
            return std::min(feed_limit_, std::max(1.0, std::floor(forced_flow_ / (width * height * flow_) * 60.0)) / 60.0);
        if (max_flow_ > 0.0 && width * height * flow_ > 0.0) {
            const double cap = max_flow_ / (width * height * flow_);
            if (feed > cap)
                return std::max(1.0, std::floor(cap * 60.0)) / 60.0;
        }
        return feed;
    }

    // ;WIDTH: before the moves of another width (the same width writes the same text).
    void show_width(double width) {
        if (width == width_value_)
            return;
        width_value_ = width;
        char w[kNumberCapacity];
        const std::string_view width_text(w, format_fixed(w, width, 3));
        if (width_text != width_) {
            text_ += ";WIDTH:";
            text_ += width_text;
            text_ += '\n';
            width_.assign(width_text);
        }
    }

    // A slowed-down feed is rounded down to whole mm/min (as F is written), so
    // the layer lasts at least the minimum time.
    double paced(double feed) const {
        return pace_.stage == 0 ? feed : std::max(1.0, std::floor(pace_.feed(feed, outer_) * 60.0)) / 60.0;
    }

    // Before a travel of `length` mm: the travels' acceleration, or the short
    // ones' (speeds_accels.md, rule 11).
    void travel_acceleration(double length) {
        double a = length < min_travel_ ? short_travel_accel_ : travel_accel_;
        if (cap_travel_ > 0.0)
            a = std::min(a, cap_travel_);
        if (!dialect_.accelerations || !(a > 0.0) || (a == accel_travel_ && (marlin_ || a == accel_print_)))
            return;
        text_ += marlin_ ? "M204 T" : "M204 S";
        append_integer(text_, std::lround(a));
        line_end("set acceleration");
        accel_travel_ = a;
        if (!marlin_)
            accel_print_ = a;
        time_.set_accelerations(accel_print_, accel_travel_);
        if (silent_)
            silent_->set_accelerations(accel_print_, accel_travel_);
    }

    // The feed of a move in Z, beside travels at `travel`.
    double z_feed(double travel) const { return z_feed_ > 0.0 ? z_feed_ : travel; }

    // A fan in percent as the dialect tells one setting from another: the
    // S0-255 of M106, on or off (M126, M127), or hundredths of P's 0-255.
    long fan_key(double percent) const {
        const long value = fan_value(percent);
        switch (dialect_.fan) {
        case Dialect::Fan::OnOff:
            return value > 0 ? 1 : 0;
        case Dialect::Fan::M106P:
            return value == 0 ? 0 : std::max(1L, std::lround(std::clamp(percent, 0.0, 100.0) * 255.0));
        case Dialect::Fan::M106:
        case Dialect::Fan::M106OffS0:
            break;
        }
        return value;
    }

    // The command that sets the fan to `percent`, whose fan_key is `key`.
    void append_fan(std::string& out, double percent, long key) const {
        if (key == 0) {
            out += dialect_.fan == Dialect::Fan::OnOff       ? "M127"
                   : dialect_.fan == Dialect::Fan::M106OffS0 ? "M106 S0"
                                                             : "M107";
            out += verbose_ ? " ; disable fan\n" : "\n";
            return;
        }
        if (dialect_.fan == Dialect::Fan::OnOff) {
            out += "M126";
        } else if (dialect_.fan == Dialect::Fan::M106P) {
            out += "M106 P";
            out += trimmed(std::clamp(percent, 0.0, 100.0) * 2.55, 2);
        } else {
            out += "M106 S";
            append_integer(out, key);
        }
        out += verbose_ ? " ; enable fan\n" : "\n";
    }

    // A fan in percent as M106 writes it, S0-255.
    static long fan_value(double percent) {
        const long value = std::lround(std::clamp(percent, 0.0, 100.0) * 255.0 / 100.0); // 50 % is S128
        // RepRapFirmware reads S up to 1 as a fraction: S1 is a full fan
        // there. S2 is the same breeze (0.8 %) on every firmware.
        return value == 1 ? 2 : value;
    }

    // A move for the time estimates.
    void clock(double dx, double dy, double dz, double de, double feed, bool extruding) {
        time_.add(dx, dy, dz, de, feed, extruding);
        if (silent_)
            silent_->add(dx, dy, dz, de, feed, extruding);
    }

    // The end of a line, with what it does when the comments are verbose.
    void line_end(const char* note) {
        if (verbose_) {
            text_ += " ; ";
            text_ += note;
        }
        text_ += '\n';
    }

    // x_text and y_text, when given, are x and y as format_fixed writes them;
    // note: what the move does, for the verbose comments.
    void move(const char* g, std::optional<double> x, std::optional<double> y, std::optional<double> z,
              std::optional<double> e, double feed, std::string_view x_text = {}, std::string_view y_text = {},
              const char* note = nullptr) {
        const std::size_t start = text_.size();
        text_ += g;
        const std::size_t bare = text_.size();
        auto axis = [&](char name, std::optional<double> v, int decimals, std::string& last, std::string_view given) {
            if (!v)
                return;
            char buffer[kNumberCapacity];
            const std::string_view t = given.empty() ? std::string_view(buffer, format_fixed(buffer, *v, decimals)) : given;
            if (t != last || name == dialect_.e_axis) {
                text_ += ' ';
                text_ += name;
                text_ += t;
                last.assign(t);
            }
        };
        axis('X', x, 3, x_, x_text);
        axis('Y', y, 3, y_, y_text);
        axis('Z', z, 3, z_, {});
        if (dialect_.e_axis != 0)
            axis(dialect_.e_axis, e ? std::optional<double>(*e * e_scale_) : std::nullopt, 5, e_text_, {});
        else if (!x && !y && !z) {
            text_.resize(start); // no E to write: a retraction is no move at all
            return;
        }
        const long f = std::lround(feed * 60.0);
        if (f != f_) {
            text_ += " F";
            append_integer(text_, f);
            f_ = f;
        }
        if (text_.size() == bare) {
            text_.resize(start); // nothing changed
            return;
        }
        if (note)
            line_end(note);
        else
            text_ += '\n';
        // The move for the time estimate, at the feed as written.
        const double to[4] = {x.value_or(position_[0]), y.value_or(position_[1]), z.value_or(position_[2]),
                              e ? (relative_ ? position_[3] + *e : *e) : position_[3]};
        const double de = to[3] - position_[3];
        clock(to[0] - position_[0], to[1] - position_[1], to[2] - position_[2], de, double(f_) / 60.0,
                  de > 0.0 && (x || y));
        std::copy(to, to + 4, position_);
    }

    std::string text_;
    bool relative_;
    Dialect dialect_;
    std::optional<PrintTime> silent_; // the silent mode's estimate
    double last_silent_ = 0.0;        // its time of the last layer
    double cap_print_ = 0.0, cap_travel_ = 0.0; // the written limits no acceleration passes; 0: none
    bool extruder_on_ = false;        // after MakerWare's M101
    bool verbose_ = false;            // gcode.verbose_comments
    const char* note_ = "extrude";    // what the current kind's extrusions are, for them
    bool first_travel_ = false;       // the next travel is the layer's first
    PrintTime time_;
    double position_[4] = {0.0, 0.0, 0.0, 0.0}; // X, Y, Z, E as written
    Slowdown pace_;
    std::map<double, double> extruded_, outer_extruded_;
    double max_flow_ = 0.0; // mm3/s; 0: no limit
    double forced_flow_ = 0.0; // mm3/s every extrusion runs at; 0: the feeds asked
    double feed_limit_ = 0.0;  // mm/s the machine moves: a forced flow never asks more
    double other_time_ = 0.0;
    long fan_ = -1; // not set yet
    std::vector<std::pair<std::size_t, bool>> fan_marks_; // where the layer's fan commands go: (offset, a bridge's)
    bool marked_bridge_ = false; // the last mark is a bridge's
    bool bridge_ = false;        // the current type is a bridge
    double filament_area_ = 1.0;
    double e_scale_ = 1.0; // the E written per mm of filament
    double retract_, retract_feed_, lift_, min_travel_;
    double z_feed_; // mm/s of the moves in Z (speeds_accels.md, rule 4); 0: the travel's
    // Accelerations (speeds_accels.md, rules 10 to 13): Marlin's M204 P for
    // printing and T for travels, or M204 S for both; as last set.
    bool marlin_;
    double accel_print_, accel_travel_;
    double travel_accel_, short_travel_accel_; // asked for travels; 0: none
    double retracted_amount_ = 0.0;
    double flow_ = 1.0;
    double height_ = 0.2;
    double e_ = 0.0, total_e_ = 0.0, pending_ = 0.0;
    double x_value_ = 0.0, y_value_ = 0.0, z_value_ = 0.0;
    double z_now_ = 0.0;     // the nozzle's Z as written: off the layer's top with Z anti-aliasing
    double clearance_ = 0.0; // mm above the layer's top that travels keep
    bool nonplanar_ = false; // in a wavy path: ;HEIGHT is each move's own
    std::string shown_height_;
    bool known_ = false;     // the position is known (after the first move in XY)
    bool resync_ = false;    // after a change's own G-code: the next travel writes everything
    bool retracted_ = false;
    std::string x_, y_, z_, e_text_, width_;
    double width_value_ = std::numeric_limits<double>::quiet_NaN(); // the width width_ was last checked for
    long f_ = std::numeric_limits<long>::min(); // none written yet
    const char* type_ = nullptr;
    bool outer_ = false; // the current type is the outer wall
};

} // namespace

std::string substitute_variables(std::string_view text, const std::map<std::string, std::vector<std::string>>& vars) {
    std::string out;
    std::size_t i = 0;
    while (i < text.size()) {
        const char open = text[i];
        if (open == '[' || open == '{') {
            const char close = open == '[' ? ']' : '}';
            const std::size_t end = text.find(close, i + 1);
            if (end != std::string_view::npos) {
                std::string name(text.substr(i + 1, end - i - 1));
                std::size_t index = 0;
                bool ok = true;
                if (open == '{') {
                    const std::size_t bracket = name.find('[');
                    if (bracket != std::string::npos && name.back() == ']') {
                        const std::string digits = name.substr(bracket + 1, name.size() - bracket - 2);
                        // from_chars: an index too big for a size_t is no index
                        // (std::stoul threw out of the writer).
                        const auto [last, error] = std::from_chars(digits.data(), digits.data() + digits.size(), index);
                        ok = !digits.empty() && error == std::errc() && last == digits.data() + digits.size();
                        name = name.substr(0, bracket);
                    }
                }
                const auto it = vars.find(name);
                if (ok && it != vars.end() && index < it->second.size()) {
                    out += it->second[index];
                    i = end + 1;
                    continue;
                }
            }
        }
        out += open;
        ++i;
    }
    return out;
}

// Waits for the chamber to reach at least `celsius`: M191, or Klipper's
// TEMPERATURE_WAIT on the chamber's heater.
std::string chamber_wait_gcode(GcodeFlavor flavor, double celsius) {
    const std::string t = std::to_string(std::lround(celsius));
    return flavor == GcodeFlavor::Klipper ? "TEMPERATURE_WAIT SENSOR=\"heater_generic chamber\" MINIMUM=" + t
                                          : "M191 S" + t + " ; wait for the chamber";
}

std::string chamber_gcode(GcodeFlavor flavor, double celsius) {
    const std::string t = std::to_string(std::lround(celsius));
    return flavor == GcodeFlavor::Klipper ? "SET_HEATER_TEMPERATURE HEATER=chamber TARGET=" + t + " ; the chamber"
                                          : "M141 S" + t + " ; the chamber";
}

std::vector<Warning> capability_warnings(const Settings& s) {
    std::vector<Warning> out;
    const auto number = [&](const char* variable) {
        const auto it = s.variables.find(variable);
        return it == s.variables.end() || it->second.empty() ? 0.0 : std::atof(it->second.front().c_str());
    };
    const auto add = [&](const char* kind, std::string message, double value) {
        out.push_back({kind, -1, 0.0, std::move(message), value});
    };
    char text[200];
    if (const double t = number("temperature"); s.max_nozzle_temp > 0.0 && t > s.max_nozzle_temp) {
        std::snprintf(text, sizeof text, "the filament wants %.0f C at the nozzle; the printer reaches %.0f C", t,
                      s.max_nozzle_temp);
        add("nozzle_temperature", text, t);
    }
    if (const double t = number("bed_temperature"); s.max_bed_temp > 0.0 && t > s.max_bed_temp) {
        std::snprintf(text, sizeof text, "the filament wants the bed at %.0f C; the printer reaches %.0f C", t,
                      s.max_bed_temp);
        add("bed_temperature", text, t);
    }
    // The changes by height too (a temperature tower, a project's changes).
    for (const HeightChange& c : s.changes) {
        if (c.temperature && s.max_nozzle_temp > 0.0 && *c.temperature > s.max_nozzle_temp) {
            std::snprintf(text, sizeof text, "the change at Z %.2f asks %.0f C at the nozzle; the printer reaches %.0f C",
                          c.z, *c.temperature, s.max_nozzle_temp);
            add("nozzle_temperature", text, *c.temperature);
        }
        if (c.bed_temperature && s.max_bed_temp > 0.0 && *c.bed_temperature > s.max_bed_temp) {
            std::snprintf(text, sizeof text, "the change at Z %.2f asks the bed at %.0f C; the printer reaches %.0f C", c.z,
                          *c.bed_temperature, s.max_bed_temp);
            add("bed_temperature", text, *c.bed_temperature);
        }
    }
    // Lines this far from the nozzle print, but rarely well (check_settings
    // refuses the impossible ones).
    if (const double w = s.wall_width, d = s.nozzle_diameter; w < 0.6 * d || w > 2.5 * d) {
        std::snprintf(text, sizeof text, "lines of %.3g mm from a %.3g mm nozzle: %.3g to %.3g mm print best", w, d,
                      0.6 * d, 2.5 * d);
        add("line_width", text, w);
    }
    // Wavy infill asked, with an infill whose lines move from layer to layer
    // or sit too close: it stays flat, and says so.
    if (s.infill_nonplanar && s.infill_density > 0.0 && !wave_params(s))
        add("nonplanar_ignored",
            std::string("wavy infill needs an infill that repeats layer to layer (aligned_rectilinear, grid, "
                        "triangles, stars, honeycomb, concentric, hilbert, archimedean, octagram) with its lines far "
                        "enough apart; ") +
                infill_pattern_name(s.infill_pattern) + " at this density stays flat",
            0.0);
    if (s.chamber_temperature > 0.0 && s.max_chamber_temp <= 0.0) {
        std::snprintf(text, sizeof text, "the filament wants a chamber at %.0f C; the printer does not heat one",
                      s.chamber_temperature);
        add("chamber", text, s.chamber_temperature);
    } else if (s.chamber_temperature > s.max_chamber_temp && s.max_chamber_temp > 0.0) {
        std::snprintf(text, sizeof text, "the filament wants a chamber at %.0f C; the printer heats it to %.0f C",
                      s.chamber_temperature, s.max_chamber_temp);
        add("chamber", text, s.chamber_temperature);
    }
    if (s.needs_enclosure && !s.enclosed)
        add("enclosure", "the filament wants an enclosed printer, and this one is not marked as enclosed", 0.0);
    if (s.abrasive && !s.hardened_nozzle)
        add("abrasive", "the filament is abrasive and the nozzle is not marked as hardened: a brass nozzle wears out", 0.0);
    return out;
}

std::string acceleration_gcode(GcodeFlavor flavor, double print, double travel) {
    const std::string p = std::to_string(std::lround(print)), t = std::to_string(std::lround(travel));
    if (flavor == GcodeFlavor::Klipper)
        return "SET_VELOCITY_LIMIT ACCEL=" + p;
    if (flavor == GcodeFlavor::MarlinLegacy) // Marlin 1: S for printing, T for retractions
        return "M204 S" + p + " ; acceleration";
    return "M204 P" + p + " T" + t + " ; acceleration";
}

std::string machine_limits_gcode(const Settings& s) {
    const Dialect d = dialect_of(s.flavor);
    if (d.limits == Dialect::Limits::None)
        return {};
    const bool rrf = d.limits == Dialect::Limits::RepRapFirmware;
    std::string out;
    // A command with the known values of X, Y, Z and E (0: not known, left out).
    const auto axes = [&](const char* command, std::array<double, 4> values, double scale, int decimals) {
        std::string line = command;
        for (int i = 0; i < 4; ++i)
            if (values[std::size_t(i)] > 0.0) {
                line += ' ';
                line += "XYZE"[i];
                line += decimals > 0 ? fixed(values[std::size_t(i)] * scale, decimals)
                                     : trimmed(values[std::size_t(i)] * scale, 2);
            }
        if (line.size() > std::strlen(command))
            out += line + "\n";
    };
    axes("M201", {s.max_accel_x, s.max_accel_y, s.max_accel_z, s.max_accel_e}, 1.0, 0);
    // RepRapFirmware's M203 and M566 are in mm/min.
    axes("M203", {s.max_feed_x, s.max_feed_y, s.max_feed_z, s.max_feed_e}, rrf ? 60.0 : 1.0, 0);
    const std::string print = trimmed(s.acceleration, 2), travel = trimmed(s.travel_acceleration, 2);
    const std::string retract = trimmed(s.retract_acceleration > 0.0 ? s.retract_acceleration : s.travel_acceleration, 2);
    if (d.limits == Dialect::Limits::Marlin2)
        out += "M204 P" + print + " R" + retract + " T" + travel + "\n";
    else if (d.limits == Dialect::Limits::Marlin1)
        out += "M204 S" + print + " T" + retract + "\n";
    else
        out += "M204 P" + print + " T" + travel + "\n";
    const std::array<double, 4> jerks = {s.jerk_x, s.jerk_y, s.jerk_z, s.jerk_e};
    if (rrf) {
        axes("M566", jerks, 60.0, 2);
    } else {
        if (s.junction_deviation > 0.0)
            out += "M205 J" + fixed(s.junction_deviation, 3) + "\n";
        else
            axes("M205", jerks, 1.0, 2);
        out += "M205 S" + trimmed(s.min_extrude_feed, 2) + " T" + trimmed(s.min_travel_feed, 2) + "\n";
    }
    return out;
}

TemplateContext template_context(const Settings& s, const SlicedModel& model) {
    TemplateContext c;
    // Every setting by its name (layers.height, machine.flavor...).
    for (const auto& [key, value] : describe(s))
        c.variables[key] = TemplateValue::parse(value);
    const auto list = [](std::vector<TemplateValue> items) { return TemplateValue::of(std::move(items)); };
    // The usual names of the most used ones, the per-extruder ones as lists.
    c.variables["layer_height"] = TemplateValue::of(s.layer_height);
    c.variables["first_layer_height"] = TemplateValue::of(s.first_layer_height);
    c.variables["nozzle_diameter"] = list({TemplateValue::of(s.nozzle_diameter)});
    c.variables["filament_diameter"] = list({TemplateValue::of(s.filament_diameter)});
    // machine.variables: one value per extruder.
    for (const auto& [name, values] : s.variables) {
        std::vector<TemplateValue> items;
        for (const std::string& v : values)
            items.push_back(TemplateValue::parse(v));
        c.variables[name] = list(std::move(items));
    }
    // The print's own (gcode_templates.md, rule 7).
    c.variables["total_layer_count"] = TemplateValue::of(static_cast<long long>(model.layers.size()));
    c.variables["num_extruders"] = TemplateValue::of(1LL);
    c.variables["initial_extruder"] = c.variables["initial_tool"] = TemplateValue::of(0LL);
    c.variables["is_extruder_used"] = list({TemplateValue::of(true)});
    c.variables["input_filename_base"] = TemplateValue::of(s.input_name);
    double lo[2] = {0.0, 0.0}, hi[2] = {0.0, 0.0};
    bool any = false;
    if (!model.regions.empty())
        for (const Polygon& polygon : model.regions.front())
            for (const auto& p : polygon) {
                const double xy[2] = {to_mm(p.x), to_mm(p.y)};
                for (int a = 0; a < 2; ++a) {
                    lo[a] = any ? std::min(lo[a], xy[a]) : xy[a];
                    hi[a] = any ? std::max(hi[a], xy[a]) : xy[a];
                }
                any = true;
            }
    const auto pair = [&](double x, double y) {
        return list({TemplateValue::of(std::round(x * 1000.0) / 1000.0), TemplateValue::of(std::round(y * 1000.0) / 1000.0)});
    };
    c.variables["first_layer_print_min"] = pair(lo[0], lo[1]);
    c.variables["first_layer_print_max"] = pair(hi[0], hi[1]);
    c.variables["first_layer_print_size"] = pair(hi[0] - lo[0], hi[1] - lo[1]);
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    char stamp[32];
    std::strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", &local);
    c.variables["year"] = TemplateValue::of(static_cast<long long>(local.tm_year + 1900));
    c.variables["timestamp"] = TemplateValue::of(std::string(stamp));
    // The layer the templates are at: the first, until a layer change.
    const double first_z = model.layers.empty() ? 0.0 : model.layers.front().top - model.layers.front().bottom;
    c.variables["layer_num"] = TemplateValue::of(0LL);
    c.variables["layer_z"] = c.variables["max_layer_z"] = TemplateValue::of(std::round(first_z * 1000.0) / 1000.0);
    return c;
}

bool sets_temperature(std::string_view gcode) {
    std::size_t at = 0;
    while (at < gcode.size()) {
        const std::size_t end = std::min(gcode.find('\n', at), gcode.size());
        std::string_view line = gcode.substr(at, end - at);
        at = end + 1;
        line = line.substr(std::min(line.find_first_not_of(" \t"), line.size()));
        const std::string_view word = line.substr(0, line.find_first_of(" \t;\r"));
        std::string command(word);
        for (char& c : command)
            c = char(std::toupper(static_cast<unsigned char>(c)));
        if (command == "M104" || command == "M109" || command == "M140" || command == "M190" || command == "M141" ||
            command == "M191" || command == "M116")
            return true;
        // RepRapFirmware's G10 with S sets a tool's temperature (a bare G10 retracts).
        if (command == "G10" && line.find_first_of("Ss", word.size()) != std::string_view::npos)
            return true;
    }
    return false;
}

std::string pressure_advance_gcode(GcodeFlavor flavor, double value) {
    const std::string v = fixed(value, 4);
    switch (flavor) {
    case GcodeFlavor::Marlin:
    case GcodeFlavor::MarlinLegacy:
        return "M900 K" + v + " ; pressure advance";
    case GcodeFlavor::Klipper:
        return "SET_PRESSURE_ADVANCE ADVANCE=" + v;
    case GcodeFlavor::RepRap:
    case GcodeFlavor::RepRapFirmware:
    case GcodeFlavor::Repetier:
    case GcodeFlavor::Smoothie:
        return "M572 D0 S" + v + " ; pressure advance";
    default: // the old dialects: none (src/gcode/dialect.hpp)
        return {};
    }
}

void write_gcode(std::ostream& target, const SlicedModel& model, const Settings& s, std::vector<Warning>* warnings) {
    require_speeds(s); // from the printer's profile: the writer has none of its own
    // With M73 by time (gcode_firmware.md, rule 5) the G-code is kept until
    // the times are known; else it goes out as it is written.
    std::ostringstream held;
    std::ostream& out = s.gcode_progress ? static_cast<std::ostream&>(held) : target;
    out << "; generated by Tisma engine " << TISMA_VERSION << "\n";
    // The dialect of the firmware (gcode_extras.md, rule 6; gcode_firmware.md).
    const Dialect dialect = dialect_of(s.flavor);
    Emitter g(s);
    g.set_flow(s.flow / 100.0);
    g.type("Custom");
    if (s.gcode_progress)
        g.progress_mark();
    g.flush(out);
    // The machine's limits, before the start G-code (gcode_firmware.md, rule 2).
    if (s.limits_usage == LimitsUsage::Emit)
        out << machine_limits_gcode(s);
    // The chamber the filament wants, when the machine heats one: set before
    // the start (so it heats while the nozzle does) and again after it, once
    // it reaches extrusion.chamber_min_temperature (gcode_extras.md, rule 3).
    const double chamber = std::min(s.chamber_temperature, s.max_chamber_temp);
    const bool heat_chamber = s.chamber_temperature > 0.0 && s.max_chamber_temp > 0.0;
    if (heat_chamber)
        out << chamber_gcode(s.flavor, chamber) << "\n";
    // The nozzle's temperature, set before the start G-code and waited for
    // after it, unless the start sets temperatures itself (gcode_firmware.md, rule 1).
    // The templates (gcode_templates.md): every setting, the machine's
    // variables and the print's; globals reach the templates that follow.
    TemplateContext templates = template_context(s, model);
    const std::string start_gcode = render_template(s.start_gcode, templates, "machine.start_gcode");
    const auto first_value = [&](const char* name) {
        const auto it = s.variables.find(name);
        return it == s.variables.end() || it->second.empty() ? 0.0 : std::atof(it->second.front().c_str());
    };
    const double nozzle = first_value("temperature"), bed = first_value("bed_temperature");
    // The bed first, waited for, then the nozzle set (gcode_templates.md, rule 10, T10).
    const bool heat = s.auto_temperatures && !sets_temperature(start_gcode);
    const bool heat_nozzle = heat && nozzle > 0.0;
    if (heat && bed > 0.0)
        out << (dialect.wait == Dialect::Wait::None ? "M140 " : "M190 ") << dialect.temperature << std::lround(bed)
            << "\n";
    const std::string celsius = std::to_string(std::lround(nozzle));
    if (heat_nozzle)
        out << (dialect.g10 ? "G10 S" + celsius + " P0"
                            : std::string("M104 ") + dialect.temperature + celsius + (dialect.tool_on_m104 ? " T0" : ""))
            << "\n";
    if (!start_gcode.empty())
        out << start_gcode << "\n";
    if (heat_nozzle && dialect.wait != Dialect::Wait::None)
        out << (dialect.wait == Dialect::Wait::M116 ? std::string("M116") : std::string("M109 ") + dialect.temperature + celsius)
            << "\n";
    // The filament's own start, after the printer's (rule 9).
    if (const std::string text = render_template(s.filament_start_gcode, templates, "extrusion.start_gcode"); !text.empty())
        out << text << "\n";
    // How far an arc may stray from the path (gcode.resolution; with every
    // point kept, a thousandth).
    const double arc_tolerance = s.gcode_resolution > 0.0 ? s.gcode_resolution : 0.001;
    const bool relative = relative_e(s);
    if (s.pressure_advance > 0.0 && dialect.accelerations)
        out << pressure_advance_gcode(s.flavor, s.pressure_advance) << "\n";
    if (s.acceleration > 0.0 && dialect.accelerations)
        out << acceleration_gcode(s.flavor, s.acceleration, s.travel_acceleration) << "\n";
    if (heat_chamber) {
        if (s.chamber_min_temperature > 0.0)
            out << chamber_wait_gcode(s.flavor, std::min(s.chamber_min_temperature, chamber)) << "\n";
        out << chamber_gcode(s.flavor, chamber) << "\n";
    }
    if (warnings) {
        const std::vector<Warning> capability = capability_warnings(s);
        warnings->insert(warnings->end(), capability.begin(), capability.end());
    }
    out << (s.verbose_comments ? "G90 ; use absolute coordinates\n" : "G90\n");
    if (dialect.e_mode)
        out << (relative ? "M83" : "M82")
            << (!s.verbose_comments ? "\n"
                : relative          ? " ; use relative distances for extrusion\n"
                                    : " ; use absolute distances for extrusion\n");
    Point2d cursor{0.0, 0.0};
    SeamMemory seams;

    // The values the height changes set, layer by layer (docs/CONFIG.md, "changes").
    struct Active {
        double retract_length = 0.0, retract_speed = 1.0, flow = 1.0, speed = 1.0;
        double volumetric_flow = 0.0; // mm3/s forced on every extrusion; 0: none
        std::optional<double> fan;
        std::string lines; // what this layer writes first: temperatures and the changes' own G-code
        bool user = false; // the lines hold a change's own G-code
    };
    Active active;
    active.retract_length = s.retract_length;
    active.retract_speed = s.retract_speed;
    active.flow = s.flow / 100.0;
    std::size_t next_change = 0;
    // The part prints resting on the bed wherever its mesh is in Z: heights
    // in the G-code (and of the height changes) count from its bottom.
    // machine.z_offset moves every Z of the part (gcode_extras.md, rule 5).
    const double base = (model.layers.empty() ? 0.0 : model.layers.front().bottom) - s.z_offset;
    // The layer the templates are at: its number (from 0) and its Z as written.
    const auto at_layer = [&](std::size_t l) {
        const double z = std::round((model.layers[l].top - base) * 1000.0) / 1000.0;
        templates.variables["layer_num"] = TemplateValue::of(static_cast<long long>(l));
        templates.variables["layer_z"] = templates.variables["max_layer_z"] = TemplateValue::of(z);
    };

    // No extrusion faster than speed.max, when it is set.
    const auto capped = [&](double v) { return s.max_speed > 0.0 ? std::min(v, s.max_speed) : v; };
    // Each kind's speed (gcode_motion.md; speeds_accels.md, rules 1 and 5):
    // bridges and gap fill at their own or the solid infill's; the sparse
    // infill within the filament's cap for its pattern, whose lines cross in
    // the layer (two or three directions at once) or do not.
    const double bridge_feed = s.bridge_speed > 0.0 ? s.bridge_speed : s.solid_infill_speed;
    const double gap_feed = s.gap_fill_speed > 0.0 ? s.gap_fill_speed : s.solid_infill_speed;
    const bool crossing = s.infill_pattern == InfillPattern::Grid || s.infill_pattern == InfillPattern::Triangles ||
                          s.infill_pattern == InfillPattern::Stars || s.infill_pattern == InfillPattern::Cubic ||
                          s.infill_pattern == InfillPattern::AdaptiveCubic ||
                          s.infill_pattern == InfillPattern::SupportCubic;
    const double infill_cap = crossing ? s.infill_max_crossing_speed : s.infill_max_speed;
    const double sparse_feed = infill_cap > 0.0 ? std::min(s.infill_speed, infill_cap) : s.infill_speed;
    const double fill_speed[7] = {s.solid_infill_speed, s.top_solid_infill_speed, bridge_feed, gap_feed,
                                  sparse_feed,          s.support_speed,          s.support_speed};
    // The bridges' round bead, for the walls over the air.
    const double bead = bridge_bead(s);
    // Each kind's acceleration (speeds_accels.md, rules 10 and 12): with the
    // replacements of the spec's table; none without acceleration.default.
    const auto or_else = [](double v, double fallback) { return v > 0.0 ? v : fallback; };
    const double accel_walls = or_else(s.accel_walls, s.accel_default);
    const double accel_outer = or_else(s.accel_outer_wall, accel_walls);
    const double accel_infill = or_else(s.accel_infill, s.accel_default);
    const double accel_solid = or_else(s.accel_solid_infill, accel_infill);
    const double accel_top = or_else(s.accel_top_solid_infill, accel_solid);
    const double accel_bridges = or_else(s.accel_bridges, s.accel_default);
    const auto acceleration_of = [&](const char* type, bool first, bool over_raft) {
        if (!(s.accel_default > 0.0))
            return 0.0;
        if (first && s.accel_first_layer > 0.0)
            return s.accel_first_layer;
        if (over_raft && s.accel_first_layer_over_raft > 0.0)
            return s.accel_first_layer_over_raft;
        const auto is = [&](const char* name) { return std::strcmp(type, name) == 0; };
        return is("External perimeter")                             ? accel_outer
               : is("Perimeter")                                    ? accel_walls
               : is("Overhang perimeter") || is("Bridge infill")    ? accel_bridges
               : is("Internal infill")                              ? accel_infill
               : is("Solid infill")                                 ? accel_solid
               : is("Top solid infill")                             ? accel_top
                                                                    : s.accel_default; // support, skirt, gap fill
    };

    // Every layer's travel planner and islands first, in parallel: they
    // depend on the layer's region alone, and the layers are written one after
    // another. A layer with the region of the layer below shares them
    // (prismatic parts; the speed report's V10).
    struct RegionPlan {
        std::optional<TravelPlanner> planner;
        std::vector<Polygons> pieces;
        std::vector<std::array<std::int64_t, 4>> boxes; // each piece's box
    };
    std::vector<std::size_t> plan_of(model.layers.size(), 0);
    {
        std::vector<char> same(model.layers.size(), 0);
        tbb::parallel_for(std::size_t(1), std::min(model.layers.size(), model.regions.size()),
                          [&](std::size_t l) { same[l] = model.regions[l] == model.regions[l - 1]; });
        for (std::size_t l = 0; l < plan_of.size(); ++l)
            plan_of[l] = same[l] ? plan_of[l - 1] : l;
    }
    std::vector<RegionPlan> plans(model.layers.size());
    const Polygons none;
    tbb::parallel_for(std::size_t(0), plans.size(), [&](std::size_t l) {
        if (plan_of[l] != l)
            return;
        const Polygons& region = l < model.regions.size() ? model.regions[l] : none;
        RegionPlan& p = plans[l];
        p.planner.emplace(region, s.wall_width);
        p.pieces = split_islands(region);
        // The point-in-polygon tests run only where a box holds the point
        // (hundreds of islands made every path test them all).
        p.boxes.assign(p.pieces.size(), {INT64_MAX, INT64_MAX, INT64_MIN, INT64_MIN});
        for (std::size_t k = 0; k < p.pieces.size(); ++k)
            for (const auto& q : p.pieces[k].front()) {
                auto& b = p.boxes[k];
                b[0] = std::min(b[0], q.x), b[1] = std::min(b[1], q.y), b[2] = std::max(b[2], q.x), b[3] = std::max(b[3], q.y);
            }
    });

    // What a layer lays, in order: its paths with their line types and
    // feeds, and the thick zone's changes of height (no type).
    struct Step {
        Toolpath path;
        const char* type = nullptr;
        double feed = 0.0;
        double height = 0.0;
        // Where the head comes from, when the plan knows it, and the travel
        // from there, planned with the paths across layers; the emitter takes
        // it when the head is exactly there (a route around the walls costs
        // hundreds of tests, and the layers are written one after another).
        std::optional<Point2d> from;
        TravelPlan travel;
    };
    // The aligned seam's candidates of each wall loop of a layer (by its
    // place in the layer's walls), found for many layers at once ahead of
    // the plans, which only pick among them (perimeters.md, rule 8).
    std::vector<std::vector<std::vector<std::size_t>>> loop_candidates(model.layers.size());
    // Where the last plan ended (simplifying keeps a path's ends).
    std::optional<Point2d> last_end;
    // Vase mode (vase_mode.md, rule 4): from the layer over the bottom ones
    // (the second, without any), a layer whose only path is its outer wall's
    // loop is one turn of a spiral: it starts at the point nearest to where
    // the layer below ended, going on from there when that was a turn too,
    // and rises a layer along its length, every bead a layer thick (on the
    // turn below). Another layer is laid flat, and the next turn starts anew.
    std::vector<char> continues(model.layers.size(), 0);
    bool after_turn = false;
    const std::size_t spiral_from = std::max<std::size_t>(std::size_t(std::max(s.bottom_layers, 0)), 1);
    const auto spiral_turn = [&](std::vector<Step>& steps, std::size_t l) {
        Step* only = nullptr;
        for (Step& step : steps) {
            if (!step.type || only) // a thick zone's height, or a second path
                return false;
            only = &step;
        }
        if (!only || std::strcmp(only->type, "External perimeter") != 0 || !only->path.closed ||
            only->path.points.size() < 3)
            return false;
        Toolpath& loop = only->path;
        for (const ToolpathPoint& p : loop.points)
            if (p.dz != 0.0 || p.below != 0.0)
                return false;
        if (last_end) {
            std::size_t nearest = 0;
            double best = std::numeric_limits<double>::infinity();
            for (std::size_t i = 0; i < loop.points.size(); ++i) {
                const double d = std::hypot(loop.points[i].p.x - last_end->x, loop.points[i].p.y - last_end->y);
                if (d < best)
                    best = d, nearest = i;
            }
            std::rotate(loop.points.begin(), loop.points.begin() + std::ptrdiff_t(nearest), loop.points.end());
        }
        std::vector<ToolpathPoint> points;
        if (after_turn && last_end) {
            points.push_back(loop.points.front());
            points.back().p = *last_end;
        }
        points.insert(points.end(), loop.points.begin(), loop.points.end());
        points.push_back(loop.points.front()); // round to where it started
        double total = 0.0;
        for (std::size_t i = 1; i < points.size(); ++i)
            total += std::hypot(points[i].p.x - points[i - 1].p.x, points[i].p.y - points[i - 1].p.y);
        if (!(total > 0.0))
            return false;
        const double h = model.layers[l].top - model.layers[l].bottom;
        double along = 0.0;
        for (std::size_t i = 0; i < points.size(); ++i) {
            if (i > 0)
                along += std::hypot(points[i].p.x - points[i - 1].p.x, points[i].p.y - points[i - 1].p.y);
            points[i].dz = -h * (1.0 - along / total);
            points[i].below = points[i].dz;
        }
        points.back().dz = points.back().below = 0.0;
        loop.points = std::move(points);
        loop.closed = false;
        loop.wavy = true;
        return true;
    };
    // The order of a layer's paths and their seams. It goes on from where the
    // layer below ended and from its seams, so the layers are planned one
    // after another; the G-code is written apart (write_layer), so a layer is
    // written while the next ones are planned (the speed report's V5).
    auto plan_layer = [&](std::size_t l, const Active& a) {
        const Layer& layer = model.layers[l];
        std::vector<Step> steps;
        seams.next_layer();
        const RegionPlan& plan = plans[plan_of[l]];
        const bool first = l == 0;
        // The part's first layer on a raft: no faster than
        // speed.first_layer_over_raft, or that share of each speed
        // (skirt_brim_raft.md, rule 12); unset, speed.first_layer.
        const bool over_raft = model.raft_layers > 0 && l == model.raft_layers;
        // A speed change scales every extrusion, past the usual caps (a speed tower).
        auto speed = [&](double v) {
            if (over_raft)
                v = std::min(v, s.first_layer_over_raft.value > 0.0 ? s.first_layer_over_raft.of(v) : s.first_layer_speed);
            return capped(first ? std::min(v, s.first_layer_speed) : v) * a.speed;
        };
        // The fills' feeds; the first layer's solid infill at its own speed,
        // past speed.first_layer (speeds_accels.md, rule 3).
        auto fill_feed = [&](std::size_t k) {
            if (first && k <= 1 && s.first_layer_infill_speed.value > 0.0)
                return capped(s.first_layer_infill_speed.of(s.solid_infill_speed)) * a.speed;
            return speed(fill_speed[k]);
        };
        const double over_feed = s.over_bridges_speed.value > 0.0 ? speed(s.over_bridges_speed.of(s.solid_infill_speed)) : 0.0;
        // A path that fills the support's Z gap (supports_skirt.md, rule 4) is
        // as thick as the layer and the gap: the height changes around it.
        const double layer_height = layer.top - layer.bottom;
        double base_height = layer_height, laid_height = layer_height;
        auto lay = [&](Toolpath path, const char* type, double feed) {
            if (path.points.empty())
                return;
            if (const double h = base_height + path.gap; h != laid_height) {
                steps.push_back({{}, nullptr, 0.0, h, {}, {}});
                laid_height = h;
            }
            steps.push_back({std::move(path), type, feed, 0.0, {}, {}});
        };
        // Where this layer's walls hang (walls_extra.md, rule 8) and rest on
        // the support (supports_skirt.md, rule 4).
        const Polygons* const hanging_area =
            l < model.hanging.size() && !model.hanging[l].empty() ? &model.hanging[l] : nullptr;
        const Polygons& on_support_area = l < model.on_support.size() ? model.on_support[l] : none;
        const double support_gap = l < model.support_gap.size() ? model.support_gap[l] : 0.0;
        // A vase's wall stays one turn of its spiral (vase_mode.md, rule 4).
        const bool split_walls = hanging_area && !s.vase && (s.detect_overhangs || !on_support_area.empty());
        std::optional<AreaIndex> hanging, on_support;
        if (split_walls)
            hanging.emplace(*hanging_area), on_support.emplace(on_support_area);
        const RoundBead round{bead, std::max(0.0, std::numbers::pi / 4.0 * bead - layer_height)};
        // The solid infill right over a bridge, at speed.over_bridges
        // (speeds_accels.md, rule 2).
        std::optional<AreaIndex> over_bridge, nowhere;
        if (s.over_bridges_speed.value > 0.0 && l > 0 && l - 1 < model.air_bridges.size() &&
            !model.air_bridges[l - 1].empty())
            over_bridge.emplace(model.air_bridges[l - 1]), nowhere.emplace(none);
        // The walls by how much of their bead rests on the layer below
        // (speeds_accels.md, rules 6 and 7); a vase's wall stays one turn.
        std::optional<AreaIndex> below_outline, air;
        if (s.dynamic_overhangs && !s.vase && l > 0 && l - 1 < model.regions.size() && !model.regions[l - 1].empty()) {
            below_outline.emplace(model.regions[l - 1]);
            if (hanging_area)
                air.emplace(*hanging_area);
        }
        const auto in_air = [&](const Point2d& p) { return air && air->contains(p.x, p.y); };
        // The feed of a wall at `normal` with `share` of its bead held: the
        // overhang speeds at 0, 25, 50 and 75 %, in a straight line between,
        // never faster than its own.
        const auto dynamic_feed = [&](double share, double normal) {
            const double outer = s.outer_wall_speed.of(s.wall_speed);
            std::array<double, 5> at{};
            for (std::size_t k = 0; k < 4; ++k)
                at[k] = s.overhang_speeds[k].value > 0.0 ? speed(s.overhang_speeds[k].of(outer)) : normal;
            at[4] = normal;
            const double x = std::clamp(share, 0.0, 1.0) * 4.0;
            const std::size_t k = std::min<std::size_t>(3, std::size_t(x));
            return std::min(normal, at[k] + (x - double(k)) * (at[k + 1] - at[k]));
        };
        bool in_thick = false;
        const std::uint64_t seed = std::uint64_t(l) * 1000003u;
        // Z anti-aliasing (docs/ZAA.md): of the paths off the layer's top, the
        // lower ones first, so the nozzle never runs beside a higher bead it
        // has just laid (Song et al. 2017, Section 4), in bands of 0.005 mm.
        auto band = [](const Toolpath& t) {
            double sum = 0.0;
            for (const ToolpathPoint& p : t.points)
                sum += p.dz;
            return std::floor(sum / double(std::max<std::size_t>(t.points.size(), 1)) / 0.005);
        };
        // In an island with paths off the layer's top, every path is split
        // where it goes below, on or above the top, and all of them, walls and
        // fills alike, are laid lowest first.
        struct Pending {
            Toolpath path;
            const char* type;
            double feed;
        };
        std::vector<Pending> pending;
        bool deferring = false;
        // The seams painted on this layer, for its walls.
        SeamPaint painted;
        if (l < model.seam_enforce.size())
            painted.enforce = &model.seam_enforce[l];
        if (l < model.seam_block.size())
            painted.block = &model.seam_block[l];
        // over: the feed of what lies over a bridge (solid infill).
        auto group = [&](const std::vector<Toolpath>& paths, const char* type, double feed, SeamMode seam,
                         bool walls = false, std::span<const std::vector<std::size_t>* const> candidates = {},
                         double over = 0.0) {
            std::vector<Toolpath> ordered = order_paths(paths, cursor, seam, seed, seams,
                                                        walls && !painted.empty() ? &painted : nullptr, candidates);
            std::vector<Pending> typed;
            typed.reserve(ordered.size());
            for (Toolpath& t : ordered) {
                if (over > 0.0 && over_bridge && !in_thick) {
                    for (WallPiece& piece : wall_pieces(t, *over_bridge, *nowhere, 0.0, true))
                        typed.push_back({std::move(piece.path), type, piece.overhang ? over : feed});
                    continue;
                }
                if (!walls || in_thick || (!split_walls && !below_outline)) {
                    typed.push_back({std::move(t), type, feed});
                    continue;
                }
                // Overhang perimeters at the bridges' speed, with their round
                // bead over the air (walls_extra.md, rule 8).
                std::vector<WallPiece> pieces =
                    split_walls ? wall_pieces(t, *hanging, *on_support, support_gap, s.detect_overhangs, &round)
                                : std::vector<WallPiece>(1, WallPiece{std::move(t)});
                for (WallPiece& piece : pieces) {
                    const char* kind = piece.overhang ? "Overhang perimeter" : type;
                    if (!below_outline || (piece.overhang && !piece.air)) { // over the support: as a bridge
                        typed.push_back({std::move(piece.path), kind, piece.overhang ? speed(bridge_feed) : feed});
                        continue;
                    }
                    // By how much rests on the layer below: in the air, overhang_0.
                    if (piece.air) {
                        typed.push_back({std::move(piece.path), kind, dynamic_feed(0.0, feed)});
                        continue;
                    }
                    for (auto& [path, f] : by_support(piece.path, *below_outline, in_air,
                                                      [&](double share) { return dynamic_feed(share, feed); }))
                        typed.push_back({std::move(path), kind, f});
                }
            }
            if (deferring) {
                for (Pending& p : typed) {
                    if (p.path.wavy) { // its waves are its own: not cut by height
                        pending.push_back(std::move(p));
                        continue;
                    }
                    for (Toolpath& piece : split_by_height(p.path)) {
                        piece.gap = p.path.gap;
                        pending.push_back({std::move(piece), p.type, p.feed});
                    }
                }
                return;
            }
            for (Pending& p : typed)
                lay(std::move(p.path), p.type, p.feed);
        };
        auto off_top = [](std::initializer_list<const std::vector<Toolpath>*> lists) {
            for (const std::vector<Toolpath>* list : lists)
                for (const Toolpath& t : *list)
                    for (const ToolpathPoint& p : t.points)
                        if (p.dz != 0.0)
                            return true;
            return false;
        };
        // The nozzle's reach (Song et al. 2017, Fig. 13): its flat end around
        // the bead, and a layer's height at 45 degrees up its side.
        const double reach = std::min(0.5 * (2.5 * s.nozzle_diameter + s.wall_width) + (layer.top - layer.bottom),
                                      1.6 * s.wall_width);
        auto flush = [&]() {
            // By height bands first, then the height graph within the island.
            std::stable_sort(pending.begin(), pending.end(),
                             [&](const Pending& lhs, const Pending& rhs) { return band(lhs.path) < band(rhs.path); });
            std::vector<const Toolpath*> paths;
            for (const Pending& p : pending)
                paths.push_back(&p.path);
            for (const std::size_t k : height_order(paths, reach))
                lay(std::move(pending[k].path), pending[k].type, pending[k].feed);
            pending.clear();
            deferring = false;
        };
        // Skirt, then brim, before the part, at the first layer's speed.
        // A draft shield goes up to the top (skirt_brim_raft.md, rule 4).
        if (l < std::size_t(std::max(s.skirt_layers, 0)) || s.draft_shield == DraftShield::Enabled)
            group(model.skirt, "Skirt/Brim", capped(s.first_layer_speed) * a.speed, SeamMode::Nearest);
        if (l == 0)
            group(model.brim, "Skirt/Brim", capped(s.first_layer_speed) * a.speed, SeamMode::Nearest);
        // Every island is finished before the next, the nearest one first. In an
        // island: walls from the inside out, the outer wall (bead 0) last, each
        // bead index in nearest order; then the fills, one kind after the other.
        struct Island {
            std::map<int, std::vector<Toolpath>, std::greater<int>> walls;
            std::map<int, std::vector<const std::vector<std::size_t>*>> candidates; // each wall's seam candidates
            std::array<std::vector<Toolpath>, 7> fills; // solid, top, bridge, gaps, sparse, support, support interface
            std::vector<Toolpath> arcs;                 // over overhangs, in their order
            std::vector<Toolpath> ironing;              // over the top surfaces, after the fills
            std::vector<Point2d> ends;                  // where its paths start or end
        };
        const std::vector<Polygons>& pieces = plan.pieces;
        std::vector<Island> islands(pieces.size() + 1); // the last one takes paths in no island
        const std::vector<std::array<std::int64_t, 4>>& piece_box = plan.boxes;
        auto island = [&](const Toolpath& t) -> Island& {
            if (pieces.size() <= 1)
                return islands.front();
            // As island_of: the first island whose outline holds the point and
            // none of whose holes do.
            const Point2d p = t.points.front().p;
            const Clipper2Lib::Point64 q{to_units(p.x), to_units(p.y)};
            for (std::size_t k = 0; k < pieces.size(); ++k) {
                const auto& b = piece_box[k];
                if (q.x < b[0] || q.x > b[2] || q.y < b[1] || q.y > b[3])
                    continue;
                if (Clipper2Lib::PointInPolygon(q, pieces[k].front()) == Clipper2Lib::PointInPolygonResult::IsOutside)
                    continue;
                bool in_hole = false;
                for (std::size_t h = 1; h < pieces[k].size() && !in_hole; ++h)
                    in_hole = Clipper2Lib::PointInPolygon(q, pieces[k][h]) == Clipper2Lib::PointInPolygonResult::IsInside;
                if (!in_hole)
                    return islands[k];
            }
            return islands[pieces.size()];
        };
        auto add = [&](Island& i, const Toolpath& t) {
            i.ends.push_back(t.points.front().p);
            if (!t.closed)
                i.ends.push_back(t.points.back().p);
        };
        if (l < model.walls.size())
            for (std::size_t k = 0; k < model.walls[l].toolpaths.size(); ++k)
                if (const Toolpath& t = model.walls[l].toolpaths[k]; !t.points.empty()) {
                    Island& i = island(t);
                    i.walls[t.index].push_back(t);
                    i.candidates[t.index].push_back(k < loop_candidates[l].size() ? &loop_candidates[l][k] : nullptr);
                    add(i, t);
                }
        // Gap fill between walls of one width (walls_extra.md, rule 1).
        if (l < model.walls.size())
            for (const Toolpath& t : model.walls[l].gaps)
                if (!t.points.empty()) {
                    Island& i = island(t);
                    i.fills[3].push_back(t);
                    add(i, t);
                }
        if (l < model.fills.size()) {
            const LayerFill& f = model.fills[l];
            const std::vector<Toolpath>* kinds[7] = {&f.solid, &f.top, &f.bridge, &f.gaps, &f.sparse, &f.support,
                                                     &f.support_interface};
            for (std::size_t k = 0; k < 7; ++k)
                for (const Toolpath& t : *kinds[k])
                    if (!t.points.empty()) {
                        Island& i = island(t);
                        i.fills[k].push_back(t);
                        add(i, t);
                    }
            for (const Toolpath& t : f.arcs)
                if (!t.points.empty()) {
                    Island& i = island(t);
                    i.arcs.push_back(t);
                    add(i, t);
                }
            for (const Toolpath& t : f.ironing)
                if (!t.points.empty()) {
                    Island& i = island(t);
                    i.ironing.push_back(t);
                    add(i, t);
                }
        }
        const char* const fill_type[7] = {"Solid infill",    "Top solid infill", "Bridge infill",
                                          "Gap fill",        "Internal infill",  "Support material",
                                          "Support material interface"};
        std::vector<bool> done(islands.size(), false);
        // The boxes of the islands' path ends bound how near each island can be,
        // so most are not looked at: the same pick as looking at every end
        // (the nearest, and among equals the first island), without the
        // islands-squared cost.
        std::vector<std::array<double, 4>> end_box(islands.size());
        for (std::size_t k = 0; k < islands.size(); ++k) {
            auto& b = end_box[k];
            b = {std::numeric_limits<double>::max(), std::numeric_limits<double>::max(),
                 std::numeric_limits<double>::lowest(), std::numeric_limits<double>::lowest()};
            for (const Point2d& p : islands[k].ends)
                b[0] = std::min(b[0], p.x), b[1] = std::min(b[1], p.y), b[2] = std::max(b[2], p.x), b[3] = std::max(b[3], p.y);
        }
        const auto box_distance = [&](std::size_t k) {
            const auto& b = end_box[k];
            const double dx = std::max({b[0] - cursor.x, 0.0, cursor.x - b[2]});
            const double dy = std::max({b[1] - cursor.y, 0.0, cursor.y - b[3]});
            return std::hypot(dx, dy);
        };
        for (;;) {
            std::size_t next = islands.size();
            double nearest = std::numeric_limits<double>::max();
            const auto look = [&](std::size_t k) {
                for (const Point2d& p : islands[k].ends) {
                    const double d = std::hypot(p.x - cursor.x, p.y - cursor.y);
                    if (d < nearest || (d == nearest && k < next))
                        nearest = d, next = k;
                }
            };
            // The island whose box is nearest first, for a tight bound; then the
            // others whose box could hold something as near.
            std::size_t first_look = islands.size();
            double seed_distance = std::numeric_limits<double>::max();
            for (std::size_t k = 0; k < islands.size(); ++k)
                if (!done[k] && !islands[k].ends.empty() && box_distance(k) < seed_distance)
                    seed_distance = box_distance(k), first_look = k;
            if (first_look == islands.size())
                break;
            look(first_look);
            for (std::size_t k = 0; k < islands.size(); ++k)
                if (k != first_look && !done[k] && !islands[k].ends.empty() && box_distance(k) <= nearest)
                    look(k);
            if (next == islands.size())
                break;
            done[next] = true;
            const Island& i = islands[next];
            auto walls = [&](int index, const std::vector<Toolpath>& paths) {
                const bool outer = index == 0;
                const auto found = i.candidates.find(index);
                group(paths, outer ? "External perimeter" : "Perimeter",
                      speed(outer ? s.outer_wall_speed.of(s.wall_speed) : s.wall_speed), s.seam, true,
                      found == i.candidates.end() ? std::span<const std::vector<std::size_t>* const>{} : found->second);
            };
            if (s.zaa) {
                for (const auto& [index, paths] : i.walls)
                    deferring = deferring || off_top({&paths});
                deferring = deferring || off_top({&i.fills[1], &i.fills[3]});
            }
            // The arcs over an overhang first, in their order, from the centre
            // out: the walls round them then rest on their ends
            // (arc_overhangs.md, rule 4).
            for (Toolpath t : i.arcs)
                lay(std::move(t), "Overhang perimeter", speed(s.arcs_speed > 0.0 ? s.arcs_speed : bridge_feed));
            for (const auto& [index, paths] : i.walls)
                walls(index, paths);
            for (std::size_t k = 0; k < 7; ++k)
                group(i.fills[k], fill_type[k], fill_feed(k), SeamMode::Nearest, false, {}, k == 0 ? over_feed : 0.0);
            // Ironing, in the same layer after the island's fills (ironing.md, rule 1).
            group(i.ironing, "Ironing", speed(s.ironing_speed), SeamMode::Nearest);
            flush();
        }
        // The thick zone of a group that ends here: the whole group's height at
        // once, the nozzle at its top (docs/LOCAL_LAYERS.md).
        if (l < model.thick.size() && !model.thick[l].empty()) {
            const ThickLayer& t = model.thick[l];
            steps.push_back({{}, nullptr, 0.0, t.height, {}, {}});
            base_height = laid_height = t.height;
            in_thick = true;
            deferring = s.zaa && off_top({&t.walls, &t.fill.top, &t.fill.gaps});
            std::map<int, std::vector<Toolpath>, std::greater<int>> by_index;
            for (const Toolpath& w : t.walls)
                by_index[w.index].push_back(w);
            for (const auto& [index, paths] : by_index) {
                const bool outer = index == 0;
                group(paths, outer ? "External perimeter" : "Perimeter",
                      speed(outer ? s.outer_wall_speed.of(s.wall_speed) : s.wall_speed), s.seam, true);
            }
            const std::vector<Toolpath>* kinds[5] = {&t.fill.solid, &t.fill.top, &t.fill.bridge, &t.fill.gaps,
                                                     &t.fill.sparse};
            for (std::size_t k = 0; k < 5; ++k)
                group(*kinds[k], fill_type[k], fill_feed(k), SeamMode::Nearest);
            flush();
            steps.push_back({{}, nullptr, 0.0, layer.top - layer.bottom, {}, {}});
            base_height = laid_height = layer_height;
            in_thick = false;
        }
        return steps;
    };

    // The flow equalizer (speeds_accels.md, rules 8 and 9): where the flow
    // (mm3/s, the bead's section by its feed) drops from one extrusion to the
    // next, the end of the first is cut into pieces whose flow falls at
    // speed.flow_slope_down per second down to the next one's; where it rises,
    // the start of the second, at flow_slope_up, unless a retraction comes
    // between (the pressure is down already). Up to 16 pieces of about a
    // millimetre or more, each at the flow of its middle; a path too short for
    // its ramps has them squeezed in.
    const bool equalize = s.flow_slope_up > 0.0 || s.flow_slope_down > 0.0;
    const auto equalized = [&](const std::vector<Step>& steps, double layer_height) {
        std::vector<double> section(steps.size(), 0.0);
        std::vector<std::size_t> order; // the extrusions
        std::vector<char> retracts(steps.size(), 0);
        double h = layer_height;
        for (std::size_t k = 0; k < steps.size(); ++k) {
            const Step& st = steps[k];
            if (!st.type) {
                h = st.height;
                continue;
            }
            if (st.path.points.size() < 2)
                continue;
            double w = 0.0;
            for (const ToolpathPoint& p : st.path.points)
                w += p.width;
            section[k] = w / double(st.path.points.size()) * h;
            retracts[k] = st.path.retract || !st.from || st.travel.leaves_part || st.travel.length > s.retract_min_travel;
            order.push_back(k);
        }
        // Each extrusion's ramps: the flow it starts from, the one it ends at.
        std::vector<double> ramp_in(steps.size(), -1.0), ramp_out(steps.size(), -1.0);
        for (std::size_t i = 1; i < order.size(); ++i) {
            const std::size_t a = order[i - 1], b = order[i];
            const double qa = section[a] * steps[a].feed, qb = section[b] * steps[b].feed;
            if (qb < qa && s.flow_slope_down > 0.0)
                ramp_out[a] = qb;
            else if (qb > qa && s.flow_slope_up > 0.0 && !retracts[b])
                ramp_in[b] = qa;
        }
        std::vector<Step> result;
        result.reserve(steps.size());
        for (std::size_t k = 0; k < steps.size(); ++k) {
            const Step& st = steps[k];
            if (ramp_in[k] < 0.0 && ramp_out[k] < 0.0) {
                result.push_back(st);
                continue;
            }
            Toolpath path = st.path;
            if (path.closed && path.points.size() > 2) {
                path.points.push_back(path.points.front());
                path.closed = false;
            }
            double length = 0.0;
            for (std::size_t i = 1; i < path.points.size(); ++i)
                length += std::hypot(path.points[i].p.x - path.points[i - 1].p.x, path.points[i].p.y - path.points[i - 1].p.y);
            const double q = section[k] * st.feed;
            // A ramp's length: its mean speed for the time the flow takes to change.
            const auto ramp_length = [&](double q0, double q1, double slope) {
                return (q0 + q1) / 2.0 / section[k] * std::abs(q1 - q0) / slope;
            };
            double in = ramp_in[k] < 0.0 ? 0.0 : ramp_length(ramp_in[k], q, s.flow_slope_up);
            double out_length = ramp_out[k] < 0.0 ? 0.0 : ramp_length(q, ramp_out[k], s.flow_slope_down);
            if (in + out_length > length) {
                const double squeeze = length / (in + out_length);
                in *= squeeze, out_length *= squeeze;
            }
            bool first = true;
            double at = 0.0;
            const auto piece = [&](double to, double feed) {
                if (to - at <= 1e-6)
                    return;
                Step p;
                p.path = stretch(path, at, to);
                p.type = st.type;
                p.feed = feed;
                if (first) {
                    p.path.retract = st.path.retract;
                    p.from = st.from;
                    p.travel = st.travel;
                } else if (!p.path.points.empty()) {
                    p.from = p.path.points.front().p;
                }
                if (p.path.points.size() > 1)
                    result.push_back(std::move(p)), first = false;
                at = to;
            };
            const auto ramp = [&](double q0, double q1, double distance) {
                const int n = std::clamp(int(distance), 1, 16);
                double total = 0.0;
                for (int j = 0; j < n; ++j)
                    total += q0 + (q1 - q0) * (j + 0.5) / n;
                const double start = at;
                double done = 0.0;
                for (int j = 0; j < n; ++j) {
                    const double qj = q0 + (q1 - q0) * (j + 0.5) / n;
                    done += qj / total * distance; // each piece as long as its speed
                    piece(start + done, qj / section[k]);
                }
            };
            if (in > 0.0)
                ramp(ramp_in[k], q, in);
            piece(length - out_length, st.feed);
            if (out_length > 0.0)
                ramp(q, ramp_out[k], out_length);
        }
        return result;
    };

    // A layer's G-code from its plan, the paths already simplified.
    auto write_layer = [&](std::size_t l, const Active& a, const std::vector<Step>& planned) {
        std::vector<Step> ramped;
        if (equalize)
            ramped = equalized(planned, model.layers[l].top - model.layers[l].bottom);
        const std::vector<Step>& steps = equalize ? ramped : planned;
        const Layer& layer = model.layers[l];
        g.open_layer_fan();
        const bool turn = l < continues.size() && continues[l];
        // The layer's templates, from the second layer (gcode_templates.md,
        // rule 11, T8), with the layer's number and Z.
        std::string before, after;
        at_layer(l);
        if (l > 0) {
            before = render_template(s.gcode_before_layer, templates, "gcode.before_layer");
            after = render_template(s.gcode_after_layer, templates, "gcode.after_layer");
        }
        g.new_layer(layer.top - base, layer.top - layer.bottom, s.travel_speed, turn, before, after);
        if (s.gcode_progress)
            g.progress_mark();
        else
            g.progress(long(l * 100 / model.layers.size()));
        g.custom(a.lines, a.user);
        g.set_retraction(a.retract_length, a.retract_speed);
        g.set_flow(a.flow);
        g.set_forced_flow(a.volumetric_flow);
        const TravelPlanner& planner = *plans[plan_of[l]].planner;
        if (s.zaa && s.zaa_raise) {
            // Travels keep above the highest raised bead of the layer.
            double highest = 0.0;
            auto scan = [&](const std::vector<Toolpath>& paths) {
                for (const Toolpath& t : paths)
                    for (const ToolpathPoint& p : t.points)
                        highest = std::max(highest, p.dz);
            };
            if (l < model.walls.size())
                scan(model.walls[l].toolpaths);
            if (l < model.fills.size())
                scan(model.fills[l].top), scan(model.fills[l].gaps);
            if (l < model.thick.size())
                scan(model.thick[l].walls), scan(model.thick[l].fill.top), scan(model.thick[l].fill.gaps);
            g.set_clearance(highest);
        }
        if (l < model.fills.size()) {
            // Wavy infill: travels keep above its crests.
            double highest = 0.0;
            for (const Toolpath& t : model.fills[l].sparse)
                if (t.wavy)
                    for (const ToolpathPoint& p : t.points)
                        highest = std::max(highest, p.dz);
            if (highest > 0.0)
                g.set_clearance(std::max(g.clearance(), highest));
        }
        for (std::size_t k = 0; k < steps.size(); ++k) {
            const Step& step = steps[k];
            if (!step.type) {
                g.set_height(step.height);
                continue;
            }
            const Toolpath& t = step.path;
            if (s.gcode_progress && k > 0)
                g.progress_mark();
            // A vase's turn starts where the one below ended: no travel, unless
            // the user's G-code moved the head.
            if (!turn || g.lost())
                g.travel(t.points.front().p, s.travel_speed, planner, &step.travel, step.from ? &*step.from : nullptr,
                         t.retract, step.type);
            g.type(step.type);
            g.print_acceleration(acceleration_of(step.type, l == 0, model.raft_layers > 0 && l == model.raft_layers));
            g.nonplanar(t.wavy);
            if (s.gcode_arcs && !t.wavy && t.points.size() > kArcMinSegments) {
                // Arcs where the path follows a circle (gcode_extras.md, rule
                // 10), straight moves between them; a closed path back to its start.
                std::vector<ToolpathPoint> points = t.points;
                if (t.closed && points.size() > 2)
                    points.push_back(t.points.front());
                std::size_t at = 0;
                for (const ArcRun& run : fit_arcs(points, arc_tolerance)) {
                    for (; at < run.first; ++at)
                        g.extrude(points[at], points[at + 1], step.feed);
                    g.arc(points, run, step.feed);
                    at = run.last;
                }
                for (; at + 1 < points.size(); ++at)
                    g.extrude(points[at], points[at + 1], step.feed);
            } else {
                for (std::size_t i = 1; i < t.points.size(); ++i)
                    g.extrude(t.points[i - 1], t.points[i], step.feed);
                if (t.closed && t.points.size() > 2)
                    g.extrude(t.points.back(), t.points.front(), step.feed);
            }
            g.nonplanar(false);
            // After each kind, back to acceleration.default (speeds_accels.md,
            // the table): not between paths of the same kind.
            std::size_t next = k + 1;
            while (next < steps.size() && !steps[next].type)
                ++next;
            if (next == steps.size() || std::strcmp(steps[next].type, step.type) != 0)
                g.reset_acceleration(s.accel_default);
        }
    };

    // The height changes of every layer first: the plans take their speeds.
    std::vector<Active> actives;
    actives.reserve(model.layers.size());
    for (std::size_t l = 0; l < model.layers.size(); ++l) {
        // Changes below this layer's top take effect now: its top as ;Z: writes
        // it, so a reader of the G-code (the check) takes them at the same layer.
        active.lines.clear();
        active.user = false;
        const double written_top = std::strtod(fixed(model.layers[l].top - base, 3).c_str(), nullptr);
        while (next_change < s.changes.size() && s.changes[next_change].z < written_top - 1e-6) {
            const HeightChange& c = s.changes[next_change++];
            if (c.temperature)
                active.lines += std::string("M104 ") + dialect.temperature + std::to_string(std::lround(*c.temperature)) +
                                (dialect.tool_on_m104 ? " T0" : "") + " ; change at Z " + fixed(c.z, 3) + "\n";
            if (c.bed_temperature)
                active.lines +=
                    std::string("M140 ") + dialect.temperature + std::to_string(std::lround(*c.bed_temperature)) + "\n";
            if (c.pressure_advance && dialect.accelerations)
                active.lines += pressure_advance_gcode(s.flavor, *c.pressure_advance) + "\n";
            if (c.acceleration && dialect.accelerations)
                active.lines += acceleration_gcode(s.flavor, *c.acceleration, s.travel_acceleration) + "\n";
            if (c.volumetric_flow)
                active.volumetric_flow = *c.volumetric_flow;
            at_layer(l);
            if (!c.gcode.empty()) {
                active.lines += render_template(c.gcode, templates, "changes") + "\n";
                active.user = true;
            }
            // A colour change or a pause (gcode_templates.md, rule 13): the
            // template, after a mark the preview reads.
            if (c.color_change) {
                active.lines += ";COLOR_CHANGE\n" + render_template(s.gcode_color_change, templates, "gcode.color_change") + "\n";
                active.user = true;
            }
            if (c.pause) {
                active.lines += ";PAUSE_PRINT\n" + render_template(s.gcode_pause, templates, "gcode.pause") + "\n";
                active.user = true;
            }
            if (c.retract_length)
                active.retract_length = *c.retract_length;
            if (c.retract_speed)
                active.retract_speed = *c.retract_speed;
            if (c.flow)
                active.flow = *c.flow / 100.0;
            if (c.speed)
                active.speed = *c.speed / 100.0;
            if (c.fan)
                active.fan = *c.fan;
        }
        actives.push_back(active);
    }

    // Each layer is built in the emitter's buffer first. One faster than the
    // minimum layer time is built again from the same state with its
    // extrusions slowed down and the fan at its maximum
    // (docs/spec/BEHAVIOR.md, "Enfriamiento"). Then its time sets its fan.
    std::array<double, 2> first_layer = {0.0, 0.0}; // its time, normal and silent
    const auto write = [&](std::size_t l, const std::vector<Step>& steps) {
        const Active& a = actives[l];
        const Emitter emitter_before = g;
        g.reset_times();
        write_layer(l, a, steps);
        // A forced flow is never slowed down: the tower looks for the limit at that flow.
        const Slowdown slowdown = a.volumetric_flow > 0.0
                                      ? Slowdown{}
                                      : s.keep_outer_speed
                                      ? plan_slowdown(g.extruded(), g.outer_extruded(), g.other_time(), s.min_layer_time,
                                                      s.min_print_speed)
                                      : plan_slowdown(g.extruded(), g.other_time(), s.min_layer_time, s.min_print_speed);
        if (slowdown.stage != 0) {
            g = emitter_before;
            g.pace(slowdown);
            write_layer(l, a, steps);
            g.pace(Slowdown{});
            const double reached = layer_time(g.extruded(), g.outer_extruded(), g.other_time(), slowdown);
            if (warnings && reached < s.min_layer_time * (1.0 - 1e-9)) {
                char text[160];
                std::snprintf(text, sizeof text, "the layer takes %.1f s even at 1 mm/min, less than the minimum of %.1f s",
                              reached, s.min_layer_time);
                warnings->push_back({"layer_time", int(l), model.layers[l].top - base, text, reached});
            }
        }
        // The fan: a height change's, else by the layer's time; a slowed layer
        // at fan_max, even among the first layers, which otherwise print
        // without fan (owner's decision, 2026-10-24). Bridges get their own.
        const bool fan_off = l < std::size_t(std::max(s.fan_off_layers, 0)) && slowdown.stage == 0;
        const double fan = a.fan            ? *a.fan
                           : fan_off        ? 0.0
                           : slowdown.stage ? s.fan_max
                                            : layer_fan(layer_time(g.extruded(), g.other_time(), Slowdown{}),
                                                        s.min_layer_time, s.fan_below_time, s.fan_min, s.fan_max,
                                                        s.fan_always);
        g.settle_fan(fan, a.fan || fan_off ? fan : s.fan_bridges);
        const double seconds = g.end_layer();
        if (l == 0)
            first_layer = {seconds, g.last_silent_layer()};
        g.flush(out);
    };

    // The scarf seam (docs/spec/features/walls_extra.md, rule 4): which wall
    // loops start on a ramp. Not on the first layer, with nothing under the
    // ramp but the bed, nor on a loop already off the layer's top (Z
    // anti-aliasing) or laid over a wavy bead. A hole's loop runs with the
    // hole on its left (every loop runs counter-clockwise): a bead to its
    // left is out of the part.
    const auto scarfed = [&](std::size_t l, const Step& step) {
        if (s.scarf == ScarfMode::Nowhere || l == 0)
            return false;
        const Toolpath& t = step.path;
        const bool outer = std::strcmp(step.type, "External perimeter") == 0;
        if (!t.closed || t.wavy || t.points.size() < 3 || (!outer && (std::strcmp(step.type, "Perimeter") != 0 || !s.scarf_inner)))
            return false;
        for (const ToolpathPoint& p : t.points)
            if (p.dz != 0.0 || p.below != 0.0)
                return false;
        if (s.scarf == ScarfMode::Contours && l < model.regions.size()) {
            const Point2d& a = t.points[0].p;
            const Point2d& b = t.points[1].p;
            const double length = std::hypot(b.x - a.x, b.y - a.y);
            if (length > 0.0) {
                const double side = s.wall_width / length;
                const double x = (a.x + b.x) / 2.0 - (b.y - a.y) * side, y = (a.y + b.y) / 2.0 + (b.x - a.x) * side;
                if (!region_contains(model.regions[l], x, y))
                    return false;
            }
        }
        return !s.scarf_only_smooth || !has_sharp_corner(t);
    };
    // Where a path leaves the head: a closed one back at its start.
    const auto end_of = [](const Toolpath& t) {
        return t.closed && t.points.size() > 2 ? t.points.front().p : t.points.back().p;
    };
    // Planned in order, simplified and their travels planned in parallel, and
    // written in order: the stages overlap across layers, and a slowed layer
    // is written again from the same plan. Simplifying keeps a path's ends,
    // so the last plan's end is where the next layer starts from.
    std::vector<std::vector<Step>> planned(model.layers.size());
    std::size_t next_layer = 0;
    const std::size_t in_flight = 4 * std::max<std::size_t>(1, std::thread::hardware_concurrency());
    tbb::parallel_pipeline(
        in_flight,
        tbb::make_filter<void, std::size_t>(tbb::filter_mode::serial_in_order,
                                            [&](tbb::flow_control& control) -> std::size_t {
                                                if (next_layer == model.layers.size()) {
                                                    control.stop();
                                                    return 0;
                                                }
                                                return next_layer++;
                                            }) &
            tbb::make_filter<std::size_t, std::size_t>(tbb::filter_mode::parallel,
                                                       [&](std::size_t l) {
                                                           if (s.seam != SeamMode::Aligned || l >= model.walls.size())
                                                               return l;
                                                           const std::vector<Toolpath>& walls = model.walls[l].toolpaths;
                                                           loop_candidates[l].resize(walls.size());
                                                           for (std::size_t k = 0; k < walls.size(); ++k)
                                                               if (walls[k].closed && !walls[k].points.empty())
                                                                   loop_candidates[l][k] = seam_candidates(walls[k]);
                                                           return l;
                                                       }) &
            tbb::make_filter<std::size_t, std::size_t>(tbb::filter_mode::serial_in_order,
                                                       [&](std::size_t l) {
                                                           planned[l] = plan_layer(l, actives[l]);
                                                           std::vector<std::vector<std::size_t>>().swap(loop_candidates[l]);
                                                           if (s.vase) {
                                                               const bool turn = l >= spiral_from && spiral_turn(planned[l], l);
                                                               continues[l] = turn && after_turn;
                                                               after_turn = turn;
                                                           }
                                                           for (Step& step : planned[l])
                                                               if (step.type) {
                                                                   step.from = last_end;
                                                                   break;
                                                               }
                                                           for (const Step& step : planned[l])
                                                               if (step.type)
                                                                   last_end = end_of(step.path);
                                                           return l;
                                                       }) &
            tbb::make_filter<std::size_t, std::size_t>(
                tbb::filter_mode::parallel,
                [&](std::size_t l) {
                    const TravelPlanner& planner = *plans[plan_of[l]].planner;
                    std::optional<Point2d> from;
                    double height = model.layers[l].top - model.layers[l].bottom;
                    for (Step& step : planned[l]) {
                        if (!step.type) {
                            height = step.height;
                            continue;
                        }
                        step.path = simplify_toolpath(step.path, s.gcode_resolution);
                        if (scarfed(l, step))
                            step.path = scarf_loop(step.path, height, s.scarf_start_height / 100.0,
                                                   s.scarf_entire_loop ? std::numeric_limits<double>::infinity()
                                                                       : s.scarf_length,
                                                   s.scarf_max_segment);
                        if (from)
                            step.from = from;
                        if (step.from)
                            step.travel = planner.plan(*step.from, step.path.points.front().p);
                        from = end_of(step.path);
                    }
                    return l;
                }) &
            tbb::make_filter<std::size_t, void>(tbb::filter_mode::serial_in_order, [&](std::size_t l) {
                write(l, planned[l]);
                std::vector<Step>().swap(planned[l]);
            }));

    g.extruder_off();
    if (s.gcode_progress) {
        g.progress_mark(); // the end: 100 %
        g.end_layer();
    }
    g.type("Custom");
    g.flush(out);
    // The filament's end, then the printer's (rule 12), at the last layer.
    if (!model.layers.empty())
        at_layer(model.layers.size() - 1);
    if (const std::string text = render_template(s.filament_end_gcode, templates, "extrusion.end_gcode"); !text.empty())
        out << text << "\n";
    if (const std::string text = render_template(s.end_gcode, templates, "machine.end_gcode"); !text.empty())
        out << text << "\n";
    if (s.chamber_temperature > 0.0 && s.max_chamber_temp > 0.0)
        out << chamber_gcode(s.flavor, 0.0) << "\n";
    if (dialect.ends_m2)
        out << "M2\n";
    // docs/spec/GCODE_OUTPUT.md, "Tiempos" (the start and end G-code not
    // counted); the same by mode and for the first layer, as hosts read them
    // (gcode_firmware.md, rule 6; print_time.md, rule 7).
    out << "; estimated printing time = " << format_duration(g.print_time()) << "\n";
    out << "; estimated printing time (normal mode) = " << format_duration(g.print_time()) << "\n";
    if (s.silent_mode)
        out << "; estimated printing time (silent mode) = " << format_duration(g.silent_time()) << "\n";
    out << "; estimated first layer printing time (normal mode) = " << format_duration(first_layer[0]) << "\n";
    if (s.silent_mode)
        out << "; estimated first layer printing time (silent mode) = " << format_duration(first_layer[1]) << "\n";
    // The filament, its weight and cost (gcode_extras.md, rule 1).
    {
        const double r = s.filament_diameter / 2.0;
        const double mm = g.total_e(), cm3 = mm * std::numbers::pi * r * r / 1000.0;
        const double grams = cm3 * s.filament_density, cost = grams * s.filament_cost / 1000.0;
        out << "; filament used [mm] = " << fixed(mm, 2) << "\n";
        out << "; filament used [cm3] = " << fixed(cm3, 2) << "\n";
        if (s.filament_density > 0.0)
            out << "; filament used [g] = " << fixed(grams, 2) << "\n";
        out << "; filament cost = " << fixed(cost, 2) << "\n";
        out << "; total filament used [g] = " << fixed(grams, 2) << "\n";
        out << "; total filament cost = " << fixed(cost, 2) << "\n";
        out << "; total filament used for wipe tower [g] = 0.00\n";
    }
    // Each setting on one comment line: a line break in a value (a start
    // G-code variable, say) would otherwise end the comment and run the rest
    // as G-code after the print.
    const auto one_line = [](const std::string& text) {
        std::string line;
        line.reserve(text.size());
        for (const char c : text) {
            if (c == '\n')
                line += "\\n";
            else if (c == '\r')
                line += "\\r";
            else if (static_cast<unsigned char>(c) < 0x20 && c != '\t')
                line += ' ';
            else
                line += c;
        }
        return line;
    };
    for (const auto& [key, value] : describe(s))
        out << "; " << one_line(key) << " = " << one_line(value) << "\n";
    if (!s.gcode_progress)
        return;
    // The marks become M73 P<share done> R<minutes left>, and Q S for the
    // silent mode, where either changes (gcode_firmware.md, rule 5).
    const std::string text = std::move(held).str();
    const bool silent = s.silent_mode && dialect.silent_progress;
    const auto share = [](double t, double all) {
        return all > 0.0 ? std::min(100L, long(std::floor(100.0 * t / all + 1e-9))) : 100L;
    };
    const auto minutes = [](double t, double all) { return long(std::floor(std::max(0.0, all - t) / 60.0 + 1e-9)); };
    const std::string_view mark(kProgressMark);
    const char* note = s.verbose_comments ? " ; update progress\n" : "\n";
    long shown[4] = {-1, -1, -1, -1};
    std::size_t from = 0;
    for (std::size_t at = text.find(mark); at != std::string::npos; at = text.find(mark, from)) {
        target.write(text.data() + from, std::streamsize(at - from));
        const std::size_t end = text.find('\n', at);
        const std::size_t id = std::stoul(text.substr(at + mark.size(), end - at - mark.size()));
        const long now[4] = {share(g.time_at(id), g.print_time()), minutes(g.time_at(id), g.print_time()),
                             share(g.silent_time_at(id), g.silent_time()), minutes(g.silent_time_at(id), g.silent_time())};
        if (now[0] != shown[0] || now[1] != shown[1])
            target << "M73 P" << now[0] << " R" << now[1] << note;
        if (silent && (now[2] != shown[2] || now[3] != shown[3]))
            target << "M73 Q" << now[2] << " S" << now[3] << note;
        std::copy(now, now + 4, shown);
        from = end + 1;
    }
    target.write(text.data() + from, std::streamsize(text.size() - from));
}

} // namespace tisma
