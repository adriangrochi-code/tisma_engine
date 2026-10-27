// Arcs over overhangs held from one side (docs/spec/features/arc_overhangs.md).
// Own design; see PROVENANCE.md.

#include "infill/arcs.hpp"

#include "gcode/order.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace tisma {
namespace {

// A stretch of a zone's outline that lies on what holds it.
struct Run {
    std::vector<Point2d> points;
    bool closed = false; // the whole ring: held all round
};

std::vector<Run> held_runs(const Polygons& zone, const Polygons& near) {
    std::vector<Run> runs;
    for (const Polygon& ring : zone) {
        const std::size_t n = ring.size();
        if (n < 3)
            continue;
        const auto at = [&](std::size_t i) { return Point2d{to_mm(ring[i % n].x), to_mm(ring[i % n].y)}; };
        std::vector<char> held(n);
        for (std::size_t i = 0; i < n; ++i) {
            const Point2d a = at(i), b = at(i + 1);
            held[i] = region_contains(near, (a.x + b.x) / 2.0, (a.y + b.y) / 2.0);
        }
        const auto count = std::size_t(std::count(held.begin(), held.end(), 1));
        if (count == 0)
            continue;
        if (count == n) {
            Run all{{}, true};
            for (std::size_t i = 0; i <= n; ++i)
                all.points.push_back(at(i));
            runs.push_back(std::move(all));
            continue;
        }
        // From an edge that starts a run, round the ring.
        std::size_t start = 0;
        while (!(held[start] && !held[(start + n - 1) % n]))
            ++start;
        Run run;
        for (std::size_t k = 0; k < n; ++k) {
            const std::size_t i = (start + k) % n;
            if (held[i]) {
                if (run.points.empty())
                    run.points.push_back(at(i));
                run.points.push_back(at(i + 1));
            } else if (!run.points.empty()) {
                runs.push_back(std::move(run));
                run = {};
            }
        }
        if (!run.points.empty())
            runs.push_back(std::move(run));
    }
    return runs;
}

Polygon disc(const Point2d& c, double r) {
    Polygon out;
    const int sides = std::clamp(int(std::ceil(2.0 * std::numbers::pi * r / 0.2)), 32, 2048);
    for (int k = 0; k < sides; ++k) {
        const double a = 2.0 * std::numbers::pi * k / sides;
        out.push_back({to_units(c.x + r * std::cos(a)), to_units(c.y + r * std::sin(a))});
    }
    return out;
}

} // namespace

std::vector<HeldZone> held_zones(const Polygons& region, const Polygons& below, double w) {
    std::vector<HeldZone> held;
    if (region.empty() || below.empty())
        return held;
    Polygons hanging = Clipper2Lib::Difference(region, below, Clipper2Lib::FillRule::NonZero);
    hanging = offset(offset(hanging, -w / 2.0), w / 2.0); // no slivers
    if (hanging.empty())
        return held;
    const Polygons near = offset(below, 0.01);
    // The pieces held from one side; pieces whose held edges meet end to end
    // (an L round a corner of what holds them) are one zone, held along both.
    for (Polygons& zone : split_islands(hanging)) {
        std::vector<Run> runs = held_runs(zone, near);
        if (runs.size() != 1 || runs.front().closed)
            continue; // held from more than one side (a bridge), all round, or from nothing
        held.push_back({std::move(zone), std::move(runs.front().points)});
    }
    const auto same = [&](const Point2d& a, const Point2d& b) { return std::hypot(a.x - b.x, a.y - b.y) < 0.05; };
    for (bool joined = true; joined;) {
        joined = false;
        for (std::size_t i = 0; i < held.size() && !joined; ++i)
            for (std::size_t k = i + 1; k < held.size() && !joined; ++k) {
                std::vector<Point2d>& a = held[i].edge;
                std::vector<Point2d> b = held[k].edge;
                if (same(a.back(), b.back()) || same(a.front(), b.front()))
                    std::reverse(b.begin(), b.end());
                if (same(a.back(), b.front()))
                    a.insert(a.end(), b.begin() + 1, b.end());
                else if (same(b.back(), a.front()))
                    a.insert(a.begin(), b.begin(), b.end() - 1);
                else
                    continue;
                held[i].zone.insert(held[i].zone.end(), held[k].zone.begin(), held[k].zone.end());
                held.erase(held.begin() + std::ptrdiff_t(k));
                joined = true;
            }
    }
    return held;
}

std::vector<ArcZone> overhang_arcs(const Polygons& region, const Polygons& below, const ArcParams& params) {
    std::vector<ArcZone> out;
    const double w = params.width;
    const std::vector<HeldZone> held = held_zones(region, below, w);
    if (held.empty())
        return out;
    const Polygons inside = offset(region, -w / 2.0); // the beads within the outline
    const double spacing = w * (1.0 - std::clamp(params.overlap, 0.0, 90.0) / 100.0);
    constexpr double kStep = 0.1;          // mm between the samples of a circle
    constexpr std::size_t kBins = 1440;    // angles, for what holds each arc
    for (const HeldZone& h : held) {
        const Polygons& zone = h.zone;
        // The middle of the held edge, and from it into what holds the zone.
        const std::vector<Point2d>& edge = h.edge;
        double length = 0.0;
        for (std::size_t i = 1; i < edge.size(); ++i)
            length += std::hypot(edge[i].x - edge[i - 1].x, edge[i].y - edge[i - 1].y);
        if (!(length > w))
            continue;
        Point2d middle = edge.front(), along{1.0, 0.0};
        double walked = 0.0;
        const double half = length / 2.0;
        for (std::size_t i = 1; i < edge.size(); ++i) {
            const double piece = std::hypot(edge[i].x - edge[i - 1].x, edge[i].y - edge[i - 1].y);
            if (walked + piece >= half && piece > 0.0) {
                const double u = (half - walked) / piece;
                middle = {edge[i - 1].x + u * (edge[i].x - edge[i - 1].x), edge[i - 1].y + u * (edge[i].y - edge[i - 1].y)};
                along = {(edge[i].x - edge[i - 1].x) / piece, (edge[i].y - edge[i - 1].y) / piece};
                break;
            }
            walked += piece;
        }
        Point2d into{-along.y, along.x};
        if (!region_contains(below, middle.x + 0.05 * into.x, middle.y + 0.05 * into.y))
            into = {-into.x, -into.y};
        ArcZone z;
        z.centre = {middle.x + kArcCentre * into.x, middle.y + kArcCentre * into.y};
        // The zone and a bite of what holds it, as deep as the centre.
        z.area = Clipper2Lib::Union(zone, Clipper2Lib::Intersect(offset(zone, kArcCentre), below, Clipper2Lib::FillRule::NonZero),
                                    Clipper2Lib::FillRule::NonZero);
        z.area = Clipper2Lib::Intersect(z.area, inside, Clipper2Lib::FillRule::NonZero);
        if (params.max_distance > 0.0)
            z.area = Clipper2Lib::Intersect(z.area, Polygons{disc(z.centre, params.max_distance)},
                                            Clipper2Lib::FillRule::NonZero);
        if (z.area.empty())
            continue;
        double reach = 0.0;
        for (const Polygon& ring : z.area)
            for (const auto& q : ring)
                reach = std::max(reach, std::hypot(to_mm(q.x) - z.centre.x, to_mm(q.y) - z.centre.y));
        // The axis: from the centre across the held edge, into the zone.
        const double axis = std::atan2(-into.y, -into.x);
        const auto bin = [&](double a) {
            const double turn = a / (2.0 * std::numbers::pi);
            return std::size_t((turn - std::floor(turn)) * double(kBins)) % kBins;
        };
        std::vector<char> held_before(kBins, 0), held_now(kBins, 0);
        for (std::size_t j = 0;; ++j) {
            // Radii fitted to A1: 0.36 apart from 0.324.
            const double r = spacing * (0.9 + double(j));
            if (r > reach)
                break;
            // Every other one the other way, split on the axis; the whole ones
            // at 0.686, 1.407... as in A1.
            const bool back = j % 2 == 0;
            const double start = back ? axis : axis + std::numbers::pi;
            const std::size_t steps = std::max<std::size_t>(64, std::size_t(std::ceil(2.0 * std::numbers::pi * r / kStep)));
            const auto point = [&](double a) { return Point2d{z.centre.x + r * std::cos(a), z.centre.y + r * std::sin(a)}; };
            const auto in = [&](double a) {
                const Point2d p = point(a);
                return region_contains(z.area, p.x, p.y);
            };
            // Where the circle crosses the area's outline, to a few microns.
            const auto edge_between = [&](double out_a, double in_a) {
                for (int k = 0; k < 10; ++k) {
                    const double m = (out_a + in_a) / 2.0;
                    (in(m) ? in_a : out_a) = m;
                }
                return in_a;
            };
            std::vector<std::vector<double>> pieces; // each as angles
            std::vector<double> piece;
            bool was_in = false;
            double last = start;
            for (std::size_t k = 0; k <= steps; ++k) {
                const double a = start + 2.0 * std::numbers::pi * double(k) / double(steps);
                const bool now_in = in(a);
                if (now_in && !was_in && k > 0)
                    piece.push_back(edge_between(last, a));
                if (now_in)
                    piece.push_back(a);
                if (!now_in && was_in) {
                    piece.push_back(edge_between(a, last));
                    pieces.push_back(std::move(piece));
                    piece.clear();
                }
                was_in = now_in, last = a;
            }
            if (piece.size() > 1)
                pieces.push_back(std::move(piece));
            std::vector<Toolpath> ring;
            std::fill(held_now.begin(), held_now.end(), 0);
            for (const std::vector<double>& angles : pieces) {
                if (angles.size() < 2 || (angles.back() - angles.front()) * r < w)
                    continue; // shorter than a bead
                // It must lie on the arc before, or on what holds the zone.
                bool rests = j == 0;
                for (std::size_t k = 0; k < angles.size() && !rests; ++k) {
                    const Point2d p = point(angles[k]);
                    const std::size_t b = bin(angles[k]);
                    rests = held_before[b] || held_before[(b + 1) % kBins] || held_before[(b + kBins - 1) % kBins] ||
                            region_contains(below, p.x, p.y);
                }
                if (!rests)
                    continue;
                Toolpath t;
                for (const double a : angles)
                    t.points.push_back({point(a), w});
                // Every angle it covers holds the next arc.
                const auto bins = std::size_t(std::ceil((angles.back() - angles.front()) / (2.0 * std::numbers::pi) *
                                                        double(kBins)));
                for (std::size_t k = 0, b = bin(angles.front()); k <= std::min(bins, kBins); ++k)
                    held_now[(b + k) % kBins] = 1;
                ring.push_back(std::move(t));
            }
            if (back && !ring.empty()) {
                // The other way round, from where the arc before ended: each
                // piece reversed, the first (from the axis) first, then the
                // others from the last.
                for (Toolpath& t : ring)
                    std::reverse(t.points.begin(), t.points.end());
                std::reverse(ring.begin() + 1, ring.end());
            }
            for (Toolpath& t : ring)
                z.arcs.push_back(std::move(t));
            std::swap(held_before, held_now);
            if (std::none_of(held_before.begin(), held_before.end(), [](char c) { return c != 0; }) && j > 0)
                break; // nothing left to hang the next on
        }
        if (!z.arcs.empty())
            out.push_back(std::move(z));
    }
    return out;
}

} // namespace tisma
