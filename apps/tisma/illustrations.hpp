#pragma once

#include <imgui.h>

#include <string_view>

// Small drawings that show what a setting or a basic idea is about, for the
// information cards of the settings and the first start's tutorial (the
// owner's request, 2026-10-29: someone who never used a slicer should
// understand them). Own drawings, made of lines and shapes; see PROVENANCE.md.
namespace tisma {

enum class Picture {
    None,
    Slicing,     // a part cut into layers, and the layers into lines
    LayerHeight, // thin layers and thick ones, side by side
    FirstLayer,  // the first layer on the bed, thicker
    Walls,       // a part seen from above, with its outlines
    TopBottom,   // a cut through a part: solid top and bottom, infill inside
    Infill,      // a part seen from above, filled at three densities
    Seam,        // the outlines' starting points, one above the other
    Support,     // an overhang held from the bed
    Skirt,       // a loop around the part, not touching it
    Brim,        // loops around the part's foot, touching it
    NozzleTemp,  // the nozzle, the heat and a thermometer
    BedTemp,     // the bed warming the part's first layer
    Fan,         // the fan blowing on the fresh layer
    Retraction,  // the filament pulled back before a travel
    Lift,        // the nozzle going up over the part on a travel
    Nozzle,      // the nozzle's hole and its diameter
    Filament,    // the spool and the filament's diameter
    Bed,         // the bed's size and the printing height
};

// The colours a drawing uses.
struct PictureColors {
    ImU32 line, dim, accent, soft, fill, hot, cold;
};

// The drawing for a setting's key ("layers.height"), or None.
Picture picture_for(std::string_view key);

// Draws the picture in the box at `min` of `size`.
void draw_picture(ImDrawList* dl, ImVec2 min, ImVec2 size, Picture picture, const PictureColors& colors);

} // namespace tisma
