// Painted faces: the trees of marks and the round brush. Own design; see
// PROVENANCE.md.

#include "mesh/paint.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace tisma {

namespace {

const Painting::Tree kUnpainted;

// Deeper than any brush needs (a 200 mm face cut down to 0.05 mm is 12 deep).
constexpr int kMaxDepth = 16;

struct P {
    double x, y, z;
};

P to_p(const Vec3f& v) { return {v.x, v.y, v.z}; }
Vec3f to_v(const P& p) { return {float(p.x), float(p.y), float(p.z)}; }
P sub(const P& a, const P& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
P mid(const P& a, const P& b) { return {0.5 * (a.x + b.x), 0.5 * (a.y + b.y), 0.5 * (a.z + b.z)}; }
double dot(const P& a, const P& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
P cross(const P& a, const P& b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }

double segment_distance2(const P& p, const P& a, const P& b) {
    const P ab = sub(b, a), ap = sub(p, a);
    const double length2 = dot(ab, ab);
    const double t = length2 > 0.0 ? std::clamp(dot(ap, ab) / length2, 0.0, 1.0) : 0.0;
    const P d{ap.x - t * ab.x, ap.y - t * ab.y, ap.z - t * ab.z};
    return dot(d, d);
}

// The squared distance from p to the nearest point of triangle abc: to its
// plane when p's foot on the plane is inside the triangle, else to its nearest
// edge.
double triangle_distance2(const P& p, const P& a, const P& b, const P& c) {
    const P n = cross(sub(b, a), sub(c, a));
    const double n2 = dot(n, n);
    if (n2 > 0.0) {
        const double h = dot(sub(p, a), n);
        const P q{p.x - n.x * h / n2, p.y - n.y * h / n2, p.z - n.z * h / n2};
        if (dot(cross(sub(b, a), sub(q, a)), n) >= 0.0 && dot(cross(sub(c, b), sub(q, b)), n) >= 0.0 &&
            dot(cross(sub(a, c), sub(q, c)), n) >= 0.0)
            return h * h / n2;
    }
    return std::min({segment_distance2(p, a, b), segment_distance2(p, b, c), segment_distance2(p, c, a)});
}

// The end of the subtree that starts at `pos`; throws on a cut-off tree.
std::size_t skip(const Painting::Tree& tree, std::size_t pos, int depth = 0) {
    if (pos >= tree.size() || depth > kMaxDepth)
        throw std::invalid_argument("painting: a tree is cut off or too deep");
    if (tree[pos] != Painting::kSplit)
        return pos + 1;
    ++pos;
    for (int k = 0; k < 4; ++k)
        pos = skip(tree, pos, depth + 1);
    return pos;
}

class Painter {
public:
    explicit Painter(const Brush& brush)
        : brush_(brush), centre_(to_p(brush.centre)), r2_(brush.radius * brush.radius),
          detail2_(std::max(brush.detail, 1e-3) * std::max(brush.detail, 1e-3)) {}

    // Paints the node of `in` at pos (moving pos past it) on triangle abc.
    void node(const Painting::Tree& in, std::size_t& pos, const P& a, const P& b, const P& c, int depth,
              Painting::Tree& out) const {
        if (in[pos] != Painting::kSplit) {
            leaf(in[pos++], a, b, c, depth, out);
            return;
        }
        const std::size_t start = pos;
        pos = skip(in, pos);
        if (triangle_distance2(centre_, a, b, c) > r2_) { // untouched: kept as it was
            out.insert(out.end(), in.begin() + std::ptrdiff_t(start), in.begin() + std::ptrdiff_t(pos));
            return;
        }
        std::size_t child = start + 1;
        out.push_back(Painting::kSplit);
        const std::size_t at = out.size();
        const P ab = mid(a, b), bc = mid(b, c), ca = mid(c, a);
        node(in, child, a, ab, ca, depth + 1, out);
        node(in, child, ab, b, bc, depth + 1, out);
        node(in, child, ca, bc, c, depth + 1, out);
        node(in, child, ab, bc, ca, depth + 1, out);
        merge(out, at);
    }

    // Paints a piece that has one mark, m, all over.
    void leaf(std::uint8_t m, const P& a, const P& b, const P& c, int depth, Painting::Tree& out) const {
        if (m == brush_.mark || triangle_distance2(centre_, a, b, c) > r2_) {
            out.push_back(m);
            return;
        }
        const auto inside = [&](const P& p) { const P d = sub(p, centre_); return dot(d, d) <= r2_; };
        if (inside(a) && inside(b) && inside(c)) { // a ball holds the triangle of its corners
            out.push_back(brush_.mark);
            return;
        }
        const double longest = std::max({dot(sub(b, a), sub(b, a)), dot(sub(c, b), sub(c, b)), dot(sub(a, c), sub(a, c))});
        if (depth >= kMaxDepth || longest <= detail2_) {
            const P g{(a.x + b.x + c.x) / 3.0, (a.y + b.y + c.y) / 3.0, (a.z + b.z + c.z) / 3.0};
            out.push_back(inside(g) ? brush_.mark : m);
            return;
        }
        out.push_back(Painting::kSplit);
        const std::size_t at = out.size();
        const P ab = mid(a, b), bc = mid(b, c), ca = mid(c, a);
        leaf(m, a, ab, ca, depth + 1, out);
        leaf(m, ab, b, bc, depth + 1, out);
        leaf(m, ca, bc, c, depth + 1, out);
        leaf(m, ab, bc, ca, depth + 1, out);
        merge(out, at);
    }

private:
    // Four children with one mark become one leaf again.
    static void merge(Painting::Tree& out, std::size_t at) {
        if (out.size() == at + 4 && out[at] != Painting::kSplit && out[at] == out[at + 1] && out[at] == out[at + 2] &&
            out[at] == out[at + 3]) {
            const std::uint8_t m = out[at];
            out.resize(at - 1);
            out.push_back(m);
        }
    }

    const Brush& brush_;
    P centre_;
    double r2_, detail2_;
};

void walk(const Painting::Tree& tree, std::size_t& pos, const P& a, const P& b, const P& c,
          const std::function<void(const Vec3f&, const Vec3f&, const Vec3f&, std::uint8_t)>& piece) {
    const std::uint8_t m = tree[pos++];
    if (m != Painting::kSplit) {
        if (m != kPaintNone)
            piece(to_v(a), to_v(b), to_v(c), m);
        return;
    }
    const P ab = mid(a, b), bc = mid(b, c), ca = mid(c, a);
    walk(tree, pos, a, ab, ca, piece);
    walk(tree, pos, ab, b, bc, piece);
    walk(tree, pos, ca, bc, c, piece);
    walk(tree, pos, ab, bc, ca, piece);
}

void put32(std::string& out, std::uint32_t v) {
    for (int k = 0; k < 4; ++k)
        out.push_back(char((v >> (8 * k)) & 0xFF));
}

std::uint32_t get32(std::string_view in, std::size_t& pos) {
    if (pos + 4 > in.size())
        throw std::invalid_argument("painting: the data ends too soon");
    std::uint32_t v = 0;
    for (int k = 0; k < 4; ++k)
        v |= std::uint32_t(static_cast<unsigned char>(in[pos + std::size_t(k)])) << (8 * k);
    pos += 4;
    return v;
}

} // namespace

bool Painting::empty() const {
    for (std::size_t c = 0; c < kPaintChannels; ++c)
        if (!empty(PaintChannel(c)))
            return false;
    return true;
}

bool Painting::empty(PaintChannel channel) const { return trees_[std::size_t(channel)].empty(); }

bool Painting::has(PaintChannel channel, std::uint8_t mark) const {
    for (const Tree& t : trees_[std::size_t(channel)])
        if (std::find(t.begin(), t.end(), mark) != t.end())
            return true;
    return false;
}

const Painting::Tree& Painting::tree(PaintChannel channel, std::size_t triangle) const {
    const std::vector<Tree>& trees = trees_[std::size_t(channel)];
    return triangle < trees.size() ? trees[triangle] : kUnpainted;
}

void Painting::set_tree(PaintChannel channel, std::size_t triangle, Tree tree) {
    std::vector<Tree>& trees = trees_[std::size_t(channel)];
    if (tree.size() == 1 && tree[0] == kPaintNone)
        tree.clear();
    if (tree.empty()) {
        if (triangle < trees.size())
            trees[triangle].clear();
        while (!trees.empty() && trees.back().empty()) // nothing kept past the last painted triangle
            trees.pop_back();
        return;
    }
    if (trees.size() <= triangle)
        trees.resize(triangle + 1);
    trees[triangle] = std::move(tree);
}

void Painting::clear(PaintChannel channel) { trees_[std::size_t(channel)].clear(); }

void Painting::append(const Painting& other, std::size_t triangles) {
    for (std::size_t c = 0; c < kPaintChannels; ++c) {
        const std::vector<Tree>& more = other.trees_[c];
        if (more.empty())
            continue;
        std::vector<Tree>& trees = trees_[c];
        trees.resize(std::max(trees.size(), triangles + more.size()));
        std::copy(more.begin(), more.end(), trees.begin() + std::ptrdiff_t(triangles));
    }
}

void for_each_painted(const Painting& painting, PaintChannel channel, std::size_t triangle, const Vec3f& a,
                      const Vec3f& b, const Vec3f& c,
                      const std::function<void(const Vec3f&, const Vec3f&, const Vec3f&, std::uint8_t)>& piece) {
    const Painting::Tree& tree = painting.tree(channel, triangle);
    if (tree.empty())
        return;
    std::size_t pos = 0;
    walk(tree, pos, to_p(a), to_p(b), to_p(c), piece);
}

std::size_t paint(Painting& painting, PaintChannel channel, const TriangleMesh& mesh, const Brush& brush,
                  const std::vector<std::uint32_t>* near) {
    if (!(brush.radius > 0.0) || brush.mark > kPaintMaxMark)
        return 0;
    const Painter painter(brush);
    const P centre = to_p(brush.centre), view = to_p(brush.view);
    const double r = brush.radius;
    std::size_t changed = 0;
    const std::size_t count = near ? near->size() : mesh.triangle_count();
    for (std::size_t k = 0; k < count; ++k) {
        const std::size_t t = near ? std::size_t((*near)[k]) : k;
        if (t >= mesh.triangle_count())
            continue;
        const P a = to_p(mesh.corner(t, 0)), b = to_p(mesh.corner(t, 1)), c = to_p(mesh.corner(t, 2));
        // Far from the brush, or facing away from the eye.
        if (std::min({a.x, b.x, c.x}) > centre.x + r || std::max({a.x, b.x, c.x}) < centre.x - r ||
            std::min({a.y, b.y, c.y}) > centre.y + r || std::max({a.y, b.y, c.y}) < centre.y - r ||
            std::min({a.z, b.z, c.z}) > centre.z + r || std::max({a.z, b.z, c.z}) < centre.z - r)
            continue;
        if (dot(cross(sub(b, a), sub(c, a)), view) > 0.0)
            continue;
        const Painting::Tree& before = painting.tree(channel, t);
        Painting::Tree after;
        if (before.empty()) {
            painter.leaf(kPaintNone, a, b, c, 0, after);
        } else {
            std::size_t pos = 0;
            painter.node(before, pos, a, b, c, 0, after);
        }
        if (after.size() == 1 && after[0] == kPaintNone)
            after.clear();
        if (after != before) {
            painting.set_tree(channel, t, std::move(after));
            ++changed;
        }
    }
    return changed;
}

std::string painting_to_bytes(const Painting& painting) {
    std::string out;
    for (std::size_t c = 0; c < kPaintChannels; ++c) {
        const PaintChannel channel = PaintChannel(c);
        std::uint32_t count = 0;
        for (std::size_t t = 0; t < painting.size(channel); ++t)
            count += painting.tree(channel, t).empty() ? 0 : 1;
        put32(out, count);
        for (std::size_t t = 0; t < painting.size(channel); ++t) {
            const Painting::Tree& tree = painting.tree(channel, t);
            if (tree.empty())
                continue;
            put32(out, std::uint32_t(t));
            put32(out, std::uint32_t(tree.size()));
            out.append(reinterpret_cast<const char*>(tree.data()), tree.size());
        }
    }
    return out;
}

Painting painting_from_bytes(std::string_view bytes, std::size_t triangles) {
    Painting painting;
    std::size_t pos = 0;
    for (std::size_t c = 0; c < kPaintChannels; ++c) {
        const std::uint32_t count = get32(bytes, pos);
        std::uint32_t last = 0;
        for (std::uint32_t k = 0; k < count; ++k) {
            const std::uint32_t t = get32(bytes, pos), size = get32(bytes, pos);
            if (t >= triangles)
                throw std::invalid_argument("painting: paints triangles the mesh does not have");
            if ((k > 0 && t <= last) || size > bytes.size() - pos)
                throw std::invalid_argument("painting: triangles out of order or a tree past the end");
            last = t;
            Painting::Tree tree(bytes.begin() + std::ptrdiff_t(pos), bytes.begin() + std::ptrdiff_t(pos + size));
            pos += size;
            if (skip(tree, 0) != tree.size())
                throw std::invalid_argument("painting: a tree has bytes left over");
            for (const std::uint8_t m : tree)
                if (m != Painting::kSplit && m > kPaintMaxMark)
                    throw std::invalid_argument("painting: a mark out of range");
            painting.set_tree(PaintChannel(c), t, std::move(tree));
        }
    }
    if (pos != bytes.size())
        throw std::invalid_argument("painting: bytes left over");
    return painting;
}

} // namespace tisma
