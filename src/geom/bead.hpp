#pragma once

#include <numbers>

namespace tisma {

// The section of a bead w wide and h high (gcode_motion.md, rule 1; the
// Slic3r manual's «Flow Math»): a rectangle with rounded sides, (w - h) h +
// pi (h/2)^2. A bead no wider than it is high is an ellipse, pi w h / 4: the
// same at w = h, a round bead of that diameter.
inline double bead_area(double width, double height) {
    if (!(width > 0.0) || !(height > 0.0))
        return 0.0;
    if (width <= height)
        return std::numbers::pi / 4.0 * width * height;
    return (width - height) * height + std::numbers::pi / 4.0 * height * height;
}

// The width of a bead h high with this section: bead_area's inverse.
inline double bead_width(double area, double height) {
    if (!(area > 0.0) || !(height > 0.0))
        return 0.0;
    const double round = std::numbers::pi / 4.0 * height * height; // the section at w = h
    return area <= round ? area / (std::numbers::pi / 4.0 * height) : area / height + height * (1.0 - std::numbers::pi / 4.0);
}

// How much two neighbouring beads h high overlap (perimeters.md, rule 1):
// their centres are the mean of their widths less this apart, so that the
// rounded sides fill each other's hollows.
inline double bead_overlap(double height) {
    return height > 0.0 ? height * (1.0 - std::numbers::pi / 4.0) : 0.0;
}

} // namespace tisma
