#pragma once

#include "perimeters/sites.hpp"

namespace tisma {

// A path with its corners rounded (toolpath_time.md, rule 7): each vertex
// where the path turns is replaced by an arc tangent to its two segments that
// strays at most `deviation` mm from the vertex, so the printer need not stop
// there. The arc never takes more than 45 % of either segment; the ends of an
// open path stay. Width and heights are the vertex's along the arc.
Toolpath round_corners(const Toolpath& path, double deviation);

} // namespace tisma
