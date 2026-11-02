// Solids of rings around the Z axis, for the mechanical parts of Design.
// Own design; see PROVENANCE.md.
//
// A ring is a loop of points at one height, one per angle of a fixed list
// (the same for every ring), each at its own radius, the whole ring turned by
// its own offset (a helical gear twists that way). Neighbouring rings join
// in quads, so the mesh closes by construction. The rings follow the part's
// outline in the (radius, height) half plane with the material on their left,
// so every face looks out; a last check turns the mesh out if it came in.

#pragma once

#include "mesh/mesh.hpp"

#include <vector>

namespace tisma::cad {

struct Ring {
    double z = 0;
    std::vector<double> r; // one radius per angle of the lathe
    double turn = 0;       // radians added to every angle
};

class Lathe {
public:
    // `segments` angles evenly around, from 0.
    explicit Lathe(int segments);
    // These angles, in radians, increasing, within one turn.
    explicit Lathe(std::vector<double> angles);

    int size() const { return int(angles_.size()); }
    double angle(int i) const { return angles_[std::size_t(i)]; }
    const std::vector<double>& angles() const { return angles_; }

    // A ring from a radius function of the angle.
    template <class F> Ring ring(double z, F&& radius, double turn = 0.0) const {
        Ring out{z, std::vector<double>(angles_.size()), turn};
        for (std::size_t i = 0; i < angles_.size(); ++i)
            out.r[i] = radius(angles_[i]);
        return out;
    }

    // The rings joined in order; `loop` joins the last to the first, else
    // the first and the last are closed by a fan to the axis.
    TriangleMesh mesh(const std::vector<Ring>& rings, bool loop) const;

private:
    std::vector<double> angles_;
};

// The mesh turned to face out if its volume came negative.
TriangleMesh outward(TriangleMesh m);

} // namespace tisma::cad
