// DXF and SVG into a sketch, and a sketch out as DXF; see sketch_io.hpp.
// Own code over the public format references; see PROVENANCE.md.

#include "cad/sketch_io.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <map>
#include <numbers>
#include <optional>
#include <string>

namespace tisma::cad {

namespace {

constexpr double kPi = std::numbers::pi;

// A 2D affine map: x' = a x + c y + e, y' = b x + d y + f (SVG's order).
struct Affine {
    double a = 1, b = 0, c = 0, d = 1, e = 0, f = 0;
    Pt operator()(const Pt& p) const { return {a * p.x + c * p.y + e, b * p.x + d * p.y + f}; }
    // this after `inner`.
    Affine then_after(const Affine& inner) const {
        return {a * inner.a + c * inner.b, b * inner.a + d * inner.b, a * inner.c + c * inner.d,
                b * inner.c + d * inner.d, a * inner.e + c * inner.f + e, b * inner.e + d * inner.f + f};
    }
    double det() const { return a * d - b * c; }
    // A rotation with one scale (and maybe a mirror): circles stay circles.
    bool similar() const {
        const double sx = std::hypot(a, b), sy = std::hypot(c, d);
        return std::abs(sx - sy) <= 1e-9 * std::max(sx, sy) && std::abs(a * c + b * d) <= 1e-9 * sx * sy;
    }
    double scale() const { return std::sqrt(std::abs(det())); }
};

Affine translate(double x, double y) { return {1, 0, 0, 1, x, y}; }
Affine scaling(double sx, double sy) { return {sx, 0, 0, sy, 0, 0}; }
Affine rotation(double radians) {
    const double c = std::cos(radians), s = std::sin(radians);
    return {c, s, -s, c, 0, 0};
}

std::optional<double> number(std::string_view text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t'))
        text.remove_prefix(1);
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r'))
        text.remove_suffix(1);
    if (!text.empty() && text.front() == '+')
        text.remove_prefix(1);
    double v = 0.0;
    const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), v);
    if (ec != std::errc() || end != text.data() + text.size() || !std::isfinite(v))
        return std::nullopt;
    return v;
}

// Points along an arc (centre, radius, start and sweep in radians), about
// one each 5 degrees.
std::vector<Pt> arc_points(const Pt& centre, double rx, double ry, double rotation_rad, double start, double sweep) {
    const int n = std::max(2, int(std::ceil(std::abs(sweep) / (kPi / 36.0))));
    std::vector<Pt> out;
    const double cr = std::cos(rotation_rad), sr = std::sin(rotation_rad);
    for (int i = 0; i <= n; ++i) {
        const double t = start + sweep * i / n;
        const double x = rx * std::cos(t), y = ry * std::sin(t);
        out.push_back({centre.x + cr * x - sr * y, centre.y + sr * x + cr * y});
    }
    return out;
}

Entity chain(std::vector<Pt> points, bool closed) {
    // Repeated points out; a closed chain does not repeat its first.
    std::vector<Pt> kept;
    for (const Pt& p : points)
        if (kept.empty() || std::hypot(p.x - kept.back().x, p.y - kept.back().y) > 1e-9)
            kept.push_back(p);
    if (closed && kept.size() > 2 && std::hypot(kept.front().x - kept.back().x, kept.front().y - kept.back().y) <= 1e-9)
        kept.pop_back();
    Entity e;
    e.kind = Entity::Kind::Chain;
    e.points = std::move(kept);
    e.closed = closed && e.points.size() > 2;
    return e;
}

// A circle or arc through a map: itself when the map keeps circles,
// else as points.
void add_circle(std::vector<Entity>& out, const Affine& m, const Pt& centre, double r) {
    if (m.similar()) {
        Entity e;
        e.kind = Entity::Kind::Circle;
        e.points = {m(centre)};
        e.radius = r * m.scale();
        out.push_back(e);
        return;
    }
    std::vector<Pt> pts = arc_points(centre, r, r, 0.0, 0.0, 2 * kPi);
    for (Pt& p : pts)
        p = m(p);
    out.push_back(chain(std::move(pts), true));
}

// An arc counter-clockwise from `a0` to `a1` (radians) in its own frame.
void add_arc(std::vector<Entity>& out, const Affine& m, const Pt& centre, double r, double a0, double a1) {
    double sweep = a1 - a0;
    while (sweep <= 0.0)
        sweep += 2 * kPi;
    if (m.similar()) {
        Entity e;
        e.kind = Entity::Kind::Arc;
        const Pt s{centre.x + r * std::cos(a0), centre.y + r * std::sin(a0)};
        const Pt t{centre.x + r * std::cos(a0 + sweep), centre.y + r * std::sin(a0 + sweep)};
        e.points = {m(centre), m(s), m(t)};
        e.radius = r * m.scale();
        e.clockwise = m.det() < 0.0; // a mirror turns it round
        out.push_back(e);
        return;
    }
    std::vector<Pt> pts = arc_points(centre, r, r, 0.0, a0, sweep);
    for (Pt& p : pts)
        p = m(p);
    out.push_back(chain(std::move(pts), false));
}

// ---------------------------------------------------------------- DXF

struct Pair {
    int code = 0;
    std::string value;
};

struct DxfEntity {
    std::string type;
    std::vector<Pair> pairs;
    std::vector<DxfEntity> vertices; // POLYLINE's
    std::optional<std::string> first(int code) const {
        for (const Pair& p : pairs)
            if (p.code == code)
                return p.value;
        return std::nullopt;
    }
    double num(int code, double fallback = 0.0) const {
        if (const auto v = first(code))
            if (const auto n = number(*v))
                return *n;
        return fallback;
    }
    std::vector<double> all(int code) const {
        std::vector<double> out;
        for (const Pair& p : pairs)
            if (p.code == code)
                if (const auto n = number(p.value))
                    out.push_back(*n);
        return out;
    }
};

struct Block {
    Pt base;
    std::vector<DxfEntity> entities;
};

// mm per drawing unit, by $INSUNITS (DXF Reference, HEADER variables).
double unit_mm(int units) {
    switch (units) {
    case 1: return 25.4;       // inches
    case 2: return 304.8;      // feet
    case 3: return 1609344.0;  // miles
    case 5: return 10.0;       // centimetres
    case 6: return 1000.0;     // metres
    case 7: return 1.0e6;      // kilometres
    case 8: return 25.4e-6;    // microinches
    case 9: return 0.0254;     // mils
    case 10: return 914.4;     // yards
    case 13: return 1.0e-3;    // microns
    case 14: return 100.0;     // decimetres
    default: return 1.0;       // 0 (none), 4 (mm) and the rest
    }
}

struct DxfReader {
    std::map<std::string, Block> blocks;
    std::vector<std::string>* notes = nullptr;
    std::map<std::string, int> skipped;
    int depth = 0;

    void emit(const DxfEntity& e, const Affine& m, std::vector<Entity>& out) {
        if (e.num(67) == 1.0) {
            ++skipped["paper space"];
            return;
        }
        const std::string& t = e.type;
        const Pt p10{e.num(10), e.num(20)};
        if (t == "LINE") {
            out.push_back(chain({m(p10), m(Pt{e.num(11), e.num(21)})}, false));
        } else if (t == "CIRCLE") {
            add_circle(out, m, p10, e.num(40));
        } else if (t == "ARC") {
            add_arc(out, m, p10, e.num(40), e.num(50) * kPi / 180.0, e.num(51) * kPi / 180.0);
        } else if (t == "LWPOLYLINE" || t == "POLYLINE") {
            std::vector<Pt> pts;
            std::vector<double> bulge;
            if (t == "LWPOLYLINE") {
                for (const Pair& p : e.pairs) {
                    if (p.code == 10) {
                        pts.push_back({number(p.value).value_or(0.0), 0.0});
                        bulge.push_back(0.0);
                    } else if (p.code == 20 && !pts.empty()) {
                        pts.back().y = number(p.value).value_or(0.0);
                    } else if (p.code == 42 && !bulge.empty()) {
                        bulge.back() = number(p.value).value_or(0.0);
                    }
                }
            } else {
                for (const DxfEntity& v : e.vertices) {
                    pts.push_back({v.num(10), v.num(20)});
                    bulge.push_back(v.num(42));
                }
            }
            const bool closed = (int(e.num(70)) & 1) != 0;
            polyline(pts, bulge, closed, m, out);
        } else if (t == "ELLIPSE") {
            const Pt major{e.num(11), e.num(21)};
            const double rx = std::hypot(major.x, major.y), ratio = e.num(40, 1.0);
            const double t0 = e.num(41, 0.0), t1 = e.num(42, 2 * kPi);
            double sweep = t1 - t0;
            while (sweep <= 0.0)
                sweep += 2 * kPi;
            std::vector<Pt> pts = arc_points(p10, rx, rx * ratio, std::atan2(major.y, major.x), t0, sweep);
            for (Pt& q : pts)
                q = m(q);
            out.push_back(chain(std::move(pts), std::abs(sweep - 2 * kPi) < 1e-9));
        } else if (t == "SPLINE") {
            spline(e, m, out);
        } else if (t == "INSERT") {
            insert(e, m, out);
        } else if (t == "POINT" || t == "TEXT" || t == "MTEXT" || t == "DIMENSION" || t == "HATCH" || t == "SOLID") {
            ++skipped[t];
        } else if (!t.empty()) {
            ++skipped[t];
        }
    }

    void polyline(const std::vector<Pt>& pts, const std::vector<double>& bulge, bool closed, const Affine& m,
                  std::vector<Entity>& out) {
        if (pts.size() < 2)
            return;
        const bool any_bulge = std::any_of(bulge.begin(), bulge.end(), [](double b) { return std::abs(b) > 1e-12; });
        if (!any_bulge) {
            std::vector<Pt> mapped;
            for (const Pt& p : pts)
                mapped.push_back(m(p));
            out.push_back(chain(std::move(mapped), closed));
            return;
        }
        // Straight runs as chains, bulged segments as arcs.
        std::vector<Pt> run{m(pts[0])};
        const std::size_t segments = closed ? pts.size() : pts.size() - 1;
        for (std::size_t i = 0; i < segments; ++i) {
            const Pt& p0 = pts[i];
            const Pt& p1 = pts[(i + 1) % pts.size()];
            const double b = bulge[i];
            if (std::abs(b) <= 1e-12) {
                run.push_back(m(p1));
                continue;
            }
            if (run.size() > 1)
                out.push_back(chain(run, false));
            const double ux = p1.x - p0.x, uy = p1.y - p0.y, c = std::hypot(ux, uy);
            if (c > 0.0) {
                // DXF Reference, bulge: tan of a quarter of the included
                // angle, positive counter-clockwise.
                const double offset = c * (1.0 - b * b) / (4.0 * b);
                const Pt centre{(p0.x + p1.x) / 2 - uy / c * offset, (p0.y + p1.y) / 2 + ux / c * offset};
                const double r = c * (1.0 + b * b) / (4.0 * std::abs(b));
                const double a0 = std::atan2(p0.y - centre.y, p0.x - centre.x), a1 = std::atan2(p1.y - centre.y, p1.x - centre.x);
                if (b > 0)
                    add_arc(out, m, centre, r, a0, a1);
                else
                    add_arc(out, m, centre, r, a1, a0);
            }
            run = {m(p1)};
        }
        if (run.size() > 1)
            out.push_back(chain(run, false));
    }

    void spline(const DxfEntity& e, const Affine& m, std::vector<Entity>& out) {
        const int degree = std::max(1, int(e.num(71, 3)));
        const std::vector<double> knots = e.all(40), xs = e.all(10), ys = e.all(20), weights = e.all(41);
        std::vector<Pt> ctrl;
        for (std::size_t i = 0; i < std::min(xs.size(), ys.size()); ++i)
            ctrl.push_back({xs[i], ys[i]});
        const bool closed = (int(e.num(70)) & 1) != 0;
        std::vector<Pt> pts;
        const std::size_t n = ctrl.size();
        if (n >= std::size_t(degree + 1) && knots.size() == n + std::size_t(degree) + 1) {
            // de Boor's algorithm, rational when weights are given.
            const double t0 = knots[std::size_t(degree)], t1 = knots[n];
            const int samples = int(std::max<std::size_t>(16, 8 * n));
            for (int s = 0; s <= samples; ++s) {
                const double t = std::min(t0 + (t1 - t0) * s / samples, t1 - 1e-12 * std::max(1.0, std::abs(t1)));
                std::size_t k = std::size_t(degree);
                while (k + 1 < n && knots[k + 1] <= t)
                    ++k;
                std::vector<std::array<double, 3>> d;
                for (std::size_t j = 0; j <= std::size_t(degree); ++j) {
                    const std::size_t i = k - std::size_t(degree) + j;
                    const double w = weights.size() == n ? weights[i] : 1.0;
                    d.push_back({ctrl[i].x * w, ctrl[i].y * w, w});
                }
                for (int r = 1; r <= degree; ++r)
                    for (int j = degree; j >= r; --j) {
                        const std::size_t i = k - std::size_t(degree) + std::size_t(j);
                        const double den = knots[i + std::size_t(degree) + 1 - std::size_t(r)] - knots[i];
                        const double alpha = den != 0.0 ? (t - knots[i]) / den : 0.0;
                        for (int c = 0; c < 3; ++c)
                            d[std::size_t(j)][std::size_t(c)] =
                                (1 - alpha) * d[std::size_t(j - 1)][std::size_t(c)] + alpha * d[std::size_t(j)][std::size_t(c)];
                    }
                const auto& q = d[std::size_t(degree)];
                pts.push_back(m(Pt{q[0] / q[2], q[1] / q[2]}));
            }
        } else {
            // Fit points (or the control polygon) as a chain.
            const std::vector<double> fx = e.all(11), fy = e.all(21);
            if (fx.size() >= 2 && fx.size() == fy.size())
                for (std::size_t i = 0; i < fx.size(); ++i)
                    pts.push_back(m(Pt{fx[i], fy[i]}));
            else
                for (const Pt& p : ctrl)
                    pts.push_back(m(p));
        }
        if (pts.size() >= 2)
            out.push_back(chain(std::move(pts), closed));
    }

    void insert(const DxfEntity& e, const Affine& m, std::vector<Entity>& out) {
        const auto name = e.first(2);
        const auto it = name ? blocks.find(*name) : blocks.end();
        if (it == blocks.end() || depth > 16) {
            ++skipped["INSERT"];
            return;
        }
        const Block& block = it->second;
        const double sx = e.num(41, 1.0), sy = e.num(42, 1.0), rot = e.num(50) * kPi / 180.0;
        const int cols = std::max(1, int(e.num(70, 1))), rows = std::max(1, int(e.num(71, 1)));
        const double dc = e.num(44), dr = e.num(45);
        ++depth;
        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < cols; ++c) {
                // Block coordinates from its base, scaled, turned, set at the
                // insertion point plus the cell's offset along the turned axes.
                const Affine local = translate(e.num(10), e.num(20))
                                         .then_after(rotation(rot))
                                         .then_after(translate(c * dc, r * dr))
                                         .then_after(scaling(sx, sy))
                                         .then_after(translate(-block.base.x, -block.base.y));
                const Affine whole = m.then_after(local);
                for (const DxfEntity& inner : block.entities)
                    emit(inner, whole, out);
            }
        --depth;
    }
};

} // namespace

SketchImport read_dxf(std::string_view text) {
    SketchImport result;
    // Group code and value lines.
    std::vector<Pair> pairs;
    std::size_t pos = 0;
    const auto next_line = [&](std::string& line) {
        if (pos >= text.size())
            return false;
        std::size_t end = text.find('\n', pos);
        if (end == std::string_view::npos)
            end = text.size();
        line.assign(text.substr(pos, end - pos));
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t'))
            line.pop_back();
        std::size_t first = 0;
        while (first < line.size() && (line[first] == ' ' || line[first] == '\t'))
            ++first;
        line.erase(0, first);
        pos = end + 1;
        return true;
    };
    std::string code_line, value_line;
    while (next_line(code_line)) {
        if (!next_line(value_line)) {
            result.error = "the DXF file ends in the middle of a group";
            return result;
        }
        const auto code = number(code_line);
        if (!code || *code != std::floor(*code)) {
            result.error = "not an ASCII DXF file (a group code is not a number: '" + code_line.substr(0, 20) + "')";
            return result;
        }
        pairs.push_back({int(*code), value_line});
    }
    if (pairs.empty()) {
        result.error = "the DXF file is empty";
        return result;
    }
    // Sections, entities and blocks.
    DxfReader reader;
    reader.notes = &result.notes;
    std::vector<DxfEntity> entities;
    double mm = 1.0;
    std::string section;
    std::optional<std::string> block_name;
    Block block;
    DxfEntity* polyline = nullptr;
    std::vector<DxfEntity>* target = nullptr;
    for (std::size_t i = 0; i < pairs.size(); ++i) {
        const Pair& p = pairs[i];
        if (p.code == 0 && p.value == "SECTION") {
            section = i + 1 < pairs.size() && pairs[i + 1].code == 2 ? pairs[i + 1].value : "";
            ++i;
            target = section == "ENTITIES" ? &entities : nullptr;
            continue;
        }
        if (p.code == 0 && p.value == "ENDSEC") {
            section.clear();
            target = nullptr;
            continue;
        }
        if (section == "HEADER") {
            if (p.code == 9 && p.value == "$INSUNITS" && i + 1 < pairs.size())
                mm = unit_mm(int(number(pairs[i + 1].value).value_or(0.0)));
            continue;
        }
        if (section == "BLOCKS") {
            if (p.code == 0 && p.value == "BLOCK") {
                block = Block{};
                block_name.reset();
                // Its header: name and base point, up to the first entity.
                std::size_t j = i + 1;
                for (; j < pairs.size() && pairs[j].code != 0; ++j) {
                    if (pairs[j].code == 2)
                        block_name = pairs[j].value;
                    else if (pairs[j].code == 10)
                        block.base.x = number(pairs[j].value).value_or(0.0);
                    else if (pairs[j].code == 20)
                        block.base.y = number(pairs[j].value).value_or(0.0);
                }
                i = j - 1;
                target = &block.entities;
                continue;
            }
            if (p.code == 0 && p.value == "ENDBLK") {
                if (block_name)
                    reader.blocks[*block_name] = block;
                target = nullptr;
                // Skip ENDBLK's own groups.
                while (i + 1 < pairs.size() && pairs[i + 1].code != 0)
                    ++i;
                continue;
            }
        }
        if (!target)
            continue;
        if (p.code == 0) {
            if (p.value == "VERTEX" && polyline) {
                polyline->vertices.push_back(DxfEntity{p.value, {}, {}});
                continue;
            }
            if (p.value == "SEQEND") {
                polyline = nullptr;
                target->push_back(DxfEntity{"", {}, {}}); // swallows SEQEND's groups
                continue;
            }
            target->push_back(DxfEntity{p.value, {}, {}});
            polyline = p.value == "POLYLINE" ? &target->back() : nullptr;
            continue;
        }
        if (polyline && !polyline->vertices.empty())
            polyline->vertices.back().pairs.push_back(p);
        else if (!target->empty())
            target->back().pairs.push_back(p);
    }
    const Affine units = scaling(mm, mm);
    for (const DxfEntity& e : entities)
        reader.emit(e, units, result.entities);
    for (const auto& [what, count] : reader.skipped)
        result.notes.push_back(std::to_string(count) + " × " + what + " left out");
    if (result.entities.empty() && result.error.empty())
        result.error = "the DXF file has no lines, arcs or curves in model space";
    return result;
}

// ---------------------------------------------------------------- SVG

namespace {

struct XmlTag {
    std::string name;
    std::map<std::string, std::string> attributes;
    bool closing = false, self_closing = false;
};

// The tags of an XML text, in order (comments, declarations and text left
// out). False when a tag does not end.
bool xml_tags(std::string_view text, std::vector<XmlTag>& tags) {
    std::size_t i = 0;
    while ((i = text.find('<', i)) != std::string_view::npos) {
        if (text.substr(i, 4) == "<!--") {
            const std::size_t end = text.find("-->", i);
            if (end == std::string_view::npos)
                return false;
            i = end + 3;
            continue;
        }
        if (text.substr(i, 9) == "<![CDATA[") {
            const std::size_t end = text.find("]]>", i);
            if (end == std::string_view::npos)
                return false;
            i = end + 3;
            continue;
        }
        if (i + 1 < text.size() && (text[i + 1] == '?' || text[i + 1] == '!')) {
            const std::size_t end = text.find('>', i);
            if (end == std::string_view::npos)
                return false;
            i = end + 1;
            continue;
        }
        XmlTag tag;
        std::size_t j = i + 1;
        if (j < text.size() && text[j] == '/')
            tag.closing = true, ++j;
        const auto space = [&](char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; };
        const std::size_t name0 = j;
        while (j < text.size() && !space(text[j]) && text[j] != '>' && text[j] != '/')
            ++j;
        tag.name = std::string(text.substr(name0, j - name0));
        if (const std::size_t colon = tag.name.find(':'); colon != std::string::npos)
            tag.name = tag.name.substr(colon + 1); // svg:path
        for (;;) {
            while (j < text.size() && space(text[j]))
                ++j;
            if (j >= text.size())
                return false;
            if (text[j] == '>') {
                ++j;
                break;
            }
            if (text[j] == '/') {
                tag.self_closing = true;
                ++j;
                continue;
            }
            const std::size_t key0 = j;
            while (j < text.size() && !space(text[j]) && text[j] != '=' && text[j] != '>' && text[j] != '/')
                ++j;
            std::string key(text.substr(key0, j - key0));
            while (j < text.size() && space(text[j]))
                ++j;
            std::string value;
            if (j < text.size() && text[j] == '=') {
                ++j;
                while (j < text.size() && space(text[j]))
                    ++j;
                if (j < text.size() && (text[j] == '"' || text[j] == '\'')) {
                    const char quote = text[j];
                    const std::size_t end = text.find(quote, j + 1);
                    if (end == std::string_view::npos)
                        return false;
                    value = std::string(text.substr(j + 1, end - j - 1));
                    j = end + 1;
                }
            }
            if (!key.empty())
                tag.attributes[key] = value;
        }
        tags.push_back(std::move(tag));
        i = j;
    }
    return true;
}

// Numbers in a list: separators are spaces and commas; a sign or a second
// point starts the next one ("1-2", ".5.5").
std::vector<double> numbers_in(std::string_view s) {
    std::vector<double> out;
    std::size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && (s[i] == ' ' || s[i] == ',' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r'))
            ++i;
        if (i >= s.size())
            break;
        double v = 0.0;
        const auto [end, ec] = std::from_chars(s.data() + i, s.data() + s.size(), v);
        if (ec != std::errc() || end == s.data() + i) {
            ++i;
            continue;
        }
        out.push_back(v);
        i = std::size_t(end - s.data());
    }
    return out;
}

// A length with a unit, in mm (px at 96 per inch).
std::optional<double> length_mm(const std::string& text) {
    std::size_t k = 0;
    while (k < text.size() && (std::isdigit(static_cast<unsigned char>(text[k])) || text[k] == '.' || text[k] == '-' ||
                               text[k] == '+' || text[k] == 'e' || text[k] == 'E'))
        ++k;
    const auto v = number(std::string_view(text).substr(0, k));
    if (!v)
        return std::nullopt;
    const std::string unit = text.substr(k);
    if (unit == "mm")
        return *v;
    if (unit == "cm")
        return *v * 10.0;
    if (unit == "in")
        return *v * 25.4;
    if (unit == "pt")
        return *v * 25.4 / 72.0;
    if (unit == "pc")
        return *v * 25.4 / 6.0;
    if (unit.empty() || unit == "px")
        return *v * 25.4 / 96.0;
    return std::nullopt; // %, em: not a size
}

Affine parse_transform(const std::string& text) {
    Affine m;
    std::size_t i = 0;
    while (i < text.size()) {
        const std::size_t open = text.find('(', i);
        if (open == std::string::npos)
            break;
        const std::size_t close = text.find(')', open);
        if (close == std::string::npos)
            break;
        std::string name = text.substr(i, open - i);
        name.erase(std::remove_if(name.begin(), name.end(), [](char c) { return c == ' ' || c == ',' || c == '\n' || c == '\t'; }),
                   name.end());
        const std::vector<double> v = numbers_in(std::string_view(text).substr(open + 1, close - open - 1));
        Affine t;
        if (name == "matrix" && v.size() == 6)
            t = {v[0], v[1], v[2], v[3], v[4], v[5]};
        else if (name == "translate" && !v.empty())
            t = translate(v[0], v.size() > 1 ? v[1] : 0.0);
        else if (name == "scale" && !v.empty())
            t = scaling(v[0], v.size() > 1 ? v[1] : v[0]);
        else if (name == "rotate" && !v.empty()) {
            t = rotation(v[0] * kPi / 180.0);
            if (v.size() == 3)
                t = translate(v[1], v[2]).then_after(t).then_after(translate(-v[1], -v[2]));
        } else if (name == "skewX" && !v.empty())
            t = {1, 0, std::tan(v[0] * kPi / 180.0), 1, 0, 0};
        else if (name == "skewY" && !v.empty())
            t = {1, std::tan(v[0] * kPi / 180.0), 0, 1, 0, 0};
        m = m.then_after(t);
        i = close + 1;
    }
    return m;
}

// Path data (SVG 1.1, 8.3): its subpaths as point chains, curves sampled.
void path_chains(const std::string& d, const Affine& m, std::vector<Entity>& out) {
    std::vector<Pt> current;
    Pt pen, start, last_control;
    char last = 0;
    bool any = false;
    const auto flush = [&](bool closed) {
        if (current.size() >= 2) {
            std::vector<Pt> mapped;
            for (const Pt& p : current)
                mapped.push_back(m(p));
            out.push_back(chain(std::move(mapped), closed));
        }
        current.clear();
    };
    std::size_t i = 0;
    char command = 0;
    while (i < d.size()) {
        const char c = d[i];
        if (std::isalpha(static_cast<unsigned char>(c)) && c != 'e' && c != 'E') {
            command = c;
            ++i;
            if (c == 'Z' || c == 'z') {
                if (!current.empty())
                    current.push_back(start);
                flush(true);
                pen = start;
                last = c;
            }
            continue;
        }
        if (!command) {
            ++i;
            continue;
        }
        // The arguments up to the next command.
        std::size_t j = i;
        while (j < d.size() && !(std::isalpha(static_cast<unsigned char>(d[j])) && d[j] != 'e' && d[j] != 'E'))
            ++j;
        const std::vector<double> v = numbers_in(std::string_view(d).substr(i, j - i));
        i = j;
        const bool rel = std::islower(static_cast<unsigned char>(command)) != 0;
        const char up = char(std::toupper(static_cast<unsigned char>(command)));
        const std::size_t step = up == 'M' || up == 'L' || up == 'T' ? 2 : up == 'H' || up == 'V' ? 1 : up == 'S' || up == 'Q' ? 4
                                 : up == 'C'                                                  ? 6
                                 : up == 'A'                                                  ? 7
                                                                                              : 0;
        if (step == 0)
            continue;
        for (std::size_t k = 0; k + step <= v.size(); k += step) {
            const Pt base = rel ? pen : Pt{0, 0};
            const auto at = [&](std::size_t n) { return Pt{base.x + v[k + n], base.y + v[k + n + 1]}; };
            if (up == 'M') {
                if (k == 0) {
                    flush(false);
                    pen = start = at(0);
                    current = {pen};
                    any = true;
                } else { // further pairs are lines
                    pen = at(0);
                    current.push_back(pen);
                }
                last = 'M';
                continue;
            }
            if (current.empty())
                current = {pen};
            if (up == 'L' || up == 'H' || up == 'V') {
                pen = up == 'L' ? at(0) : up == 'H' ? Pt{(rel ? pen.x : 0.0) + v[k], pen.y} : Pt{pen.x, (rel ? pen.y : 0.0) + v[k]};
                current.push_back(pen);
            } else if (up == 'C' || up == 'S' || up == 'Q' || up == 'T') {
                Pt c1, c2, end;
                const bool reflect = (up == 'S' && (last == 'C' || last == 'S')) || (up == 'T' && (last == 'Q' || last == 'T'));
                const Pt mirrored = reflect ? Pt{2 * pen.x - last_control.x, 2 * pen.y - last_control.y} : pen;
                bool cubic = true;
                if (up == 'C')
                    c1 = at(0), c2 = at(2), end = at(4);
                else if (up == 'S')
                    c1 = mirrored, c2 = at(0), end = at(2);
                else if (up == 'Q')
                    c1 = at(0), end = at(2), cubic = false;
                else
                    c1 = mirrored, end = at(0), cubic = false;
                const Pt p0 = pen;
                for (int s = 1; s <= 16; ++s) {
                    const double t = s / 16.0, u = 1 - t;
                    if (cubic)
                        current.push_back({u * u * u * p0.x + 3 * u * u * t * c1.x + 3 * u * t * t * c2.x + t * t * t * end.x,
                                           u * u * u * p0.y + 3 * u * u * t * c1.y + 3 * u * t * t * c2.y + t * t * t * end.y});
                    else
                        current.push_back({u * u * p0.x + 2 * u * t * c1.x + t * t * end.x, u * u * p0.y + 2 * u * t * c1.y + t * t * end.y});
                }
                last_control = cubic ? c2 : c1;
                pen = end;
            } else if (up == 'A') {
                // Endpoint to centre (SVG 1.1, F.6.5).
                double rx = std::abs(v[k]), ry = std::abs(v[k + 1]);
                const double phi = v[k + 2] * kPi / 180.0;
                const bool large = v[k + 3] != 0.0, sweep = v[k + 4] != 0.0;
                const Pt end{base.x + v[k + 5], base.y + v[k + 6]};
                if (rx == 0 || ry == 0) {
                    current.push_back(end);
                    pen = end;
                    last = up;
                    continue;
                }
                const double cp = std::cos(phi), sp = std::sin(phi);
                const double dx = (pen.x - end.x) / 2, dy = (pen.y - end.y) / 2;
                const double x1 = cp * dx + sp * dy, y1 = -sp * dx + cp * dy;
                const double lambda = x1 * x1 / (rx * rx) + y1 * y1 / (ry * ry);
                if (lambda > 1)
                    rx *= std::sqrt(lambda), ry *= std::sqrt(lambda);
                const double num = std::max(0.0, rx * rx * ry * ry - rx * rx * y1 * y1 - ry * ry * x1 * x1);
                const double den = rx * rx * y1 * y1 + ry * ry * x1 * x1;
                double coef = den > 0 ? std::sqrt(num / den) : 0.0;
                if (large == sweep)
                    coef = -coef;
                const double cxp = coef * rx * y1 / ry, cyp = -coef * ry * x1 / rx;
                const Pt centre{cp * cxp - sp * cyp + (pen.x + end.x) / 2, sp * cxp + cp * cyp + (pen.y + end.y) / 2};
                const auto angle = [](double ux, double uy, double vx, double vy) {
                    return std::atan2(ux * vy - uy * vx, ux * vx + uy * vy);
                };
                const double t1 = angle(1, 0, (x1 - cxp) / rx, (y1 - cyp) / ry);
                double dt = angle((x1 - cxp) / rx, (y1 - cyp) / ry, (-x1 - cxp) / rx, (-y1 - cyp) / ry);
                if (!sweep && dt > 0)
                    dt -= 2 * kPi;
                else if (sweep && dt < 0)
                    dt += 2 * kPi;
                std::vector<Pt> pts = arc_points(centre, rx, ry, phi, t1, dt);
                current.insert(current.end(), pts.begin() + 1, pts.end());
                pen = end;
            }
            last = up;
        }
    }
    flush(false);
    (void)any;
}

double attr(const XmlTag& t, const char* name, double fallback = 0.0) {
    const auto it = t.attributes.find(name);
    if (it == t.attributes.end())
        return fallback;
    if (const auto v = length_mm(it->second))
        return *v * 96.0 / 25.4; // back to user units: the root's scale applies once
    return number(it->second).value_or(fallback);
}

} // namespace

SketchImport read_svg(std::string_view text) {
    SketchImport result;
    std::vector<XmlTag> tags;
    if (!xml_tags(text, tags)) {
        result.error = "the SVG file is not well formed (a tag does not end)";
        return result;
    }
    const auto root = std::find_if(tags.begin(), tags.end(), [](const XmlTag& t) { return t.name == "svg" && !t.closing; });
    if (root == tags.end()) {
        result.error = "not an SVG file (no <svg> element)";
        return result;
    }
    // The root's scale: mm per user unit, from its width and viewBox; y
    // turned up about the drawing's height.
    double mm_per_unit = 25.4 / 96.0, flip_height = 0.0;
    std::array<double, 4> view{0, 0, 0, 0};
    if (const auto it = root->attributes.find("viewBox"); it != root->attributes.end()) {
        const std::vector<double> v = numbers_in(it->second);
        if (v.size() == 4 && v[2] > 0 && v[3] > 0)
            view = {v[0], v[1], v[2], v[3]};
    }
    if (view[2] > 0) {
        if (const auto w = root->attributes.find("width"); w != root->attributes.end())
            if (const auto mm = length_mm(w->second); mm && *mm > 0)
                mm_per_unit = *mm / view[2];
        flip_height = view[3];
    }
    const Affine root_map =
        scaling(mm_per_unit, -mm_per_unit).then_after(translate(-view[0], -view[1] - flip_height));
    std::vector<Affine> stack{root_map};
    std::map<std::string, int> skipped;
    int hidden_depth = 0; // inside <defs>, <clipPath>, <mask>, <symbol>
    for (auto t = root + 1; t != tags.end(); ++t) {
        const bool container = t->name == "g" || t->name == "svg" || t->name == "a";
        const bool hidden = t->name == "defs" || t->name == "clipPath" || t->name == "mask" || t->name == "symbol" ||
                            t->name == "pattern" || t->name == "marker";
        if (t->closing) {
            if (hidden && hidden_depth > 0)
                --hidden_depth;
            else if (container && stack.size() > 1)
                stack.pop_back();
            continue;
        }
        if (hidden) {
            if (!t->self_closing)
                ++hidden_depth;
            continue;
        }
        if (hidden_depth > 0)
            continue;
        Affine m = stack.back();
        if (const auto it = t->attributes.find("transform"); it != t->attributes.end())
            m = m.then_after(parse_transform(it->second));
        if (container) {
            if (!t->self_closing)
                stack.push_back(m);
            continue;
        }
        const std::string& n = t->name;
        if (n == "path") {
            if (const auto it = t->attributes.find("d"); it != t->attributes.end())
                path_chains(it->second, m, result.entities);
        } else if (n == "rect") {
            const double x = attr(*t, "x"), y = attr(*t, "y"), w = attr(*t, "width"), h = attr(*t, "height");
            if (w > 0 && h > 0) {
                if (attr(*t, "rx") > 0 || attr(*t, "ry") > 0)
                    ++skipped["rounded corners of rect (drawn square)"];
                result.entities.push_back(chain({m(Pt{x, y}), m(Pt{x + w, y}), m(Pt{x + w, y + h}), m(Pt{x, y + h})}, true));
            }
        } else if (n == "circle") {
            if (const double r = attr(*t, "r"); r > 0)
                add_circle(result.entities, m, {attr(*t, "cx"), attr(*t, "cy")}, r);
        } else if (n == "ellipse") {
            const double rx = attr(*t, "rx"), ry = attr(*t, "ry");
            if (rx > 0 && ry > 0) {
                std::vector<Pt> pts = arc_points({attr(*t, "cx"), attr(*t, "cy")}, rx, ry, 0, 0, 2 * kPi);
                for (Pt& p : pts)
                    p = m(p);
                result.entities.push_back(chain(std::move(pts), true));
            }
        } else if (n == "line") {
            result.entities.push_back(chain({m(Pt{attr(*t, "x1"), attr(*t, "y1")}), m(Pt{attr(*t, "x2"), attr(*t, "y2")})}, false));
        } else if (n == "polyline" || n == "polygon") {
            if (const auto it = t->attributes.find("points"); it != t->attributes.end()) {
                const std::vector<double> v = numbers_in(it->second);
                std::vector<Pt> pts;
                for (std::size_t k = 0; k + 1 < v.size(); k += 2)
                    pts.push_back(m(Pt{v[k], v[k + 1]}));
                result.entities.push_back(chain(std::move(pts), n == "polygon"));
            }
        } else if (n == "text" || n == "image" || n == "use") {
            ++skipped[n];
        }
    }
    // Entities of fewer than two points say nothing.
    result.entities.erase(std::remove_if(result.entities.begin(), result.entities.end(),
                                         [](const Entity& e) { return e.kind == Entity::Kind::Chain && e.points.size() < 2; }),
                          result.entities.end());
    for (const auto& [what, count] : skipped)
        result.notes.push_back(std::to_string(count) + " × " + what + " left out");
    if (result.entities.empty())
        result.error = "the SVG file has no shapes to bring in";
    return result;
}

// ---------------------------------------------------------------- DXF out

std::string write_dxf(const Sketch& sketch) {
    std::string out;
    const auto group = [&](int code, const std::string& value) {
        out += std::to_string(code);
        out += '\n';
        out += value;
        out += '\n';
    };
    const auto num = [&](int code, double v) {
        char buf[64];
        const auto [end, ec] = std::to_chars(buf, buf + sizeof buf, std::abs(v) < 1e-12 ? 0.0 : v, std::chars_format::general, 12);
        group(code, ec == std::errc() ? std::string(buf, end) : std::string("0"));
    };
    group(0, "SECTION");
    group(2, "HEADER");
    group(9, "$ACADVER");
    group(1, "AC1009");
    group(9, "$INSUNITS");
    group(70, "4");
    group(0, "ENDSEC");
    group(0, "SECTION");
    group(2, "ENTITIES");
    for (const Entity& e : sketch.entities) {
        const std::string layer = e.construction ? "CONSTRUCTION" : "0";
        const auto poly = [&](const std::vector<Pt>& pts, bool closed) {
            if (pts.size() == 2 && !closed) {
                group(0, "LINE");
                group(8, layer);
                num(10, pts[0].x), num(20, pts[0].y), num(30, 0);
                num(11, pts[1].x), num(21, pts[1].y), num(31, 0);
                return;
            }
            group(0, "POLYLINE");
            group(8, layer);
            group(66, "1");
            num(10, 0), num(20, 0), num(30, 0);
            group(70, closed ? "1" : "0");
            for (const Pt& p : pts) {
                group(0, "VERTEX");
                group(8, layer);
                num(10, p.x), num(20, p.y), num(30, 0);
            }
            group(0, "SEQEND");
            group(8, layer);
        };
        switch (e.kind) {
        case Entity::Kind::Chain:
            if (e.points.size() >= 2)
                poly(e.points, e.closed);
            break;
        case Entity::Kind::Rectangle:
            if (e.points.size() >= 2) {
                const Pt a = e.points[0], b = e.points[1];
                poly({a, {b.x, a.y}, b, {a.x, b.y}}, true);
            }
            break;
        case Entity::Kind::Circle:
            if (!e.points.empty()) {
                group(0, "CIRCLE");
                group(8, layer);
                num(10, e.points[0].x), num(20, e.points[0].y), num(30, 0);
                num(40, e.radius);
            }
            break;
        case Entity::Kind::Arc:
            if (e.points.size() >= 3) {
                const Pt c = e.points[0];
                double a0 = std::atan2(e.points[1].y - c.y, e.points[1].x - c.x) * 180.0 / kPi;
                double a1 = std::atan2(e.points[2].y - c.y, e.points[2].x - c.x) * 180.0 / kPi;
                if (e.clockwise)
                    std::swap(a0, a1); // DXF arcs run counter-clockwise
                const double r = e.radius > 0 ? e.radius : std::hypot(e.points[1].x - c.x, e.points[1].y - c.y);
                group(0, "ARC");
                group(8, layer);
                num(10, c.x), num(20, c.y), num(30, 0);
                num(40, r);
                num(50, a0 < 0 ? a0 + 360 : a0);
                num(51, a1 < 0 ? a1 + 360 : a1);
            }
            break;
        }
    }
    group(0, "ENDSEC");
    group(0, "EOF");
    return out;
}

} // namespace tisma::cad
