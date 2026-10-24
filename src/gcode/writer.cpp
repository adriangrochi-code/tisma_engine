// G-code output (docs/spec/GCODE_OUTPUT.md). Own design; see PROVENANCE.md.

#include "gcode/writer.hpp"

#include <algorithm>
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
    Emitter(std::ostream& out, const Settings& s) : out_(out), relative_(s.relative_extrusion) {
        const double r = s.filament_diameter / 2.0;
        filament_area_ = std::numbers::pi * r * r;
    }

    void comment(const std::string& line) { out_ << line << "\n"; }

    void new_layer(double z, double height, double travel_feed) {
        out_ << ";LAYER_CHANGE\n;Z:" << fixed(z, 3) << "\n;HEIGHT:" << fixed(height, 3) << "\n";
        if (!relative_) {
            out_ << "G92 E0\n";
            e_ = 0.0;
        }
        move("G1", std::nullopt, std::nullopt, z, std::nullopt, travel_feed);
        height_ = height;
    }

    void type(const char* t) {
        if (type_ != t) {
            out_ << ";TYPE:" << t << "\n";
            type_ = t;
        }
    }

    void travel(const Point2d& p, double feed) { move("G1", p.x, p.y, std::nullopt, std::nullopt, feed); }

    void extrude(const ToolpathPoint& from, const ToolpathPoint& to, double feed) {
        const double width = (from.width + to.width) / 2.0;
        const std::string w = fixed(width, 3);
        if (w != width_) {
            out_ << ";WIDTH:" << w << "\n";
            width_ = w;
        }
        const double length = std::hypot(to.p.x - from.p.x, to.p.y - from.p.y);
        const double e = width * height_ * length / filament_area_;
        e_ = relative_ ? e : e_ + e;
        total_e_ += e;
        move("G1", to.p.x, to.p.y, std::nullopt, e_, feed);
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
    double height_ = 0.2;
    double e_ = 0.0, total_e_ = 0.0;
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
        // Walls from the inside out: the outer wall (bead 0) last.
        if (l < model.walls.size()) {
            std::vector<const Toolpath*> walls;
            for (const Toolpath& t : model.walls[l].toolpaths)
                walls.push_back(&t);
            std::stable_sort(walls.begin(), walls.end(), [](const Toolpath* a, const Toolpath* b) { return a->index > b->index; });
            for (const Toolpath* t : walls) {
                const bool outer = t->index == 0;
                lay(*t, outer ? "External perimeter" : "Perimeter",
                    speed(outer ? s.outer_wall_speed.of(s.wall_speed) : s.wall_speed));
            }
        }
        if (l < model.fills.size()) {
            const LayerFill& f = model.fills[l];
            for (const Toolpath& t : f.solid)
                lay(t, "Solid infill", speed(s.solid_infill_speed));
            for (const Toolpath& t : f.top)
                lay(t, "Top solid infill", speed(s.top_solid_infill_speed));
            for (const Toolpath& t : f.bridge)
                lay(t, "Bridge infill", speed(s.solid_infill_speed));
            for (const Toolpath& t : f.gaps)
                lay(t, "Gap fill", speed(s.solid_infill_speed));
            for (const Toolpath& t : f.sparse)
                lay(t, "Internal infill", speed(s.infill_speed));
        }
    }

    g.type("Custom");
    if (!s.end_gcode.empty())
        out << substitute_variables(s.end_gcode, s.variables) << "\n";
    for (const auto& [key, value] : describe(s))
        out << "; " << key << " = " << value << "\n";
}

} // namespace tisma
