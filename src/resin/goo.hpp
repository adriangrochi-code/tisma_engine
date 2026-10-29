#pragma once

#include "config/settings.hpp"
#include "resin/resin.hpp"

#include <cstddef>
#include <cstdint>
#include <istream>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

// The GOO file of Elegoo's resin printers (docs/spec/features/resin.md,
// rules 9 to 12), written and read after Elegoo's "Goo Format Spec V1.2":
// a header, each layer's definition and image, and a fixed ending; every
// number big-endian, speeds in mm/min.
namespace tisma {

inline constexpr std::size_t kGooHeaderSize = 195477;

struct GooHeader {
    std::string version, software, software_version, created, printer_name, printer_type, profile_name;
    int antialiasing = 0, grey_level = 0, blur_level = 0;
    std::vector<std::uint16_t> small_preview, big_preview; // 116 by 116 and 290 by 290, RGB565
    int layers = 0;
    int x_resolution = 0, y_resolution = 0;
    bool mirror_x = false, mirror_y = false;
    float size_x = 0, size_y = 0, size_z = 0;
    float layer_height = 0, exposure = 0;
    bool fixed_waits = false; // the "exposure delay mode": 1 waits as given, 0 a turn-off time
    float off_time = 0;
    float bottom_before_lift = 0, bottom_after_lift = 0, bottom_after_retract = 0;
    float before_lift = 0, after_lift = 0, after_retract = 0;
    float bottom_exposure = 0;
    int bottom_layers = 0;
    float bottom_lift_distance = 0, bottom_lift_speed = 0, lift_distance = 0, lift_speed = 0;
    float bottom_retract_distance = 0, bottom_retract_speed = 0, retract_distance = 0, retract_speed = 0;
    float bottom_second_lift_distance = 0, bottom_second_lift_speed = 0, second_lift_distance = 0, second_lift_speed = 0;
    float bottom_second_retract_distance = 0, bottom_second_retract_speed = 0, second_retract_distance = 0,
          second_retract_speed = 0;
    int bottom_pwm = 0, pwm = 0;
    bool advanced = false; // 1: the printer takes each layer's values
    int print_seconds = 0;
    float volume = 0, weight = 0, price = 0;
    std::string price_unit;
    int layer_offset = 0;
    bool greyscale = false; // 1: greys from 0 to 255
    int transition_layers = 0;
};

struct GooLayer {
    int pause = 0;
    float pause_z = 0, z = 0, exposure = 0, off_time = 0;
    float before_lift = 0, after_lift = 0, after_retract = 0;
    float lift_distance = 0, lift_speed = 0, second_lift_distance = 0, second_lift_speed = 0;
    float retract_distance = 0, retract_speed = 0, second_retract_distance = 0, second_retract_speed = 0;
    int pwm = 0;
    std::vector<std::uint8_t> image; // coded, 0x55 to the checksum
};

struct GooFile {
    GooHeader header;
    std::vector<GooLayer> layers;
};

// The file of the print (rules 9 to 12). `created` is the file's time,
// "YYYY-MM-DD hh:mm:ss". The advanced mode is on when a layer's values
// differ from those the header gives a layer of its kind (bottom or not).
void write_goo(std::ostream& out, const ResinPrint& print, const Settings& settings, std::string_view created);

// A GOO file read back; throws ResinError when it does not follow the spec.
GooFile read_goo(std::istream& in);

// The local time now as the header writes it.
std::string goo_time_now();

} // namespace tisma
