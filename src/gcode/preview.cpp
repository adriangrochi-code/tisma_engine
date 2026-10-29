// Reads G-code into moves for display. Own design; see PROVENANCE.md.

#include "gcode/preview.hpp"

#include "geom/bead.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <numbers>
#include <optional>
#include <vector>

#include <oneapi/tbb/parallel_for.h>

namespace tisma {
namespace {

constexpr double kExtrudeMin = 1e-7; // mm of filament: less than this does not count as extruding
constexpr double kNewZ = 1e-4;       // mm: a Z this far from the layer's starts a new one (files without comments)

struct KindName {
    std::string_view type;
    LineKind kind;
    const char* id;
};

// The ";TYPE:" words of docs/spec/GCODE_OUTPUT.md, and the ids, in LineKind order.
constexpr KindName kKinds[kLineKinds] = {
    {"", LineKind::Travel, "travel"},
    {"External perimeter", LineKind::OuterWall, "outer_wall"},
    {"Perimeter", LineKind::InnerWall, "inner_wall"},
    {"Overhang perimeter", LineKind::OverhangWall, "overhang_wall"},
    {"Internal infill", LineKind::SparseInfill, "sparse_infill"},
    {"Solid infill", LineKind::SolidInfill, "solid_infill"},
    {"Top solid infill", LineKind::TopSolidInfill, "top_solid_infill"},
    {"Bridge infill", LineKind::BridgeInfill, "bridge_infill"},
    {"Gap fill", LineKind::GapFill, "gap_fill"},
    {"Ironing", LineKind::Ironing, "ironing"},
    {"Skirt/Brim", LineKind::SkirtBrim, "skirt_brim"},
    {"Support material", LineKind::Support, "support"},
    {"Support material interface", LineKind::SupportInterface, "support_interface"},
    {"Wipe tower", LineKind::WipeTower, "wipe_tower"},
    {"Custom", LineKind::Custom, "custom"},
    {"", LineKind::Other, "other"},
};

std::string_view trim(std::string_view t) {
    while (!t.empty() && (t.front() == ' ' || t.front() == '\t'))
        t.remove_prefix(1);
    while (!t.empty() && (t.back() == ' ' || t.back() == '\t' || t.back() == '\r'))
        t.remove_suffix(1);
    return t;
}

std::optional<double> number(std::string_view t) {
    t = trim(t);
    if (!t.empty() && t.front() == '+')
        t.remove_prefix(1);
    double v = 0.0;
    const auto [end, ec] = std::from_chars(t.data(), t.data() + t.size(), v, std::chars_format::general);
    if (ec != std::errc() || !std::isfinite(v))
        return std::nullopt;
    return v;
}

struct Words {
    bool present[26] = {};
    std::optional<double> value[26];
};

class Reader {
public:
    Reader(double filament_diameter, char extruder_axis)
        : filament_area_(std::numbers::pi * 0.25 * filament_diameter * filament_diameter), e_axis_(extruder_axis) {}

    void line(std::string_view raw) {
        ++out_.lines;
        const std::size_t semicolon = raw.find(';');
        if (semicolon != std::string_view::npos) {
            comment(trim(raw.substr(semicolon + 1)));
            raw = raw.substr(0, semicolon);
        }
        // Words are a letter and a number, with or without spaces between words.
        // A letter alone (G28 X) is present without a value.
        Words word;
        char head = 0;
        double code = -1.0;
        std::size_t i = 0;
        while (i < raw.size()) {
            const char c = raw[i];
            if (!std::isalpha(static_cast<unsigned char>(c))) {
                ++i;
                continue;
            }
            const char letter = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            std::size_t j = i + 1;
            while (j < raw.size() && (std::isdigit(static_cast<unsigned char>(raw[j])) || raw[j] == '.' ||
                                      raw[j] == '-' || raw[j] == '+'))
                ++j;
            const std::optional<double> v = number(raw.substr(i + 1, j - i - 1));
            if (head == 0) {
                if (letter == 'N') { // a line number
                    i = j;
                    continue;
                }
                if (letter == 'T') { // the extruder from now on
                    if (v && *v >= 0.0 && *v < 256.0 && static_cast<std::uint8_t>(*v) != tool_) {
                        if (tool_seen_)
                            mark(MarkKind::ToolChange);
                        tool_ = static_cast<std::uint8_t>(*v);
                    }
                    tool_seen_ = true;
                    return;
                }
                if (letter != 'G' && letter != 'M')
                    return; // anything else: nothing to draw
                head = letter;
                code = v.value_or(-1.0);
            } else {
                word.present[letter - 'A'] = true;
                word.value[letter - 'A'] = v;
            }
            i = j;
        }
        if (head == 'G')
            g(code, word);
        else if (head == 'M')
            m(code, word);
    }

    GcodePreview finish() {
        if (out_.extrusions.empty())
            out_.min = out_.max = {0, 0, 0};
        return std::move(out_);
    }

private:
    void comment(std::string_view c) {
        if (c.starts_with("TYPE:")) {
            const std::string_view type = trim(c.substr(5));
            kind_ = line_kind(type);
            out_.typed = true;
            // The user's G-code, once the print has started.
            if (kind_ == LineKind::Custom && !out_.extrusions.empty())
                mark(MarkKind::Custom);
            seam_open_ = false;
        } else if (c == "COLOR_CHANGE") {
            colour_comment_ = true;
            mark(MarkKind::ColorChange);
        } else if (c == "PAUSE_PRINT") {
            pause_comment_ = true;
            mark(MarkKind::Pause);
        } else if (c == "CUSTOM_GCODE") {
            mark(MarkKind::Custom);
        } else if (c == "LAYER_CHANGE") {
            layer_comments_ = true;
            // A layer of only travels (the start G-code before the first
            // layer) is taken over instead of left empty.
            if (out_.layers.empty() || out_.layers.back().extrusion_count > 0)
                new_layer(z_);
        } else if (c.starts_with("Z:")) {
            if (const std::optional<double> v = number(c.substr(2)); v && !out_.layers.empty())
                out_.layers.back().z = *v;
        } else if (c == "NONPLANAR:1" || c == "NONPLANAR:0") {
            nonplanar_ = c == "NONPLANAR:1";
        } else if (c.starts_with("HEIGHT:")) {
            if (const std::optional<double> v = number(c.substr(7)); v && *v > 0.0) {
                height_ = *v;
                if (!out_.layers.empty())
                    out_.layers.back().height = *v;
            }
        } else if (c.starts_with("estimated printing time") && c.find("silent mode") == std::string_view::npos) {
            // The normal mode's: a silent mode's estimate follows it (gcode_extras.md, rule 7).
            if (const std::size_t eq = c.find('='); eq != std::string_view::npos)
                out_.estimated_seconds = parse_duration(c.substr(eq + 1));
        } else if (c.starts_with("WIDTH:")) {
            if (const std::optional<double> v = number(c.substr(6)); v && *v > 0.0) {
                width_ = *v;
                width_comments_ = true;
            }
        }
    }

    void new_layer(double z) {
        PreviewLayer layer;
        layer.z = z;
        layer.height = height_;
        layer.first = out_.moves.size();
        layer.first_extrusion = out_.extrusions.size();
        out_.layers.push_back(layer);
    }

    void m(double code, const Words& w) {
        const auto at = [&](char letter) -> const std::optional<double>& { return w.value[letter - 'A']; };
        if (code == 82) {
            absolute_e_ = true;
        } else if (code == 83) {
            absolute_e_ = false;
        } else if (code == 106 && (!at('P') || *at('P') == 0.0)) {
            // The part fan, S of 255 (the first one when there are several).
            fan_ = static_cast<std::uint8_t>(std::lround(std::clamp(at('S').value_or(255.0) / 255.0, 0.0, 1.0) * 100.0));
        } else if (code == 107 && (!at('P') || *at('P') == 0.0)) {
            fan_ = 0;
        } else if ((code == 104 || code == 109) && at('S') && (!at('T') || *at('T') == double(tool_))) {
            temperature_ = static_cast<std::uint16_t>(std::lround(std::clamp(*at('S'), 0.0, 1000.0)));
        } else if (code == 600) {
            // Marked already by the engine's ;COLOR_CHANGE.
            if (!colour_comment_)
                mark(MarkKind::ColorChange);
            colour_comment_ = false;
            ++color_;
        } else if (code == 601 || code == 0 || code == 1) {
            if (!pause_comment_)
                mark(MarkKind::Pause);
        }
    }

    void mark(MarkKind kind) {
        if (out_.layers.empty())
            new_layer(z_);
        out_.marks.push_back({float(x_), float(y_), float(z_), kind, static_cast<std::uint32_t>(out_.layers.size() - 1)});
    }

    void g(double code, const Words& w) {
        const auto at = [&](char letter) -> const std::optional<double>& { return w.value[letter - 'A']; };
        if (code == 0 || code == 1 || code == 2 || code == 3) {
            if (code >= 2)
                ++out_.arcs;
            if (at('F') && *at('F') > 0.0)
                feed_ = *at('F') / 60.0;
            const auto axis = [&](char letter, double now) {
                if (!at(letter))
                    return now;
                return absolute_ ? *at(letter) : now + *at(letter);
            };
            const double x = axis('X', x_), y = axis('Y', y_), z = axis('Z', z_);
            double de = 0.0;
            if (const std::optional<double>& e = at(e_axis_)) {
                de = absolute_e_ ? *e - e_ : *e;
                e_ = absolute_e_ ? *e : e_ + *e;
            }
            out_.filament += de;
            if (code >= 2 && (at('I') || at('J'))) {
                // An arc around (I, J) from here: in pieces of 5 degrees at most,
                // each with its share of E and of any move in Z.
                const double cx = x_ + at('I').value_or(0.0), cy = y_ + at('J').value_or(0.0);
                const double r = std::hypot(x_ - cx, y_ - cy);
                const double a0 = std::atan2(y_ - cy, x_ - cx), a1 = std::atan2(y - cy, x - cx);
                double sweep = code == 3 ? a1 - a0 : a0 - a1;
                while (sweep <= 1e-12)
                    sweep += 2.0 * std::numbers::pi;
                while (sweep > 2.0 * std::numbers::pi)
                    sweep -= 2.0 * std::numbers::pi;
                const int pieces = std::clamp(int(std::ceil(sweep / (std::numbers::pi / 36.0))), 1, 72);
                const double z0 = z_;
                for (int k = 1; k <= pieces; ++k) {
                    const double a = a0 + (code == 3 ? 1.0 : -1.0) * sweep * k / pieces;
                    const bool last = k == pieces;
                    move(last ? x : cx + r * std::cos(a), last ? y : cy + r * std::sin(a), z0 + (z - z0) * k / pieces,
                         de / pieces);
                }
            } else {
                move(x, y, z, de);
            }
        } else if (code == 90) {
            absolute_ = true;
        } else if (code == 91) {
            absolute_ = false;
        } else if (code == 92) {
            if (at('X'))
                x_ = *at('X');
            if (at('Y'))
                y_ = *at('Y');
            if (at('Z'))
                z_ = *at('Z');
            if (at(e_axis_))
                e_ = *at(e_axis_);
        } else if (code == 10 && !at('P') && !at('L')) {
            mark(MarkKind::Retract); // the firmware's retraction
        } else if (code == 11) {
            mark(MarkKind::Unretract);
        } else if (code == 28) {
            const auto named = [&](char letter) { return w.present[letter - 'A']; };
            const bool all = !named('X') && !named('Y') && !named('Z');
            if (all || named('X'))
                x_ = 0.0;
            if (all || named('Y'))
                y_ = 0.0;
            if (all || named('Z'))
                z_ = 0.0;
        }
    }

    void move(double x, double y, double z, double de) {
        const double dx = x - x_, dy = y - y_, dz = z - z_;
        const double xy = std::hypot(dx, dy);
        const double length = std::hypot(xy, dz);
        if (length <= 0.0) {
            // A retraction (the rest of one a wipe began is the same one) or
            // an unretraction (or a feed change).
            if (de < -kExtrudeMin && !wiping_)
                mark(MarkKind::Retract);
            else if (de > kExtrudeMin)
                mark(MarkKind::Unretract);
            if (de > kExtrudeMin)
                wiping_ = false;
            return;
        }
        colour_comment_ = pause_comment_ = false;
        const bool extrudes = de > kExtrudeMin;
        if (extrudes && !layer_comments_ &&
            (out_.layers.empty() || std::abs(z - out_.layers.back().z) > kNewZ)) {
            const double below = out_.layers.empty() ? 0.0 : out_.layers.back().z;
            height_ = z > below ? z - below : height_;
            new_layer(z);
        } else if (out_.layers.empty()) {
            new_layer(z);
        }
        PreviewMove mv;
        mv.x0 = float(x_), mv.y0 = float(y_), mv.z0 = float(z_);
        mv.x1 = float(x), mv.y1 = float(y), mv.z1 = float(z);
        mv.feed = float(feed_);
        mv.fan = fan_, mv.tool = tool_, mv.temperature = temperature_, mv.color = color_;
        mv.layer = static_cast<std::uint32_t>(out_.layers.size() - 1);
        PreviewLayer& layer = out_.layers.back();
        if (extrudes) {
            // An outer wall's loop starts here: its seam.
            if (kind_ == LineKind::OuterWall && !seam_open_)
                mark(MarkKind::Seam);
            seam_open_ = kind_ == LineKind::OuterWall;
            wiping_ = false;
            double height = layer.height > 0.0 ? layer.height : height_ > 0.0 ? height_ : std::max(z, 0.0);
            // Off the layer's Z (Z anti-aliasing, docs/ZAA.md): what is left down to its bottom.
            if (nonplanar_ && height_ > 0.0)
                height = height_; // a wavy bead: its own thickness (docs/NONPLANAR_INFILL.md)
            else if (layer_comments_ && layer.height > 0.0)
                height = std::max(0.0, height + ((z_ - layer.z) + (z - layer.z)) / 2.0);
            double width = width_;
            if (!width_comments_ && height > 0.0)
                width = bead_width(de * filament_area_ / length, height);
            mv.kind = kind_;
            mv.width = float(std::clamp(width, 0.01, 10.0));
            mv.height = float(height);
            out_.extrusions.push_back(static_cast<std::uint32_t>(out_.moves.size()));
            ++layer.extrusion_count;
            out_.kind_filament[static_cast<std::size_t>(kind_)] += de;
            out_.extruded += length;
            const float lo[3] = {std::min(mv.x0, mv.x1), std::min(mv.y0, mv.y1), std::min(mv.z0, mv.z1)};
            const float hi[3] = {std::max(mv.x0, mv.x1), std::max(mv.y0, mv.y1), std::max(mv.z0, mv.z1)};
            for (int k = 0; k < 3; ++k) {
                out_.min[k] = out_.extrusions.size() == 1 ? lo[k] : std::min(out_.min[k], lo[k]);
                out_.max[k] = out_.extrusions.size() == 1 ? hi[k] : std::max(out_.max[k], hi[k]);
            }
        } else {
            mv.kind = LineKind::Travel;
            out_.travelled += length;
            seam_open_ = false;
            // Retracting as it goes: a wipe, whose start is a retraction.
            mv.wipe = de < -kExtrudeMin;
            if (mv.wipe && !wiping_)
                mark(MarkKind::Retract);
            wiping_ = mv.wipe;
        }
        ++out_.kind_moves[static_cast<std::size_t>(mv.kind)];
        out_.moves.push_back(mv);
        ++layer.count;
        x_ = x, y_ = y, z_ = z;
    }

    GcodePreview out_;
    double filament_area_;
    char e_axis_; // the extruder's axis: E, or A on Mach3 and Machinekit
    double x_ = 0.0, y_ = 0.0, z_ = 0.0, e_ = 0.0;
    double feed_ = 0.0;
    double width_ = 0.4, height_ = 0.0;
    bool absolute_ = true, absolute_e_ = true;
    bool layer_comments_ = false, width_comments_ = false;
    bool nonplanar_ = false; // in a ;NONPLANAR block
    LineKind kind_ = LineKind::Other;
    std::uint8_t fan_ = 0, tool_ = 0;
    std::uint16_t temperature_ = 0, color_ = 0;
    bool tool_seen_ = false;      // a T came before: the next one changes the tool
    bool seam_open_ = false;      // in an outer wall's loop
    bool wiping_ = false;         // the last move was a wipe
    bool colour_comment_ = false; // ;COLOR_CHANGE marked the M600 that follows
    bool pause_comment_ = false;  // ;PAUSE_PRINT marked the pause that follows
};

} // namespace

LineKind line_kind(std::string_view type) {
    for (const KindName& k : kKinds)
        if (!k.type.empty() && k.type == type)
            return k.kind;
    return LineKind::Other;
}

const char* line_kind_id(LineKind kind) {
    return kKinds[static_cast<std::size_t>(kind)].id;
}

double parse_duration(std::string_view text) {
    text = trim(text);
    if (text.empty())
        return -1.0;
    double seconds = 0.0;
    int last = -1; // the unit before, so the parts go in order
    while (!text.empty()) {
        std::size_t digits = 0;
        while (digits < text.size() && (std::isdigit(static_cast<unsigned char>(text[digits])) || text[digits] == '.'))
            ++digits;
        if (digits == 0 || digits == text.size())
            return -1.0;
        const std::optional<double> v = number(text.substr(0, digits));
        constexpr std::string_view kUnits = "dhms";
        const std::size_t unit = kUnits.find(text[digits]);
        if (!v || unit == std::string_view::npos || int(unit) <= last)
            return -1.0;
        constexpr double kScale[] = {86400.0, 3600.0, 60.0, 1.0};
        seconds += *v * kScale[unit];
        last = int(unit);
        text = trim(text.substr(digits + 1));
    }
    return seconds;
}

GcodePreview read_gcode_preview(std::string_view text, double filament_diameter, char extruder_axis) {
    Reader reader(filament_diameter, extruder_axis >= 'A' && extruder_axis <= 'Z' ? extruder_axis : 'E');
    std::size_t start = 0;
    while (start < text.size()) {
        std::size_t end = text.find('\n', start);
        if (end == std::string_view::npos)
            end = text.size();
        reader.line(text.substr(start, end - start));
        start = end + 1;
    }
    return reader.finish();
}

void time_preview(GcodePreview& preview, const Kinematics& kinematics, double filament_diameter) {
    const double area = std::numbers::pi * 0.25 * filament_diameter * filament_diameter;
    // Each layer from rest to rest, as the estimate plans them: the layers
    // apart, in parallel.
    tbb::parallel_for(std::size_t(0), preview.layers.size(), [&](std::size_t l) {
        PreviewLayer& layer = preview.layers[l];
        PrintTime time(kinematics);
        std::vector<std::size_t> marks;
        marks.reserve(layer.count + 1);
        marks.push_back(time.mark());
        for (std::size_t i = layer.first; i < layer.first + layer.count; ++i) {
            const PreviewMove& m = preview.moves[i];
            const double dx = double(m.x1) - m.x0, dy = double(m.y1) - m.y0, dz = double(m.z1) - m.z0;
            const bool extruding = m.kind != LineKind::Travel;
            const double length = std::sqrt(dx * dx + dy * dy + dz * dz);
            const double de = extruding && area > 0.0 ? bead_area(m.width, m.height) * length / area : 0.0;
            // A move with no feed of its own (a file that never sets one): 50 mm/s.
            time.add(dx, dy, dz, de, m.feed > 0.0f ? double(m.feed) : 50.0, extruding);
            marks.push_back(time.mark());
        }
        layer.seconds = time.flush();
        for (std::size_t j = 0; j < layer.count; ++j)
            preview.moves[layer.first + j].seconds = float(time.time_at(marks[j + 1]) - time.time_at(marks[j]));
    });
    preview.kind_seconds = {};
    preview.moves_seconds = preview.first_layer_seconds = 0.0;
    bool first = true;
    for (const PreviewLayer& layer : preview.layers) {
        for (std::size_t i = layer.first; i < layer.first + layer.count; ++i)
            preview.kind_seconds[static_cast<std::size_t>(preview.moves[i].kind)] += preview.moves[i].seconds;
        preview.moves_seconds += layer.seconds;
        if (first && layer.extrusion_count > 0) {
            preview.first_layer_seconds = layer.seconds;
            first = false;
        }
    }
}

} // namespace tisma
