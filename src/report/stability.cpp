// The stability review (docs/spec/features/stability.md). Own design on the
// statics and beam theory of any text on the mechanics of materials; see
// PROVENANCE.md.

#include "report/stability.hpp"

#include "config/settings.hpp"
#include "gcode/order.hpp"
#include "slice/pipeline.hpp"

#include <oneapi/tbb/parallel_for.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <numbers>

namespace tisma {
namespace {

// Area and first and second moments of a region about the origin (Green's
// theorem over its rings; holes run the other way and subtract).
struct Moments {
    double a = 0.0, sx = 0.0, sy = 0.0, ixx = 0.0, iyy = 0.0, ixy = 0.0;
    Moments& operator+=(const Moments& o) {
        a += o.a, sx += o.sx, sy += o.sy, ixx += o.ixx, iyy += o.iyy, ixy += o.ixy;
        return *this;
    }
    double cx() const { return sx / a; }
    double cy() const { return sy / a; }
};

Moments moments(const Polygons& region) {
    Moments m;
    for (const Polygon& ring : region)
        for (std::size_t i = 0; i < ring.size(); ++i) {
            const double x0 = to_mm(ring[i].x), y0 = to_mm(ring[i].y);
            const double x1 = to_mm(ring[(i + 1) % ring.size()].x), y1 = to_mm(ring[(i + 1) % ring.size()].y);
            const double c = x0 * y1 - x1 * y0;
            m.a += c / 2.0;
            m.sx += (x0 + x1) * c / 6.0;
            m.sy += (y0 + y1) * c / 6.0;
            m.iyy += (x0 * x0 + x0 * x1 + x1 * x1) * c / 12.0;
            m.ixx += (y0 * y0 + y0 * y1 + y1 * y1) * c / 12.0;
            m.ixy += (x0 * y1 + 2.0 * x0 * y0 + 2.0 * x1 * y1 + x1 * y0) * c / 24.0;
        }
    return m;
}

// About the centroid: Ixx, Iyy, Ixy.
std::array<double, 3> centroidal(const Moments& m) {
    return {m.ixx - m.a * m.cy() * m.cy(), m.iyy - m.a * m.cx() * m.cx(), m.ixy - m.a * m.cx() * m.cy()};
}

// The smaller principal second moment about the centroid (mm4).
double min_inertia(const Moments& m) {
    if (!(m.a > 0.0))
        return 0.0;
    const auto [ixx, iyy, ixy] = centroidal(m);
    return std::max(0.0, (ixx + iyy) / 2.0 - std::hypot((ixx - iyy) / 2.0, ixy));
}

// The smaller section modulus of a region about its principal axes (mm3): the
// second moment about each over the farthest point from it.
double min_modulus(const Polygons& region) {
    const Moments m = moments(region);
    if (!(m.a > 0.0))
        return 0.0;
    const auto [ixx, iyy, ixy] = centroidal(m);
    const double theta = 0.5 * std::atan2(-2.0 * ixy, ixx - iyy);
    double best = std::numeric_limits<double>::infinity();
    for (const double t : {theta, theta + std::numbers::pi / 2.0}) {
        const double s = std::sin(t), c = std::cos(t);
        const double i = ixx * c * c + iyy * s * s - 2.0 * ixy * s * c; // about the axis along (c, s)
        double far = 0.0;
        for (const Polygon& ring : region)
            for (const auto& q : ring)
                far = std::max(far, std::fabs(-(to_mm(q.x) - m.cx()) * s + (to_mm(q.y) - m.cy()) * c));
        if (far > 0.0)
            best = std::min(best, i / far);
    }
    return std::isfinite(best) ? best : 0.0;
}

} // namespace

std::vector<Warning> stability_warnings(const SlicedModel& model, const Settings& s) {
    std::vector<Warning> out;
    // Object by object (sequential.md): each stands on the bed on its own.
    if (model.sequential && !model.objects.empty()) {
        for (std::size_t k = 0; k < model.objects.size(); ++k)
            for (Warning w : stability_warnings(object_part(model, k), s)) {
                if (w.layer >= 0)
                    w.layer += int(model.objects[k].first_layer);
                out.push_back(std::move(w));
            }
        return out;
    }
    const std::size_t n = std::min(model.layers.size(), model.regions.size());
    if (s.support || n == 0) // print_review.md, rule 6: only parts printed without supports
        return out;
    const double bed = model.layers.front().bottom;
    const auto top = [&](std::size_t l) { return model.layers[l].top - bed; };
    char text[256];

    // Rule 1: the islands of every layer and the ones under each.
    std::vector<std::vector<Polygons>> islands(n);
    std::vector<std::vector<Moments>> moment(n);
    std::vector<std::vector<std::vector<std::size_t>>> under(n);
    std::vector<std::vector<double>> resting(n); // mm2 of each island over the layer below
    tbb::parallel_for(std::size_t(0), n, [&](std::size_t l) {
        islands[l] = split_islands(model.regions[l]);
        for (const Polygons& island : islands[l])
            moment[l].push_back(moments(island));
    });
    tbb::parallel_for(std::size_t(1), n, [&](std::size_t l) {
        under[l].resize(islands[l].size());
        resting[l].assign(islands[l].size(), 0.0);
        for (std::size_t i = 0; i < islands[l].size(); ++i) {
            const Clipper2Lib::Rect64 a = Clipper2Lib::GetBounds(islands[l][i]);
            for (std::size_t j = 0; j < islands[l - 1].size(); ++j) {
                const Clipper2Lib::Rect64 b = Clipper2Lib::GetBounds(islands[l - 1][j]);
                if (a.right < b.left || b.right < a.left || a.bottom < b.top || b.bottom < a.top)
                    continue;
                const double shared =
                    area_mm2(Clipper2Lib::Intersect(islands[l][i], islands[l - 1][j], Clipper2Lib::FillRule::NonZero));
                if (shared > 0.0) {
                    under[l][i].push_back(j);
                    resting[l][i] += shared;
                }
            }
        }
    });

    // Rule 2: parts that start in the air, as one warning: how many, and
    // from which layer to which (print_review.md: the places and the layers).
    const double bead = s.wall_width;
    std::size_t in_air = 0, first_air = n, last_air = 0;
    for (std::size_t l = 1; l < n; ++l)
        for (std::size_t i = 0; i < islands[l].size(); ++i)
            // (an island smaller than that, resting on what is under it with
            // half its area or more, stands)
            if (resting[l][i] < bead * bead && resting[l][i] < 0.5 * moment[l][i].a) {
                ++in_air;
                first_air = std::min(first_air, l);
                last_air = std::max(last_air, l);
            }
    if (in_air > 0) {
        if (in_air == 1)
            std::snprintf(text, sizeof text, "a part starts in the air at Z %.2f: it will fall; add supports or turn the part",
                          top(first_air));
        else
            std::snprintf(text, sizeof text,
                          "%zu parts start in the air, from Z %.2f to Z %.2f: they will fall; add supports or turn the "
                          "part",
                          in_air, top(first_air), top(last_air));
        out.push_back({"stability_air", int(first_air), top(first_air), text, double(in_air), "critical"});
    }

    // Rules 3 and 5: the footing, what touches the bed (and the brim).
    Polygons footing = model.regions.front();
    if (s.brim_width > 0.0)
        footing = offset(footing, s.brim_width);
    Polygon points;
    for (const Polygon& ring : footing)
        points.insert(points.end(), ring.begin(), ring.end());
    const Polygons hull = {convex_hull(std::move(points))};
    const Moments foot = moments(footing);
    const double foot_modulus = min_modulus(footing);
    const double force = s.review_nozzle_force, adhesion = s.review_bed_adhesion;
    Moments printed; // everything printed so far, times the layer's thickness
    bool tipping = false, tipping_critical = false, peeling = false;
    for (std::size_t l = 0; l < n && foot.a > 0.0; ++l) {
        const double h = model.layers[l].top - model.layers[l].bottom;
        for (const Moments& m : moment[l]) {
            Moments v = m;
            v.a *= h, v.sx *= h, v.sy *= h;
            printed += v;
        }
        if (!(printed.a > 0.0))
            continue;
        const double x = printed.cx(), y = printed.cy();
        const double d = std::hypot(x - foot.cx(), y - foot.cy());
        const double weight = printed.a * kStabilityDensity / 1000.0 * 9.81; // N
        if (!tipping && !region_contains(hull, x, y)) {
            tipping = true;
            std::snprintf(text, sizeof text,
                          "from Z %.2f the centre of mass (%.2f, %.2f) is off the footing: the part stands by its hold "
                          "on the bed alone; add a brim or supports, or turn it",
                          top(l), x, y);
            out.push_back({"stability_overturn", int(l), top(l), text, d, "warning"});
        }
        if (tipping && adhesion > 0.0 && !tipping_critical && weight * d > adhesion * foot_modulus) {
            tipping_critical = true;
            std::snprintf(text, sizeof text, "from Z %.2f its weight (%.1f N mm) tears it off the bed (it holds %.1f N mm)",
                          top(l), weight * d, adhesion * foot_modulus);
            out.push_back({"stability_overturn", int(l), top(l), text, weight * d, "critical"});
        }
        // Rule 5: the nozzle's push at this height, and the weight.
        if (force > 0.0 && adhesion > 0.0 && !peeling && force * top(l) + weight * d > adhesion * foot_modulus) {
            peeling = true;
            std::snprintf(text, sizeof text,
                          "from Z %.2f the nozzle's push (%.1f N mm at the footing) may peel the part off the bed (it "
                          "holds %.1f N mm); add a brim",
                          top(l), force * top(l) + weight * d, adhesion * foot_modulus);
            out.push_back({"stability_peel", int(l), top(l), text, force * top(l) + weight * d, "warning"});
        }
    }

    // Rule 4: columns that bend under the nozzle. Down each island's chain,
    // the least stiffness seen so far, to the first layer ten times stiffer
    // (its base) or the bed.
    std::vector<std::vector<double>> flexibility(n); // mm/N at each island's top
    tbb::parallel_for(std::size_t(0), n, [&](std::size_t l) {
        flexibility[l].assign(islands[l].size(), 0.0);
        std::vector<std::size_t> chain, next;
        std::vector<char> seen;
        for (std::size_t i = 0; i < islands[l].size(); ++i) {
            chain.assign(1, i);
            double least = std::numeric_limits<double>::infinity();
            double base = 0.0;
            for (std::size_t k = l + 1; k-- > 0;) {
                Moments section;
                for (const std::size_t j : chain)
                    section += moment[k][j];
                const double stiffness = min_inertia(section);
                if (k < l && stiffness >= 10.0 * least) {
                    base = top(k);
                    break;
                }
                least = std::min(least, stiffness);
                if (k == 0)
                    break;
                next.clear();
                seen.assign(islands[k - 1].size(), 0);
                for (const std::size_t j : chain)
                    for (const std::size_t below : under[k][j])
                        if (!seen[below])
                            seen[below] = 1, next.push_back(below);
                if (next.empty())
                    break; // in the air: rule 2's
                chain.swap(next);
            }
            const double length = top(l) - base;
            if (least > 0.0 && std::isfinite(least))
                flexibility[l][i] = length * length * length / (3.0 * kStabilityModulusZ * least);
        }
    });
    if (force > 0.0) {
        bool bending = false;
        for (std::size_t l = 0; l < n; ++l) {
            double worst = 0.0;
            for (const double f : flexibility[l])
                worst = std::max(worst, f * force);
            if (worst > s.review_max_deflection && !bending) {
                std::snprintf(text, sizeof text,
                              "from Z %.2f a slender part bends %.3f mm under the nozzle's push: print it lower, add a "
                              "rib or print it next to something that holds it",
                              top(l), worst);
                out.push_back({"stability_bending", int(l), top(l), text, worst, "warning"});
            }
            bending = worst > s.review_max_deflection;
        }
    } else {
        // The three most flexible columns, at their tops (an island with
        // nothing over it), as information; not those that bend less than a
        // tenth of review.max_deflection per newton (a cube: nothing to say).
        std::vector<std::vector<char>> covered(n);
        for (std::size_t l = 0; l < n; ++l)
            covered[l].assign(islands[l].size(), 0);
        for (std::size_t l = 1; l < n; ++l)
            for (const auto& below : under[l])
                for (const std::size_t j : below)
                    covered[l - 1][j] = 1;
        std::vector<std::pair<double, std::size_t>> tops;
        for (std::size_t l = 0; l < n; ++l)
            for (std::size_t i = 0; i < islands[l].size(); ++i)
                if (!covered[l][i] && flexibility[l][i] >= s.review_max_deflection / 10.0)
                    tops.push_back({flexibility[l][i], l});
        std::sort(tops.begin(), tops.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
        for (std::size_t k = 0; k < std::min<std::size_t>(3, tops.size()); ++k) {
            const auto [f, l] = tops[k];
            std::snprintf(text, sizeof text,
                          "a column topping at Z %.2f gives %.3f mm per newton of the nozzle's push "
                          "(review.nozzle_force, measured, turns this into a check)",
                          top(l), f);
            out.push_back({"stability_flexibility", int(l), top(l), text, f, "information"});
        }
    }
    // The most serious first; among equals, from the bottom up.
    const auto rank = [](const std::string& severity) { return severity == "critical" ? 0 : severity == "warning" ? 1 : 2; };
    std::stable_sort(out.begin(), out.end(), [&](const Warning& a, const Warning& b) {
        return rank(a.severity) < rank(b.severity) || (rank(a.severity) == rank(b.severity) && a.z < b.z);
    });
    return out;
}

} // namespace tisma
