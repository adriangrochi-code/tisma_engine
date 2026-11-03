#include "cad/sketch_edit.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace tisma::cad {

namespace {

constexpr double kEps = 1e-9;
constexpr double kTwoPi = 2.0 * std::numbers::pi;

double dist(const Pt& a, const Pt& b) { return std::hypot(b.x - a.x, b.y - a.y); }
Pt lerp(const Pt& a, const Pt& b, double t) { return {a.x + t * (b.x - a.x), a.y + t * (b.y - a.y)}; }
double cross(const Pt& a, const Pt& b) { return a.x * b.y - a.y * b.x; }
Pt sub(const Pt& a, const Pt& b) { return {a.x - b.x, a.y - b.y}; }

// An angle in [0, 2 pi).
double wrap(double a) {
    a = std::fmod(a, kTwoPi);
    return a < 0.0 ? a + kTwoPi : a;
}

bool two_entity_rule(GeoConstraint::Kind k) {
    using K = GeoConstraint::Kind;
    return k == K::Parallel || k == K::Perpendicular || k == K::Equal || k == K::Concentric || k == K::Tangent ||
           k == K::Coincident || k == K::Distance || k == K::HorizontalDistance || k == K::VerticalDistance || k == K::Angle;
}

bool rule_on(const GeoConstraint& r, std::size_t e) { return r.a.entity == e || (two_entity_rule(r.kind) && r.b.entity == e); }

// The constraints, dimensions and rules that name the entity, dropped: its
// shape changed under them.
void drop_rules_on(Sketch& s, std::size_t e) {
    std::erase_if(s.constraints, [e](const Constraint& c) { return c.point.entity == e || c.target == e; });
    std::erase_if(s.dimensions, [e](const Dimension& d) { return d.entity == e; });
    std::erase_if(s.rules, [e](const GeoConstraint& r) { return rule_on(r, e); });
}

// A curve of the sketch: a segment, or a circle or an arc (an arc from a0
// sweeping `sweep` radians counter-clockwise; a circle sweeps 2 pi).
struct Curve {
    std::size_t entity = 0, segment = 0;
    bool round = false;
    Pt a, b;                 // segment
    Pt c;                    // circle, arc
    double r = 0.0, a0 = 0.0, sweep = kTwoPi;
};

// An arc's start angle and counter-clockwise sweep (a clockwise arc from s
// to e is the counter-clockwise one from e to s).
void arc_span(const Entity& e, double& a0, double& sweep) {
    const Pt& c = e.points[0];
    const Pt& s = e.clockwise ? e.points[2] : e.points[1];
    const Pt& t = e.clockwise ? e.points[1] : e.points[2];
    a0 = wrap(std::atan2(s.y - c.y, s.x - c.x));
    sweep = wrap(std::atan2(t.y - c.y, t.x - c.x) - a0);
    if (sweep < kEps)
        sweep = kTwoPi;
}

std::vector<Curve> curves_of(const Sketch& s, std::size_t i) {
    std::vector<Curve> out;
    const Entity& e = s.entities[i];
    if (e.kind == Entity::Kind::Circle && !e.points.empty()) {
        Curve c;
        c.entity = i, c.round = true, c.c = e.points[0], c.r = e.radius;
        out.push_back(c);
    } else if (e.kind == Entity::Kind::Arc && e.points.size() >= 3) {
        Curve c;
        c.entity = i, c.round = true, c.c = e.points[0], c.r = dist(e.points[0], e.points[1]);
        arc_span(e, c.a0, c.sweep);
        out.push_back(c);
    } else {
        const auto segs = segments_of(e);
        for (std::size_t k = 0; k < segs.size(); ++k) {
            Curve c;
            c.entity = i, c.segment = k, c.a = segs[k].first, c.b = segs[k].second;
            out.push_back(c);
        }
    }
    return out;
}

// A segment that is no entity's (a ray, a side of the selection box).
Curve segment_curve(const Pt& a, const Pt& b) {
    Curve c;
    c.entity = std::numeric_limits<std::size_t>::max();
    c.a = a;
    c.b = b;
    return c;
}

bool on_arc(const Curve& c, const Pt& p) {
    if (c.sweep >= kTwoPi - kEps)
        return true;
    return wrap(std::atan2(p.y - c.c.y, p.x - c.c.x) - c.a0) <= c.sweep + 1e-9;
}

// Where two curves meet.
std::vector<Pt> meet(const Curve& u, const Curve& v) {
    std::vector<Pt> out;
    if (!u.round && !v.round) {
        const Pt r = sub(u.b, u.a), q = sub(v.b, v.a);
        const double den = cross(r, q);
        if (std::abs(den) < kEps)
            return out;
        const Pt w = sub(v.a, u.a);
        const double t = cross(w, q) / den, s = cross(w, r) / den;
        if (t >= -1e-9 && t <= 1 + 1e-9 && s >= -1e-9 && s <= 1 + 1e-9)
            out.push_back(lerp(u.a, u.b, t));
        return out;
    }
    if (u.round && !v.round)
        return meet(v, u);
    if (!u.round) { // a segment and a circle
        const Pt d = sub(u.b, u.a), f = sub(u.a, v.c);
        const double A = d.x * d.x + d.y * d.y, B = 2 * (f.x * d.x + f.y * d.y), C = f.x * f.x + f.y * f.y - v.r * v.r;
        const double disc = B * B - 4 * A * C;
        if (A < kEps || disc < 0)
            return out;
        const double root = std::sqrt(disc);
        for (const double t : {(-B - root) / (2 * A), (-B + root) / (2 * A)}) {
            if (t < -1e-9 || t > 1 + 1e-9)
                continue;
            const Pt p = lerp(u.a, u.b, t);
            if (on_arc(v, p) && (out.empty() || dist(out.back(), p) > 1e-9))
                out.push_back(p);
        }
        return out;
    }
    // Two circles.
    const double dd = dist(u.c, v.c);
    if (dd < kEps || dd > u.r + v.r + 1e-9 || dd < std::abs(u.r - v.r) - 1e-9)
        return out;
    const double a = (u.r * u.r - v.r * v.r + dd * dd) / (2 * dd);
    const double h = std::sqrt(std::max(0.0, u.r * u.r - a * a));
    const Pt m = lerp(u.c, v.c, a / dd);
    const Pt n{-(v.c.y - u.c.y) / dd, (v.c.x - u.c.x) / dd};
    for (const double sgn : {1.0, -1.0}) {
        const Pt p{m.x + sgn * h * n.x, m.y + sgn * h * n.y};
        if (on_arc(u, p) && on_arc(v, p) && (out.empty() || dist(out.back(), p) > 1e-9))
            out.push_back(p);
    }
    return out;
}

// Whether two curves of the same entity are neighbours (they share a corner,
// which is no crossing).
bool neighbours(const Entity& e, const Curve& u, const Curve& v) {
    if (u.entity != v.entity)
        return false;
    if (u.segment == v.segment)
        return true;
    const std::size_t n = segments_of(e).size();
    const bool loop = e.kind == Entity::Kind::Rectangle || e.closed;
    const std::size_t lo = std::min(u.segment, v.segment), hi = std::max(u.segment, v.segment);
    return hi == lo + 1 || (loop && lo == 0 && hi + 1 == n);
}

// The crossings on curve `u` from every other curve.
std::vector<Pt> crossings_of(const Sketch& s, const Curve& u) {
    std::vector<Pt> out;
    for (std::size_t j = 0; j < s.entities.size(); ++j)
        for (const Curve& v : curves_of(s, j)) {
            if (neighbours(s.entities[u.entity], u, v))
                continue;
            for (const Pt& p : meet(u, v))
                out.push_back(p);
        }
    return out;
}

// Where along the curve a point is: 0 to 1 along a segment, radians from a0
// along a circle or an arc.
double param(const Curve& c, const Pt& p) {
    if (!c.round) {
        const Pt d = sub(c.b, c.a);
        const double l2 = d.x * d.x + d.y * d.y;
        return l2 > 0 ? std::clamp(((p.x - c.a.x) * d.x + (p.y - c.a.y) * d.y) / l2, 0.0, 1.0) : 0.0;
    }
    return wrap(std::atan2(p.y - c.c.y, p.x - c.c.x) - c.a0);
}

double distance_to(const Curve& c, const Pt& p) {
    if (!c.round)
        return dist(p, lerp(c.a, c.b, param(c, p)));
    const double radial = std::abs(dist(p, c.c) - c.r);
    if (on_arc(c, p))
        return radial;
    const Pt s{c.c.x + c.r * std::cos(c.a0), c.c.y + c.r * std::sin(c.a0)};
    const Pt e{c.c.x + c.r * std::cos(c.a0 + c.sweep), c.c.y + c.r * std::sin(c.a0 + c.sweep)};
    return std::min(dist(p, s), dist(p, e));
}

// A rectangle as the closed chain of its corners.
Entity as_chain(const Entity& e) {
    if (e.kind != Entity::Kind::Rectangle)
        return e;
    Entity c;
    c.kind = Entity::Kind::Chain;
    c.closed = true;
    c.construction = e.construction;
    for (std::size_t k = 0; k < 4; ++k)
        c.points.push_back(point_of(e, k));
    return c;
}

Entity arc_entity(const Pt& c, double r, double from, double to, bool construction) {
    Entity e;
    e.kind = Entity::Kind::Arc;
    e.construction = construction;
    e.points = {c, {c.x + r * std::cos(from), c.y + r * std::sin(from)}, {c.x + r * std::cos(to), c.y + r * std::sin(to)}};
    return e;
}

} // namespace

std::vector<Pt> crossings_on(const Sketch& sketch, std::size_t entity) {
    std::vector<Pt> out;
    if (entity >= sketch.entities.size())
        return out;
    for (const Curve& u : curves_of(sketch, entity))
        for (const Pt& p : crossings_of(sketch, u))
            out.push_back(p);
    return out;
}

void erase_entity(Sketch& s, std::size_t e) {
    if (e >= s.entities.size())
        return;
    drop_rules_on(s, e);
    s.entities.erase(s.entities.begin() + std::ptrdiff_t(e));
    const auto shift = [e](std::size_t& i) {
        if (i > e)
            --i;
    };
    for (Constraint& c : s.constraints)
        shift(c.point.entity), shift(c.target);
    for (Dimension& d : s.dimensions)
        shift(d.entity);
    for (GeoConstraint& r : s.rules) {
        shift(r.a.entity);
        if (two_entity_rule(r.kind))
            shift(r.b.entity);
    }
}

bool trim_at(Sketch& s, const Pt& pick, double tolerance) {
    // The curve under the pick.
    std::optional<Curve> hit;
    double best = tolerance;
    for (std::size_t i = 0; i < s.entities.size(); ++i)
        for (const Curve& c : curves_of(s, i))
            if (const double d = distance_to(c, pick); d <= best)
                best = d, hit = c;
    if (!hit)
        return false;
    const Curve u = *hit;
    const std::size_t i = u.entity;
    const Entity old = s.entities[i];
    // The crossings either side of the pick, as parameters along the curve.
    const double tp = param(u, pick);
    std::vector<double> ts;
    for (const Pt& p : crossings_of(s, u))
        ts.push_back(param(u, p));
    if (u.round) {
        const bool circle = old.kind == Entity::Kind::Circle;
        std::vector<double> inner; // the crossings inside the arc, not at its ends
        for (const double t : ts)
            if (circle || (t > 1e-7 && t < u.sweep - 1e-7))
                inner.push_back(t);
        std::sort(inner.begin(), inner.end());
        if (circle) {
            if (inner.size() < 2) { // nothing cuts it into pieces: it goes whole
                erase_entity(s, i);
                return true;
            }
            // The piece around the pick goes; the arc from its upper crossing
            // round to its lower one stays.
            double lo = inner.back() - kTwoPi, hi = inner.front();
            for (const double t : inner) {
                if (t <= tp)
                    lo = t;
                if (t > tp) {
                    hi = t;
                    break;
                }
            }
            if (tp > inner.back())
                hi = inner.front() + kTwoPi;
            drop_rules_on(s, i);
            s.entities[i] = arc_entity(u.c, u.r, u.a0 + hi, u.a0 + lo + kTwoPi, old.construction);
            return true;
        }
        double lo = 0.0, hi = u.sweep;
        for (const double t : inner) {
            if (t < tp)
                lo = std::max(lo, t);
            else
                hi = std::min(hi, t);
        }
        drop_rules_on(s, i);
        std::vector<Entity> pieces;
        if (lo > 1e-7)
            pieces.push_back(arc_entity(u.c, u.r, u.a0, u.a0 + lo, old.construction));
        if (hi < u.sweep - 1e-7)
            pieces.push_back(arc_entity(u.c, u.r, u.a0 + hi, u.a0 + u.sweep, old.construction));
        if (pieces.empty()) {
            erase_entity(s, i);
            return true;
        }
        s.entities[i] = pieces[0];
        if (pieces.size() > 1)
            s.entities.push_back(pieces[1]);
        return true;
    }
    // A segment of a chain (a rectangle as one).
    double lo = 0.0, hi = 1.0;
    for (const double t : ts) {
        if (t > 1e-9 && t < tp)
            lo = std::max(lo, t);
        if (t < 1 - 1e-9 && t > tp)
            hi = std::min(hi, t);
    }
    const Entity chain = as_chain(old);
    const std::vector<Pt>& p = chain.points;
    const std::size_t n = p.size(), k = u.segment;
    const Pt a = p[k], b = p[(k + 1) % n];
    const Pt x1 = lerp(a, b, lo), x2 = lerp(a, b, hi);
    std::vector<std::vector<Pt>> parts;
    if (chain.closed) {
        // Opened at the cut: from x2 round to x1.
        std::vector<Pt> q;
        if (hi < 1.0)
            q.push_back(x2);
        for (std::size_t m = 1; m <= n; ++m)
            q.push_back(p[(k + m) % n]);
        if (lo > 0.0)
            q.push_back(x1);
        parts.push_back(std::move(q));
    } else {
        std::vector<Pt> first(p.begin(), p.begin() + std::ptrdiff_t(k) + 1), second;
        if (lo > 0.0)
            first.push_back(x1);
        if (hi < 1.0)
            second.push_back(x2);
        second.insert(second.end(), p.begin() + std::ptrdiff_t(k) + 1, p.end());
        parts.push_back(std::move(first));
        parts.push_back(std::move(second));
    }
    std::erase_if(parts, [](const std::vector<Pt>& q) { return q.size() < 2; });
    drop_rules_on(s, i);
    if (parts.empty()) {
        erase_entity(s, i);
        return true;
    }
    for (std::size_t m = 0; m < parts.size(); ++m) {
        Entity e;
        e.kind = Entity::Kind::Chain;
        e.construction = old.construction;
        e.points = std::move(parts[m]);
        if (m == 0)
            s.entities[i] = std::move(e);
        else
            s.entities.push_back(std::move(e));
    }
    return true;
}

bool extend_at(Sketch& s, const Pt& pick, double tolerance) {
    // The open chain end nearest the pick.
    std::optional<std::pair<std::size_t, bool>> end; // entity, its last point (else its first)
    double best = std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < s.entities.size(); ++i) {
        const Entity& e = s.entities[i];
        if (e.kind != Entity::Kind::Chain || e.closed || e.points.size() < 2)
            continue;
        bool near = false;
        for (const Curve& c : curves_of(s, i))
            near = near || distance_to(c, pick) <= tolerance;
        if (!near)
            continue;
        for (const bool last : {false, true}) {
            const double d = dist(pick, last ? e.points.back() : e.points.front());
            if (d < best)
                best = d, end = std::pair{i, last};
        }
    }
    if (!end)
        return false;
    Entity& e = s.entities[end->first];
    const bool last = end->second;
    const Pt from = last ? e.points.back() : e.points.front();
    const Pt prev = last ? e.points[e.points.size() - 2] : e.points[1];
    const Pt dir = sub(from, prev);
    const double len = std::hypot(dir.x, dir.y);
    if (len < kEps)
        return false;
    // A long ray ahead, met with every other curve; the nearest wins.
    double reach = 1.0;
    for (const Entity& f : s.entities)
        for (const Pt& q : f.points)
            reach += dist(from, q) + f.radius;
    const Curve ray = segment_curve(from, {from.x + dir.x / len * reach * 2, from.y + dir.y / len * reach * 2});
    std::optional<Pt> to;
    double nearest = std::numeric_limits<double>::infinity();
    // Its own last segment meets the ray only at the end (or along it,
    // which is no crossing).
    for (std::size_t j = 0; j < s.entities.size(); ++j)
        for (const Curve& v : curves_of(s, j)) {
            for (const Pt& q : meet(ray, v)) {
                const double d = dist(from, q);
                if (d > 1e-7 && d < nearest)
                    nearest = d, to = q;
            }
        }
    if (!to)
        return false;
    drop_rules_on(s, end->first);
    (last ? e.points.back() : e.points.front()) = *to;
    return true;
}

std::optional<std::size_t> offset_entity(Sketch& s, std::size_t i, double d, const Pt& side) {
    if (i >= s.entities.size() || !(d > 0.0))
        return std::nullopt;
    const Entity& e = s.entities[i];
    Entity out = e;
    switch (e.kind) {
    case Entity::Kind::Circle: {
        if (e.points.empty())
            return std::nullopt;
        const double r = dist(side, e.points[0]) > e.radius ? e.radius + d : e.radius - d;
        if (!(r > 1e-9))
            return std::nullopt;
        out.radius = r;
        break;
    }
    case Entity::Kind::Arc: {
        if (e.points.size() < 3)
            return std::nullopt;
        const Pt& c = e.points[0];
        const double r0 = dist(c, e.points[1]);
        const double r = dist(side, c) > r0 ? r0 + d : r0 - d;
        if (!(r > 1e-9) || r0 < kEps)
            return std::nullopt;
        for (std::size_t k = 1; k < 3; ++k)
            out.points[k] = lerp(c, e.points[k], r / r0);
        break;
    }
    case Entity::Kind::Rectangle: {
        if (e.points.size() < 2)
            return std::nullopt;
        const Pt lo = point_of(e, 0), hi = point_of(e, 2);
        const bool inside = side.x > lo.x && side.x < hi.x && side.y > lo.y && side.y < hi.y;
        const double g = inside ? -d : d;
        if (hi.x - lo.x + 2 * g <= 1e-9 || hi.y - lo.y + 2 * g <= 1e-9)
            return std::nullopt;
        out.points = {{lo.x - g, lo.y - g}, {hi.x + g, hi.y + g}};
        break;
    }
    case Entity::Kind::Chain: {
        const auto segs = segments_of(e);
        if (segs.empty())
            return std::nullopt;
        // The side: left or right of the segment nearest the pick.
        std::size_t nearest = 0;
        double best = std::numeric_limits<double>::infinity();
        for (const Curve& c : curves_of(s, i))
            if (const double dd = distance_to(c, side); dd < best)
                best = dd, nearest = c.segment;
        const double left = cross(sub(segs[nearest].second, segs[nearest].first), sub(side, segs[nearest].first)) > 0 ? 1.0 : -1.0;
        // Each segment moved along its left normal, the corners where the
        // moved lines meet.
        std::vector<std::pair<Pt, Pt>> moved_segs;
        for (const auto& [a, b] : segs) {
            const double l = dist(a, b);
            if (l < kEps)
                return std::nullopt;
            const Pt n{-(b.y - a.y) / l * d * left, (b.x - a.x) / l * d * left};
            moved_segs.push_back({{a.x + n.x, a.y + n.y}, {b.x + n.x, b.y + n.y}});
        }
        const auto corner = [&](std::size_t u, std::size_t v) {
            const Pt r = sub(moved_segs[u].second, moved_segs[u].first), q = sub(moved_segs[v].second, moved_segs[v].first);
            const double den = cross(r, q);
            if (std::abs(den) < kEps)
                return moved_segs[u].second;
            const double t = cross(sub(moved_segs[v].first, moved_segs[u].first), q) / den;
            return lerp(moved_segs[u].first, moved_segs[u].second, t);
        };
        const std::size_t n = moved_segs.size();
        out.points.clear();
        if (e.closed && e.points.size() > 2) {
            // Point k sits between segment k - 1 and segment k.
            for (std::size_t k = 0; k < n; ++k)
                out.points.push_back(corner((k + n - 1) % n, k));
        } else {
            out.points.push_back(moved_segs.front().first);
            for (std::size_t k = 1; k < n; ++k)
                out.points.push_back(corner(k - 1, k));
            out.points.push_back(moved_segs.back().second);
        }
        break;
    }
    }
    s.entities.push_back(std::move(out));
    return s.entities.size() - 1;
}

bool chamfer_corner(Sketch& s, std::size_t i, std::size_t corner, double d) {
    if (i >= s.entities.size() || !(d > 0.0))
        return false;
    Entity e = as_chain(s.entities[i]);
    if (e.kind != Entity::Kind::Chain)
        return false;
    const std::size_t n = e.points.size();
    const bool loop = e.closed && n > 2;
    if (corner >= n || (!loop && (corner == 0 || corner + 1 == n)))
        return false;
    const Pt p = e.points[corner];
    const Pt a = e.points[(corner + n - 1) % n], b = e.points[(corner + 1) % n];
    const double la = dist(p, a), lb = dist(p, b);
    if (d >= la - 1e-9 || d >= lb - 1e-9)
        return false;
    const Pt pa = lerp(p, a, d / la), pb = lerp(p, b, d / lb);
    e.points[corner] = pa;
    e.points.insert(e.points.begin() + std::ptrdiff_t(corner) + 1, pb);
    drop_rules_on(s, i);
    s.entities[i] = std::move(e);
    return true;
}

Map2 Map2::then(const Map2& n) const {
    Map2 m;
    m.a = n.a * a + n.b * c;
    m.b = n.a * b + n.b * d;
    m.c = n.c * a + n.d * c;
    m.d = n.c * b + n.d * d;
    m.tx = n.a * tx + n.b * ty + n.tx;
    m.ty = n.c * tx + n.d * ty + n.ty;
    return m;
}

Map2 moved(double dx, double dy) {
    Map2 m;
    m.tx = dx, m.ty = dy;
    return m;
}

Map2 rotated(const Pt& o, double degrees) {
    const double r = degrees * std::numbers::pi / 180.0;
    // Whole quarter turns exactly, so a rectangle turned by 90° stays one.
    double cs = std::cos(r), sn = std::sin(r);
    const double quarters = degrees / 90.0;
    if (std::abs(quarters - std::round(quarters)) < 1e-12) {
        const long q = ((long(std::llround(quarters)) % 4) + 4) % 4;
        cs = q == 0 ? 1 : q == 2 ? -1 : 0;
        sn = q == 1 ? 1 : q == 3 ? -1 : 0;
    }
    Map2 m;
    m.a = cs, m.b = -sn, m.c = sn, m.d = cs;
    m.tx = o.x - cs * o.x + sn * o.y;
    m.ty = o.y - sn * o.x - cs * o.y;
    return m;
}

Map2 scaled(const Pt& o, double f) {
    Map2 m;
    m.a = m.d = f;
    m.tx = o.x * (1 - f), m.ty = o.y * (1 - f);
    return m;
}

Map2 mirrored(const Pt& p1, const Pt& p2) {
    const double dx = p2.x - p1.x, dy = p2.y - p1.y, l2 = dx * dx + dy * dy;
    Map2 m;
    if (l2 < kEps)
        return m;
    // Reflection across the line: 2 u u^T - I, u its unit direction.
    m.a = (dx * dx - dy * dy) / l2;
    m.b = 2 * dx * dy / l2;
    m.c = m.b;
    m.d = -m.a;
    m.tx = p1.x - (m.a * p1.x + m.b * p1.y);
    m.ty = p1.y - (m.c * p1.x + m.d * p1.y);
    return m;
}

std::vector<std::size_t> map_entities(Sketch& s, const std::vector<std::size_t>& which, const Map2& m, bool copy) {
    std::vector<std::size_t> out;
    const double det = m.a * m.d - m.b * m.c;
    if (std::abs(det) < kEps)
        return out;
    // Similarity maps only (rotations, mirrors, uniform scales): a turn or a
    // mirror keeps a radius exactly.
    const double scale = std::abs(std::abs(det) - 1.0) < 1e-12 ? 1.0 : std::sqrt(std::abs(det));
    const bool square = (std::abs(m.b) < 1e-12 && std::abs(m.c) < 1e-12) || (std::abs(m.a) < 1e-12 && std::abs(m.d) < 1e-12);
    std::vector<std::size_t> sorted = which;
    std::sort(sorted.begin(), sorted.end());
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
    for (const std::size_t i : sorted) {
        if (i >= s.entities.size())
            continue;
        Entity e = s.entities[i];
        if (e.kind == Entity::Kind::Rectangle && !square)
            e = as_chain(e);
        for (Pt& p : e.points)
            p = m(p);
        if (e.kind == Entity::Kind::Circle)
            e.radius *= scale;
        if (e.kind == Entity::Kind::Arc && det < 0)
            e.clockwise = !e.clockwise;
        if (copy) {
            s.entities.push_back(std::move(e));
            out.push_back(s.entities.size() - 1);
        } else {
            // A move keeps what does not depend on place; anything else, and
            // a Fixed point, is dropped with the old shape.
            const bool translation = m.a == 1 && m.b == 0 && m.c == 0 && m.d == 1;
            if (translation) {
                std::erase_if(s.rules, [i](const GeoConstraint& r) {
                    return r.a.entity == i && r.kind == GeoConstraint::Kind::Fixed;
                });
                // Rules tying it to entities that stay are no longer met.
                std::erase_if(s.rules, [&](const GeoConstraint& r) {
                    return two_entity_rule(r.kind) && (r.a.entity == i) != (r.b.entity == i) &&
                           !(std::binary_search(sorted.begin(), sorted.end(), r.a.entity) &&
                             std::binary_search(sorted.begin(), sorted.end(), r.b.entity));
                });
                std::erase_if(s.constraints, [&](const Constraint& c) {
                    return (c.point.entity == i) != (c.target == i) &&
                           !(std::binary_search(sorted.begin(), sorted.end(), c.point.entity) &&
                             std::binary_search(sorted.begin(), sorted.end(), c.target));
                });
            } else {
                drop_rules_on(s, i);
            }
            s.entities[i] = std::move(e);
            out.push_back(i);
        }
    }
    return out;
}

std::vector<std::size_t> mirror_entities(Sketch& s, const std::vector<std::size_t>& which, const Pt& p1, const Pt& p2,
                                         bool erase_source) {
    if (dist(p1, p2) < kEps)
        return {};
    std::vector<std::size_t> made = map_entities(s, which, mirrored(p1, p2), true);
    if (erase_source) {
        std::vector<std::size_t> gone = which;
        std::sort(gone.begin(), gone.end());
        gone.erase(std::unique(gone.begin(), gone.end()), gone.end());
        for (auto it = gone.rbegin(); it != gone.rend(); ++it) {
            if (*it >= s.entities.size())
                continue;
            erase_entity(s, *it);
            for (std::size_t& m : made)
                if (m > *it)
                    --m;
        }
    }
    return made;
}

std::vector<std::size_t> pattern_rectangular(Sketch& s, const std::vector<std::size_t>& which, int columns, double dx, int rows,
                                             double dy) {
    std::vector<std::size_t> out;
    if (columns < 1 || rows < 1 || columns * rows < 2 || columns * rows > 10000)
        return out;
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < columns; ++c) {
            if (r == 0 && c == 0)
                continue;
            const auto made = map_entities(s, which, moved(c * dx, r * dy), true);
            out.insert(out.end(), made.begin(), made.end());
        }
    return out;
}

std::vector<std::size_t> pattern_polar(Sketch& s, const std::vector<std::size_t>& which, const Pt& centre, int count, double angle) {
    std::vector<std::size_t> out;
    if (count < 2 || count > 10000 || !(std::abs(angle) > 0.0))
        return out;
    const bool full = std::abs(std::abs(angle) - 360.0) < 1e-9 || std::abs(angle) > 360.0;
    const double step = full ? angle / count : angle / (count - 1);
    for (int k = 1; k < count; ++k) {
        const auto made = map_entities(s, which, rotated(centre, step * k), true);
        out.insert(out.end(), made.begin(), made.end());
    }
    return out;
}

std::vector<std::size_t> select_box(const Sketch& s, const Pt& from, const Pt& to) {
    const double x0 = std::min(from.x, to.x), x1 = std::max(from.x, to.x);
    const double y0 = std::min(from.y, to.y), y1 = std::max(from.y, to.y);
    const bool window = to.x >= from.x;
    const auto inside = [&](const Pt& p) { return p.x >= x0 && p.x <= x1 && p.y >= y0 && p.y <= y1; };
    std::vector<std::size_t> out;
    for (std::size_t i = 0; i < s.entities.size(); ++i) {
        // The entity as points along it: segments' ends and points between
        // (a segment crossing the box without an end in it is caught by
        // its crossing with the box's sides), circles and arcs sampled.
        std::vector<Pt> samples;
        bool crosses_side = false;
        const Curve sides[4] = {segment_curve({x0, y0}, {x1, y0}), segment_curve({x1, y0}, {x1, y1}),
                                segment_curve({x1, y1}, {x0, y1}), segment_curve({x0, y1}, {x0, y0})};
        for (const Curve& c : curves_of(s, i)) {
            if (!c.round) {
                samples.push_back(c.a);
                samples.push_back(c.b);
            } else {
                for (int k = 0; k <= 128; ++k) {
                    const double t = c.a0 + c.sweep * k / 128.0;
                    samples.push_back({c.c.x + c.r * std::cos(t), c.c.y + c.r * std::sin(t)});
                }
            }
            for (const Curve& side : sides)
                crosses_side = crosses_side || !meet(c, side).empty();
        }
        if (samples.empty())
            continue;
        const bool all = std::all_of(samples.begin(), samples.end(), inside);
        const bool any = std::any_of(samples.begin(), samples.end(), inside);
        if (window ? all : (any || crosses_side))
            out.push_back(i);
    }
    return out;
}

} // namespace tisma::cad
