// Metric fasteners for printing. Own design; see PROVENANCE.md.
//
// Every part is a solid of rings: a ring is a loop of points at one height,
// one per angle of a fixed set, so neighbouring rings join in quads and the
// mesh closes by construction. A ring's radius may change with the angle (a
// hexagon, a thread); the rings follow the part's outline in the (radius,
// height) half plane with the material on their left, so every face looks out.

#include "cad/fasteners.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <vector>

namespace tisma::cad {
namespace {

// ISO 261 coarse pitches; ISO 4032 nuts; ISO 4017 hex heads; ISO 4762 socket
// heads and keys; ISO 7089 washers; ISO 273 medium clearance holes.
constexpr MetricSize kSizes[] = {
    {"M3", 3, 0.5, 5.5, 2.4, 2.0, 5.5, 2.5, 3.2, 7, 0.5, 3.4},
    {"M4", 4, 0.7, 7, 3.2, 2.8, 7, 3, 4.3, 9, 0.8, 4.5},
    {"M5", 5, 0.8, 8, 4.7, 3.5, 8.5, 4, 5.3, 10, 1.0, 5.5},
    {"M6", 6, 1.0, 10, 5.2, 4.0, 10, 5, 6.4, 12, 1.6, 6.6},
    {"M8", 8, 1.25, 13, 6.8, 5.3, 13, 6, 8.4, 16, 1.6, 9},
    {"M10", 10, 1.5, 16, 8.4, 6.4, 16, 8, 10.5, 20, 2.0, 11},
    {"M12", 12, 1.75, 18, 10.8, 7.5, 18, 10, 13, 24, 2.5, 13.5},
    {"M16", 16, 2.0, 24, 14.8, 10, 24, 14, 17, 30, 3.0, 17.5},
    {"M20", 20, 2.5, 30, 18, 12.5, 30, 17, 21, 37, 3.0, 22},
    {"M24", 24, 3.0, 36, 21.5, 15, 36, 19, 25, 44, 4.0, 26},
};

constexpr double kPi = std::numbers::pi;
const double kSqrt3 = std::sqrt(3.0);

struct Ring {
    double z = 0;
    std::vector<double> r; // one radius per angle
};

class Lathe {
public:
    explicit Lathe(int segments) : n_(segments) {
        for (int i = 0; i < n_; ++i) {
            const double a = 2.0 * kPi * i / n_;
            cos_.push_back(std::cos(a));
            sin_.push_back(std::sin(a));
        }
    }
    int size() const { return n_; }
    double angle(int i) const { return 2.0 * kPi * i / n_; }

    // A ring from a radius function of the angle.
    template <class F> Ring ring(double z, F&& radius) const {
        Ring out{z, std::vector<double>(std::size_t(n_))};
        for (int i = 0; i < n_; ++i)
            out.r[std::size_t(i)] = radius(angle(i));
        return out;
    }

    // The rings joined in order; `loop` joins the last to the first, else
    // the first and the last are closed by a fan to the axis.
    TriangleMesh mesh(const std::vector<Ring>& rings, bool loop) const {
        TriangleMesh m;
        const std::size_t count = rings.size();
        m.reserve(count * std::size_t(n_) * 2 + std::size_t(2 * n_));
        const auto point = [&](const Ring& g, int i) {
            const std::size_t k = std::size_t(i % n_);
            return Vec3f{float(g.r[k] * cos_[k]), float(g.r[k] * sin_[k]), float(g.z)};
        };
        const auto add = [&](const Vec3f& a, const Vec3f& b, const Vec3f& c) {
            if (a == b || b == c || a == c)
                return; // a ring that touches its neighbour there
            m.add_triangle(a, b, c);
        };
        const std::size_t joins = loop ? count : count - 1;
        for (std::size_t g = 0; g < joins; ++g) {
            const Ring& a = rings[g];
            const Ring& b = rings[(g + 1) % count];
            for (int i = 0; i < n_; ++i) {
                const Vec3f a0 = point(a, i), a1 = point(a, i + 1), b0 = point(b, i), b1 = point(b, i + 1);
                add(a0, a1, b1);
                add(a0, b1, b0);
            }
        }
        if (!loop) {
            const Ring& first = rings.front();
            const Ring& last = rings.back();
            const Vec3f c0{0, 0, float(first.z)}, c1{0, 0, float(last.z)};
            for (int i = 0; i < n_; ++i) {
                add(c0, point(first, i + 1), point(first, i));
                add(c1, point(last, i), point(last, i + 1));
            }
        }
        return m;
    }

private:
    int n_;
    std::vector<double> cos_, sin_;
};

// A hexagon of `width` across flats, its corners at multiples of 60°.
double hexagon(double width, double angle) {
    const double sixty = kPi / 3.0;
    const double phi = std::fmod(angle, sixty) - sixty / 2.0;
    return width / 2.0 / std::cos(phi);
}

// The thread's surface at height z and angle a (right-hand: it climbs
// turning counter-clockwise seen from above), moved out by `offset`.
double thread_at(double d, double p, double z, double a, double offset) {
    double u = z / p - a / (2.0 * kPi);
    u -= std::floor(u);
    return thread_profile_radius(d, p, u) + offset;
}

double root_radius(double d, double p) {
    return d / 2.0 - 5.0 / 8.0 * (kSqrt3 / 2.0) * p;
}

// The heights of a threaded stretch from z0 to z1: steps short enough for the
// profile's corners (a sixteenth of the pitch, at most 0.2 mm).
std::vector<double> steps(double z0, double z1, double p) {
    const double step = std::min(p / 16.0, 0.2);
    const int count = std::max(1, int(std::ceil((z1 - z0) / step - 1e-9)));
    std::vector<double> out;
    for (int k = 0; k <= count; ++k)
        out.push_back(z0 + (z1 - z0) * k / count);
    return out;
}

// A hexagonal prism's side from z0 to z1 with the 30° chamfer at the ends that
// ask for one (the face's circle at 0.95 of the flats).
void hex_side(const Lathe& lathe, std::vector<Ring>& rings, double width, double z0, double z1, bool chamfer0,
              bool chamfer1, bool upward) {
    const double flat = width / 2.0 * 0.95;
    const double corner = width / kSqrt3;
    const double height = std::min((corner - flat) / kSqrt3, (z1 - z0) / 2.0);
    std::vector<double> zs;
    constexpr int kChamferRings = 4;
    if (chamfer0)
        for (int k = 0; k < kChamferRings; ++k)
            zs.push_back(z0 + height * k / kChamferRings);
    else
        zs.push_back(z0);
    if (chamfer1) {
        for (int k = kChamferRings; k >= 0; --k)
            zs.push_back(z1 - height * k / kChamferRings);
    } else {
        if (chamfer0)
            zs.push_back(z0 + height);
        zs.push_back(z1);
    }
    if (!upward)
        std::reverse(zs.begin(), zs.end());
    for (const double z : zs)
        rings.push_back(lathe.ring(z, [&](double a) {
            double r = hexagon(width, a);
            if (chamfer0)
                r = std::min(r, flat + (z - z0) * kSqrt3);
            if (chamfer1)
                r = std::min(r, flat + (z1 - z) * kSqrt3);
            return r;
        }));
}

// The outside thread from z0 up to z1, thinner by the play, with a 45°
// chamfer down to the root at the ends that ask for one.
void outer_thread(const Lathe& lathe, std::vector<Ring>& rings, double d, double p, double play, double z0, double z1,
                  bool chamfer0, bool chamfer1) {
    const double root = root_radius(d, p) - play;
    for (const double z : steps(z0, z1, p))
        rings.push_back(lathe.ring(z, [&](double a) {
            double r = thread_at(d, p, z, a, -play);
            if (chamfer0)
                r = std::min(r, root + (z - z0));
            if (chamfer1)
                r = std::min(r, root + (z1 - z));
            return r;
        }));
}

TriangleMesh outward(TriangleMesh m) {
    if (m.signed_volume() >= 0.0)
        return m;
    std::vector<Vec3f> corners = m.corners();
    for (std::size_t i = 0; i + 2 < corners.size(); i += 3)
        std::swap(corners[i + 1], corners[i + 2]);
    return TriangleMesh(std::move(corners));
}

std::string lower(std::string_view s) {
    std::string out(s);
    for (char& c : out)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

} // namespace

std::span<const MetricSize> metric_sizes() {
    return kSizes;
}

const MetricSize* metric_size(std::string_view name) {
    const std::string wanted = lower(name);
    for (const MetricSize& s : kSizes)
        if (lower(s.name) == wanted)
            return &s;
    return nullptr;
}

const char* fastener_id(FastenerKind kind) {
    switch (kind) {
    case FastenerKind::HexBolt: return "hex_bolt";
    case FastenerKind::SocketBolt: return "socket_bolt";
    case FastenerKind::HexNut: return "hex_nut";
    case FastenerKind::Washer: return "washer";
    case FastenerKind::ThreadedRod: return "threaded_rod";
    }
    return "hex_bolt";
}

bool has_length(FastenerKind kind) {
    return kind == FastenerKind::HexBolt || kind == FastenerKind::SocketBolt || kind == FastenerKind::ThreadedRod;
}

bool has_thread(FastenerKind kind) {
    return kind != FastenerKind::Washer;
}

FastenerDims fastener_dims(const FastenerSpec& spec) {
    FastenerDims out;
    if (const MetricSize* s = metric_size(spec.size)) {
        out = {s->diameter,       s->pitch,      s->nut_width,     s->nut_height,
               s->head_height,    s->socket_head, s->socket_key,   s->washer_inner,
               s->washer_outer,   s->washer_height, s->clearance_hole};
    } else {
        // A size of its own: the M8 row's proportions, the pitch about 0.15 d.
        const MetricSize& m8 = kSizes[4];
        const double d = spec.diameter, k = d / m8.diameter;
        out = {d,
               std::max(0.25, std::round(d * 0.15 / 0.05) * 0.05),
               m8.nut_width * k,
               m8.nut_height * k,
               m8.head_height * k,
               m8.socket_head * k,
               m8.socket_key * k,
               m8.washer_inner * k,
               m8.washer_outer * k,
               m8.washer_height * k,
               m8.clearance_hole * k};
    }
    if (spec.pitch > 0.0)
        out.pitch = spec.pitch;
    return out;
}

std::string check_fastener(const FastenerSpec& spec) {
    const bool custom = lower(spec.size) == "custom";
    if (!custom && !metric_size(spec.size))
        return "unknown size " + spec.size;
    if (custom && !(spec.diameter >= 2.0 && spec.diameter <= 300.0))
        return "the diameter must be between 2 and 300 mm";
    const FastenerDims dims = fastener_dims(spec);
    if (spec.pitch != 0.0 && !(spec.pitch >= 0.2 && spec.pitch <= dims.d / 2.0))
        return "the pitch must be between 0.2 mm and half the diameter";
    if (!(spec.clearance >= 0.0 && spec.clearance <= 1.0 && spec.clearance < dims.d / 4.0))
        return "the play must be between 0 and 1 mm, and under a quarter of the diameter";
    if (has_thread(spec.kind) && root_radius(dims.d, dims.pitch) - spec.clearance < 0.1 * dims.d)
        return "the thread is too deep for the diameter";
    if (has_length(spec.kind) && !(spec.length >= 2.0 * dims.pitch && spec.length <= 1000.0))
        return "the length must be between two pitches and 1000 mm";
    if (spec.segments < 12 || spec.segments > 360)
        return "the segments must be between 12 and 360";
    return {};
}

double thread_profile_radius(double d, double p, double u) {
    const double depth = 5.0 / 8.0 * (kSqrt3 / 2.0) * p; // 5H/8
    const double major = d / 2.0, minor = major - depth;
    u -= std::floor(u);
    if (u < 0.125) // the crest, p/8
        return major;
    if (u < 0.4375) // down the flank, 5H/8 * tan 30° = 5p/16 along the axis
        return major - depth * (u - 0.125) / 0.3125;
    if (u < 0.6875) // the root, p/4
        return minor;
    return minor + depth * (u - 0.6875) / 0.3125; // up the other flank
}

TriangleMesh make_fastener(const FastenerSpec& spec) {
    if (const std::string why = check_fastener(spec); !why.empty())
        throw std::invalid_argument(why);
    const FastenerDims dims = fastener_dims(spec);
    const Lathe lathe(std::max(12, spec.segments / 6 * 6));
    const double d = dims.d, p = dims.pitch, play = spec.clearance;
    std::vector<Ring> rings;
    switch (spec.kind) {
    case FastenerKind::HexBolt: {
        // On its head: the head from 0 to k (chamfered on the bed side), the
        // thread above it.
        const double k = dims.head_height;
        hex_side(lathe, rings, dims.nut_width, 0.0, k, true, false, true);
        outer_thread(lathe, rings, d, p, play, k, k + spec.length, false, true);
        return outward(lathe.mesh(rings, false));
    }
    case FastenerKind::SocketBolt: {
        // On its head: the hex socket opens on the bed, then the round head
        // (as tall as the diameter) and the thread.
        const double k = d, depth = std::min(0.5 * d, 0.8 * k);
        const double key = dims.socket_key + 2.0 * play;
        rings.push_back(lathe.ring(depth, [&](double a) { return hexagon(key, a); }));
        rings.push_back(lathe.ring(0.0, [&](double a) { return hexagon(key, a); }));
        const double head = dims.socket_head / 2.0, edge = std::min(0.5, 0.1 * k);
        rings.push_back(lathe.ring(0.0, [&](double) { return head - edge; }));
        rings.push_back(lathe.ring(edge, [&](double) { return head; }));
        rings.push_back(lathe.ring(k, [&](double) { return head; }));
        outer_thread(lathe, rings, d, p, play, k, k + spec.length, false, true);
        return outward(lathe.mesh(rings, false));
    }
    case FastenerKind::HexNut: {
        // Up the outside, in over the top, down the thread, out under the
        // bottom; the thread's ends widened to a 45° countersink.
        const double m = dims.nut_height;
        hex_side(lathe, rings, dims.nut_width, 0.0, m, true, true, true);
        const double entry = d / 2.0 + play;
        for (const double z : [&] {
                 std::vector<double> zs = steps(0.0, m, p);
                 std::reverse(zs.begin(), zs.end());
                 return zs;
             }())
            rings.push_back(lathe.ring(z, [&](double a) {
                const double r = thread_at(d, p, z, a, play);
                return std::max(r, entry - std::min(z, m - z));
            }));
        return outward(lathe.mesh(rings, true));
    }
    case FastenerKind::Washer: {
        const double h = dims.washer_height, in = dims.washer_inner / 2.0, out = dims.washer_outer / 2.0;
        rings.push_back(lathe.ring(0.0, [&](double) { return out; }));
        rings.push_back(lathe.ring(h, [&](double) { return out; }));
        rings.push_back(lathe.ring(h, [&](double) { return in; }));
        rings.push_back(lathe.ring(0.0, [&](double) { return in; }));
        return outward(lathe.mesh(rings, true));
    }
    case FastenerKind::ThreadedRod:
        outer_thread(lathe, rings, d, p, play, 0.0, spec.length, true, true);
        return outward(lathe.mesh(rings, false));
    }
    return {};
}

std::string fastener_json(const FastenerSpec& spec) {
    nlohmann::json j;
    j["format"] = "tisma-fastener-1";
    j["kind"] = fastener_id(spec.kind);
    j["size"] = spec.size;
    j["diameter"] = spec.diameter;
    j["pitch"] = spec.pitch;
    j["length"] = spec.length;
    j["clearance"] = spec.clearance;
    j["segments"] = spec.segments;
    return j.dump();
}

FastenerSpec parse_fastener(std::string_view text) {
    try {
        const nlohmann::json j = nlohmann::json::parse(text);
        if (j.value("format", "") != "tisma-fastener-1")
            throw std::runtime_error("not a fastener");
        FastenerSpec spec;
        const std::string kind = j.value("kind", "hex_bolt");
        bool found = false;
        for (const FastenerKind k : kFastenerKinds)
            if (kind == fastener_id(k))
                spec.kind = k, found = true;
        if (!found)
            throw std::runtime_error("unknown fastener kind " + kind);
        spec.size = j.value("size", spec.size);
        spec.diameter = j.value("diameter", spec.diameter);
        spec.pitch = j.value("pitch", spec.pitch);
        spec.length = j.value("length", spec.length);
        spec.clearance = j.value("clearance", spec.clearance);
        spec.segments = j.value("segments", spec.segments);
        if (const std::string why = check_fastener(spec); !why.empty())
            throw std::runtime_error(why);
        return spec;
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error(std::string("fastener: ") + e.what());
    }
}

} // namespace tisma::cad
