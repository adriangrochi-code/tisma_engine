#pragma once

#include "config/settings.hpp"
#include "slice/contours.hpp"

#include <clipper2/clipper.h>

#include <cmath>
#include <cstdint>
#include <vector>

namespace tisma {

// Planar regions of a layer as integer polygons for Clipper2. Coordinates are
// in units of kUnitsPerMm (1 unit = 1 nm), so float millimetres convert
// without loss and int64 leaves room for any printable size.
using Polygon = Clipper2Lib::Path64;
using Polygons = Clipper2Lib::Paths64;

inline constexpr double kUnitsPerMm = 1e6;

inline std::int64_t to_units(double mm) { return std::llround(mm * kUnitsPerMm); }
inline double to_mm(std::int64_t units) { return double(units) / kUnitsPerMm; }

// The closed contours of a planar layer projected on XY. Open contours are
// left out: they enclose nothing.
Polygons to_polygons(const std::vector<Contour>& contours);

// The region enclosed by the polygons, with overlaps merged and holes kept.
// Uses the non-zero rule: outer contours counter-clockwise and holes clockwise
// (as level_set_segments produces them) cancel inside holes, overlapping
// volumes add up, and a mesh wound the wrong way still gives a region.
Polygons layer_region(const Polygons& polygons);

// Same with a choice of rule (docs/CONFIG.md, slicing.fill_mode): Union is the
// non-zero rule above; EvenOdd alternates inside and outside at every crossing;
// FillHoles is Union with the holes filled in.
Polygons layer_region(const Polygons& polygons, FillMode mode);

// Removes vertices closer than tolerance_mm to the line through their
// neighbours (Clipper2's SimplifyPaths). The result may touch itself; build the
// region from it with layer_region.
Polygons simplify(const Polygons& polygons, double tolerance_mm);

// Grows the region by delta_mm (shrinks it when negative): outer contours move
// out and holes move in. Corners are mitered up to miter_limit times delta,
// as in XY size compensation.
Polygons offset(const Polygons& region, double delta_mm, double miter_limit = 2.0);

// Grows only the holes of the region by delta_mm (shrinks them when
// negative), leaving the outer contours where they are: each piece's own
// holes are offset as filled shapes and taken out of it, so islands inside
// a hole are kept. The compensation of holes (docs/CALIBRATION.md).
Polygons offset_holes(const Polygons& region, double delta_mm);

// layer_region(to_polygons(contours)) for every layer, in parallel.
std::vector<Polygons> layer_regions(const std::vector<std::vector<Contour>>& layers);

// Enclosed area in mm2: counter-clockwise polygons count positive, holes negative.
double area_mm2(const Polygons& region);

// Whether the point (mm) is in the region, by the non-zero rule (outer
// contours counter-clockwise, holes clockwise); on its edge counts as in.
bool region_contains(const Polygons& region, double x, double y);

// The region of the set cells of a grid of nx by ny cells, hx by hy mm from
// (x0, y0), row by row (cell (i, j) is mask[j * nx + i]): each row's runs of
// set cells as rectangles, joined, and closed over by half a cell so that
// diagonal neighbours join.
Polygons cell_region(const std::vector<char>& mask, int nx, int ny, double x0, double y0, double hx, double hy);

} // namespace tisma
