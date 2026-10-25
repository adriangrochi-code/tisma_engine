// G-code output (docs/spec/GCODE_OUTPUT.md). Own design; see PROVENANCE.md.

#include "gcode/writer.hpp"

#include "gcode/cooling.hpp"
#include "gcode/order.hpp"
#include "gcode/print_time.hpp"
#include "gcode/travel.hpp"

#include <algorithm>
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
#include <unordered_map>
#include <set>
#include <numbers>
#include <optional>
#include <string_view>

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

} // namespace

std::string fixed_number(double v, int decimals) { return fixed(v, decimals); }

namespace {

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
    std::size_t edges = 0;
    for (const auto& [key, vote] : votes) {
        if (vote == 0.0)
            continue;
        const auto a = std::uint32_t(key >> 32), b = std::uint32_t(key & 0xFFFFFFFFu);
        if (vote > 0.0)
            after[a].push_back(b); // b higher: a first
        else
            after[b].push_back(a);
        ++edges;
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
class Emitter {
public:
    explicit Emitter(const Settings& s)
        : relative_(s.relative_extrusion),
          time_(Kinematics{{s.max_feed_x, s.max_feed_y, s.max_feed_z, s.max_feed_e},
                           s.acceleration,
                           s.travel_acceleration,
                           s.corner_speed}),
          retract_(s.retract_length), retract_feed_(s.retract_speed), lift_(s.retract_lift),
          min_travel_(s.retract_min_travel) {
        const double r = s.filament_diameter / 2.0;
        filament_area_ = std::numbers::pi * r * r;
        // The melt limit: the lower of the hot end's and the filament's (0: none).
        for (const double limit : {s.machine_max_flow, s.filament_max_flow})
            if (limit > 0.0)
                max_flow_ = max_flow_ > 0.0 ? std::min(max_flow_, limit) : limit;
        feed_limit_ = std::max(s.max_feed_x, s.max_feed_y);
    }

    // Plans the time of the moves since the last call (they end at rest).
    void end_layer() { time_.flush(); }
    double print_time() const { return time_.total(); }

    void flush(std::ostream& out) {
        out.write(text_.data(), std::streamsize(text_.size()));
        text_.clear();
    }

    // Extrusion speeds of the layer being written go through this.
    void pace(const Slowdown& slowdown) { pace_ = slowdown; }

    // What the layer's moves take: extruded length per feed, and the time of
    // everything else (travels, retractions, lifts).
    void reset_times() {
        extruded_.clear();
        other_time_ = 0.0;
    }
    const std::map<double, double>& extruded() const { return extruded_; }
    double other_time() const { return other_time_; }

    // The part-cooling fan in percent: M106 S0-255, or M107 when off.
    void fan(double percent) {
        const long value = std::lround(std::clamp(percent, 0.0, 100.0) * 2.55);
        if (value == fan_)
            return;
        if (value == 0) {
            text_ += "M107\n";
        } else {
            text_ += "M106 S";
            append_integer(text_, value);
            text_ += '\n';
        }
        fan_ = value;
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
    void custom(const std::string& lines) {
        if (lines.empty())
            return;
        const char* before = type_;
        type("Custom");
        text_ += lines;
        if (lines.back() != '\n')
            text_ += '\n';
        if (before && std::string_view(before) != "Custom")
            type(before);
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

    void new_layer(double z, double height, double travel_feed) {
        text_ += ";LAYER_CHANGE\n;Z:" + fixed(z, 3) + "\n;HEIGHT:" + fixed(height, 3) + "\n";
        if (!relative_) {
            // A retraction carries over the layer change: E restarts at minus its length.
            e_ = retracted_ ? -retracted_amount_ : 0.0;
            text_ += "G92 E" + fixed(e_, 5) + "\n";
            position_[3] = e_;
            e_text_.clear();
        }
        move("G1", std::nullopt, std::nullopt, z, std::nullopt, travel_feed);
        height_ = height;
        z_value_ = z;
        z_now_ = written(z);
        clearance_ = 0.0;
    }

    void type(const char* t) {
        if (type_ != t) {
            text_ += ";TYPE:";
            text_ += t;
            text_ += '\n';
            type_ = t;
        }
    }

    // A travel along the planner's route. It retracts first (and lifts Z by
    // lift) when it leaves the part or is longer than min_travel.
    void travel(const Point2d& p, double feed, const TravelPlanner& planner) {
        if (!known_) {
            move("G1", p.x, p.y, std::nullopt, std::nullopt, feed);
            x_value_ = p.x, y_value_ = p.y, known_ = true;
            return;
        }
        const TravelPlan plan = planner.plan({x_value_, y_value_}, p);
        const bool retract = plan.leaves_part || plan.length > min_travel_;
        if (retract && retract_ > 0.0 && !retracted_) {
            e_ = relative_ ? -retract_ : e_ - retract_;
            move("G1", std::nullopt, std::nullopt, std::nullopt, e_, retract_feed_);
            retracted_ = true;
            retracted_amount_ = retract_;
            other_time_ += retract_ / retract_feed_;
        }
        // Off the layer's top (Z anti-aliasing): back up, and over what was raised.
        const double cruise = written(z_value_ + clearance_);
        if (z_now_ != cruise) {
            move("G1", std::nullopt, std::nullopt, cruise, std::nullopt, feed);
            z_now_ = cruise;
        }
        const bool lift = retract && lift_ > 0.0;
        const double level = clearance_ > 0.0 ? cruise : z_value_;
        other_time_ += plan.length / feed + (lift ? 2.0 * lift_ / feed : 0.0);
        if (lift)
            move("G1", std::nullopt, std::nullopt, level + lift_, std::nullopt, feed);
        for (const Point2d& q : plan.points)
            move("G1", q.x, q.y, std::nullopt, std::nullopt, feed);
        if (lift)
            move("G1", std::nullopt, std::nullopt, level, std::nullopt, feed);
        x_value_ = p.x, y_value_ = p.y;
    }

    void extrude(const ToolpathPoint& from, const ToolpathPoint& to, double feed) {
        if (retracted_) { // unretract: back to where E was (a length changed since stays the old one)
            e_ = relative_ ? retracted_amount_ : e_ + retracted_amount_;
            move("G1", std::nullopt, std::nullopt, std::nullopt, e_, retract_feed_);
            retracted_ = false;
            other_time_ += retracted_amount_ / retract_feed_;
        }
        const double width = (from.width + to.width) / 2.0;
        char w[kNumberCapacity];
        const std::string_view width_text(w, format_fixed(w, width, 3));
        if (width_text != width_) {
            text_ += ";WIDTH:";
            text_ += width_text;
            text_ += '\n';
            width_.assign(width_text);
        }
        const double length = std::hypot(to.p.x - from.p.x, to.p.y - from.p.y);
        // Z anti-aliasing (docs/ZAA.md): the nozzle at the surface, and the bead
        // as thick as what is left down to the layer's bottom, by the Z written.
        const double z_layer = written(z_value_), z_from = written(z_value_ + from.dz), z_to = written(z_value_ + to.dz);
        if (z_now_ != z_from) {
            move("G1", std::nullopt, std::nullopt, z_from, std::nullopt, feed);
            z_now_ = z_from;
        }
        double height = height_ + ((z_from - z_layer) + (z_to - z_layer)) / 2.0;
        if (nonplanar_) {
            // On the wavy bead below, not on the layer below's top.
            height -= (from.below + to.below) / 2.0;
            const std::string shown = fixed(height, 3);
            if (shown != shown_height_) {
                text_ += ";HEIGHT:" + shown + "\n";
                shown_height_ = shown;
            }
        }
        const double e = width * height * length / filament_area_ * flow_;
        if (forced_flow_ > 0.0 && width * height * flow_ > 0.0) {
            // A flow tower: exactly this flow, past every cap (it looks for the
            // limit), as far as the machine moves.
            feed = std::min(feed_limit_, std::max(1.0, std::floor(forced_flow_ / (width * height * flow_) * 60.0)) / 60.0);
        } else if (max_flow_ > 0.0 && width * height * flow_ > 0.0) {
            // No faster than the hot end and the filament can melt: the feed is
            // capped at the volumetric limit, rounded down to whole mm/min.
            const double cap = max_flow_ / (width * height * flow_);
            if (feed > cap)
                feed = std::max(1.0, std::floor(cap * 60.0)) / 60.0;
        }
        extruded_[feed] += length;
        total_e_ += e;
        pending_ += e;
        // A piece that rounds to no move at 3 decimals is not written; its E
        // (next to nothing) goes with the next piece.
        char tx[kNumberCapacity], ty[kNumberCapacity];
        if (std::string_view(tx, format_fixed(tx, to.p.x, 3)) == x_ &&
            std::string_view(ty, format_fixed(ty, to.p.y, 3)) == y_)
            return;
        e_ = relative_ ? pending_ : e_ + pending_;
        pending_ = 0.0;
        // A slowed-down feed is rounded down to whole mm/min (as F is written), so
        // the layer lasts at least the minimum time.
        const double paced = pace_.stage == 0 ? feed : std::max(1.0, std::floor(pace_.feed(feed) * 60.0)) / 60.0;
        move("G1", to.p.x, to.p.y, z_to != z_now_ ? std::optional<double>(z_to) : std::nullopt, e_, paced);
        x_value_ = to.p.x, y_value_ = to.p.y, known_ = true;
        z_now_ = z_to;
    }

    double total_e() const { return total_e_; }

private:
    // A Z as the G-code writes it (three decimals), to compare and to measure with.
    static double written(double z) { return std::round(z * 1000.0) / 1000.0; }

    void move(const char* g, std::optional<double> x, std::optional<double> y, std::optional<double> z,
              std::optional<double> e, double feed) {
        const std::size_t start = text_.size();
        text_ += g;
        const std::size_t bare = text_.size();
        auto axis = [&](char name, std::optional<double> v, int decimals, std::string& last) {
            if (!v)
                return;
            char buffer[kNumberCapacity];
            const std::string_view t(buffer, format_fixed(buffer, *v, decimals));
            if (t != last || name == 'E') {
                text_ += ' ';
                text_ += name;
                text_ += t;
                last.assign(t);
            }
        };
        axis('X', x, 3, x_);
        axis('Y', y, 3, y_);
        axis('Z', z, 3, z_);
        axis('E', e, 5, e_text_);
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
        text_ += '\n';
        // The move for the time estimate, at the feed as written.
        const double to[4] = {x.value_or(position_[0]), y.value_or(position_[1]), z.value_or(position_[2]),
                              e ? (relative_ ? position_[3] + *e : *e) : position_[3]};
        const double de = to[3] - position_[3];
        time_.add(to[0] - position_[0], to[1] - position_[1], to[2] - position_[2], de, double(f_) / 60.0,
                  de > 0.0 && (x || y));
        std::copy(to, to + 4, position_);
    }

    std::string text_;
    bool relative_;
    PrintTime time_;
    double position_[4] = {0.0, 0.0, 0.0, 0.0}; // X, Y, Z, E as written
    Slowdown pace_;
    std::map<double, double> extruded_;
    double max_flow_ = 0.0; // mm3/s; 0: no limit
    double forced_flow_ = 0.0; // mm3/s every extrusion runs at; 0: the feeds asked
    double feed_limit_ = 0.0;  // mm/s the machine moves: a forced flow never asks more
    double other_time_ = 0.0;
    long fan_ = -1; // not set yet
    double filament_area_ = 1.0;
    double retract_, retract_feed_, lift_, min_travel_;
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
    bool retracted_ = false;
    std::string x_, y_, z_, e_text_, width_;
    long f_ = std::numeric_limits<long>::min(); // none written yet
    const char* type_ = nullptr;
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
                        ok = !digits.empty() && std::all_of(digits.begin(), digits.end(), [](char c) { return c >= '0' && c <= '9'; });
                        if (ok)
                            index = std::stoul(digits);
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
    return "M204 P" + p + " T" + t + " ; acceleration";
}

std::string pressure_advance_gcode(GcodeFlavor flavor, double value) {
    const std::string v = fixed(value, 4);
    switch (flavor) {
    case GcodeFlavor::Marlin:
        return "M900 K" + v + " ; pressure advance";
    case GcodeFlavor::Klipper:
        return "SET_PRESSURE_ADVANCE ADVANCE=" + v;
    case GcodeFlavor::RepRap:
        return "M572 D0 S" + v + " ; pressure advance";
    }
    return {};
}

void write_gcode(std::ostream& out, const SlicedModel& model, const Settings& s, std::vector<Warning>* warnings) {
    out << "; generated by Tisma engine " << TISMA_VERSION << "\n";
    Emitter g(s);
    g.set_flow(s.flow / 100.0);
    g.type("Custom");
    g.flush(out);
    if (!s.start_gcode.empty())
        out << substitute_variables(s.start_gcode, s.variables) << "\n";
    if (s.pressure_advance > 0.0)
        out << pressure_advance_gcode(s.flavor, s.pressure_advance) << "\n";
    if (s.acceleration > 0.0)
        out << acceleration_gcode(s.flavor, s.acceleration, s.travel_acceleration) << "\n";
    // The chamber the filament wants, when the machine heats one.
    const double chamber = std::min(s.chamber_temperature, s.max_chamber_temp);
    if (s.chamber_temperature > 0.0 && s.max_chamber_temp > 0.0)
        out << chamber_gcode(s.flavor, chamber) << "\n";
    if (warnings) {
        const std::vector<Warning> capability = capability_warnings(s);
        warnings->insert(warnings->end(), capability.begin(), capability.end());
    }
    out << "G90\n" << (s.relative_extrusion ? "M83" : "M82") << "\n";
    Point2d cursor{0.0, 0.0};
    SeamMemory seams;

    // The values the height changes set, layer by layer (docs/CONFIG.md, "changes").
    struct Active {
        double retract_length = 0.0, retract_speed = 1.0, flow = 1.0, speed = 1.0;
        double volumetric_flow = 0.0; // mm3/s forced on every extrusion; 0: none
        std::optional<double> fan;
        std::string lines; // what this layer writes first: temperatures and the changes' own G-code
    };
    Active active;
    active.retract_length = s.retract_length;
    active.retract_speed = s.retract_speed;
    active.flow = s.flow / 100.0;
    std::size_t next_change = 0;
    // The part prints resting on the bed wherever its mesh is in Z: heights
    // in the G-code (and of the height changes) count from its bottom.
    const double base = model.layers.empty() ? 0.0 : model.layers.front().bottom;

    auto emit_layer = [&](std::size_t l, double fan, const Active& a) {
        const Layer& layer = model.layers[l];
        g.new_layer(layer.top - base, layer.top - layer.bottom, s.travel_speed);
        g.custom(a.lines);
        g.set_retraction(a.retract_length, a.retract_speed);
        g.set_flow(a.flow);
        g.set_forced_flow(a.volumetric_flow);
        g.fan(fan);
        seams.next_layer();
        const TravelPlanner planner(l < model.regions.size() ? model.regions[l] : Polygons{}, s.wall_width);
        const bool first = l == 0;
        // A speed change scales every extrusion, past the usual caps (a speed tower).
        auto speed = [&](double v) {
            return std::min(first ? std::min(v, s.first_layer_speed) : v, s.max_speed) * a.speed;
        };
        auto lay = [&](const Toolpath& t, const char* type, double feed) {
            if (t.points.empty())
                return;
            g.travel(t.points.front().p, s.travel_speed, planner);
            g.type(type);
            g.nonplanar(t.wavy);
            for (std::size_t i = 1; i < t.points.size(); ++i)
                g.extrude(t.points[i - 1], t.points[i], feed);
            if (t.closed && t.points.size() > 2)
                g.extrude(t.points.back(), t.points.front(), feed);
            g.nonplanar(false);
        };
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
        auto group = [&](const std::vector<Toolpath>& paths, const char* type, double feed, SeamMode seam) {
            std::vector<Toolpath> ordered = order_paths(paths, cursor, seam, seed, seams);
            if (deferring) {
                for (const Toolpath& t : ordered) {
                    if (t.wavy) { // its waves are its own: not cut by height
                        pending.push_back({t, type, feed});
                        continue;
                    }
                    for (Toolpath& piece : split_by_height(t))
                        pending.push_back({std::move(piece), type, feed});
                }
                return;
            }
            for (const Toolpath& t : ordered)
                lay(t, type, feed);
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
                lay(pending[k].path, pending[k].type, pending[k].feed);
            pending.clear();
            deferring = false;
        };
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
        // Skirt, then brim, before the part, at the first layer's speed.
        if (l < std::size_t(std::max(s.skirt_layers, 0)))
            group(model.skirt, "Skirt/Brim", std::min(s.first_layer_speed, s.max_speed) * a.speed, SeamMode::Nearest);
        if (l == 0)
            group(model.brim, "Skirt/Brim", std::min(s.first_layer_speed, s.max_speed) * a.speed, SeamMode::Nearest);
        // Every island is finished before the next, the nearest one first. In an
        // island: walls from the inside out, the outer wall (bead 0) last, each
        // bead index in nearest order; then the fills, one kind after the other.
        struct Island {
            std::map<int, std::vector<Toolpath>, std::greater<int>> walls;
            std::array<std::vector<Toolpath>, 7> fills; // solid, top, bridge, gaps, sparse, support, support interface
            std::vector<Point2d> ends;                  // where its paths start or end
        };
        const std::vector<Polygons> pieces = l < model.regions.size() ? split_islands(model.regions[l]) : std::vector<Polygons>{};
        std::vector<Island> islands(pieces.size() + 1); // the last one takes paths in no island
        auto island = [&](const Toolpath& t) -> Island& {
            if (pieces.size() <= 1)
                return islands.front();
            const int k = island_of(pieces, t.points.front().p);
            return islands[k < 0 ? pieces.size() : std::size_t(k)];
        };
        auto add = [&](Island& i, const Toolpath& t) {
            i.ends.push_back(t.points.front().p);
            if (!t.closed)
                i.ends.push_back(t.points.back().p);
        };
        if (l < model.walls.size())
            for (const Toolpath& t : model.walls[l].toolpaths)
                if (!t.points.empty()) {
                    Island& i = island(t);
                    i.walls[t.index].push_back(t);
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
        }
        const char* const fill_type[7] = {"Solid infill",    "Top solid infill", "Bridge infill",
                                          "Gap fill",        "Internal infill",  "Support material",
                                          "Support material interface"};
        const double fill_speed[7] = {s.solid_infill_speed, s.top_solid_infill_speed, s.solid_infill_speed,
                                      s.solid_infill_speed, s.infill_speed,          s.support_speed,
                                      s.support_speed};
        std::vector<bool> done(islands.size(), false);
        for (;;) {
            std::size_t next = islands.size();
            double nearest = std::numeric_limits<double>::max();
            for (std::size_t k = 0; k < islands.size(); ++k)
                for (const Point2d& p : islands[k].ends)
                    if (!done[k] && std::hypot(p.x - cursor.x, p.y - cursor.y) < nearest)
                        nearest = std::hypot(p.x - cursor.x, p.y - cursor.y), next = k;
            if (next == islands.size())
                break;
            done[next] = true;
            const Island& i = islands[next];
            auto walls = [&](int index, const std::vector<Toolpath>& paths) {
                const bool outer = index == 0;
                group(paths, outer ? "External perimeter" : "Perimeter",
                      speed(outer ? s.outer_wall_speed.of(s.wall_speed) : s.wall_speed), s.seam);
            };
            if (s.zaa) {
                for (const auto& [index, paths] : i.walls)
                    deferring = deferring || off_top({&paths});
                deferring = deferring || off_top({&i.fills[1], &i.fills[3]});
            }
            for (const auto& [index, paths] : i.walls)
                walls(index, paths);
            for (std::size_t k = 0; k < 7; ++k)
                group(i.fills[k], fill_type[k], speed(fill_speed[k]), SeamMode::Nearest);
            flush();
        }
        // The thick zone of a group that ends here: the whole group's height at
        // once, the nozzle at its top (docs/LOCAL_LAYERS.md).
        if (l < model.thick.size() && !model.thick[l].empty()) {
            const ThickLayer& t = model.thick[l];
            g.set_height(t.height);
            deferring = s.zaa && off_top({&t.walls, &t.fill.top, &t.fill.gaps});
            std::map<int, std::vector<Toolpath>, std::greater<int>> by_index;
            for (const Toolpath& w : t.walls)
                by_index[w.index].push_back(w);
            for (const auto& [index, paths] : by_index) {
                const bool outer = index == 0;
                group(paths, outer ? "External perimeter" : "Perimeter",
                      speed(outer ? s.outer_wall_speed.of(s.wall_speed) : s.wall_speed), s.seam);
            }
            const std::vector<Toolpath>* kinds[5] = {&t.fill.solid, &t.fill.top, &t.fill.bridge, &t.fill.gaps,
                                                     &t.fill.sparse};
            for (std::size_t k = 0; k < 5; ++k)
                group(*kinds[k], fill_type[k], speed(fill_speed[k]), SeamMode::Nearest);
            flush();
            g.set_height(layer.top - layer.bottom);
        }
    };

    for (std::size_t l = 0; l < model.layers.size(); ++l) {
        // Each layer is built in the emitter's buffer first. One faster than the
        // minimum layer time is built again from the same state with its
        // extrusions slowed down and the fan at its maximum
        // (docs/spec/BEHAVIOR.md, "Enfriamiento").
        const Emitter emitter_before = g;
        const Point2d cursor_before = cursor;
        const SeamMemory seams_before = seams;
        g.reset_times();
        // Changes below this layer's top take effect now: its top as ;Z: writes
        // it, so a reader of the G-code (the check) takes them at the same layer.
        active.lines.clear();
        const double written_top = std::strtod(fixed(model.layers[l].top - base, 3).c_str(), nullptr);
        while (next_change < s.changes.size() && s.changes[next_change].z < written_top - 1e-6) {
            const HeightChange& c = s.changes[next_change++];
            if (c.temperature)
                active.lines += "M104 S" + std::to_string(std::lround(*c.temperature)) + " ; change at Z " + fixed(c.z, 3) + "\n";
            if (c.bed_temperature)
                active.lines += "M140 S" + std::to_string(std::lround(*c.bed_temperature)) + "\n";
            if (c.pressure_advance)
                active.lines += pressure_advance_gcode(s.flavor, *c.pressure_advance) + "\n";
            if (c.acceleration)
                active.lines += acceleration_gcode(s.flavor, *c.acceleration, s.travel_acceleration) + "\n";
            if (c.volumetric_flow)
                active.volumetric_flow = *c.volumetric_flow;
            if (!c.gcode.empty())
                active.lines += substitute_variables(c.gcode, s.variables) + "\n";
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
        const bool fan_off = l < std::size_t(std::max(s.fan_off_layers, 0));
        emit_layer(l, active.fan ? *active.fan : fan_off ? 0.0 : s.fan_min, active);
        // A forced flow is never slowed down: the tower looks for the limit at that flow.
        const Slowdown slowdown = active.volumetric_flow > 0.0
                                      ? Slowdown{}
                                      : plan_slowdown(g.extruded(), g.other_time(), s.min_layer_time, s.min_print_speed);
        if (slowdown.stage != 0) {
            g = emitter_before;
            cursor = cursor_before;
            seams = seams_before;
            g.pace(slowdown);
            emit_layer(l, active.fan ? *active.fan : s.fan_max, active); // fan_max also on the first layers (owner's decision, 2026-10-24)
            g.pace(Slowdown{});
            const double reached = layer_time(g.extruded(), g.other_time(), slowdown);
            if (warnings && reached < s.min_layer_time * (1.0 - 1e-9)) {
                char text[160];
                std::snprintf(text, sizeof text, "the layer takes %.1f s even at 1 mm/min, less than the minimum of %.1f s",
                              reached, s.min_layer_time);
                warnings->push_back({"layer_time", int(l), model.layers[l].top - base, text, reached});
            }
        }
        g.end_layer();
        g.flush(out);
    }

    g.type("Custom");
    g.flush(out);
    if (!s.end_gcode.empty())
        out << substitute_variables(s.end_gcode, s.variables) << "\n";
    if (s.chamber_temperature > 0.0 && s.max_chamber_temp > 0.0)
        out << chamber_gcode(s.flavor, 0.0) << "\n";
    // docs/spec/GCODE_OUTPUT.md, "Tiempos" (the start and end G-code not counted).
    out << "; estimated printing time = " << format_duration(g.print_time()) << "\n";
    for (const auto& [key, value] : describe(s))
        out << "; " << key << " = " << value << "\n";
}

} // namespace tisma
