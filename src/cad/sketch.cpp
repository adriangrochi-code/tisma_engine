// Sketches of the Design tab: the drafting-program input (typed points,
// snaps, ortho and polar) and the SolidCraft script of a sketch. Own design
// over SolidCraft's public command names; see PROVENANCE.md.

#include "cad/sketch.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <numbers>
#include <stdexcept>

namespace tisma::cad {

using json = nlohmann::ordered_json;

const char* plane_name(SketchPlane plane) {
    return plane == SketchPlane::XZ ? "XZ" : plane == SketchPlane::YZ ? "YZ" : "XY";
}

std::optional<SketchPlane> plane_named(const std::string& name) {
    if (name == "XY" || name == "xy")
        return SketchPlane::XY;
    if (name == "XZ" || name == "xz")
        return SketchPlane::XZ;
    if (name == "YZ" || name == "yz")
        return SketchPlane::YZ;
    return std::nullopt;
}

namespace {

Pt rect_corner(const Entity& e, std::size_t k) {
    const double x0 = std::min(e.points[0].x, e.points[1].x), x1 = std::max(e.points[0].x, e.points[1].x);
    const double y0 = std::min(e.points[0].y, e.points[1].y), y1 = std::max(e.points[0].y, e.points[1].y);
    switch (k % 4) {
    case 0: return {x0, y0};
    case 1: return {x1, y0};
    case 2: return {x1, y1};
    default: return {x0, y1};
    }
}

double dist(const Pt& a, const Pt& b) { return std::hypot(b.x - a.x, b.y - a.y); }

// The nearest point of segment ab to p, and where along it (0 to 1).
Pt nearest_on_segment(const Pt& p, const Pt& a, const Pt& b, double* t_out = nullptr) {
    const double dx = b.x - a.x, dy = b.y - a.y, l2 = dx * dx + dy * dy;
    const double t = l2 > 0.0 ? std::clamp(((p.x - a.x) * dx + (p.y - a.y) * dy) / l2, 0.0, 1.0) : 0.0;
    if (t_out)
        *t_out = t;
    return {a.x + t * dx, a.y + t * dy};
}

// How many lines an entity makes in SolidCraft, and how many circles and arcs.
std::size_t lines_of(const Entity& e) {
    switch (e.kind) {
    case Entity::Kind::Chain: return e.points.size() < 2 ? 0 : e.closed ? e.points.size() : e.points.size() - 1;
    case Entity::Kind::Rectangle: return 4;
    default: return 0;
    }
}

// The ids SolidCraft gives the entity's first line, circle or arc (from 1).
struct Ids {
    std::size_t line = 1, circle = 1, arc = 1;
};
Ids ids_before(const Sketch& s, std::size_t entity) {
    Ids ids;
    for (std::size_t i = 0; i < entity && i < s.entities.size(); ++i) {
        const Entity& e = s.entities[i];
        ids.line += lines_of(e);
        ids.circle += e.kind == Entity::Kind::Circle ? 1 : 0;
        ids.arc += e.kind == Entity::Kind::Arc ? 1 : 0;
    }
    return ids;
}

std::string curve_id(const Sketch& s, std::size_t entity, std::size_t segment) {
    if (entity >= s.entities.size())
        return "";
    const Entity& e = s.entities[entity];
    const Ids ids = ids_before(s, entity);
    switch (e.kind) {
    case Entity::Kind::Circle: return "c" + std::to_string(ids.circle);
    case Entity::Kind::Arc: return "a" + std::to_string(ids.arc);
    default: return "l" + std::to_string(ids.line + std::min(segment, std::max<std::size_t>(lines_of(e), 1) - 1));
    }
}

json pt(const Pt& p) { return json::array({p.x, p.y}); }

json command(const char* name, json params) {
    return json{{"command", name}, {"params", std::move(params)}};
}

} // namespace

namespace {
constexpr std::pair<GeoConstraint::Kind, const char*> geo_names[] = {
    {GeoConstraint::Kind::Horizontal, "horizontal"},
    {GeoConstraint::Kind::Vertical, "vertical"},
    {GeoConstraint::Kind::Parallel, "parallel"},
    {GeoConstraint::Kind::Perpendicular, "perpendicular"},
    {GeoConstraint::Kind::Equal, "equal"},
    {GeoConstraint::Kind::Concentric, "concentric"},
    {GeoConstraint::Kind::Tangent, "tangent"},
    {GeoConstraint::Kind::Coincident, "coincident"},
    {GeoConstraint::Kind::Fixed, "fixed"},
    {GeoConstraint::Kind::Length, "length"},
    {GeoConstraint::Kind::Distance, "distance"},
    {GeoConstraint::Kind::HorizontalDistance, "horizontal_distance"},
    {GeoConstraint::Kind::VerticalDistance, "vertical_distance"},
    {GeoConstraint::Kind::Radius, "radius"},
    {GeoConstraint::Kind::Diameter, "diameter"},
    {GeoConstraint::Kind::Angle, "angle"},
};
} // namespace

const char* geo_kind_name(GeoConstraint::Kind kind) {
    for (const auto& [k, name] : geo_names)
        if (k == kind)
            return name;
    return "";
}

std::optional<GeoConstraint::Kind> geo_kind_named(const std::string& name) {
    for (const auto& [k, n] : geo_names)
        if (name == n)
            return k;
    return std::nullopt;
}

bool is_dimension(GeoConstraint::Kind kind) {
    using K = GeoConstraint::Kind;
    return kind == K::Length || kind == K::Distance || kind == K::HorizontalDistance || kind == K::VerticalDistance ||
           kind == K::Radius || kind == K::Diameter || kind == K::Angle;
}

Pt point_of(const Entity& e, std::size_t index) {
    switch (e.kind) {
    case Entity::Kind::Chain: return e.points.empty() ? Pt{} : e.points[std::min(index, e.points.size() - 1)];
    case Entity::Kind::Rectangle: return e.points.size() < 2 ? Pt{} : rect_corner(e, index);
    case Entity::Kind::Circle: return e.points.empty() ? Pt{} : e.points[0];
    case Entity::Kind::Arc: return e.points.size() < 3 ? Pt{} : e.points[std::min<std::size_t>(index, 2)];
    }
    return {};
}

std::vector<std::pair<Pt, Pt>> segments_of(const Entity& e) {
    std::vector<std::pair<Pt, Pt>> out;
    if (e.kind == Entity::Kind::Chain) {
        for (std::size_t i = 0; i + 1 < e.points.size(); ++i)
            out.emplace_back(e.points[i], e.points[i + 1]);
        if (e.closed && e.points.size() > 2)
            out.emplace_back(e.points.back(), e.points.front());
    } else if (e.kind == Entity::Kind::Rectangle && e.points.size() >= 2) {
        for (std::size_t k = 0; k < 4; ++k)
            out.emplace_back(rect_corner(e, k), rect_corner(e, k + 1));
    }
    return out;
}

bool Sketch::has_region() const {
    for (const Entity& e : entities) {
        if (e.construction)
            continue;
        if (e.kind == Entity::Kind::Chain && e.closed && e.points.size() >= 3)
            return true;
        if (e.kind == Entity::Kind::Rectangle && e.points.size() >= 2 && e.points[0].x != e.points[1].x && e.points[0].y != e.points[1].y)
            return true;
        if (e.kind == Entity::Kind::Circle && e.radius > 0.0)
            return true;
    }
    return !joined_loops(*this).empty();
}

std::vector<std::vector<Pt>> joined_loops(const Sketch& sketch) {
    constexpr double kNear = 1e-6;
    const auto near = [](const Pt& a, const Pt& b) { return std::hypot(a.x - b.x, a.y - b.y) <= kNear; };
    // The pieces as polylines from one end to the other.
    std::vector<std::vector<Pt>> pieces;
    for (const Entity& e : sketch.entities) {
        if (e.construction)
            continue;
        if (e.kind == Entity::Kind::Chain && !e.closed && e.points.size() >= 2) {
            pieces.push_back(e.points);
        } else if (e.kind == Entity::Kind::Arc && e.points.size() >= 3) {
            const Pt& c = e.points[0];
            const double r = std::hypot(e.points[1].x - c.x, e.points[1].y - c.y);
            if (!(r > 0.0))
                continue;
            const double a0 = std::atan2(e.points[1].y - c.y, e.points[1].x - c.x);
            double sweep = std::atan2(e.points[2].y - c.y, e.points[2].x - c.x) - a0; // counter-clockwise
            if (e.clockwise) {
                while (sweep >= 0.0)
                    sweep -= 2.0 * std::numbers::pi;
            } else {
                while (sweep <= 0.0)
                    sweep += 2.0 * std::numbers::pi;
            }
            const int n = std::max(4, int(std::ceil(std::abs(sweep) / (2.0 * std::numbers::pi) * 720.0)));
            std::vector<Pt> q;
            for (int k = 0; k <= n; ++k) {
                const double a = a0 + sweep * k / n;
                q.push_back({c.x + r * std::cos(a), c.y + r * std::sin(a)});
            }
            q.front() = e.points[1];
            q.back() = e.points[2];
            pieces.push_back(std::move(q));
        }
    }
    std::vector<std::vector<Pt>> loops;
    std::vector<bool> used(pieces.size(), false);
    for (std::size_t i = 0; i < pieces.size(); ++i) {
        if (used[i])
            continue;
        used[i] = true;
        std::vector<Pt> loop = pieces[i];
        std::vector<std::size_t> taken{i};
        // Walk on from the loose end until back at the start or stuck.
        for (;;) {
            if (loop.size() > 2 && near(loop.back(), loop.front())) {
                loop.pop_back();
                loops.push_back(std::move(loop));
                break;
            }
            std::optional<std::size_t> next;
            bool reversed = false;
            for (std::size_t j = 0; j < pieces.size() && !next; ++j) {
                if (used[j])
                    continue;
                if (near(pieces[j].front(), loop.back()))
                    next = j;
                else if (near(pieces[j].back(), loop.back()))
                    next = j, reversed = true;
            }
            if (!next) {
                // A dead end: its pieces may still close a loop with others.
                for (const std::size_t t : taken)
                    used[t] = t == i;
                break;
            }
            used[*next] = true;
            taken.push_back(*next);
            std::vector<Pt> add = pieces[*next];
            if (reversed)
                std::reverse(add.begin(), add.end());
            loop.insert(loop.end(), add.begin() + 1, add.end());
        }
    }
    return loops;
}

namespace {

std::string trimmed(std::string s) {
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t'))
        s.pop_back();
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
        s.erase(s.begin());
    return s;
}

std::optional<double> whole_number(const std::string& text, char decimal = '.') {
    std::string t = trimmed(text);
    if (t.empty())
        return std::nullopt;
    if (decimal == ',')
        std::replace(t.begin(), t.end(), ',', '.');
    char* end = nullptr;
    const double v = std::strtod(t.c_str(), &end);
    if (end != t.c_str() + t.size() || !std::isfinite(v))
        return std::nullopt;
    return v;
}

} // namespace

std::optional<Pt> parse_typed_point(const std::string& text, const Pt& last) {
    std::string t = trimmed(text);
    const bool relative = !t.empty() && t.front() == '@';
    if (relative)
        t.erase(t.begin());
    if (const std::size_t angle = t.find('<'); angle != std::string::npos) {
        if (!relative)
            return std::nullopt;
        const std::optional<double> d = whole_number(t.substr(0, angle)), a = whole_number(t.substr(angle + 1));
        if (!d || !a)
            return std::nullopt;
        const double rad = *a * std::numbers::pi / 180.0;
        return Pt{last.x + *d * std::cos(rad), last.y + *d * std::sin(rad)};
    }
    const std::size_t comma = t.find(',');
    if (comma == std::string::npos)
        return std::nullopt;
    const std::optional<double> x = whole_number(t.substr(0, comma)), y = whole_number(t.substr(comma + 1));
    if (!x || !y)
        return std::nullopt;
    return relative ? Pt{last.x + *x, last.y + *y} : Pt{*x, *y};
}

std::optional<double> parse_typed_value(const std::string& text) { return whole_number(text, ','); }

const char* snap_name(SnapKind kind) {
    switch (kind) {
    case SnapKind::None: return "none";
    case SnapKind::Endpoint: return "endpoint";
    case SnapKind::Midpoint: return "midpoint";
    case SnapKind::Center: return "center";
    case SnapKind::Quadrant: return "quadrant";
    case SnapKind::Intersection: return "intersection";
    case SnapKind::Perpendicular: return "perpendicular";
    case SnapKind::Tangent: return "tangent";
    case SnapKind::Nearest: return "nearest";
    case SnapKind::Grid: return "grid";
    }
    return "";
}

Snap snap_point(const Sketch& sketch, const Pt& cursor, const SnapOptions& options) {
    Snap best;
    if (!options.enabled) {
        if (options.grid && options.grid_step > 0.0) {
            best.kind = SnapKind::Grid;
            best.at = {std::round(cursor.x / options.grid_step) * options.grid_step, std::round(cursor.y / options.grid_step) * options.grid_step};
        }
        return best;
    }
    struct Candidate {
        SnapKind kind;
        Pt at;
        std::size_t entity, index;
    };
    std::vector<Candidate> candidates;
    const auto add = [&](SnapKind kind, const Pt& at, std::size_t entity, std::size_t index) {
        if (dist(at, cursor) <= options.radius)
            candidates.push_back({kind, at, entity, index});
    };
    // Every segment and circle of the sketch, for the pairwise snaps.
    struct Seg {
        Pt a, b;
        std::size_t entity, index;
    };
    std::vector<Seg> segs;
    struct Circ {
        Pt c;
        double r;
        std::size_t entity;
    };
    std::vector<Circ> circs;
    for (std::size_t i = 0; i < sketch.entities.size(); ++i) {
        const Entity& e = sketch.entities[i];
        switch (e.kind) {
        case Entity::Kind::Chain:
            for (std::size_t k = 0; k < e.points.size(); ++k)
                add(SnapKind::Endpoint, e.points[k], i, k);
            break;
        case Entity::Kind::Rectangle:
            if (e.points.size() >= 2)
                for (std::size_t k = 0; k < 4; ++k)
                    add(SnapKind::Endpoint, rect_corner(e, k), i, k);
            break;
        case Entity::Kind::Circle:
            if (!e.points.empty()) {
                add(SnapKind::Center, e.points[0], i, 0);
                for (std::size_t q = 0; q < 4; ++q) {
                    const double a = q * std::numbers::pi / 2.0;
                    add(SnapKind::Quadrant, {e.points[0].x + e.radius * std::cos(a), e.points[0].y + e.radius * std::sin(a)}, i, q);
                }
                circs.push_back({e.points[0], e.radius, i});
            }
            break;
        case Entity::Kind::Arc:
            if (e.points.size() >= 3) {
                add(SnapKind::Center, e.points[0], i, 0);
                add(SnapKind::Endpoint, e.points[1], i, 1);
                add(SnapKind::Endpoint, e.points[2], i, 2);
            }
            break;
        }
        const std::vector<std::pair<Pt, Pt>> ss = segments_of(e);
        for (std::size_t k = 0; k < ss.size(); ++k) {
            segs.push_back({ss[k].first, ss[k].second, i, k});
            add(SnapKind::Midpoint, {(ss[k].first.x + ss[k].second.x) / 2.0, (ss[k].first.y + ss[k].second.y) / 2.0}, i, k);
        }
    }
    // Intersections: segment with segment, segment with circle, circle with circle.
    for (std::size_t i = 0; i < segs.size(); ++i) {
        for (std::size_t j = i + 1; j < segs.size(); ++j) {
            const Seg &p = segs[i], &q = segs[j];
            const double rx = p.b.x - p.a.x, ry = p.b.y - p.a.y, sx = q.b.x - q.a.x, sy = q.b.y - q.a.y;
            const double den = rx * sy - ry * sx;
            if (std::abs(den) < 1e-12)
                continue;
            const double t = ((q.a.x - p.a.x) * sy - (q.a.y - p.a.y) * sx) / den;
            const double u = ((q.a.x - p.a.x) * ry - (q.a.y - p.a.y) * rx) / den;
            if (t >= -1e-9 && t <= 1.0 + 1e-9 && u >= -1e-9 && u <= 1.0 + 1e-9)
                add(SnapKind::Intersection, {p.a.x + t * rx, p.a.y + t * ry}, p.entity, p.index);
        }
        for (const Circ& c : circs) {
            const double dx = segs[i].b.x - segs[i].a.x, dy = segs[i].b.y - segs[i].a.y;
            const double fx = segs[i].a.x - c.c.x, fy = segs[i].a.y - c.c.y;
            const double A = dx * dx + dy * dy, B = 2.0 * (fx * dx + fy * dy), C = fx * fx + fy * fy - c.r * c.r;
            const double disc = B * B - 4.0 * A * C;
            if (A < 1e-12 || disc < 0.0)
                continue;
            for (const double t : {(-B - std::sqrt(disc)) / (2.0 * A), (-B + std::sqrt(disc)) / (2.0 * A)})
                if (t >= -1e-9 && t <= 1.0 + 1e-9)
                    add(SnapKind::Intersection, {segs[i].a.x + t * dx, segs[i].a.y + t * dy}, segs[i].entity, segs[i].index);
        }
    }
    for (std::size_t i = 0; i < circs.size(); ++i)
        for (std::size_t j = i + 1; j < circs.size(); ++j) {
            const Circ &p = circs[i], &q = circs[j];
            const double d = dist(p.c, q.c);
            if (d < 1e-9 || d > p.r + q.r || d < std::abs(p.r - q.r))
                continue;
            const double a = (p.r * p.r - q.r * q.r + d * d) / (2.0 * d);
            const double h = std::sqrt(std::max(0.0, p.r * p.r - a * a));
            const Pt m{p.c.x + a * (q.c.x - p.c.x) / d, p.c.y + a * (q.c.y - p.c.y) / d};
            const Pt n{-(q.c.y - p.c.y) / d, (q.c.x - p.c.x) / d};
            add(SnapKind::Intersection, {m.x + h * n.x, m.y + h * n.y}, p.entity, 0);
            add(SnapKind::Intersection, {m.x - h * n.x, m.y - h * n.y}, p.entity, 0);
        }
    // Perpendicular and tangent from the segment's start.
    if (options.from) {
        const Pt f = *options.from;
        for (const Seg& s : segs) {
            double t = 0.0;
            const Pt foot = nearest_on_segment(f, s.a, s.b, &t);
            if (t > 1e-9 && t < 1.0 - 1e-9)
                add(SnapKind::Perpendicular, foot, s.entity, s.index);
        }
        for (const Circ& c : circs) {
            const double d = dist(f, c.c);
            if (d < 1e-9)
                continue;
            const Pt dir{(f.x - c.c.x) / d, (f.y - c.c.y) / d};
            add(SnapKind::Perpendicular, {c.c.x + c.r * dir.x, c.c.y + c.r * dir.y}, c.entity, 0);
            add(SnapKind::Perpendicular, {c.c.x - c.r * dir.x, c.c.y - c.r * dir.y}, c.entity, 0);
            if (d > c.r) {
                const double a = std::acos(c.r / d), base = std::atan2(dir.y, dir.x);
                for (const double s : {base + a, base - a})
                    add(SnapKind::Tangent, {c.c.x + c.r * std::cos(s), c.c.y + c.r * std::sin(s)}, c.entity, 0);
            }
        }
    }
    // Nearest on a curve.
    for (const Seg& s : segs)
        add(SnapKind::Nearest, nearest_on_segment(cursor, s.a, s.b), s.entity, s.index);
    for (const Circ& c : circs) {
        const double d = dist(cursor, c.c);
        if (d > 1e-9)
            add(SnapKind::Nearest, {c.c.x + c.r * (cursor.x - c.c.x) / d, c.c.y + c.r * (cursor.y - c.c.y) / d}, c.entity, 0);
    }
    // The strongest kind that has a candidate, its nearest one.
    for (const SnapKind kind : {SnapKind::Endpoint, SnapKind::Midpoint, SnapKind::Center, SnapKind::Quadrant, SnapKind::Intersection,
                                SnapKind::Perpendicular, SnapKind::Tangent, SnapKind::Nearest}) {
        const Candidate* nearest = nullptr;
        for (const Candidate& c : candidates)
            if (c.kind == kind && (!nearest || dist(c.at, cursor) < dist(nearest->at, cursor)))
                nearest = &c;
        if (nearest) {
            best.kind = kind;
            best.at = nearest->at;
            best.entity = nearest->entity;
            best.index = nearest->index;
            return best;
        }
    }
    if (options.grid && options.grid_step > 0.0) {
        best.kind = SnapKind::Grid;
        best.at = {std::round(cursor.x / options.grid_step) * options.grid_step, std::round(cursor.y / options.grid_step) * options.grid_step};
    }
    return best;
}

Pt tracked_point(const Pt& from, const Pt& cursor, bool ortho, double polar_step, double polar_tolerance) {
    const double dx = cursor.x - from.x, dy = cursor.y - from.y;
    if (ortho)
        return std::abs(dx) >= std::abs(dy) ? Pt{cursor.x, from.y} : Pt{from.x, cursor.y};
    if (polar_step > 0.0) {
        const double length = std::hypot(dx, dy);
        if (length < 1e-9)
            return cursor;
        const double angle = std::atan2(dy, dx) * 180.0 / std::numbers::pi;
        const double snapped = std::round(angle / polar_step) * polar_step;
        double off = std::fmod(std::abs(angle - snapped), 360.0);
        off = std::min(off, 360.0 - off);
        if (off <= polar_tolerance) {
            const double rad = snapped * std::numbers::pi / 180.0;
            return {from.x + length * std::cos(rad), from.y + length * std::sin(rad)};
        }
    }
    return cursor;
}

std::string point_id(const Sketch& sketch, const EntityPoint& p) {
    if (p.entity >= sketch.entities.size())
        return "";
    const Entity& e = sketch.entities[p.entity];
    const Ids ids = ids_before(sketch, p.entity);
    switch (e.kind) {
    case Entity::Kind::Chain: {
        const std::size_t n = e.points.size();
        if (n < 2)
            return "";
        if (!e.closed && p.index + 1 >= n)
            return "l" + std::to_string(ids.line + n - 2) + ".end";
        return "l" + std::to_string(ids.line + std::min(p.index, n - 1)) + ".start";
    }
    case Entity::Kind::Rectangle:
        // Sides bottom, right, top, left (SolidCraft's order), each from its
        // first corner counter-clockwise.
        switch (p.index % 4) {
        case 0: return "l" + std::to_string(ids.line) + ".start";
        case 1: return "l" + std::to_string(ids.line) + ".end";
        case 2: return "l" + std::to_string(ids.line + 1) + ".end";
        default: return "l" + std::to_string(ids.line + 2) + ".end";
        }
    case Entity::Kind::Circle: return "c" + std::to_string(ids.circle) + ".center";
    case Entity::Kind::Arc:
        return "a" + std::to_string(ids.arc) + (p.index == 1 ? ".start" : p.index == 2 ? ".end" : ".center");
    }
    return "";
}

std::string line_id(const Sketch& sketch, std::size_t entity, std::size_t segment) { return curve_id(sketch, entity, segment); }

std::vector<std::string> sketch_commands(const Sketch& sketch, const SketchOperation& op, const std::string& body) {
    std::vector<json> out;
    out.push_back(command("sketch.create", {{"plane", plane_name(sketch.plane)}, {"name", body + " sketch"}}));
    for (const Entity& e : sketch.entities) {
        switch (e.kind) {
        case Entity::Kind::Chain: {
            if (e.points.size() < 2)
                break;
            json points = json::array();
            for (const Pt& p : e.points)
                points.push_back(pt(p));
            json params{{"points", points}, {"closed", e.closed}, {"infer", true}};
            if (e.construction)
                params["construction"] = true;
            out.push_back(command("sketch.line", params));
            break;
        }
        case Entity::Kind::Rectangle:
            if (e.points.size() >= 2) {
                json params{{"p0", pt(rect_corner(e, 0))}, {"p1", pt(rect_corner(e, 2))}};
                if (e.construction)
                    params["construction"] = true;
                out.push_back(command("sketch.rectangle.two_point", params));
            }
            break;
        case Entity::Kind::Circle:
            if (!e.points.empty() && e.radius > 0.0) {
                json params{{"center", pt(e.points[0])}, {"radius", e.radius}};
                if (e.construction)
                    params["construction"] = true;
                out.push_back(command("sketch.circle.center", params));
            }
            break;
        case Entity::Kind::Arc:
            if (e.points.size() >= 3) {
                json params{{"center", pt(e.points[0])}, {"start", pt(e.points[1])}, {"end", pt(e.points[2])}};
                if (e.clockwise)
                    params["clockwise"] = true;
                if (e.construction)
                    params["construction"] = true;
                out.push_back(command("sketch.arc.center_point", params));
            }
            break;
        }
    }
    for (const Constraint& c : sketch.constraints) {
        const std::string a = point_id(sketch, c.point);
        if (a.empty() || c.target >= sketch.entities.size())
            continue;
        switch (c.kind) {
        case Constraint::Kind::Coincident:
            if (const std::string b = point_id(sketch, {c.target, c.target_index}); !b.empty())
                out.push_back(command("sketch.constraint.coincident", {{"a", a}, {"b", b}}));
            break;
        case Constraint::Kind::Midpoint:
            if (const std::string line = curve_id(sketch, c.target, c.target_index); !line.empty())
                out.push_back(command("sketch.constraint.midpoint", {{"point", a}, {"line", line}}));
            break;
        case Constraint::Kind::OnCurve:
            if (const std::string curve = curve_id(sketch, c.target, c.target_index); !curve.empty())
                out.push_back(command("sketch.constraint.coincident", {{"a", a}, {"b", curve}}));
            break;
        }
    }
    for (const Dimension& d : sketch.dimensions)
        if (const std::string line = curve_id(sketch, d.entity, d.segment); !line.empty() && d.length > 0.0)
            out.push_back(command("sketch.dimension", {{"entities", json::array({line})}, {"type", "length"}, {"value", d.length}}));
    out.push_back(command("sketch.finish", json::object()));
    if (op.kind == SketchOperation::Kind::Extrude)
        out.push_back(command("solid.extrude", {{"distance", op.distance}, {"operation", "new"}, {"body_name", body}, {"name", body}}));
    else
        out.push_back(command("solid.revolve", {{"axis", std::string(1, op.axis == 'x' ? 'x' : 'y')},
                                                {"angle", op.angle},
                                                {"operation", "new"},
                                                {"body_name", body},
                                                {"name", body}}));
    std::vector<std::string> text;
    text.reserve(out.size());
    for (const json& c : out)
        text.push_back(c.dump());
    return text;
}

std::string sketch_json(const Sketch& sketch, const SketchOperation& op) {
    json entities = json::array();
    for (const Entity& e : sketch.entities) {
        json points = json::array();
        for (const Pt& p : e.points)
            points.push_back(pt(p));
        json j{{"kind", e.kind == Entity::Kind::Chain       ? "chain"
                        : e.kind == Entity::Kind::Rectangle ? "rectangle"
                        : e.kind == Entity::Kind::Circle    ? "circle"
                                                            : "arc"},
               {"points", points}};
        if (e.kind == Entity::Kind::Circle)
            j["radius"] = e.radius;
        if (e.kind == Entity::Kind::Chain)
            j["closed"] = e.closed;
        if (e.kind == Entity::Kind::Arc)
            j["clockwise"] = e.clockwise;
        if (e.construction)
            j["construction"] = true;
        entities.push_back(std::move(j));
    }
    json constraints = json::array();
    for (const Constraint& c : sketch.constraints)
        constraints.push_back({{"kind", c.kind == Constraint::Kind::Coincident ? "coincident"
                                        : c.kind == Constraint::Kind::Midpoint ? "midpoint"
                                                                               : "on_curve"},
                               {"entity", c.point.entity},
                               {"index", c.point.index},
                               {"target", c.target},
                               {"target_index", c.target_index}});
    json dimensions = json::array();
    for (const Dimension& d : sketch.dimensions)
        dimensions.push_back({{"entity", d.entity}, {"segment", d.segment}, {"length", d.length}});
    json rules = json::array();
    for (const GeoConstraint& r : sketch.rules) {
        json j{{"kind", geo_kind_name(r.kind)}, {"a", json::array({r.a.entity, r.a.index})},
               {"b", json::array({r.b.entity, r.b.index})}};
        if (is_dimension(r.kind))
            j["value"] = r.value;
        if (r.kind == GeoConstraint::Kind::Fixed)
            j["at"] = pt(r.at);
        rules.push_back(std::move(j));
    }
    json operation = op.kind == SketchOperation::Kind::Extrude
                         ? json{{"kind", "extrude"}, {"distance", op.distance}}
                         : json{{"kind", "revolve"}, {"axis", std::string(1, op.axis)}, {"angle", op.angle}};
    return json{{"plane", plane_name(sketch.plane)},
                {"entities", std::move(entities)},
                {"constraints", std::move(constraints)},
                {"dimensions", std::move(dimensions)},
                {"rules", std::move(rules)},
                {"operation", std::move(operation)}}
        .dump();
}

void parse_sketch(const std::string& text, Sketch& sketch, SketchOperation& op) {
    json j;
    try {
        j = json::parse(text);
    } catch (const json::exception& e) {
        throw std::runtime_error(std::string("the sketch is not JSON: ") + e.what());
    }
    if (!j.is_object())
        throw std::runtime_error("the sketch is not an object");
    Sketch s;
    SketchOperation o;
    const std::optional<SketchPlane> plane = plane_named(j.value("plane", std::string("XY")));
    if (!plane)
        throw std::runtime_error("unknown sketch plane");
    s.plane = *plane;
    const auto read_pt = [](const json& v) {
        if (!v.is_array() || v.size() != 2 || !v[0].is_number() || !v[1].is_number())
            throw std::runtime_error("a sketch point is not [x, y]");
        return Pt{v[0].get<double>(), v[1].get<double>()};
    };
    for (const json& e : j.value("entities", json::array())) {
        Entity entity;
        const std::string kind = e.value("kind", std::string());
        entity.kind = kind == "chain"       ? Entity::Kind::Chain
                      : kind == "rectangle" ? Entity::Kind::Rectangle
                      : kind == "circle"    ? Entity::Kind::Circle
                      : kind == "arc"       ? Entity::Kind::Arc
                                            : throw std::runtime_error("unknown sketch entity \"" + kind + "\"");
        for (const json& p : e.value("points", json::array()))
            entity.points.push_back(read_pt(p));
        entity.radius = e.value("radius", 0.0);
        entity.closed = e.value("closed", false);
        entity.clockwise = e.value("clockwise", false);
        entity.construction = e.value("construction", false);
        s.entities.push_back(std::move(entity));
    }
    for (const json& c : j.value("constraints", json::array())) {
        Constraint constraint;
        const std::string kind = c.value("kind", std::string());
        constraint.kind = kind == "coincident" ? Constraint::Kind::Coincident
                          : kind == "midpoint" ? Constraint::Kind::Midpoint
                          : kind == "on_curve" ? Constraint::Kind::OnCurve
                                               : throw std::runtime_error("unknown constraint \"" + kind + "\"");
        constraint.point = {c.value("entity", std::size_t(0)), c.value("index", std::size_t(0))};
        constraint.target = c.value("target", std::size_t(0));
        constraint.target_index = c.value("target_index", std::size_t(0));
        s.constraints.push_back(constraint);
    }
    for (const json& d : j.value("dimensions", json::array()))
        s.dimensions.push_back({d.value("entity", std::size_t(0)), d.value("segment", std::size_t(0)), d.value("length", 0.0)});
    const auto read_ref = [](const json& v) {
        if (!v.is_array() || v.size() != 2 || !v[0].is_number_unsigned() || !v[1].is_number_unsigned())
            throw std::runtime_error("a constraint's reference is not [entity, index]");
        return EntityPoint{v[0].get<std::size_t>(), v[1].get<std::size_t>()};
    };
    for (const json& r : j.value("rules", json::array())) {
        const std::string kind = r.value("kind", std::string());
        const std::optional<GeoConstraint::Kind> k = geo_kind_named(kind);
        if (!k)
            throw std::runtime_error("unknown sketch constraint \"" + kind + "\"");
        GeoConstraint rule;
        rule.kind = *k;
        rule.a = read_ref(r.value("a", json::array({0, 0})));
        rule.b = read_ref(r.value("b", json::array({0, 0})));
        rule.value = r.value("value", 0.0);
        if (r.contains("at"))
            rule.at = read_pt(r["at"]);
        s.rules.push_back(rule);
    }
    const json operation = j.value("operation", json::object());
    const std::string kind = operation.value("kind", std::string("extrude"));
    if (kind == "extrude") {
        o.kind = SketchOperation::Kind::Extrude;
        o.distance = operation.value("distance", 20.0);
    } else if (kind == "revolve") {
        o.kind = SketchOperation::Kind::Revolve;
        const std::string axis = operation.value("axis", std::string("y"));
        o.axis = axis == "x" ? 'x' : 'y';
        o.angle = operation.value("angle", 360.0);
    } else {
        throw std::runtime_error("unknown sketch operation \"" + kind + "\"");
    }
    sketch = std::move(s);
    op = o;
}

} // namespace tisma::cad
