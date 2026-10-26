#pragma once

#include "mesh/mesh.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

// What is painted on a model's faces (docs/PAINTING.md): where support goes or
// must not go, where the seam goes or must not go, where the skin is fuzzy, and
// which extruder prints each part. Own design; see PROVENANCE.md.
namespace tisma {

enum class PaintChannel : std::uint8_t { Support, Seam, Fuzzy, Color };
inline constexpr std::size_t kPaintChannels = 4;

// The marks of the support and seam channels; fuzzy skin uses kPaintEnforce
// alone, and the colour channel the extruder number from 1 (0: the model's own).
inline constexpr std::uint8_t kPaintNone = 0;
inline constexpr std::uint8_t kPaintEnforce = 1;
inline constexpr std::uint8_t kPaintBlock = 2;
inline constexpr std::uint8_t kPaintMaxMark = 16;

// The marks on every triangle of one mesh, each channel apart. A triangle's
// marks are a tree: either one mark for all of it, or the triangle cut at the
// midpoints of its edges into four smaller ones (at corner 0, corner 1, corner
// 2, and the middle one), each a tree again. So a brush can paint a detail on
// a large face without changing the mesh, and the paint follows the model when
// it is moved, turned or scaled.
class Painting {
public:
    // Stored depth first, one byte per node: a mark, or kSplit followed by the
    // four children.
    static constexpr std::uint8_t kSplit = 0xFF;
    using Tree = std::vector<std::uint8_t>;

    bool empty() const;
    bool empty(PaintChannel channel) const;
    // Whether any triangle of the channel has this mark somewhere.
    bool has(PaintChannel channel, std::uint8_t mark) const;

    // The tree of triangle t (empty: unpainted, mark 0).
    const Tree& tree(PaintChannel channel, std::size_t triangle) const;
    void set_tree(PaintChannel channel, std::size_t triangle, Tree tree);
    void clear(PaintChannel channel);

    // How many triangles have a tree in the channel (the largest index + 1).
    std::size_t size(PaintChannel channel) const { return trees_[std::size_t(channel)].size(); }

    // Appends another mesh's painting after `triangles` triangles of this one,
    // as the plate joins its models into one mesh.
    void append(const Painting& other, std::size_t triangles);

    friend bool operator==(const Painting&, const Painting&) = default;

private:
    std::array<std::vector<Tree>, kPaintChannels> trees_;
};

// Each painted piece of triangle t with its mark (never 0), as the corners of
// a smaller triangle with the same winding.
void for_each_painted(const Painting& painting, PaintChannel channel, std::size_t triangle, const Vec3f& a,
                      const Vec3f& b, const Vec3f& c,
                      const std::function<void(const Vec3f&, const Vec3f&, const Vec3f&, std::uint8_t)>& piece);

// A round brush: what lies within `radius` of `centre` gets `mark` (0 erases).
// A triangle wholly inside takes the mark; one the brush's edge crosses is cut
// into four, down to pieces about `detail` long, and the smallest pieces take
// the mark when their middle is inside. Triangles facing away from `view`
// (pointing the same way, when it is not zero) are left alone, so a brush on
// the front of a thin wall does not paint its back. `mesh` is the model as
// placed: the trees do not depend on where it is.
struct Brush {
    Vec3f centre;
    double radius = 2.0;
    double detail = 0.25;
    Vec3f view{0.0f, 0.0f, 0.0f};
    std::uint8_t mark = kPaintEnforce;
};
// Returns how many triangles changed.
std::size_t paint(Painting& painting, PaintChannel channel, const TriangleMesh& mesh, const Brush& brush);

// The painting as bytes for the project file, and back: for each channel the
// number of painted triangles, then each one's index, the length of its tree
// and the tree (32-bit numbers, little-endian). Reading throws
// std::invalid_argument on bytes that are not a painting of a mesh of this
// many triangles.
std::string painting_to_bytes(const Painting& painting);
Painting painting_from_bytes(std::string_view bytes, std::size_t triangles);

} // namespace tisma
