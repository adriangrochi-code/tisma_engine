#include "mesh/mesh.hpp"

#include <algorithm>

namespace tisma {

BoundingBox TriangleMesh::bounding_box() const {
    BoundingBox box;
    if (corners_.empty())
        return box;
    box.min = box.max = corners_.front();
    for (const Vec3f& p : corners_) {
        box.min.x = std::min(box.min.x, p.x);
        box.min.y = std::min(box.min.y, p.y);
        box.min.z = std::min(box.min.z, p.z);
        box.max.x = std::max(box.max.x, p.x);
        box.max.y = std::max(box.max.y, p.y);
        box.max.z = std::max(box.max.z, p.z);
    }
    box.empty = false;
    return box;
}

double TriangleMesh::signed_volume() const {
    // Sum of the signed volumes of the tetrahedra (origin, a, b, c).
    double six_volume = 0.0;
    for (std::size_t i = 0; i + 2 < corners_.size(); i += 3) {
        const Vec3f& a = corners_[i];
        const Vec3f& b = corners_[i + 1];
        const Vec3f& c = corners_[i + 2];
        const double cx = double(b.y) * c.z - double(b.z) * c.y;
        const double cy = double(b.z) * c.x - double(b.x) * c.z;
        const double cz = double(b.x) * c.y - double(b.y) * c.x;
        six_volume += a.x * cx + a.y * cy + a.z * cz;
    }
    return six_volume / 6.0;
}

} // namespace tisma
