#include "device/printer.hpp"

#include <cctype>
#include <cmath>
#include <cstdio>

namespace tisma::device {
namespace {

std::string fixed(double v, int decimals) {
    char text[32];
    std::snprintf(text, sizeof text, "%.*f", decimals, v);
    return text;
}

std::string feed(double mm_s) { return " F" + std::to_string(std::lround(std::max(mm_s, 0.1) * 60.0)); }

} // namespace

bool Printer::set_nozzle(double celsius) { return send("M104 S" + std::to_string(std::lround(std::max(celsius, 0.0)))); }
bool Printer::set_bed(double celsius) { return send("M140 S" + std::to_string(std::lround(std::max(celsius, 0.0)))); }
bool Printer::home() { return send("G28"); }

bool Printer::move(char axis, double mm, double feed_mm_s) {
    axis = char(std::toupper(static_cast<unsigned char>(axis)));
    if (axis != 'X' && axis != 'Y' && axis != 'Z')
        return false;
    return send("G91") && send(std::string("G0 ") + axis + fixed(mm, 3) + feed(feed_mm_s)) && send("G90");
}

bool Printer::extrude(double mm, double feed_mm_s) {
    return send("G91") && send("G1 E" + fixed(mm, 3) + feed(feed_mm_s)) && send("G90");
}

bool Printer::fan(double percent) {
    const double clamped = std::min(std::max(percent, 0.0), 100.0);
    return send(clamped <= 0.0 ? std::string("M107") : "M106 S" + std::to_string(std::lround(clamped * 2.55)));
}

const char* printer_state_name(PrinterState state) {
    switch (state) {
    case PrinterState::Disconnected: return "disconnected";
    case PrinterState::Connecting: return "connecting";
    case PrinterState::Idle: return "idle";
    case PrinterState::Printing: return "printing";
    case PrinterState::Paused: return "paused";
    case PrinterState::Error: return "error";
    }
    return "disconnected";
}

} // namespace tisma::device
