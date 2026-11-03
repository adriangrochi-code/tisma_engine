#include "cad/constraints.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <utility>

namespace tisma::cad {

namespace {

using Vec = std::vector<double>;
using Eq = std::function<double(const Vec&)>;

struct PointVar {
    std::size_t x = 0, y = 0;
};

struct Invalid : std::runtime_error {
    Invalid() : std::runtime_error("a constraint names something the sketch does not have") {}
};

// The sketch's coordinates as one vector, and its equations over it.
struct System {
    std::vector<std::size_t> first; // per entity, its first coordinate
    std::vector<std::size_t> count; // per entity, how many
    Vec x;
    std::vector<Eq> eqs;
};

// The coordinates of each entity: a chain's points; a rectangle's two
// corners; a circle's centre and radius; an arc's centre, start and end.
System layout(const Sketch& s) {
    System sys;
    for (const Entity& e : s.entities) {
        sys.first.push_back(sys.x.size());
        switch (e.kind) {
        case Entity::Kind::Chain:
            for (const Pt& p : e.points)
                sys.x.insert(sys.x.end(), {p.x, p.y});
            break;
        case Entity::Kind::Rectangle:
            if (e.points.size() >= 2)
                sys.x.insert(sys.x.end(), {e.points[0].x, e.points[0].y, e.points[1].x, e.points[1].y});
            break;
        case Entity::Kind::Circle:
            if (!e.points.empty())
                sys.x.insert(sys.x.end(), {e.points[0].x, e.points[0].y, e.radius});
            break;
        case Entity::Kind::Arc:
            if (e.points.size() >= 3)
                for (std::size_t i = 0; i < 3; ++i)
                    sys.x.insert(sys.x.end(), {e.points[i].x, e.points[i].y});
            break;
        }
        sys.count.push_back(sys.x.size() - sys.first.back());
    }
    return sys;
}

void write_back(const System& sys, Sketch& s) {
    for (std::size_t i = 0; i < s.entities.size(); ++i) {
        Entity& e = s.entities[i];
        const double* v = sys.x.data() + sys.first[i];
        switch (e.kind) {
        case Entity::Kind::Chain:
            for (std::size_t k = 0; k < e.points.size(); ++k)
                e.points[k] = {v[2 * k], v[2 * k + 1]};
            break;
        case Entity::Kind::Rectangle:
            if (sys.count[i] == 4)
                e.points[0] = {v[0], v[1]}, e.points[1] = {v[2], v[3]};
            break;
        case Entity::Kind::Circle:
            if (sys.count[i] == 3)
                e.points[0] = {v[0], v[1]}, e.radius = v[2];
            break;
        case Entity::Kind::Arc:
            if (sys.count[i] == 6)
                for (std::size_t k = 0; k < 3; ++k)
                    e.points[k] = {v[2 * k], v[2 * k + 1]};
            break;
        }
    }
}

// The coordinates of an entity's point, as point_of counts them. A
// rectangle's corners take the smaller or larger of its two x and two y, as
// drawn.
std::optional<PointVar> point_var(const Sketch& s, const System& sys, const EntityPoint& p) {
    if (p.entity >= s.entities.size())
        return std::nullopt;
    const Entity& e = s.entities[p.entity];
    const std::size_t f = sys.first[p.entity], n = sys.count[p.entity];
    switch (e.kind) {
    case Entity::Kind::Chain:
        if (p.index < e.points.size())
            return PointVar{f + 2 * p.index, f + 2 * p.index + 1};
        break;
    case Entity::Kind::Rectangle: {
        if (n != 4 || p.index > 3)
            break;
        const bool x_first = e.points[0].x <= e.points[1].x, y_first = e.points[0].y <= e.points[1].y;
        const std::size_t x0 = x_first ? f : f + 2, x1 = x_first ? f + 2 : f;
        const std::size_t y0 = y_first ? f + 1 : f + 3, y1 = y_first ? f + 3 : f + 1;
        constexpr bool high_x[4] = {false, true, true, false}, high_y[4] = {false, false, true, true};
        return PointVar{high_x[p.index] ? x1 : x0, high_y[p.index] ? y1 : y0};
    }
    case Entity::Kind::Circle:
        if (n == 3 && p.index == 0)
            return PointVar{f, f + 1};
        break;
    case Entity::Kind::Arc:
        if (n == 6 && p.index <= 2)
            return PointVar{f + 2 * p.index, f + 2 * p.index + 1};
        break;
    }
    return std::nullopt;
}

// A chain's segment or a rectangle's side, as its two points.
std::optional<std::pair<PointVar, PointVar>> segment_var(const Sketch& s, const System& sys, std::size_t entity,
                                                         std::size_t segment) {
    if (entity >= s.entities.size())
        return std::nullopt;
    const Entity& e = s.entities[entity];
    if (e.kind == Entity::Kind::Chain) {
        const std::size_t n = e.points.size();
        const std::size_t segments = n < 2 ? 0 : e.closed && n > 2 ? n : n - 1;
        if (segment >= segments)
            return std::nullopt;
        return std::pair{*point_var(s, sys, {entity, segment}), *point_var(s, sys, {entity, (segment + 1) % n})};
    }
    if (e.kind == Entity::Kind::Rectangle && segment < 4 && sys.count[entity] == 4)
        return std::pair{*point_var(s, sys, {entity, segment}), *point_var(s, sys, {entity, (segment + 1) % 4})};
    return std::nullopt;
}

// A circle's or an arc's centre and radius.
struct Round {
    PointVar centre;
    std::function<double(const Vec&)> radius;
};
std::optional<Round> round_var(const Sketch& s, const System& sys, std::size_t entity) {
    if (entity >= s.entities.size())
        return std::nullopt;
    const Entity& e = s.entities[entity];
    const std::size_t f = sys.first[entity];
    if (e.kind == Entity::Kind::Circle && sys.count[entity] == 3)
        return Round{{f, f + 1}, [f](const Vec& x) { return x[f + 2]; }};
    if (e.kind == Entity::Kind::Arc && sys.count[entity] == 6)
        return Round{{f, f + 1}, [f](const Vec& x) { return std::hypot(x[f + 2] - x[f], x[f + 3] - x[f + 1]); }};
    return std::nullopt;
}

double length(const Vec& x, const std::pair<PointVar, PointVar>& s) {
    return std::hypot(x[s.second.x] - x[s.first.x], x[s.second.y] - x[s.first.y]);
}

// The equations of one rule; throws Invalid when it names what the sketch
// does not have.
void rule_equations(const Sketch& s, System& sys, const GeoConstraint& r) {
    using K = GeoConstraint::Kind;
    const auto seg = [&](const EntityPoint& p) {
        auto v = segment_var(s, sys, p.entity, p.index);
        if (!v)
            throw Invalid();
        return *v;
    };
    const auto point = [&](const EntityPoint& p) {
        auto v = point_var(s, sys, p);
        if (!v)
            throw Invalid();
        return *v;
    };
    const auto round = [&](const EntityPoint& p) {
        auto v = round_var(s, sys, p.entity);
        if (!v)
            throw Invalid();
        return *v;
    };
    const auto is_round = [&](const EntityPoint& p) { return round_var(s, sys, p.entity).has_value(); };
    auto& eqs = sys.eqs;
    const double value = r.value;
    switch (r.kind) {
    case K::Horizontal: {
        const auto a = seg(r.a);
        eqs.push_back([a](const Vec& x) { return x[a.second.y] - x[a.first.y]; });
        break;
    }
    case K::Vertical: {
        const auto a = seg(r.a);
        eqs.push_back([a](const Vec& x) { return x[a.second.x] - x[a.first.x]; });
        break;
    }
    case K::Parallel:
    case K::Perpendicular:
    case K::Angle: {
        const auto a = seg(r.a), b = seg(r.b);
        const auto parts = [a, b](const Vec& x) {
            const double ax = x[a.second.x] - x[a.first.x], ay = x[a.second.y] - x[a.first.y];
            const double bx = x[b.second.x] - x[b.first.x], by = x[b.second.y] - x[b.first.y];
            const double l = std::max(std::hypot(ax, ay) * std::hypot(bx, by), 1e-12);
            return std::pair{(ax * by - ay * bx) / l, (ax * bx + ay * by) / l};
        };
        if (r.kind == K::Parallel)
            eqs.push_back([parts](const Vec& x) { return parts(x).first; });
        else if (r.kind == K::Perpendicular)
            eqs.push_back([parts](const Vec& x) { return parts(x).second; });
        else
            eqs.push_back([parts, value](const Vec& x) {
                const auto [cross, dot] = parts(x);
                return std::remainder(std::atan2(cross, dot) - value * std::numbers::pi / 180.0, 2.0 * std::numbers::pi);
            });
        break;
    }
    case K::Equal:
        if (is_round(r.a) && is_round(r.b)) {
            const Round a = round(r.a), b = round(r.b);
            eqs.push_back([a, b](const Vec& x) { return a.radius(x) - b.radius(x); });
        } else {
            const auto a = seg(r.a), b = seg(r.b);
            eqs.push_back([a, b](const Vec& x) { return length(x, a) - length(x, b); });
        }
        break;
    case K::Concentric: {
        const Round a = round(r.a), b = round(r.b);
        eqs.push_back([a, b](const Vec& x) { return x[a.centre.x] - x[b.centre.x]; });
        eqs.push_back([a, b](const Vec& x) { return x[a.centre.y] - x[b.centre.y]; });
        break;
    }
    case K::Tangent: {
        const auto a = seg(r.a);
        const Round b = round(r.b);
        eqs.push_back([a, b](const Vec& x) {
            const double dx = x[a.second.x] - x[a.first.x], dy = x[a.second.y] - x[a.first.y];
            const double cross = dx * (x[b.centre.y] - x[a.first.y]) - dy * (x[b.centre.x] - x[a.first.x]);
            return std::abs(cross) / std::max(std::hypot(dx, dy), 1e-12) - b.radius(x);
        });
        break;
    }
    case K::Coincident: {
        const PointVar a = point(r.a), b = point(r.b);
        eqs.push_back([a, b](const Vec& x) { return x[b.x] - x[a.x]; });
        eqs.push_back([a, b](const Vec& x) { return x[b.y] - x[a.y]; });
        break;
    }
    case K::Fixed: {
        const PointVar a = point(r.a);
        const Pt at = r.at;
        eqs.push_back([a, at](const Vec& x) { return x[a.x] - at.x; });
        eqs.push_back([a, at](const Vec& x) { return x[a.y] - at.y; });
        break;
    }
    case K::Length: {
        if (value <= 0.0)
            throw Invalid();
        const auto a = seg(r.a);
        eqs.push_back([a, value](const Vec& x) { return length(x, a) - value; });
        break;
    }
    case K::Distance: {
        if (value <= 0.0)
            throw Invalid();
        const PointVar a = point(r.a), b = point(r.b);
        eqs.push_back([a, b, value](const Vec& x) { return std::hypot(x[b.x] - x[a.x], x[b.y] - x[a.y]) - value; });
        break;
    }
    case K::HorizontalDistance: {
        const PointVar a = point(r.a), b = point(r.b);
        eqs.push_back([a, b, value](const Vec& x) { return x[b.x] - x[a.x] - value; });
        break;
    }
    case K::VerticalDistance: {
        const PointVar a = point(r.a), b = point(r.b);
        eqs.push_back([a, b, value](const Vec& x) { return x[b.y] - x[a.y] - value; });
        break;
    }
    case K::Radius:
    case K::Diameter: {
        if (value <= 0.0)
            throw Invalid();
        const Round a = round(r.a);
        const double want = r.kind == K::Radius ? value : value / 2.0;
        eqs.push_back([a, want](const Vec& x) { return a.radius(x) - want; });
        break;
    }
    }
}

// Every equation of the sketch: an arc's equal radii, the snaps of D2, the
// typed lengths and the rules added by hand.
System build(const Sketch& s, std::size_t rules = std::size_t(-1)) {
    System sys = layout(s);
    for (std::size_t i = 0; i < s.entities.size(); ++i)
        if (s.entities[i].kind == Entity::Kind::Arc && sys.count[i] == 6) {
            const std::size_t f = sys.first[i];
            sys.eqs.push_back([f](const Vec& x) {
                return std::hypot(x[f + 2] - x[f], x[f + 3] - x[f + 1]) - std::hypot(x[f + 4] - x[f], x[f + 5] - x[f + 1]);
            });
        }
    for (const Constraint& c : s.constraints) {
        const auto p = point_var(s, sys, c.point);
        if (!p)
            continue;
        if (c.kind == Constraint::Kind::Coincident) {
            const auto q = point_var(s, sys, {c.target, c.target_index});
            if (!q)
                continue;
            sys.eqs.push_back([p = *p, q = *q](const Vec& x) { return x[q.x] - x[p.x]; });
            sys.eqs.push_back([p = *p, q = *q](const Vec& x) { return x[q.y] - x[p.y]; });
        } else if (c.kind == Constraint::Kind::Midpoint) {
            const auto g = segment_var(s, sys, c.target, c.target_index);
            if (!g)
                continue;
            sys.eqs.push_back([p = *p, g = *g](const Vec& x) { return x[p.x] - 0.5 * (x[g.first.x] + x[g.second.x]); });
            sys.eqs.push_back([p = *p, g = *g](const Vec& x) { return x[p.y] - 0.5 * (x[g.first.y] + x[g.second.y]); });
        } else if (const auto g = segment_var(s, sys, c.target, c.target_index)) {
            sys.eqs.push_back([p = *p, g = *g](const Vec& x) {
                const double dx = x[g.second.x] - x[g.first.x], dy = x[g.second.y] - x[g.first.y];
                return (dx * (x[p.y] - x[g.first.y]) - dy * (x[p.x] - x[g.first.x])) / std::max(std::hypot(dx, dy), 1e-12);
            });
        } else if (const auto r = round_var(s, sys, c.target)) {
            sys.eqs.push_back([p = *p, r = *r](const Vec& x) {
                return std::hypot(x[p.x] - x[r.centre.x], x[p.y] - x[r.centre.y]) - r.radius(x);
            });
        }
    }
    for (const Dimension& d : s.dimensions)
        if (const auto g = segment_var(s, sys, d.entity, d.segment); g && d.length > 0.0)
            sys.eqs.push_back([g = *g, l = d.length](const Vec& x) { return length(x, g) - l; });
    for (std::size_t i = 0; i < s.rules.size() && i < rules; ++i)
        rule_equations(s, sys, s.rules[i]);
    return sys;
}

Vec residuals(const System& sys, const Vec& x) {
    Vec r(sys.eqs.size());
    for (std::size_t i = 0; i < r.size(); ++i)
        r[i] = sys.eqs[i](x);
    return r;
}

double largest(const Vec& v) {
    double m = 0.0;
    for (double e : v)
        m = std::max(m, std::abs(e));
    return m;
}

double squared(const Vec& v) {
    double m = 0.0;
    for (double e : v)
        m += e * e;
    return m;
}

// The equations' derivatives by central differences, row-major m x n.
Vec jacobian(const System& sys, const Vec& x0) {
    const std::size_t m = sys.eqs.size(), n = x0.size();
    Vec j(m * n);
    Vec x = x0;
    for (std::size_t c = 0; c < n; ++c) {
        const double h = 1e-6 * std::max(1.0, std::abs(x0[c]));
        x[c] = x0[c] + h;
        const Vec up = residuals(sys, x);
        x[c] = x0[c] - h;
        const Vec down = residuals(sys, x);
        x[c] = x0[c];
        for (std::size_t r = 0; r < m; ++r)
            j[r * n + c] = (up[r] - down[r]) / (2.0 * h);
    }
    return j;
}

// Solves a x = b (n x n, row-major) by elimination with partial pivoting.
std::optional<Vec> linear_solve(Vec a, Vec b) {
    const std::size_t n = b.size();
    for (std::size_t c = 0; c < n; ++c) {
        std::size_t p = c;
        for (std::size_t r = c + 1; r < n; ++r)
            if (std::abs(a[r * n + c]) > std::abs(a[p * n + c]))
                p = r;
        if (std::abs(a[p * n + c]) < 1e-300)
            return std::nullopt;
        if (p != c) {
            for (std::size_t k = 0; k < n; ++k)
                std::swap(a[p * n + k], a[c * n + k]);
            std::swap(b[p], b[c]);
        }
        for (std::size_t r = c + 1; r < n; ++r) {
            const double f = a[r * n + c] / a[c * n + c];
            if (f == 0.0)
                continue;
            for (std::size_t k = c; k < n; ++k)
                a[r * n + k] -= f * a[c * n + k];
            b[r] -= f * b[c];
        }
    }
    Vec x(n);
    for (std::size_t r = n; r-- > 0;) {
        double v = b[r];
        for (std::size_t k = r + 1; k < n; ++k)
            v -= a[r * n + k] * x[k];
        x[r] = v / a[r * n + r];
    }
    return x;
}

constexpr double solved_within = 1e-6;

// Minimum-norm Gauss-Newton steps, damped as Levenberg-Marquardt: each step
// dx = -J^T (J J^T + mu I)^-1 r moves the coordinates as little as it can.
bool newton(System& sys) {
    const std::size_t m = sys.eqs.size(), n = sys.x.size();
    if (m == 0)
        return true;
    Vec r = residuals(sys, sys.x);
    double cost = squared(r);
    double mu = 1e-9;
    for (int it = 0; it < 200 && largest(r) > 1e-10; ++it) {
        const Vec j = jacobian(sys, sys.x);
        Vec a(m * m, 0.0);
        double diagonal = 0.0;
        for (std::size_t p = 0; p < m; ++p)
            for (std::size_t q = p; q < m; ++q) {
                double v = 0.0;
                for (std::size_t c = 0; c < n; ++c)
                    v += j[p * n + c] * j[q * n + c];
                a[p * m + q] = a[q * m + p] = v;
                if (p == q)
                    diagonal = std::max(diagonal, v);
            }
        bool stepped = false;
        for (int tries = 0; tries < 30 && !stepped; ++tries) {
            Vec damped = a;
            for (std::size_t p = 0; p < m; ++p)
                damped[p * m + p] += mu * std::max(diagonal, 1e-12);
            const auto y = linear_solve(std::move(damped), r);
            if (y) {
                Vec x = sys.x;
                for (std::size_t c = 0; c < n; ++c) {
                    double v = 0.0;
                    for (std::size_t p = 0; p < m; ++p)
                        v += j[p * n + c] * (*y)[p];
                    x[c] -= v;
                }
                const Vec rn = residuals(sys, x);
                const double cn = squared(rn);
                if (std::isfinite(cn) && cn < cost) {
                    sys.x = std::move(x);
                    r = rn;
                    cost = cn;
                    mu = std::max(mu / 10.0, 1e-12);
                    stepped = true;
                    break;
                }
            }
            mu *= 10.0;
        }
        if (!stepped)
            break;
    }
    return largest(r) <= solved_within;
}

// The rank of the Jacobian and which coordinates it fixes: reduced row
// echelon form; a coordinate is fixed when it is a pivot whose row has
// nothing in the free columns (no motion left moves it).
struct Rank {
    int rank = 0;
    std::vector<bool> fixed;
};
Rank rank_of(const System& sys) {
    const std::size_t m = sys.eqs.size(), n = sys.x.size();
    Rank out;
    out.fixed.assign(n, false);
    if (m == 0 || n == 0)
        return out;
    Vec j = jacobian(sys, sys.x);
    const double tol = 1e-7 * std::max(1.0, largest(j));
    std::vector<std::size_t> pivot_col;
    std::vector<bool> is_pivot(n, false);
    std::size_t row = 0;
    for (std::size_t c = 0; c < n && row < m; ++c) {
        std::size_t p = row;
        for (std::size_t r = row + 1; r < m; ++r)
            if (std::abs(j[r * n + c]) > std::abs(j[p * n + c]))
                p = r;
        if (std::abs(j[p * n + c]) < tol)
            continue;
        for (std::size_t k = 0; k < n; ++k)
            std::swap(j[p * n + k], j[row * n + k]);
        const double d = j[row * n + c];
        for (std::size_t k = 0; k < n; ++k)
            j[row * n + k] /= d;
        for (std::size_t r = 0; r < m; ++r) {
            if (r == row)
                continue;
            const double f = j[r * n + c];
            if (f == 0.0)
                continue;
            for (std::size_t k = 0; k < n; ++k)
                j[r * n + k] -= f * j[row * n + k];
        }
        pivot_col.push_back(c);
        is_pivot[c] = true;
        ++row;
    }
    out.rank = int(row);
    for (std::size_t k = 0; k < row; ++k) {
        bool alone = true;
        for (std::size_t c = 0; c < n && alone; ++c)
            if (!is_pivot[c] && std::abs(j[k * n + c]) > 1e-6)
                alone = false;
        out.fixed[pivot_col[k]] = alone;
    }
    return out;
}

SketchSolve report(const Sketch& s, const System& sys, bool solved) {
    SketchSolve out;
    out.solved = solved;
    out.residual = largest(residuals(sys, sys.x));
    out.unknowns = int(sys.x.size());
    const Rank rank = rank_of(sys);
    out.dof = out.unknowns - rank.rank;
    out.entity_fixed.assign(s.entities.size(), false);
    for (std::size_t i = 0; i < s.entities.size(); ++i) {
        bool all = sys.count[i] > 0;
        for (std::size_t k = 0; k < sys.count[i] && all; ++k)
            all = rank.fixed[sys.first[i] + k];
        out.entity_fixed[i] = all;
    }
    return out;
}

} // namespace

bool set_entity_size(Sketch& sketch, std::size_t entity, const std::string& what, double value) {
    using K = GeoConstraint::Kind;
    if (entity >= sketch.entities.size() || !(value > 0.0))
        return false;
    Entity& en = sketch.entities[entity];
    if (en.kind == Entity::Kind::Rectangle && en.points.size() >= 2 && (what == "width" || what == "height")) {
        const bool width = what == "width";
        Pt& a = en.points[0];
        Pt& b = en.points[1];
        if (width)
            b.x = a.x + (b.x < a.x ? -value : value);
        else
            b.y = a.y + (b.y < a.y ? -value : value);
        // Sides 0 and 2 run along x, 1 and 3 along y.
        for (GeoConstraint& r : sketch.rules)
            if (r.kind == K::Length && r.a.entity == entity && (r.a.index % 2 == 0) == width)
                r.value = value;
    } else if (en.kind == Entity::Kind::Circle && what == "diameter") {
        en.radius = 0.5 * value;
        for (GeoConstraint& r : sketch.rules)
            if (r.a.entity == entity && (r.kind == K::Diameter || r.kind == K::Radius))
                r.value = r.kind == K::Diameter ? value : 0.5 * value;
    } else {
        return false;
    }
    if (!sketch.rules.empty())
        solve_sketch(sketch);
    return true;
}

int equations_of(GeoConstraint::Kind kind) {
    using K = GeoConstraint::Kind;
    return kind == K::Fixed || kind == K::Coincident || kind == K::Concentric ? 2 : 1;
}

namespace {
System build_safe(const Sketch& sketch) {
    try {
        return build(sketch);
    } catch (const Invalid&) {
        return build(sketch, 0);
    }
}
} // namespace

SketchSolve analyze_sketch(const Sketch& sketch) {
    try {
        const System sys = build(sketch);
        return report(sketch, sys, largest(residuals(sys, sys.x)) <= solved_within);
    } catch (const Invalid&) {
        // A rule naming a missing entity (an entity deleted after): count
        // without the rules.
        const System sys = build(sketch, 0);
        return report(sketch, sys, false);
    }
}

SketchSolve solve_sketch(Sketch& sketch) {
    System sys;
    try {
        sys = build(sketch);
    } catch (const Invalid&) {
        sys = build(sketch, 0);
    }
    bool solved = newton(sys);
    // A rectangle keeps its corners where they are (its width and height
    // never cross zero: the corners' names would change), and a circle or
    // an arc a radius above zero; a solve that would break that is not one.
    Sketch after = sketch;
    write_back(sys, after);
    for (std::size_t i = 0; i < sketch.entities.size() && solved; ++i) {
        const Entity& a = sketch.entities[i];
        const Entity& b = after.entities[i];
        if (a.kind == Entity::Kind::Rectangle && a.points.size() >= 2) {
            const double w0 = a.points[1].x - a.points[0].x, h0 = a.points[1].y - a.points[0].y;
            const double w1 = b.points[1].x - b.points[0].x, h1 = b.points[1].y - b.points[0].y;
            solved = w0 * w1 > 0.0 && h0 * h1 > 0.0 && std::abs(w1) > 1e-6 && std::abs(h1) > 1e-6;
        } else if (b.kind == Entity::Kind::Circle) {
            solved = b.radius > 1e-6;
        } else if (b.kind == Entity::Kind::Arc && b.points.size() >= 3) {
            solved = std::hypot(b.points[1].x - b.points[0].x, b.points[1].y - b.points[0].y) > 1e-6;
        }
    }
    if (!solved) // the sketch stays as it was
        return report(sketch, build_safe(sketch), false);
    sketch = std::move(after);
    return report(sketch, sys, true);
}

AddRule add_rule(Sketch& sketch, const GeoConstraint& rule) {
    Sketch with = sketch;
    with.rules.push_back(rule);
    try {
        build(with);
    } catch (const Invalid&) {
        return AddRule::Invalid;
    }
    const SketchSolve solved = solve_sketch(with);
    if (!solved.solved)
        return AddRule::Conflicting;
    // At the solved geometry: what the rule adds to the rank.
    Sketch without = with;
    without.rules.pop_back();
    const SketchSolve before = analyze_sketch(without);
    if ((solved.unknowns - solved.dof) - (before.unknowns - before.dof) < equations_of(rule.kind))
        return AddRule::Redundant;
    sketch = std::move(with);
    return AddRule::Added;
}

} // namespace tisma::cad
