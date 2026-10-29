// Print time estimate. Own design from textbook constant-acceleration
// kinematics; see PROVENANCE.md.

#include "gcode/print_time.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace tisma {

double move_time(double length, double v0, double v1, double feed, double a) {
    if (length <= 0.0)
        return 0.0;
    if (!(a > 0.0))
        return length / feed;
    const double accelerate = (feed * feed - v0 * v0) / (2.0 * a);
    const double decelerate = (feed * feed - v1 * v1) / (2.0 * a);
    if (accelerate + decelerate <= length) // reaches the feed: trapezoid
        return (feed - v0) / a + (feed - v1) / a + (length - accelerate - decelerate) / feed;
    const double peak = std::sqrt((2.0 * a * length + v0 * v0 + v1 * v1) / 2.0); // triangle
    return (peak - v0) / a + (peak - v1) / a;
}

void PrintTime::add(double dx, double dy, double dz, double de, double feed, bool extruding) {
    const double xyz = std::sqrt(dx * dx + dy * dy + dz * dz);
    const double length = xyz > 0.0 ? xyz : std::abs(de);
    if (length <= 0.0 || !(feed > 0.0))
        return;
    // No slower than the machine moves at all, and no axis faster than its
    // limit: the feed shrinks with the worst one, and so does the acceleration.
    const double d[4] = {dx, dy, dz, de};
    double v = std::max(feed, extruding ? k_.min_extrude_feed : k_.min_travel_feed);
    double a = extruding ? k_.acceleration
               : xyz > 0.0 || !(k_.retract_acceleration > 0.0) ? k_.travel_acceleration
                                                               : k_.retract_acceleration;
    for (int i = 0; i < 4; ++i)
        if (d[i] != 0.0) {
            if (k_.max_feed[i] > 0.0)
                v = std::min(v, k_.max_feed[i] * length / std::abs(d[i]));
            if (k_.max_accel[i] > 0.0)
                a = std::min(a, k_.max_accel[i] * length / std::abs(d[i]));
        }
    Move m{length, v, a, 0.0, 0.0, 0.0, de / length};
    if (xyz > 0.0)
        m.ux = dx / xyz, m.uy = dy / xyz, m.uz = dz / xyz;
    moves_.push_back(m);
}

std::size_t PrintTime::mark() {
    pending_marks_.push_back({mark_times_.size(), moves_.size()});
    mark_times_.push_back(total_);
    return mark_times_.size() - 1;
}

double PrintTime::flush() {
    const std::size_t n = moves_.size();
    if (n == 0) {
        for (const auto& [mark, before] : pending_marks_)
            mark_times_[mark] = total_;
        pending_marks_.clear();
        return 0.0;
    }
    const bool jerks = k_.jerk[0] > 0.0 || k_.jerk[1] > 0.0 || k_.jerk[2] > 0.0 || k_.jerk[3] > 0.0;
    // junction[i]: the speed between move i - 1 and move i; at rest at both ends.
    std::vector<double> junction(n + 1, 0.0);
    for (std::size_t i = 1; i < n; ++i) {
        const Move& a = moves_[i - 1];
        const Move& b = moves_[i];
        const double lower = std::min(a.feed, b.feed);
        const double cosine = a.ux * b.ux + a.uy * b.uy + a.uz * b.uz; // 0 when either is E alone
        if (k_.junction_deviation > 0.0) {
            // The corner rounded by a circle that keeps junction_deviation from
            // it: sin(a/2) of the angle a between the moves, from its cosine.
            const double sine = std::sqrt(std::max(0.0, 0.5 * (1.0 + cosine)));
            const double r = sine >= 1.0 - 1e-9 ? std::numeric_limits<double>::infinity()
                                                : k_.junction_deviation * sine / (1.0 - sine);
            junction[i] = std::min(lower, std::sqrt(std::min(a.acceleration, b.acceleration) * r));
        } else if (jerks) {
            // Each axis changes its speed at once by its jerk at most.
            const double da[4] = {a.ux - b.ux, a.uy - b.uy, a.uz - b.uz, a.ue - b.ue};
            double v = lower;
            for (int k = 0; k < 4; ++k)
                if (k_.jerk[k] > 0.0 && std::abs(da[k]) > 1e-12)
                    v = std::min(v, k_.jerk[k] / std::abs(da[k]));
            junction[i] = v;
        } else {
            junction[i] = std::min(lower, std::max(k_.corner_speed, lower * cosine));
        }
    }
    // Backward: each move must be able to slow down to its exit speed.
    for (std::size_t i = n; i-- > 0;)
        junction[i] = std::min(junction[i], std::sqrt(junction[i + 1] * junction[i + 1] +
                                                      2.0 * moves_[i].acceleration * moves_[i].length));
    // Forward: and to speed up from its entry speed.
    for (std::size_t i = 0; i < n; ++i)
        junction[i + 1] = std::min(junction[i + 1], std::sqrt(junction[i] * junction[i] +
                                                              2.0 * moves_[i].acceleration * moves_[i].length));
    double t = 0.0;
    std::size_t next_mark = 0;
    for (std::size_t i = 0; i < n; ++i) {
        for (; next_mark < pending_marks_.size() && pending_marks_[next_mark].second <= i; ++next_mark)
            mark_times_[pending_marks_[next_mark].first] = total_ + t;
        t += move_time(moves_[i].length, junction[i], junction[i + 1], moves_[i].feed, moves_[i].acceleration);
    }
    for (; next_mark < pending_marks_.size(); ++next_mark)
        mark_times_[pending_marks_[next_mark].first] = total_ + t;
    pending_marks_.clear();
    moves_.clear();
    total_ += t;
    return t;
}

std::string format_duration(double seconds) {
    long s = std::lround(std::max(seconds, 0.0));
    const long d = s / 86400, h = s / 3600 % 24, m = s / 60 % 60;
    s %= 60;
    std::string out;
    if (d > 0)
        out += std::to_string(d) + "d ";
    if (d > 0 || h > 0)
        out += std::to_string(h) + "h ";
    if (d > 0 || h > 0 || m > 0)
        out += std::to_string(m) + "m ";
    return out + std::to_string(s) + "s";
}

} // namespace tisma
