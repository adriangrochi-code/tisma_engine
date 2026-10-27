// Reads the engine's G-code back and checks it. Own design; see
// PROVENANCE.md.

#include "gcode/check.hpp"
#include "adhesion/adhesion.hpp"
#include "gcode/dialect.hpp"

#include "infill/nonplanar.hpp"

#include "config/settings.hpp"
#include "geom/polygons.hpp"
#include "slice/pipeline.hpp"
#include "slice/wipe_tower.hpp"

#include <oneapi/tbb/parallel_pipeline.h>

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <memory>
#include <cmath>
#include <limits>
#include <numbers>
#include <optional>
#include <span>
#include <string>

namespace tisma {
namespace {

constexpr double kPositionSlack = 1e-3; // mm: coordinates are written with 3 decimals
// mm an arc's end may be off the circle its start and centre make: the three
// are written with 3 decimals each.
constexpr double kArcRadiusSlack = 0.01;
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

// A line read apart from what came before it, so that many lines are read at
// once (the speed report's V5); the checker then takes them in order.
struct ParsedLine {
    std::string_view comment; // after the ';'
    bool has_comment = false;
    bool too_many = false;       // more words than a G-code command has
    std::string_view head;       // the command word; empty: no words
    bool readable = false;       // G, M or T with a whole number
    char kind = 0;
    int code = 0;
    std::string_view unreadable; // the first parameter that is not a letter and a number
    std::uint32_t first = 0, count = 0; // the parameters, in the chunk's list
};

struct Parameter {
    char letter;
    double value;
};

void parse_line(std::string_view raw, ParsedLine& out, std::vector<Parameter>& parameters) {
    const std::size_t semicolon = raw.find(';');
    if (semicolon != std::string_view::npos) {
        out.comment = raw.substr(semicolon + 1);
        out.has_comment = true;
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
                out.too_many = true;
                return;
            }
            words[count++] = raw.substr(start, i - start);
        }
    }
    if (count == 0)
        return;
    out.head = words[0];
    out.kind = out.head.front();
    const std::optional<double> code = out.head.size() > 1 ? number(out.head.substr(1)) : std::nullopt;
    out.readable = (out.kind == 'G' || out.kind == 'M' || out.kind == 'T') && code && *code == std::floor(*code);
    if (!out.readable)
        return;
    out.code = int(*code);
    // Parameters: a letter and a number each.
    out.first = std::uint32_t(parameters.size());
    for (std::size_t k = 1; k < count; ++k) {
        const std::string_view w = words[k];
        const char letter = w.front();
        const std::optional<double> v = letter >= 'A' && letter <= 'Z' ? number(w.substr(1)) : std::nullopt;
        if (!v) {
            out.unreadable = w;
            break;
        }
        parameters.push_back({letter, *v});
    }
    out.count = std::uint32_t(parameters.size()) - out.first;
}

class Checker {
public:
    explicit Checker(const GcodeLimits& limits)
        : limits_(limits), retract_length_(limits.retract_length), flow_(limits.flow) {}

    void line(const ParsedLine& p, std::span<const Parameter> parameters) {
        ++line_;
        ++out_.stats.lines;
        if (p.has_comment)
            comment(p.comment);
        // An object's label (gcode_firmware.md, rule 4): its name may hold
        // anything.
        if (p.head == "M486" || p.head.starts_with("EXCLUDE_OBJECT"))
            return;
        if (p.too_many) {
            if (!custom_)
                problem("more words than a G-code command has");
            return;
        }
        if (p.head.empty())
            return;
        command(p, parameters.subspan(p.first, p.count));
    }

    GcodeCheck finish(bool ends_with_newline, bool has_summary) {
        if (!ends_with_newline)
            problem_at(line_, "the file does not end with a line break: it may be cut short");
        if (!has_summary)
            problem_at(line_, "the closing summary is missing: the file is incomplete");
        if (out_.stats.layers == 0)
            problem_at(line_, "no layers");
        if (out_.stats.extrusions == 0 && limits_.e_axis != 0)
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
            if (new_object_) {
                retract_length_ = limits_.retract_length;
                flow_ = limits_.flow;
                next_change_ = 0;
                new_object_ = false;
            }
            // The height changes below this layer's top hold from here on.
            while (next_change_ < limits_.changes.size() && limits_.changes[next_change_].z < *z - 1e-6) {
                retract_length_ = limits_.changes[next_change_].retract_length;
                flow_ = limits_.changes[next_change_].flow;
                ++next_change_;
            }
        } else if (c.starts_with("NEXT_OBJECT:")) {
            // Object by object (sequential.md): the next one starts on the
            // bed again, with the height changes from its bottom at its first
            // layer (the way over to it is still the last one's).
            layer_z_.reset();
            new_object_ = true;
        } else if (c == "NONPLANAR:1" || c == "NONPLANAR:0") {
            nonplanar_ = c == "NONPLANAR:1";
            if (nonplanar_ && !(limits_.wave > 0.0) && !(limits_.scarf > 0.0))
                problem("a nonplanar block, but the settings ask for no wavy infill and no scarf seam");
        } else if (const std::optional<double> h = value("HEIGHT:")) {
            // A nonplanar bead may taper to nothing (the foot of a scarf ramp).
            if (*h < 0.0 || (*h == 0.0 && !nonplanar_))
                problem("layer height " + text(*h) + " is not positive");
            height_ = *h;
            if (!nonplanar_)
                layer_height_ = *h;
        } else if (const std::optional<double> w = value("WIDTH:")) {
            if (*w <= 0.0)
                problem("line width " + text(*w) + " is not positive");
            width_ = *w;
        }
    }

    void command(const ParsedLine& p, std::span<const Parameter> parameters) {
        const char kind = p.kind;
        if (!p.readable) {
            if (!custom_)
                problem("unreadable command '" + std::string(p.head) + "'");
            return; // the user's own G-code may hold anything
        }
        if (!p.unreadable.empty()) {
            if (!custom_)
                problem("unreadable parameter '" + std::string(p.unreadable) + "'");
            return;
        }
        std::optional<double> param[26];
        for (const Parameter& q : parameters)
            param[q.letter - 'A'] = q.value;
        const int n = p.code;
        if (kind == 'T') {
            tool_change(n);
            return;
        }
        if (kind == 'G' && n >= 0 && n <= 3)
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
        else if (kind == 'G' && (n == 10 || n == 11) && parameters.empty() && !custom_)
            firmware_retraction(n == 10);
        else if (kind == 'M' && n == 106 && !custom_) {
            const std::optional<double>& s = param[limits_.fan_parameter - 'A'];
            if (!s || *s < 0.0 || *s > 255.0)
                problem("fan speed out of 0 to 255");
        }
    }

    // Another extruder (multi_extruder.md, rule 5): the leaving one keeps its
    // filament pulled back; the next one's, pulled back when it left, comes
    // back with its own extra.
    void tool_change(int n) {
        if (limits_.toolchange_length.empty() || n < 0)
            return;
        const auto at = [](std::vector<double>& list, std::size_t k) -> double& {
            if (list.size() <= k)
                list.resize(k + 1, 0.0);
            return list[k];
        };
        at(pulled_, tool_) = retracted_;
        tool_ = std::size_t(n);
        retracted_ = at(pulled_, tool_);
        at(pulled_, tool_) = 0.0;
        target_ = retracted_;
        tool_back_ = retracted_ > 1e-9;
        pulling_ = false;
    }

    // A tool change's retraction: standing, up to the leaving tool's
    // retraction.toolchange_length with what a retraction pulled already.
    bool toolchange_pull(double amount) {
        if (limits_.toolchange_length.empty())
            return false;
        const std::vector<double>& lengths = limits_.toolchange_length;
        const double length = lengths[std::min(tool_, lengths.size() - 1)];
        if (std::abs(retracted_ + amount - length) > slack())
            return false;
        if (retracted_ <= 1e-9)
            ++out_.stats.retractions;
        out_.stats.filament -= amount;
        retracted_ = target_ = retracted_ + amount;
        pulling_ = false;
        return true;
    }

    // The firmware's retraction (retraction_travel.md, rule 7): a bare G10
    // pulls back, G11 undoes it, no E moves.
    void firmware_retraction(bool pull) {
        if (!limits_.firmware_retraction)
            problem(std::string(pull ? "G10" : "G11") + " but the settings ask for no firmware retraction");
        if (pull) {
            ++out_.stats.retractions;
            if (retracted_ > 1e-9)
                problem("retracts again without undoing the last retraction");
            retracted_ = target_ = std::max(retract_length_, 1e-6);
        } else {
            if (retracted_ <= 1e-9)
                problem("pushes filament without a retraction to undo");
            retracted_ = 0.0;
            tool_back_ = false;
        }
    }

    // X, Y, Z and the extruder's axis as the dialect names it.
    char axis(int a) const { return a < 3 ? "XYZ"[a] : limits_.e_axis != 0 ? limits_.e_axis : 'E'; }

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
        // An arc (G2 clockwise, G3 counter-clockwise, gcode_extras.md, rule
        // 10): its length along the circle around (I, J) from its start, and
        // the area checked along it.
        std::optional<double> path;
        bool off_area = false;
        if (g >= 2) {
            const std::optional<double>&i = param['I' - 'A'], &j = param['J' - 'A'];
            if (!i && !j) {
                if (!custom_)
                    problem("an arc without I and J");
            } else if (known_[0] && known_[1]) {
                const double cx = position_[0] + i.value_or(0.0), cy = position_[1] + j.value_or(0.0);
                const double r0 = std::hypot(position_[0] - cx, position_[1] - cy), r1 = std::hypot(to[0] - cx, to[1] - cy);
                if (!custom_ && std::abs(r0 - r1) > kArcRadiusSlack)
                    problem("an arc that starts " + text(r0) + " mm from its centre and ends " + text(r1) + " mm from it");
                const double a0 = std::atan2(position_[1] - cy, position_[0] - cx), a1 = std::atan2(to[1] - cy, to[0] - cx);
                double sweep = g == 3 ? a1 - a0 : a0 - a1;
                while (sweep <= 1e-12)
                    sweep += 2.0 * std::numbers::pi; // the same point: a whole turn
                while (sweep > 2.0 * std::numbers::pi)
                    sweep -= 2.0 * std::numbers::pi;
                path = (r0 + r1) / 2.0 * sweep;
                for (int k = 1; k < 16; ++k) {
                    const double a = a0 + (g == 3 ? 1.0 : -1.0) * sweep * k / 16.0;
                    const double x = cx + r0 * std::cos(a), y = cy + r0 * std::sin(a);
                    off_area = off_area || x < limits_.min_x - kPositionSlack || x > limits_.max_x + kPositionSlack ||
                               y < limits_.min_y - kPositionSlack || y > limits_.max_y + kPositionSlack;
                }
            }
        }
        if (!custom_) {
            if (g == 0 && given[3])
                problem("G0 moves the extruder");
            if (off_area)
                problem("an arc to X" + text(to[0]) + " Y" + text(to[1]) + " leaves the allowed area");
            if ((given[0] && (to[0] < limits_.min_x - kPositionSlack || to[0] > limits_.max_x + kPositionSlack)) ||
                (given[1] && (to[1] < limits_.min_y - kPositionSlack || to[1] > limits_.max_y + kPositionSlack)))
                problem("move to X" + text(to[0]) + " Y" + text(to[1]) +
                        " leaves the allowed area (the part with its skirt and brim, on the bed)");
            if (given[2] && (to[2] < limits_.min_z - kPositionSlack || to[2] > limits_.max_z + kPositionSlack))
                problem("move to Z" + text(to[2]) + " is out of " + text(limits_.min_z) + " to " + text(limits_.max_z));
            classify(to, given, path);
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
    // path: an arc's length.
    void classify(const double to[4], const bool given[4], std::optional<double> path) {
        const double de = to[3] - position_[3];
        if (!(de < -1e-9))
            pulling_ = false; // a move that pulls nothing ends a retraction's pieces
        const double dx = to[0] - position_[0], dy = to[1] - position_[1];
        const bool moves = path ? *path > 1e-9 : (given[0] || given[1]) && (std::abs(dx) > 1e-9 || std::abs(dy) > 1e-9);
        const double length = path ? *path : moves && known_[0] && known_[1] ? std::hypot(dx, dy) : 0.0;
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
                // A scarf ramp goes down by the layer's ;HEIGHT, itself rounded.
                const bool zaa = limits_.zaa_below > 0.0 || limits_.zaa_above > 0.0;
                const double slack = zaa || (nonplanar_ && limits_.scarf > 0.0) ? 2.0 * kPositionSlack : kPositionSlack;
                const double low =
                    *layer_z_ - (nonplanar_ ? std::max(limits_.wave, limits_.scarf * layer_height_) : limits_.zaa_below * h) - slack;
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
                // ;HEIGHT, written to 3 decimals, may hide half the last one:
                // much of a bead a few thousandths thick (a scarf ramp's foot,
                // a Z anti-aliased bead on a thin layer).
                const double rounding = *width_ * 0.0005 / limits_.filament_area * flow_ * length;
                if (std::abs(de - expected) > kFlowShare * expected + per_mm * kFlowLength + rounding + 2e-5)
                    problem("extrudes " + text(de) + " mm of filament over " + text(length) + " mm, where width " +
                            text(*width_) + " and height " + text(thickness) + " ask for " + text(expected));
            }
        } else if (moves && de < -1e-9) {
            // With a wipe, part of the retraction comes while the nozzle goes
            // back over its path (retraction_travel.md, rule 3).
            if (!limits_.wipe)
                problem("pulls the filament back during a move");
            else
                pull_back(-de);
        } else if (moves) {
            ++out_.stats.travels;
            out_.stats.travel_path += length;
        } else if (de < -1e-9 && toolchange_pull(-de)) {
            // A tool change's retraction, counted.
        } else if (de < -1e-9) {
            if (limits_.wipe) {
                pull_back(-de);
            } else {
                ++out_.stats.retractions;
                out_.stats.filament += de;
                if (retracted_ > 1e-9)
                    problem("retracts again without undoing the last retraction");
                if (std::abs(-de - retract_length_) > kRetractSlack)
                    problem("retracts " + text(-de) + " mm instead of " + text(retract_length_));
                retracted_ = target_ = -de;
            }
        } else if (de > 1e-9) {
            out_.stats.filament += de;
            // The retraction undone, with retraction.restart_extra more (rule 1).
            if (retracted_ <= 1e-9)
                problem("pushes filament without a retraction to undo");
            else if (limits_.firmware_retraction)
                problem("pushes filament with E after a G10 retraction");
            else if (const double extra = restart_extra(); std::abs(de - retracted_ - extra) > slack())
                problem("undoes " + text(de) + " mm of a " + text(retracted_) + " mm retraction" +
                        (extra > 0.0 ? " and " + text(extra) + " mm more" : ""));
            tool_back_ = false;
            if (limits_.wipe && retracted_ > 1e-9 && std::abs(retracted_ - target_) > slack())
                problem("retracts " + text(retracted_) + " mm instead of " + text(target_));
            retracted_ = 0.0;
            pieces_ = 0;
        }
    }

    // A piece of a retraction with a wipe: standing or while wiping, until
    // the pieces make the retraction's length.
    void pull_back(double amount) {
        out_.stats.filament -= amount;
        if (retracted_ <= 1e-9) {
            ++out_.stats.retractions;
            target_ = retract_length_;
            retracted_ = 0.0;
            pieces_ = 0;
        } else if (!pulling_) {
            problem("retracts again without undoing the last retraction");
        }
        pulling_ = true;
        ++pieces_;
        retracted_ += amount;
        if (retracted_ > target_ + slack())
            problem("retracts " + text(retracted_) + " mm instead of " + text(target_));
    }

    // What comes back past a retraction: a tool's own extra when it comes
    // back from a change (multi_extruder.md, rule 5), else restart_extra.
    double restart_extra() const {
        if (!tool_back_ || limits_.toolchange_restart_extra.empty())
            return limits_.restart_extra;
        const std::vector<double>& extras = limits_.toolchange_restart_extra;
        return extras[std::min(tool_, extras.size() - 1)];
    }

    // How far the pieces of a retraction may add up from its length: E is
    // rounded to 5 decimals in each one.
    double slack() const { return kRetractSlack + 1e-5 * double(pieces_); }

    const GcodeLimits& limits_;
    double retract_length_, flow_;
    std::size_t next_change_ = 0;
    GcodeCheck out_;
    std::size_t line_ = 0;
    bool custom_ = false;
    bool nonplanar_ = false; // in a ;NONPLANAR block
    double layer_height_ = 0.0; // the last ;HEIGHT out of a ;NONPLANAR block: the layer's
    bool absolute_ = true;
    bool relative_e_ = false;
    double position_[4] = {0.0, 0.0, 0.0, 0.0};
    bool known_[3] = {false, false, false};
    double retracted_ = 0.0;
    double target_ = 0.0; // the length of the retraction under way
    std::size_t pieces_ = 0; // its pieces so far (a wipe's)
    bool pulling_ = false;   // the last move was one of them
    std::size_t tool_ = 0;        // the extruder printing, from 0
    std::vector<double> pulled_;  // each extruder's filament pulled back when it left
    bool tool_back_ = false;      // the tool came back from a change: its own extra
    bool awaiting_z_ = false;
    bool new_object_ = false; // after ;NEXT_OBJECT, until the next object's first ;Z:
    std::optional<double> layer_z_, height_, width_;
};

} // namespace

GcodeLimits gcode_limits(const SlicedModel& model, const Settings& given) {
    // The filament's own retraction values, as the writer takes them.
    std::optional<Settings> resolved;
    if (!given.filament_retraction.empty())
        resolved = with_filament_retraction(given);
    const Settings& s = resolved ? *resolved : given;
    GcodeLimits limits;
    double lo[2] = {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity()};
    double hi[2] = {-lo[0], -lo[1]};
    // The part and its support (tree supports lean out past the part).
    for (const std::vector<Polygons>* areas : {&model.regions, &model.support_areas})
        for (const Polygons& region : *areas)
            for (const Polygon& polygon : region)
                for (const auto& p : polygon) {
                    lo[0] = std::min(lo[0], to_mm(p.x)), hi[0] = std::max(hi[0], to_mm(p.x));
                    lo[1] = std::min(lo[1], to_mm(p.y)), hi[1] = std::max(hi[1], to_mm(p.y));
                }
    // The wipe tower with its brim (multi_extruder.md, rule 10).
    for (const auto& p : plan_wipe_tower(model, s, plan_tools(model, s)).outline) {
        lo[0] = std::min(lo[0], to_mm(p.x)), hi[0] = std::max(hi[0], to_mm(p.x));
        lo[1] = std::min(lo[1], to_mm(p.y)), hi[1] = std::max(hi[1], to_mm(p.y));
    }
    // Around them: the brim, the skirt beyond it, and slack for half a bead
    // and the travels that go round the outside.
    // The brim and skirt are mitred offsets (offset(), miter limit 2): at a
    // sharp corner they reach out up to twice their distance.
    double skirt = s.skirt_loops > 0 ? s.skirt_distance + s.skirt_loops * s.wall_width : 0.0;
    if (s.skirt_loops > 0 && s.skirt_min_filament > 0.0) {
        // The loops added for the skirt's filament (skirt_brim_raft.md, rule
        // 1): each at least as long as a circle of the skirt's distance.
        const double area = std::numbers::pi * s.filament_diameter * s.filament_diameter / 4.0;
        const double path = s.skirt_min_filament * area / std::max(s.wall_width * s.first_layer_height, 1e-9);
        const double shortest = 2.0 * std::numbers::pi * std::max(s.skirt_distance, 0.5 * s.wall_width);
        skirt += std::min(double(kMaxSkirtLoops), std::ceil(path / shortest)) * s.wall_width;
    }
    const double margin = 2.0 * (s.brim_width + s.brim_gap + skirt) + 2.0 * s.wall_width + 1.0;
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
    // A ramping lift climbs to travel.max_lift (retraction_travel.md, rule 5).
    const double lift = s.ramping_lift && s.travel_slope > 0.0 && s.max_lift > 0.0 ? s.max_lift : s.retract_lift;
    limits.max_z = (model.layers.empty() ? 0.0 : model.layers.back().top - model.layers.front().bottom) + lift;
    // machine.z_offset moves every Z (gcode_extras.md, rule 5), below the bed too.
    limits.max_z += std::max(0.0, s.z_offset);
    limits.min_z = std::min(0.0, s.z_offset);
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
    if (s.scarf != ScarfMode::Nowhere)
        limits.scarf = 1.0 - s.scarf_start_height / 100.0;
    if (s.vase) // a turn starts at the layer below's top
        limits.scarf = 1.0;
    if (const std::optional<WaveParams> waves = wave_params(s)) {
        // The crests, and the travels over them, go up to half the amplitude higher.
        limits.wave = 0.5 * waves->amplitude;
        limits.max_z += limits.wave;
    }
    double speed_factor = 1.0, retract_speed = std::max(s.retract_speed, s.unretract_speed);
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
        // The ironing's lines are beads spacing x flow wide (ironing.md, rule 3).
        const double ironing = std::max(0.001, std::round(s.ironing_spacing * s.ironing_flow / 100.0 * 1000.0) / 1000.0);
        const double narrow = s.ironing ? std::min(0.25 * s.wall_width, ironing) : 0.25 * s.wall_width;
        const double machine = std::max(s.max_feed_x, s.max_feed_y); // 0: not known, no cap
        // A scarf ramp's foot or a Z anti-aliased bead is thinner than any
        // layer: up to what the machine moves.
        const bool thinner = s.scarf != ScarfMode::Nowhere || s.zaa;
        const double needed = thinner ? std::numeric_limits<double>::infinity()
                                      : forced / std::max(narrow * height * min_flow, 1e-9);
        limits.max_feed = std::max(limits.max_feed, machine > 0.0 ? std::min(machine, needed) : needed);
    }
    const double r = s.filament_diameter / 2.0;
    // With volumetric E every E is in mm3 (gcode_extras.md, rule 11): the
    // section of the filament is 1, and the retractions are its volume.
    const double area = std::numbers::pi * r * r, e_scale = s.volumetric_e ? area : 1.0;
    limits.filament_area = s.volumetric_e ? 1.0 : area;
    limits.retract_length = s.retract_length * e_scale;
    limits.restart_extra = s.restart_extra * e_scale;
    limits.wipe = s.wipe;
    limits.firmware_retraction = s.firmware_retraction;
    if (s.extruders > 1) {
        for (int e = 0; e < s.extruders; ++e) {
            limits.toolchange_length.push_back(per_extruder(s.toolchange_length, std::size_t(e), 10.0) * e_scale);
            limits.toolchange_restart_extra.push_back(per_extruder(s.toolchange_restart_extra, std::size_t(e)) * e_scale);
        }
        // Each nozzle's offset comes off its X and Y (rule 8).
        double off_lo[2] = {0.0, 0.0}, off_hi[2] = {0.0, 0.0};
        for (const auto& o : s.extruder_offset)
            for (int a = 0; a < 2; ++a)
                off_lo[a] = std::min(off_lo[a], o[std::size_t(a)]), off_hi[a] = std::max(off_hi[a], o[std::size_t(a)]);
        limits.min_x -= off_hi[0], limits.max_x -= off_lo[0];
        limits.min_y -= off_hi[1], limits.max_y -= off_lo[1];
    }
    limits.flow = s.flow / 100.0;
    GcodeLimits::Change current{0.0, s.retract_length * e_scale, s.flow / 100.0};
    for (const HeightChange& c : s.changes) {
        current.z = c.z;
        if (c.retract_length)
            current.retract_length = *c.retract_length * e_scale;
        if (c.flow)
            current.flow = *c.flow / 100.0;
        limits.changes.push_back(current);
    }
    limits.relative_extrusion = relative_e(s);
    const Dialect dialect = dialect_of(s.flavor);
    limits.e_axis = dialect.e_axis;
    limits.fan_parameter = dialect.fan == Dialect::Fan::M106P ? 'P' : 'S';
    return limits;
}

GcodeCheck check_gcode(std::string_view text, const GcodeLimits& limits) {
    Checker checker(limits);
    bool summary = false;
    // In chunks of whole lines: read in parallel, checked in order, a few
    // chunks in flight at a time.
    struct Chunk {
        std::string_view text;
        std::vector<ParsedLine> lines;
        std::vector<Parameter> parameters;
        bool summary = false;
    };
    constexpr std::size_t kChunk = 256 * 1024;
    std::size_t start = 0;
    tbb::parallel_pipeline(
        16,
        tbb::make_filter<void, std::shared_ptr<Chunk>>(
            tbb::filter_mode::serial_in_order,
            [&](tbb::flow_control& control) -> std::shared_ptr<Chunk> {
                if (start >= text.size()) {
                    control.stop();
                    return nullptr;
                }
                std::size_t end = std::min(start + kChunk, text.size());
                if (end < text.size()) {
                    const std::size_t newline = text.find('\n', end);
                    end = newline == std::string_view::npos ? text.size() : newline + 1;
                }
                auto chunk = std::make_shared<Chunk>();
                chunk->text = text.substr(start, end - start);
                start = end;
                return chunk;
            }) &
            tbb::make_filter<std::shared_ptr<Chunk>, std::shared_ptr<Chunk>>(
                tbb::filter_mode::parallel,
                [](std::shared_ptr<Chunk> chunk) {
                    const std::string_view piece = chunk->text;
                    std::size_t at = 0;
                    while (at < piece.size()) {
                        std::size_t end = piece.find('\n', at);
                        if (end == std::string_view::npos)
                            end = piece.size();
                        const std::string_view line = piece.substr(at, end - at);
                        chunk->summary = chunk->summary || line.starts_with("; estimated printing time = ");
                        parse_line(line, chunk->lines.emplace_back(), chunk->parameters);
                        at = end + 1;
                    }
                    return chunk;
                }) &
            tbb::make_filter<std::shared_ptr<Chunk>, void>(tbb::filter_mode::serial_in_order,
                                                          [&](std::shared_ptr<Chunk> chunk) {
                                                              summary = summary || chunk->summary;
                                                              for (const ParsedLine& p : chunk->lines)
                                                                  checker.line(p, chunk->parameters);
                                                          }));
    return checker.finish(!text.empty() && text.back() == '\n', summary);
}

} // namespace tisma
