#pragma once

#include "geom/polygons.hpp"

#include <cstdint>
#include <vector>

namespace tisma {

struct Point2d {
    double x = 0.0;
    double y = 0.0;
};

// An edge of the medial axis: a piece of the Voronoi diagram of the region's
// boundary segments that lies inside the region. radius_a and radius_b are the
// distances from its ends to the boundary (the radius of the largest inscribed
// circle there); in Kuipers et al. 2020 they decide how many beads fit.
// Parabolic edges (between a corner and a segment) come as chains of straight
// pieces of at most kMaxCurvedPiece, marked curved (Section 3.2).
//
// support_a[k] and support_b[k] are the closest boundary points of each end on
// side k (Kuipers et al. 2020, Eq. 1, "support"): side 0 and side 1 are the two
// Voronoi cells the edge separates. A node and its two supports span the
// skeletal trapezoidation (Section 3.2): each edge with its supports on one side
// is a trapezoid (or a triangle when the supports meet).
struct MedialEdge {
    Point2d a, b;     // mm
    double radius_a;  // mm
    double radius_b;  // mm
    bool curved;
    Point2d support_a[2];
    Point2d support_b[2];
};

inline constexpr double kMaxCurvedPiece = 0.2; // mm, Kuipers et al. 2020, Section 3.2

// Medial axis of a region (outer contours counter-clockwise, holes clockwise),
// from Boost.Polygon's segment Voronoi diagram. Each edge appears once.
// Coordinates must fit in 32 bits of the 1 nm unit (about +-2 m).
std::vector<MedialEdge> medial_axis(const Polygons& region);

// The medial axis as a graph with shared nodes, and the central part marked
// (Kuipers et al. 2020, Section 3.3).
struct MedialGraph {
    struct Node {
        Point2d p;
        double radius = 0.0;
        bool central = false;
    };
    struct Edge {
        std::uint32_t a = 0, b = 0;
        bool curved = false;
        bool central = false;
        Point2d support_a[2]; // closest boundary points of node a on each side
        Point2d support_b[2];
    };
    std::vector<Node> nodes;
    std::vector<Edge> edges;
};

// Builds the graph of medial_axis(region). Ends closer than 1e-9 mm are one node.
MedialGraph medial_graph(const Polygons& region);

// Marks the centre (Section 3.3): an edge is central when it is significant,
// that is when the bisector angle alpha = 2 acos(|dR| / |dv|) along it exceeds
// alpha_max (so the ridge slope |dR| / |dv| is below cos(alpha_max / 2)); its
// ends become central too. A node is also central when its radius is larger
// than all its neighbours' (a local maximum).
void mark_central(MedialGraph& graph, double alpha_max_degrees = 135.0);

// Marking filter (Section 3.3): from every central node, walk up (towards
// larger radius) along non-central edges; when another central node is
// reached within max_length, the edges and nodes on the way become central.
// The paper uses max_length = w*, the preferred bead width.
void filter_marking(MedialGraph& graph, double max_length);

} // namespace tisma
