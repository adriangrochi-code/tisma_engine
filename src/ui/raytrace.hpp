#pragma once

#include "mesh/mesh.hpp"
#include "ui/math.hpp"
#include "ui/scene.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

// The scene the 3D view's ray tracing shoots its rays at (soft shadows and
// ambient occlusion), and the CPU reference of the GPU's traversal: the same
// bounding volume hierarchy, the same intersections, the same layout of the
// data the shader reads. Own design; see PROVENANCE.md.
namespace tisma::ui {

inline constexpr std::uint32_t kAlwaysVisible = 31; // a kind no mask hides (the plate)

// A triangle (the model, the plate) or an oriented box (an extrusion of the
// preview, around its prism).
struct RayPrimitive {
    bool box = false;
    // Triangle: a, b, c. Box: the centre, then three orthogonal half axes.
    Vec3 p0, p1, p2, p3;
    std::uint32_t layer = 0; // preview layer; 0 for the model
    std::uint32_t kind = kAlwaysVisible; // LineKind of an extrusion (the mask hides it)
};

struct Ray {
    Vec3 origin;
    Vec3 dir; // need not be unit length; t is in units of dir
    float tmax = std::numeric_limits<float>::infinity();
};

// What a ray may hit: the layers [first_layer, last_layer] and the kinds whose
// bit is set in kind_mask (kAlwaysVisible is always on).
struct RayFilter {
    std::uint32_t first_layer = 0, last_layer = 0xFFFFFFFFu;
    std::uint32_t kind_mask = 0xFFFFFFFFu;
};

// Möller and Trumbore 1997 ("Fast, minimum storage ray/triangle
// intersection"): the distance t to the triangle (a, a + e1, a + e2), or -1.
float intersect_triangle(const Ray& ray, Vec3 a, Vec3 e1, Vec3 e2);

// The slab method of Kay and Kajiya 1986 on the box's own axes: the distance
// at which the ray enters the box, or -1. A ray that starts inside the box
// does not hit it: rays leave a surface from inside its own extrusion's box.
float intersect_box(const Ray& ray, Vec3 centre, Vec3 half_x, Vec3 half_y, Vec3 half_z);

// A bounding volume hierarchy built with the binned surface area heuristic
// (Wald 2007, "On fast construction of SAH-based bounding volume
// hierarchies"); leaves hold up to kLeafSize primitives.
class RayScene {
public:
    static constexpr std::size_t kLeafSize = 4;
    static constexpr int kTextureWidth = 4096; // texels per row of the GPU textures
    static constexpr int kNodeTexels = 3, kPrimitiveTexels = 4;
    static constexpr std::size_t kMaxPrimitives = std::size_t(1) << 22; // indices stay exact in float texels

    void build(std::vector<RayPrimitive> primitives);
    bool empty() const { return primitives_.empty(); }
    std::size_t primitive_count() const { return primitives_.size(); }
    std::size_t node_count() const { return nodes_.size(); }
    std::size_t depth() const { return depth_; }

    // Whether anything the filter lets through is hit before ray.tmax (any
    // hit, in no particular order), and the nearest such hit (-1 if none).
    bool occluded(const Ray& ray, const RayFilter& filter = {}) const;
    float closest(const Ray& ray, const RayFilter& filter = {}) const;

    // The GPU's copy, as RGBA32F texels in rows of kTextureWidth (the last row
    // padded with zeros). A node is 3 texels: (min, first child or first
    // primitive), (max, primitive count, 0 for an inner node whose children
    // are next to each other), (first layer, last layer, 0, 0). A primitive is
    // 4 texels: (p0, layer), (p1, 1 for a box), (p2, kind), (p3, 0); a
    // triangle stores a, b - a and c - a. Indices and layers are whole numbers
    // in floats, exact below 2^24.
    std::vector<float> node_texels() const;
    std::vector<float> primitive_texels() const;

private:
    struct Node {
        Vec3 lo, hi;
        std::uint32_t first = 0; // first child (inner) or first primitive (leaf)
        std::uint32_t count = 0; // primitives in a leaf; 0 for an inner node
        std::uint32_t first_layer = 0, last_layer = 0;
    };
    template <class Visit>
    void traverse(const Ray& ray, const RayFilter& filter, Visit visit) const;

    std::vector<RayPrimitive> primitives_;
    std::vector<Node> nodes_;
    std::size_t depth_ = 0;
};

// The primitives of the scenes the view shows: the model's triangles moved by
// offset, the plate (two triangles per quad of its fan, kAlwaysVisible), and
// the preview's extrusions as boxes around their prisms (the same prism the
// view draws: hanging under the path, each end grown by half a width).
std::vector<RayPrimitive> model_primitives(const TriangleMesh& mesh, Vec3 offset);
std::vector<RayPrimitive> plate_primitives(const BedPolygon& bed);
std::vector<RayPrimitive> preview_primitives(const PreviewBuffers& preview);

} // namespace tisma::ui
