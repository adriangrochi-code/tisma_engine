// Path order and seams. Own design from the behaviour in
// docs/spec/BEHAVIOR.md; see PROVENANCE.md.

#include "gcode/order.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <optional>

namespace tisma {
namespace {

double distance(const Point2d& a, const Point2d& b) { return std::hypot(a.x - b.x, a.y - b.y); }

// distance(a, b) >= d, the same answer without the square root unless the
// squares come within a hair of each other (far beyond their rounding).
bool at_least(const Point2d& a, const Point2d& b, double d) {
    const double dx = a.x - b.x, dy = a.y - b.y, squared = dx * dx + dy * dy, limit = d * d;
    if (squared > limit * (1.0 + 1e-9))
        return true;
    if (squared < limit * (1.0 - 1e-9))
        return false;
    return distance(a, b) >= d;
}

std::size_t rear_point(const Toolpath& t, const std::vector<std::size_t>& candidates) {
    std::size_t best = candidates.front();
    for (std::size_t i : candidates) {
        const Point2d& p = t.points[i].p;
        const Point2d& b = t.points[best].p;
        if (p.y > b.y || (p.y == b.y && p.x < b.x))
            best = i;
    }
    return best;
}

// A small deterministic mix (splitmix64 finaliser, a public-domain constant set).
std::uint64_t mix(std::uint64_t z) {
    z += 0x9e3779b97f4a7c15ull;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
}


// For each offered point of a loop, the first point at least kSeamSpan away
// from it going forward (after) and going backward (before) along the loop,
// or the last one before coming back to it if none is. A point nearer than
// kSeamSpan along the loop, by more than the sums' rounding, is nearer in a
// straight line too: those are passed over by pointers that only move ahead,
// so the points crowded at rounded corners cost a step each where looking at
// them one by one from every point cost dozens.
struct Away {
    std::vector<std::size_t> after, before;
};
Away away(const Toolpath& loop, const std::vector<char>& offered) {
    const std::size_t n = loop.points.size();
    Away out{std::vector<std::size_t>(n, 0), std::vector<std::size_t>(n, 0)};
    if (n == 0)
        return out;
    // Points by their index in the loop gone round twice.
    const auto point = [&](std::size_t m) -> const Point2d& { return loop.points[m < n ? m : m - n].p; };
    std::vector<double> along(2 * n, 0.0); // from point 0 forward to point m
    for (std::size_t m = 1; m < 2 * n; ++m) {
        const Point2d& a = point(m - 1);
        const Point2d& b = point(m);
        along[m] = along[m - 1] + std::sqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y));
    }
    const double near = kSeamSpan - (1e-6 + 8.0 * double(n) * along.back() * std::numeric_limits<double>::epsilon());
    // Going forward, points i + 1 to last are met; going backward, last down
    // to i + 1. e: the first near enough to look at forward; b: backward.
    std::size_t e = 0, b = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t last = i + n - 1;
        e = std::max(e, i + 1);
        while (e < last && along[e] - along[i] < near)
            ++e;
        b = std::max(b, i + 1);
        while (b < last && along[i + n] - along[b + 1] >= near)
            ++b;
        if (!offered[i])
            continue;
        const Point2d& p = loop.points[i].p;
        std::size_t j = e;
        while (j < last && !at_least(point(j), p, kSeamSpan))
            ++j;
        out.after[i] = j < n ? j : j - n;
        std::size_t k = along[i + n] - along[b] >= near ? b : i + 1;
        while (k > i + 1 && !at_least(point(k), p, kSeamSpan))
            --k;
        out.before[i] = k < n ? k : k - n;
    }
    return out;
}

// Below this many paths, looking at every one is as quick.
constexpr std::size_t kFiledPaths = 64;

// Where each path may start (a loop at its seam, an open path at either end),
// filed in a grid of square cells with about one end each, so that the
// nearest start is found without looking at every path: a 100 mm cube with
// lightning infill at 100 % has layers of thousands of short paths, and
// looking at every one took 20 of the 23 s to write it.
class EndGrid {
public:
    // With open_only, the loops are left out (their seams follow the cursor).
    EndGrid(const std::vector<Toolpath>& paths, const std::vector<std::size_t>& seams, bool open_only = false) {
        double xmin = std::numeric_limits<double>::max(), ymin = xmin;
        double xmax = std::numeric_limits<double>::lowest(), ymax = xmax;
        for (std::size_t i = 0; i < paths.size(); ++i) {
            const Toolpath& t = paths[i];
            if (t.points.empty())
                continue;
            if (t.closed) {
                if (!open_only)
                    ends_.push_back({t.points[seams[i]].p, i, false});
            } else {
                ends_.push_back({t.points.front().p, i, false});
                ends_.push_back({t.points.back().p, i, true});
            }
        }
        for (const End& e : ends_) {
            xmin = std::min(xmin, e.p.x), xmax = std::max(xmax, e.p.x);
            ymin = std::min(ymin, e.p.y), ymax = std::max(ymax, e.p.y);
        }
        if (ends_.empty())
            return;
        const double w = xmax - xmin, h = ymax - ymin;
        cell_ = std::max({std::sqrt(w * h / double(ends_.size())), std::max(w, h) / 1024.0, 1e-3});
        x0_ = xmin;
        y0_ = ymin;
        nx_ = int(w / cell_) + 1;
        ny_ = int(h / cell_) + 1;
        cells_.resize(std::size_t(nx_) * std::size_t(ny_));
        for (std::size_t e = 0; e < ends_.size(); ++e)
            cells_[std::size_t(row(ends_[e].p.y)) * std::size_t(nx_) + std::size_t(column(ends_[e].p.x))].push_back(e);
    }

    // Offers every end not yet done, ring by ring from the cursor's cell, to
    // consider(path, back, distance), until the nearest one so far is nearer
    // than the next ring can be: an end filed in ring r + 1 lies at least r
    // cells away (ties at exactly that are still offered).
    template <class Consider>
    void nearest(const Point2d& cursor, const std::vector<bool>& done, Consider consider, const double& found) {
        if (cells_.empty())
            return;
        const int ci = column(cursor.x), cj = row(cursor.y);
        for (int r = 0; r <= std::max(nx_, ny_); ++r) {
            for (int j = std::max(0, cj - r); j <= std::min(ny_ - 1, cj + r); ++j) {
                const int step = r == 0 || j == cj - r || j == cj + r ? 1 : 2 * r;
                for (int i = ci - r; i <= ci + r; i += step) {
                    if (i < 0 || i >= nx_)
                        continue;
                    std::vector<std::size_t>& cell = cells_[std::size_t(j) * std::size_t(nx_) + std::size_t(i)];
                    // Ends of paths already laid leave as they are met.
                    std::erase_if(cell, [&](std::size_t e) { return done[ends_[e].path]; });
                    for (std::size_t e : cell)
                        consider(ends_[e].path, ends_[e].back, distance(cursor, ends_[e].p));
                }
            }
            if (found < r * cell_)
                break;
        }
    }

private:
    struct End {
        Point2d p;
        std::size_t path;
        bool back;
    };
    int column(double x) const { return std::clamp(int(std::floor((x - x0_) / cell_)), 0, nx_ - 1); }
    int row(double y) const { return std::clamp(int(std::floor((y - y0_) / cell_)), 0, ny_ - 1); }

    std::vector<End> ends_;
    double x0_ = 0.0, y0_ = 0.0, cell_ = 1.0;
    int nx_ = 0, ny_ = 0;
    std::vector<std::vector<std::size_t>> cells_;
};

} // namespace

void SeamMemory::next_layer() {
    for (auto& [index, seams] : current) {
        below[index] = std::move(seams);
        grids_.erase(index);
    }
    current.clear();
}

double SeamMemory::nearest_below(int index, const Point2d& p) const {
    const auto it = below.find(index);
    if (it == below.end() || it->second.empty())
        return std::numeric_limits<double>::infinity();
    const std::vector<Point2d>& seams = it->second;
    double best = std::numeric_limits<double>::infinity();
    if (seams.size() <= 32) {
        for (const Point2d& s : seams)
            best = std::min(best, distance(p, s));
        return best;
    }
    auto [at, built] = grids_.try_emplace(index);
    Grid& g = at->second;
    if (built) {
        double xmin = seams.front().x, xmax = xmin, ymin = seams.front().y, ymax = ymin;
        for (const Point2d& s : seams)
            xmin = std::min(xmin, s.x), xmax = std::max(xmax, s.x), ymin = std::min(ymin, s.y), ymax = std::max(ymax, s.y);
        const double w = xmax - xmin, h = ymax - ymin;
        g.cell = std::max({std::sqrt(w * h / double(seams.size())), std::max(w, h) / 1024.0, 1e-3});
        g.x0 = xmin, g.y0 = ymin;
        g.nx = long(w / g.cell) + 1, g.ny = long(h / g.cell) + 1;
        g.cells.assign(std::size_t(g.nx) * std::size_t(g.ny), {});
        for (std::size_t k = 0; k < seams.size(); ++k) {
            const long cx = std::min(g.nx - 1, long((seams[k].x - g.x0) / g.cell));
            const long cy = std::min(g.ny - 1, long((seams[k].y - g.y0) / g.cell));
            g.cells[std::size_t(cy) * std::size_t(g.nx) + std::size_t(cx)].push_back(std::uint32_t(k));
        }
    }
    // Rings of cells around p's cell (p may lie outside the grid): every seam
    // in ring r + 1 or beyond is at least r cells away.
    const long cx = long(std::floor((p.x - g.x0) / g.cell)), cy = long(std::floor((p.y - g.y0) / g.cell));
    const long far = std::max({std::labs(cx), std::labs(cx - g.nx), std::labs(cy), std::labs(cy - g.ny)}) + 1;
    const auto visit = [&](long x, long y) {
        if (x < 0 || y < 0 || x >= g.nx || y >= g.ny)
            return;
        for (const std::uint32_t k : g.cells[std::size_t(y) * std::size_t(g.nx) + std::size_t(x)])
            best = std::min(best, distance(p, seams[k]));
    };
    for (long r = 0; r <= far; ++r) {
        for (long x = cx - r; x <= cx + r; ++x) {
            visit(x, cy - r);
            if (r > 0)
                visit(x, cy + r);
        }
        for (long y = cy - r + 1; y <= cy + r - 1; ++y) {
            visit(cx - r, y);
            if (r > 0)
                visit(cx + r, y);
        }
        if (best <= double(r) * g.cell)
            break;
    }
    return best;
}

namespace {

// The concave corners of a loop among some of its points: the turns against
// the loop's direction sharper than kSeamCorner, measured to the points at
// least kSeamSpan away on each side (zig-zags much smaller than a bead are
// not corners).
std::vector<std::size_t> concave_corners(const Toolpath& loop, const std::vector<std::size_t>& allowed) {
    const std::size_t n = loop.points.size();
    double area = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const Point2d& a = loop.points[i].p;
        const Point2d& b = loop.points[i + 1 < n ? i + 1 : 0].p;
        area += a.x * b.y - b.x * a.y;
    }
    const double limit = std::sin(kSeamCorner * std::numbers::pi / 180.0);
    std::vector<char> offered(n, 0);
    for (std::size_t i : allowed)
        offered[i] = 1;
    std::vector<std::size_t> corners;
    const Away reach = away(loop, offered);
    for (std::size_t i = 0; i < n; ++i) {
        if (!offered[i])
            continue;
        const Point2d& a = loop.points[reach.before[i]].p;
        const Point2d& b = loop.points[i].p;
        const Point2d& c = loop.points[reach.after[i]].p;
        const double ux = b.x - a.x, uy = b.y - a.y, vx = c.x - b.x, vy = c.y - b.y;
        const double cross = ux * vy - uy * vx;
        // Only a turn against the loop's direction can be a corner: the
        // others need no lengths.
        if (cross * (area > 0.0 ? -1.0 : 1.0) <= 0.0)
            continue;
        const double lu = std::hypot(ux, uy), lv = std::hypot(vx, vy);
        if (lu == 0.0 || lv == 0.0)
            continue;
        const double turn = cross / (lu * lv); // sine of the turn, + to the left
        if (turn * (area > 0.0 ? -1.0 : 1.0) > limit)
            corners.push_back(i); // turns against the loop's direction: a concave corner
    }
    return corners;
}

// The aligned seam among the candidates (never none): the one nearest to a
// seam of the same wall index below, the first of the nearest; the rear one
// if there is none below.
std::size_t aligned_pick(const Toolpath& loop, const std::vector<std::size_t>& candidates, const SeamMemory& memory) {
    const auto it = memory.below.find(loop.index);
    if (it == memory.below.end() || it->second.empty())
        return rear_point(loop, candidates);
    std::size_t best = candidates.front();
    double nearest = std::numeric_limits<double>::max();
    // With few seams below, the squares first: only the candidates within
    // a hair of the nearest square need the distances, which pick as
    // before (the first of the nearest).
    const std::vector<Point2d>& below = it->second;
    std::vector<double> squares;
    double least = std::numeric_limits<double>::infinity();
    if (below.size() <= 32) {
        squares.reserve(candidates.size());
        for (std::size_t i : candidates) {
            const Point2d& p = loop.points[i].p;
            double square = std::numeric_limits<double>::infinity();
            for (const Point2d& q : below) {
                const double dx = p.x - q.x, dy = p.y - q.y;
                square = std::min(square, dx * dx + dy * dy);
            }
            squares.push_back(square);
            least = std::min(least, square);
        }
    }
    for (std::size_t k = 0; k < candidates.size(); ++k) {
        if (!squares.empty() && squares[k] > least * (1.0 + 1e-9))
            continue;
        const std::size_t i = candidates[k];
        if (const double d = memory.nearest_below(loop.index, loop.points[i].p); d < nearest)
            nearest = d, best = i;
    }
    return best;
}

// seam_point among some of the loop's points only (never none).
std::size_t seam_among(const Toolpath& loop, const std::vector<std::size_t>& allowed, SeamMode mode,
                       const Point2d& from, std::uint64_t seed, const SeamMemory& memory) {
    switch (mode) {
    case SeamMode::Nearest: {
        std::size_t best = allowed.front();
        for (std::size_t i : allowed)
            if (distance(loop.points[i].p, from) < distance(loop.points[best].p, from))
                best = i;
        return best;
    }
    case SeamMode::Rear:
        return rear_point(loop, allowed);
    case SeamMode::Random:
        return allowed[static_cast<std::size_t>(mix(seed) % allowed.size())];
    case SeamMode::Aligned: {
        const std::vector<std::size_t> corners = concave_corners(loop, allowed);
        return aligned_pick(loop, corners.empty() ? allowed : corners, memory);
    }
    }
    return 0;
}

bool overlaps(const Clipper2Lib::Rect64& a, const Polygons* area) {
    if (!area || area->empty())
        return false;
    const Clipper2Lib::Rect64 b = Clipper2Lib::GetBounds(*area);
    return a.left <= b.right && b.left <= a.right && a.top <= b.bottom && b.top <= a.bottom;
}

// A loop the painted seams touch: its points with more every kSeamPaintStep
// between them, and the ones offered as its seam.
struct PaintedLoop {
    Toolpath dense;
    std::vector<std::size_t> origin; // the loop's point each dense point follows (or is)
    std::vector<char> own;           // whether it is one of the loop's own points
    std::vector<std::size_t> offered;
    std::optional<std::size_t> middle; // the middle of the longest enforced run
};

std::optional<PaintedLoop> painted_loop(const Toolpath& loop, const SeamPaint& paint) {
    const std::size_t n = loop.points.size();
    Clipper2Lib::Rect64 bounds(std::numeric_limits<std::int64_t>::max(), std::numeric_limits<std::int64_t>::max(),
                               std::numeric_limits<std::int64_t>::lowest(), std::numeric_limits<std::int64_t>::lowest());
    for (const ToolpathPoint& q : loop.points) {
        const std::int64_t x = to_units(q.p.x), y = to_units(q.p.y);
        bounds.left = std::min(bounds.left, x), bounds.right = std::max(bounds.right, x);
        bounds.top = std::min(bounds.top, y), bounds.bottom = std::max(bounds.bottom, y);
    }
    const bool enforce = overlaps(bounds, paint.enforce), block = overlaps(bounds, paint.block);
    if (!enforce && !block)
        return std::nullopt;
    PaintedLoop out;
    out.dense.closed = true;
    out.dense.index = loop.index;
    out.dense.wavy = loop.wavy;
    for (std::size_t i = 0; i < n; ++i) {
        const ToolpathPoint& a = loop.points[i];
        const ToolpathPoint& b = loop.points[(i + 1) % n];
        out.dense.points.push_back(a);
        out.origin.push_back(i);
        out.own.push_back(1);
        const int steps = int(distance(a.p, b.p) / kSeamPaintStep);
        for (int k = 1; k < steps; ++k) {
            const double t = double(k) / double(steps);
            const auto lerp = [t](double u, double v) { return u + t * (v - u); };
            out.dense.points.push_back({{lerp(a.p.x, b.p.x), lerp(a.p.y, b.p.y)}, lerp(a.width, b.width),
                                        lerp(a.dz, b.dz), lerp(a.below, b.below)});
            out.origin.push_back(i);
            out.own.push_back(0);
        }
    }
    std::vector<std::size_t> in, clear;
    bool blocked = false;
    for (std::size_t i = 0; i < out.dense.points.size(); ++i) {
        const Point2d& p = out.dense.points[i].p;
        if (enforce && region_contains(*paint.enforce, p.x, p.y))
            in.push_back(i);
        else if (block && region_contains(*paint.block, p.x, p.y))
            blocked = true;
        else
            clear.push_back(i);
    }
    if (!in.empty()) {
        // The runs of enforced points along the loop; the middle of the
        // longest one by length, where a painted line wants its seam.
        const std::size_t m = out.dense.points.size();
        std::vector<char> enforced(m, 0);
        for (std::size_t i : in)
            enforced[i] = 1;
        std::size_t start = 0; // a run's start: an enforced point after one that is not
        while (start < m && enforced[(start + m - 1) % m])
            ++start;
        if (start == m) { // the whole loop: its rear point stands
            out.offered = std::move(in);
            return out;
        }
        double longest = -1.0;
        for (std::size_t k = 0; k < m; ++k) {
            const std::size_t first = (start + k) % m;
            if (!enforced[first] || enforced[(first + m - 1) % m])
                continue;
            std::vector<std::size_t> run{first};
            double length = 0.0;
            for (std::size_t j = (first + 1) % m; enforced[j] && j != first; j = (j + 1) % m) {
                length += distance(out.dense.points[run.back()].p, out.dense.points[j].p);
                run.push_back(j);
            }
            if (length > longest) {
                longest = length;
                double walked = 0.0;
                out.middle = run.front();
                for (std::size_t r = 1; r < run.size() && walked < 0.5 * length; ++r) {
                    walked += distance(out.dense.points[run[r - 1]].p, out.dense.points[run[r]].p);
                    out.middle = run[r];
                }
            }
        }
        out.offered = std::move(in);
    } else if (blocked && !clear.empty())
        out.offered = std::move(clear);
    else
        return std::nullopt;
    return out;
}

// The seam picked on the dense loop as an index of the loop itself, adding the
// point when it lies between two of the loop's own.
std::size_t settle(Toolpath& loop, const PaintedLoop& painted, std::size_t seam) {
    if (painted.own[seam])
        return painted.origin[seam];
    const std::size_t at = painted.origin[seam] + 1;
    loop.points.insert(loop.points.begin() + std::ptrdiff_t(at), painted.dense.points[seam]);
    return at;
}

} // namespace

std::size_t seam_point(const Toolpath& loop, SeamMode mode, const Point2d& from, std::uint64_t seed,
                       const SeamMemory& memory) {
    const std::size_t n = loop.points.size();
    if (n == 0)
        return 0;
    std::vector<std::size_t> all(n);
    for (std::size_t i = 0; i < n; ++i)
        all[i] = i;
    return seam_among(loop, all, mode, from, seed, memory);
}

std::vector<std::size_t> seam_candidates(const Toolpath& loop) {
    const std::size_t n = loop.points.size();
    std::vector<std::size_t> all(n);
    for (std::size_t i = 0; i < n; ++i)
        all[i] = i;
    std::vector<std::size_t> corners = concave_corners(loop, all);
    return corners.empty() ? all : corners;
}

bool has_sharp_corner(const Toolpath& loop) {
    const std::size_t n = loop.points.size();
    if (n < 3)
        return false;
    const double limit = std::sin(kSeamCorner * std::numbers::pi / 180.0);
    const Away reach = away(loop, std::vector<char>(n, 1));
    for (std::size_t i = 0; i < n; ++i) {
        const Point2d& a = loop.points[reach.before[i]].p;
        const Point2d& b = loop.points[i].p;
        const Point2d& c = loop.points[reach.after[i]].p;
        const double ux = b.x - a.x, uy = b.y - a.y, vx = c.x - b.x, vy = c.y - b.y;
        const double lu = std::hypot(ux, uy), lv = std::hypot(vx, vy);
        if (lu == 0.0 || lv == 0.0)
            continue;
        // Sharper than kSeamCorner: the sine past the limit, or turned back
        // (the cosine below zero).
        if (std::abs(ux * vy - uy * vx) / (lu * lv) > limit || ux * vx + uy * vy < 0.0)
            return true;
    }
    return false;
}

std::vector<Toolpath> order_paths(std::vector<Toolpath> paths, Point2d& cursor, SeamMode mode, std::uint64_t seed,
                                  SeamMemory& memory, const SeamPaint* paint,
                                  std::span<const std::vector<std::size_t>* const> candidates) {
    std::vector<Toolpath> out;
    out.reserve(paths.size());
    std::vector<bool> done(paths.size(), false);
    std::vector<std::size_t> seams(paths.size(), 0);
    // The loops painted seams touch pick their seam on their dense copy.
    std::vector<std::optional<PaintedLoop>> painted(paths.size());
    if (paint && !paint->empty())
        for (std::size_t i = 0; i < paths.size(); ++i)
            if (paths[i].closed && paths[i].points.size() > 2)
                painted[i] = painted_loop(paths[i], *paint);
    // An enforced seam goes to the middle of its painted run in the aligned
    // and rear modes; nearest and random choose among the painted points.
    const auto seam_of = [&](std::size_t i) {
        if (!painted[i] && mode == SeamMode::Aligned && i < candidates.size() && candidates[i] &&
            !paths[i].points.empty())
            return aligned_pick(paths[i], *candidates[i], memory);
        if (!painted[i])
            return seam_point(paths[i], mode, cursor, seed + i, memory);
        if (painted[i]->middle && (mode == SeamMode::Aligned || mode == SeamMode::Rear))
            return *painted[i]->middle;
        return seam_among(painted[i]->dense, painted[i]->offered, mode, cursor, seed + i, memory);
    };
    // A loop's seam does not depend on the cursor except in nearest mode.
    for (std::size_t i = 0; i < paths.size(); ++i)
        if (paths[i].closed && mode != SeamMode::Nearest) {
            seams[i] = seam_of(i);
            if (painted[i]) {
                seams[i] = settle(paths[i], *painted[i], seams[i]);
                painted[i].reset();
            }
        }
    // The ends a path may start at stay put unless a loop's seam follows the
    // cursor; then every path is looked at each time, else the ends are
    // filed in a grid. Both pick the same: the nearest start, and between
    // equally near ones the first path, its front before its back.
    const bool seams_move = mode == SeamMode::Nearest &&
                            std::any_of(paths.begin(), paths.end(), [](const Toolpath& t) { return t.closed; });
    std::optional<EndGrid> grid;
    const std::size_t open = std::size_t(std::count_if(paths.begin(), paths.end(), [](const Toolpath& t) { return !t.closed; }));
    if ((seams_move ? open : paths.size()) > kFiledPaths)
        grid.emplace(paths, seams, seams_move);
    // With seams that follow the cursor, a loop's nearest point is no nearer
    // than its box: the loops are looked at nearest box first, and those
    // whose box lies farther than the best start so far cannot win nor tie.
    // The same pick as looking at every one, without finding the nearest
    // point of each loop at every step.
    std::vector<std::array<double, 4>> boxes;
    std::vector<std::pair<double, std::size_t>> by_box;
    if (seams_move) {
        boxes.resize(paths.size());
        for (std::size_t i = 0; i < paths.size(); ++i) {
            auto& b = boxes[i];
            b = {std::numeric_limits<double>::max(), std::numeric_limits<double>::max(),
                 std::numeric_limits<double>::lowest(), std::numeric_limits<double>::lowest()};
            if (paths[i].closed)
                for (const ToolpathPoint& q : (painted[i] ? painted[i]->dense : paths[i]).points)
                    b = {std::min(b[0], q.p.x), std::min(b[1], q.p.y), std::max(b[2], q.p.x), std::max(b[3], q.p.y)};
        }
    }
    const auto box_distance = [&](std::size_t i) {
        const auto& b = boxes[i];
        return std::hypot(std::max({b[0] - cursor.x, 0.0, cursor.x - b[2]}), std::max({b[1] - cursor.y, 0.0, cursor.y - b[3]}));
    };
    for (std::size_t k = 0; k < paths.size(); ++k) {
        std::size_t best = paths.size();
        bool reverse = false;
        double nearest = std::numeric_limits<double>::max();
        const auto consider = [&](std::size_t i, bool back, double d) {
            if (d < nearest || (d == nearest && (i < best || (i == best && reverse && !back))))
                nearest = d, best = i, reverse = back;
        };
        if (grid && !seams_move) {
            grid->nearest(cursor, done, consider, nearest);
        } else if (seams_move) {
            // The open paths' ends (from the grid when there are many), then the loops.
            if (grid)
                grid->nearest(cursor, done, consider, nearest);
            by_box.clear();
            for (std::size_t i = 0; i < paths.size(); ++i) {
                if (done[i] || paths[i].points.empty())
                    continue;
                const Toolpath& t = paths[i];
                if (t.closed) {
                    by_box.push_back({box_distance(i), i});
                } else if (!grid) {
                    consider(i, false, distance(cursor, t.points.front().p));
                    consider(i, true, distance(cursor, t.points.back().p));
                }
            }
            // A heap: only the few nearest boxes come out.
            const auto farther = [](const std::pair<double, std::size_t>& a, const std::pair<double, std::size_t>& b) {
                return a > b;
            };
            std::make_heap(by_box.begin(), by_box.end(), farther);
            while (!by_box.empty() && by_box.front().first <= nearest) {
                const std::size_t i = by_box.front().second;
                std::pop_heap(by_box.begin(), by_box.end(), farther);
                by_box.pop_back();
                seams[i] = seam_of(i);
                consider(i, false, distance(cursor, (painted[i] ? painted[i]->dense : paths[i]).points[seams[i]].p));
            }
        } else {
            for (std::size_t i = 0; i < paths.size(); ++i) {
                if (done[i] || paths[i].points.empty())
                    continue;
                const Toolpath& t = paths[i];
                if (t.closed)
                    consider(i, false, distance(cursor, (painted[i] ? painted[i]->dense : t).points[seams[i]].p));
                else {
                    consider(i, false, distance(cursor, t.points.front().p));
                    consider(i, true, distance(cursor, t.points.back().p));
                }
            }
        }
        if (best == paths.size())
            break;
        done[best] = true;
        Toolpath t = std::move(paths[best]);
        if (t.closed && painted[best])
            seams[best] = settle(t, *painted[best], seams[best]);
        if (t.closed) {
            std::rotate(t.points.begin(), t.points.begin() + std::ptrdiff_t(seams[best]), t.points.end());
            memory.current[t.index].push_back(t.points.front().p);
            cursor = t.points.front().p; // a loop ends where it starts
        } else {
            if (reverse)
                std::reverse(t.points.begin(), t.points.end());
            cursor = t.points.back().p;
        }
        out.push_back(std::move(t));
    }
    return out;
}

std::vector<Polygons> split_islands(const Polygons& region) {
    Clipper2Lib::Clipper64 clipper;
    clipper.AddSubject(region);
    Clipper2Lib::PolyTree64 tree;
    clipper.Execute(Clipper2Lib::ClipType::Union, Clipper2Lib::FillRule::NonZero, tree);
    std::vector<Polygons> islands;
    std::vector<const Clipper2Lib::PolyPath64*> outers;
    for (const auto& outer : tree)
        outers.push_back(outer.get());
    for (std::size_t k = 0; k < outers.size(); ++k) {
        Polygons island{outers[k]->Polygon()};
        for (const auto& hole : *outers[k]) {
            island.push_back(hole->Polygon());
            for (const auto& inner : *hole)
                outers.push_back(inner.get());
        }
        islands.push_back(std::move(island));
    }
    return islands;
}

int island_of(const std::vector<Polygons>& islands, const Point2d& p) {
    const Clipper2Lib::Point64 q{to_units(p.x), to_units(p.y)};
    for (std::size_t k = 0; k < islands.size(); ++k) {
        const Polygons& island = islands[k];
        if (island.empty() || Clipper2Lib::PointInPolygon(q, island.front()) == Clipper2Lib::PointInPolygonResult::IsOutside)
            continue;
        bool in_hole = false;
        for (std::size_t h = 1; h < island.size() && !in_hole; ++h)
            in_hole = Clipper2Lib::PointInPolygon(q, island[h]) == Clipper2Lib::PointInPolygonResult::IsInside;
        if (!in_hole)
            return int(k);
    }
    return -1;
}

} // namespace tisma
