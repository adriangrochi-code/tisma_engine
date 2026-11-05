// Tests of reading the printer's limits (printer_connections.md, «Leer los
// límites de la impresora»).

#include "config/settings.hpp"
#include "device/printer_limits.hpp"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

using namespace tisma;
using namespace tisma::device;

TEST_CASE("printer limits: Marlin's M115 and M503 (rules 2, 3, 6, 7)", "[device][limits]") {
    // M115 as Marlin's documentation shows it, with the geometry report.
    const std::string m115 = "FIRMWARE_NAME:Marlin 2.1.3 (Aug  1 2024 12:00:00) SOURCE_CODE_URL:github.com/MarlinFirmware/Marlin "
                             "PROTOCOL_VERSION:1.0 MACHINE_TYPE:3D Printer KINEMATICS:Cartesian EXTRUDER_COUNT:1\n"
                             "Cap:EEPROM:1\n"
                             "area:{full:{min:{x:-5,y:-10,z:0},max:{x:240,y:240,z:260}},"
                             "work:{min:{x:0,y:0,z:0},max:{x:235,y:235,z:250}}}\nok\n";
    const std::string m503 = "echo:; Linear Units:\necho:  G21 ; (mm)\n"
                             "echo:; Max feedrates (units/s):\necho:  M203 X500.00 Y500.00 Z5.00 E25.00\n"
                             "echo:  M201 X500.00 Y500.00 Z100.00 E5000.00\n"
                             "echo:  M204 P500.00 R1000.00 T1500.00\n"
                             "echo:  M205 B20000.00 S0.00 T0.00 J0.08\n"
                             "echo:  M900 K0.04\nok\n";
    const PrinterLimits l = parse_marlin_limits(m115, m503);
    CHECK(l.source == "Marlin 2.1.3");
    REQUIRE(l.has_area); // the work area, not the full one
    CHECK(l.min_x == 0.0);
    CHECK(l.max_x == 235.0);
    CHECK(l.max_z == 250.0);
    CHECK(l.values.at("max_feed_x") == 500.0);
    CHECK(l.values.at("max_feed_e") == 25.0);
    CHECK(l.values.at("max_accel_e") == 5000.0);
    CHECK(l.values.at("acceleration") == 500.0);
    CHECK(l.values.at("retract_acceleration") == 1000.0);
    CHECK(l.values.at("travel_acceleration") == 1500.0);
    CHECK(l.values.at("junction_deviation") == 0.08);
    CHECK(l.values.count("min_extrude_feed") == 0); // S0: not a limit, left out (rule 6: never 0)
    CHECK(l.values.count("jerk_x") == 0);
    REQUIRE(l.has_pressure_advance);
    CHECK(l.pressure_advance == 0.04);

    // Without the geometry report and in inches (G20): no bed, values in mm.
    const PrinterLimits inch = parse_marlin_limits("FIRMWARE_NAME:Marlin 2.0.9\n", "echo:  G20\necho:  M203 X10 Y10\n");
    CHECK_FALSE(inch.has_area);
    CHECK(inch.values.at("max_feed_x") == 254.0);
    // M2030 is not M203.
    CHECK(parse_marlin_limits("", "M2030 X5\n").values.empty());
}

TEST_CASE("printer limits: Klipper's toolhead and configfile (rule 3)", "[device][limits]") {
    const nlohmann::json toolhead = {{"max_velocity", 300.0},
                                     {"max_accel", 3000.0},
                                     {"square_corner_velocity", 5.0},
                                     {"axis_minimum", {0.0, -2.0, -2.0, 0.0}},
                                     {"axis_maximum", {250.0, 210.0, 220.0, 0.0}}};
    const nlohmann::json settings = {{"printer", {{"max_z_velocity", "15"}}}, {"extruder", {{"pressure_advance", 0.055}}}};
    const PrinterLimits l = parse_klipper_limits(toolhead, settings);
    CHECK(l.values.at("max_feed_y") == 300.0);
    CHECK(l.values.at("max_feed_z") == 15.0);
    CHECK(l.values.at("max_accel_x") == 3000.0);
    CHECK(l.values.at("corner_speed") == 5.0);
    REQUIRE(l.has_area);
    CHECK(l.min_y == -2.0);
    CHECK(l.max_y == 210.0);
    CHECK(l.max_z == 220.0);
    CHECK(l.pressure_advance == 0.055);
    CHECK(parse_klipper_limits(nullptr, nullptr).values.empty());
}

TEST_CASE("printer limits: only the empty values are filled by themselves (rule 4)", "[device][limits]") {
    Settings s;
    s.max_feed_x = 400.0; // written by the person
    PrinterLimits l;
    l.values = {{"max_feed_x", 500.0}, {"max_feed_y", 500.0}};
    l.has_area = true;
    l.max_x = l.max_y = 220.0;
    l.max_z = 250.0;
    const std::vector<std::string> filled = apply_limits(s, l, true);
    CHECK(s.max_feed_x == 400.0); // kept
    CHECK(s.max_feed_y == 500.0);
    CHECK(s.bed.size() == 4);
    CHECK(s.max_height == 250.0);
    CHECK(filled == std::vector<std::string>{"max_feed_y", "bed", "max_height"});
    // «Leer de la impresora» takes everything.
    CHECK(apply_limits(s, l, false) == std::vector<std::string>{"max_feed_x"});
    CHECK(s.max_feed_x == 500.0);
}
