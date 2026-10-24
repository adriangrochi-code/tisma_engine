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
#include <functional>
#include <limits>
#include <map>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <numbers>
#include <optional>
#include <string_view>

namespace tisma {
namespace {

// A number with fixed decimals, as printf's "%.*f" writes it (std::to_chars
// is specified to round the same way, and is much faster), without a negative
// zero. Returns the length written to buffer.
constexpr std::size_t kNumberCapacity = 400; // any finite double with a few decimals
std::size_t format_fixed(char* buffer, double v, int decimals) {
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

    void new_layer(double z, double height, double travel_feed) {
        text_ += ";LAYER_CHANGE\n;Z:" + fixed(z, 3) + "\n;HEIGHT:" + fixed(height, 3) + "\n";
        if (!relative_) {
            // A retraction carries over the layer change: E restarts at minus its length.
            e_ = retracted_ ? -retract_ : 0.0;
            text_ += "G92 E" + fixed(e_, 5) + "\n";
            position_[3] = e_;
            e_text_.clear();
        }
        move("G1", std::nullopt, std::nullopt, z, std::nullopt, travel_feed);
        height_ = height;
        z_value_ = z;
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
            other_time_ += retract_ / retract_feed_;
        }
        const bool lift = retract && lift_ > 0.0;
        other_time_ += plan.length / feed + (lift ? 2.0 * lift_ / feed : 0.0);
        if (lift)
            move("G1", std::nullopt, std::nullopt, z_value_ + lift_, std::nullopt, feed);
        for (const Point2d& q : plan.points)
            move("G1", q.x, q.y, std::nullopt, std::nullopt, feed);
        if (lift)
            move("G1", std::nullopt, std::nullopt, z_value_, std::nullopt, feed);
        x_value_ = p.x, y_value_ = p.y;
    }

    void extrude(const ToolpathPoint& from, const ToolpathPoint& to, double feed) {
        if (retracted_) { // unretract: back to where E was
            e_ = relative_ ? retract_ : e_ + retract_;
            move("G1", std::nullopt, std::nullopt, std::nullopt, e_, retract_feed_);
            retracted_ = false;
            other_time_ += retract_ / retract_feed_;
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
        const double e = width * height_ * length / filament_area_;
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
        move("G1", to.p.x, to.p.y, std::nullopt, e_, paced);
        x_value_ = to.p.x, y_value_ = to.p.y, known_ = true;
    }

    double total_e() const { return total_e_; }

private:
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
    double other_time_ = 0.0;
    long fan_ = -1; // not set yet
    double filament_area_ = 1.0;
    double retract_, retract_feed_, lift_, min_travel_;
    double height_ = 0.2;
    double e_ = 0.0, total_e_ = 0.0, pending_ = 0.0;
    double x_value_ = 0.0, y_value_ = 0.0, z_value_ = 0.0;
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

void write_gcode(std::ostream& out, const SlicedModel& model, const Settings& s, std::vector<Warning>* warnings) {
    out << "; generated by Tisma engine " << TISMA_VERSION << "\n";
    Emitter g(s);
    g.type("Custom");
    g.flush(out);
    if (!s.start_gcode.empty())
        out << substitute_variables(s.start_gcode, s.variables) << "\n";
    out << "G90\n" << (s.relative_extrusion ? "M83" : "M82") << "\n";
    Point2d cursor{0.0, 0.0};
    SeamMemory seams;

    auto emit_layer = [&](std::size_t l, double fan) {
        const Layer& layer = model.layers[l];
        g.new_layer(layer.top, layer.top - layer.bottom, s.travel_speed);
        g.fan(fan);
        seams.next_layer();
        const TravelPlanner planner(l < model.regions.size() ? model.regions[l] : Polygons{}, s.wall_width);
        const bool first = l == 0;
        auto speed = [&](double v) { return std::min(first ? std::min(v, s.first_layer_speed) : v, s.max_speed); };
        auto lay = [&](const Toolpath& t, const char* type, double feed) {
            if (t.points.empty())
                return;
            g.travel(t.points.front().p, s.travel_speed, planner);
            g.type(type);
            for (std::size_t i = 1; i < t.points.size(); ++i)
                g.extrude(t.points[i - 1], t.points[i], feed);
            if (t.closed && t.points.size() > 2)
                g.extrude(t.points.back(), t.points.front(), feed);
        };
        const std::uint64_t seed = std::uint64_t(l) * 1000003u;
        auto group = [&](const std::vector<Toolpath>& paths, const char* type, double feed, SeamMode seam) {
            for (const Toolpath& t : order_paths(paths, cursor, seam, seed, seams))
                lay(t, type, feed);
        };
        // Every island is finished before the next, the nearest one first. In an
        // island: walls from the inside out, the outer wall (bead 0) last, each
        // bead index in nearest order; then the fills, one kind after the other.
        struct Island {
            std::map<int, std::vector<Toolpath>, std::greater<int>> walls;
            std::array<std::vector<Toolpath>, 5> fills; // solid, top, bridge, gaps, sparse
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
            const std::vector<Toolpath>* kinds[5] = {&f.solid, &f.top, &f.bridge, &f.gaps, &f.sparse};
            for (std::size_t k = 0; k < 5; ++k)
                for (const Toolpath& t : *kinds[k])
                    if (!t.points.empty()) {
                        Island& i = island(t);
                        i.fills[k].push_back(t);
                        add(i, t);
                    }
        }
        const char* const fill_type[5] = {"Solid infill", "Top solid infill", "Bridge infill", "Gap fill", "Internal infill"};
        const double fill_speed[5] = {s.solid_infill_speed, s.top_solid_infill_speed, s.solid_infill_speed,
                                      s.solid_infill_speed, s.infill_speed};
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
            for (const auto& [index, paths] : i.walls) {
                const bool outer = index == 0;
                group(paths, outer ? "External perimeter" : "Perimeter",
                      speed(outer ? s.outer_wall_speed.of(s.wall_speed) : s.wall_speed), s.seam);
            }
            for (std::size_t k = 0; k < 5; ++k)
                group(i.fills[k], fill_type[k], speed(fill_speed[k]), SeamMode::Nearest);
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
        const bool fan_off = l < std::size_t(std::max(s.fan_off_layers, 0));
        emit_layer(l, fan_off ? 0.0 : s.fan_min);
        const Slowdown slowdown = plan_slowdown(g.extruded(), g.other_time(), s.min_layer_time, s.min_print_speed);
        if (slowdown.stage != 0) {
            g = emitter_before;
            cursor = cursor_before;
            seams = seams_before;
            g.pace(slowdown);
            emit_layer(l, fan_off ? 0.0 : s.fan_max);
            g.pace(Slowdown{});
            const double reached = layer_time(g.extruded(), g.other_time(), slowdown);
            if (warnings && reached < s.min_layer_time * (1.0 - 1e-9)) {
                char text[160];
                std::snprintf(text, sizeof text, "the layer takes %.1f s even at 1 mm/min, less than the minimum of %.1f s",
                              reached, s.min_layer_time);
                warnings->push_back({"layer_time", int(l), model.layers[l].top, text, reached});
            }
        }
        g.end_layer();
        g.flush(out);
    }

    g.type("Custom");
    g.flush(out);
    if (!s.end_gcode.empty())
        out << substitute_variables(s.end_gcode, s.variables) << "\n";
    // docs/spec/GCODE_OUTPUT.md, "Tiempos" (the start and end G-code not counted).
    out << "; estimated printing time = " << format_duration(g.print_time()) << "\n";
    for (const auto& [key, value] : describe(s))
        out << "; " << key << " = " << value << "\n";
}

} // namespace tisma
