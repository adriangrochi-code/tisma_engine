// Reads the engine's G-code back and checks it. Own design; see
// PROVENANCE.md.

#include "gcode/check.hpp"

#include "infill/nonplanar.hpp"

#include "config/settings.hpp"
#include "geom/polygons.hpp"
#include "slice/pipeline.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <numbers>
#include <optional>
#include <span>
#include <string>

namespace tisma {
namespace {

constexpr double kPositionSlack = 1e-3; // mm: coordinates are written with 3 decimals
constexpr double kRetractSlack = 1e-4;  // mm of filament: E is written with 5 decimals
constexpr double kFlowShare = 0.02;     // a move's E may differ from width x height x length by this share...
constexpr double kFlowLength = 0.005;   // ...plus the E of this much path (pieces too short to write ride along)

std::string text(double v) {
    char buffer[32];
    const auto end = std::to_chars(buffer, buffer + sizeof buffer, v, std::chars_format::general, 6).ptr;
    return std::string(buffer, end);
}

// A number in G-code: digits with an optional sign and point. Not NaN, not
// infinite, nothing left over.
std::optional<double> number(std::string_view t) {
    if (t.empty())
        return std::nullopt;
    if (t.front() == '+')
        t.remove_prefix(1);
    double v = 0.0;
    const auto [end, ec] = std::from_chars(t.data(), t.data() + t.size(), v, std::chars_format::fixed);
    if (ec != std::errc() || end != t.data() + t.size() || !std::isfinite(v))
        return std::nullopt;
    return v;
}

class Checker {
public:
    explicit Checker(const GcodeLimits& limits)
        : limits_(limits), retract_length_(limits.retract_length), flow_(limits.flow) {}

    void line(std::string_view raw) {
        ++line_;
        ++out_.stats.lines;
        const std::size_t semicolon = raw.find(';');
        if (semicolon != std::string_view::npos) {
            comment(raw.substr(semicolon + 1));
            raw = raw.substr(0, semicolon);
        }
        // Words separated by spaces or tabs.
        std::string_view words[16];
        std::size_t count = 0;
        std::size_t i = 0;
        while (i < raw.size()) {
            while (i < raw.size() && (raw[i] == ' ' || raw[i] == '\t' || raw[i] == '\r'))
                ++i;
            const std::size_t start = i;
            while (i < raw.size() && raw[i] != ' ' && raw[i] != '\t' && raw[i] != '\r')
                ++i;
            if (i > start) {
                if (count == std::size(words)) {
                    if (!custom_)
                        problem("more words than a G-code command has");
                    return;
                }
                words[count++] = raw.substr(start, i - start);
            }
        }
        if (count == 0)
            return;
        command(std::span(words, count));
    }

    GcodeCheck finish(bool ends_with_newline, bool has_summary) {
        if (!ends_with_newline)
            problem_at(line_, "the file does not end with a line break: it may be cut short");
        if (!has_summary)
            problem_at(line_, "the closing summary is missing: the file is incomplete");
        if (out_.stats.layers == 0)
            problem_at(line_, "no layers");
        if (out_.stats.extrusions == 0)
            problem_at(line_, "nothing is extruded");
        return std::move(out_);
    }

private:
    void problem(const std::string& message) { problem_at(line_, message); }
    void problem_at(std::size_t line, const std::string& message) {
        if (out_.problems.size() < kMaxProblems)
            out_.problems.push_back({line, message});
        ++out_.problem_count;
    }

    // The engine's comments carry the layer, the line type and the width.
    void comment(std::string_view c) {
        auto value = [&](std::string_view key) -> std::optional<double> {
            if (!c.starts_with(key))
                return std::nullopt;
            const std::optional<double> v = number(c.substr(key.size()));
            if (!v)
                problem("unreadable value in ;" + std::string(c));
            return v;
        };
        if (c.starts_with("TYPE:")) {
            custom_ = c.substr(5) == "Custom";
        } else if (c == "LAYER_CHANGE") {
            ++out_.stats.layers;
            awaiting_z_ = true;
        } else if (const std::optional<double> z = value("Z:")) {
            if (!awaiting_z_)
                problem(";Z: outside a layer change");
            if (layer_z_ && *z <= *layer_z_ + 1e-9)
                problem("layer at Z=" + text(*z) + " does not go above the one before (" + text(*layer_z_) + ")");
            layer_z_ = *z;
            awaiting_z_ = false;
            // The height changes below this layer's top hold from here on.
            while (next_change_ < limits_.changes.size() && limits_.changes[next_change_].z < *z - 1e-6) {
                retract_length_ = limits_.changes[next_change_].retract_length;
                flow_ = limits_.changes[next_change_].flow;
                ++next_change_;
            }
        } else if (c == "NONPLANAR:1" || c == "NONPLANAR:0") {
            nonplanar_ = c == "NONPLANAR:1";
            if (nonplanar_ && !(limits_.wave > 0.0))
                problem("a nonplanar block, but the settings ask for no wavy infill");
        } else if (const std::optional<double> h = value("HEIGHT:")) {
            if (*h <= 0.0)
                problem("layer height " + text(*h) + " is not positive");
            height_ = *h;
        } else if (const std::optional<double> w = value("WIDTH:")) {
            if (*w <= 0.0)
                problem("line width " + text(*w) + " is not positive");
            width_ = *w;
        }
    }

    void command(std::span<const std::string_view> words) {
        const std::string_view head = words.front();
        const char kind = head.front();
        const std::optional<double> code = head.size() > 1 ? number(head.substr(1)) : std::nullopt;
        if ((kind != 'G' && kind != 'M' && kind != 'T') || !code || *code != std::floor(*code)) {
            if (!custom_)
                problem("unreadable command '" + std::string(head) + "'");
            return; // the user's own G-code may hold anything
        }
        // Parameters: a letter and a number each.
        std::optional<double> param[26];
        for (std::size_t i = 1; i < words.size(); ++i) {
            const std::string_view w = words[i];
            const char letter = w.front();
            const std::optional<double> v = letter >= 'A' && letter <= 'Z' ? number(w.substr(1)) : std::nullopt;
            if (!v) {
                if (!custom_)
                    problem("unreadable parameter '" + std::string(w) + "'");
                return;
            }
            param[letter - 'A'] = v;
        }
        const int n = int(*code);
        if (kind == 'G' && (n == 0 || n == 1))
            move(n, param);
        else if (kind == 'G' && n == 90)
            absolute_ = true;
        else if (kind == 'G' && n == 91)
            absolute_ = false;
        else if (kind == 'M' && n == 82)
            relative_e_ = false;
        else if (kind == 'M' && n == 83)
            relative_e_ = true;
        else if (kind == 'G' && n == 92)
            for (int a = 0; a < 4; ++a) {
                if (const std::optional<double>& v = param[axis(a) - 'A'])
                    position_[a] = *v;
            }
        else if (kind == 'G' && n == 28)
            known_[0] = known_[1] = known_[2] = false; // homed: where exactly, the printer knows
        else if (kind == 'M' && n == 106 && !custom_) {
            const std::optional<double>& s = param['S' - 'A'];
            if (!s || *s < 0.0 || *s > 255.0)
                problem("fan speed out of 0 to 255");
        }
    }

    static char axis(int a) { return "XYZE"[a]; }

    void move(int g, const std::optional<double> (&param)[26]) {
        double to[4];
        bool given[4];
        for (int a = 0; a < 4; ++a) {
            const std::optional<double>& v = param[axis(a) - 'A'];
            given[a] = v.has_value();
            const bool relative = a == 3 ? relative_e_ : !absolute_;
            to[a] = !v ? position_[a] : relative ? position_[a] + *v : *v;
        }
        if (const std::optional<double>& f = param['F' - 'A']) {
            if (*f <= 0.0)
                problem("feed rate F" + text(*f) + " is not positive");
            else if (!custom_ && *f / 60.0 > limits_.max_feed * (1.0 + 1e-9) + 1.0 / 60.0)
                problem("feed rate " + text(*f / 60.0) + " mm/s is above the fastest allowed, " +
                        text(limits_.max_feed) + " mm/s");
        }
        if (!custom_) {
            if (g == 0 && given[3])
                problem("G0 moves the extruder");
            if ((given[0] && (to[0] < limits_.min_x - kPositionSlack || to[0] > limits_.max_x + kPositionSlack)) ||
                (given[1] && (to[1] < limits_.min_y - kPositionSlack || to[1] > limits_.max_y + kPositionSlack)))
                problem("move to X" + text(to[0]) + " Y" + text(to[1]) +
                        " leaves the allowed area (the part with its skirt and brim, on the bed)");
            if (given[2] && (to[2] < -kPositionSlack || to[2] > limits_.max_z + kPositionSlack))
                problem("move to Z" + text(to[2]) + " is out of 0 to " + text(limits_.max_z));
            classify(to, given);
        }
        for (int a = 0; a < 4; ++a) {
            if (given[a] && a < 3)
                known_[a] = absolute_ || known_[a];
            position_[a] = to[a];
        }
        if (known_[2])
            out_.stats.max_z = std::max(out_.stats.max_z, position_[2]);
    }

    // What an engine move is, by its E: an extrusion, a travel, a retraction
    // or the move that undoes one.
    void classify(const double to[4], const bool given[4]) {
        const double de = to[3] - position_[3];
        const double dx = to[0] - position_[0], dy = to[1] - position_[1];
        const bool moves = (given[0] || given[1]) && (std::abs(dx) > 1e-9 || std::abs(dy) > 1e-9);
        const double length = moves && known_[0] && known_[1] ? std::hypot(dx, dy) : 0.0;
        if (moves && de > 1e-9) {
            ++out_.stats.extrusions;
            out_.stats.extruded_path += length;
            out_.stats.filament += de;
            if (retracted_ > 1e-9)
                problem("extrudes while the filament is retracted");
            const bool at_layer = layer_z_ && known_[2];
            if (at_layer) {
                const double h = height_.value_or(0.0);
                // Off the layer's Z (Z anti-aliasing) the bound comes from the
                // layer's true top, and both it and the move's Z are rounded.
                const bool zaa = limits_.zaa_below > 0.0 || limits_.zaa_above > 0.0;
                const double slack = zaa ? 2.0 * kPositionSlack : kPositionSlack;
                const double low = *layer_z_ - (nonplanar_ ? limits_.wave : limits_.zaa_below * h) - slack;
                const double high = *layer_z_ + (nonplanar_ ? limits_.wave : limits_.zaa_above * h) + slack;
                for (const double z : {position_[2], to[2]})
                    if (z < low || z > high) {
                        problem("extrudes at Z" + text(z) + ", not at the layer's Z" + text(*layer_z_));
                        break;
                    }
            }
            if (!width_ || !height_) {
                problem("extrudes before any ;WIDTH and ;HEIGHT");
            } else if (known_[0] && known_[1]) {
                // Off the layer's Z (Z anti-aliasing), the bead is what is left down to its bottom.
                // In a nonplanar block, ;HEIGHT is the bead's own thickness.
                const double thickness =
                    *height_ + (at_layer && !nonplanar_ ? ((position_[2] - *layer_z_) + (to[2] - *layer_z_)) / 2.0 : 0.0);
                const double per_mm = *width_ * thickness / limits_.filament_area * flow_;
                const double expected = per_mm * length;
                if (std::abs(de - expected) > kFlowShare * expected + per_mm * kFlowLength + 2e-5)
                    problem("extrudes " + text(de) + " mm of filament over " + text(length) + " mm, where width " +
                            text(*width_) + " and height " + text(thickness) + " ask for " + text(expected));
            }
        } else if (moves && de < -1e-9) {
            problem("pulls the filament back during a move");
        } else if (moves) {
            ++out_.stats.travels;
            out_.stats.travel_path += length;
        } else if (de < -1e-9) {
            ++out_.stats.retractions;
            out_.stats.filament += de;
            if (retracted_ > 1e-9)
                problem("retracts again without undoing the last retraction");
            if (std::abs(-de - retract_length_) > kRetractSlack)
                problem("retracts " + text(-de) + " mm instead of " + text(retract_length_));
            retracted_ = -de;
        } else if (de > 1e-9) {
            out_.stats.filament += de;
            if (retracted_ <= 1e-9)
                problem("pushes filament without a retraction to undo");
            else if (std::abs(de - retracted_) > kRetractSlack)
                problem("undoes " + text(de) + " mm of a " + text(retracted_) + " mm retraction");
            retracted_ = 0.0;
        }
    }

    const GcodeLimits& limits_;
    double retract_length_, flow_;
    std::size_t next_change_ = 0;
    GcodeCheck out_;
    std::size_t line_ = 0;
    bool custom_ = false;
    bool nonplanar_ = false; // in a ;NONPLANAR block
    bool absolute_ = true;
    bool relative_e_ = false;
    double position_[4] = {0.0, 0.0, 0.0, 0.0};
    bool known_[3] = {false, false, false};
    double retracted_ = 0.0;
    bool awaiting_z_ = false;
    std::optional<double> layer_z_, height_, width_;
};

} // namespace

GcodeLimits gcode_limits(const SlicedModel& model, const Settings& s) {
    GcodeLimits limits;
    double lo[2] = {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity()};
    double hi[2] = {-lo[0], -lo[1]};
    for (const Polygons& region : model.regions)
        for (const Polygon& polygon : region)
            for (const auto& p : polygon) {
                lo[0] = std::min(lo[0], to_mm(p.x)), hi[0] = std::max(hi[0], to_mm(p.x));
                lo[1] = std::min(lo[1], to_mm(p.y)), hi[1] = std::max(hi[1], to_mm(p.y));
            }
    // Around the part: the brim, the skirt beyond it, and slack for half a bead
    // and the travels that go round the outside.
    // The brim and skirt are mitred offsets (offset(), miter limit 2): at a
    // sharp corner they reach out up to twice their distance.
    const double skirt = s.skirt_loops > 0 ? s.skirt_distance + s.skirt_loops * s.wall_width : 0.0;
    const double margin = 2.0 * (s.brim_width + skirt) + 2.0 * s.wall_width + 1.0;
    limits.min_x = lo[0] - margin, limits.max_x = hi[0] + margin;
    limits.min_y = lo[1] - margin, limits.max_y = hi[1] + margin;
    if (!s.bed.empty()) { // and never off the bed
        double bed_lo[2] = {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity()};
        double bed_hi[2] = {-bed_lo[0], -bed_lo[1]};
        for (const auto& p : s.bed)
            for (int a = 0; a < 2; ++a)
                bed_lo[a] = std::min(bed_lo[a], p[std::size_t(a)]), bed_hi[a] = std::max(bed_hi[a], p[std::size_t(a)]);
        limits.min_x = std::max(limits.min_x, bed_lo[0]), limits.max_x = std::min(limits.max_x, bed_hi[0]);
        limits.min_y = std::max(limits.min_y, bed_lo[1]), limits.max_y = std::min(limits.max_y, bed_hi[1]);
    }
    // Heights count from the part's bottom, which rests on the bed (write_gcode).
    limits.max_z = (model.layers.empty() ? 0.0 : model.layers.back().top - model.layers.front().bottom) + s.retract_lift;
    if (s.zaa) {
        limits.zaa_below = 1.0 - s.zaa_min_thickness / 100.0;
        limits.zaa_above = s.zaa_raise ? 0.5 : 0.0;
        // Raised beads, and the travels over them, go up to half a layer higher.
        double thickest = 0.0;
        for (const Layer& layer : model.layers)
            thickest = std::max(thickest, layer.top - layer.bottom);
        for (const ThickLayer& t : model.thick)
            thickest = std::max(thickest, t.height);
        limits.max_z += limits.zaa_above * thickest;
    }
    if (const std::optional<WaveParams> waves = wave_params(s)) {
        // The crests, and the travels over them, go up to half the amplitude higher.
        limits.wave = 0.5 * waves->amplitude;
        limits.max_z += limits.wave;
    }
    double speed_factor = 1.0, retract_speed = s.retract_speed;
    for (const HeightChange& c : s.changes) {
        speed_factor = std::max(speed_factor, c.speed.value_or(100.0) / 100.0);
        retract_speed = std::max(retract_speed, c.retract_speed.value_or(0.0));
    }
    limits.max_feed = std::max({s.travel_speed, fastest_extrusion(s) * speed_factor, retract_speed});
    // A forced flow (a flow tower) runs its lines as fast as that flow needs
    // on the narrowest bead and thinnest layer, up to what the machine moves.
    double forced = 0.0, min_flow = s.flow / 100.0;
    for (const HeightChange& c : s.changes) {
        forced = std::max(forced, c.volumetric_flow.value_or(0.0));
        min_flow = std::min(min_flow, c.flow.value_or(s.flow) / 100.0);
    }
    if (forced > 0.0) {
        double height = s.layer_height;
        for (const Layer& layer : model.layers)
            height = std::min(height, layer.top - layer.bottom);
        const double narrow = 0.25 * s.wall_width;
        const double machine = std::max(s.max_feed_x, s.max_feed_y); // 0: not known, no cap
        const double needed = forced / std::max(narrow * height * min_flow, 1e-9);
        limits.max_feed = std::max(limits.max_feed, machine > 0.0 ? std::min(machine, needed) : needed);
    }
    const double r = s.filament_diameter / 2.0;
    limits.filament_area = std::numbers::pi * r * r;
    limits.retract_length = s.retract_length;
    limits.flow = s.flow / 100.0;
    GcodeLimits::Change current{0.0, s.retract_length, s.flow / 100.0};
    for (const HeightChange& c : s.changes) {
        current.z = c.z;
        if (c.retract_length)
            current.retract_length = *c.retract_length;
        if (c.flow)
            current.flow = *c.flow / 100.0;
        limits.changes.push_back(current);
    }
    limits.relative_extrusion = s.relative_extrusion;
    return limits;
}

GcodeCheck check_gcode(std::string_view text, const GcodeLimits& limits) {
    Checker checker(limits);
    bool summary = false;
    std::size_t start = 0;
    while (start < text.size()) {
        std::size_t end = text.find('\n', start);
        if (end == std::string_view::npos)
            end = text.size();
        const std::string_view line = text.substr(start, end - start);
        summary = summary || line.starts_with("; estimated printing time = ");
        checker.line(line);
        start = end + 1;
    }
    return checker.finish(!text.empty() && text.back() == '\n', summary);
}

} // namespace tisma
