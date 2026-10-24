// G-code output (docs/spec/GCODE_OUTPUT.md). Own design; see PROVENANCE.md.

#include "gcode/writer.hpp"

#include "gcode/order.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <optional>

namespace tisma {
namespace {

std::string fixed(double v, int decimals) {
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, "%.*f", decimals, v);
    std::string s = buffer;
    if (s == "-0" || s.rfind("-0.", 0) == 0) { // no negative zero
        bool zero = true;
        for (char c : s)
            zero = zero && (c == '-' || c == '0' || c == '.');
        if (zero)
            s.erase(0, 1);
    }
    return s;
}

// Keeps the machine's position and writes only the axes that change.
class Emitter {
public:
    Emitter(std::ostream& out, const Settings& s)
        : out_(out), relative_(s.relative_extrusion), retract_(s.retract_length), retract_feed_(s.retract_speed),
          lift_(s.retract_lift), min_travel_(s.retract_min_travel) {
        const double r = s.filament_diameter / 2.0;
        filament_area_ = std::numbers::pi * r * r;
    }

    void comment(const std::string& line) { out_ << line << "\n"; }

    void new_layer(double z, double height, double travel_feed) {
        out_ << ";LAYER_CHANGE\n;Z:" << fixed(z, 3) << "\n;HEIGHT:" << fixed(height, 3) << "\n";
        if (!relative_) {
            // A retraction carries over the layer change: E restarts at minus its length.
            e_ = retracted_ ? -retract_ : 0.0;
            out_ << "G92 E" << fixed(e_, 5) << "\n";
            e_text_.clear();
        }
        move("G1", std::nullopt, std::nullopt, z, std::nullopt, travel_feed);
        height_ = height;
        z_value_ = z;
    }

    void type(const char* t) {
        if (type_ != t) {
            out_ << ";TYPE:" << t << "\n";
            type_ = t;
        }
    }

    // A travel; longer than min_travel it retracts first (and lifts Z by lift).
    void travel(const Point2d& p, double feed) {
        const bool far = known_ && std::hypot(p.x - x_value_, p.y - y_value_) > min_travel_;
        if (far && retract_ > 0.0 && !retracted_) {
            e_ = relative_ ? -retract_ : e_ - retract_;
            move("G1", std::nullopt, std::nullopt, std::nullopt, e_, retract_feed_);
            retracted_ = true;
        }
        const bool lift = far && lift_ > 0.0;
        if (lift)
            move("G1", std::nullopt, std::nullopt, z_value_ + lift_, std::nullopt, feed);
        move("G1", p.x, p.y, std::nullopt, std::nullopt, feed);
        if (lift)
            move("G1", std::nullopt, std::nullopt, z_value_, std::nullopt, feed);
        x_value_ = p.x, y_value_ = p.y, known_ = true;
    }

    void extrude(const ToolpathPoint& from, const ToolpathPoint& to, double feed) {
        if (retracted_) { // unretract: back to where E was
            e_ = relative_ ? retract_ : e_ + retract_;
            move("G1", std::nullopt, std::nullopt, std::nullopt, e_, retract_feed_);
            retracted_ = false;
        }
        const double width = (from.width + to.width) / 2.0;
        const std::string w = fixed(width, 3);
        if (w != width_) {
            out_ << ";WIDTH:" << w << "\n";
            width_ = w;
        }
        const double length = std::hypot(to.p.x - from.p.x, to.p.y - from.p.y);
        const double e = width * height_ * length / filament_area_;
        total_e_ += e;
        pending_ += e;
        // A piece that rounds to no move at 3 decimals is not written; its E
        // (next to nothing) goes with the next piece.
        if (fixed(to.p.x, 3) == x_ && fixed(to.p.y, 3) == y_)
            return;
        e_ = relative_ ? pending_ : e_ + pending_;
        pending_ = 0.0;
        move("G1", to.p.x, to.p.y, std::nullopt, e_, feed);
        x_value_ = to.p.x, y_value_ = to.p.y, known_ = true;
    }

    double total_e() const { return total_e_; }

private:
    void move(const char* g, std::optional<double> x, std::optional<double> y, std::optional<double> z,
              std::optional<double> e, double feed) {
        std::string line = g;
        auto axis = [&](char name, std::optional<double> v, int decimals, std::string& last) {
            if (!v)
                return;
            const std::string t = fixed(*v, decimals);
            if (t != last || name == 'E') {
                line += ' ';
                line += name;
                line += t;
                last = t;
            }
        };
        axis('X', x, 3, x_);
        axis('Y', y, 3, y_);
        axis('Z', z, 3, z_);
        axis('E', e, 5, e_text_);
        const std::string f = std::to_string(std::lround(feed * 60.0));
        if (f != f_) {
            line += " F" + f;
            f_ = f;
        }
        if (line != g)
            out_ << line << "\n";
    }

    std::ostream& out_;
    bool relative_;
    double filament_area_ = 1.0;
    double retract_, retract_feed_, lift_, min_travel_;
    double height_ = 0.2;
    double e_ = 0.0, total_e_ = 0.0, pending_ = 0.0;
    double x_value_ = 0.0, y_value_ = 0.0, z_value_ = 0.0;
    bool known_ = false;     // the position is known (after the first move in XY)
    bool retracted_ = false;
    std::string x_, y_, z_, e_text_, f_, width_;
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

void write_gcode(std::ostream& out, const SlicedModel& model, const Settings& s) {
    out << "; generated by Tisma engine " << TISMA_VERSION << "\n";
    Emitter g(out, s);
    g.type("Custom");
    if (!s.start_gcode.empty())
        out << substitute_variables(s.start_gcode, s.variables) << "\n";
    out << "G90\n" << (s.relative_extrusion ? "M83" : "M82") << "\n";
    Point2d cursor{0.0, 0.0};
    SeamMemory seams;

    for (std::size_t l = 0; l < model.layers.size(); ++l) {
        const Layer& layer = model.layers[l];
        g.new_layer(layer.top, layer.top - layer.bottom, s.travel_speed);
        const bool first = l == 0;
        auto speed = [&](double v) { return std::min(first ? std::min(v, s.first_layer_speed) : v, s.max_speed); };
        auto lay = [&](const Toolpath& t, const char* type, double feed) {
            if (t.points.empty())
                return;
            g.travel(t.points.front().p, s.travel_speed);
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
        // Walls from the inside out: the outer wall (bead 0) last; each bead index in nearest order.
        if (l < model.walls.size()) {
            std::map<int, std::vector<Toolpath>, std::greater<int>> by_index;
            for (const Toolpath& t : model.walls[l].toolpaths)
                by_index[t.index].push_back(t);
            for (const auto& [index, paths] : by_index) {
                const bool outer = index == 0;
                group(paths, outer ? "External perimeter" : "Perimeter",
                      speed(outer ? s.outer_wall_speed.of(s.wall_speed) : s.wall_speed), s.seam);
            }
        }
        if (l < model.fills.size()) {
            const LayerFill& f = model.fills[l];
            group(f.solid, "Solid infill", speed(s.solid_infill_speed), SeamMode::Nearest);
            group(f.top, "Top solid infill", speed(s.top_solid_infill_speed), SeamMode::Nearest);
            group(f.bridge, "Bridge infill", speed(s.solid_infill_speed), SeamMode::Nearest);
            group(f.gaps, "Gap fill", speed(s.solid_infill_speed), SeamMode::Nearest);
            group(f.sparse, "Internal infill", speed(s.infill_speed), SeamMode::Nearest);
        }
    }

    g.type("Custom");
    if (!s.end_gcode.empty())
        out << substitute_variables(s.end_gcode, s.variables) << "\n";
    for (const auto& [key, value] : describe(s))
        out << "; " << key << " = " << value << "\n";
}

} // namespace tisma
