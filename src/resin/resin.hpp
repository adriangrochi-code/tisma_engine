#pragma once

#include "config/settings.hpp"
#include "geom/polygons.hpp"
#include "mesh/indexed_mesh.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <stdexcept>
#include <vector>

// Resin printing on an MSLA printer (docs/spec/features/resin.md): every
// layer of the part becomes a grey image of the printer's display, each
// pixel lit by the share of its area the section covers. Own design after
// the spec; see PROVENANCE.md.
namespace tisma {

class ResinError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// A grid of pixels over the plane: column 0's left side at x = left, row 0's
// top side at y = top (rows go down in y), each pixel pixel_x by pixel_y mm;
// mirror_x turns the columns around, mirror_y the rows.
struct PixelGrid {
    int width = 0, height = 0;
    double left = 0.0, top = 0.0;
    double pixel_x = 1.0, pixel_y = 1.0;
    bool mirror_x = false, mirror_y = false;
};

// The printer's display (rules 2 and 3): x and y measured from its corner,
// row 0 at the largest y.
PixelGrid display_grid(const Settings& settings);

// The share of each pixel's area inside the region, exactly (the area of
// the region within the pixel over the pixel's), row by row from the top
// over the region's box: row(r, x0, share) gets the shares of columns x0
// on of row r. Rows and columns the region does not reach are left out;
// what lies off the grid is cut away.
void coverage_rows(const Polygons& region, const PixelGrid& grid,
                   const std::function<void(int row, int x0, std::span<const double> share)>& row);

// A pixel's grey from the share it is covered (rules 4 and 20): the share
// by 255 rounded up, so that every pixel the section reaches lights (within
// a twentieth of a level, against rounding errors); then the gamma curve,
// rounded, never darker than 1 if it was lit. Gamma 0: 255 from half the
// pixel up, else 0.
std::uint8_t resin_grey(double share, double gamma);

// Run-length coding of a display image (the GOO format's, rule 11): 0x55,
// chunks of equal pixels, and the checksum (the complement of the 8-bit sum
// of the bytes between). Pixels come in row by row from the top.
class GooImageEncoder {
public:
    void add(std::uint8_t value, std::size_t count = 1);
    // The coded image; the encoder is empty after.
    std::vector<std::uint8_t> finish();

private:
    void flush();
    std::vector<std::uint8_t> bytes_{0x55};
    std::uint8_t value_ = 0;
    std::size_t count_ = 0;
};

// An image decoded: width by height pixels, row by row from the top. Throws
// ResinError when the data is not a valid image of that size.
std::vector<std::uint8_t> decode_goo_image(std::span<const std::uint8_t> data, int width, int height);

// One layer of a resin print: its top, how long and how bright it is
// exposed, and the platform's moves after it (rules 7 and 8), mm, mm/s and s.
struct ResinLayer {
    double z = 0.0;
    double exposure = 0.0;
    double lift_distance = 0.0, lift_speed = 0.0;
    double retract_distance = 0.0, retract_speed = 0.0;
    double wait_after_cure = 0.0, wait_after_lift = 0.0, wait_before_exposure = 0.0;
    int pwm = 255;
    bool bottom = false;
    std::vector<std::uint8_t> image; // GOO coded
    std::size_t lit = 0;             // pixels with light
};

struct ResinPrint {
    std::vector<ResinLayer> layers;
    int bottom_layers = 1;        // the layers exposed as bottom ones, at least 1 (rule 7)
    double volume = 0.0;          // mm3 of the part (scaled)
    double weight = 0.0;          // g
    double seconds = 0.0;         // the print's estimated time
    double max_height = 150.0;    // the printer's, mm
    // Previews (116 by 116 and 290 by 290 pixels, RGB565, row by row from
    // the top): the part seen from above, shaded by its height.
    std::vector<std::uint16_t> small_preview, big_preview;
};

// The part's sections, one per layer of resin.layer_height from the
// platform (rules 1 and 5): the mesh scaled by resin.scale_* about its box's
// centre in x and y and from its base, laid on the platform.
std::vector<Polygons> resin_sections(const IndexedMesh& mesh, const Settings& settings);

// The part's print (rules 1 to 8 and 20 to 22): the sections grown by
// resin.absolute_correction, the bottom layers shrunk against the
// elephant's foot, what hangs left dark for resin.z_correction_layers
// layers; coded images, exposures and moves. exposure(z) gives the exposure
// of the normal layer whose top is at z (the exposure tower), else
// resin.exposure. Throws ResinError when the part does not fit on the
// display or under machine.max_height (150 mm when 0).
ResinPrint slice_resin(const IndexedMesh& mesh, const Settings& settings,
                       const std::function<double(double z)>& exposure = {});

} // namespace tisma
