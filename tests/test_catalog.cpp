#include "catalog/catalog.hpp"

#include "config/profiles.hpp"
#include "gcode/check.hpp"
#include "gcode/writer.hpp"
#include "io/stl.hpp"
#include "mesh/indexed_mesh.hpp"
#include "slice/pipeline.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <sstream>

using namespace tisma;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::WithinAbs;

namespace {

// Made-up entries in the catalog's format (not real products).
constexpr const char* kPrinters = R"([
  {"vendor": "Acme", "model": "Cube 220", "build_volume": [220, 220, 250], "bed_shape": "rectangular",
   "nozzle_diameters": [0.4, 0.6, 0.2], "filament_diameter": 1.75, "firmware": "marlin", "extruder": "bowden",
   "heated_bed": true, "max_bed_temp": 100, "max_nozzle_temp": 260, "max_speed": 180, "max_acceleration": 2500,
   "auto_leveling": true, "enclosed": false, "sources": ["https://acme.example/cube220"], "evidence": "220x220x250"},
  {"vendor": "Acme", "model": "Delta 180", "build_volume": [180, 180, 300], "bed_shape": "circular",
   "nozzle_diameters": [0.4], "firmware": "klipper", "extruder": "direct",
   "sources": ["https://acme.example/delta"]},
  {"vendor": "Bolt", "model": "X1", "build_volume": [256, 256, 256], "nozzle_diameters": [0.4],
   "firmware": "bambu", "extruder": "direct", "enclosed": true, "multi_material": "4 filaments",
   "sources": ["https://bolt.example/x1"]}
])";

constexpr const char* kFilaments = R"([
  {"vendor": "Acme", "name": "Tough PLA", "material": "PLA", "diameter": 1.75, "density": 1.24,
   "nozzle_temp": [200, 230], "bed_temp": [50, 70], "sources": ["https://acme.example/pla.pdf"]},
  {"vendor": "Acme", "name": "PETG Clear", "material": "PETG", "nozzle_temp": [230, 250],
   "nozzle_temp_recommended": 245, "bed_temp": [70, 85], "fan": [30, 60], "sources": ["https://acme.example/petg"]},
  {"vendor": "Bolt", "name": "ABS", "material": "ABS", "nozzle_temp": [240, 270], "bed_temp": [90, 110],
   "enclosure": true, "sources": ["https://bolt.example/abs.pdf"]}
])";

std::string replaced(std::string text, const std::string& from, const std::string& to) {
    const std::size_t at = text.find(from);
    REQUIRE(at != std::string::npos);
    return text.replace(at, from.size(), to);
}

void fails(const std::string& printers, const std::string& filaments, const std::string& message) {
    try {
        parse_catalog(printers, filaments);
        FAIL("accepted");
    } catch (const SettingsError& e) {
        CHECK_THAT(e.what(), ContainsSubstring(message));
    }
}

} // namespace

TEST_CASE("catalog: entries are read, sorted and searched", "[catalog]") {
    const Catalog c = parse_catalog(kPrinters, kFilaments);
    REQUIRE(c.printers.size() == 3);
    REQUIRE(c.filaments.size() == 3);
    CHECK(c.printers[0].model == "Cube 220"); // by vendor, then model
    CHECK(c.printers[1].model == "Delta 180");
    CHECK(c.printers[1].circular);
    CHECK(c.printers[0].nozzles == std::vector<double>{0.4, 0.6, 0.2});
    CHECK_FALSE(c.printers[0].direct_drive);
    CHECK(c.printers[2].firmware == "bambu");
    CHECK(c.filaments[0].name == "PETG Clear");

    CHECK(find_printers(c, "acme").size() == 2);
    CHECK(find_printers(c, "ACME delta").size() == 1);
    CHECK(find_printers(c, "delta cube").empty());
    CHECK(find_printers(c, "").size() == 3);
    CHECK(find_filaments(c, "petg").size() == 1);
    CHECK(find_filaments(c, "bolt abs").size() == 1);
}

TEST_CASE("catalog: bad entries are refused by name and key", "[catalog]") {
    const std::string p = kPrinters, f = kFilaments;
    fails("{", f, "not valid JSON");
    fails(replaced(p, "[220, 220, 250]", "[220, 250]"), f, "Acme Cube 220: build_volume");
    fails(replaced(p, "[0.4, 0.6, 0.2]", "[]"), f, "nozzle_diameters");
    fails(replaced(p, "\"bowden\"", "\"belt\""), f, "extruder");
    fails(replaced(p, "\"klipper\"", "\"sailfish\""), f, "firmware");
    fails(replaced(p, "\"max_nozzle_temp\": 260", "\"max_nozzle_temp\": 60"), f, "max_nozzle_temp");
    fails(replaced(p, "[\"https://acme.example/delta\"]", "[]"), f, "Acme Delta 180: sources");
    fails(p, replaced(f, "[200, 230]", "[230, 200]"), "nozzle_temp");
    fails(p, replaced(f, "\"nozzle_temp\": [240, 270], ", ""), "Bolt ABS: needs nozzle_temp");
    fails(replaced(p, "\n]", ",\n  {\"vendor\": \"ACME\", \"model\": \"cube 220\", \"build_volume\": [100, 100, 100], "
                          "\"nozzle_diameters\": [0.4], \"sources\": [\"https://acme.example/x\"]}\n]"),
          f, "appears twice");
    // Facts come from makers, never from other programs' profiles or code.
    fails(replaced(p, "https://acme.example/delta", "https://github.com/someone/profiles"), f, "not allowed");
    CHECK_FALSE(allowed_source("https://raw.githubusercontent.com/x/y/printer.json"));
    CHECK_FALSE(allowed_source("https://github.com/OrcaSlicer/OrcaSlicer"));
    CHECK_FALSE(allowed_source("https://example.org/prusaslicer-settings"));
    CHECK(allowed_source("https://www.prusa3d.com/product/original-prusa-mk4s/"));
    CHECK(allowed_source("https://wiki.bambulab.com/en/a1"));
}

TEST_CASE("catalog: printer profiles from a model", "[catalog]") {
    const Catalog c = parse_catalog(kPrinters, kFilaments);
    const PrinterModel& cube = c.printers[0];
    const Profile p = printer_profile(cube, 0.6);
    CHECK(p.kind == ProfileKind::Printer);
    CHECK(p.name == "Acme Cube 220 0.6 mm");
    const Settings& s = p.settings;
    REQUIRE(s.bed.size() == 4);
    CHECK(s.bed[2] == std::array<double, 2>{220.0, 220.0});
    CHECK(s.max_height == 250.0);
    CHECK(s.nozzle_diameter == 0.6);
    CHECK(s.flavor == GcodeFlavor::Marlin);
    CHECK(s.retract_length == 4.0); // Bowden
    CHECK(s.max_feed_x == 180.0);
    CHECK(s.acceleration == 2500.0);
    CHECK_THAT(s.start_gcode, ContainsSubstring("M190 S[bed_temperature]"));
    CHECK_THAT(s.start_gcode, ContainsSubstring("M420 S1"));
    CHECK_FALSE(needs_own_start_gcode(cube));

    const Profile delta = printer_profile(c.printers[1], 0.4);
    CHECK(delta.settings.flavor == GcodeFlavor::Klipper);
    CHECK(delta.settings.retract_length == 0.8); // direct drive
    REQUIRE(delta.settings.bed.size() == 64);
    for (const auto& q : delta.settings.bed)
        CHECK_THAT(std::hypot(q[0], q[1]), WithinAbs(90.0, 1e-9));

    CHECK(needs_own_start_gcode(c.printers[2]));
    CHECK_THAT(printer_profile(c.printers[2], 0.4).settings.start_gcode, ContainsSubstring("check this G-code"));

    // Only printer settings: it reads back as a printer profile.
    CHECK(parse_profile(profile_file_json(p)).kind == ProfileKind::Printer);

    // The profile prints: the cube on its bed slices to G-code that passes the check.
    for (const Profile& printer : {p, delta}) {
        Settings run = with_profile(Settings{}, ProfileKind::Printer, printer.settings);
        run.gpu = GpuMode::Off;
        IndexedMesh m = weld_exact(read_stl(std::filesystem::path(TISMA_TEST_DATA_DIR) / "cube_ascii.stl").mesh);
        BoundingBox box;
        for (const Vec3f& v : m.vertices) {
            box.min.x = box.empty ? v.x : std::min(box.min.x, v.x);
            box.min.y = box.empty ? v.y : std::min(box.min.y, v.y);
            box.max.x = box.empty ? v.x : std::max(box.max.x, v.x);
            box.max.y = box.empty ? v.y : std::max(box.max.y, v.y);
            box.empty = false;
        }
        // To the bed's middle: the rectangle's, or the origin for the round one.
        const float cx = printer.settings.bed.size() == 4 ? 110.0f : 0.0f;
        for (Vec3f& v : m.vertices)
            v.x += cx - (box.min.x + box.max.x) / 2.0f, v.y += cx - (box.min.y + box.max.y) / 2.0f;
        const SlicedModel model = slice_model(m, run);
        std::ostringstream out;
        write_gcode(out, model, run);
        INFO(printer.name);
        CHECK(check_gcode(out.str(), gcode_limits(model, run)).ok());
    }
}

TEST_CASE("catalog: filament profiles from a data sheet", "[catalog]") {
    const Catalog c = parse_catalog(kPrinters, kFilaments);
    const FilamentModel& pla = *find_filaments(c, "tough pla").front();
    const Profile p = filament_profile(pla);
    CHECK(p.kind == ProfileKind::Filament);
    CHECK(p.name == "Acme Tough PLA");
    CHECK(p.settings.variables.at("temperature") == std::vector<std::string>{"215"}); // middle of 200-230
    CHECK(p.settings.variables.at("bed_temperature") == std::vector<std::string>{"60"});
    CHECK(p.settings.fan_min == Settings{}.fan_min); // no numbers: the material's

    const Profile petg = filament_profile(*find_filaments(c, "petg").front());
    CHECK(petg.settings.variables.at("temperature") == std::vector<std::string>{"245"}); // the recommended one
    CHECK(petg.settings.fan_min == 30.0);
    CHECK(petg.settings.fan_max == 60.0);

    const Profile abs = filament_profile(*find_filaments(c, "bolt abs").front());
    CHECK(abs.settings.fan_max == 20.0);
    CHECK(abs.settings.fan_off_layers == 3);
    CHECK(parse_profile(profile_file_json(abs)).kind == ProfileKind::Filament);
}

TEST_CASE("catalog: the built-in catalog is valid", "[catalog]") {
    const Catalog& c = builtin_catalog();
    for (const PrinterModel& p : c.printers) {
        INFO(p.vendor << " " << p.model);
        CHECK(!p.sources.empty());
        for (const std::string& s : p.sources)
            CHECK(allowed_source(s));
        const Profile profile = printer_profile(p, p.nozzles.front());
        CHECK(profile.settings.max_height > 0.0);
    }
    for (const FilamentModel& f : c.filaments) {
        INFO(f.vendor << " " << f.name);
        CHECK(!f.sources.empty());
        CHECK(!filament_profile(f).settings.variables.at("temperature").front().empty());
    }
}
