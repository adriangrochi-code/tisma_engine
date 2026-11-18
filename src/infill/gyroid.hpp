#pragma once

#include <cstddef>

#include "geom/polygons.hpp"
#include "perimeters/sites.hpp"

#include <functional>
#include <cmath>
#include <vector>

namespace tisma {

// Gyroid infill (docs/spec/SOURCES.md): the curves where
// sin X cos Y + sin Y cos Z + sin Z cos X = 0, with X, Y, Z = 2 pi (x, y, z) /
// period, at height z, clipped to the region shrunk by width / 2, as open
// toolpaths of constant width. The curves come from marching squares on a
// grid fixed to the origin, kGyroidSamples steps per period; points within
// kGyroidTolerance of the line between their neighbours are left out. Curve
// ends that follow each other along the boundary, up to half a period apart,
// are joined along it (join_along_boundary), anchoring them to the walls.
inline constexpr int kGyroidSamples = 24;
inline constexpr double kGyroidTolerance = 0.005; // mm
// Nodes of the sampling grid marched at once: larger regions go in bands of rows.
inline constexpr std::size_t kGyroidBandNodes = std::size_t(1) << 22;
// A field to march instead of the gyroid at `period` (its value at x, y in
// the plane): the blended gyroids of the adaptive density (orca_2_5.md,
// rule 10). `period` then sets the grid's step and how far ends join.
using GyroidField = std::function<double(double, double)>;
std::vector<Toolpath> gyroid_fill(const Polygons& region, double period, double z, double width,
                                  const GyroidField* field = nullptr);
// The gyroid's value at a point for a period.
inline double gyroid_value(double x, double y, double z, double period) {
    const double k = 2.0 * 3.14159265358979323846 / period;
    return std::sin(k * x) * std::cos(k * y) + std::sin(k * y) * std::cos(k * z) + std::sin(k * z) * std::cos(k * x);
}

// Two more triply periodic minimal surfaces marched the same way
// (orca_2_5.md, rule 10), by their usual level-set approximations:
// Schwarz D (Diamond), cos X cos Y cos Z - sin X sin Y sin Z = 0, and
// Fischer-Koch S, cos 2X sin Y cos Z + cos X cos 2Y sin Z + sin X cos Y cos 2Z = 0.
enum class Tpms { Gyroid, SchwarzD, FischerKochS };
inline double tpms_value(Tpms kind, double x, double y, double z, double period) {
    const double k = 2.0 * 3.14159265358979323846 / period;
    const double X = k * x, Y = k * y, Z = k * z;
    switch (kind) {
    case Tpms::SchwarzD:
        return std::cos(X) * std::cos(Y) * std::cos(Z) - std::sin(X) * std::sin(Y) * std::sin(Z);
    case Tpms::FischerKochS:
        return std::cos(2.0 * X) * std::sin(Y) * std::cos(Z) + std::cos(X) * std::cos(2.0 * Y) * std::sin(Z) +
               std::sin(X) * std::cos(Y) * std::cos(2.0 * Z);
    case Tpms::Gyroid:
        break;
    }
    return gyroid_value(x, y, z, period);
}

// The period for a density (0 to 1) with beads of this width: at fixed z the
// curves are about 2.61 / period long per unit area (measured numerically,
// averaged over z), and the material is that length times the width.
inline constexpr double kGyroidLengthPerArea = 2.61;
inline double gyroid_period(double width, double density) { return kGyroidLengthPerArea * width / density; }
// The same for Schwarz D (3.44) and Fischer-Koch S (4.40), measured the same
// way against the gyroid's 2.61.
inline constexpr double kSchwarzDLengthPerArea = 3.44;
inline constexpr double kFischerKochLengthPerArea = 4.40;
inline double tpms_period(Tpms kind, double width, double density) {
    const double c = kind == Tpms::SchwarzD       ? kSchwarzDLengthPerArea
                     : kind == Tpms::FischerKochS ? kFischerKochLengthPerArea
                                                  : kGyroidLengthPerArea;
    return c * width / density;
}

} // namespace tisma
