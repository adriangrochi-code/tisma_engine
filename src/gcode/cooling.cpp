// Minimum layer time (docs/spec/BEHAVIOR.md, "Enfriamiento"). Own design; see
// PROVENANCE.md.

#include "gcode/cooling.hpp"

#include <algorithm>
#include <limits>

namespace tisma {

namespace {

double time_of(const std::map<double, double>& length_by_feed, double cap) {
    double t = 0.0;
    for (const auto& [f, length] : length_by_feed)
        t += length / std::min(f, cap);
    return t;
}

double fastest(const std::map<double, double>& length_by_feed) {
    return length_by_feed.empty() ? 0.0 : length_by_feed.rbegin()->first;
}

// The highest v in [lo, hi] at which time(v) still reaches min_time, time(lo)
// reaching it; the time only grows as v goes down.
template <typename Time>
double highest(double lo, double hi, double min_time, Time time) {
    if (time(hi) >= min_time)
        return hi;
    for (int i = 0; i < 60; ++i) {
        const double mid = (lo + hi) / 2.0;
        (time(mid) >= min_time ? lo : hi) = mid;
    }
    return lo;
}

} // namespace

double layer_time(const std::map<double, double>& rest, const std::map<double, double>& surface, double other_time,
                  const Slowdown& slowdown) {
    return other_time + time_of(rest, slowdown.cap) + time_of(surface, slowdown.surface_cap);
}

Slowdown plan_slowdown(const std::map<double, double>& rest, const std::map<double, double>& surface,
                       double other_time, double min_time, double min_speed) {
    Slowdown s;
    if ((rest.empty() && surface.empty()) || layer_time(rest, surface, other_time, s) >= min_time)
        return s;
    s.stage = 1;
    const double inf = std::numeric_limits<double>::infinity();
    const double surface_time = time_of(surface, inf);
    // The rest first, no lower than the minimum speed.
    if (!rest.empty() && other_time + surface_time + time_of(rest, min_speed) >= min_time) {
        s.cap = highest(min_speed, std::max(min_speed, fastest(rest)), min_time - other_time - surface_time,
                        [&](double v) { return time_of(rest, v); });
        return s;
    }
    // Then the surface walls too, the rest at the minimum speed.
    const double rest_time = time_of(rest, min_speed);
    if (!surface.empty() && other_time + rest_time + time_of(surface, min_speed) >= min_time) {
        s.cap = min_speed;
        s.surface_cap = highest(min_speed, std::max(min_speed, fastest(surface)), min_time - other_time - rest_time,
                                [&](double v) { return time_of(surface, v); });
        return s;
    }
    // Everything at one speed under the minimum, down to 1 mm/min.
    s.stage = 2;
    const auto all = [&](double v) { return time_of(rest, v) + time_of(surface, v); };
    const double v = all(kSlowestFeed) < min_time - other_time
                         ? kSlowestFeed
                         : highest(kSlowestFeed, std::max(kSlowestFeed, min_speed), min_time - other_time, all);
    s.cap = s.surface_cap = v;
    return s;
}

double layer_fan(double t, double min_time, double below_time, double fan_min, double fan_max, bool always) {
    if (t <= min_time)
        return fan_max;
    if (t >= below_time)
        return always ? fan_min : 0.0;
    return fan_max - (fan_max - fan_min) * (t - min_time) / (below_time - min_time);
}

} // namespace tisma
