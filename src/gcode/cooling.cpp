// Minimum layer time (docs/spec/BEHAVIOR.md, "Enfriamiento"). Own design; see
// PROVENANCE.md.

#include "gcode/cooling.hpp"

#include <algorithm>

namespace tisma {

double Slowdown::feed(double f) const { return std::min(std::max(f * scale, std::min(f, floor)), cap); }

double layer_time(const std::map<double, double>& length_by_feed, double other_time, const Slowdown& slowdown) {
    double t = other_time;
    for (const auto& [f, length] : length_by_feed)
        t += length / slowdown.feed(f);
    return t;
}

Slowdown plan_slowdown(const std::map<double, double>& length_by_feed, double other_time, double min_time,
                       double min_speed) {
    Slowdown s;
    if (length_by_feed.empty() || layer_time(length_by_feed, other_time, s) >= min_time)
        return s;
    // The time only grows as the speeds go down: bisection on one number, lo
    // always reaching the time and hi not.
    auto bisect = [&](double& value, double lo, double hi) {
        for (int i = 0; i < 60; ++i) {
            value = (lo + hi) / 2.0;
            (layer_time(length_by_feed, other_time, s) >= min_time ? lo : hi) = value;
        }
        value = lo;
    };
    s.stage = 1;
    s.floor = min_speed;
    s.scale = 0.0; // every extrusion at the minimum print speed (or slower, if it was)
    if (layer_time(length_by_feed, other_time, s) >= min_time) {
        bisect(s.scale, 0.0, 1.0);
        return s;
    }
    s = Slowdown{};
    s.stage = 2;
    s.cap = kSlowestFeed;
    if (layer_time(length_by_feed, other_time, s) < min_time)
        return s; // not even 1 mm/min is enough
    bisect(s.cap, kSlowestFeed, length_by_feed.rbegin()->first);
    return s;
}

namespace {

// What is not an outer wall, by feed.
std::map<double, double> inner_part(const std::map<double, double>& all, const std::map<double, double>& outer) {
    std::map<double, double> out;
    for (const auto& [f, length] : all) {
        const auto o = outer.find(f);
        const double rest = length - (o == outer.end() ? 0.0 : o->second);
        if (rest > 1e-9)
            out[f] = rest;
    }
    return out;
}

double outer_time(const std::map<double, double>& outer) {
    double t = 0.0;
    for (const auto& [f, length] : outer)
        t += length / f;
    return t;
}

} // namespace

Slowdown plan_slowdown(const std::map<double, double>& length_by_feed, const std::map<double, double>& outer_by_feed,
                       double other_time, double min_time, double min_speed) {
    if (outer_by_feed.empty())
        return plan_slowdown(length_by_feed, other_time, min_time, min_speed);
    const std::map<double, double> inner = inner_part(length_by_feed, outer_by_feed);
    if (!inner.empty()) {
        Slowdown s = plan_slowdown(inner, other_time + outer_time(outer_by_feed), min_time, min_speed);
        if (s.stage <= 1) {
            s.spare_outer = s.stage == 1;
            return s;
        }
    }
    return plan_slowdown(length_by_feed, other_time, min_time, min_speed);
}

double layer_time(const std::map<double, double>& length_by_feed, const std::map<double, double>& outer_by_feed,
                  double other_time, const Slowdown& slowdown) {
    if (!slowdown.spare_outer)
        return layer_time(length_by_feed, other_time, slowdown);
    return layer_time(inner_part(length_by_feed, outer_by_feed), other_time + outer_time(outer_by_feed), slowdown);
}

double layer_fan(double t, double min_time, double below_time, double fan_min, double fan_max, bool always) {
    if (t <= min_time)
        return fan_max;
    if (t >= below_time)
        return always ? fan_min : 0.0;
    return fan_max - (fan_max - fan_min) * (t - min_time) / (below_time - min_time);
}

} // namespace tisma
