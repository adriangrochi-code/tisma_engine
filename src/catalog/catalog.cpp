// The printer and filament catalog (docs/CATALOG.md), on nlohmann/json (MIT).
// Own design; the data are manufacturers' published facts, each with its
// source. See PROVENANCE.md.

#include "catalog/catalog.hpp"

#include "catalog/catalog_files.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <set>

namespace tisma {
namespace {

using nlohmann::json;

[[noreturn]] void fail(const std::string& where, const std::string& message) {
    throw SettingsError("catalog: " + where + ": " + message);
}

std::string lower(std::string_view text) {
    std::string out(text);
    for (char& c : out)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

// Readers of one entry's keys; `where` names the entry in messages.
struct Reader {
    const json& j;
    std::string where;

    std::string text(const char* key, bool required = true) const {
        if (!j.contains(key) || j.at(key).is_null()) {
            if (required)
                fail(where, std::string(key) + " is missing");
            return {};
        }
        if (!j.at(key).is_string() || (required && j.at(key).get<std::string>().empty()))
            fail(where, std::string(key) + " must be a text");
        return j.at(key).get<std::string>();
    }
    std::optional<double> number(const char* key, double lo, double hi) const {
        if (!j.contains(key) || j.at(key).is_null())
            return std::nullopt;
        if (!j.at(key).is_number())
            fail(where, std::string(key) + " must be a number");
        const double v = j.at(key).get<double>();
        if (!(v >= lo && v <= hi))
            fail(where, std::string(key) + " is out of " + std::to_string(int(lo)) + " to " + std::to_string(int(hi)));
        return v;
    }
    std::optional<std::array<double, 2>> range(const char* key, double lo, double hi) const {
        if (!j.contains(key) || j.at(key).is_null())
            return std::nullopt;
        const json& r = j.at(key);
        if (!r.is_array() || r.size() != 2 || !r[0].is_number() || !r[1].is_number())
            fail(where, std::string(key) + " must be [min, max]");
        const double a = r[0].get<double>(), b = r[1].get<double>();
        if (!(a >= lo && b <= hi && a <= b))
            fail(where, std::string(key) + " must go up, within " + std::to_string(int(lo)) + " to " +
                            std::to_string(int(hi)));
        return std::array<double, 2>{a, b};
    }
    std::vector<std::string> texts(const char* key) const {
        std::vector<std::string> out;
        if (!j.contains(key) || j.at(key).is_null())
            return out;
        if (!j.at(key).is_array())
            fail(where, std::string(key) + " must be a list of texts");
        for (const json& t : j.at(key)) {
            if (!t.is_string())
                fail(where, std::string(key) + " must be a list of texts");
            out.push_back(t.get<std::string>());
        }
        return out;
    }
    std::optional<int> count(const char* key, int lo, int hi) const {
        const std::optional<double> v = number(key, lo, hi);
        if (v && *v != std::floor(*v))
            fail(where, std::string(key) + " must be a whole number");
        return v ? std::optional<int>(int(*v)) : std::nullopt;
    }
    std::optional<bool> flag(const char* key) const {
        if (!j.contains(key) || j.at(key).is_null())
            return std::nullopt;
        if (!j.at(key).is_boolean())
            fail(where, std::string(key) + " must be true or false");
        return j.at(key).get<bool>();
    }
    std::vector<std::string> sources() const {
        if (!j.contains("sources") || !j.at("sources").is_array() || j.at("sources").empty())
            fail(where, "sources: at least one official page");
        std::vector<std::string> out;
        for (const json& s : j.at("sources")) {
            if (!s.is_string() || !s.get<std::string>().starts_with("http"))
                fail(where, "sources: each must be an address");
            if (!allowed_source(s.get<std::string>()))
                fail(where, "sources: " + s.get<std::string>() + " is not allowed (docs/CATALOG.md)");
            out.push_back(s.get<std::string>());
        }
        return out;
    }
};

json parse_array(std::string_view text, const char* what) {
    json root;
    try {
        root = json::parse(text.begin(), text.end());
    } catch (const json::parse_error& e) {
        fail(what, std::string("not valid JSON: ") + e.what());
    }
    if (!root.is_array())
        fail(what, "must be a list");
    return root;
}

PrinterModel read_printer(const json& j, std::size_t index) {
    if (!j.is_object())
        fail("printers[" + std::to_string(index) + "]", "must be an object");
    Reader r{j, "printers[" + std::to_string(index) + "]"};
    PrinterModel p;
    p.vendor = r.text("vendor");
    p.model = r.text("model");
    r.where = p.vendor + " " + p.model;
    const json& v = j.contains("build_volume") ? j.at("build_volume") : json();
    if (!v.is_array() || v.size() != 3)
        fail(r.where, "build_volume must be [x, y, z]");
    for (std::size_t k = 0; k < 3; ++k) {
        if (!v[k].is_number() || !(v[k].get<double>() >= 30.0 && v[k].get<double>() <= 3000.0))
            fail(r.where, "build_volume must be from 30 to 3000 mm");
        p.build_volume[k] = v[k].get<double>();
    }
    const std::string shape = r.text("bed_shape", false);
    if (!shape.empty() && shape != "rectangular" && shape != "circular")
        fail(r.where, "bed_shape must be rectangular or circular");
    p.circular = shape == "circular";
    // A maker that does not state the nozzle: the common 0.4 mm (docs/CATALOG.md).
    const json nozzles = j.contains("nozzle_diameters") && !j.at("nozzle_diameters").is_null() ? j.at("nozzle_diameters")
                                                                                                : json::array({0.4});
    if (!nozzles.is_array() || nozzles.empty())
        fail(r.where, "nozzle_diameters must list at least the stock nozzle");
    for (const json& n : nozzles) {
        if (!n.is_number() || !(n.get<double>() >= 0.1 && n.get<double>() <= 2.0))
            fail(r.where, "nozzle_diameters must be from 0.1 to 2 mm");
        if (std::find(p.nozzles.begin(), p.nozzles.end(), n.get<double>()) == p.nozzles.end())
            p.nozzles.push_back(n.get<double>());
    }
    p.filament_diameter = r.number("filament_diameter", 1.0, 3.5).value_or(1.75);
    p.firmware = r.text("firmware", false); // empty: the maker does not say
    if (!p.firmware.empty() && p.firmware != "marlin" && p.firmware != "klipper" && p.firmware != "reprap" &&
        p.firmware != "bambu" && p.firmware != "other")
        fail(r.where, "firmware must be marlin, klipper, reprap, bambu or other");
    const std::string extruder = r.text("extruder", false);
    if (!extruder.empty() && extruder != "direct" && extruder != "bowden")
        fail(r.where, "extruder must be direct or bowden");
    p.direct_drive = extruder != "bowden";
    p.heated_bed = r.flag("heated_bed").value_or(true);
    p.max_bed_temp = r.number("max_bed_temp", 0.0, 200.0);
    p.max_nozzle_temp = r.number("max_nozzle_temp", 150.0, 600.0);
    p.max_speed = r.number("max_speed", 10.0, 2000.0);
    p.max_acceleration = r.number("max_acceleration", 100.0, 100000.0);
    p.auto_leveling = r.flag("auto_leveling");
    p.enclosed = r.flag("enclosed").value_or(false);
    p.multi_material = r.text("multi_material", false);
    p.kinematics = r.text("kinematics", false);
    p.max_flow = r.number("max_flow", 1.0, 1000.0);
    p.chamber_heating = r.number("chamber_heating", 20.0, 200.0);
    p.hardened_nozzle = r.flag("hardened_nozzle");
    p.hotend = r.text("hotend", false);
    p.probe = r.text("probe", false);
    p.bed_surfaces = r.texts("bed_surfaces");
    p.connectivity = r.texts("connectivity");
    p.features = r.texts("features");
    p.material_slots = r.count("material_slots", 1, 64);
    p.tools = r.count("tools", 1, 16);
    p.sources = r.sources();
    return p;
}

FilamentModel read_filament(const json& j, std::size_t index) {
    if (!j.is_object())
        fail("filaments[" + std::to_string(index) + "]", "must be an object");
    Reader r{j, "filaments[" + std::to_string(index) + "]"};
    FilamentModel f;
    f.vendor = r.text("vendor");
    f.name = r.text("name");
    r.where = f.vendor + " " + f.name;
    f.material = r.text("material");
    f.diameter = r.number("diameter", 1.0, 3.5).value_or(1.75);
    const std::string feedstock = r.text("feedstock", false);
    if (feedstock == "pellet")
        f.feedstock = Feedstock::Pellet;
    else if (!feedstock.empty() && feedstock != "filament")
        fail(r.where, "feedstock must be \"filament\" or \"pellet\"");
    f.density = r.number("density", 0.5, 3.0);
    f.nozzle_temp = r.range("nozzle_temp", 150.0, 500.0);
    f.nozzle_recommended = r.number("nozzle_temp_recommended", 150.0, 500.0);
    f.bed_temp = r.range("bed_temp", 0.0, 200.0);
    f.bed_recommended = r.number("bed_temp_recommended", 0.0, 200.0);
    f.fan = r.range("fan", 0.0, 100.0);
    f.print_speed = r.range("print_speed", 1.0, 2000.0);
    f.enclosure = r.flag("enclosure");
    f.max_flow = r.number("max_flow", 1.0, 1000.0);
    f.chamber_temp = r.range("chamber_temp", 0.0, 200.0);
    f.drying_temp = r.number("drying_temp", 20.0, 200.0);
    f.drying_hours = r.number("drying_hours", 0.5, 96.0);
    f.abrasive = r.flag("abrasive");
    f.soluble = r.flag("soluble");
    f.hardness = r.text("hardness", false);
    f.glass_transition = r.number("glass_transition", -100.0, 400.0);
    f.heat_deflection = r.number("heat_deflection", 0.0, 400.0);
    f.shrinkage = r.number("shrinkage", 0.0, 10.0);
    f.bed_surfaces = r.texts("bed_surfaces");
    if (!f.nozzle_temp && !f.nozzle_recommended)
        fail(r.where, "needs nozzle_temp or nozzle_temp_recommended");
    f.sources = r.sources();
    return f;
}

// Words of the query, all in the text (any case).
bool matches(std::string_view text, std::string_view query) {
    const std::string haystack = lower(text);
    std::size_t at = 0;
    const std::string q = lower(query);
    while (at < q.size()) {
        while (at < q.size() && q[at] == ' ')
            ++at;
        std::size_t end = q.find(' ', at);
        if (end == std::string::npos)
            end = q.size();
        if (end > at && haystack.find(q.substr(at, end - at)) == std::string::npos)
            return false;
        at = end;
    }
    return true;
}

std::string number_text(double v) {
    char text[32];
    std::snprintf(text, sizeof text, "%g", v);
    return text;
}

double round5(double v) { return std::round(v / 5.0) * 5.0; }

} // namespace

bool allowed_source(std::string_view url) {
    const std::string u = lower(url);
    for (const char* banned : {"github.com", "githubusercontent.com", "gitlab.com", "bitbucket.org", "orcaslicer",
                               "prusaslicer", "superslicer", "slic3r", "bambustudio", "bambu-studio",
                               "curaengine", "/cura/", "cura.", "creality-print", "crealityprint", "elegooslicer",
                               "anycubicslicer", "flashprint"})
        if (u.find(banned) != std::string::npos)
            return false;
    return true;
}

Catalog parse_catalog(std::string_view printers_json, std::string_view filaments_json) {
    Catalog c;
    const json printers = parse_array(printers_json, "printers");
    for (std::size_t i = 0; i < printers.size(); ++i)
        c.printers.push_back(read_printer(printers[i], i));
    const json filaments = parse_array(filaments_json, "filaments");
    for (std::size_t i = 0; i < filaments.size(); ++i)
        c.filaments.push_back(read_filament(filaments[i], i));
    const auto by_name = [](const std::string& va, const std::string& a, const std::string& vb, const std::string& b) {
        const std::string x = lower(va), y = lower(vb);
        return x != y ? x < y : lower(a) < lower(b);
    };
    std::sort(c.printers.begin(), c.printers.end(),
              [&](const PrinterModel& a, const PrinterModel& b) { return by_name(a.vendor, a.model, b.vendor, b.model); });
    std::sort(c.filaments.begin(), c.filaments.end(), [&](const FilamentModel& a, const FilamentModel& b) {
        return by_name(a.vendor, a.name, b.vendor, b.name);
    });
    std::set<std::string> seen;
    for (const PrinterModel& p : c.printers)
        if (!seen.insert(lower(p.vendor + "\n" + p.model)).second)
            fail(p.vendor + " " + p.model, "appears twice");
    seen.clear();
    for (const FilamentModel& f : c.filaments)
        if (!seen.insert(lower(f.vendor + "\n" + f.name + "\n" + number_text(f.diameter))).second)
            fail(f.vendor + " " + f.name, "appears twice");
    return c;
}

const Catalog& builtin_catalog() {
    static const Catalog catalog = parse_catalog(catalog_file("printers.json"), catalog_file("filaments.json"));
    return catalog;
}

std::vector<const PrinterModel*> find_printers(const Catalog& catalog, std::string_view query) {
    std::vector<const PrinterModel*> out;
    for (const PrinterModel& p : catalog.printers)
        if (matches(p.vendor + " " + p.model, query))
            out.push_back(&p);
    return out;
}

std::vector<const FilamentModel*> find_filaments(const Catalog& catalog, std::string_view query) {
    std::vector<const FilamentModel*> out;
    for (const FilamentModel& f : catalog.filaments)
        if (matches(f.vendor + " " + f.name + " " + f.material, query))
            out.push_back(&f);
    return out;
}

namespace {

bool offered(Feedstock material, double diameter, Feedstock feedstock, const std::vector<double>& diameters) {
    if (material != feedstock)
        return false;
    if (material == Feedstock::Pellet || diameters.empty())
        return true;
    return std::any_of(diameters.begin(), diameters.end(), [&](double d) { return std::abs(d - diameter) < 0.01; });
}

} // namespace

bool filament_offered(const FilamentModel& f, Feedstock feedstock, const std::vector<double>& diameters) {
    return offered(f.feedstock, f.diameter, feedstock, diameters);
}

bool material_offered(const Settings& m, Feedstock feedstock, const std::vector<double>& diameters) {
    return offered(m.material_feedstock, m.filament_diameter, feedstock, diameters);
}

const PrinterModel* printer_of_profile(const Catalog& catalog, std::string_view name) {
    for (const PrinterModel& p : catalog.printers)
        for (const double nozzle : p.nozzles)
            if (printer_profile_name(p, nozzle) == name)
                return &p;
    return nullptr;
}

std::string printer_profile_name(const PrinterModel& p, double nozzle) {
    return p.vendor + " " + p.model + " " + number_text(nozzle) + " mm";
}

std::string filament_profile_name(const FilamentModel& f) {
    std::string name = f.vendor + " " + f.name;
    if (f.diameter != 1.75)
        name += " " + number_text(f.diameter) + " mm";
    return name;
}

bool is_abrasive(const FilamentModel& f) {
    if (f.abrasive)
        return *f.abrasive;
    const std::string m = lower(f.material);
    return m.ends_with("-cf") || m.ends_with("-gf") || m.find("-cf") != std::string::npos ||
           m.find("-gf") != std::string::npos;
}

bool needs_own_start_gcode(const PrinterModel& p) { return p.firmware == "bambu" || p.firmware == "other"; }

Profile printer_profile(const PrinterModel& p, double nozzle) {
    Profile out;
    out.kind = ProfileKind::Printer;
    out.name = printer_profile_name(p, nozzle);
    Settings& s = out.settings;
    const double x = p.build_volume[0], y = p.build_volume[1], z = p.build_volume[2];
    s.bed.clear();
    if (p.circular) {
        constexpr int kSides = 64;
        for (int i = 0; i < kSides; ++i) {
            const double a = 2.0 * std::numbers::pi * i / kSides;
            s.bed.push_back({x / 2.0 * std::cos(a), x / 2.0 * std::sin(a)});
        }
    } else {
        s.bed = {{0.0, 0.0}, {x, 0.0}, {x, y}, {0.0, y}};
    }
    s.max_height = z;
    s.nozzle_diameter = nozzle;
    s.flavor = p.firmware == "klipper" ? GcodeFlavor::Klipper
               : p.firmware == "reprap" ? GcodeFlavor::RepRap
                                        : GcodeFlavor::Marlin;
    // Retraction: short for a direct drive, long for a Bowden tube (own
    // starting lengths; the retraction towers find the printer's own).
    s.retract_length = p.direct_drive ? 0.8 : 4.0;
    if (p.max_speed) {
        s.max_feed_x = s.max_feed_y = *p.max_speed;
    }
    // The speeds: the catalog knows none of a model's but its top speed, so
    // the profile takes a stock Ender-3's (owner's decision, 2026-10-26),
    // none past the model's top speed.
    fill_missing_speeds(s);
    if (p.max_speed)
        for (double* v : {&s.wall_speed, &s.outer_wall_speed.value, &s.infill_speed, &s.solid_infill_speed,
                          &s.top_solid_infill_speed, &s.support_speed, &s.travel_speed, &s.first_layer_speed})
            *v = std::min(*v, *p.max_speed);
    if (p.max_acceleration)
        s.acceleration = s.travel_acceleration = *p.max_acceleration;
    // What it can do: the writer keeps to it, and the checks warn against it.
    s.machine_max_flow = p.max_flow.value_or(0.0);
    s.max_nozzle_temp = p.max_nozzle_temp.value_or(0.0);
    s.max_bed_temp = p.heated_bed ? p.max_bed_temp.value_or(0.0) : 0.0;
    s.max_chamber_temp = p.chamber_heating.value_or(0.0);
    s.enclosed = p.enclosed;
    s.hardened_nozzle = p.hardened_nozzle.value_or(false);

    // The engine's own start and end G-code, in plain commands every flavor
    // takes. A round bed has its origin in the middle.
    const double px = p.circular ? -x * 0.3 : 3.0;
    const double py0 = p.circular ? -x * 0.3 : 10.0, py1 = p.circular ? x * 0.3 : std::min(y - 10.0, 120.0);
    char prime[256];
    std::snprintf(prime, sizeof prime,
                  "G1 Z2 F600\n"
                  "G1 X%.1f Y%.1f F6000 ; to the priming line\n"
                  "G1 Z0.3 F600\n"
                  "G1 X%.1f Y%.1f E%.2f F1200 ; prime the nozzle\n"
                  "G1 Z2 F600\n"
                  "G92 E0\n",
                  px, py0, px, py1, (py1 - py0) * 0.04);
    std::string start = "; Tisma engine: " + p.vendor + " " + p.model + "\n";
    if (needs_own_start_gcode(p))
        start += "; This printer's firmware has its own start sequence: check this G-code before printing\n";
    if (p.heated_bed)
        start += "M140 S[bed_temperature] ; heat the bed\n";
    start += "M104 S[temperature] ; heat the nozzle\n"
             "G28 ; home\n"
             "G90\n";
    if (p.auto_leveling.value_or(false) && s.flavor == GcodeFlavor::Marlin && p.firmware == "marlin")
        start += "M420 S1 ; the stored bed mesh\n";
    if (p.heated_bed)
        start += "M190 S[bed_temperature] ; wait for the bed\n";
    start += "M109 S[temperature] ; wait for the nozzle\n"
             "G92 E0\n";
    start += prime;
    s.start_gcode = start;
    char end[256];
    std::snprintf(end, sizeof end,
                  "M104 S0 ; nozzle off\n"
                  "M140 S0 ; bed off\n"
                  "M107 ; fan off\n"
                  "G91\n"
                  "G1 Z5 F600 ; lift\n"
                  "G90\n"
                  "G1 X%.1f Y%.1f F6000 ; present the print\n"
                  "M84 ; motors off\n",
                  p.circular ? 0.0 : 5.0, p.circular ? 0.0 : std::max(5.0, y - 10.0));
    s.end_gcode = end;
    return out;
}

Profile filament_profile(const FilamentModel& f) {
    Profile out;
    out.kind = ProfileKind::Filament;
    out.name = filament_profile_name(f);
    Settings& s = out.settings;
    s.filament_diameter = f.diameter;
    s.material_feedstock = f.feedstock;
    const auto middle = [](const std::optional<std::array<double, 2>>& r) {
        return r ? std::optional<double>(round5(((*r)[0] + (*r)[1]) / 2.0)) : std::nullopt;
    };
    const std::optional<double> nozzle = f.nozzle_recommended ? f.nozzle_recommended : middle(f.nozzle_temp);
    const std::optional<double> bed = f.bed_recommended ? f.bed_recommended : middle(f.bed_temp);
    s.variables["temperature"] = {number_text(nozzle.value_or(210.0))};
    s.variables["bed_temperature"] = {number_text(bed.value_or(60.0))};
    s.filament_max_flow = f.max_flow.value_or(0.0);
    s.chamber_temperature = middle(f.chamber_temp).value_or(0.0);
    s.abrasive = is_abrasive(f);
    s.needs_enclosure = f.enclosure.value_or(false);
    // The fan: the data sheet's numbers, else the material's (src/ui/presets.*
    // uses the same).
    const std::string m = lower(f.material);
    const bool hot = m.starts_with("abs") || m.starts_with("asa") || m.starts_with("pa") || m.starts_with("pc") ||
                     m.starts_with("nylon") || m.starts_with("hips");
    if (f.fan) {
        s.fan_min = (*f.fan)[0], s.fan_max = (*f.fan)[1];
    } else if (hot) {
        s.fan_min = 0.0, s.fan_max = 20.0, s.fan_off_layers = 3;
    } else if (m.starts_with("petg") || m.starts_with("pet") || m.starts_with("cpe")) {
        s.fan_min = 20.0, s.fan_max = 50.0;
    } else if (m.starts_with("tpu") || m.starts_with("tpe") || m.starts_with("flex")) {
        s.fan_min = 50.0, s.fan_max = 100.0;
    }
    return out;
}

} // namespace tisma
