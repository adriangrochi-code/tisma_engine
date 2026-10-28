#include "gcode/template.hpp"
#include "gcode/check.hpp"
#include "gcode/export.hpp"
#include "gcode/output.hpp"
#include "gcode/writer.hpp"
#include "mesh/indexed_mesh.hpp"
#include "slice/pipeline.hpp"
#include "test_printer.hpp"
#include "test_temp.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

using namespace tisma;
using Catch::Matchers::ContainsSubstring;

namespace {

// The variables of the spec's tests: 200 C, layers of 0.2, PLA, notes with TESTNOTE.
TemplateContext context() {
    TemplateContext c;
    c.variables["temperature"] = TemplateValue::of(std::vector<TemplateValue>{TemplateValue::of(200LL)});
    c.variables["first_layer_temperature"] = TemplateValue::of(std::vector<TemplateValue>{TemplateValue::of(200LL)});
    c.variables["first_layer_bed_temperature"] = TemplateValue::of(std::vector<TemplateValue>{TemplateValue::of(0LL)});
    c.variables["layer_height"] = TemplateValue::of(0.2);
    c.variables["filament_type"] = TemplateValue::of(std::vector<TemplateValue>{TemplateValue::of(std::string("PLA"))});
    c.variables["printer_notes"] = TemplateValue::of(std::string("hello TESTNOTE"));
    c.variables["layers.height"] = TemplateValue::of(0.2);
    return c;
}

std::string render(const std::string& text) {
    TemplateContext c = context();
    return render_template(text, c, "test");
}

} // namespace

TEST_CASE("templates: [name] and {name} (gcode_templates.md, rule 1, T1)", "[template]") {
    CHECK(render("[temperature] [first_layer_temperature] [layer_height]") == "200 200 0.2");
    CHECK(render("{temperature[0]} {first_layer_temperature[0]} {layer_height}") == "200 200 0.2");
    CHECK(render("{first_layer_bed_temperature[0]}") == "0");
    CHECK(render("{layers.height * 2}") == "0.4"); // the engine's settings by their names
    CHECK(render("M104 S[temperature] ; [not a variable") == "M104 S200 ; [not a variable");
}

TEST_CASE("templates: arithmetic, texts and logic (rules 2 and 3, T2, T3)", "[template]") {
    CHECK(render("{temperature[0] + 5} {layer_height * 2} {10 / 4} {10 % 4} {2 - 7}") == "205 0.4 2 2 -5");
    CHECK(render("{10.0 / 4} {-3 * -2} {(1 + 2) * 3}") == "2.5 6 9");
    CHECK(render("{if temperature[0] > 190}hot{elsif temperature[0] > 100}warm{else}cold{endif}") == "hot");
    CHECK(render("{(temperature[0] > 190 ? \"H\" : \"C\")}") == "H");
    CHECK(render("{true and not false} {false or false} {1 < 2 and 2 <= 2}") == "true false true");
    CHECK(render("{if printer_notes =~ /.*TESTNOTE.*/}match{else}no{endif}") == "match");
    CHECK(render("{printer_notes !~ /XYZ/}") == "true");
    CHECK(render("{\"abc\" + \"def\"} {filament_type[0]}") == "abcdef PLA");
    // A list of one value is that value in sums and comparisons.
    CHECK(render("{temperature + 5} {if temperature > 190}hot{endif}") == "205 hot");
    CHECK(render("{\"a\" == \"a\"} {\"a\" != \"b\"} {\"abc\" < \"abd\"}") == "true true true");
    // Nested conditions: only the kept branch is evaluated.
    CHECK(render("{if false}{nope}{else}{if true}x{endif}{endif}") == "x");
}

TEST_CASE("templates: functions (rule 5, T4, T4b, T5)", "[template]") {
    CHECK(render("{min(3, 5)} {max(3, 5)}") == "3 5");
    CHECK(render("{round(3.5)} {round(2.5)} {int(3.7)} {int(-3.7)}") == "4 3 3 -3");
    CHECK(render("{digits(3.14159, 6, 2)}|{zdigits(3.14159, 6, 2)}|{digits(42, 3)}|{zdigits(-5, 4)}") ==
          "  3.14|003.14| 42|-005");
    CHECK(render("{is_nil(temperature[0])} {one_of(filament_type[0], \"ABS\", \"PLA\")}") == "false true");
    CHECK(render("{size(temperature)} {empty(temperature)}") == "1 false");
    CHECK(render("{interpolate_table(15, (0, 0), (10, 100), (20, 300))}") == "200");
    CHECK(render("{interpolate_table(-5, (0, 0), (10, 100))} {interpolate_table(50, (0, 0), (10, 100))}") == "0 100");
}

TEST_CASE("templates: local and global variables (rule 6, T5)", "[template]") {
    TemplateContext c = context();
    CHECK(render_template("{local a = 5}{a * 2}", c, "a") == "10");
    CHECK(render_template("{local l = (1, 2, 3)}{l[1]} {size(l)}", c, "a") == "2 3");
    CHECK(render_template("{global g = 7}{g}", c, "start") == "7");
    CHECK(render_template("{g}", c, "end") == "7"); // the globals reach the templates after
    // A local does not.
    CHECK_THROWS_WITH(render_template("{a}", c, "end"), ContainsSubstring("unknown variable 'a'"));
}

TEST_CASE("templates: errors name the section, the line and the column (rule 8, T7)", "[template]") {
    TemplateContext c = context();
    try {
        render_template("G28\nM104 S{temperature[0]}\nM109 S{tempertaure[0]}\n", c, "machine.start_gcode");
        FAIL("no error");
    } catch (const TemplateError& e) {
        const std::string m = e.what();
        CHECK_THAT(m, ContainsSubstring("machine.start_gcode, line 3: unknown variable 'tempertaure'"));
        CHECK_THAT(m, ContainsSubstring("\nM109 S{tempertaure[0]}\n       ^"));
    }
    CHECK_THROWS_WITH(render("{empty(layer_height)}"), ContainsSubstring("parameter of empty() is not a list"));
    CHECK_THROWS_WITH(render("[print_time]"), ContainsSubstring("unknown variable 'print_time'"));
    CHECK_THROWS_WITH(render("{if true}x"), ContainsSubstring("{if} without its {endif}"));
    CHECK_THROWS_WITH(render("{endif}"), ContainsSubstring("{endif} without its {if}"));
    CHECK_THROWS_WITH(render("{1 +}"), ContainsSubstring("a value is expected"));
    CHECK_THROWS_WITH(render("{foo(1)}"), ContainsSubstring("unknown function 'foo'"));
    CHECK_THROWS_WITH(render("{temperature[3]}"), ContainsSubstring("index 3 is out of 'temperature'"));
    CHECK_THROWS_WITH(render("{1 / 0}"), ContainsSubstring("division by zero"));
    CHECK_THROWS_WITH(render("{ {"), ContainsSubstring("a { without its }"));
    CHECK_THROWS_WITH(render("{if 1 +}{endif}"), ContainsSubstring("a value is expected"));
}

TEST_CASE("templates: values from the settings' text", "[template]") {
    CHECK(TemplateValue::parse("215").kind == TemplateValue::Kind::Int);
    CHECK(TemplateValue::parse("0.2").kind == TemplateValue::Kind::Real);
    CHECK(TemplateValue::parse("true").kind == TemplateValue::Kind::Bool);
    CHECK(TemplateValue::parse("\"gyroid\"").text == "gyroid");
    CHECK(TemplateValue::parse("60%").text == "60%");
    CHECK(template_text(TemplateValue::of(0.1 + 0.2)) == "0.30000000000000004");
    CHECK(template_text(TemplateValue::of(-0.0)) == "0");
}

namespace {

// The cube of the spec's tests, 20 x 20 x 10 mm, centred on (100, 100).
IndexedMesh cube() {
    IndexedMesh m;
    for (float z : {0.0f, 10.0f})
        for (const auto& p : {std::array<float, 2>{90, 90}, {110, 90}, {110, 110}, {90, 110}})
            m.vertices.push_back({p[0], p[1], z});
    for (const auto& f : std::vector<std::array<std::uint32_t, 4>>{{0, 3, 2, 1}, {4, 5, 6, 7}, {0, 1, 5, 4}, {1, 2, 6, 5}, {2, 3, 7, 6}, {3, 0, 4, 7}}) {
        m.triangles.push_back({f[0], f[1], f[2]});
        m.triangles.push_back({f[0], f[2], f[3]});
    }
    return m;
}

Settings printer() {
    Settings s = test_printer();
    s.variables["temperature"] = {"200"};
    s.variables["bed_temperature"] = {"60"};
    s.variables["filament_type"] = {"PLA"};
    s.input_name = "cube20";
    return s;
}

std::string gcode_of(const Settings& s) {
    const SlicedModel model = slice_model(cube(), s);
    std::ostringstream out;
    write_gcode(out, model, s);
    CHECK(check_gcode(out.str(), gcode_limits(model, s)).ok());
    return out.str();
}

std::size_t at(const std::string& g, const std::string& line) {
    const std::size_t i = g.find("\n" + line + "\n");
    return i == std::string::npos ? i : i + 1;
}

} // namespace

TEST_CASE("templates in the G-code: the print's variables (rule 7, T6)", "[template][gcode]") {
    Settings s = printer();
    s.start_gcode = ";T6 {total_layer_count} {input_filename_base} {num_extruders} {first_layer_print_min[0]} "
                    "{first_layer_print_max[0]} {first_layer_print_size[0]} {initial_extruder} {initial_tool} "
                    "{is_extruder_used[0]} {year > 2000} {layers.height}";
    const std::string g = gcode_of(s);
    CHECK(at(g, ";T6 50 cube20 1 90 110 20 0 0 true true 0.2") != std::string::npos);
}

TEST_CASE("templates in the G-code: the profiles' own parameters (gcode_templates.md; presets.md, rules 21 and 22)",
          "[template][gcode]") {
    const Settings own = parse_settings(R"({"machine": {"custom_parameters": {"bed_probe": true, "zone": "A \"1\""}},
                                            "material": {"custom_parameters": {"soak": 90}},
                                            "print": {"custom_parameters": {"mode": 2}, "variables": {"tag": "fast"}}})");
    Settings s = printer();
    s.printer_parameters = own.printer_parameters, s.filament_parameters = own.filament_parameters;
    s.print_parameters = own.print_parameters, s.print_variables = own.print_variables;
    s.start_gcode = ";OWN {custom_parameter_printer_bed_probe} {custom_parameter_printer_zone} "
                    "{custom_parameter_filament_soak[0] + 1} {custom_parameter_print_mode * 2} {print_tag}";
    CHECK(at(gcode_of(s), ";OWN true A \"1\" 91 4 fast") != std::string::npos);
    // Written out and read back the same.
    const Settings back = parse_settings(settings_json(s));
    CHECK(back.printer_parameters == s.printer_parameters);
    CHECK(back.filament_parameters == s.filament_parameters);
    CHECK(back.print_parameters == s.print_parameters);
    CHECK(back.print_variables == s.print_variables);
    // Only flat values (rule 22).
    CHECK_THROWS_WITH(parse_settings(R"({"print": {"custom_parameters": {"a": [1, 2]}}})"),
                      Catch::Matchers::ContainsSubstring("unable to parse custom parameters"));
    CHECK_THROWS(parse_settings(R"({"machine": {"custom_parameters": 3}})"));
}

TEST_CASE("templates in the G-code: the custom template at a height (rule 13)", "[template][gcode]") {
    Settings s = printer();
    s.gcode_custom_template = "M117 at {layer_z}";
    HeightChange c;
    c.z = 2.0;
    c.custom = true;
    s.changes.push_back(c);
    const std::string g = gcode_of(s);
    const std::size_t mark = g.find(";CUSTOM_GCODE\nM117 at 2.2\n");
    CHECK(mark != std::string::npos);
    CHECK(mark > g.find(";Z:2.2"));
    CHECK(g.find(";CUSTOM_GCODE", mark + 1) == std::string::npos);
    // Written out and read back.
    const Settings back = parse_settings(settings_json(s));
    REQUIRE(back.changes.size() == 1);
    CHECK(back.changes.front().custom);
    CHECK(back.gcode_custom_template == s.gcode_custom_template);
}

TEST_CASE("templates in the G-code: the layer change and the end (rule 11, T8)", "[template][gcode]") {
    Settings s = printer();
    s.gcode_before_layer = ";BEFORE {layer_num} {layer_z}";
    s.gcode_after_layer = ";AFTER {layer_num} {layer_z} {max_layer_z}";
    s.end_gcode = ";END {total_layer_count} {layer_num}";
    const std::string g = gcode_of(s);
    CHECK(at(g, ";BEFORE 1 0.4") != std::string::npos);
    CHECK(at(g, ";AFTER 1 0.4 0.4") != std::string::npos);
    CHECK(at(g, ";BEFORE 0 0.2") == std::string::npos);
    CHECK(at(g, ";END 50 49") != std::string::npos);
}

TEST_CASE("templates in the G-code: the printer's and the filament's start and end (rules 9 and 12, T9)",
          "[template][gcode]") {
    Settings s = printer();
    s.start_gcode = ";PRINTER START";
    s.filament_start_gcode = ";FILAMENT START {filament_type[0]} {layer_height}";
    s.filament_end_gcode = ";FILAMENT END";
    s.end_gcode = ";PRINTER END";
    const std::string g = gcode_of(s);
    const std::vector<std::string> order = {"M190 S60", "M104 S200", ";PRINTER START", "M109 S200",
                                            ";FILAMENT START PLA 0.2", ";LAYER_CHANGE", ";FILAMENT END", ";PRINTER END"};
    std::size_t last = 0;
    for (const std::string& line : order) {
        INFO(line);
        const std::size_t i = at(g, line);
        REQUIRE(i != std::string::npos);
        CHECK(i >= last);
        last = i;
    }
}

TEST_CASE("templates in the G-code: automatic temperatures (rule 10, T10)", "[template][gcode]") {
    Settings s = printer();
    s.start_gcode = "M190 S[bed_temperature]\nM109 S{temperature[0]}";
    std::string g = gcode_of(s);
    CHECK(g.find("M104 S200") == std::string::npos); // only the user's
    CHECK(at(g, "M190 S60") != std::string::npos);
    s.start_gcode = "G28";
    g = gcode_of(s);
    CHECK(g.find("\nM190 S60\nM104 S200\nG28\nM109 S200\n") != std::string::npos);
    s.auto_temperatures = false;
    g = gcode_of(s);
    CHECK(g.find("M190") == std::string::npos);
    CHECK(g.find("M104 S200") == std::string::npos);
    CHECK(g.find("M109") == std::string::npos);
}

TEST_CASE("templates in the G-code: an error fails the export (rule 8, T7)", "[template][gcode]") {
    Settings s = printer();
    s.start_gcode = "G28\nM104 S{print_time}";
    const SlicedModel model = slice_model(cube(), s);
    std::ostringstream out;
    CHECK_THROWS_WITH(write_gcode(out, model, s), ContainsSubstring("machine.start_gcode, line 2: unknown variable 'print_time'"));
    const std::filesystem::path path = test_temp_dir() / "tisma_template_error.gcode";
    std::filesystem::remove(path);
    const GcodeExport exported = export_gcode(path, model, s);
    CHECK_FALSE(exported.written);
    CHECK_THAT(exported.error, ContainsSubstring("unknown variable 'print_time'"));
    CHECK_FALSE(std::filesystem::exists(path));
    // A template in a layer change names its section.
    s.start_gcode = "G28";
    s.gcode_after_layer = "{if layer_num > }x{endif}";
    CHECK_THROWS_WITH(write_gcode(out, model, s), ContainsSubstring("gcode.after_layer, line 1"));
}

TEST_CASE("templates: the output file's name, before and after the G-code (rule 15, T11)", "[template]") {
    Settings s = printer();
    s.infill_density = 15.0;
    s.filename_format =
        "{input_filename_base}_{layer_height}mm_[fill_density]_{filament_type[0]}_{print_time}_{total_weight}.{default_output_extension}";
    const SlicedModel model = slice_model(cube(), s);
    CHECK(output_filename(s, model, std::nullopt) == "cube20_0.2mm_15%_PLA_{print_time}_{total_weight}..gcode");
    CHECK(output_filename(s, model, GcodeTotals{684.0, 0.0}) == "cube20_0.2mm_15%_PLA_11m_0..gcode");
    CHECK(output_filename(s, model, GcodeTotals{4056.0, 12.144}) == "cube20_0.2mm_15%_PLA_1h7m_12.14..gcode");
    s.filename_format = "[input_filename_base].gcode";
    CHECK(output_filename(s, model, std::nullopt) == "cube20.gcode");
    // The totals from a written G-code.
    const std::filesystem::path path = test_temp_dir() / "tisma_totals.gcode";
    s.filament_density = 1.24;
    const GcodeExport exported = export_gcode(path, model, s);
    REQUIRE(exported.written);
    const std::optional<GcodeTotals> totals = read_gcode_totals(path);
    REQUIRE(totals);
    CHECK(totals->print_seconds > 60.0);
    CHECK(totals->grams > 1.0);
    std::filesystem::remove(path);
}
