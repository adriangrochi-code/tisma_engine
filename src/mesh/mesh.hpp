#pragma once

#include <cstddef>
#include <vector>

namespace tisma {

struct Vec3f {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    friend bool operator==(const Vec3f&, const Vec3f&) = default;
};

struct BoundingBox {
    Vec3f min;
    Vec3f max;
    bool empty = true;
};

// Triangles stored as a flat, contiguous list of corners: triangle i uses
// corners[3*i], corners[3*i+1] and corners[3*i+2]. Facet normals from the file
// are not kept; they are recomputed from the winding when needed.
class TriangleMesh {
public:
    void reserve(std::size_t triangles) { corners_.reserve(3 * triangles); }

    void add_triangle(const Vec3f& a, const Vec3f& b, const Vec3f& c) {
        corners_.push_back(a);
        corners_.push_back(b);
        corners_.push_back(c);
    }

    // Every corner moved by (dx, dy, dz).
    void translate(float dx, float dy, float dz) {
        for (Vec3f& v : corners_)
            v.x += dx, v.y += dy, v.z += dz;
    }

    std::size_t triangle_count() const { return corners_.size() / 3; }
    bool empty() const { return corners_.empty(); }

    const Vec3f& corner(std::size_t triangle, int k) const { return corners_[3 * triangle + static_cast<std::size_t>(k)]; }
    const std::vector<Vec3f>& corners() const { return corners_; }

    BoundingBox bounding_box() const;

    // Signed volume from the divergence theorem; positive for a closed mesh
    // whose triangles are wound counter-clockwise seen from outside.
    double signed_volume() const;

private:
    std::vector<Vec3f> corners_;
};

} // namespace tisma
