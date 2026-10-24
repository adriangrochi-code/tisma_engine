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

} // namespace tisma
