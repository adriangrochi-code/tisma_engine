#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace tisma {

// Voronoi diagram built by topology-oriented incremental insertion: the
// approach of Sugihara and Iri (1992) for points, extended to segments by
// Imai (1996) and Held (2001), as described in Held and Huber 2009,
// Section 2 ("Incremental construction"), and Huber 2008, Chapter 2. Own
// implementation; see PROVENANCE.md.
//
// Every insertion deletes a tree of nodes, those whose clearance disk the new
// site violates, and replaces it by the new site's cell. Numbers only choose
// among topologically valid updates (a tree of deleted nodes, every old cell
// keeps a node, every cell stays connected), so rounding cannot break the
// diagram's structure.
//
// The diagram is a half-edge graph. Each Voronoi edge is two half-edges h and
// h ^ 1 (twins) running in opposite directions; the cell of site(h) lies on
// the left of h, and next(h) continues around that cell counter-clockwise.
// Nodes have degree three. Three dummy sites around the input and far nodes on
// the rays between them keep every edge finite: the far nodes are joined by
// edges whose outer side belongs to no site (site -1).
//
// This first part inserts points; segments come next (PLAN.md, F2).
class VoronoiDiagram {
public:
    struct Node {
        double x = 0.0, y = 0.0;
        double r = 0.0;      // clearance: the distance to the sites it is equidistant to
        std::int32_t half = -1; // one half-edge leaving the node
        bool fixed = false;  // never deleted (far nodes)
        bool alive = true;
    };
    struct Half {
        std::int32_t origin = -1;
        std::int32_t next = -1, prev = -1; // around the cell on the left
        std::int32_t site = -1;            // the cell on the left; -1 outside everything
        bool alive = true;
    };
    enum class SiteKind : std::uint8_t { Dummy, Point };
    struct Site {
        SiteKind kind = SiteKind::Point;
        double x = 0.0, y = 0.0;
        std::int32_t half = -1; // one half-edge of its cell
    };

    // Starts with three dummy sites around the box [min_x, max_x] x [min_y, max_y].
    VoronoiDiagram(double min_x, double min_y, double max_x, double max_y);

    // Inserts a point and returns its site index. Points must be distinct.
    std::int32_t insert_point(double x, double y);

    const std::vector<Node>& nodes() const { return nodes_; }
    const std::vector<Half>& halves() const { return halves_; }
    const std::vector<Site>& sites() const { return sites_; }

    static std::int32_t twin(std::int32_t h) { return h ^ 1; }
    std::int32_t dest(std::int32_t h) const { return halves_[std::size_t(twin(h))].origin; }
    // The next half-edge leaving the same node, clockwise.
    std::int32_t clockwise(std::int32_t h) const { return halves_[std::size_t(twin(h))].next; }

    // Checks the half-edge structure: twins, next and prev, every node of
    // degree three, every live cell a closed loop. Returns an empty string or
    // the first problem found.
    std::string check() const;

private:
    std::int32_t new_node(double x, double y, double r);
    std::int32_t new_edge(std::int32_t a, std::int32_t b, std::int32_t left, std::int32_t right);
    void link(std::int32_t h, std::int32_t next);
    std::int32_t nearest_site(double x, double y, std::int32_t start) const;
    double site_distance2(std::int32_t site, double x, double y) const;
    bool can_mark(std::int32_t w, std::int32_t from, double x, double y) const;

    std::vector<Node> nodes_;
    std::vector<Half> halves_;
    std::vector<Site> sites_;
    std::vector<std::int32_t> free_nodes_, free_edges_; // reused slots (an edge is a pair of halves)
    std::vector<std::uint8_t> marked_;
    std::int32_t last_ = 0; // the last site inserted: where a search starts without a better hint
    // A coarse grid over the input box: the last site inserted in each square,
    // where the search for the nearest site starts (geometric hashing, as Held
    // 2001 does; own layout).
    static constexpr int kGrid = 128;
    double grid_x_ = 0.0, grid_y_ = 0.0, grid_scale_ = 1.0;
    std::vector<std::int32_t> grid_;
    std::size_t grid_cell(double x, double y) const;
    // Per site, during one insertion: marked nodes in its cell, the half-edges
    // entering and leaving the run of marked nodes, and its new edge.
    std::vector<std::int32_t> marked_count_, face_in_, face_out_, new_edge_of_;
    std::vector<std::int32_t> scratch_marked_, scratch_touched_, scratch_doomed_;
    std::vector<std::pair<std::int32_t, std::int32_t>> scratch_runs_;
};

} // namespace tisma
