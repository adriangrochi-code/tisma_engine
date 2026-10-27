#pragma once

#include "config/settings.hpp"
#include "geom/polygons.hpp"
#include "mesh/indexed_mesh.hpp"
#include "report/warnings.hpp"
#include "resin/hollow.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <stdexcept>
#include <string>
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

// A cup (rules 13 and 14): a hollow closed towards the platform and open
// towards the film, which the film seals as each layer peels off. Its
// layers, the heights from its floor to where it closes or the part ends,
// and its largest section.
struct ResinCup {
    int first = 0, last = 0;
    double z0 = 0.0, z1 = 0.0; // mm
    double area = 0.0;         // mm2
};

// Cups of these sections, one per layer `layer_height` thick from the
// platform (rule 14): layer by layer, a hole of a section is vented when at
// least kCupExit of it lies over air that is vented (the air around the
// part, or a vented hole) on the layer below; the first layer's holes are
// closed by the platform or the pad, unless the part is `raised` on
// supports. The holes that are not vented, joined where they overlap from
// layer to layer, are the cups; those whose largest section is under
// kCupReport are left out.
inline constexpr double kCupExit = 3.0;   // mm2, about 2 mm across
inline constexpr double kCupReport = 4.0; // mm2
std::vector<ResinCup> find_cups(const std::vector<Polygons>& sections, double layer_height, bool raised);

struct ResinPrint {
    std::vector<ResinLayer> layers;
    std::vector<ResinCup> cups;
    int bottom_layers = 1;        // the layers exposed as bottom ones, at least 1 (rule 7)
    double volume = 0.0;          // mm3 of the part (scaled)
    double weight = 0.0;          // g
    double cost = 0.0;            // the resin's, by its bottle
    double seconds = 0.0;         // the print's estimated time
    double max_height = 150.0;    // the printer's, mm
    double bar_width = 0.0;       // the hollow's inner bars, mm; 0: none
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
// layers; coded images, exposures and moves; and the cups. With
// resin.supports the part stands on supports over the pad (and its cups are
// open underneath), hollowed with resin.hollow, with the drain holes taken
// out (docs/spec/features/resin_supports.md). exposure(z) gives the
// exposure of the normal layer whose middle is at z (the exposure tower),
// else resin.exposure. Throws ResinError when the part (or its supports and
// pad) does not fit on the display or under machine.max_height (150 mm when
// 0), or when the pad has nothing to stand under or around.
ResinPrint slice_resin(const IndexedMesh& mesh, const Settings& settings,
                       const std::function<double(double z)>& exposure = {},
                       const std::vector<ResinDrain>& drains = {});

// The print's warnings, for the warnings' JSON: each cup (resin.md, rule
// 15) with the part, the heights and the largest section, and how to fix
// it; and inner bars thinner than 0.8 mm (resin_supports.md, rule 5).
std::vector<Warning> resin_warnings(const ResinPrint& print, const std::string& part);

// The exposure tower (rules 16 and 17): a base and a band per exposure, from
// start to end by step, each `band` mm tall.
struct ResinTower {
    double start = 1.5, end = 3.5, step = 0.25, band = 2.0;
};
// The exposures the tower tries, from start to end (at most 40).
std::vector<double> tower_exposures(const ResinTower& tower);
// The exposure of the normal layer whose middle is at z (rule 17): band i
// starts at i · band; band 0, the base, keeps `base`; band i from 1 tries
// start + (i - 1) · step, never past the end.
double tower_exposure(const ResinTower& tower, double base, double z);
// The tower's part (calibration_models.md, "Resina"), standing on z = 0 and
// centred on the origin: 30 mm wide and 4 mm deep, a band per exposure over a
// base band. Each band has a slot in front at its foot (0.3 mm high, 0.8 mm
// deep); round holes from front to back at its middle (0.4, 0.6, 0.8, 1.0
// and 1.4 mm across, 3, 7, 11, 15 and 19 mm from the left; each only if it
// leaves 0.3 mm above and below), which close with too much light; and fins
// at the back (0.15, 0.25, 0.35 and 0.5 mm thick, 22 to 28 mm from the left,
// 1.5 mm out, from 0.2 mm over the band's foot to 0.2 mm under the next),
// which go missing or bend with too little. Throws ResinError when the step
// or the band's height is 0.
TriangleMesh resin_tower_mesh(const ResinTower& tower);

// The measuring block (rule 19): 30 by 30 by 10 mm, centred on the origin.
TriangleMesh resin_measure_block();
// The scale corrections from what the block measures once printed (rule
// 19): each the current one times the wanted size over the printed one.
std::array<double, 3> resin_scale_from(const Settings& settings, double x, double y, double z);

} // namespace tisma
