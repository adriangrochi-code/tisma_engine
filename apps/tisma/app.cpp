// The interface: window, panels, 3D view and the actions behind the buttons.
// Own design; see PROVENANCE.md.

#include "app.hpp"

#include "embedded.hpp"
#include "file_browser.hpp"
#include "gl.hpp"
#include "renderer.hpp"
#include "vk_view.hpp"
#include "vk_window.hpp"

#include "calibration/calibration.hpp"
#include "catalog/catalog.hpp"
#include "config/analysis_case.hpp"
#include "config/profiles.hpp"
#include "config/settings.hpp"
#include "device/gcode_host.hpp"
#include "device/serial_port.hpp"
#include "device/simulator.hpp"
#include "fea/analysis.hpp"
#include "gcode/binary.hpp"
#include "gcode/export.hpp"
#include "resin/goo.hpp"
#include "gcode/print_time.hpp"
#include "gcode/writer.hpp"
#include "io/model.hpp"
#include "io/stl.hpp"
#include "project/project.hpp"
#include "ui/camera.hpp"
#include "ui/credits.hpp"
#include "ui/files.hpp"
#include "ui/icons.hpp"
#include "ui/help.hpp"
#include "ui/plate.hpp"
#include "ui/png.hpp"
#include "ui/presets.hpp"
#include "ui/raytrace.hpp"
#include "ui/scene.hpp"
#include "ui/settings_layout.hpp"
#include "ui/settings_view.hpp"
#include "ui/printer_card.hpp"
#include "ui/measure.hpp"
#include "ui/knowledge.hpp"
#include "ui/guide.hpp"
#include "slice/transform.hpp"
#include "ui/slicing.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_internal.h> // ImGui::BringWindowToDisplayFront, for the guided tutorials
#ifdef TISMA_VULKAN_WINDOW
#include <imgui_impl_vulkan.h>
#endif
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <misc/cpp/imgui_stdlib.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string_view>
#include <tuple>
#include <utility>

namespace tisma::ui {
namespace {

// Sizes in screen units before the monitor's scale.
constexpr float kTopBar = 56.0f;
constexpr float kStatusBar = 30.0f;
constexpr float kPanel = 420.0f;
constexpr float kFontSize = 15.0f;
constexpr float kTitleSize = 21.0f;
constexpr double kToastSeconds = 6.0;
constexpr std::uint32_t kWarningColour = 0xE5A13A; // a warning in the help (an amber of its own)

constexpr const char* kIcons[] = {
    "tisma_plate",      "tisma_print",     "tisma_filament", "tisma_printer",     "tisma_more",      "tisma_folder",
    "tisma_save",       "tisma_slice",     "tisma_send",     "tisma_engineering", "tisma_calibrate", "tisma_vibration",
    "tisma_view",       "tisma_view_back", "tisma_view_front", "tisma_view_left", "tisma_view_right", "tisma_view_top",
    "tisma_zoom_fit",   "tisma_zoom_in",   "tisma_zoom_out",
    "tool_add",         "tool_remove",     "tool_duplicate", "tool_move",         "tool_rotate",     "tool_scale",
    "tool_lay_flat",    "tool_arrange",     "tool_paint",     "tool_material",   "tool_cut",   "tool_measure",
    "section_quality",  "section_strength", "section_speed", "section_support",  "section_adhesion", "section_other",
    "section_temperature", "section_cooling", "section_flow", "section_retraction", "section_needs",
    "section_machine",  "section_nozzle",  "section_limits",  "section_gcode",   "help",
};

const std::vector<std::string> kOpenExtensions = {".stl", ".obj", ".3mf", ".tisma", ".gcode", ".gco", ".g", ".bgcode", ".json"};
const std::vector<std::string> kModelExtensions = {".stl", ".obj", ".3mf"};

bool is_model(const std::filesystem::path& path) {
    const std::string ext = lower_extension(path);
    return std::find(kModelExtensions.begin(), kModelExtensions.end(), ext) != kModelExtensions.end();
}

ImVec4 rgb(std::uint32_t c, float alpha = 1.0f) {
    return ImVec4(float((c >> 16) & 0xFF) / 255.0f, float((c >> 8) & 0xFF) / 255.0f, float(c & 0xFF) / 255.0f, alpha);
}
Vec3 vec(std::uint32_t c) {
    return Vec3{float((c >> 16) & 0xFF) / 255.0f, float((c >> 8) & 0xFF) / 255.0f, float(c & 0xFF) / 255.0f};
}

// The colours of a theme: the brand palette of assets/brand/README.md around
// neutral grays.
struct Theme {
    bool dark = true;
    std::uint32_t window, panel, frame, frame_hover, frame_active, border, text, text_dim;
    std::uint32_t accent = 0x7A24C9, accent_hover = 0x9D40E9, accent_active = 0x5B0FA7, lavender = 0xD9BDF5;
    std::uint32_t view_top, view_bottom, plate, model;
    std::uint32_t sky, ground; // the ambient light from above and from below
    std::uint32_t grid_minor, grid_major;
    std::uint32_t paint_enforce = 0x2FBF71, paint_block = 0xE5484D; // the Paint tool's marks
};

Theme dark_theme() {
    Theme t;
    t.dark = true;
    t.window = 0x1E1E23, t.panel = 0x25252B, t.frame = 0x33333B, t.frame_hover = 0x3E3E48, t.frame_active = 0x4A4A56;
    t.border = 0x393943, t.text = 0xE9E9EE, t.text_dim = 0x9B9BA8;
    t.view_top = 0x2C2C34, t.view_bottom = 0x17171B, t.plate = 0x2E2E36, t.model = 0xA05BE6;
    t.sky = 0xC9CCE3, t.ground = 0x4B4540;
    t.grid_minor = 0x2EFFFFFF, t.grid_major = 0x5CFFFFFF;
    return t;
}

Theme light_theme() {
    Theme t;
    t.dark = false;
    t.window = 0xF2F2F4, t.panel = 0xFFFFFF, t.frame = 0xE7E7EC, t.frame_hover = 0xDCDCE3, t.frame_active = 0xCFCFD8;
    t.border = 0xD2D2DA, t.text = 0x0B0B0B, t.text_dim = 0x5E5E6A;
    t.view_top = 0xFAFAFC, t.view_bottom = 0xD9D9E1, t.plate = 0xC4C4CE, t.model = 0x8E44D8;
    t.sky = 0xEEF0FA, t.ground = 0x8C857F;
    t.grid_minor = 0x33000000, t.grid_major = 0x66000000;
    return t;
}

void apply_style(const Theme& t, float scale) {
    ImGuiStyle style;
    if (t.dark)
        ImGui::StyleColorsDark(&style);
    else
        ImGui::StyleColorsLight(&style);
    style.WindowRounding = 0.0f;
    style.ChildRounding = 8.0f;
    style.FrameRounding = 5.0f;
    style.PopupRounding = 8.0f;
    style.GrabRounding = 5.0f;
    style.ScrollbarRounding = 6.0f;
    style.WindowBorderSize = 0.0f;
    style.FrameBorderSize = 0.0f;
    style.WindowPadding = ImVec2(12, 10);
    style.FramePadding = ImVec2(8, 5);
    style.ItemSpacing = ImVec2(8, 7);
    style.ScrollbarSize = 12.0f;
    style.GrabMinSize = 12.0f;
    ImVec4* c = style.Colors;
    c[ImGuiCol_Text] = rgb(t.text);
    c[ImGuiCol_TextDisabled] = rgb(t.text_dim);
    c[ImGuiCol_WindowBg] = rgb(t.window);
    c[ImGuiCol_ChildBg] = rgb(t.panel, 0.0f);
    c[ImGuiCol_PopupBg] = rgb(t.panel, 0.98f);
    c[ImGuiCol_Border] = rgb(t.border);
    c[ImGuiCol_FrameBg] = rgb(t.frame);
    c[ImGuiCol_FrameBgHovered] = rgb(t.frame_hover);
    c[ImGuiCol_FrameBgActive] = rgb(t.frame_active);
    c[ImGuiCol_TitleBg] = rgb(t.panel);
    c[ImGuiCol_TitleBgActive] = rgb(t.panel);
    c[ImGuiCol_CheckMark] = rgb(t.accent_hover);
    c[ImGuiCol_SliderGrab] = rgb(t.accent);
    c[ImGuiCol_SliderGrabActive] = rgb(t.accent_hover);
    c[ImGuiCol_Button] = rgb(t.frame);
    c[ImGuiCol_ButtonHovered] = rgb(t.frame_hover);
    c[ImGuiCol_ButtonActive] = rgb(t.frame_active);
    c[ImGuiCol_Header] = rgb(t.frame);
    c[ImGuiCol_HeaderHovered] = rgb(t.frame_hover);
    c[ImGuiCol_HeaderActive] = rgb(t.frame_active);
    c[ImGuiCol_Separator] = rgb(t.border);
    c[ImGuiCol_ResizeGrip] = rgb(t.accent, 0.3f);
    c[ImGuiCol_TextSelectedBg] = rgb(t.accent, 0.45f);
    c[ImGuiCol_NavCursor] = rgb(t.accent_hover);
    c[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, 0.45f);
    style.ScaleAllSizes(scale);
    style.FontSizeBase = kFontSize;
    style.FontScaleDpi = scale;
    ImGui::GetStyle() = style;
}

// Thousands as the language groups them: 297 412, or 297,412 in English (TR2).
std::string number_text(std::size_t n) { return grouped(n); }

// With TISMA_TIMING set, how long each step took since the last one, on
// stderr: opening a large model, step by step (the owner's priority item,
// 2026-10-28).
void timing(const char* step) {
    static const bool on = std::getenv("TISMA_TIMING") != nullptr;
    static auto last = std::chrono::steady_clock::now();
    if (!on)
        return;
    const auto now = std::chrono::steady_clock::now();
    std::fprintf(stderr, "timing: %-24s %8.1f ms\n", step, std::chrono::duration<double, std::milli>(now - last).count());
    last = now;
}

// A default or a range as docs/CONFIG.md writes it, in Spanish ("0,1 a 2"),
// in the interface's language: in English the decimal point and "to"
// (the spec team's TR3).
std::string doc_value_text(const std::string& text) {
    if (language() == Language::Spanish)
        return text;
    std::string out;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const bool between_digits = i > 0 && i + 1 < text.size() && std::isdigit(static_cast<unsigned char>(text[i - 1])) &&
                                    std::isdigit(static_cast<unsigned char>(text[i + 1]));
        if (text[i] == ',' && between_digits) {
            out += '.';
        } else if (text.compare(i, 3, " a ") == 0) {
            out += " to ";
            i += 2;
        } else if (text.compare(i, 3, " o ") == 0) {
            out += " or ";
            i += 2;
        } else {
            out += text[i];
        }
    }
    return out;
}

// What a drag or slider field shows (the spec team's TR1): its value with
// the language's decimal point. While the field is hovered, focused or in
// use, ImGui's own format: it writes '.' and reads typed text with it.
const char* field_text(const char* label, const char* fmt, double v) {
    static std::string kept[8];
    static std::size_t next = 0;
    const ImGuiID id = ImGui::GetID(label);
    const ImGuiContext& g = *GImGui;
    if (language() != Language::Spanish || g.ActiveId == id || g.HoveredIdPreviousFrame == id || g.NavId == id ||
        !std::strchr(fmt, '%'))
        return fmt;
    std::string& out = kept[next++ % 8];
    out.clear();
    for (const char c : format(fmt, v)) { // its '%' stays a '%'
        out += c;
        if (c == '%')
            out += '%';
    }
    return out.c_str();
}

// ImGui's printf-like texts, each number with the language's decimal point (TR1).
namespace loc {
#define TISMA_LOC_TEXT(call)                                                                                           \
    std::va_list args;                                                                                                 \
    va_start(args, fmt);                                                                                               \
    const std::string text = vformat(fmt, args);                                                                       \
    va_end(args);                                                                                                      \
    call
void Text(const char* fmt, ...) IM_FMTARGS(1);
void Text(const char* fmt, ...) { TISMA_LOC_TEXT(ImGui::TextUnformatted(text.c_str())); }
void TextDisabled(const char* fmt, ...) IM_FMTARGS(1);
void TextDisabled(const char* fmt, ...) { TISMA_LOC_TEXT(ImGui::TextDisabled("%s", text.c_str())); }
void TextWrapped(const char* fmt, ...) IM_FMTARGS(1);
void TextWrapped(const char* fmt, ...) { TISMA_LOC_TEXT(ImGui::TextWrapped("%s", text.c_str())); }
void TextColored(const ImVec4& colour, const char* fmt, ...) IM_FMTARGS(2);
void TextColored(const ImVec4& colour, const char* fmt, ...) { TISMA_LOC_TEXT(ImGui::TextColored(colour, "%s", text.c_str())); }
void BulletText(const char* fmt, ...) IM_FMTARGS(1);
void BulletText(const char* fmt, ...) { TISMA_LOC_TEXT(ImGui::BulletText("%s", text.c_str())); }
void SetTooltip(const char* fmt, ...) IM_FMTARGS(1);
void SetTooltip(const char* fmt, ...) { TISMA_LOC_TEXT(ImGui::SetTooltip("%s", text.c_str())); }
void SetItemTooltip(const char* fmt, ...) IM_FMTARGS(1);
void SetItemTooltip(const char* fmt, ...) { TISMA_LOC_TEXT(ImGui::SetItemTooltip("%s", text.c_str())); }
#undef TISMA_LOC_TEXT
} // namespace loc

T kind_text(LineKind kind) {
    constexpr T names[kLineKinds] = {T::KindTravel,       T::KindOuterWall,     T::KindInnerWall,  T::KindOverhangWall,
                                     T::KindSparseInfill, T::KindSolidInfill,   T::KindTopSolidInfill, T::KindBridgeInfill,
                                     T::KindGapFill,      T::KindIroning,       T::KindSkirtBrim,  T::KindSupport,
                                     T::KindSupportInterface, T::KindWipeTower, T::KindCustom,     T::KindOther};
    return names[static_cast<std::size_t>(kind)];
}

// The interface's own preferences, kept between runs next to the user's
// other settings: language, theme, the last folder and the slicing settings.
std::filesystem::path preferences_path() {
#ifdef _WIN32
    const char* base = std::getenv("APPDATA");
    return base ? std::filesystem::path(base) / "Tisma" / "interface.json" : std::filesystem::path();
#else
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg)
        return std::filesystem::path(xdg) / "tisma" / "interface.json";
    const char* home = std::getenv("HOME");
    return home ? std::filesystem::path(home) / ".config" / "tisma" / "interface.json" : std::filesystem::path();
#endif
}

// Writes a file aside and moves it over, so a failed write leaves the old one.
bool write_aside(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::path temporary = path;
    temporary += ".tmp";
    {
        std::ofstream out(temporary, std::ios::binary);
        out << text;
        out.close();
        if (!out)
            return false;
    }
    std::error_code ec;
    std::filesystem::rename(temporary, path, ec);
    if (ec)
        std::filesystem::remove(temporary, ec);
    return !ec;
}

// Preferences this build cannot read (a newer build's setting, say) are kept
// next to them before the defaults are written over them on leaving.
void keep_unreadable(const std::filesystem::path& path) {
    std::filesystem::path copy = path;
    copy += ".unreadable";
    std::error_code ec;
    std::filesystem::copy_file(path, copy, std::filesystem::copy_options::overwrite_existing, ec);
}

enum class Tab { Prepare, Preview, Analysis, Calibration, Device };
// The preview's views (preview.md, rule 1), in its order: what colours the
// lines, the shader's way to colour them, and the number the instances carry.
enum class PreviewView {
    Kind,
    Height,
    Width,
    Speed,
    ActualSpeed,
    Fan,
    Temperature,
    Flow,
    ActualFlow,
    LayerTime,
    LayerTimeLog,
    Tool,
    ColorPrint
};
inline constexpr int kPreviewViews = 13;
struct ViewInfo {
    T name;
    ColorMode shader;
    PreviewValue value;
};
constexpr ViewInfo kViewInfo[kPreviewViews] = {
    {T::ByKind, ColorMode::Kind, PreviewValue::Feed},
    {T::ByHeight, ColorMode::Height, PreviewValue::Feed},
    {T::ByWidth, ColorMode::Width, PreviewValue::Feed},
    {T::BySpeed, ColorMode::Speed, PreviewValue::Feed},
    {T::ByActualSpeed, ColorMode::Speed, PreviewValue::ActualFeed},
    {T::ByFan, ColorMode::Speed, PreviewValue::Fan},
    {T::ByTemperature, ColorMode::Speed, PreviewValue::Temperature},
    {T::ByFlow, ColorMode::Speed, PreviewValue::Flow},
    {T::ByActualFlow, ColorMode::Speed, PreviewValue::ActualFlow},
    {T::ByLayerTime, ColorMode::Speed, PreviewValue::LayerTime},
    {T::ByLayerTimeLog, ColorMode::Speed, PreviewValue::LayerTimeLog},
    {T::ByTool, ColorMode::Speed, PreviewValue::Tool},
    {T::ByColorPrint, ColorMode::Speed, PreviewValue::ColorPrint},
};

// The colours of the preview's marks (rule 11), in MarkKind order, and of
// the wipes.
constexpr std::uint32_t kMarkColours[kMarkKinds] = {0xFF5A36, 0x3FBF7F, 0xF0F0F0, 0xFFD23F, 0xE04FD0, 0x9B6BFF, 0x4FA3FF};
constexpr std::uint32_t kWipeColour = 0x7FE0C0;

// What the layer slider's «+» adds at a layer (preview.md, rule 12).
enum class ChangeKind { Color, Pause, Gcode };

enum class Purpose {
    Open,
    AddModel,
    SaveGcode,
    LoadSettings,
    SaveSettings,
    SaveProject,
    SaveExtruderTest,
    LoadCase,
    ExportView,
    ImportView
};

// The plate tool in use (docs/UI.md, «Herramientas de la placa»).
enum class PlateTool { Select, Move, Rotate, Scale, LayFlat, Cut, Measure, Paint, Material };

class App {
public:
    explicit App(const AppOptions& options) : options_(options) {}
    int run();

private:
    bool init(std::string& error);
    void shutdown();
    void frame();
    void layout();

    // Panels.
    void top_bar();
    void settings_panel();
    void status_bar();
    void viewport();
    void view_tools();
    void preview_panels();
    void analysis_panel();
    void calibration_panel();
    void use_mesh(TriangleMesh mesh, const std::filesystem::path& name, Tab tab);
    void add_model(const std::filesystem::path& path);
    void add_mesh(TriangleMesh mesh, const std::filesystem::path& name);
    void rebuild_plate();
    void plate_changed();
    void arrange_plate();
    void plate_tools();
    // The chosen object mirrored, or cut in two (model_tools.md, rules 13 and 16).
    void mirror_selected(int axis);
    void cut_selected();
    void split_selected();
    void cut_plane_overlay();
    void measure_overlay();
    void paint_card(ImVec2 at);
    void mixing_dialog();
    std::string filament_color(int extruder) const;
    void objects_material_changed();
    void brush_stroke(std::size_t object);
    void update_paint_view();
    Vec3 mouse_ray() const;
    void load_calibration(CalibrationPrint print);
    void create_calibration(CalibrationKind kind, bool custom);
    void begin_export();
    void empty_hint();
    void toasts();
    void dialogs();

    // Actions.
    void open_path(const std::filesystem::path& path);
    void load_model(const std::filesystem::path& path);
    void load_gcode(const std::filesystem::path& path);
    void load_settings_file(const std::filesystem::path& path);
    void load_case_file(const std::filesystem::path& path);
    void save_settings_file(const std::filesystem::path& path);
    void open_project(const std::filesystem::path& path);
    void begin_save_project(bool ask);
    void save_project_to(const std::filesystem::path& path);
    void export_gcode_to(const std::filesystem::path& path);
    void start_slice();
    void poll_jobs();
    void show_preview();
    void upload_preview();
    void preview_overlay(const GcodePreview& preview);
    void add_change_at(int layer, ChangeKind kind, std::string text = {});
    void part_labels();
    void update_ray_scene();
    void update_coarse_model();
    void check_rays(const ViewFrame& f);
    void compare_view(const std::vector<std::uint8_t>& picture, int fb_w, int fb_h);
    void check_moire(int fb_w, int fb_h);
    // The Device page (docs/DEVICE.md).
    void device_panel();
    void device_connect(const std::string& port);
    void device_disconnect();
    void device_poll();
    bool device_can_print() const;
    void device_print();
    ViewFrame view_frame(int fb_w, int fb_h);
    void settings_changed();
    void bed_changed();
    void place_model();
    void frame_all();
    void frame_now();
    void set_tab(Tab tab);
    void estimate_voxels_if_stale();
    void notify(std::string text, bool error = false);
    void fail(T title, std::string message, std::vector<std::string> details = {});
    void load_preferences();
    void save_preferences();
    void apply_theme();

    // Profiles (docs/PROFILES.md).
    std::filesystem::path profiles_root() const;
    void load_profile_lists();
    void profile_row(ProfileKind kind);
    void choose_profile(ProfileKind kind, const Profile& profile);
    // Speeds the settings lack: a stock Ender-3's, and a word about it.
    void fill_speeds();
    void save_profile_as(ProfileKind kind, std::string name);
    void refresh_profiles();
    const Profile* find_profile(ProfileKind kind, const std::string& name, bool* builtin = nullptr) const;
    void catalog_dialog();
    void add_catalog_profile(const Profile& profile);

    // Widgets.
    ImTextureRef icon(const char* name) const;
    bool button(const char* id, const char* icon_name, const char* label, bool primary, bool selected, bool enabled,
                const char* tip, float height);
    bool icon_button(const char* id, const char* icon_name, const char* tip, float size);
    // The settings panel by sections and cards (docs/spec/features/settings_panel.md).
    bool settings_card(const SubsectionLayout& sub, bool first, int changes, const std::string& summary, bool& reset);
    bool card_rows(std::string_view id, bool& bed);
    // An object's menu and the window with its own settings (settings_levels.md, rules 10 and 11).
    void object_menu();
    void object_settings_window();
    bool own_settings_editor(std::string& own, const std::string& under, bool layers, std::string_view where);
    bool volume_editor(PlateObject& o, std::size_t k, bool& removed);
    bool range_editor(PlateObject& o, std::size_t k, bool& removed);
    void objects_list();
    void volumes_overlay();
    int object_settings_ = -1;       // the object whose settings window is open
    std::set<std::string> row_only_; // while drawing an object's settings: the rows to show
    bool setting_shown(std::string_view key, std::string_view card) const;
    bool setting_applies(std::string_view key) const;
    bool card_shown(const SubsectionLayout& sub) const;
    // Whether the level (or the Custom view) shows a setting, before what
    // else shows it.
    bool view_shows(std::string_view key) const;
    // The settings of the chosen profiles, as saved: what a change is
    // measured from and a card's Reset goes back to (rule 8).
    Settings saved_settings() const;
    void choose_level(ViewLevel level);
    // The search (rule 5): the field in the top bar, and going to a setting.
    void search_box(float width, float height);
    void go_to_setting(const std::string& key);
    // Choosing what Custom shows (rule 16).
    void choose_settings_dialog();
    // «Based on» under a card, and the Credits and sources page (credits.md).
    void card_credits(std::string_view card, float left, float right);
    void credits_dialog();
    // The help panel over the settings panel (assistant.md).
    void help_panel();
    void load_help_data();
    void help_body();
    void help_problem(const KnowledgeProblem& problem);
    void help_procedure(const KnowledgeProcedure& procedure);
    void help_guide(const KnowledgeCalibration& guide);
    void open_guide(std::string_view id);
    std::set<std::string> guides_done() const;
    void help_action(const KnowledgeAction& action);
    void help_apply_dialog();
    void anchor(const char* name); // the last item, as a part tutorials can point at
    void anchor_rect(const char* name, ImVec2 min, ImVec2 max);
    void fire(std::string_view event);
    void offer_tutorials(std::string_view when);
    void end_guide(bool finished);
    void guide_overlay();
    // The chosen filament's material in the help's data, its card and its
    // warnings (assistant.md, rules 13 and 14).
    const KnowledgeMaterial* current_material() const;
    void material_card();
    // A section in the column of sections, and a section's cards.
    bool section_button(std::string_view id, const char* title, const char* icon_name, bool selected, bool marked,
                        float column);
    void section_cards(const SectionLayout& section, bool& changed, bool& bed);
    // The printer's card on top of the panel, its nozzle change, and its page.
    void printer_card();
    void choose_nozzle(double diameter, const std::string& profile);
    void printer_page();
    void record_calibration(CalibrationKind kind);
    void record_calibration(const std::string& name);
    void notify_undo(std::string text, std::function<void()> undo);

    AppOptions options_;
    GLFWwindow* window_ = nullptr;
    Renderer renderer_;
    OrbitCamera camera_;
    float scale_ = 1.0f;
    Theme theme_ = dark_theme();
    ImFont* font_ = nullptr;
    std::map<std::string, ImTextureID> icons_;
    // The settings panel: its tab, level, the section chosen in each tab, the
    // cards open (the first of each section at first), each card's height in
    // the last frame (for its background), Basic's «Show all» until the
    // section changes, and the settings that differ from the chosen profiles.
    SettingsTab panel_tab_ = SettingsTab::Print;
    // The Objects tab instead of the settings (settings_levels.md, rule 13):
    // the object chosen in its list, and the row under it.
    bool objects_tab_ = false;
    int list_object_ = -1;
    std::optional<ObjectRow> list_row_;
    ViewLevel view_level_ = ViewLevel::Normal;
    std::array<std::string, 3> panel_section_{"quality", "temperature", "machine"};
    std::map<std::string, bool> card_open_;
    std::map<std::string, float> card_heights_;
    std::set<std::string> show_all_cards_;
    std::set<std::string> changed_keys_;
    // Custom (rules 15 to 19): the settings it shows; «See all together»
    // (rule 4); the search's text, the setting it went to (lit for 2 s, and
    // shown whatever the level until the section changes), and the dialog
    // that chooses what Custom shows, with its draft and its filter.
    CustomView custom_view_;
    bool single_column_ = false;
    std::string search_query_;
    bool search_focus_ = false;
    bool search_list_hovered_ = false;
    std::string highlight_key_;
    double highlight_until_ = 0.0;
    bool scroll_to_highlight_ = false;
    std::set<std::string> revealed_keys_;
    bool choose_open_ = false;
    CustomView choose_draft_;
    std::string choose_query_;
    // The printer's card and its page (rules 21 to 40): the page open and its
    // section, the printer list's search, the size asked for «Other…», the
    // bed chosen on printers with several, and each printer's calibrations
    // applied (rule 38) and notes, kept in the preferences.
    bool printer_page_ = false;
    std::string printer_section_ = "machine";
    std::string printer_query_;
    double other_nozzle_ = 0.4;
    std::string bed_type_;
    std::map<std::string, std::map<std::string, CalibrationRecord>> calibration_records_;
    std::map<std::string, std::string> printer_notes_;
    ImTextureID logo_ = 0;
    // The window with Vulkan (--graphics; PLAN.md «Vista con Vulkan»): the
    // panels and the 3D view drawn by it. The view goes first at shutdown.
    VulkanWindow vk_;
    VulkanView vk_view_;
    bool vulkan_ = false;
    ImVec2 view_pos_{0, 0}, view_size_{1, 1};

    Tab tab_ = Tab::Prepare;
    Settings settings_ = default_settings();

    // The plate: each model as read with where it goes (objects_), each as
    // placed (placed_meshes_), and all of them in one mesh, as the slicer and
    // the view take them (mesh_, in bed coordinates: offset_ stays zero).
    std::vector<PlateObject> objects_;
    std::vector<TriangleMesh> placed_meshes_;
    std::vector<std::size_t> object_vertices_; // where each model's vertices start in the view's mesh
    int selected_ = -1;
    PlateTool tool_ = PlateTool::Select;
    int pressed_object_ = -1;  // the model the left button went down on
    Vec3 drag_anchor_{};       // the point on the bed plane the drag holds
    bool dragged_ = false;
    Vec3 drag_offset_{};       // how far the dragged model moved since its mesh was made (the view shifts it)
    std::vector<TriangleIndex> indexes_; // a search over each placed mesh, made when first needed
    const std::vector<TriangleIndex>& plate_indexes();
    bool keep_scale_ = true;   // the Scale tool scales all three axes together
    // The Cut tool (model_tools.md, rules 13 and 18): the plane's height over
    // the bed, and whether the part below is turned over onto its cut face.
    double cut_height_ = 5.0;
    bool cut_flip_lower_ = false;
    // The Measure tool (rule 20): what was picked on which object, and the
    // distance it should be.
    std::vector<MeasureFeature> measure_picks_;
    int measure_object_ = -1;
    double measure_wanted_ = 0.0;
    // The project's mixed colours and each filament's colour
    // (color_mixing.md); the dialog that edits them, what they were when it
    // opened and the blend being edited.
    std::vector<ColorMix> mixes_;
    std::vector<std::string> filament_colors_;
    bool mixing_open_ = false;
    std::vector<ColorMix> mixes_before_;
    int mix_edit_ = -1;
    // The Paint tool (docs/PAINTING.md): what it paints, with which mark and
    // how wide; the model a stroke started on; where the brush is now.
    PaintChannel paint_channel_ = PaintChannel::Support;
    std::uint8_t paint_mark_ = kPaintEnforce;
    float brush_radius_ = 2.0f;
    int painting_object_ = -1;
    bool painted_ = false;     // the stroke changed something
    bool paint_dirty_ = true;  // the view's copy of the paint is out of date
    std::optional<float> brush_depth_; // how far ahead of the eye the face under the mouse is
    std::optional<TriangleMesh> mesh_;
    std::filesystem::path model_path_;
    Vec3 offset_{};
    BoundingBox placed_;

    // What the model and the settings are now, and what the running jobs
    // were started from.
    Revision revision_, slice_made_, voxels_made_;
    Slicer slicer_;
    Background<SliceResult> slice_job_;
    std::optional<SliceResult> result_;
    // The load case of stress-guided infill (docs/INFILL.md), as analyze takes it.
    std::optional<AnalysisCase> load_case_;
    std::string load_case_name_;
    bool stale_ = false;  // the model or the settings changed since the result
    bool viewer_ = false; // the result is an opened G-code file, not a slice
    std::filesystem::path gcode_path_;
    double slice_started_ = 0.0;

    Background<VoxelEstimate> voxel_job_;
    std::optional<VoxelEstimate> voxels_;
    bool voxels_stale_ = true;

    // The preview's controls.
    int top_layer_ = 0; // 0-based
    int bottom_layer_ = 0;
    bool only_layer_ = false;
    // The layer slider: which handle is dragged; a pause's message or a
    // G-code being written for a layer, and the change whose menu is open.
    bool dragging_bottom_ = false;
    // The move slider (preview.md, rule 4): the top layer's moves drawn, for
    // that layer (it starts whole on another one).
    int move_cut_ = -1, move_cut_layer_ = -1;
    bool change_dialog_ = false;
    int change_layer_ = 0, change_edit_ = -1;
    ChangeKind change_kind_ = ChangeKind::Color;
    std::string change_text_;
    bool show_travels_ = true;
    // The preview's icon row (preview.md, rule 11): wipes, the marks by kind,
    // the centre of gravity and the nozzle (travels: show_travels_).
    bool show_wipes_ = false, show_gravity_ = false, show_nozzle_ = true;
    bool show_legend_ = true;  // L hides the preview's legend
    bool show_labels_ = false; // E shows the parts' names
    std::array<bool, kMarkKinds> show_marks_{false, false, true, false, false, false, false}; // seams on
    ColorMode color_mode_ = ColorMode::Kind; // the shader's, from the view
    PreviewView preview_view_ = PreviewView::Kind;
    std::uint32_t kind_mask_ = 0xFFFFFFFFu;
    float feed_range_[2] = {0, 0}, width_range_[2] = {0, 0}, height_range_[2] = {0, 0};
    PreviewBuffers preview_buffers_;

    // How the view is drawn, and the scene its rays are traced against.
    RenderMode render_mode_ = RenderMode::Ambient;
    int msaa_ = 4;
    bool fxaa_ = true;
    bool moire_ = true; // the moiré filter of the preview's lines and the grid
    float ao_strength_ = 1.0f;
    struct RayBuild {
        std::uint64_t version = 0;
        bool preview = false;
        std::shared_ptr<RayScene> scene;
        std::vector<float> nodes, primitives;
    };
    Background<RayBuild> ray_job_;
    // The coarse copy of a large plate to draw while the view moves (PLAN.md
    // V6), built on another thread for scene_version_, with each object's
    // first vertex in it.
    struct CoarseBuild {
        std::uint64_t version = 0;
        std::vector<float> vertices;
        std::vector<std::size_t> objects;
    };
    Background<CoarseBuild> coarse_job_;
    std::uint64_t coarse_version_ = 0;
    std::vector<std::size_t> coarse_object_vertices_;
    int moved_frame_ = -100; // the last frame the camera moved
    std::uint64_t scene_version_ = 1, ray_version_ = 0;
    bool ray_preview_ = false, ray_failed_ = false;
    std::shared_ptr<RayScene> ray_scene_;
    bool ray_checked_ = false;
    bool moire_checked_ = false;

    // The connected printer: a serial port, or the simulated one.
    std::unique_ptr<device::Printer> printer_;
    std::unique_ptr<device::SimulatedPrinter> simulator_;
    std::vector<std::string> device_ports_;  // last listed
    std::string device_port_ = "sim";        // the chosen one ("sim": the simulated printer)
    int device_baud_ = 115200;
    double device_nozzle_ = 200.0, device_bed_ = 60.0, device_step_ = 10.0, device_fan_ = 100.0;
    std::vector<std::string> device_log_;    // the events, newest last
    bool device_print_sent_ = false;         // with --device-print: the job was started
    bool device_print_done_ = false;         // and it finished (or failed)

    FileBrowser browser_;
    Purpose purpose_ = Purpose::Open;
    std::filesystem::path last_folder_;
    std::vector<std::filesystem::path> dropped_;

    struct Toast {
        std::string text;
        bool error = false;
        double until = 0.0;
        std::function<void()> undo; // with it, the toast has an «Undo» button
    };
    std::vector<Toast> toasts_;
    bool error_open_ = false;
    std::string error_title_, error_message_;
    std::vector<std::string> error_details_;
    bool about_open_ = false;
    // Credits and sources (credits.md, rule 4): open, and the library whose
    // license notice shows (-1: none).
    bool credits_open_ = false;
    int credits_notice_ = -1;
    // The help panel (assistant.md, rules 1 to 4): open, its tab and its search.
    bool help_open_ = false;
    int help_tab_ = 0;
    bool help_select_ = false; // choose help_tab_ in the tab bar once
    // The help's data (knowledge/), read when the panel opens; what it shows
    // (a list, or one problem or procedure) and the problems it came through,
    // for Back; a setting change waiting for its confirmation (rule 7).
    std::optional<Knowledge> knowledge_;
    enum class HelpPage { List, Problem, Procedure, Guide };
    HelpPage help_page_ = HelpPage::List;
    std::size_t help_item_ = 0;
    std::vector<std::size_t> help_back_;
    std::vector<bool> help_steps_done_;
    // What the person writes in a guide's result (rule 23).
    GuideInput guide_input_;
    GuideInput guide_from_;               // what the proposal was computed from
    std::optional<double> guide_proposed_; // the proposal, as corrected
    // The assistant's preferences (assistant.md, «Ajustes»).
    bool assistant_tutorials_ = true, assistant_glossary_ = true, assistant_material_ = true;
    std::optional<SettingChange> help_change_;
    std::string help_change_guide_; // the guide whose result it is (rule 23), to date it
    bool help_change_open_ = false;
    // The guided tutorials (assistant.md, rules 15 to 18): the parts of the
    // interface named this frame (anchors), what is remembered of each
    // tutorial, the one going on and those offered now.
    std::map<std::string, std::array<float, 4>> anchors_;
    GuideMemory guide_memory_;
    std::optional<Guide> guide_;
    std::vector<std::size_t> guide_offers_;
    bool guide_opened_panel_ = false;
    std::string help_query_;
    // Help (docs/UI.md, «Ayuda»): the assistant's step on screen (-1: none),
    // the first-use tip on screen, the tips already seen and whether they show.
    int tour_step_ = -1;
    std::string tip_;
    std::set<std::string> tips_seen_;
    bool tips_ = true;
    bool first_run_ = false; // no preferences yet: the assistant opens
    PlateTool last_tool_ = PlateTool::Select;
    Tab last_tab_ = Tab::Prepare;
    bool had_result_ = false;
    void tip(const std::string& id);
    void help_windows();

    // Calibration: the dimensional test, what was measured on it, the towers'
    // ranges, and the test loaded last (its segments, without the mesh).
    DimensionTest dimension_test_;
    Measurements measured_{30.0, 20.0, 15.0, 10.0};
    double elephant_bottom_ = 20.0, elephant_body_ = 20.0; // the elephant's foot block, measured
    double resin_measured_[3] = {30.0, 30.0, 10.0};       // the resin measuring block, measured
    struct TowerRange {
        double start = 0.0, end = 0.0, step = 1.0, segment = 5.0;
    };
    TowerRange ranges_[kCalibrationKinds]; // by CalibrationKind, filled in when a test is created
    std::optional<CalibrationPrint> calibration_;
    CalibrationKind calibration_kind_ = CalibrationKind::Dimensions;
    std::optional<double> chosen_value_;
    // The extruder steps test: open, the value in the printer now, and what
    // was left of the 120 mm mark after extruding 100 mm.
    bool extruder_open_ = false, extruder_scroll_ = false;
    double extruder_current_ = 93.0, extruder_left_ = 20.0;

    // Named profiles: the built-in ones and the user's of each kind (by
    // ProfileKind), the one chosen, and whether the settings still match it
    // (worked out again when the settings change).
    std::array<std::vector<Profile>, 3> builtin_profiles_, user_profiles_;
    // Printer profiles without speeds of their own, which took a stock
    // Ender-3's (fill_missing_speeds).
    std::set<std::string> speed_fallback_;
    std::array<std::string, 3> chosen_profile_;
    std::array<bool, 3> profile_modified_{};
    std::string profiles_checked_;
    std::optional<ProfileKind> save_profile_kind_; // the "save as" dialog, while open
    bool save_profile_open_ = false;
    std::string profile_name_;
    // The catalog dialog (docs/CATALOG.md): for which kind, the search, the
    // entry picked (an index in the search's results) and its nozzle.
    std::optional<ProfileKind> catalog_kind_;
    bool catalog_open_ = false;
    std::string catalog_query_;
    int catalog_pick_ = -1;
    std::size_t catalog_nozzle_ = 0;
    // The project file last opened or saved.
    std::filesystem::path project_path_;

    // Screenshot mode.
    int frames_ = 0, settled_frames_ = 0;
    bool quit_ = false;
    bool gl_loaded_ = false;
    bool frame_pending_ = false;
    bool failed_ = false; // the screenshot or its export failed
    bool benched_ = false; // --bench-frames done
    unsigned view_queries_[3] = {}; // timer queries of the 3D view, in turn
    bool view_timing_[3] = {};      // a query is out, its result not read yet
    int view_query_ = 0;
    double view_ms_ = 0.0; // the 3D view's GPU time per frame, smoothed
    double preview_ms_ = 0.0; // building the last preview's buffers
};

ImTextureRef App::icon(const char* name) const {
    const auto it = icons_.find(name);
    return ImTextureRef(it == icons_.end() ? ImTextureID(0) : it->second);
}

// A button with an icon and a label, drawn by hand so it can carry the brand's
// colours: plain, selected (a tab) or primary (the purple action).
bool App::button(const char* id, const char* icon_name, const char* label, bool primary, bool selected, bool enabled,
                 const char* tip, float height) {
    const ImGuiStyle& style = ImGui::GetStyle();
    const float icon_size = std::round(height * 0.5f);
    const float pad = std::round(height * 0.32f);
    const ImVec2 text = label ? ImGui::CalcTextSize(label) : ImVec2(0, 0);
    const float width = pad * 2.0f + (icon_name ? icon_size : 0.0f) +
                        (label && icon_name ? style.ItemInnerSpacing.x + 2.0f : 0.0f) + text.x;
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::BeginDisabled(!enabled);
    const bool clicked = ImGui::InvisibleButton(id, ImVec2(width, height));
    const bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
    const bool held = ImGui::IsItemActive();
    ImGui::EndDisabled();
    if (tip && hovered)
        loc::SetItemTooltip("%s", tip);

    ImU32 bg = 0, fg = ImGui::GetColorU32(rgb(theme_.text));
    if (primary) {
        bg = ImGui::GetColorU32(rgb(held ? theme_.accent_active : hovered ? theme_.accent_hover : theme_.accent));
        fg = IM_COL32_WHITE;
    } else if (selected) {
        bg = ImGui::GetColorU32(rgb(theme_.accent, theme_.dark ? 0.35f : 0.18f));
    } else if (hovered) {
        bg = ImGui::GetColorU32(rgb(held ? theme_.frame_active : theme_.frame_hover));
    }
    if (!enabled) {
        fg = ImGui::GetColorU32(rgb(theme_.text_dim, 0.7f));
        if (primary)
            bg = ImGui::GetColorU32(rgb(theme_.accent, 0.35f));
    }
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (bg)
        dl->AddRectFilled(p, ImVec2(p.x + width, p.y + height), bg, style.FrameRounding * 1.4f);
    if (selected && !primary)
        dl->AddRectFilled(ImVec2(p.x + pad * 0.5f, p.y + height - 3.0f * scale_),
                          ImVec2(p.x + width - pad * 0.5f, p.y + height), ImGui::GetColorU32(rgb(theme_.accent_hover)),
                          2.0f * scale_);
    float x = p.x + pad;
    if (icon_name) {
        const float y = p.y + (height - icon_size) * 0.5f;
        dl->AddImage(icon(icon_name), ImVec2(x, y), ImVec2(x + icon_size, y + icon_size), ImVec2(0, 0), ImVec2(1, 1), fg);
        x += icon_size + (label ? style.ItemInnerSpacing.x + 2.0f : 0.0f);
    }
    if (label)
        dl->AddText(ImVec2(x, p.y + (height - text.y) * 0.5f), fg, label);
    return clicked && enabled;
}

bool App::icon_button(const char* id, const char* icon_name, const char* tip, float size) {
    return button(id, icon_name, nullptr, false, false, true, tip, size);
}

// What each setting does, from the copy of docs/CONFIG.md the program carries.
const std::map<std::string, SettingDoc>& setting_docs() {
    static const std::map<std::string, SettingDoc> docs = parse_setting_docs(embedded("CONFIG.md"));
    return docs;
}

// Where the functions come from and the libraries' notices, from the copy of
// resources/credits.json the program carries (credits.md, rule 5); none when
// that copy does not read, which the tests rule out.
const Credits& credits() {
    static const Credits all = [] {
        try {
            return parse_credits(embedded("credits.json"));
        } catch (const CreditsError&) {
            return Credits{};
        }
    }();
    return all;
}

// The setting each row of the panel edits, for its info button and the
// search.
const std::map<T, const char*>& row_keys() {
    static const std::map<T, const char*> keys = {
        {T::LayerHeight, "layers.height"}, {T::FirstLayerHeight, "layers.first_height"},
        {T::LayerHeights, "layers.heights"},
        {T::DetailHeight, "layers.detail_height"}, {T::DetailTolerance, "layers.detail_tolerance"},
        {T::DetailMargin, "layers.detail_margin"}, {T::CuspHeight, "layers.cusp_height"},
        {T::Zaa, "zaa.enabled"}, {T::ZaaMinThickness, "zaa.min_thickness"}, {T::ZaaRaise, "zaa.raise"},
        {T::Walls, "walls.count"}, {T::LineWidth, "walls.width"}, {T::Seam, "walls.seam"},
        {T::FuzzySkin, "fuzzy.mode"}, {T::FuzzyThickness, "fuzzy.thickness"},
        {T::FuzzyDistance, "fuzzy.point_distance"}, {T::TopLayers, "surfaces.top_layers"},
        {T::BottomLayers, "surfaces.bottom_layers"}, {T::TopMinThickness, "surfaces.top_min_thickness"},
        {T::BottomMinThickness, "surfaces.bottom_min_thickness"}, {T::TopPattern, "surfaces.top_pattern"},
        {T::BottomPattern, "surfaces.bottom_pattern"}, {T::BridgeAngle, "surfaces.bridge_angle"},
        {T::InfillBeforeWalls, "infill.before_walls"}, {T::MaxLink, "infill.max_link"},
        {T::InfillDensity, "infill.density"}, {T::InfillPattern, "infill.pattern"}, {T::InfillAngle, "infill.angle"},
        {T::SolidEvery, "infill.solid_every"}, {T::SolidBelowArea, "infill.solid_below_area"},
        {T::NonplanarInfill, "infill.nonplanar"}, {T::NonplanarAmplitude, "infill.nonplanar_amplitude"},
        {T::NonplanarWavelength, "infill.nonplanar_wavelength"}, {T::CombineInfill, "infill.combine_layers"},
        {T::CombineAuto, "infill.combine_auto"}, {T::DenseUnderTop, "infill.dense_under_top"},
        {T::DenseDensity, "infill.dense_density"}, {T::StressDensity, "infill.stress_density"},
        {T::Supports, "support.enabled"}, {T::SupportAutomatic, "support.automatic"},
        {T::SupportBuildplateOnly, "support.buildplate_only"}, {T::SupportEnforceLayers, "support.enforce_layers"},
        {T::SupportClosingRadius, "support.closing_radius"}, {T::SupportFreeBridge, "support.free_bridge_length"},
        {T::SupportAngle, "support.threshold_angle"}, {T::SkirtLoops, "adhesion.skirt_loops"},
        {T::SkirtDistance, "adhesion.skirt_distance"}, {T::BrimWidth, "adhesion.brim_width"},
        {T::SpeedWalls, "speed.walls"}, {T::SpeedOuterWall, "speed.outer_wall"}, {T::SpeedInfill, "speed.infill"},
        {T::SpeedSolid, "speed.solid_infill"}, {T::SpeedTop, "speed.top_solid_infill"},
        {T::SpeedSupport, "speed.support"}, {T::SpeedTravel, "speed.travel"},
        {T::SpeedFirstLayer, "speed.first_layer"}, {T::SpeedMax, "speed.max"},
        {T::SpeedMaxFlow, "speed.max_flow"},
        {T::SpeedBridges, "speed.bridges"}, {T::SpeedOverBridges, "speed.over_bridges"},
        {T::SpeedSmallLoops, "speed.small_loops"},
        {T::SpeedSupportInterface, "speed.support_interface"}, {T::AccelWipeTower, "acceleration.wipe_tower"},
        {T::SpeedGapFill, "speed.gap_fill"}, {T::SpeedFirstLayerInfill, "speed.first_layer_infill"},
        {T::SpeedTravelZ, "speed.travel_z"}, {T::InfillMaxSpeed, "extrusion.infill_max_speed"},
        {T::InfillMaxCrossingSpeed, "extrusion.infill_max_crossing_speed"},
        {T::BridgeFlowRatio, "extrusion.bridge_flow_ratio"}, {T::AccelDefault, "acceleration.default"},
        {T::AccelWalls, "acceleration.walls"}, {T::AccelOuterWall, "acceleration.outer_wall"},
        {T::AccelInfill, "acceleration.infill"}, {T::AccelSolid, "acceleration.solid_infill"},
        {T::AccelTop, "acceleration.top_solid_infill"}, {T::AccelBridges, "acceleration.bridges"},
        {T::AccelFirstLayer, "acceleration.first_layer"}, {T::AccelTravel, "acceleration.travel"},
        {T::AccelShortTravel, "acceleration.short_travel"}, {T::DynamicOverhangs, "speed.dynamic_overhangs"},
        {T::Overhang0, "speed.overhang_0"}, {T::Overhang1, "speed.overhang_1"}, {T::Overhang2, "speed.overhang_2"},
        {T::Overhang3, "speed.overhang_3"}, {T::FlowSlopeUp, "speed.flow_slope_up"},
        {T::FlowSlopeDown, "speed.flow_slope_down"},
        {T::FilamentDiameter, "machine.filament_diameter"}, {T::NozzleTemp, "machine.variables"},
        {T::BedTemp, "machine.variables"}, {T::FlowRow, "extrusion.flow"}, {T::FanMin, "cooling.fan_min"},
        {T::FanMax, "cooling.fan_max"}, {T::MinLayerTime, "cooling.min_layer_time"},
        {T::KeepOuterSpeed, "cooling.keep_outer_speed"}, {T::BedWidth, "machine.bed"}, {T::BedDepth, "machine.bed"},
        {T::MaxHeight, "machine.max_height"}, {T::Nozzle, "machine.nozzle_diameter"},
        {T::MinLayerHeight, "machine.min_layer_height"}, {T::MaxLayerHeight, "machine.max_layer_height"},
        {T::Flavor, "machine.flavor"}, {T::RelativeE, "machine.relative_extrusion"},
        {T::Retraction, "retraction.length"}, {T::RetractSpeed, "retraction.speed"},
        {T::RetractLift, "retraction.lift"}, {T::GcodeResolution, "gcode.resolution"},
        {T::CornerSmoothing, "gcode.corner_smoothing"},
        {T::ScarfMode, "walls.scarf"}, {T::ScarfLength, "walls.scarf_length"},
        {T::ScarfEntire, "walls.scarf_entire_loop"}, {T::ScarfStart, "walls.scarf_start_height"},
        {T::ScarfSegment, "walls.scarf_max_segment"}, {T::ScarfSmooth, "walls.scarf_only_smooth"},
        {T::ScarfInner, "walls.scarf_inner"}, {T::DetectOverhangs, "walls.detect_overhangs"},
        {T::MinLineWidth, "walls.min_line_width"}, {T::MinDetail, "walls.min_detail"},
        {T::SeamGap, "walls.seam_gap"}, {T::OuterWidth, "walls.outer_width"}, {T::SpreadCount, "walls.spread_count"}, {T::TransitionLength, "walls.transition_length"}, {T::TransitionAngle, "walls.transition_angle"}, {T::FirstLineWidth, "layers.first_line_width"}, {T::TopWidth, "surfaces.top_width"}, {T::SolidWidth, "surfaces.solid_width"},
        {T::InfillWidth, "infill.width"},
        {T::WallGeneratorRow, "walls.generator"}, {T::WallGapFill, "walls.gap_fill"},
        {T::ZOffset, "machine.z_offset"}, {T::VolumetricE, "gcode.volumetric_e"},
        {T::VerboseComments, "gcode.verbose_comments"}, {T::GcodeArcs, "gcode.arcs"}, {T::GcodeBinary, "gcode.binary"},
        {T::ChamberMinTemp, "extrusion.chamber_min_temperature"}, {T::FilamentDensity, "extrusion.density"},
        {T::FilamentCost, "extrusion.cost"}, {T::SpoolWeight, "extrusion.spool_weight"},
        {T::ShrinkageXY, "extrusion.shrinkage_xy"}, {T::ShrinkageZ, "extrusion.shrinkage_z"},
        {T::ColorChangeGcode, "gcode.color_change"}, {T::PauseGcode, "gcode.pause"},
        {T::CustomTemplateGcode, "gcode.custom_template"},
        {T::BeforeLayerGcode, "gcode.before_layer"}, {T::AfterLayerGcode, "gcode.after_layer"},
        {T::RaftLayers, "adhesion.raft_layers"}, {T::RaftGap, "adhesion.raft_gap"},
        {T::RaftExpansion, "adhesion.raft_expansion"}, {T::RaftFirstDensity, "adhesion.raft_first_layer_density"},
        {T::RaftFirstExpansion, "adhesion.raft_first_layer_expansion"}, {T::SpeedOverRaft, "speed.first_layer_over_raft"},
        {T::AccelOverRaft, "acceleration.first_layer_over_raft"},
        {T::IroningEnabled, "ironing.enabled"}, {T::IroningSurfacesRow, "ironing.surfaces"},
        {T::IroningFlow, "ironing.flow"}, {T::IroningSpacing, "ironing.spacing"}, {T::IroningSpeed, "ironing.speed"}, {T::ScaleXRow, "compensation.scale_x"},
        {T::ScaleYRow, "compensation.scale_y"}, {T::ScaleZRow, "compensation.scale_z"},
        {T::HoleCorrection, "compensation.hole_offset"}, {T::XyOffset, "slicing.xy_offset"},
        {T::FirstLayerShrink, "slicing.first_layer_shrink"}, {T::FillModeRow, "slicing.fill_mode"},
        {T::GapClosing, "slicing.gap_closing_radius"}, {T::SimplifyTolerance, "slicing.simplify_tolerance"},
        {T::WallOverlap, "infill.wall_overlap"}, {T::CombineMaxHeight, "infill.combine_max_height"},
        {T::SupportZGap, "support.z_gap"}, {T::SupportBottomZGap, "support.bottom_z_gap"}, {T::SupportXyGap, "support.xy_gap"},
        {T::SupportSpacing, "support.spacing"}, {T::SupportInterface, "support.interface_layers"},
        {T::SupportBottomInterface, "support.bottom_interface_layers"},
        {T::SupportLineAngle, "support.line_angle"}, {T::SupportPatternRow, "support.pattern"},
        {T::SupportSheath, "support.sheath"}, {T::SupportInterfacePatternRow, "support.interface_pattern"},
        {T::SupportInterfaceSpacing, "support.interface_spacing"},
        {T::SupportContactLoops, "support.interface_contact_loops"},
        {T::SupportStyle, "support.style"}, {T::TreeAngle, "support.tree_angle"},
        {T::TreeAnglePreferred, "support.tree_angle_preferred"}, {T::TreeBranchDiameter, "support.tree_branch_diameter"},
        {T::TreeBranchAngle, "support.tree_branch_diameter_angle"},
        {T::TreeDoubleWall, "support.tree_double_wall_diameter"},
        {T::SupportWidth, "support.width"}, {T::ArcsEnabled, "arcs.enabled"}, {T::ArcsOverlap, "arcs.overlap"}, {T::ArcsSpeed, "arcs.speed"},
        {T::ArcsMaxDistance, "arcs.max_distance"},
        {T::NozzleForce, "review.nozzle_force"}, {T::BedAdhesion, "review.bed_adhesion"},
        {T::MaxDeflection, "review.max_deflection"},
        {T::TreeBranchDistance, "support.tree_branch_distance"}, {T::TreeTipDiameter, "support.tree_tip_diameter"},
        {T::TreeTopRate, "support.tree_top_rate"},
        {T::SkirtLayers, "adhesion.skirt_layers"}, {T::HeightChanges, "changes"},
        {T::SkirtMinFilament, "adhesion.skirt_min_filament"}, {T::AutoTemperatures, "gcode.auto_temperatures"},
        {T::FilamentStartGcode, "extrusion.start_gcode"}, {T::FilamentEndGcode, "extrusion.end_gcode"},
        {T::FilenameFormat, "output.filename_format"}, {T::PostProcessRow, "output.post_process"}, {T::DraftShieldRow, "adhesion.draft_shield"},
        {T::BrimTypeRow, "adhesion.brim_type"}, {T::BrimGap, "adhesion.brim_gap"},
        {T::ChamberTemp, "extrusion.chamber_temperature"}, {T::FanBelowTime, "cooling.fan_below_time"},
        {T::FanAlways, "cooling.fan_always"}, {T::FanBridges, "cooling.fan_bridges"},
        {T::FanOffLayers, "cooling.fan_off_layers"}, {T::MinSpeed, "cooling.min_speed"},
        {T::FullFanLayer, "cooling.full_fan_layer"}, {T::CoolingEnabled, "cooling.enabled"},
        {T::DynamicOverhangFan, "cooling.dynamic_overhang_fan"}, {T::OverhangFan0, "cooling.overhang_fan_0"},
        {T::OverhangFan1, "cooling.overhang_fan_1"}, {T::OverhangFan2, "cooling.overhang_fan_2"},
        {T::OverhangFan3, "cooling.overhang_fan_3"}, {T::SlowdownLogicRow, "cooling.slowdown_logic"},
        {T::TransitionDistance, "cooling.transition_distance"},
        {T::PressureAdvance, "extrusion.pressure_advance"}, {T::FilamentMaxFlow, "extrusion.max_flow"},
        {T::RetractMinTravel, "retraction.min_travel"}, {T::Abrasive, "extrusion.abrasive"},
        {T::UnretractSpeed, "retraction.unretract_speed"}, {T::RestartExtra, "retraction.restart_extra"},
        {T::RetractLayerChange, "retraction.on_layer_change"}, {T::WipeRow, "retraction.wipe"},
        {T::BeforeWipe, "retraction.before_wipe"},
        {T::Coast, "retraction.coast"}, {T::FirmwareRetraction, "retraction.firmware"},
        {T::LiftAbove, "retraction.lift_above"}, {T::LiftBelow, "retraction.lift_below"},
        {T::RampingLift, "travel.ramping_lift"}, {T::TravelSlope, "travel.slope"}, {T::MaxLift, "travel.max_lift"},
        {T::LiftBeforeObstacle, "travel.lift_before_obstacle"}, {T::AvoidCrossingWalls, "travel.avoid_crossing_walls"},
        {T::MaxDetour, "travel.max_detour"}, {T::WallsClockwise, "walls.clockwise"},
        {T::WallsOuterFirst, "walls.outer_first"},
        {T::FilamentRetractionRow, "extrusion.retraction"},
        {T::StaggeredSeams, "walls.staggered_inner_seams"}, {T::WallsStaggered, "walls.staggered"}, {T::TopOneWallRow, "walls.top_one_wall"},
        {T::FirstLayerOneWall, "walls.first_layer_one_wall"}, {T::ExtraIfNeeded, "walls.extra_if_needed"},
        {T::AvoidCurledOverhangs, "walls.avoid_curled_overhangs"}, {T::ExtraOnOverhangs, "walls.extra_on_overhangs"},
        {T::NeedsEnclosure, "extrusion.needs_enclosure"}, {T::Enclosed, "machine.enclosed"},
        {T::MaxChamberTemp, "machine.max_chamber_temp"}, {T::HardenedNozzle, "machine.hardened_nozzle"},
        {T::MachineMaxFlow, "machine.max_flow"}, {T::MaxFeedX, "machine.max_feed_x"},
        {T::MaxFeedY, "machine.max_feed_y"}, {T::MaxFeedZ, "machine.max_feed_z"},
        {T::MaxFeedE, "machine.max_feed_e"}, {T::CornerSpeed, "machine.corner_speed"},
        {T::AccelerationRow, "machine.acceleration"}, {T::TravelAcceleration, "machine.travel_acceleration"},
        {T::RetractAcceleration, "machine.retract_acceleration"}, {T::MaxAccelX, "machine.max_accel_x"},
        {T::MaxAccelY, "machine.max_accel_y"}, {T::MaxAccelZ, "machine.max_accel_z"},
        {T::MaxAccelE, "machine.max_accel_e"}, {T::JerkX, "machine.jerk_x"}, {T::JerkY, "machine.jerk_y"},
        {T::JerkZ, "machine.jerk_z"}, {T::JerkE, "machine.jerk_e"}, {T::JunctionDeviation, "machine.junction_deviation"},
        {T::MinExtrudeFeed, "machine.min_extrude_feed"}, {T::MinTravelFeed, "machine.min_travel_feed"},
        {T::LimitsUsageRow, "machine.limits_usage"}, {T::GcodeProgress, "gcode.progress"},
        {T::SilentModeRow, "machine.silent_mode"}, {T::SilentLimitsRow, "machine.silent"},
        {T::MaxNozzleTemp, "machine.max_nozzle_temp"}, {T::MaxBedTemp, "machine.max_bed_temp"},
        {T::VariablesRow, "machine.variables"}, {T::StartGcode, "machine.start_gcode"},
        {T::EndGcode, "machine.end_gcode"}, {T::VaseMode, "vase.enabled"}, {T::SequentialRow, "sequential.enabled"},
        {T::ClearanceRadius, "sequential.clearance_radius"}, {T::ClearanceHeight, "sequential.clearance_height"},
        {T::BetweenObjectsGcode, "gcode.between_objects"}, {T::ObjectLabelsRow, "gcode.object_labels"},
        {T::ExtrudersRow, "machine.extruders"}, {T::ExtruderOffsetRow, "machine.extruder_offset"},
        {T::ToolchangeLength, "retraction.toolchange_length"},
        {T::ToolchangeRestartExtra, "retraction.toolchange_restart_extra"}, {T::ToolchangeGcode, "gcode.toolchange"},
        {T::WallsExtruder, "extruders.walls"}, {T::InfillExtruder, "extruders.infill"},
        {T::SolidInfillExtruder, "extruders.solid_infill"}, {T::SupportExtruder, "extruders.support"},
        {T::SupportInterfaceExtruder, "extruders.support_interface"},
        {T::BedTemperatureFrom, "extruders.bed_temperature_from"}, {T::OozePrevention, "extruders.ooze_prevention"},
        {T::StandbyDelta, "extruders.standby_delta"}, {T::InterfaceShells, "surfaces.interface_shells"},
        {T::WipeTowerRow, "wipe_tower.enabled"}, {T::WipeTowerWidth, "wipe_tower.width"},
        {T::WipeTowerBrim, "wipe_tower.brim_width"}, {T::WipeTowerX, "wipe_tower.x"}, {T::WipeTowerY, "wipe_tower.y"},
        {T::WipeTowerRotation, "wipe_tower.rotation"}, {T::WipeTowerBridging, "wipe_tower.bridging"},
        {T::WipeTowerExtruder, "extruders.wipe_tower"}, {T::PurgeVolumesRow, "wipe_tower.purge_volumes"},
        {T::WipeTowerSpacing, "wipe_tower.extra_spacing"}, {T::WipeTowerFlow, "wipe_tower.extra_flow"},
        {T::WipeTowerCone, "wipe_tower.cone_angle"}, {T::WipeTowerNoSparse, "wipe_tower.no_sparse_layers"},
        {T::SingleExtruderMmu, "machine.single_extruder_mmu"}, {T::PurgeMultiplier, "extrusion.purge_multiplier"},
        {T::MinPurgeOnTower, "extrusion.min_purge_on_tower"}, {T::CoolingTubeLength, "mmu.cooling_tube_length"},
        {T::CoolingTubePosition, "mmu.cooling_tube_position"}, {T::ParkingPosition, "mmu.parking_position"},
        {T::ExtraLoadingMove, "mmu.extra_loading_move"}, {T::HighCurrentOnSwap, "mmu.high_current_on_swap"},
        {T::LoadingSpeed, "extrusion.loading_speed"}, {T::LoadingSpeedStart, "extrusion.loading_speed_start"},
        {T::UnloadingSpeed, "extrusion.unloading_speed"}, {T::UnloadingSpeedStart, "extrusion.unloading_speed_start"},
        {T::CoolingMoves, "extrusion.cooling_moves"}, {T::CoolingInitialSpeed, "extrusion.cooling_initial_speed"},
        {T::CoolingFinalSpeed, "extrusion.cooling_final_speed"}, {T::RammingRow, "extrusion.ramming"},
        {T::ToolchangeDelay, "extrusion.toolchange_delay"}, {T::LoadTime, "extrusion.load_time"},
        {T::UnloadTime, "extrusion.unload_time"}, {T::InterlockBeams, "interlocking.beams"},
        {T::InterlockWidth, "interlocking.beam_width"}, {T::InterlockLayers, "interlocking.beam_layers"},
        {T::InterlockOrientation, "interlocking.orientation"}, {T::InterlockDepth, "interlocking.depth"},
        {T::InterlockAvoidance, "interlocking.boundary_avoidance"}, {T::PaintMaxRegionWidth, "painting.max_region_width"},
        {T::PaintInterlockingDepth, "painting.interlocking_depth"}, {T::SyncSupportLayers, "support.synchronize_layers"},
        {T::FineWallsCoarseInfill, "multi.fine_walls_coarse_infill"}, {T::ColorMixingRow, "multi.color_mixing"},
        {T::CurvedModeRow, "curved.mode"}, {T::CurvedPatternRow, "curved.pattern"}, {T::CurvedFitHead, "curved.fit_head"},
        {T::CurvedAmplitude, "curved.amplitude"}, {T::CurvedWavelength, "curved.wavelength"},
        {T::CurvedAngle, "curved.angle"}, {T::CurvedTwist, "curved.twist"}, {T::CurvedConeAngle, "curved.cone_angle"},
        {T::CurvedFlatBelow, "curved.flat_below"}, {T::CurvedRampHeight, "curved.ramp_height"},
        {T::CurvedFlatTop, "curved.flat_top"}, {T::CurvedSegment, "curved.segment_length"},
        {T::CurvedMaxSlope, "curved.max_slope"}, {T::CurvedFlowPolicy, "curved.flow_policy"},
        {T::CurvedUniformFlow, "curved.uniform_flow"}, {T::CurvedOverhangThreshold, "curved.overhang_threshold"},
        {T::HeadFreeAngleX, "head.free_angle_x"},
        {T::HeadFreeAngleY, "head.free_angle_y"}, {T::HeadClearanceHeight, "head.clearance_height"},
        {T::HeadClearanceRadius, "head.clearance_radius"}, {T::HeadProfile, "head.profile"},
        {T::BeltEnabled, "belt.enabled"}, {T::BeltAngle, "belt.angle"}, {T::BeltIsZ, "belt.belt_is_z"},
        {T::InterlockWaves, "infill.waves"}, {T::WaveAmplitude, "infill.wave_amplitude"},
        {T::WaveLength, "infill.wave_length"}, {T::Technology, "machine.technology"},
        {T::DisplayPixelsX, "machine.display_pixels_x"}, {T::DisplayPixelsY, "machine.display_pixels_y"},
        {T::DisplayWidth, "machine.display_width"}, {T::DisplayHeight, "machine.display_height"},
        {T::DisplayMirrorX, "machine.display_mirror_x"}, {T::DisplayMirrorY, "machine.display_mirror_y"},
        {T::ResinLayerHeight, "resin.layer_height"}, {T::ResinExposure, "resin.exposure"},
        {T::ResinBottomExposure, "resin.bottom_exposure"}, {T::ResinBottomLayers, "resin.bottom_layers"},
        {T::ResinElephantFoot, "resin.elephant_foot"}, {T::ResinScaleX, "resin.scale_x"},
        {T::ResinScaleY, "resin.scale_y"}, {T::ResinScaleZ, "resin.scale_z"},
        {T::ResinAbsoluteCorrection, "resin.absolute_correction"}, {T::ResinZCorrection, "resin.z_correction_layers"},
        {T::ResinGamma, "resin.gamma"}, {T::ResinAntialiasing, "resin.antialiasing"},
        {T::ResinLiftDistance, "resin.lift_distance"}, {T::ResinLiftSpeed, "resin.lift_speed"},
        {T::ResinBottomLiftDistance, "resin.bottom_lift_distance"}, {T::ResinBottomLiftSpeed, "resin.bottom_lift_speed"},
        {T::ResinRetractSpeed, "resin.retract_speed"}, {T::ResinWaitAfterCure, "resin.wait_after_cure"},
        {T::ResinWaitAfterLift, "resin.wait_after_lift"}, {T::ResinWaitBeforeExposure, "resin.wait_before_exposure"},
        {T::ResinLightPwm, "resin.light_pwm"}, {T::ResinBottomLightPwm, "resin.bottom_light_pwm"},
        {T::ResinDensity, "resin.density"}, {T::ResinBottleCost, "resin.bottle_cost"},
        {T::ResinBottleVolume, "resin.bottle_volume"}, {T::ResinBottleWeight, "resin.bottle_weight"},
        {T::ResinPrinterName, "resin.printer_name"}, {T::ResinProfileName, "resin.profile_name"},
        {T::ResinSupports, "resin.supports"}, {T::ResinSupportDensity, "resin.support_density"},
        {T::ResinElevation, "resin.support_elevation"}, {T::ResinPillarDiameter, "resin.support_pillar_diameter"},
        {T::ResinTipDiameter, "resin.support_tip_diameter"}, {T::ResinTipPenetration, "resin.support_tip_penetration"},
        {T::ResinCriticalAngle, "resin.support_critical_angle"}, {T::ResinPad, "resin.pad"},
        {T::ResinPadAroundObject, "resin.pad_around_object"}, {T::ResinPadMargin, "resin.pad_margin"},
        {T::ResinPadThickness, "resin.pad_thickness"}, {T::ResinPadWallSlope, "resin.pad_wall_slope"},
        {T::ResinHollow, "resin.hollow"}, {T::ResinHollowWall, "resin.hollow_wall"},
        {T::ResinHollowClosing, "resin.hollow_closing"}, {T::ResinHollowInfill, "resin.hollow_infill"},
        {T::ResinHollowInfillCell, "resin.hollow_infill_cell"}};
    return keys;
}

const char* row_key(T label) {
    const auto it = row_keys().find(label);
    return it == row_keys().end() ? nullptr : it->second;
}

// A setting's row name, for the search and the list of settings to show; the
// key itself when no row names it.
std::string key_label(std::string_view key) {
    static const std::map<std::string, T, std::less<>> labels = [] {
        std::map<std::string, T, std::less<>> m;
        for (const auto& [label, k] : row_keys())
            m.emplace(k, label);
        m.emplace("machine.variables.temperature", T::NozzleTemp);
        m.emplace("machine.variables.bed_temperature", T::BedTemp);
        return m;
    }();
    const auto it = labels.find(key);
    return it == labels.end() ? std::string(key) : std::string(tr(it->second));
}

// The setting whose info card is kept open (--info, for screenshots).
std::string g_open_info;

// The help's data, for the glossary's definitions beside the settings
// (assistant.md, rule 12); none until it is read.
const Knowledge* g_knowledge = nullptr;
// assistant.glossary_tips: the glossary's definition next to the settings.
bool g_glossary_tips = true;

// The (i) beside a setting's name: a click opens what the setting does, its
// key, default and range (docs/UI.md, «Ayuda»).
void info_button(T label, const char* key) {
    const float size = ImGui::GetFontSize() * 0.95f;
    ImGui::SameLine(0.0f, size * 0.35f);
    ImGui::PushID(key);
    ImGui::PushID(static_cast<int>(label));
    const ImVec2 at = ImGui::GetCursorScreenPos();
    const float y = at.y + (ImGui::GetFrameHeight() - size) * 0.5f;
    ImGui::SetCursorScreenPos(ImVec2(at.x, y));
    // Asked from the command line, it stays open: the screenshot is taken
    // some frames later.
    if (g_open_info == key && !ImGui::IsPopupOpen("##info_card"))
        ImGui::SetScrollHereY(0.5f); // the row in sight, wherever it is in the panel
    if (ImGui::InvisibleButton("##info", ImVec2(size, size)) || (g_open_info == key && !ImGui::IsPopupOpen("##info_card")))
        ImGui::OpenPopup("##info_card");
    const bool hovered = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 colour = ImGui::GetColorU32(hovered ? ImGuiCol_CheckMark : ImGuiCol_TextDisabled);
    const ImVec2 c(at.x + size * 0.5f, y + size * 0.5f);
    dl->AddCircle(c, size * 0.45f, colour, 16, 1.2f);
    dl->AddLine(ImVec2(c.x, c.y - size * 0.05f), ImVec2(c.x, c.y + size * 0.25f), colour, 1.6f);
    dl->AddCircleFilled(ImVec2(c.x, c.y - size * 0.22f), size * 0.07f, colour);
    const KnowledgeTerm* term = g_knowledge && g_glossary_tips ? term_for_setting(*g_knowledge, key) : nullptr;
    if (hovered) {
        if (term) {
            // The glossary's definition, on the pointer (rule 12).
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 22.0f);
            loc::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark), "%s", term->term.c_str());
            ImGui::TextUnformatted(term->definition.c_str());
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        } else {
            loc::SetTooltip("%s", tr(T::Info));
        }
    }
    if (ImGui::BeginPopup("##info_card")) {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 24.0f);
        loc::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark), "%s", tr(label));
        const auto& docs = setting_docs();
        const auto doc = docs.find(key);
        if (label == T::NozzleTemp || label == T::BedTemp)
            ImGui::TextUnformatted(tr(T::InfoTemperatures));
        else if (doc != docs.end())
            ImGui::TextUnformatted(doc->second.meaning.c_str());
        ImGui::Spacing();
        loc::TextDisabled("%s: %s", tr(T::InfoKey), key);
        if (doc != docs.end() && !doc->second.default_value.empty())
            loc::TextDisabled("%s: %s", tr(T::InfoDefault), doc_value_text(doc->second.default_value).c_str());
        if (doc != docs.end() && !doc->second.range.empty())
            loc::TextDisabled("%s: %s", tr(T::InfoRange), doc_value_text(doc->second.range).c_str());
        if (language() == Language::English)
            loc::TextDisabled("%s", tr(T::InfoSpanish));
        if (term) {
            ImGui::SeparatorText(tr(T::HelpGlossary));
            ImGui::TextUnformatted((term->term + ": " + term->definition).c_str());
        }
        ImGui::PopTextWrapPos();
        ImGui::EndPopup();
    }
    ImGui::PopID();
    ImGui::PopID();
}

// A gray tag after the next row's label: the level of a changed setting that
// the panel's level would hide (settings_panel.md, rule 12).
const char* g_row_tag = nullptr;
// The next row's setting, as the panel asked whether to show it.
const char* g_row_key = nullptr;
// The setting the search went to (rule 5): its row lit in this colour, and
// brought into sight once.
std::string g_highlight_key;
ImU32 g_highlight_colour = 0;
bool g_scroll_highlight = false;
// In Custom, an eye on the hovered row hides its setting (rule 16): the key
// to hide, for the panel.
bool g_custom_rows = false;
std::string g_hide_key;

// The eye struck through, in a square of `size` at `at`.
void eye_struck(ImDrawList* dl, ImVec2 at, float size, ImU32 colour) {
    const ImVec2 c(at.x + size * 0.5f, at.y + size * 0.5f);
    dl->AddEllipse(c, ImVec2(size * 0.45f, size * 0.26f), colour, 0.0f, 20, 1.3f);
    dl->AddCircleFilled(c, size * 0.12f, colour);
    dl->AddLine(ImVec2(at.x + size * 0.12f, at.y + size * 0.88f), ImVec2(at.x + size * 0.88f, at.y + size * 0.12f), colour,
                1.5f);
}

// The printer card's drawings (settings_panel.md, rules 34 to 36): the
// engine's own, one ink on a soft background, in a square of `size` at `at`.
void draw_back(ImDrawList* dl, ImVec2 at, float size, ImU32 back) {
    dl->AddRectFilled(at, ImVec2(at.x + size, at.y + size), back, size * 0.18f);
}

void draw_printer(ImDrawList* dl, ImVec2 at, float size, PrinterKind kind, ImU32 ink, ImU32 back) {
    draw_back(dl, at, size, back);
    const auto p = [&](float x, float y) { return ImVec2(at.x + x * size, at.y + y * size); };
    const float w = std::max(1.0f, size * 0.045f);
    switch (kind) {
    case PrinterKind::Resin:
        // The lid over the vat, the plate hanging from its arm.
        dl->AddRect(p(0.25f, 0.14f), p(0.75f, 0.62f), ink, size * 0.06f, w);
        dl->AddLine(p(0.5f, 0.2f), p(0.5f, 0.42f), ink, w);
        dl->AddRectFilled(p(0.38f, 0.42f), p(0.62f, 0.47f), ink);
        dl->AddRectFilled(p(0.2f, 0.62f), p(0.8f, 0.84f), ink, size * 0.03f);
        break;
    case PrinterKind::Pellets:
    case PrinterKind::Open:
        // Two uprights, the gantry with its head, the bed.
        dl->AddLine(p(0.2f, 0.16f), p(0.2f, 0.82f), ink, w);
        dl->AddLine(p(0.8f, 0.16f), p(0.8f, 0.82f), ink, w);
        dl->AddLine(p(0.2f, 0.36f), p(0.8f, 0.36f), ink, w);
        dl->AddRectFilled(p(0.42f, 0.3f), p(0.58f, 0.46f), ink, size * 0.02f);
        dl->AddTriangleFilled(p(0.46f, 0.46f), p(0.54f, 0.46f), p(0.5f, 0.52f), ink);
        if (kind == PrinterKind::Pellets) // the hopper on the head
            dl->AddQuadFilled(p(0.4f, 0.12f), p(0.6f, 0.12f), p(0.55f, 0.3f), p(0.45f, 0.3f), ink);
        dl->AddRectFilled(p(0.14f, 0.7f), p(0.86f, 0.76f), ink, size * 0.02f);
        break;
    case PrinterKind::Enclosed:
        // A box with its door, the head inside.
        dl->AddRect(p(0.16f, 0.14f), p(0.84f, 0.86f), ink, size * 0.06f, w);
        dl->AddRect(p(0.24f, 0.26f), p(0.76f, 0.76f), ink, size * 0.04f, w * 0.7f);
        dl->AddRectFilled(p(0.43f, 0.34f), p(0.57f, 0.46f), ink, size * 0.02f);
        dl->AddRectFilled(p(0.3f, 0.64f), p(0.7f, 0.69f), ink);
        break;
    case PrinterKind::Delta:
        // Three towers, the arms down to the effector, a round bed.
        for (const float x : {0.2f, 0.5f, 0.8f})
            dl->AddLine(p(x, 0.12f), p(x, 0.78f), ink, w);
        dl->AddLine(p(0.2f, 0.3f), p(0.47f, 0.52f), ink, w * 0.8f);
        dl->AddLine(p(0.8f, 0.3f), p(0.53f, 0.52f), ink, w * 0.8f);
        dl->AddCircleFilled(p(0.5f, 0.54f), size * 0.05f, ink);
        dl->AddEllipseFilled(p(0.5f, 0.78f), ImVec2(size * 0.32f, size * 0.07f), ink);
        break;
    case PrinterKind::Belt:
        // The belt on its two rollers, the gantry leaning over it.
        dl->AddRect(p(0.1f, 0.66f), p(0.9f, 0.8f), ink, size * 0.07f, w);
        dl->AddCircleFilled(p(0.17f, 0.73f), size * 0.035f, ink);
        dl->AddCircleFilled(p(0.83f, 0.73f), size * 0.035f, ink);
        dl->AddLine(p(0.3f, 0.66f), p(0.62f, 0.18f), ink, w);
        dl->AddRectFilled(p(0.42f, 0.36f), p(0.56f, 0.48f), ink, size * 0.02f);
        break;
    }
}

void draw_bed(ImDrawList* dl, ImVec2 at, float size, BedFamily family, ImU32 ink, ImU32 back) {
    draw_back(dl, at, size, back);
    const auto p = [&](float x, float y) { return ImVec2(at.x + x * size, at.y + y * size); };
    const float w = std::max(1.0f, size * 0.045f);
    // The plate seen from a little above.
    dl->AddQuad(p(0.22f, 0.3f), p(0.86f, 0.3f), p(0.78f, 0.74f), p(0.14f, 0.74f), ink, w);
    switch (family) {
    case BedFamily::Textured:
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 3; ++j)
                dl->AddCircleFilled(p(0.3f + 0.13f * float(i) - 0.03f * float(j), 0.4f + 0.12f * float(j)),
                                    size * 0.022f, ink);
        break;
    case BedFamily::Smooth:
        dl->AddLine(p(0.32f, 0.62f), p(0.5f, 0.4f), ink, w * 0.6f);
        break;
    case BedFamily::Glass:
        dl->AddLine(p(0.3f, 0.66f), p(0.48f, 0.36f), ink, w);
        dl->AddLine(p(0.42f, 0.66f), p(0.6f, 0.36f), ink, w * 0.6f);
        break;
    case BedFamily::Cool:
        for (int i = 0; i < 3; ++i) {
            const float a = float(i) * 3.14159265f / 3.0f;
            const float dx = 0.13f * std::cos(a), dy = 0.13f * std::sin(a);
            dl->AddLine(p(0.5f - dx, 0.52f - dy), p(0.5f + dx, 0.52f + dy), ink, w);
        }
        break;
    case BedFamily::Engineering:
        dl->AddTriangleFilled(p(0.5f, 0.36f), p(0.6f, 0.64f), p(0.4f, 0.64f), ink);
        dl->AddCircleFilled(p(0.5f, 0.6f), size * 0.08f, ink);
        break;
    case BedFamily::Other:
        break;
    }
}

void draw_nozzle(ImDrawList* dl, ImVec2 at, float size, const NozzleLook& look, ImU32 ink, ImU32 back) {
    draw_back(dl, at, size, back);
    const auto p = [&](float x, float y) { return ImVec2(at.x + x * size, at.y + y * size); };
    // The body (longer for high flow), the hex, and the cone whose tip grows
    // with the diameter.
    const float bottom = look.high_flow ? 0.66f : 0.56f;
    dl->AddRectFilled(p(0.32f, 0.14f), p(0.68f, 0.3f), ink, size * 0.02f);
    dl->AddRectFilled(p(0.4f, 0.3f), p(0.6f, bottom), ink);
    const float tip = 0.03f + 0.09f * float(look.size);
    dl->AddQuadFilled(p(0.36f, bottom), p(0.64f, bottom), p(0.5f + tip, 0.84f), p(0.5f - tip, 0.84f), ink);
    if (look.hardened) // stripes across the body, in the background's colour
        for (int i = 0; i < 4; ++i) {
            const float y = 0.33f + 0.07f * float(i);
            dl->AddLine(p(0.4f, y + 0.05f), p(0.6f, y), back, std::max(1.0f, size * 0.035f));
        }
}

// A gear, for the printer's settings.
void draw_gear(ImDrawList* dl, ImVec2 c, float r, ImU32 ink) {
    for (int i = 0; i < 8; ++i) {
        const float a = float(i) * 3.14159265f / 4.0f;
        dl->AddLine(ImVec2(c.x + std::cos(a) * r * 0.55f, c.y + std::sin(a) * r * 0.55f),
                    ImVec2(c.x + std::cos(a) * r, c.y + std::sin(a) * r), ink, r * 0.32f);
    }
    dl->AddCircleFilled(c, r * 0.7f, ink);
}

// A calibration's name, as its card in Calibrate shows it.
const char* calibration_title(CalibrationKind kind) {
    switch (kind) {
    case CalibrationKind::Dimensions: return tr(T::CalDimensions);
    case CalibrationKind::Temperature: return tr(T::CalTemperature);
    case CalibrationKind::Retraction: return tr(T::CalRetraction);
    case CalibrationKind::Flow: return tr(T::CalFlow);
    case CalibrationKind::Speed: return tr(T::CalSpeed);
    case CalibrationKind::RetractionSpeed: return tr(T::CalRetractionSpeed);
    case CalibrationKind::PressureAdvance: return tr(T::CalPressureAdvance);
    case CalibrationKind::FirstLayer: return tr(T::CalFirstLayer);
    case CalibrationKind::Fit: return tr(T::CalFit);
    case CalibrationKind::MaxFlow: return tr(T::CalMaxFlow);
    case CalibrationKind::Acceleration: return tr(T::CalAcceleration);
    case CalibrationKind::ElephantFoot: return tr(T::CalElephantFoot);
    case CalibrationKind::ResinExposure: return tr(T::CalResinExposure);
    case CalibrationKind::ResinMeasure: return tr(T::CalResinMeasure);
    }
    return "";
}

// A nozzle's name: "0,4 mm" and, when known, "· hardened steel".
std::string nozzle_text(double diameter, bool hardened) {
    char text[32];
    std::snprintf(text, sizeof text, "%s", format("%g mm", diameter).c_str());
    std::string out = text;
    if (language() == Language::Spanish)
        std::replace(out.begin(), out.end(), '.', ',');
    if (hardened)
        out += std::string(" · ") + tr(T::HardenedSteel);
    return out;
}

// The drawing of a printer in the catalog, by what it says of itself.
PrinterKind catalog_kind(const PrinterModel& p) {
    if (p.kinematics == "belt")
        return PrinterKind::Belt;
    if (p.circular || p.kinematics == "delta")
        return PrinterKind::Delta;
    return p.enclosed ? PrinterKind::Enclosed : PrinterKind::Open;
}

// The printer's card on top of the panel (settings_panel.md, rules 21 and 32
// to 36): the printer with its drawing, the bed on printers with several, the
// gear to its settings; below, the nozzle, changed with one click.
void App::printer_card() {
    const std::size_t k = std::size_t(ProfileKind::Printer);
    const Profile* chosen = find_profile(ProfileKind::Printer, chosen_profile_[k]);
    const Catalog& catalog = builtin_catalog();
    const std::optional<CatalogMatch> match = chosen ? catalog_model_of(catalog, chosen->name) : std::nullopt;
    const float em = ImGui::GetFontSize(), gap = ImGui::GetStyle().ItemSpacing.x;
    const float pic = 3.0f * em, small = 2.0f * em, line = ImGui::GetFrameHeight();
    const ImU32 ink = ImGui::GetColorU32(rgb(theme_.accent_hover)), back = ImGui::GetColorU32(rgb(theme_.accent, 0.18f));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGui::PushID("##printer_card");

    // The printer.
    const ImVec2 at = ImGui::GetCursorScreenPos();
    draw_printer(dl, at, pic, printer_kind(settings_), ink, back);
    ImGui::Dummy(ImVec2(pic, pic));
    ImGui::SameLine(0, gap);
    ImGui::BeginGroup();
    static const std::vector<std::string> no_beds;
    const std::vector<std::string>& beds = match ? match->model->bed_surfaces : no_beds;
    const bool several_beds = beds.size() >= 2;
    const float rest = ImGui::GetContentRegionAvail().x - line - gap;
    std::string preview = chosen ? chosen->name : std::string(tr(T::NoProfile));
    if (chosen && profile_modified_[k])
        preview += " *";
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (pic - 2.0f * line - ImGui::GetStyle().ItemSpacing.y) * 0.5f);
    ImGui::SetNextItemWidth(several_beds ? rest * 0.6f : rest);
    const Profile* picked = nullptr;
    const PrinterModel* from_catalog = nullptr;
    if (ImGui::BeginCombo("##printer", preview.c_str(), ImGuiComboFlags_HeightLargest)) {
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::IsWindowAppearing())
            ImGui::SetKeyboardFocusHere();
        ImGui::InputTextWithHint("##find", tr(T::SearchPrinter), &printer_query_);
        const std::string query = fold(printer_query_);
        const auto wanted = [&](std::string_view name) { return query.empty() || fold(name).find(query) != std::string::npos; };
        // A row of the list: the drawing (2 em) and the name.
        const auto row = [&](const char* id, const std::string& name, PrinterKind kind, bool selected) {
            const ImVec2 r = ImGui::GetCursorScreenPos();
            const bool hit = ImGui::Selectable(id, selected, 0, ImVec2(0, small));
            draw_printer(ImGui::GetWindowDrawList(), r, small, kind, ink, back);
            ImGui::GetWindowDrawList()->AddText(ImVec2(r.x + small + gap, r.y + (small - em) * 0.5f),
                                                ImGui::GetColorU32(ImGuiCol_Text), name.c_str());
            return hit;
        };
        // The person's printers first, then the engine's, then the catalog by brand.
        if (std::any_of(user_profiles_[k].begin(), user_profiles_[k].end(), [&](const Profile& p) { return wanted(p.name); })) {
            ImGui::SeparatorText(tr(T::MyPrinters));
            for (const Profile& p : user_profiles_[k])
                if (wanted(p.name) && row(("##u" + p.name).c_str(), p.name, printer_kind(p.settings), chosen == &p))
                    picked = &p;
        }
        if (std::any_of(builtin_profiles_[k].begin(), builtin_profiles_[k].end(), [&](const Profile& p) { return wanted(p.name); })) {
            ImGui::SeparatorText(tr(T::BuiltinProfiles));
            for (const Profile& p : builtin_profiles_[k])
                if (wanted(p.name) && row(("##b" + p.name).c_str(), p.name, printer_kind(p.settings), chosen == &p))
                    picked = &p;
        }
        ImGui::SeparatorText(tr(T::FromCatalog));
        for (std::size_t i = 0; i < catalog.printers.size();) {
            const std::string& vendor = catalog.printers[i].vendor;
            std::size_t j = i;
            std::vector<const PrinterModel*> models;
            for (; j < catalog.printers.size() && catalog.printers[j].vendor == vendor; ++j)
                if (wanted(vendor + " " + catalog.printers[j].model))
                    models.push_back(&catalog.printers[j]);
            if (!models.empty()) {
                if (!query.empty())
                    ImGui::SetNextItemOpen(true);
                if (ImGui::TreeNode(vendor.c_str())) {
                    for (const PrinterModel* m : models)
                        if (row(("##c" + vendor + m->model).c_str(), m->model, catalog_kind(*m), false))
                            from_catalog = m;
                    ImGui::TreePop();
                }
            }
            i = j;
        }
        ImGui::Separator();
        if (ImGui::Selectable(tr(T::AddPrinter)))
            catalog_kind_ = ProfileKind::Printer, catalog_open_ = true;
        ImGui::EndCombo();
    }
    if (picked) {
        choose_profile(ProfileKind::Printer, *picked);
        bed_type_.clear();
    }
    if (from_catalog) {
        // Its stock nozzle; the profile made before, if there is one.
        const std::string name = printer_profile_name(*from_catalog, from_catalog->nozzles.front());
        if (const Profile* p = find_profile(ProfileKind::Printer, name))
            choose_profile(ProfileKind::Printer, *p);
        else
            add_catalog_profile(printer_profile(*from_catalog, from_catalog->nozzles.front()));
        bed_type_.clear();
    }
    // The bed, on printers with several (rule 32): the maker's names.
    if (several_beds) {
        if (std::find(beds.begin(), beds.end(), bed_type_) == beds.end())
            bed_type_ = beds.front();
        ImGui::SameLine(0, gap);
        const ImVec2 b = ImGui::GetCursorScreenPos();
        draw_bed(dl, b, line, bed_family(bed_type_), ink, back);
        ImGui::Dummy(ImVec2(line, line));
        ImGui::SameLine(0, gap * 0.5f);
        ImGui::SetNextItemWidth(rest * 0.4f - line - gap * 1.5f);
        if (ImGui::BeginCombo("##bed", bed_type_.c_str())) {
            for (const std::string& bed : beds) {
                const ImVec2 r = ImGui::GetCursorScreenPos();
                if (ImGui::Selectable(("##" + bed).c_str(), bed == bed_type_, 0, ImVec2(0, small)))
                    bed_type_ = bed;
                draw_bed(ImGui::GetWindowDrawList(), r, small, bed_family(bed), ink, back);
                ImGui::GetWindowDrawList()->AddText(ImVec2(r.x + small + gap, r.y + (small - em) * 0.5f),
                                                    ImGui::GetColorU32(ImGuiCol_Text), bed.c_str());
            }
            ImGui::EndCombo();
        }
        if (ImGui::IsItemHovered())
            loc::SetTooltip("%s", tr(T::BedType));
    }
    // The gear: the printer's settings (rule 37).
    ImGui::SameLine(0, gap);
    {
        const ImVec2 g = ImGui::GetCursorScreenPos();
        if (ImGui::InvisibleButton("##gear", ImVec2(line, line)))
            printer_page_ = true;
        const bool hovered = ImGui::IsItemHovered();
        draw_gear(dl, ImVec2(g.x + line * 0.5f, g.y + line * 0.5f), line * 0.38f,
                  ImGui::GetColorU32(rgb(hovered ? theme_.accent_hover : theme_.text_dim)));
        dl->AddCircleFilled(ImVec2(g.x + line * 0.5f, g.y + line * 0.5f), line * 0.12f, ImGui::GetColorU32(rgb(theme_.panel)));
        if (hovered)
            loc::SetTooltip("%s", tr(T::PrinterSettingsTip));
    }

    // The nozzle (rules 21 to 23).
    const std::vector<NozzleChoice> nozzles =
        nozzle_choices(catalog, chosen ? chosen->name : std::string(), settings_.nozzle_diameter);
    {
        const ImVec2 n = ImGui::GetCursorScreenPos();
        draw_nozzle(dl, n, line, nozzle_look(settings_.nozzle_diameter, settings_.hardened_nozzle), ink, back);
        ImGui::Dummy(ImVec2(line, line));
        ImGui::SameLine(0, gap * 0.5f);
    }
    ImGui::SetNextItemWidth(-FLT_MIN);
    bool other = false;
    if (ImGui::BeginCombo("##nozzle", nozzle_text(settings_.nozzle_diameter, settings_.hardened_nozzle).c_str())) {
        for (const NozzleChoice& c : nozzles) {
            const ImVec2 r = ImGui::GetCursorScreenPos();
            const bool now = std::abs(c.diameter - settings_.nozzle_diameter) < 1e-9;
            if (ImGui::Selectable(("##n" + std::to_string(c.diameter)).c_str(), now, 0, ImVec2(0, small)) && !now)
                choose_nozzle(c.diameter, c.profile);
            draw_nozzle(ImGui::GetWindowDrawList(), r, small, nozzle_look(c.diameter, now && settings_.hardened_nozzle),
                        ink, back);
            ImGui::GetWindowDrawList()->AddText(ImVec2(r.x + small + gap, r.y + (small - em) * 0.5f),
                                                ImGui::GetColorU32(ImGuiCol_Text),
                                                nozzle_text(c.diameter, now && settings_.hardened_nozzle).c_str());
        }
        // A printer not from the catalog: another size (rule 23).
        if (nozzles.size() == 1 && nozzles.front().profile.empty() && ImGui::Selectable(tr(T::OtherNozzle)))
            other = true;
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered())
        loc::SetTooltip("%s", tr(T::Nozzle));
    if (other) {
        other_nozzle_ = settings_.nozzle_diameter;
        ImGui::OpenPopup("##other_nozzle");
    }
    if (ImGui::BeginPopup("##other_nozzle")) {
        ImGui::TextUnformatted(tr(T::OtherNozzleTitle));
        ImGui::SetNextItemWidth(120.0f * scale_);
        ImGui::InputDouble("mm", &other_nozzle_, 0.05, 0.1, "%.2f");
        other_nozzle_ = std::clamp(other_nozzle_, 0.1, 3.0);
        if (ImGui::Button(tr(T::OkButton))) {
            choose_nozzle(other_nozzle_, {});
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::EndGroup();
    ImGui::PopID();
    ImGui::Spacing();
}

// One click to another nozzle (rule 22): the printer's profile for it (made
// from the catalog when there is none yet), or, for a printer of its own,
// the size alone, the profile marked changed (rule 23). A toast says so,
// with Undo.
void App::choose_nozzle(double diameter, const std::string& profile) {
    const Settings before = settings_;
    const std::array<std::string, 3> profiles = chosen_profile_;
    if (!profile.empty()) {
        if (const Profile* p = find_profile(ProfileKind::Printer, profile))
            choose_profile(ProfileKind::Printer, *p);
        else if (const std::optional<CatalogMatch> match = catalog_model_of(builtin_catalog(), profile))
            add_catalog_profile(printer_profile(*match->model, diameter));
    } else {
        settings_.nozzle_diameter = diameter;
        bed_changed();
        settings_changed();
    }
    notify_undo(format(tr(T::NozzleChanged), nozzle_text(diameter, false).c_str()), [this, before, profiles] {
        settings_ = before;
        chosen_profile_ = profiles;
        profiles_checked_.clear();
        bed_changed();
        settings_changed();
    });
}

// A calibration's result applied: its day, the nozzle and the filament, for
// the printer page (rule 38).
void App::record_calibration(CalibrationKind kind) {
    const std::string name(calibration_kind_name(kind));
    record_calibration(name);
    // Its guides too, by their id (assistant.md, rule 22).
    if (knowledge_)
        for (const KnowledgeCalibration& g : knowledge_->calibrations)
            if (test_for_guide(g.id) == name)
                record_calibration(g.id);
}

void App::record_calibration(const std::string& name) {
    const std::string& printer = chosen_profile_[std::size_t(ProfileKind::Printer)];
    if (printer.empty())
        return;
    const std::time_t now = std::time(nullptr);
    char date[16] = "";
    if (const std::tm* t = std::localtime(&now))
        std::strftime(date, sizeof date, "%Y-%m-%d", t);
    // In the profile the result belongs to (assistant.md, rule 22): a
    // filament guide's or test's in the filament's, the rest in the printer's.
    bool filament = false;
    if (const KnowledgeCalibration* g = knowledge_ ? find_guide(*knowledge_, name) : nullptr)
        filament = g->group == "filament";
    else if (const auto kind = calibration_kind_from_name(name))
        filament = filament_calibration(*kind);
    date_calibration(settings_, name, filament, date, printer);
}

// The printer's own page (rules 37 to 40), the whole width: back to the
// plate, its profile, and its sections: the machine, the nozzle, the limits,
// the G-code, its calibrations, its connection and the person's notes.
void App::printer_page() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float top = kTopBar * scale_, bottom = kStatusBar * scale_;
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x, vp->Pos.y + top));
    ImGui::SetNextWindowSize(ImVec2(vp->Size.x, vp->Size.y - top - bottom));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, rgb(theme_.panel));
    ImGui::Begin("##printer_page", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);
    bool changed = false, bed = false;
    refresh_profiles();
    if (ImGui::Button(tr(T::BackToPlate)) ||
        (ImGui::IsKeyPressed(ImGuiKey_Escape) && !ImGui::IsAnyItemActive() &&
         !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)))
        printer_page_ = false;
    ImGui::SameLine();
    ImGui::PushFont(nullptr, kTitleSize * 0.8f);
    ImGui::TextUnformatted(tr(T::PrinterSettingsTitle));
    ImGui::PopFont();
    {
        const float w = std::min(ImGui::GetContentRegionAvail().x, 520.0f * scale_);
        ImGui::BeginChild("##printer_profile", ImVec2(w, ImGui::GetFrameHeightWithSpacing()));
        profile_row(ProfileKind::Printer);
        ImGui::EndChild();
    }
    ImGui::Separator();

    // The sections: the printer's settings, then its calibrations, its
    // connection and the notes.
    const std::vector<SectionLayout>& layout = settings_layout(SettingsTab::Printer);
    std::vector<const SectionLayout*> sections;
    for (const SectionLayout& section : layout)
        if (std::any_of(section.subsections.begin(), section.subsections.end(),
                        [&](const SubsectionLayout& sub) { return card_shown(sub); }))
            sections.push_back(&section);
    struct Extra {
        const char* id;
        T title;
        const char* icon;
    };
    static constexpr Extra extras[] = {{"calibration", T::SecCalibration, "tisma_calibrate"},
                                       {"connection", T::SecConnection, "tisma_printer"},
                                       {"notes", T::SecNotes, "section_other"}};
    const bool known = std::any_of(sections.begin(), sections.end(), [&](const SectionLayout* s) { return s->id == printer_section_; }) ||
                       std::any_of(std::begin(extras), std::end(extras), [&](const Extra& e) { return printer_section_ == e.id; });
    if (!known)
        printer_section_ = sections.empty() ? "calibration" : std::string(sections.front()->id);
    // The column as wide as its longest name.
    float column = 84.0f * scale_;
    {
        const float small = ImGui::GetFontSize() * 0.78f;
        const auto fit = [&](const char* name) {
            column = std::max(column, ImGui::GetFont()->CalcTextSizeA(small, FLT_MAX, 0.0f, name).x + 18.0f * scale_);
        };
        for (const SectionLayout* section : sections)
            fit(tr(section->title));
        for (const Extra& e : extras)
            fit(tr(e.title));
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::BeginChild("##printer_sections", ImVec2(column, 0), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
    for (const SectionLayout* section : sections) {
        const bool marked = std::any_of(section->subsections.begin(), section->subsections.end(), [&](const SubsectionLayout& sub) {
            return std::any_of(sub.settings.begin(), sub.settings.end(),
                               [&](const SettingEntry& e) { return changed_keys_.contains(std::string(e.key)); });
        });
        const std::string icon_name(section->icon.substr(0, section->icon.size() - 4));
        if (section_button(section->id, tr(section->title), icon_name.c_str(), section->id == printer_section_, marked, column))
            printer_section_ = std::string(section->id), show_all_cards_.clear(), revealed_keys_.clear();
    }
    for (const Extra& e : extras)
        if (section_button(e.id, tr(e.title), e.icon, printer_section_ == e.id, false, column))
            printer_section_ = e.id;
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::SameLine(0, 8.0f * scale_);

    ImGui::BeginChild("##printer_cards", ImVec2(std::min(ImGui::GetContentRegionAvail().x, 720.0f * scale_), 0));
    g_custom_rows = view_level_ == ViewLevel::Custom;
    for (const SectionLayout* section : sections)
        if (section->id == printer_section_)
            section_cards(*section, changed, bed);
    g_custom_rows = false;
    const std::string& printer = chosen_profile_[std::size_t(ProfileKind::Printer)];
    if (printer_section_ == "calibration") {
        // Each calibration this printer takes: what it fixes, its value now,
        // whether it was made and with what (rule 38).
        const std::vector<CalibrationKind> kinds = printer_calibrations(settings_);
        // The profiles' dates; those an older version kept in the
        // preferences, while the profiles have none.
        const auto records = calibration_records_.find(printer);
        std::map<CalibrationKind, CalibrationRecord> found;
        for (const CalibrationKind kind : kinds)
            if (const std::optional<CalibrationRecord> r = calibration_record(
                    settings_, calibration_kind_name(kind), printer, chosen_profile_[std::size_t(ProfileKind::Filament)]))
                found[kind] = *r;
            else if (records != calibration_records_.end())
                if (const auto it = records->second.find(calibration_kind_name(kind)); it != records->second.end())
                    found[kind] = it->second;
        const auto record_of = [&](CalibrationKind kind) -> const CalibrationRecord* {
            const auto it = found.find(kind);
            return it == found.end() ? nullptr : &it->second;
        };
        int done = 0;
        std::optional<CalibrationKind> next;
        for (const CalibrationKind kind : kinds) {
            if (calibration_state(record_of(kind), settings_.nozzle_diameter) == CalibrationState::Done)
                ++done;
            else if (!next)
                next = kind;
        }
        loc::Text(tr(T::CalibratedCount), done, int(kinds.size()));
        ImGui::SameLine();
        if (next && ImGui::SmallButton(tr(T::CalibrateInOrder))) {
            printer_page_ = false;
            set_tab(Tab::Calibration);
            create_calibration(*next, false);
        }
        ImGui::Spacing();
        const std::map<std::string, std::string> values = [&] {
            std::map<std::string, std::string> m;
            for (auto& [key, value] : describe(settings_))
                m[key] = std::move(value);
            return m;
        }();
        if (ImGui::BeginTable("##calibrations", 4, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) {
            for (const CalibrationKind kind : kinds) {
                ImGui::PushID(int(kind));
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(calibration_title(kind));
                ImGui::TableNextColumn();
                if (const std::string_view key = calibration_key(kind); !key.empty()) {
                    std::string value;
                    if (key == "machine.variables.temperature") {
                        const auto t = settings_.variables.find("temperature");
                        value = t != settings_.variables.end() && !t->second.empty() ? t->second.front() + " °C" : "—";
                    } else if (const auto v = values.find(std::string(key)); v != values.end()) {
                        const bool number = !v->second.empty() &&
                                            v->second.find_first_not_of("0123456789.-+eE") == std::string::npos;
                        value = v->second == "null" ? "—" : number ? localized(v->second) : v->second;
                    }
                    loc::TextDisabled("%s: %s", key_label(key).c_str(), value.c_str());
                }
                ImGui::TableNextColumn();
                const CalibrationRecord* record = record_of(kind);
                switch (calibration_state(record, settings_.nozzle_diameter)) {
                case CalibrationState::Missing:
                    loc::TextDisabled("%s", tr(T::NotCalibrated));
                    break;
                case CalibrationState::Done:
                    loc::TextColored(rgb(0x3FBF7F), tr(T::CalibratedOn), record->date.c_str());
                    break;
                case CalibrationState::OtherNozzle:
                    loc::TextColored(rgb(0xE8B83D), tr(T::CalibratedOtherNozzle),
                                       nozzle_text(record->nozzle, false).c_str());
                    break;
                }
                if (record && filament_calibration(kind) && !record->filament.empty()) {
                    ImGui::SameLine();
                    loc::TextDisabled(tr(T::CalibratedWith), record->filament.c_str());
                }
                ImGui::TableNextColumn();
                if (ImGui::SmallButton(tr(T::CalibrateButton))) {
                    printer_page_ = false;
                    set_tab(Tab::Calibration);
                    create_calibration(kind, false);
                }
                if (const std::string_view guide = guide_for_test(calibration_kind_name(kind));
                    !guide.empty() && knowledge_ && find_guide(*knowledge_, guide)) {
                    ImGui::SameLine();
                    if (ImGui::SmallButton(tr(T::GuideHow)))
                        open_guide(guide);
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    } else if (printer_section_ == "connection") {
        // The connection is the Printer tab's for now (rule 43 waits for the
        // network connections).
        const bool connected = printer_ && printer_->status().state != device::PrinterState::Disconnected &&
                               printer_->status().state != device::PrinterState::Error;
        ImGui::PushTextWrapPos(0.0f);
        if (connected)
            loc::Text(tr(T::ConnectionNow), device_port_ == "sim" ? tr(T::DeviceSimulated) : device_port_.c_str());
        else
            loc::TextDisabled("%s", tr(T::ConnectionNone));
        ImGui::PopTextWrapPos();
        if (ImGui::Button(tr(T::OpenInDevices))) {
            printer_page_ = false;
            set_tab(Tab::Device);
        }
    } else if (printer_section_ == "notes") {
        std::string& notes = printer_notes_[printer];
        ImGui::InputTextMultiline("##notes", &notes, ImVec2(-FLT_MIN, 18.0f * ImGui::GetTextLineHeight()));
        if (notes.empty() && !ImGui::IsItemActive()) {
            const ImVec2 at = ImGui::GetItemRectMin();
            ImGui::GetWindowDrawList()->AddText(ImVec2(at.x + 8.0f * scale_, at.y + 6.0f * scale_),
                                                ImGui::GetColorU32(ImGuiCol_TextDisabled), tr(T::NotesHint));
        }
    }
    ImGui::EndChild();
    ImGui::End();
    ImGui::PopStyleColor();
    if (bed)
        bed_changed();
    if (changed)
        settings_changed();
}


// Rows of the settings tables: a label (with a tooltip and, for a setting of
// the engine, its info button) and a control.
void label_cell(T label, const char* tip = nullptr) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    const std::string key = g_row_key ? g_row_key : row_key(label) ? row_key(label) : "";
    g_row_key = nullptr;
    if (!key.empty() && key == g_highlight_key) {
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, g_highlight_colour);
        if (g_scroll_highlight) {
            ImGui::SetScrollHereY(0.35f);
            g_scroll_highlight = false;
        }
    }
    const ImVec2 cell = ImGui::GetCursorScreenPos();
    const float column = ImGui::GetContentRegionAvail().x;
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(tr(label));
    if (tip && ImGui::IsItemHovered())
        loc::SetTooltip("%s", tip);
    if (const char* info = row_key(label))
        info_button(label, info);
    if (g_row_tag) {
        ImGui::SameLine();
        loc::TextDisabled("%s", g_row_tag);
        if (ImGui::IsItemHovered())
            loc::SetTooltip("%s", tr(T::HiddenChanged));
        g_row_tag = nullptr;
    }
    const float tall = ImGui::GetFrameHeight();
    if (g_custom_rows && !key.empty() && ImGui::IsWindowHovered() &&
        ImGui::IsMouseHoveringRect(cell, ImVec2(cell.x + column, cell.y + tall))) {
        const float size = tall * 0.72f;
        const ImVec2 at(cell.x + column - size, cell.y + (tall - size) * 0.5f);
        ImGui::SetCursorScreenPos(at);
        ImGui::PushID(key.c_str());
        if (ImGui::InvisibleButton("##hide", ImVec2(size, size)))
            g_hide_key = key;
        const bool hovered = ImGui::IsItemHovered();
        eye_struck(ImGui::GetWindowDrawList(), at, size, ImGui::GetColorU32(hovered ? ImGuiCol_CheckMark : ImGuiCol_TextDisabled));
        if (hovered)
            loc::SetTooltip("%s", tr(T::HideSetting));
        ImGui::PopID();
    }
    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(-FLT_MIN);
}

// Each row's ID: its label and the address of its value. The address alone
// is not enough: a local copy (a start G-code variable, a percentage) can sit
// in the same stack slot as another row's, and the two would move together.
struct RowId {
    RowId(T label, const void* v) {
        ImGui::PushID(static_cast<int>(label));
        ImGui::PushID(v);
    }
    ~RowId() {
        ImGui::PopID();
        ImGui::PopID();
    }
    RowId(const RowId&) = delete;
    RowId& operator=(const RowId&) = delete;
};

bool row(T label, double& v, double min, double max, const char* fmt, float speed = 0.01f, const char* tip = nullptr) {
    label_cell(label, tip);
    const RowId id(label, &v);
    const bool changed =
        ImGui::DragScalar("##v", ImGuiDataType_Double, &v, speed, &min, &max, field_text("##v", fmt, v), ImGuiSliderFlags_AlwaysClamp);
    return changed;
}

bool row(T label, int& v, int min, int max, const char* fmt = "%d", const char* tip = nullptr) {
    label_cell(label, tip);
    const RowId id(label, &v);
    const bool changed = ImGui::DragInt("##v", &v, 0.1f, min, max, fmt, ImGuiSliderFlags_AlwaysClamp);
    return changed;
}

bool row(T label, bool& v, const char* tip = nullptr) {
    label_cell(label, tip);
    const RowId id(label, &v);
    const bool changed = ImGui::Checkbox("##v", &v);
    return changed;
}

template <class Enum>
bool row(T label, Enum& v, std::initializer_list<std::pair<Enum, const char*>> options, const char* tip = nullptr) {
    label_cell(label, tip);
    const char* current = "";
    for (const auto& [value, name] : options)
        if (value == v)
            current = name;
    bool changed = false;
    const RowId id(label, &v);
    if (ImGui::BeginCombo("##v", current)) {
        for (const auto& [value, name] : options)
            if (ImGui::Selectable(name, value == v)) {
                changed = value != v;
                v = value;
            }
        ImGui::EndCombo();
    }
    return changed;
}

// The layer heights by Z (docs/VARIABLE_LAYERS.md): a row per step, its Z and
// its height, sorted by Z once no field is being edited.
bool height_steps_rows(std::vector<std::array<double, 2>>& steps, double layer_height) {
    label_cell(T::LayerHeights, tr(T::LayerHeightsTip));
    bool changed = false;
    if (ImGui::Button(tr(T::AddStep), ImVec2(-FLT_MIN, 0))) {
        steps.push_back({steps.empty() ? 1.0 : steps.back()[0] + 1.0, layer_height});
        changed = true;
    }
    std::size_t remove = steps.size();
    const double z_min = 0.0, z_max = 10000.0, h_min = 0.01, h_max = 10.0;
    for (std::size_t i = 0; i < steps.size(); ++i) {
        ImGui::PushID(int(i));
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        changed |= ImGui::DragScalar("##z", ImGuiDataType_Double, &steps[i][0], 0.05f, &z_min, &z_max, field_text("##z", "Z %.2f mm", steps[i][0]),
                                     ImGuiSliderFlags_AlwaysClamp);
        ImGui::TableNextColumn();
        const float button = ImGui::GetFrameHeight();
        ImGui::SetNextItemWidth(-(button + ImGui::GetStyle().ItemSpacing.x));
        changed |= ImGui::DragScalar("##h", ImGuiDataType_Double, &steps[i][1], 0.002f, &h_min, &h_max, field_text("##h", "%.3f mm", steps[i][1]),
                                     ImGuiSliderFlags_AlwaysClamp);
        ImGui::SameLine();
        if (ImGui::Button("x", ImVec2(button, button)))
            remove = i;
        ImGui::PopID();
    }
    if (remove < steps.size()) {
        steps.erase(steps.begin() + std::ptrdiff_t(remove));
        changed = true;
    }
    if (!ImGui::IsAnyItemActive()) {
        // Sorted, and apart: the settings file takes each Z once.
        std::stable_sort(steps.begin(), steps.end(), [](const auto& a, const auto& b) { return a[0] < b[0]; });
        for (std::size_t i = 1; i < steps.size(); ++i)
            if (steps[i][0] <= steps[i - 1][0])
                steps[i][0] = steps[i - 1][0] + 0.01, changed = true;
    }
    return changed;
}

// A whole-number variable of the start G-code ([temperature]), edited as a number.
bool variable_row(T label, Settings& s, const char* name, int min, int max, const char* fmt, const char* tip) {
    std::vector<std::string>& values = s.variables[name];
    if (values.empty())
        values.push_back("0");
    int v = std::atoi(values.front().c_str());
    if (!row(label, v, min, max, fmt, tip))
        return false;
    values.front() = std::to_string(v);
    return true;
}

// The bed as width and depth when it is a rectangle from the origin.
bool rectangle_bed(const Settings& s, double& width, double& depth) {
    if (s.bed.size() != 4)
        return false;
    width = s.bed[2][0], depth = s.bed[2][1];
    return s.bed[0] == std::array<double, 2>{0.0, 0.0} && s.bed[1] == std::array<double, 2>{width, 0.0} &&
           s.bed[3] == std::array<double, 2>{0.0, depth} && width > 0.0 && depth > 0.0;
}

bool App::init(std::string& error) {
    glfwSetErrorCallback([](int code, const char* message) { std::fprintf(stderr, "glfw %d: %s\n", code, message); });
    if (!glfwInit()) {
        error = "cannot start GLFW (is there a display?)";
        return false;
    }
    const auto open_window = [&](bool vulkan) {
        glfwDefaultWindowHints();
        if (vulkan) {
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        } else {
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
            glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
        }
        glfwWindowHint(GLFW_SCALE_TO_MONITOR, options_.screenshot.empty() ? GLFW_TRUE : GLFW_FALSE);
        window_ = glfwCreateWindow(options_.width, options_.height, "Tisma", nullptr, nullptr);
    };
    // Vulkan when this machine has it (found by starting it: the loader, a
    // Vulkan 1.1 device, a surface for this window, the 3D view's shaders
    // and targets); else, and as the fallback, OpenGL 3.3. Automatically
    // only once the Vulkan view does all the OpenGL one does (PLAN.md «Vista
    // con Vulkan»): until then «auto» is OpenGL.
    constexpr bool kVulkanViewReady = false;
    if (options_.graphics == "vulkan" || (options_.graphics == "auto" && kVulkanViewReady)) {
        std::string why = "cannot open a window for Vulkan";
        open_window(true);
        vulkan_ = window_ && vk_.init(window_, true, why) && vk_view_.init(*vk_.device(), why);
        if (!vulkan_) {
            vk_view_.shutdown();
            vk_.shutdown();
            // Vulkan asked for in the CI, which has a device and sets
            // TISMA_REQUIRE_GPU=1: there a fallback is a failure.
            if (const char* require = std::getenv("TISMA_REQUIRE_GPU");
                options_.graphics == "vulkan" && require && std::string_view(require) == "1") {
                error = "vulkan: " + why;
                return false;
            }
            std::fprintf(stderr, "vulkan: %s; the window stays with OpenGL\n", why.c_str());
            if (window_)
                glfwDestroyWindow(window_);
            window_ = nullptr;
        }
    }
    if (!vulkan_) {
        open_window(false);
        if (!window_) {
            error = "cannot open a window with OpenGL 3.3";
            return false;
        }
        glfwMakeContextCurrent(window_);
        glfwSwapInterval(1);
        if (const char* missing = load_gl(glfwGetProcAddress)) {
            error = std::string("the OpenGL driver lacks ") + missing;
            return false;
        }
        gl_loaded_ = true;
    }
    glfwSetWindowUserPointer(window_, this);
    glfwSetDropCallback(window_, [](GLFWwindow* w, int count, const char** paths) {
        auto* app = static_cast<App*>(glfwGetWindowUserPointer(w));
        for (int i = 0; i < count; ++i)
            app->dropped_.push_back(from_utf8(paths[i]));
    });
    if (options_.screenshot.empty()) {
        float sx = 1.0f, sy = 1.0f;
        glfwGetWindowContentScale(window_, &sx, &sy);
        scale_ = std::clamp(std::max(sx, sy), 1.0f, 4.0f);
    }

    // The window icon: the application icon, drawn at three sizes.
    std::vector<Image> sizes;
    for (int size : {16, 32, 64})
        sizes.push_back(rasterize_svg(embedded("tisma.svg"), size));
    std::vector<GLFWimage> images;
    for (Image& image : sizes)
        if (!image.rgba.empty())
            images.push_back({image.width, image.height, image.rgba.data()});
    if (!images.empty())
        glfwSetWindowIcon(window_, int(images.size()), images.data());

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; // the interface keeps its own preferences
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    const std::string_view font = embedded("Roboto-Medium.ttf");
    if (!font.empty()) {
        ImFontConfig config;
        config.FontDataOwnedByAtlas = false;
        font_ = io.Fonts->AddFontFromMemoryTTF(const_cast<char*>(font.data()), int(font.size()), kFontSize, &config);
    }
    if (!font_)
        font_ = io.Fonts->AddFontDefault();
    if (vulkan_) {
        ImGui_ImplGlfw_InitForVulkan(window_, true);
        if (!vk_.init_imgui(error))
            return false;
    } else {
        ImGui_ImplGlfw_InitForOpenGL(window_, true);
        ImGui_ImplOpenGL3_Init("#version 330 core");
        if (!renderer_.init(error))
            return false;
    }
    // A texture for ImGui::Image from RGBA rows, with OpenGL or Vulkan.
    const auto texture = [&](const Image& image, bool clamp) -> ImTextureID {
        if (vulkan_)
            return vk_.add_texture(image.rgba.data(), image.width, image.height);
        unsigned tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        if (clamp) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     image.rgba.data());
        glBindTexture(GL_TEXTURE_2D, 0);
        return ImTextureID(tex);
    };

    // Icons drawn white at twice their size; the buttons tint them.
    const int icon_pixels = int(std::lround(48.0f * scale_));
    for (const char* name : kIcons) {
        const Image image = rasterize_svg(embedded(std::string(name) + ".svg"), icon_pixels, 0xFFFFFF);
        if (image.rgba.empty())
            continue;
        icons_[name] = texture(image, true);
    }
    const Image logo = rasterize_svg(embedded("tisma.svg"), int(std::lround(96.0f * scale_)));
    if (!logo.rgba.empty())
        logo_ = texture(logo, false);

    if (options_.screenshot.empty())
        load_preferences();
    // The assistant on the first run; screenshots show help only when asked.
    if (options_.screenshot.empty() && first_run_)
        tour_step_ = 0;
    if (!options_.screenshot.empty())
        tips_ = options_.tips;
    if (options_.tour >= 0)
        tour_step_ = options_.tour;
    g_open_info = options_.info;
    if (!options_.level.empty())
        choose_level(view_level_from(options_.level));
    if (options_.single_column)
        single_column_ = true;
    if (options_.ortho)
        camera_.orthographic = true;
    if (options_.labels)
        show_labels_ = true;
    if (const std::size_t slash = options_.section.find('/'); slash != std::string::npos) {
        const std::string tab = options_.section.substr(0, slash);
        objects_tab_ = tab == "objects"; // objects/<row>: the list, its row chosen below (after --object-add)
        panel_tab_ = tab == "material" ? SettingsTab::Material : tab == "printer" ? SettingsTab::Printer : SettingsTab::Print;
        panel_section_[std::size_t(panel_tab_)] = options_.section.substr(slash + 1);
        // The printer's settings are on its page (settings_panel.md, rule 37).
        if (panel_tab_ == SettingsTab::Printer)
            printer_page_ = true, printer_section_ = options_.section.substr(slash + 1), panel_tab_ = SettingsTab::Print;
    }
    if (!options_.search.empty()) {
        const std::vector<SettingHit> hits = search_settings(
            options_.search, [](std::string_view key) { return SettingText{key_label(key), {}}; });
        if (!hits.empty())
            go_to_setting(hits.front().key);
    }
    load_help_data();
    // --help-item: a problem or a procedure open from the start.
    for (std::size_t i = 0; i < knowledge_->problems.size(); ++i)
        if (knowledge_->problems[i].id == options_.help_open)
            help_tab_ = 0, help_select_ = true, help_page_ = HelpPage::Problem, help_item_ = i;
    for (std::size_t i = 0; i < knowledge_->procedures.size(); ++i)
        if (knowledge_->procedures[i].id == options_.help_open)
            help_tab_ = 1, help_select_ = true, help_page_ = HelpPage::Procedure, help_item_ = i;
    if (!options_.help_open.empty() && find_guide(*knowledge_, options_.help_open))
        open_guide(options_.help_open);
    if (!options_.tutorial.empty())
        for (std::size_t i = 0; i < knowledge_->tutorials.size(); ++i)
            if (knowledge_->tutorials[i].id == options_.tutorial) {
                guide_ = start_guide(*knowledge_, i, guide_memory_);
                guide_->step = std::clamp(options_.tutorial_step, 0, int(knowledge_->tutorials[i].steps.size()) - 1);
            }
    if (options_.credits)
        credits_open_ = true;
    if (!options_.help.empty()) {
        help_open_ = true;
        static constexpr const char* tabs[] = {"problems", "procedures", "glossary", "tutorials", "calibrations"};
        for (int i = 0; i < 5; ++i)
            if (options_.help == tabs[i])
                help_tab_ = i;
        help_select_ = true;
    }
    if (options_.choose_settings) {
        choose_level(ViewLevel::Custom);
        choose_draft_ = custom_view_;
        choose_open_ = true;
    }
    if (options_.light)
        theme_ = light_theme();
    if (options_.language_given)
        set_language(options_.language);
    if (options_.render >= 0)
        render_mode_ = static_cast<RenderMode>(std::clamp(options_.render, 0, 2));
    if (options_.msaa >= 0)
        msaa_ = std::max(options_.msaa, 1);
    if (options_.fxaa >= 0)
        fxaa_ = options_.fxaa != 0;
    if (options_.moire >= 0)
        moire_ = options_.moire != 0;
    if (options_.color >= 0 && options_.color < kPreviewViews) {
        preview_view_ = static_cast<PreviewView>(options_.color);
        color_mode_ = kViewInfo[options_.color].shader;
    }
    device_ports_ = device::list_serial_ports();
    if (!options_.device.empty()) {
        device_port_ = options_.device;
        device_connect(device_port_);
    }
    // The profiles; with none chosen yet, the ones the settings match.
    load_profile_lists();
    for (const ProfileKind kind : kProfileKinds) {
        std::string& chosen = chosen_profile_[std::size_t(kind)];
        if (!chosen.empty())
            continue;
        for (const auto* list : {&builtin_profiles_[std::size_t(kind)], &user_profiles_[std::size_t(kind)]})
            for (const Profile& p : *list)
                if (chosen.empty() && same_profile(kind, settings_, p.settings))
                    chosen = p.name;
    }
    apply_theme();
    bed_changed();
    camera_.set_view(StandardView::Iso);
    frame_all();
    // The first model opens the plate; the next ones join it.
    bool first_model = true;
    for (const std::string& file : options_.files) {
        const std::filesystem::path path = from_utf8(file);
        if (is_model(path) && !first_model)
            add_model(path);
        else
            open_path(path);
        first_model = first_model && !is_model(path);
    }
    if (!options_.tool.empty())
        tool_ = options_.tool == "move" ? PlateTool::Move : options_.tool == "rotate" ? PlateTool::Rotate
              : options_.tool == "scale" ? PlateTool::Scale : options_.tool == "paint" ? PlateTool::Paint
              : options_.tool == "material" || options_.tool == "mixing" ? PlateTool::Material
              : options_.tool == "cut"                                    ? PlateTool::Cut
              : options_.tool == "measure"                                ? PlateTool::Measure
                                                                          : PlateTool::LayFlat;
    if (options_.measure[0] >= 0 && !objects_.empty()) {
        selected_ = measure_object_ = 0;
        const TriangleMesh placed = placed_mesh(objects_.front());
        for (const int t : options_.measure)
            if (t >= 0 && std::size_t(t) < placed.triangle_count())
                measure_picks_.push_back(surface_feature(placed, std::size_t(t)));
    }
    if (options_.cut > 0.0 && !objects_.empty()) {
        selected_ = 0;
        cut_height_ = options_.cut;
        cut_selected();
    }
    if (!options_.object_add.empty() && !objects_.empty()) {
        PlateObject& o = objects_.front();
        const std::array<double, 3> size = model_size(o);
        const std::array<double, 3> half{size[0] / 2.0, size[1] / 2.0, size[2] / 2.0};
        std::stringstream list(options_.object_add);
        for (std::string what; std::getline(list, what, ',');)
            if (what == "modifier")
                o.volumes.push_back({PlateVolume::Kind::Modifier, PlateVolume::Shape::Box, {size[0] / 4.0, 0.0, 0.0},
                                     {half[0], size[1] + 2.0, size[2] + 2.0}, R"({"walls":{"count":5}})"});
            else if (what == "negative")
                o.volumes.push_back({PlateVolume::Kind::Negative, PlateVolume::Shape::Cylinder, {0.0, 0.0, -1.0},
                                     {half[0], half[1], size[2] + 2.0}, {}});
            else if (what == "range")
                o.ranges.push_back({0.0, 2.0, with_own_key("", settings_, "layers.height")});
        selected_ = object_settings_ = 0;
    }
    if (options_.object_turn != std::array<double, 3>{0.0, 0.0, 0.0} && !objects_.empty()) {
        objects_.front().place.rotation = from_euler_degrees(options_.object_turn);
        plate_changed();
    }
    if (!options_.object_settings.empty() && !objects_.empty()) {
        try {
            own_settings(settings_, options_.object_settings, objects_.front().name);
            objects_.front().settings = options_.object_settings;
            selected_ = object_settings_ = 0;
        } catch (const std::exception& e) {
            fail(T::CannotOpen, e.what());
        }
    }
    // --section objects/<row>: the first object's row in the Objects tab
    // (infill, walls, surfaces, volume1, range1...), its window closed.
    if (objects_tab_ && !objects_.empty()) {
        list_object_ = selected_ = 0;
        object_settings_ = -1;
        const std::string row = options_.section.substr(options_.section.find('/') + 1);
        for (const ObjectRow& r : object_rows(objects_.front()))
            if ((r.kind == ObjectRow::Kind::Settings && r.group == row) ||
                (r.kind == ObjectRow::Kind::Volume && row == "volume" + std::to_string(r.index + 1)) ||
                (r.kind == ObjectRow::Kind::Range && row == "range" + std::to_string(r.index + 1)))
                list_row_ = r;
    }
    // The mixed colours' dialog open, with the project's first blend (for screenshots).
    if (options_.tool == "mixing") {
        mixing_open_ = true;
        mixes_before_ = mixes_;
        mix_edit_ = mixes_.empty() ? -1 : 0;
    }
    // Brush strokes asked on the command line, for screenshots and tests.
    for (const AppOptions::Stroke& b : options_.strokes) {
        const PaintChannel channel = b.channel == "seam"    ? PaintChannel::Seam
                                     : b.channel == "fuzzy" ? PaintChannel::Fuzzy
                                                            : PaintChannel::Support;
        for (std::size_t i = 0; i < objects_.size() && i < placed_meshes_.size(); ++i) {
            Brush brush;
            brush.centre = {b.x, b.y, b.z};
            brush.radius = b.radius;
            brush.detail = std::max(0.05, b.radius / 8.0);
            brush.mark = b.block ? kPaintBlock : kPaintEnforce;
            paint(objects_[i].painting, channel, placed_meshes_[i], brush);
        }
        paint_channel_ = channel;
        paint_dirty_ = true;
        ++revision_.model;
    }
    for (const std::string& choice : options_.profiles) {
        const std::size_t eq = choice.find('=');
        const auto kind = profile_kind_from_name(choice.substr(0, eq));
        const Profile* p = kind && eq != std::string::npos ? find_profile(*kind, choice.substr(eq + 1)) : nullptr;
        // A printer of the catalog by its profile's name, for this run.
        if (!p && kind == ProfileKind::Printer)
            if (const std::optional<CatalogMatch> m = catalog_model_of(builtin_catalog(), choice.substr(eq + 1))) {
                std::vector<Profile>& mine = user_profiles_[std::size_t(ProfileKind::Printer)];
                mine.push_back(printer_profile(*m->model, m->nozzle));
                p = &mine.back();
            }
        if (!p) {
            error = "--profile " + choice + ": no such profile";
            return false;
        }
        choose_profile(*kind, *p);
    }
    if (const std::string& c = options_.calibration; c == "extruder") {
        set_tab(Tab::Calibration);
        extruder_open_ = extruder_scroll_ = true;
    } else if (!c.empty()) {
        create_calibration(calibration_kind_from_name(c).value_or(CalibrationKind::Dimensions), false);
    }
    if (!options_.screenshot.empty() && (options_.view == "preview" || !options_.export_path.empty() || options_.device_print) && mesh_ &&
        !result_)
        start_slice();
    if (!options_.catalog.empty()) {
        catalog_kind_ = options_.catalog == "filament" ? ProfileKind::Filament : ProfileKind::Printer;
        catalog_open_ = true;
        catalog_query_ = options_.catalog_query;
    }
    if (!options_.browse.empty()) {
        purpose_ = Purpose::Open;
        browser_.open(FileBrowser::Mode::Open, tr(T::OpenFile), kOpenExtensions, from_utf8(options_.browse));
    }
    return true;
}

void App::apply_theme() {
    apply_style(theme_, scale_);
    BedLook look;
    look.minor = theme_.grid_minor;
    look.major = theme_.grid_major;
    look.outline = 0xFF000000u | theme_.accent_hover;
    renderer_.set_bed(settings_.bed, look);
    vk_view_.set_bed(settings_.bed, look);
}

void App::shutdown() {
    slice_job_.wait();
    voxel_job_.wait();
    device_disconnect();
    if (options_.screenshot.empty() && (gl_loaded_ || vulkan_))
        save_preferences();
    if (gl_loaded_) {
        for (const auto& [name, tex] : icons_) {
            const auto id = unsigned(tex);
            glDeleteTextures(1, &id);
        }
        if (logo_) {
            const auto id = unsigned(logo_);
            glDeleteTextures(1, &id);
        }
        if (view_queries_[0])
            glDeleteQueries(3, view_queries_);
        renderer_.shutdown();
    }
    vk_view_.shutdown(); // its picture is an ImGui texture: before the backend
    if (ImGui::GetCurrentContext()) {
        if (vulkan_)
            vk_.shutdown(); // ImGui's Vulkan backend and the textures with it
        else
            ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
    vk_.shutdown();
    if (window_)
        glfwDestroyWindow(window_);
    glfwTerminate();
}

void App::load_preferences() {
    const std::filesystem::path path = preferences_path();
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        first_run_ = true;
        return;
    }
    try {
        const nlohmann::json j = nlohmann::json::parse(in);
        tips_ = j.value("tips", true);
        if (const auto a = j.find("assistant"); a != j.end() && a->is_object()) {
            assistant_tutorials_ = a->value("tutorials", true);
            g_glossary_tips = assistant_glossary_ = a->value("glossary_tips", true);
            assistant_material_ = a->value("material_tips", true);
        }
        if (j.contains("tips_seen") && j.at("tips_seen").is_array())
            for (const auto& t : j.at("tips_seen"))
                if (t.is_string())
                    tips_seen_.insert(t.get<std::string>());
        // Where each guided tutorial was left (assistant.md, rule 18).
        if (const auto g = j.find("tutorials"); g != j.end() && g->is_object()) {
            if (const auto st = g->find("step"); st != g->end() && st->is_object())
                for (const auto& [id, n] : st->items())
                    if (n.is_number_integer())
                        guide_memory_.step[id] = n.get<int>();
            for (const char* list : {"dismissed", "done"})
                if (const auto l = g->find(list); l != g->end() && l->is_array())
                    for (const auto& id : *l)
                        if (id.is_string())
                            (std::string(list) == "done" ? guide_memory_.done : guide_memory_.dismissed)
                                .insert(id.get<std::string>());
        }
        if (j.value("language", "es") == "en")
            set_language(Language::English);
        if (j.value("theme", "dark") == "light")
            theme_ = light_theme();
        last_folder_ = from_utf8(j.value("folder", std::string()));
        render_mode_ = static_cast<RenderMode>(std::clamp(j.value("render", 1), 0, 2));
        msaa_ = std::clamp(j.value("msaa", 4), 1, 16);
        fxaa_ = j.value("fxaa", true);
        moire_ = j.value("moire_filter", true);
        device_port_ = j.value("device_port", std::string("sim"));
        device_baud_ = j.value("device_baud", 115200);
        ao_strength_ = std::clamp(j.value("ao_strength", 1.0f), 0.0f, 1.5f);
        if (j.contains("settings")) {
            try {
                settings_ = parse_settings(j.at("settings").dump());
                fill_speeds();
            } catch (const std::exception&) {
                // The rest still counts; the settings start from the defaults.
                keep_unreadable(path);
            }
        }
        // The settings panel: level, section by tab and the cards open or folded.
        if (j.contains("ui") && j.at("ui").is_object()) {
            const nlohmann::json& ui = j.at("ui");
            view_level_ = view_level_from(ui.value("level", std::string("normal")));
            // The Custom view: its keys, the level it started from, and the
            // keys there were then (settings_panel.md, rules 15 to 18).
            custom_view_.base = view_level_from(ui.value("custom_base", std::string("normal")));
            if (custom_view_.base == ViewLevel::Custom)
                custom_view_.base = ViewLevel::Normal;
            for (auto& [list, keys] : {std::pair<const char*, std::set<std::string>*>{"custom_visible", &custom_view_.visible},
                                       {"custom_known", &custom_view_.known}})
                if (ui.contains(list) && ui.at(list).is_array())
                    for (const auto& key : ui.at(list))
                        if (key.is_string())
                            keys->insert(key.get<std::string>());
            if (view_level_ == ViewLevel::Custom && custom_view_.known.empty())
                custom_view_ = custom_view_from(custom_view_.base);
            single_column_ = ui.value("single_column", false);
            bed_type_ = ui.value("bed_type", std::string());
            camera_.orthographic = ui.value("orthographic", false);
            if (ui.contains("section") && ui.at("section").is_array())
                for (std::size_t t = 0; t < std::min<std::size_t>(3, ui.at("section").size()); ++t)
                    if (ui.at("section")[t].is_string())
                        panel_section_[t] = ui.at("section")[t].get<std::string>();
            if (ui.contains("cards") && ui.at("cards").is_object())
                for (const auto& [card, open] : ui.at("cards").items())
                    if (open.is_boolean())
                        card_open_[card] = open.get<bool>();
        }
        // Each printer's calibrations applied and notes (settings_panel.md, rule 38).
        if (j.contains("printers") && j.at("printers").is_object()) {
            for (const auto& [printer, data] : j.at("printers").items()) {
                if (!data.is_object())
                    continue;
                if (data.contains("notes") && data.at("notes").is_string())
                    printer_notes_[printer] = data.at("notes").get<std::string>();
                if (data.contains("calibrations") && data.at("calibrations").is_object())
                    for (const auto& [kind, r] : data.at("calibrations").items())
                        if (r.is_object() && calibration_kind_from_name(kind))
                            calibration_records_[printer][kind] = CalibrationRecord{
                                r.value("date", std::string()), r.value("nozzle", 0.0), r.value("filament", std::string())};
            }
        }
        if (j.contains("profiles") && j.at("profiles").is_object())
            for (const auto& [kind, name] : j.at("profiles").items())
                if (const auto k = profile_kind_from_name(kind); k && name.is_string())
                    chosen_profile_[std::size_t(*k)] = name.get<std::string>();
    } catch (const std::exception&) {
        // Unreadable preferences: start from the defaults.
        keep_unreadable(path);
    }
}

void App::save_preferences() {
    const std::filesystem::path path = preferences_path();
    if (path.empty())
        return;
    try {
        nlohmann::json j;
        j["language"] = language() == Language::English ? "en" : "es";
        j["theme"] = theme_.dark ? "dark" : "light";
        j["folder"] = utf8(last_folder_);
        j["render"] = int(render_mode_);
        j["msaa"] = msaa_;
        j["fxaa"] = fxaa_;
        j["moire_filter"] = moire_;
        j["device_port"] = device_port_;
        j["device_baud"] = device_baud_;
        j["ao_strength"] = ao_strength_;
        j["tips"] = tips_;
        j["assistant"]["tutorials"] = assistant_tutorials_;
        j["assistant"]["glossary_tips"] = assistant_glossary_;
        j["assistant"]["material_tips"] = assistant_material_;
        j["tips_seen"] = tips_seen_;
        j["tutorials"]["step"] = guide_memory_.step;
        j["tutorials"]["dismissed"] = guide_memory_.dismissed;
        j["tutorials"]["done"] = guide_memory_.done;
        j["ui"]["level"] = std::string(view_level_name(view_level_));
        j["ui"]["custom_base"] = std::string(view_level_name(custom_view_.base));
        j["ui"]["custom_visible"] = custom_view_.visible;
        j["ui"]["custom_known"] = custom_view_.known;
        j["ui"]["single_column"] = single_column_;
        j["ui"]["bed_type"] = bed_type_;
        j["ui"]["orthographic"] = camera_.orthographic;
        for (const auto& [printer, notes] : printer_notes_)
            if (!notes.empty())
                j["printers"][printer]["notes"] = notes;
        for (const auto& [printer, records] : calibration_records_)
            for (const auto& [kind, r] : records)
                j["printers"][printer]["calibrations"][kind] = {{"date", r.date}, {"nozzle", r.nozzle}, {"filament", r.filament}};
        j["ui"]["section"] = panel_section_;
        j["ui"]["cards"] = card_open_;
        // Without the changes of a calibration tower left on the bed.
        Settings kept = settings_;
        if (calibration_)
            kept.changes.clear();
        j["settings"] = nlohmann::json::parse(settings_json(kept));
        for (const ProfileKind kind : kProfileKinds)
            j["profiles"][std::string(profile_kind_name(kind))] = chosen_profile_[std::size_t(kind)];
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        write_aside(path, j.dump(2) + "\n");
    } catch (const std::exception&) {
    }
}

void App::notify(std::string text, bool error) {
    toasts_.push_back({std::move(text), error, ImGui::GetTime() + kToastSeconds, {}});
}

// A toast with «Undo», kept twice as long.
void App::notify_undo(std::string text, std::function<void()> undo) {
    toasts_.push_back({std::move(text), false, ImGui::GetTime() + 2.0 * kToastSeconds, std::move(undo)});
}

void App::fail(T title, std::string message, std::vector<std::string> details) {
    error_title_ = tr(title);
    error_message_ = std::move(message);
    error_details_ = std::move(details);
    error_open_ = true;
}

void App::open_path(const std::filesystem::path& path) {
    const std::string ext = lower_extension(path);
    last_folder_ = path.parent_path();
    if (is_model(path))
        load_model(path);
    else if (ext == ".tisma")
        open_project(path);
    else if (ext == ".gcode" || ext == ".gco" || ext == ".g" || ext == ".bgcode")
        load_gcode(path);
    else if (ext == ".json")
        load_settings_file(path);
    else
        fail(T::CannotOpen, utf8(path) + ": only .stl, .obj, .3mf, .tisma, .gcode and .json files");
}

void App::load_model(const std::filesystem::path& path) {
    try {
        timing("before reading");
        ModelFile stl = read_model(path);
        timing("read the file");
        if (stl.mesh.empty()) {
            fail(T::CannotOpen, utf8(path) + ": the file has no triangles");
            return;
        }
        // A calibration tower's changes by height belong to the tower: the
        // next model must not print with its temperatures or flows.
        if (calibration_) {
            settings_.changes.clear();
            settings_changed();
        }
        calibration_.reset();
        use_mesh(std::move(stl.mesh), path, Tab::Prepare);
    } catch (const std::exception& e) {
        fail(T::CannotOpen, e.what());
    }
}

void App::use_mesh(TriangleMesh mesh, const std::filesystem::path& name, Tab tab) {
    ++revision_.model;
    objects_.clear();
    objects_.push_back({utf8(name.filename()), std::move(mesh), {}, {}});
    fire("model_added");
    selected_ = 0;
    tool_ = PlateTool::Select;
    model_path_ = name;
    viewer_ = false;
    result_.reset();
    renderer_.clear_preview();
    vk_view_.clear_preview();
    place_model();
    voxels_.reset();
    voxels_stale_ = true;
    stale_ = false;
    set_tab(tab);
    frame_all();
    glfwSetWindowTitle(window_, ("Tisma · " + utf8(name.filename())).c_str());
}

// A calibration part on the bed, with its changes by height in the settings
// (the dimensional test has none, so it clears them).
void App::load_calibration(CalibrationPrint print) {
    settings_.changes = print.changes;
    settings_changed();
    TriangleMesh mesh = std::move(print.mesh);
    print.mesh = {};
    const std::string name = "tisma_calibration_" + print.kind + ".stl";
    calibration_ = std::move(print);
    use_mesh(std::move(mesh), name, Tab::Calibration);
}

// After the bed changed: one model goes back to its centre; several keep
// their places.
void App::place_model() {
    if (objects_.size() == 1) {
        PlateObject& o = objects_[0];
        o.place.x = o.place.y = 0.0;
        // Its box set down at the origin, as placed_mesh puts it, without
        // building the placed mesh twice.
        const std::array<double, 3> size = placed_size(o);
        BoundingBox box;
        box.min = {float(-0.5 * size[0]), float(-0.5 * size[1]), 0.0f};
        box.max = {float(0.5 * size[0]), float(0.5 * size[1]), float(size[2])};
        box.empty = o.mesh.empty();
        const Vec3 at = centre_on_bed(box, settings_.bed);
        o.place.x = 0.5 * (double(box.min.x) + double(box.max.x)) + at.x;
        o.place.y = 0.5 * (double(box.min.y) + double(box.max.y)) + at.y;
    }
    rebuild_plate();
}

// The placed meshes, the plate's mesh and the view's copy, from objects_.
void App::rebuild_plate() {
    timing("before the plate");
    placed_meshes_.clear();
    object_vertices_.clear();
    std::size_t vertices = 0;
    for (const PlateObject& o : objects_) {
        placed_meshes_.push_back(placed_mesh(o));
        object_vertices_.push_back(vertices);
        vertices += 3 * placed_meshes_.back().triangle_count();
    }
    timing("placed meshes");
    object_vertices_.push_back(vertices);
    paint_dirty_ = true;
    drag_offset_ = {};
    indexes_.clear();
    offset_ = {};
    if (objects_.empty()) {
        mesh_.reset();
        placed_ = {};
        renderer_.set_model({});
        vk_view_.set_model({});
    } else {
        mesh_ = plate_mesh(placed_meshes_);
        placed_ = mesh_->bounding_box();
        timing("plate mesh");
        const std::vector<float> model = mesh_vertices(*mesh_, offset_);
        timing("view's vertices");
        renderer_.set_model(model);
        vk_view_.set_model(model);
        timing("sent to the view");
    }
    if (selected_ >= int(objects_.size()))
        selected_ = int(objects_.size()) - 1;
    ++scene_version_;
}

// A model was moved, turned, scaled, added or removed: the slice and the
// analysis are out of date.
void App::plate_changed() {
    ++revision_.model;
    if (result_ && !viewer_)
        stale_ = true;
    voxels_stale_ = true;
    rebuild_plate();
}

void App::add_model(const std::filesystem::path& path) {
    try {
        ModelFile stl = read_model(path);
        if (stl.mesh.empty()) {
            fail(T::CannotOpen, utf8(path) + ": the file has no triangles");
            return;
        }
        last_folder_ = path.parent_path();
        add_mesh(std::move(stl.mesh), path);
    } catch (const std::exception& e) {
        fail(T::CannotOpen, e.what());
    }
}

// One more model on the plate, and the plate arranged (placement.md, rule 2).
void App::add_mesh(TriangleMesh mesh, const std::filesystem::path& name) {
    if (objects_.empty() || calibration_ || viewer_) {
        use_mesh(std::move(mesh), name, Tab::Prepare);
        return;
    }
    objects_.push_back({utf8(name.filename()), std::move(mesh), {}, {}});
    fire("model_added");
    selected_ = int(objects_.size()) - 1;
    arrange_plate();
    frame_all();
}

void App::arrange_plate() {
    if (objects_.empty())
        return;
    std::vector<std::array<double, 2>> sizes;
    for (const PlateObject& o : objects_) {
        const std::array<double, 3> s = placed_size(o);
        sizes.push_back({s[0], s[1]});
    }
    // The bed's centre (its box), or the origin with no bed.
    double x0 = 0, x1 = 0, y0 = 0, y1 = 0;
    for (std::size_t i = 0; i < settings_.bed.size(); ++i) {
        const auto& p = settings_.bed[i];
        x0 = i == 0 ? p[0] : std::min(x0, p[0]), x1 = i == 0 ? p[0] : std::max(x1, p[0]);
        y0 = i == 0 ? p[1] : std::min(y0, p[1]), y1 = i == 0 ? p[1] : std::max(y1, p[1]);
    }
    // Printed one by one, the head's cylinder between them (sequential.md, rule 2).
    const double spacing = settings_.sequential ? std::max(settings_.placement_spacing, settings_.clearance_radius)
                                                : settings_.placement_spacing;
    const auto at = arrange(sizes, {0.5 * (x0 + x1), 0.5 * (y0 + y1)}, spacing);
    for (std::size_t i = 0; i < objects_.size(); ++i)
        objects_[i].place.x = at[i][0], objects_[i].place.y = at[i][1];
    plate_changed();
}

const std::vector<TriangleIndex>& App::plate_indexes() {
    if (indexes_.size() != placed_meshes_.size()) {
        indexes_.clear();
        for (const TriangleMesh& m : placed_meshes_)
            indexes_.emplace_back(m);
    }
    return indexes_;
}

// The ray from the eye through the mouse, in the view.
Vec3 App::mouse_ray() const {
    const ImVec2 m = ImGui::GetIO().MousePos;
    const float nx = (m.x - view_pos_.x) / view_size_.x * 2.0f - 1.0f;
    const float ny = 1.0f - (m.y - view_pos_.y) / view_size_.y * 2.0f;
    const float half = std::tan(0.5f * camera_.lens_fov()), aspect = view_size_.x / std::max(view_size_.y, 1.0f);
    return normalize(camera_.forward() + camera_.right() * (nx * half * aspect) + camera_.up() * (ny * half));
}

void App::load_gcode(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        fail(T::CannotOpen, utf8(path));
        return;
    }
    std::ostringstream text;
    text << in.rdbuf();
    SliceResult r;
    r.ok = true;
    r.settings = settings_;
    r.gcode = std::move(text).str();
    // A binary G-code (gcode_extras.md): its G-code block, read back.
    if (r.gcode.starts_with("GCDE")) {
        try {
            r.gcode = read_binary_gcode(r.gcode).gcode;
        } catch (const std::exception& e) {
            fail(T::CannotOpen, utf8(path) + ": " + e.what());
            return;
        }
    }
    r.preview = read_gcode_preview(r.gcode, settings_.filament_diameter);
    time_preview(r.preview, machine_kinematics(settings_), settings_.filament_diameter);
    if (r.preview.extrusions.empty()) {
        fail(T::CannotOpen, utf8(path) + ": no extrusions found");
        return;
    }
    ++revision_.model; // a slice still running belongs to the model, not to this file
    result_ = std::move(r);
    viewer_ = true;
    stale_ = false;
    gcode_path_ = path;
    show_preview();
    set_tab(Tab::Preview);
    frame_all();
    notify(format(tr(T::GcodeOpened), utf8(path.filename()).c_str()));
}

void App::load_settings_file(const std::filesystem::path& path) {
    try {
        settings_ = load_settings(path);
        bed_changed();
        settings_changed();
        notify(format(tr(T::SettingsLoaded), utf8(path.filename()).c_str()));
        fill_speeds();
    } catch (const std::exception& e) {
        fail(T::CannotOpen, e.what());
    }
}

void App::load_case_file(const std::filesystem::path& path) {
    try {
        load_case_ = load_analysis_case(path);
        load_case_name_ = utf8(path.filename());
        settings_changed();
        notify(format(tr(T::CaseLoaded), load_case_name_.c_str()));
    } catch (const std::exception& e) {
        fail(T::CannotOpen, e.what());
    }
}

void App::save_settings_file(const std::filesystem::path& path) {
    if (!write_aside(path, settings_json(settings_)))
        fail(T::CannotOpen, utf8(path));
    else
        notify(format(tr(T::SettingsSaved), utf8(path.filename()).c_str()));
}

// A project brings back the settings, the profiles chosen and the model; a
// calibration tower comes back in its tab, sliced, to choose its segment.
void App::open_project(const std::filesystem::path& path) {
    Project p;
    try {
        p = load_project(path);
    } catch (const std::exception& e) {
        fail(T::CannotOpen, e.what());
        return;
    }
    settings_ = std::move(p.settings);
    fill_speeds();
    chosen_profile_ = p.profiles;
    profiles_checked_.clear();
    // The project's mixed colours; those using filaments this printer does
    // not have are dropped (color_mixing.md, rule 5).
    mixes_ = std::move(p.mixes);
    std::erase_if(mixes_, [&](const ColorMix& m) { return !mix_usable(m, settings_.extruders); });
    filament_colors_ = std::move(p.filament_colors);
    bed_changed();
    settings_changed();
    if (!p.models.empty()) {
        // Each model as the project placed it (its mesh moved by its offset).
        std::vector<PlateObject> objects;
        for (ProjectModel& m : p.models) {
            PlateObject o{m.name, translated(m.mesh, {float(m.offset[0]), float(m.offset[1]), float(m.offset[2])}), {},
                          std::move(m.painting)};
            o.extruder = m.extruder;
            o.wipe_into_infill = m.wipe_into_infill;
            o.wipe_into_object = m.wipe_into_object;
            o.settings = m.settings;
            for (const ProjectVolume& v : m.volumes)
                o.volumes.push_back({v.negative ? PlateVolume::Kind::Negative : PlateVolume::Kind::Modifier,
                                     v.cylinder ? PlateVolume::Shape::Cylinder : PlateVolume::Shape::Box, v.offset, v.size,
                                     v.settings, v.turn});
            for (const ProjectRange& r : m.ranges)
                o.ranges.push_back({r.from, r.to, r.settings});
            const BoundingBox box = o.mesh.bounding_box();
            o.place.x = 0.5 * (double(box.min.x) + double(box.max.x));
            o.place.y = 0.5 * (double(box.min.y) + double(box.max.y));
            objects.push_back(std::move(o));
        }
        ProjectModel& model = p.models.front();
        if (p.calibration) {
            CalibrationPrint print;
            print.kind = p.calibration->kind;
            print.unit = p.calibration->unit;
            print.segments = p.calibration->segments;
            print.changes = settings_.changes;
            calibration_kind_ = calibration_kind_from_name(print.kind).value_or(CalibrationKind::Dimensions);
            calibration_ = std::move(print);
            chosen_value_.reset();
            use_mesh(std::move(model.mesh), from_utf8(model.name), Tab::Calibration);
            if (calibration_kind_ != CalibrationKind::Dimensions)
                start_slice();
        } else {
            calibration_.reset();
            use_mesh(std::move(model.mesh), from_utf8(model.name), Tab::Prepare);
            objects_ = std::move(objects);
            selected_ = objects_.size() == 1 ? 0 : -1;
            rebuild_plate();
            frame_all();
        }
    }
    project_path_ = path;
    glfwSetWindowTitle(window_, ("Tisma · " + utf8(path.filename())).c_str());
    notify(format(tr(T::ProjectOpened), utf8(path.filename()).c_str()));
}

// Saves to the project's file, or asks where (always, with `ask`).
void App::begin_save_project(bool ask) {
    if (!ask && !project_path_.empty()) {
        save_project_to(project_path_);
        return;
    }
    std::filesystem::path name = project_path_.filename();
    if (name.empty())
        name = mesh_ ? model_path_.stem() : std::filesystem::path("tisma");
    name.replace_extension(".tisma");
    purpose_ = Purpose::SaveProject;
    browser_.open(FileBrowser::Mode::Save, tr(T::SaveProject), {".tisma"},
                  project_path_.empty() ? last_folder_ : project_path_.parent_path(), utf8(name));
}

void App::save_project_to(const std::filesystem::path& path) {
    Project p;
    p.settings = settings_;
    p.profiles = chosen_profile_;
    // Each model as placed on the bed, its turn and scale applied.
    for (std::size_t i = 0; i < objects_.size() && i < placed_meshes_.size(); ++i) {
        p.models.push_back({objects_[i].name, placed_meshes_[i], {0.0, 0.0, 0.0}, objects_[i].painting,
                            objects_[i].extruder, objects_[i].wipe_into_infill, objects_[i].wipe_into_object,
                            objects_[i].settings});
        // The model is saved as placed: its volumes go with it.
        const Affine to_bed = placing(objects_[i], placed_meshes_[i]);
        const BoundingBox from = objects_[i].mesh.bounding_box(), to = placed_meshes_[i].bounding_box();
        for (const PlateVolume& own : objects_[i].volumes) {
            const PlateVolume v = carried_volume(own, from, to_bed, to);
            p.models.back().volumes.push_back({v.kind == PlateVolume::Kind::Negative, v.shape == PlateVolume::Shape::Cylinder,
                                               v.offset, v.size, v.settings, v.turn});
        }
        for (const PlateRange& r : objects_[i].ranges)
            p.models.back().ranges.push_back({r.from, r.to, r.settings});
    }
    p.mixes = mixes_;
    p.filament_colors = filament_colors_;
    if (calibration_)
        p.calibration = ProjectCalibration{calibration_->kind, calibration_->unit, calibration_->segments};
    try {
        save_project(path, p);
    } catch (const std::exception& e) {
        fail(T::CannotSave, e.what());
        failed_ = true;
        return;
    }
    project_path_ = path;
    glfwSetWindowTitle(window_, ("Tisma · " + utf8(path.filename())).c_str());
    notify(format(tr(T::ProjectSaved), utf8(path.filename()).c_str()));
}

// User profiles live next to the preferences; without a place for those,
// there are only the built-in ones.
std::filesystem::path App::profiles_root() const {
    const std::filesystem::path prefs = preferences_path();
    return prefs.empty() ? prefs : prefs.parent_path() / "profiles";
}

void App::load_profile_lists() {
    // Screenshots do not read the user's profiles, so they come out the same
    // on every machine.
    const std::filesystem::path root = options_.screenshot.empty() ? profiles_root() : std::filesystem::path();
    std::vector<std::string> errors;
    speed_fallback_.clear();
    for (const ProfileKind kind : kProfileKinds) {
        const std::size_t k = std::size_t(kind);
        builtin_profiles_[k] = builtin_profiles(kind);
        // The built-in printer is one made from nothing: its speeds are the fallback.
        if (kind == ProfileKind::Printer)
            for (const Profile& p : builtin_profiles_[k])
                speed_fallback_.insert(p.name);
        user_profiles_[k].clear();
        if (root.empty())
            continue;
        for (Profile& p : load_profiles(root, kind, &errors)) {
            bool builtin = false;
            if (find_profile(kind, p.name, &builtin) && builtin) {
                errors.push_back(p.name + ": the name of a built-in profile");
                continue;
            }
            // A printer saved without speeds (before they were the printer's):
            // the fallback, said when it is chosen.
            if (kind == ProfileKind::Printer && !fill_missing_speeds(p.settings).empty())
                speed_fallback_.insert(p.name);
            user_profiles_[k].push_back(std::move(p));
        }
    }
    if (!errors.empty())
        notify(format(tr(T::ProfilesSkipped), int(errors.size())), true);
    profiles_checked_.clear();
}

const Profile* App::find_profile(ProfileKind kind, const std::string& name, bool* builtin) const {
    const std::size_t k = std::size_t(kind);
    for (const Profile& p : builtin_profiles_[k])
        if (p.name == name) {
            if (builtin)
                *builtin = true;
            return &p;
        }
    for (const Profile& p : user_profiles_[k])
        if (p.name == name) {
            if (builtin)
                *builtin = false;
            return &p;
        }
    return nullptr;
}

// Whether the settings still match each chosen profile; worked out only when
// the settings change.
void App::refresh_profiles() {
    std::string now = settings_json(settings_);
    if (now == profiles_checked_)
        return;
    profiles_checked_ = std::move(now);
    for (const ProfileKind kind : kProfileKinds) {
        const std::size_t k = std::size_t(kind);
        const Profile* p = find_profile(kind, chosen_profile_[k]);
        if (!p)
            chosen_profile_[k].clear();
        profile_modified_[k] = p && !same_profile(kind, settings_, p->settings);
    }
    // The settings that differ from the chosen profiles: the cards' «N
    // changes», and what a level never hides.
    changed_keys_.clear();
    try {
        const Settings saved = saved_settings();
        std::map<std::string, std::string> before;
        for (auto& [key, value] : describe(saved))
            before[key] = std::move(value);
        for (const auto& [key, value] : describe(settings_))
            if (const auto it = before.find(key); it == before.end() || it->second != value)
                changed_keys_.insert(key);
    } catch (const std::exception&) {
        // A profile that no longer applies: nothing marked.
    }
}

void App::choose_profile(ProfileKind kind, const Profile& profile) {
    try {
        settings_ = with_profile(settings_, kind, profile.settings);
    } catch (const std::exception& e) {
        fail(T::CannotOpen, e.what());
        return;
    }
    chosen_profile_[std::size_t(kind)] = profile.name;
    profiles_checked_.clear();
    if (kind == ProfileKind::Printer)
        fire("printer_selected");
    if (kind == ProfileKind::Filament) {
        fire("filament_selected");
        // A material chosen for the first time offers its tutorials (rule 15).
        if (const KnowledgeMaterial* m = current_material(); m && !guide_memory_.done.contains("material:" + m->name)) {
            guide_memory_.done.insert("material:" + m->name);
            offer_tutorials("new_filament");
        }
    }
    if (kind == ProfileKind::Printer) {
        bed_changed();
        if (speed_fallback_.contains(profile.name))
            notify(tr(T::SpeedFallback));
    }
    settings_changed();
}

void App::fill_speeds() {
    if (!fill_missing_speeds(settings_).empty())
        notify(tr(T::SpeedFallback));
}

void App::save_profile_as(ProfileKind kind, std::string name) {
    Profile p;
    p.kind = kind;
    p.name = name;
    p.settings = settings_;
    try {
        const std::filesystem::path root = profiles_root();
        if (root.empty())
            throw SettingsError("profile: there is no folder for the settings (no HOME or APPDATA)");
        save_profile(root, p);
    } catch (const std::exception& e) {
        fail(T::CannotSave, e.what());
        return;
    }
    load_profile_lists();
    chosen_profile_[std::size_t(kind)] = name;
    profiles_checked_.clear();
    notify(format(tr(T::ProfileSaved), name.c_str()));
}

// The profile row at the top of a settings section: the list, save, and a
// menu with save as and delete.
void App::profile_row(ProfileKind kind) {
    const std::size_t k = std::size_t(kind);
    ImGui::PushID(int(k));
    const float size = ImGui::GetFrameHeight();
    const float gap = ImGui::GetStyle().ItemSpacing.x;
    bool builtin = false;
    const Profile* chosen = find_profile(kind, chosen_profile_[k], &builtin);
    std::string preview = chosen ? chosen->name : std::string(tr(T::NoProfile));
    if (chosen && profile_modified_[k])
        preview += " *";
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 2.0f * (size + gap));
    const Profile* picked = nullptr;
    if (ImGui::BeginCombo("##profile", preview.c_str())) {
        ImGui::SeparatorText(tr(T::BuiltinProfiles));
        for (const Profile& p : builtin_profiles_[k])
            if (ImGui::Selectable(p.name.c_str(), chosen == &p))
                picked = &p;
        if (!user_profiles_[k].empty()) {
            ImGui::SeparatorText(tr(T::MyProfiles));
            for (const Profile& p : user_profiles_[k])
                if (ImGui::Selectable(p.name.c_str(), chosen == &p))
                    picked = &p;
        }
        if (kind != ProfileKind::Print) {
            ImGui::Separator();
            if (ImGui::Selectable(tr(T::FromCatalog)))
                catalog_kind_ = kind, catalog_open_ = true;
        }
        ImGui::EndCombo();
    }
    if (chosen && profile_modified_[k] && ImGui::IsItemHovered())
        loc::SetTooltip("%s", tr(T::ProfileModifiedTip));
    auto save_as = [&] {
        save_profile_kind_ = kind;
        save_profile_open_ = true;
        profile_name_ = chosen && !builtin ? chosen->name : std::string();
    };
    ImGui::SameLine(0, gap);
    if (icon_button("##save", "tisma_save", tr(T::SaveProfileTip), size)) {
        if (chosen && !builtin)
            save_profile_as(kind, chosen->name);
        else
            save_as();
    }
    ImGui::SameLine(0, gap);
    if (icon_button("##menu", "tisma_more", tr(T::More), size))
        ImGui::OpenPopup("##profile_menu");
    if (ImGui::BeginPopup("##profile_menu")) {
        if (ImGui::MenuItem(tr(T::SaveProfileAs)))
            save_as();
        if (kind != ProfileKind::Print && ImGui::MenuItem(tr(T::FromCatalog)))
            catalog_kind_ = kind, catalog_open_ = true;
        const std::string remove = chosen ? format(tr(T::DeleteProfile), chosen->name.c_str()) : std::string();
        if (chosen && !builtin && ImGui::MenuItem(remove.c_str())) {
            const std::string name = chosen->name;
            if (remove_profile(profiles_root(), kind, name)) {
                load_profile_lists();
                chosen_profile_[k].clear();
                notify(format(tr(T::ProfileDeleted), name.c_str()));
            }
        }
        ImGui::EndPopup();
    }
    ImGui::PopID();
    if (picked)
        choose_profile(kind, *picked);
}

// The catalog: search printers or filaments, see an entry with its sources,
// and add it to the user's profiles.
void App::catalog_dialog() {
    if (catalog_open_) {
        ImGui::OpenPopup("###catalog");
        catalog_open_ = false;
        catalog_pick_ = -1;
        catalog_nozzle_ = 0;
    }
    if (!catalog_kind_)
        return;
    const bool printers = *catalog_kind_ == ProfileKind::Printer;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(std::min(820.0f * scale_, vp->Size.x * 0.9f), std::min(560.0f * scale_, vp->Size.y * 0.85f)),
                             ImGuiCond_Appearing);
    const std::string title = std::string(tr(printers ? T::PrinterCatalog : T::FilamentCatalog)) + "###catalog";
    bool open = true;
    if (!ImGui::BeginPopupModal(title.c_str(), &open)) {
        if (!open)
            catalog_kind_.reset();
        return;
    }
    const Catalog* catalog = nullptr;
    try {
        catalog = &builtin_catalog();
    } catch (const std::exception& e) {
        ImGui::TextUnformatted(e.what());
    }
    if (catalog) {
        ImGui::PushTextWrapPos(0.0f);
        loc::TextDisabled("%s", tr(T::CatalogHint));
        ImGui::PopTextWrapPos();
        if (ImGui::IsWindowAppearing())
            ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputTextWithHint("##search", tr(T::CatalogSearch), &catalog_query_))
            catalog_pick_ = -1, catalog_nozzle_ = 0;
        const std::vector<const PrinterModel*> found_printers =
            printers ? find_printers(*catalog, catalog_query_) : std::vector<const PrinterModel*>{};
        const std::vector<const FilamentModel*> found_filaments =
            printers ? std::vector<const FilamentModel*>{} : find_filaments(*catalog, catalog_query_);
        const int found = int(printers ? found_printers.size() : found_filaments.size());
        const int total = int(printers ? catalog->printers.size() : catalog->filaments.size());
        if (catalog_pick_ >= found)
            catalog_pick_ = -1;
        if (catalog_pick_ < 0 && found > 0 && !options_.screenshot.empty())
            catalog_pick_ = 0; // a screenshot shows the first entry
        loc::TextDisabled(tr(T::CatalogCount), found, total);

        const float footer = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
        ImGui::BeginChild("##catalog_list", ImVec2(300.0f * scale_, -footer), ImGuiChildFlags_Borders);
        std::string vendor;
        for (int i = 0; i < found; ++i) {
            const std::string& v = printers ? found_printers[std::size_t(i)]->vendor : found_filaments[std::size_t(i)]->vendor;
            if (v != vendor) {
                ImGui::SeparatorText(v.c_str());
                vendor = v;
            }
            ImGui::PushID(i);
            const std::string label = printers ? found_printers[std::size_t(i)]->model : found_filaments[std::size_t(i)]->name;
            if (ImGui::Selectable(label.c_str(), catalog_pick_ == i))
                catalog_pick_ = i, catalog_nozzle_ = 0;
            ImGui::PopID();
        }
        if (found == 0)
            loc::TextDisabled("%s", tr(T::CatalogEmpty));
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("##catalog_entry", ImVec2(0, -footer), ImGuiChildFlags_Borders);
        ImGui::PushTextWrapPos(0.0f);
        const auto maybe = [](const std::optional<double>& v, const char* unit) {
            char text[32];
            if (!v)
                return std::string("-");
            std::snprintf(text, sizeof text, "%s", format("%g %s", *v, unit).c_str());
            return std::string(text);
        };
        const auto range = [](const std::optional<std::array<double, 2>>& r, const char* unit) {
            char text[48];
            if (!r)
                return std::string("-");
            std::snprintf(text, sizeof text, "%s", format("%g–%g %s", (*r)[0], (*r)[1], unit).c_str());
            return std::string(text);
        };
        const std::vector<std::string>* sources = nullptr;
        if (catalog_pick_ >= 0 && printers) {
            const PrinterModel& p = *found_printers[std::size_t(catalog_pick_)];
            ImGui::PushFont(nullptr, kTitleSize * 0.9f);
            loc::Text("%s %s", p.vendor.c_str(), p.model.c_str());
            ImGui::PopFont();
            loc::Text("%s: %g × %g × %g mm", tr(T::BuildVolume), p.build_volume[0], p.build_volume[1], p.build_volume[2]);
            if (p.circular)
                loc::TextDisabled(tr(T::BedRound), p.build_volume[0]);
            const char* firmware = p.firmware == "marlin"    ? "Marlin"
                                   : p.firmware == "klipper" ? "Klipper"
                                   : p.firmware == "reprap"  ? "RepRapFirmware"
                                   : p.firmware == "bambu"   ? "Bambu Lab"
                                   : p.firmware == "other"   ? tr(T::OwnFirmware)
                                                             : "-";
            loc::Text("%s: %s · %s: %s%s", tr(T::Firmware), firmware, tr(T::Extruder),
                        p.direct_drive ? tr(T::DirectDrive) : tr(T::Bowden),
                        p.enclosed ? (std::string(" · ") + tr(T::Enclosed)).c_str() : "");
            loc::Text(tr(T::MaxTemps), maybe(p.max_nozzle_temp, "°C").c_str(), maybe(p.max_bed_temp, "°C").c_str());
            loc::Text(tr(T::MaxMotion), maybe(p.max_speed, "mm/s").c_str(), maybe(p.max_acceleration, "mm/s²").c_str());
            if (!p.multi_material.empty())
                loc::Text("%s: %s", tr(T::MultiMaterial), p.multi_material.c_str());
            // The capabilities in detail, those the maker states.
            const auto join = [](const std::vector<std::string>& items) {
                std::string out;
                for (const std::string& i : items)
                    out += (out.empty() ? "" : ", ") + i;
                return out;
            };
            const auto line = [](T label, const std::string& value) {
                if (!value.empty())
                    loc::Text("%s: %s", tr(label), value.c_str());
            };
            line(T::Kinematics, p.kinematics);
            if (p.max_flow)
                line(T::MaxFlow, maybe(p.max_flow, "mm³/s"));
            if (p.chamber_heating)
                line(T::ChamberHeating, maybe(p.chamber_heating, "°C"));
            if (p.hardened_nozzle)
                line(T::HardenedNozzle, *p.hardened_nozzle ? tr(T::Yes) : tr(T::No));
            line(T::Hotend, p.hotend);
            line(T::Probe, p.probe);
            line(T::BedSurfaces, join(p.bed_surfaces));
            line(T::Connectivity, join(p.connectivity));
            line(T::Features, join(p.features));
            if (p.material_slots)
                line(T::MaterialSlots, std::to_string(*p.material_slots));
            if (p.tools)
                line(T::Tools, std::to_string(*p.tools));
            if (needs_own_start_gcode(p))
                loc::TextColored(rgb(0xE8A33D), "%s", tr(T::OwnStartGcode));
            catalog_nozzle_ = std::min(catalog_nozzle_, p.nozzles.size() - 1);
            ImGui::SetNextItemWidth(120.0f * scale_);
            char nozzle[24];
            std::snprintf(nozzle, sizeof nozzle, "%s", format("%g mm", p.nozzles[catalog_nozzle_]).c_str());
            if (ImGui::BeginCombo(tr(T::NozzleSize), nozzle)) {
                for (std::size_t n = 0; n < p.nozzles.size(); ++n) {
                    std::snprintf(nozzle, sizeof nozzle, "%s", format("%g mm", p.nozzles[n]).c_str());
                    if (ImGui::Selectable(nozzle, n == catalog_nozzle_))
                        catalog_nozzle_ = n;
                }
                ImGui::EndCombo();
            }
            sources = &p.sources;
        } else if (catalog_pick_ >= 0) {
            const FilamentModel& f = *found_filaments[std::size_t(catalog_pick_)];
            ImGui::PushFont(nullptr, kTitleSize * 0.9f);
            loc::Text("%s %s", f.vendor.c_str(), f.name.c_str());
            ImGui::PopFont();
            loc::Text("%s: %s · %g mm", tr(T::Material), f.material.c_str(), f.diameter);
            loc::Text("%s: %s%s", tr(T::NozzleRange), range(f.nozzle_temp, "°C").c_str(),
                        f.nozzle_recommended ? (" · " + maybe(f.nozzle_recommended, "°C") + " " + tr(T::Recommended)).c_str() : "");
            loc::Text("%s: %s%s", tr(T::BedRange), range(f.bed_temp, "°C").c_str(),
                        f.bed_recommended ? (" · " + maybe(f.bed_recommended, "°C") + " " + tr(T::Recommended)).c_str() : "");
            if (f.fan)
                loc::Text("%s: %s", tr(T::FanMin), range(f.fan, "%").c_str());
            if (f.density)
                loc::Text("%s: %s", tr(T::Density), maybe(f.density, "g/cm³").c_str());
            if (f.enclosure.value_or(false))
                loc::TextColored(rgb(0xE8A33D), "%s", tr(T::NeedsEnclosure));
            if (is_abrasive(f))
                loc::TextColored(rgb(0xE8A33D), "%s", tr(T::AbrasiveFilament));
            if (f.max_flow)
                loc::Text("%s: %s", tr(T::MaxFlow), maybe(f.max_flow, "mm³/s").c_str());
            if (f.chamber_temp)
                loc::Text("%s: %s", tr(T::ChamberHeating), range(f.chamber_temp, "°C").c_str());
            if (f.drying_temp)
                loc::Text("%s: %s%s", tr(T::Drying), maybe(f.drying_temp, "°C").c_str(),
                            f.drying_hours ? (", " + maybe(f.drying_hours, "h")).c_str() : "");
            if (f.soluble.value_or(false))
                loc::Text("%s", tr(T::Soluble));
            if (!f.hardness.empty())
                loc::Text("%s: %s", tr(T::Hardness), f.hardness.c_str());
            if (f.glass_transition)
                loc::Text("Tg: %s", maybe(f.glass_transition, "°C").c_str());
            if (f.heat_deflection)
                loc::Text("HDT: %s", maybe(f.heat_deflection, "°C").c_str());
            if (f.shrinkage)
                loc::Text("%s: %s", tr(T::Shrinkage), maybe(f.shrinkage, "%").c_str());
            if (!f.bed_surfaces.empty()) {
                std::string surfaces;
                for (const std::string& b : f.bed_surfaces)
                    surfaces += (surfaces.empty() ? "" : ", ") + b;
                loc::Text("%s: %s", tr(T::BedSurfaces), surfaces.c_str());
            }
            sources = &f.sources;
        }
        if (sources) {
            ImGui::SeparatorText(tr(T::Sources));
            for (const std::string& url : *sources)
                ImGui::TextLinkOpenURL(url.c_str());
        }
        ImGui::PopTextWrapPos();
        ImGui::EndChild();

        ImGui::BeginDisabled(catalog_pick_ < 0);
        if (ImGui::Button(tr(T::AddToProfiles), ImVec2(200.0f * scale_, 0))) {
            const Profile profile =
                printers ? printer_profile(*found_printers[std::size_t(catalog_pick_)],
                                           found_printers[std::size_t(catalog_pick_)]->nozzles[catalog_nozzle_])
                         : filament_profile(*found_filaments[std::size_t(catalog_pick_)]);
            add_catalog_profile(profile);
            catalog_kind_.reset();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
    }
    if (ImGui::Button(tr(T::Cancel), ImVec2(120.0f * scale_, 0)) ||
        (ImGui::IsKeyPressed(ImGuiKey_Escape) && catalog_kind_)) {
        catalog_kind_.reset();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

// A catalog entry becomes one of the user's profiles, and the chosen one.
void App::add_catalog_profile(const Profile& catalog) {
    // A profile of the same name may hold the user's own changes: the copy
    // from the catalog takes a free name, "(2)", "(3)"..., instead of
    // replacing it.
    Profile profile = catalog;
    for (int n = 2; find_profile(profile.kind, profile.name) != nullptr; ++n)
        profile.name = catalog.name + " (" + std::to_string(n) + ")";
    try {
        const std::filesystem::path root = profiles_root();
        if (root.empty())
            throw SettingsError("profile: there is no folder for the settings (no HOME or APPDATA)");
        save_profile(root, profile);
    } catch (const std::exception& e) {
        fail(T::CannotSave, e.what());
        return;
    }
    load_profile_lists();
    if (const Profile* saved = find_profile(profile.kind, profile.name))
        choose_profile(profile.kind, *saved);
    // The catalog's printers have no speeds of their own (printer_profile).
    notify(format(tr(profile.kind == ProfileKind::Printer ? T::CatalogSpeedFallback : T::ProfileAdded),
                  profile.name.c_str()));
}

void App::export_gcode_to(const std::filesystem::path& path) {
    // The result can go while the dialog is open: say so instead of writing
    // nothing, or an old result, without a word.
    if (!result_ || !result_->ok || (!result_->model && !result_->resin) || viewer_ || stale_) {
        fail(T::ExportFailed, tr(T::Stale));
        return;
    }
    // A resin printer's GOO file (resin.md), written whole or not at all.
    if (result_->resin) {
        const std::filesystem::path part = path.string() + ".part";
        bool written = false;
        {
            std::ofstream out(part, std::ios::binary);
            if (out) {
                write_goo(out, *result_->resin, result_->settings, goo_time_now());
                out.close();
                written = bool(out);
            }
        }
        std::error_code ec;
        if (written)
            std::filesystem::rename(part, path, ec);
        if (!written || ec) {
            std::filesystem::remove(part, ec);
            fail(T::ExportFailed, "cannot write " + utf8(path));
            return;
        }
        last_folder_ = path.parent_path();
        notify(format(tr(T::Exported), utf8(path.filename()).c_str()) + " · " +
               format(tr(T::ResinLayersN), int(result_->resin->layers.size())));
        return;
    }
    std::vector<Warning> warnings;
    const GcodeExport e = export_gcode(path, *result_->model, result_->settings, &warnings);
    if (!e.written) {
        std::vector<std::string> details;
        for (const GcodeProblem& p : e.check.problems)
            details.push_back("line " + std::to_string(p.line) + ": " + p.message);
        fail(T::ExportFailed, e.error, std::move(details));
        return;
    }
    last_folder_ = path.parent_path();
    notify(format(tr(T::Exported), utf8(path.filename()).c_str()) + " · " +
           format(tr(T::GcodeOk), number_text(e.check.stats.lines).c_str()));
}

void App::begin_export() {
    purpose_ = Purpose::SaveGcode;
    // A resin printer's file is a GOO (resin.md).
    const std::string ext = result_ && result_->resin ? ".goo" : ".gcode";
    const std::string name = (model_path_.empty() ? std::string("tisma") : utf8(model_path_.stem())) + ext;
    browser_.open(FileBrowser::Mode::Save, tr(T::SaveGcode), {ext}, last_folder_, name);
}

void App::start_slice() {
    if (!mesh_ || slice_job_.running())
        return;
    // The whole set, profiles merged: what no model can print stops here.
    try {
        check_settings(settings_);
    } catch (const SettingsError& e) {
        fail(T::CannotSlice, e.what());
        return;
    }
    // Vase mode prints one object (vase_mode.md, rule 2), or several one
    // after another (sequential.md, rule 5).
    if (settings_.vase && objects_.size() > 1 && !settings_.sequential) {
        fail(T::CannotSlice, tr(T::VaseManyObjects));
        return;
    }
    TriangleMesh placed = translated(*mesh_, offset_);
    Settings settings = settings_;
    // The resin tests print on the platform, without supports, pad or
    // hollowing (calibration_models.md, "Resina").
    if (calibration_ && is_resin_calibration(calibration_kind_))
        settings.resin_supports = settings.resin_pad = settings.resin_pad_around_object = settings.resin_hollow = false;
    Painting painting = plate_painting(objects_);
    // Each model's triangles in the plate's mesh, as plate_mesh joins them.
    std::vector<MeshObject> objects;
    for (std::size_t i = 0, first = 0; i < objects_.size() && i < placed_meshes_.size(); ++i) {
        objects.push_back({objects_[i].name, first, placed_meshes_[i].triangle_count(), i, 0, objects_[i].extruder,
                           objects_[i].wipe_into_infill, objects_[i].wipe_into_object, objects_[i].settings});
        for (const PlateRange& r : objects_[i].ranges)
            objects.back().ranges.push_back({r.from, r.to, r.settings});
        first += placed_meshes_[i].triangle_count();
    }
    // Its modifiers and negative volumes, where the objects stand (model_tools.md, rules 2 and 3).
    SliceVolumes volumes;
    for (std::size_t i = 0; i < objects_.size() && i < placed_meshes_.size(); ++i) {
        for (std::size_t k = 0; k < objects_[i].volumes.size(); ++k) {
            const PlateVolume& v = objects_[i].volumes[k];
            TriangleMesh shape = volume_mesh(v, objects_[i], placed_meshes_[i]);
            if (v.kind == PlateVolume::Kind::Negative) {
                for (std::size_t t = 0; t < shape.triangle_count(); ++t)
                    volumes.negative.add_triangle(shape.corner(t, 0), shape.corner(t, 1), shape.corner(t, 2));
            } else {
                volumes.modifiers.push_back(
                    {objects_[i].name + " modifier " + std::to_string(k + 1), i, std::move(shape), v.settings});
            }
        }
    }
    // The objects go to the slicer when there are several, or when each may
    // print with its own extruder or a mixed colour.
    const bool by_object = objects.size() > 1 || settings.extruders > 1 || !volumes.empty() ||
                           std::any_of(objects_.begin(), objects_.end(), [](const PlateObject& o) {
                               return !o.settings.empty() || !o.ranges.empty();
                           });
    slice_started_ = glfwGetTime();
    slice_made_ = revision_;
    slice_job_.start([this, placed = std::move(placed), settings = std::move(settings), load = load_case_,
                      painting = std::move(painting), objects = std::move(objects), mixes = mixes_, by_object,
                      volumes = std::move(volumes)]() {
        return slicer_.run(placed, settings, load ? &*load : nullptr, painting.empty() ? nullptr : &painting,
                           by_object ? &objects : nullptr, mixes.empty() ? nullptr : &mixes,
                           volumes.empty() ? nullptr : &volumes);
    });
}

void App::poll_jobs() {
    if (std::optional<SliceResult> r = slice_job_.take()) {
        // Another model was opened while slicing: this result is not its.
        const Fit made = fit(slice_made_, revision_);
        if (made == Fit::OtherModel)
            return;
        // The settings changed while slicing: a screenshot slices again (it
        // must show what was asked); the window shows it as out of date.
        if (made == Fit::OutOfDate && !options_.screenshot.empty()) {
            start_slice();
            return;
        }
        if (!r->ok) {
            fail(T::CannotSlice, r->error);
            failed_ |= !options_.export_path.empty();
            return;
        }
        result_ = std::move(*r);
        viewer_ = false;
        stale_ = made == Fit::OutOfDate;
        fire("sliced");
        show_preview();
        const bool calibrating = tab_ == Tab::Calibration;
        if (options_.layer > 0)
            top_layer_ = std::clamp(options_.layer - 1, 0, top_layer_);
        // A screenshot keeps the tab it asked for.
        const Tab asked = options_.view == "analysis"      ? Tab::Analysis
                          : options_.view == "calibration" ? Tab::Calibration
                          : options_.view == "device"      ? Tab::Device
                          : options_.view == "prepare"     ? Tab::Prepare
                                                           : Tab::Preview;
        set_tab(!options_.screenshot.empty() ? asked : calibrating ? Tab::Calibration : Tab::Preview);
        if (!options_.export_path.empty()) {
            export_gcode_to(from_utf8(options_.export_path));
            failed_ |= !error_title_.empty();
        }
        if (!result_->check.ok()) {
            std::vector<std::string> details;
            for (const GcodeProblem& p : result_->check.problems)
                details.push_back("line " + std::to_string(p.line) + ": " + p.message);
            fail(T::CannotSlice, format(tr(T::GcodeBad), int(result_->check.problem_count)), std::move(details));
        }
    }
    if (std::optional<VoxelEstimate> v = voxel_job_.take()) {
        if (fit(voxels_made_, revision_) == Fit::Current) {
            voxels_ = std::move(*v);
        } else {
            voxels_.reset();
            voxels_stale_ = true;
        }
    }
    // The Analysis tab keeps its estimate up to date.
    if (tab_ == Tab::Analysis)
        estimate_voxels_if_stale();
}

void App::show_preview() {
    upload_preview();
    top_layer_ = int(result_->preview.layers.size()) - 1;
    bottom_layer_ = 0;
}

// The preview's buffers with the view's numbers, to the GPU.
void App::upload_preview() {
    const auto start = std::chrono::steady_clock::now();
    preview_buffers_ = preview_buffers(result_->preview, 0xFF4FA3FF, kViewInfo[int(preview_view_)].value);
    preview_ms_ = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    const PreviewBuffers& buffers = preview_buffers_;
    ++scene_version_;
    feed_range_[0] = buffers.min_feed, feed_range_[1] = buffers.max_feed;
    width_range_[0] = buffers.min_width, width_range_[1] = buffers.max_width;
    height_range_[0] = buffers.min_height, height_range_[1] = buffers.max_height;
    renderer_.set_preview(buffers);
    vk_view_.set_preview(buffers);
}

void App::settings_changed() {
    ++revision_.settings;
    if (result_ && !viewer_)
        stale_ = true;
    voxels_stale_ = true;
}

void App::bed_changed() {
    ++revision_.settings; // the model moves with the bed
    apply_theme(); // remakes the bed's lines
    place_model();
    ++scene_version_;
}

// Framing needs the view's size, known after the first layout: until then it
// waits.
void App::frame_all() { frame_pending_ = true; }

void App::frame_now() {
    frame_pending_ = false;
    double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    bool any = false;
    for (const auto& p : settings_.bed) {
        x0 = any ? std::min(x0, p[0]) : p[0], x1 = any ? std::max(x1, p[0]) : p[0];
        y0 = any ? std::min(y0, p[1]) : p[1], y1 = any ? std::max(y1, p[1]) : p[1];
        any = true;
    }
    Vec3 lo{float(x0), float(y0), 0.0f}, hi{float(x1), float(y1), 0.0f};
    if (!any)
        lo = {-100, -100, 0}, hi = {100, 100, 0};
    // A part, with some of the bed around it; without one, the whole bed.
    std::optional<std::pair<Vec3, Vec3>> part;
    if (result_ && viewer_) {
        const auto& p = result_->preview;
        part = {{p.min[0], p.min[1], 0.0f}, {p.max[0], p.max[1], p.max[2]}};
    } else if (mesh_ && !placed_.empty) {
        part = {{placed_.min.x, placed_.min.y, 0.0f}, {placed_.max.x, placed_.max.y, placed_.max.z}};
    }
    if (part) {
        const Vec3 centre = (part->first + part->second) * 0.5f;
        const Vec3 half = (part->second - part->first) * 0.5f;
        const float reach = std::max({half.x, half.y, half.z, 20.0f}) * 1.6f;
        lo = {centre.x - reach, centre.y - reach, 0.0f};
        hi = {centre.x + reach, centre.y + reach, part->second.z};
    }
    camera_.fit(lo, hi, view_size_.x / std::max(view_size_.y, 1.0f));
    // The calibration and device panels cover the left of the view: the part goes right.
    if (tab_ == Tab::Calibration || tab_ == Tab::Device)
        camera_.pan(225.0f * scale_, 0.0f, view_size_.y);
    if (options_.look.size() == 6) {
        constexpr float kDegrees = 3.14159265f / 180.0f;
        camera_.target = {options_.look[0], options_.look[1], options_.look[2]};
        camera_.distance = options_.look[3];
        camera_.yaw = options_.look[4] * kDegrees;
        camera_.pitch = options_.look[5] * kDegrees;
    }
}

void App::set_tab(Tab tab) {
    // The part moves aside for the calibration panel, and back when it closes
    // (a pending framing does it itself).
    const auto aside = [](Tab t) { return t == Tab::Calibration || t == Tab::Device; };
    if (aside(tab) != aside(tab_) && !frame_pending_ && view_size_.y > 10.0f)
        camera_.pan((aside(tab) ? 225.0f : -225.0f) * scale_, 0.0f, view_size_.y);
    tab_ = tab;
    if (tab == Tab::Analysis)
        estimate_voxels_if_stale();
}

void App::estimate_voxels_if_stale() {
    if (!mesh_ || !voxels_stale_ || voxel_job_.running())
        return;
    voxels_stale_ = false;
    voxels_made_ = revision_;
    TriangleMesh placed = translated(*mesh_, offset_);
    Settings settings = settings_;
    voxel_job_.start([placed = std::move(placed), settings = std::move(settings)]() {
        return estimate_voxels(placed, settings);
    });
}

void App::top_bar() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float height = kTopBar * scale_;
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(ImVec2(vp->Size.x, height));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, rgb(theme_.panel));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f * scale_, 0));
    ImGui::Begin("##top", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetWindowPos();
    const float button_h = height - 14.0f * scale_;
    const float top = (height - button_h) * 0.5f;

    // The logo and the name.
    const float logo = height - 16.0f * scale_;
    ImGui::SetCursorPos(ImVec2(12.0f * scale_, (height - logo) * 0.5f));
    if (logo_)
        ImGui::Image(ImTextureRef(logo_), ImVec2(logo, logo));
    ImGui::SameLine(0, 10.0f * scale_);
    ImGui::PushFont(nullptr, kTitleSize);
    ImGui::SetCursorPosY((height - ImGui::GetTextLineHeight()) * 0.5f);
    ImGui::TextUnformatted("TISMA");
    ImGui::PopFont();
    ImGui::SameLine(0, 22.0f * scale_);

    // The tabs.
    struct TabButton {
        Tab tab;
        const char* icon;
        T label;
    };
    const TabButton tabs[] = {{Tab::Prepare, "tisma_plate", T::TabPrepare},
                              {Tab::Preview, "tisma_view", T::TabPreview},
                              {Tab::Analysis, "tisma_engineering", T::TabAnalysis},
                              {Tab::Calibration, "tisma_calibrate", T::TabCalibration},
                              {Tab::Device, "tisma_printer", T::TabDevice}};
    for (const TabButton& t : tabs) {
        ImGui::SetCursorPosY(top);
        const bool enabled = t.tab != Tab::Preview || result_.has_value();
        if (button(tr(t.label), t.icon, tr(t.label), false, tab_ == t.tab, enabled, nullptr, button_h))
            set_tab(t.tab);
        if (t.tab == Tab::Preview)
            anchor("preview");
        else if (t.tab == Tab::Calibration)
            anchor("calibrations");
        else if (t.tab == Tab::Analysis)
            anchor("engineering_panel");
        ImGui::SameLine(0, 4.0f * scale_);
    }

    // The actions, on the right.
    const float more_w = button_h;
    const float gap = 8.0f * scale_;
    auto width_of = [&](const char* label) {
        return std::round(button_h * 0.32f) * 2.0f + std::round(button_h * 0.5f) + ImGui::GetStyle().ItemInnerSpacing.x +
               2.0f + ImGui::CalcTextSize(label).x;
    };
    const float right = width_of(tr(T::Open)) + width_of(tr(T::Slice)) + width_of(tr(T::Export)) + more_w + gap * 3.0f;
    // The search for a setting and the help (assistant.md, rule 1), between
    // the tabs and the actions, when there is room; F1 and More open the help
    // anyway.
    {
        const float from = ImGui::GetCursorPosX() + 10.0f * scale_;
        float room = ImGui::GetWindowWidth() - right - 12.0f * scale_ - 16.0f * scale_ - from;
        if (room >= button_h) {
            ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() - right - 12.0f * scale_ - gap - button_h, top));
            if (icon_button("##help", "help", tr(T::HelpTip), button_h))
                help_open_ = !help_open_;
            room -= button_h + gap;
        }
        const float w = std::min(300.0f * scale_, room);
        if (w > 120.0f * scale_) {
            ImGui::SetCursorPos(ImVec2(from, top));
            search_box(w, button_h);
            ImGui::SameLine();
        }
    }
    ImGui::SameLine(ImGui::GetWindowWidth() - right - 12.0f * scale_);
    ImGui::SetCursorPosY(top);
    if (button("##open", "tisma_folder", tr(T::Open), false, false, true, tr(T::OpenTip), button_h)) {
        purpose_ = Purpose::Open;
        browser_.open(FileBrowser::Mode::Open, tr(T::OpenFile), kOpenExtensions, last_folder_);
    }
    ImGui::SameLine(0, gap);
    ImGui::SetCursorPosY(top);
    const bool slicing = slice_job_.running();
    if (button("##slice", "tisma_slice", slicing ? tr(T::Slicing) : tr(T::Slice), true, false,
               mesh_.has_value() && !slicing, tr(T::SliceTip), button_h))
        start_slice();
    anchor("slice_button");
    ImGui::SameLine(0, gap);
    ImGui::SetCursorPosY(top);
    const bool can_export = result_ && result_->ok && !viewer_ && !stale_ && result_->check.ok();
    if (button("##export", "tisma_save", tr(result_ && result_->resin ? T::ExportGoo : T::Export), false, false, can_export,
               can_export ? tr(T::ExportTip) : tr(T::ExportNeedsSlice), button_h))
        begin_export();
    anchor("send_button");
    ImGui::SameLine(0, gap);
    ImGui::SetCursorPosY(top);
    if (icon_button("##more", "tisma_more", tr(T::More), button_h))
        ImGui::OpenPopup("##more_menu");
    if (ImGui::BeginPopup("##more_menu")) {
        if (ImGui::MenuItem(tr(T::SaveProject), "Ctrl+S"))
            begin_save_project(false);
        if (ImGui::MenuItem(tr(T::SaveProjectAs), "Ctrl+Shift+S"))
            begin_save_project(true);
        ImGui::Separator();
        if (ImGui::BeginMenu(tr(T::Language))) {
            if (ImGui::MenuItem("Español", nullptr, language() == Language::Spanish))
                set_language(Language::Spanish);
            if (ImGui::MenuItem("English", nullptr, language() == Language::English))
                set_language(Language::English);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(tr(T::Theme))) {
            if (ImGui::MenuItem(tr(T::Dark), nullptr, theme_.dark)) {
                theme_ = dark_theme();
                apply_theme();
            }
            if (ImGui::MenuItem(tr(T::Light), nullptr, !theme_.dark)) {
                theme_ = light_theme();
                apply_theme();
            }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem(tr(T::GcodePreviewMenu))) {
            purpose_ = Purpose::Open;
            browser_.open(FileBrowser::Mode::Open, tr(T::GcodePreviewMenu), {".gcode", ".gco", ".g", ".bgcode"}, last_folder_);
        }
        if (ImGui::MenuItem(tr(T::ExportView))) {
            purpose_ = Purpose::ExportView;
            browser_.open(FileBrowser::Mode::Save, tr(T::ExportViewTitle), {".json"}, last_folder_, "tisma-view.json");
        }
        if (ImGui::MenuItem(tr(T::ImportView))) {
            purpose_ = Purpose::ImportView;
            browser_.open(FileBrowser::Mode::Open, tr(T::ImportViewTitle), {".json"}, last_folder_);
        }
        if (ImGui::MenuItem(tr(T::Tutorial)))
            tour_step_ = 0;
        if (ImGui::MenuItem(tr(T::TipsOn), nullptr, tips_))
            tips_ = !tips_;
        if (ImGui::MenuItem(tr(T::TipsAgain))) {
            tips_seen_.clear();
            tips_ = true;
        }
        // The assistant's preferences (assistant.md, «Ajustes»).
        if (ImGui::MenuItem(tr(T::AssistantTutorials), nullptr, assistant_tutorials_))
            assistant_tutorials_ = !assistant_tutorials_;
        if (ImGui::MenuItem(tr(T::AssistantGlossary), nullptr, assistant_glossary_))
            g_glossary_tips = assistant_glossary_ = !assistant_glossary_;
        if (ImGui::MenuItem(tr(T::AssistantMaterial), nullptr, assistant_material_))
            assistant_material_ = !assistant_material_;
        if (ImGui::MenuItem(tr(T::Help), "F1"))
            help_open_ = true;
        if (ImGui::MenuItem(tr(T::Credits)))
            credits_open_ = true;
        if (ImGui::MenuItem(tr(T::About)))
            about_open_ = true;
        ImGui::Separator();
        if (ImGui::MenuItem(tr(T::Quit)))
            glfwSetWindowShouldClose(window_, GLFW_TRUE);
        ImGui::EndPopup();
    }
    dl->AddLine(ImVec2(origin.x, origin.y + height - 1.0f), ImVec2(origin.x + vp->Size.x, origin.y + height - 1.0f),
                ImGui::GetColorU32(rgb(theme_.border)));
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// Whether a setting is of the printer's kind: a resin printer's settings, or
// the rest (interface.md, «Con una impresora de resina»; settings_panel.md,
// rule 1).
bool of_technology(const Settings& s, std::string_view key) {
    if (key == "machine.technology" || key == "machine.max_height")
        return true;
    const bool resin = key.starts_with("resin.") || key.starts_with("machine.display_");
    return resin == (s.machine_technology == MachineTechnology::Msla);
}

// Whether a setting does anything with the others as they are: the ones
// that hang on a switch hide while it is off (settings_panel.md, rule 6).
bool App::setting_applies(std::string_view key) const {
    const Settings& s = settings_;
    if (key == "machine.technology" || key == "machine.max_height")
        return true;
    if (!of_technology(s, key))
        return false;
    if (key.starts_with("support.tree_"))
        return s.support && s.support_style == SupportStyle::Organic;
    if (key.starts_with("support.") && key != "support.enabled")
        return s.support;
    if (key.starts_with("zaa.") && key != "zaa.enabled")
        return s.zaa;
    if (key == "infill.nonplanar_amplitude" || key == "infill.nonplanar_wavelength")
        return s.infill_nonplanar;
    if (key == "infill.wave_amplitude" || key == "infill.wave_length")
        return s.infill_waves;
    if (key == "belt.angle" || key == "belt.belt_is_z")
        return s.belt_enabled;
    if (key == "infill.dense_density")
        return s.infill_dense_under_top;
    if (key == "infill.combine_layers")
        return !s.infill_combine_auto;
    if (key.starts_with("walls.scarf_"))
        return s.scarf != ScarfMode::Nowhere;
    if (key == "layers.detail_tolerance" || key == "layers.detail_margin")
        return s.detail_height > 0.0;
    if (key == "fuzzy.thickness" || key == "fuzzy.point_distance")
        return s.fuzzy_mode != FuzzyMode::None ||
               std::any_of(objects_.begin(), objects_.end(),
                           [](const PlateObject& o) { return !o.painting.empty(PaintChannel::Fuzzy); });
    return true;
}

// Whether the panel shows a setting: its level is in the chosen one (or the
// card's «Show all» was pressed), or it differs from the profile, which no
// level hides; and it does something (rule 6).
bool App::setting_shown(std::string_view key, std::string_view card) const {
    if (!setting_applies(key))
        return false;
    const std::string k(key);
    return view_shows(key) || show_all_cards_.contains(std::string(card)) || changed_keys_.contains(k) ||
           revealed_keys_.contains(k);
}

bool App::view_shows(std::string_view key) const {
    if (view_level_ == ViewLevel::Custom)
        return custom_shows(custom_view_, key);
    const std::optional<SettingPlace> place = setting_place(key);
    return level_shows(view_level_, place ? place->level : SettingLevel::Advanced);
}

Settings App::saved_settings() const {
    Settings saved = settings_;
    for (const ProfileKind kind : kProfileKinds)
        if (const Profile* p = find_profile(kind, chosen_profile_[std::size_t(kind)]))
            saved = with_profile(saved, kind, p->settings);
    return saved;
}

// A level for the panel; Custom the first time shows what the level before
// it showed (rule 15).
void App::choose_level(ViewLevel level) {
    if (level == ViewLevel::Custom && custom_view_.known.empty())
        custom_view_ = custom_view_from(view_level_);
    view_level_ = level;
    show_all_cards_.clear();
}

// The search goes to a setting (rule 5): its tab, section and card open, the
// row brought into sight and lit for 2 s; shown, with its level's tag, if
// the level hides it.
void App::go_to_setting(const std::string& key) {
    const std::optional<SettingPlace> place = setting_place(key);
    if (!place)
        return;
    if (tab_ != Tab::Prepare && tab_ != Tab::Preview)
        set_tab(Tab::Prepare);
    // A printer's setting is on its page (rule 37).
    printer_page_ = place->tab == SettingsTab::Printer;
    if (printer_page_)
        printer_section_ = std::string(place->section->id);
    else
        panel_tab_ = place->tab;
    panel_section_[std::size_t(place->tab)] = std::string(place->section->id);
    card_open_[std::string(place->subsection->id)] = true;
    show_all_cards_.clear();
    revealed_keys_.clear();
    if (!view_shows(key))
        revealed_keys_.insert(key);
    highlight_key_ = key;
    // A screenshot keeps it lit until it is taken.
    highlight_until_ = options_.screenshot.empty() ? ImGui::GetTime() + 2.0 : 1e30;
    scroll_to_highlight_ = true;
}

// The search field of the top bar (interface.md, «Búsqueda de ajustes»): the
// settings that match as one types, by name, card, key or what they do;
// Enter or a click goes to one.
void App::search_box(float width, float height) {
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, height * 0.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        ImVec2(height * 0.9f, std::max(0.0f, (height - ImGui::GetFontSize()) * 0.5f)));
    ImGui::SetNextItemWidth(width);
    if (search_focus_) {
        ImGui::SetKeyboardFocusHere();
        search_focus_ = false;
    }
    const bool enter = ImGui::InputTextWithHint("##search", tr(T::SearchSettings), &search_query_,
                                                ImGuiInputTextFlags_EnterReturnsTrue);
    const bool active = ImGui::IsItemActive() || ImGui::IsItemFocused();
    const ImVec2 lo = ImGui::GetItemRectMin(), hi = ImGui::GetItemRectMax();
    ImGui::PopStyleVar(2);
    // The magnifier.
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float r = height * 0.18f;
    const ImVec2 c(lo.x + height * 0.45f, (lo.y + hi.y) * 0.5f - r * 0.25f);
    const ImU32 dim = ImGui::GetColorU32(rgb(theme_.text_dim));
    dl->AddCircle(c, r, dim, 16, 1.5f);
    dl->AddLine(ImVec2(c.x + r * 0.7f, c.y + r * 0.7f), ImVec2(c.x + r * 1.6f, c.y + r * 1.6f), dim, 1.8f);
    if (search_query_.empty())
        return;
    const Settings& s = settings_;
    const auto& docs = setting_docs();
    const std::vector<SettingHit> hits = search_settings(
        search_query_,
        [&](std::string_view key) {
            const auto doc = docs.find(std::string(key));
            return SettingText{key_label(key), doc != docs.end() ? doc->second.meaning : std::string()};
        },
        [&](std::string_view key) { return of_technology(s, key); }, 10);
    if (enter && !hits.empty()) {
        go_to_setting(hits.front().key);
        search_query_.clear();
        ImGui::SetWindowFocus(nullptr);
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        search_query_.clear();
        return;
    }
    if (!active && !search_list_hovered_)
        return;
    // The matches under the field.
    ImGui::SetNextWindowPos(ImVec2(lo.x, hi.y + 4.0f * scale_));
    ImGui::SetNextWindowSizeConstraints(ImVec2(std::max(width, 320.0f * scale_), 0), ImVec2(FLT_MAX, FLT_MAX));
    ImGui::Begin("##search_results", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    search_list_hovered_ = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    if (hits.empty())
        loc::TextDisabled("%s", tr(T::SearchNoMatch));
    for (const SettingHit& hit : hits) {
        const std::string label = key_label(hit.key) + "##" + hit.key;
        if (ImGui::Selectable(label.c_str())) {
            go_to_setting(hit.key);
            search_query_.clear();
            search_list_hovered_ = false;
        }
        ImGui::SameLine();
        loc::TextDisabled("%s › %s", tr(hit.place.section->title), tr(hit.place.subsection->title));
    }
    ImGui::End();
}

// What Custom shows, chosen in a tree of tab, section, card and setting
// (rule 16), each with its box; a section's or a card's box sets all of it
// and shows a dash when only some are on. «Start from» a level, all or none;
// a filter; Cancel or Accept.
void App::choose_settings_dialog() {
    if (choose_open_) {
        ImGui::OpenPopup("###choose_settings");
        choose_open_ = false;
    }
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(560.0f * scale_, vp->Size.y * 0.8f), ImGuiCond_Appearing);
    const std::string title = std::string(tr(T::ChooseSettingsTitle)) + "###choose_settings";
    if (!ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_NoSavedSettings))
        return;
    CustomView& draft = choose_draft_;
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(tr(T::StartFrom));
    for (const auto& [level, name] : {std::pair{ViewLevel::Basic, T::LevelBasic},
                                      {ViewLevel::Normal, T::LevelNormal},
                                      {ViewLevel::Advanced, T::LevelAdvanced}}) {
        ImGui::SameLine();
        if (ImGui::SmallButton(tr(name)))
            draft = custom_view_from(level);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(tr(T::AllSettings))) {
        draft = custom_view_from(ViewLevel::Advanced);
        draft.base = custom_view_.base;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(tr(T::NoSettings))) {
        draft.visible.clear();
        draft.known.insert(panel_keys().begin(), panel_keys().end());
    }
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##filter", tr(T::FilterSettings), &choose_query_);
    const std::string filter = fold(choose_query_);
    const auto matches = [&](std::string_view key) {
        return of_technology(settings_, key) &&
               (filter.empty() || fold(key_label(key)).find(filter) != std::string::npos ||
                fold(key).find(filter) != std::string::npos);
    };
    // A box for many settings: on when all are, a dash when some are.
    const auto group_box = [&](const char* id, const std::vector<SettingEntry>& entries) {
        int on = 0, all = 0;
        for (const SettingEntry& e : entries)
            if (matches(e.key))
                ++all, on += custom_shows(draft, e.key) ? 1 : 0;
        bool value = all > 0 && on == all;
        if (ImGui::Checkbox(id, &value))
            custom_set(draft, entries, value);
        if (on > 0 && on < all) {
            const ImVec2 a = ImGui::GetItemRectMin();
            const float box = ImGui::GetFrameHeight();
            ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(a.x + box * 0.25f, a.y + box * 0.45f),
                                                      ImVec2(a.x + box * 0.75f, a.y + box * 0.55f),
                                                      ImGui::GetColorU32(ImGuiCol_CheckMark));
        }
        ImGui::SameLine();
    };
    const float bottom = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
    ImGui::BeginChild("##tree", ImVec2(0, -bottom), ImGuiChildFlags_Borders);
    for (const auto& [tab, tab_name] : {std::pair{SettingsTab::Print, T::Print},
                                        {SettingsTab::Material, T::Material},
                                        {SettingsTab::Printer, T::Printer}}) {
        ImGui::PushID(int(tab));
        if (!filter.empty())
            ImGui::SetNextItemOpen(true);
        if (ImGui::TreeNodeEx(tr(tab_name), ImGuiTreeNodeFlags_DefaultOpen)) {
            for (const SectionLayout& section : settings_layout(tab)) {
                std::vector<SettingEntry> in_section;
                for (const SubsectionLayout& sub : section.subsections)
                    for (const SettingEntry& e : sub.settings)
                        if (matches(e.key))
                            in_section.push_back(e);
                if (in_section.empty())
                    continue;
                ImGui::PushID(section.id.data(), section.id.data() + section.id.size());
                group_box("##section", in_section);
                if (!filter.empty())
                    ImGui::SetNextItemOpen(true);
                if (ImGui::TreeNode(tr(section.title))) {
                    for (const SubsectionLayout& sub : section.subsections) {
                        std::vector<SettingEntry> in_card;
                        for (const SettingEntry& e : sub.settings)
                            if (matches(e.key))
                                in_card.push_back(e);
                        if (in_card.empty())
                            continue;
                        ImGui::PushID(sub.id.data(), sub.id.data() + sub.id.size());
                        group_box("##card", in_card);
                        if (!filter.empty())
                            ImGui::SetNextItemOpen(true);
                        if (ImGui::TreeNode(tr(sub.title))) {
                            for (const SettingEntry& e : in_card) {
                                bool value = custom_shows(draft, e.key);
                                const std::string label = key_label(e.key) + "##" + std::string(e.key);
                                if (ImGui::Checkbox(label.c_str(), &value))
                                    custom_set(draft, {e}, value);
                                ImGui::SameLine();
                                loc::TextDisabled("%s", tr(e.level == SettingLevel::Basic    ? T::LevelBasic
                                                             : e.level == SettingLevel::Normal ? T::LevelNormal
                                                                                               : T::LevelAdvanced));
                            }
                            ImGui::TreePop();
                        }
                        ImGui::PopID();
                    }
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
    const float w = 120.0f * scale_;
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 2.0f * w - ImGui::GetStyle().ItemSpacing.x -
                         ImGui::GetStyle().WindowPadding.x);
    if (ImGui::Button(tr(T::Cancel), ImVec2(w, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
        ImGui::CloseCurrentPopup();
    ImGui::SameLine();
    if (ImGui::Button(tr(T::Accept), ImVec2(w, 0))) {
        custom_view_ = draft;
        view_level_ = ViewLevel::Custom;
        show_all_cards_.clear();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

bool App::card_shown(const SubsectionLayout& sub) const {
    return std::any_of(sub.settings.begin(), sub.settings.end(),
                       [&](const SettingEntry& e) { return setting_shown(e.key, sub.id); });
}

// A card of the panel: its title, «N changes» in violet, its menu (Reset,
// rule 8) and, folded, the line that sums it up; a click on the title folds
// or opens it. Its background is as tall as the card was in the last frame.
bool App::settings_card(const SubsectionLayout& sub, bool first, int changes, const std::string& summary, bool& reset) {
    const std::string id(sub.id);
    const auto known = card_open_.find(id);
    bool open = known != card_open_.end() ? known->second : first;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const float pad = 9.0f * scale_, line = ImGui::GetTextLineHeight();
    const float header = open ? line : 2.0f * line + 2.0f * scale_;
    const auto height = card_heights_.find(id);
    const float tall = height != card_heights_.end() ? height->second : header + 2.0f * pad;
    dl->AddRectFilled(start, ImVec2(start.x + width, start.y + tall), ImGui::GetColorU32(rgb(theme_.frame, 0.45f)),
                      7.0f * scale_);
    ImGui::PushID(id.c_str());
    ImGui::SetCursorScreenPos(ImVec2(start.x, start.y));
    ImGui::SetNextItemAllowOverlap();
    if (ImGui::InvisibleButton("##head", ImVec2(width, header + 2.0f * pad * 0.85f))) {
        open = !open;
        card_open_[id] = open;
    }
    const bool hovered = ImGui::IsItemHovered();
    // Where the card's function comes from, over its title (credits.md, rule 3).
    if (hovered && ImGui::GetMousePos().y < start.y + pad + line) {
        const std::vector<const FeatureCredit*> from = credits_for_card(credits(), sub.id);
        if (!from.empty()) {
            std::string text;
            for (const FeatureCredit* c : from)
                text += (text.empty() ? "" : "\n") + based_on(*c);
            loc::SetTooltip("%s", text.c_str());
        }
    }
    // The arrow, the title and the changes.
    const ImU32 strong = ImGui::GetColorU32(rgb(theme_.text)), dim = ImGui::GetColorU32(rgb(theme_.text_dim));
    const ImU32 violet = ImGui::GetColorU32(rgb(theme_.accent_hover));
    const float a = line * 0.32f;
    const ImVec2 c(start.x + pad + a, start.y + pad + line * 0.5f);
    if (open)
        dl->AddTriangleFilled(ImVec2(c.x - a, c.y - a * 0.6f), ImVec2(c.x + a, c.y - a * 0.6f), ImVec2(c.x, c.y + a * 0.7f),
                              hovered ? violet : dim);
    else
        dl->AddTriangleFilled(ImVec2(c.x - a * 0.6f, c.y - a), ImVec2(c.x - a * 0.6f, c.y + a), ImVec2(c.x + a * 0.7f, c.y),
                              hovered ? violet : dim);
    const float text_x = start.x + pad + 2.0f * a + 8.0f * scale_;
    dl->AddText(ImVec2(text_x, start.y + pad), hovered ? violet : strong, tr(sub.title));
    // The menu: three dots at the right of the title.
    const float dots = line * 1.1f;
    const ImVec2 menu_at(start.x + width - pad - dots, start.y + pad);
    ImGui::SetCursorScreenPos(menu_at);
    if (ImGui::InvisibleButton("##menu", ImVec2(dots, line)))
        ImGui::OpenPopup("##card_menu");
    const bool menu_hovered = ImGui::IsItemHovered();
    for (int i = 0; i < 3; ++i)
        dl->AddCircleFilled(ImVec2(menu_at.x + dots * (0.2f + 0.3f * float(i)), menu_at.y + line * 0.5f), 1.7f * scale_,
                            menu_hovered ? violet : dim);
    if (ImGui::BeginPopup("##card_menu")) {
        if (ImGui::MenuItem(tr(T::ResetCard), nullptr, false, changes > 0))
            reset = true;
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            loc::SetTooltip("%s", tr(T::ResetCardTip));
        ImGui::EndPopup();
    }
    if (changes > 0) {
        const std::string count = changes == 1 ? std::string(tr(T::OneChangeCount)) : format(tr(T::ChangesCount), changes);
        const float w = ImGui::CalcTextSize(count.c_str()).x;
        dl->AddText(ImVec2(menu_at.x - 6.0f * scale_ - w, start.y + pad), violet, count.c_str());
    }
    if (!open) {
        // The summary, cut to the card's width.
        std::string text = summary;
        const float room = width - (text_x - start.x) - pad;
        while (text.size() > 1 && ImGui::CalcTextSize(text.c_str()).x > room) {
            do
                text.pop_back();
            while (!text.empty() && (static_cast<unsigned char>(text.back()) & 0xC0) == 0x80);
            if (!text.empty())
                text.pop_back();
            text += "…";
        }
        dl->AddText(ImVec2(text_x, start.y + pad + line + 2.0f * scale_), dim, text.c_str());
    }
    ImGui::SetCursorScreenPos(ImVec2(start.x + pad, start.y + pad + header + pad * 0.7f));
    ImGui::PopID();
    return open;
}

// «Basado en Support Fins, de gittrahan» at a card's foot, small and grey,
// the source's name a link to it (credits.md, rule 3).
void App::card_credits(std::string_view card, float left, float right) {
    const std::vector<const FeatureCredit*> from = credits_for_card(credits(), card);
    if (from.empty())
        return;
    ImGui::Spacing();
    ImGui::PushFont(nullptr, ImGui::GetFontSize() * 0.85f);
    ImGui::PushStyleColor(ImGuiCol_TextLink, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    // The next piece on this line when it fits before the card's edge, else
    // on the next one.
    const auto next = [&](const char* text, float gap) {
        ImGui::SameLine(0.0f, gap);
        if (ImGui::GetCursorScreenPos().x + ImGui::CalcTextSize(text).x > right) {
            ImGui::NewLine();
            ImGui::SetCursorScreenPos(ImVec2(left, ImGui::GetCursorScreenPos().y));
        }
    };
    for (const FeatureCredit* c : from) {
        ImGui::PushID(c);
        ImGui::SetCursorScreenPos(ImVec2(left, ImGui::GetCursorScreenPos().y));
        loc::TextDisabled("%s", tr(T::BasedOn));
        const std::string& name = card_source(*c);
        next(name.c_str(), ImGui::CalcTextSize(" ").x);
        ImGui::TextLinkOpenURL(name.c_str(), c->url.c_str());
        if (ImGui::IsItemHovered())
            loc::SetTooltip("%s\n%s", c->source.c_str(), c->url.c_str());
        if (!c->author.empty()) {
            const std::string by = std::string(tr(T::BasedOnBy)) + c->author;
            next(by.c_str(), 0.0f);
            loc::TextDisabled("%s", by.c_str());
        }
        ImGui::PopID();
    }
    ImGui::PopStyleColor();
    ImGui::PopFont();
}

// Where the assistant's data lives once the profile pack brings it
// (profile_pack.md, rules 8 and 21): the interface's data folder, apart from
// the person's profiles.
std::filesystem::path knowledge_dir() {
    const std::filesystem::path prefs = preferences_path();
    return prefs.empty() ? prefs : prefs.parent_path() / "pack" / "knowledge";
}

// The help's data, read once (the pack's knowledge/, or --knowledge).
void App::load_help_data() {
    const std::filesystem::path folder = options_.knowledge.empty() ? knowledge_dir() : from_utf8(options_.knowledge);
    knowledge_ = folder.empty() ? Knowledge{} : load_knowledge(folder, language() == Language::English ? "en" : "es");
    g_knowledge = &*knowledge_;
}

// The help panel (assistant.md, rules 1 to 4), over the settings panel: its
// tabs, a search over all of them, and at its foot where its texts come
// from. Without the pack's data it says it comes with the profiles (AS1).
void App::help_panel() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float top = kTopBar * scale_, bottom = kStatusBar * scale_;
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x, vp->Pos.y + top));
    ImGui::SetNextWindowSize(ImVec2(kPanel * scale_, vp->Size.y - top - bottom));
    ImGui::Begin("##help_panel", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
    if (!knowledge_)
        load_help_data();
    ImGui::PushFont(nullptr, kTitleSize * 0.9f);
    ImGui::TextUnformatted(tr(T::Help));
    ImGui::PopFont();
    ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetFrameHeight() - ImGui::GetStyle().WindowPadding.x);
    if (ImGui::Button("×", ImVec2(ImGui::GetFrameHeight(), 0)) ||
        (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyPressed(ImGuiKey_Escape)))
        help_open_ = false;
    ImGui::SetNextItemWidth(-FLT_MIN);
    char query[256] = {};
    help_query_.copy(query, sizeof query - 1);
    if (ImGui::InputTextWithHint("##help_search", tr(T::HelpSearch), query, sizeof query))
        help_query_ = query;
    static constexpr T tabs[] = {T::HelpProblems, T::HelpProcedures, T::HelpGlossary, T::HelpTutorials,
                                 T::HelpCalibrations};
    if (ImGui::BeginTabBar("##help_tabs", ImGuiTabBarFlags_FittingPolicyScroll)) {
        for (int i = 0; i < int(std::size(tabs)); ++i) {
            const ImGuiTabItemFlags flags = help_select_ && help_tab_ == i ? ImGuiTabItemFlags_SetSelected : 0;
            if (ImGui::BeginTabItem(tr(tabs[i]), nullptr, flags)) {
                // Until the tab asked for shows, the one shown is not chosen.
                if ((!help_select_ || help_tab_ == i) && help_tab_ != i)
                    help_page_ = HelpPage::List, help_back_.clear();
                if (!help_select_ || help_tab_ == i)
                    help_tab_ = i, help_select_ = false;
                ImGui::EndTabItem();
            }
            if (i == 0)
                anchor("help_problems");
            else if (i == 1)
                anchor("help_procedures");
        }
        ImGui::EndTabBar();
    }
    const float foot = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
    ImGui::BeginChild("##help_body", ImVec2(0, -foot));
    ImGui::PushTextWrapPos(0.0f);
    help_body();
    ImGui::PopTextWrapPos();
    ImGui::EndChild();
    help_apply_dialog();
    // Rule 4: where the help's texts come from. The calibration guides name
    // their own sources at their foot (rule 21), so their tab leaves it out.
    ImGui::PushFont(nullptr, ImGui::GetFontSize() * 0.85f);
    for (const FeatureCredit& c : credits().features)
        if (help_tab_ != 4 && c.spec == "assistant.md" && c.author == "Rafa Barajas") {
            loc::TextDisabled("%s", tr(T::HelpBasedOn));
            ImGui::SameLine(0.0f, ImGui::CalcTextSize(" ").x);
            ImGui::PushStyleColor(ImGuiCol_TextLink, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::TextLinkOpenURL(c.author.c_str(), c.url.c_str());
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered())
                loc::SetTooltip("%s", c.url.c_str());
            break;
        }
    ImGui::PopFont();
    ImGui::End();
}

// The help's pages (assistant.md, rules 2 and 5 to 12): the search's hits,
// a tab's list, or one problem or procedure.
void App::help_body() {
    const Knowledge& k = *knowledge_;
    const auto open_problem = [&](std::size_t i) {
        help_tab_ = 0, help_select_ = true, help_page_ = HelpPage::Problem, help_item_ = i;
    };
    const auto open_procedure = [&](std::size_t i) {
        help_tab_ = 1, help_select_ = true, help_page_ = HelpPage::Procedure, help_item_ = i;
        help_steps_done_.assign(k.procedures[i].steps.size(), false);
    };
    ImGui::Spacing();
    if (help_page_ == HelpPage::Problem && help_item_ < k.problems.size()) {
        help_problem(k.problems[help_item_]);
        return;
    }
    if (help_page_ == HelpPage::Procedure && help_item_ < k.procedures.size()) {
        help_procedure(k.procedures[help_item_]);
        return;
    }
    if (help_page_ == HelpPage::Guide && help_item_ < k.calibrations.size()) {
        help_guide(k.calibrations[help_item_]);
        return;
    }
    // The search, over every tab (rule 2).
    if (!help_query_.empty()) {
        const std::vector<KnowledgeHit> hits = search_knowledge(k, help_query_);
        if (hits.empty())
            loc::TextDisabled("%s", tr(T::HelpNothingFound));
        for (const KnowledgeHit& h : hits) {
            ImGui::PushID(int(h.index) * 8 + int(h.kind));
            using Kind = KnowledgeHit::Kind;
            switch (h.kind) {
            case Kind::Problem:
                if (ImGui::Selectable(k.problems[h.index].name.c_str()))
                    open_problem(h.index), help_back_.clear();
                break;
            case Kind::Procedure:
                if (ImGui::Selectable(k.procedures[h.index].name.c_str()))
                    open_procedure(h.index);
                break;
            case Kind::Term:
                ImGui::TextUnformatted(k.terms[h.index].term.c_str());
                loc::TextDisabled("%s", k.terms[h.index].definition.c_str());
                break;
            case Kind::Material:
                ImGui::TextUnformatted(k.materials[h.index].name.c_str());
                break;
            case Kind::Tutorial:
                ImGui::TextUnformatted(k.tutorials[h.index].name.c_str());
                break;
            case Kind::Calibration:
                if (ImGui::Selectable(k.calibrations[h.index].name.c_str()))
                    open_guide(k.calibrations[h.index].id);
                break;
            }
            ImGui::SameLine();
            loc::TextDisabled("· %s", tr(h.kind == Kind::Problem     ? T::HelpProblems
                                           : h.kind == Kind::Procedure ? T::HelpProcedures
                                           : h.kind == Kind::Term      ? T::HelpGlossary
                                           : h.kind == Kind::Tutorial  ? T::HelpTutorials
                                           : h.kind == Kind::Calibration ? T::HelpCalibrations
                                                                       : T::HelpMaterial));
            ImGui::PopID();
        }
        return;
    }
    if (help_tab_ == 3) {
        // The interface's own tour is here whether or not the data came.
        ImGui::TextUnformatted(tr(T::HelpTour));
        loc::TextDisabled("%s", tr(T::HelpTourAbout));
        if (ImGui::Button(tr(T::HelpStart))) {
            tour_step_ = 0;
            help_open_ = false;
        }
        ImGui::Separator();
        for (std::size_t i = 0; i < k.tutorials.size(); ++i) {
            const KnowledgeTutorial& t = k.tutorials[i];
            ImGui::PushID(int(i));
            ImGui::TextUnformatted(t.name.c_str());
            loc::TextDisabled("%s", format(tr(T::HelpTutorialSteps), int(t.steps.size()), t.minutes).c_str());
            // Where it was left, if it was (rule 18).
            const auto left = guide_memory_.step.find(t.id);
            if (ImGui::Button(tr(left != guide_memory_.step.end() ? T::GuideContinue : T::HelpStart))) {
                end_guide(false);
                guide_ = start_guide(k, i, guide_memory_);
                help_open_ = false;
            }
            ImGui::PopID();
            ImGui::Spacing();
        }
    }
    if (k.empty()) {
        loc::TextDisabled("%s", tr(T::HelpNoData));
        for (const std::string& e : k.errors)
            loc::TextColored(rgb(theme_.paint_block), "%s", e.c_str());
        return;
    }
    if (help_tab_ == 0) {
        for (std::size_t i = 0; i < k.problems.size(); ++i) {
            ImGui::PushID(int(i));
            if (ImGui::Selectable(k.problems[i].name.c_str()))
                open_problem(i), help_back_.clear();
            loc::TextDisabled("%s", k.problems[i].symptoms.c_str());
            ImGui::PopID();
            ImGui::Spacing();
        }
    } else if (help_tab_ == 1) {
        for (std::size_t i = 0; i < k.procedures.size(); ++i) {
            ImGui::PushID(int(i));
            if (ImGui::Selectable(k.procedures[i].name.c_str()))
                open_procedure(i);
            loc::TextDisabled("%s", k.procedures[i].for_cases.c_str());
            ImGui::PopID();
            ImGui::Spacing();
        }
    } else if (help_tab_ == 2) {
        for (const KnowledgeTerm& t : k.terms) {
            ImGui::TextUnformatted(t.term.c_str());
            loc::TextDisabled("%s", t.definition.c_str());
            ImGui::Spacing();
        }
    } else if (help_tab_ == 4) {
        // The guides in their order, the filament's and then the printer's (rule 20).
        for (const char* group : {"filament", "printer"}) {
            ImGui::SeparatorText(tr(std::string_view(group) == "filament" ? T::GuidesFilament : T::GuidesPrinter));
            for (std::size_t i = 0; i < k.calibrations.size(); ++i) {
                const KnowledgeCalibration& g = k.calibrations[i];
                if ((g.group == "printer") != (std::string_view(group) == "printer"))
                    continue;
                ImGui::PushID(int(i));
                if (ImGui::Selectable(g.name.c_str()))
                    open_guide(g.id);
                loc::TextDisabled("%s", g.why.c_str());
                ImGui::PopID();
                ImGui::Spacing();
            }
        }
    }
}

// One problem (rules 6 to 8): its symptom, and its causes with the most
// common first and the last resorts at the end, each with its question and
// what to do.
void App::help_problem(const KnowledgeProblem& p) {
    if (ImGui::SmallButton(tr(T::HelpBack))) {
        if (!help_back_.empty()) {
            help_item_ = help_back_.back();
            help_back_.pop_back();
        } else {
            help_page_ = HelpPage::List;
        }
        return;
    }
    ImGui::PushFont(nullptr, kTitleSize * 0.8f);
    ImGui::TextUnformatted(p.name.c_str());
    ImGui::PopFont();
    loc::TextDisabled("%s", p.symptoms.c_str());
    for (const KnowledgeCause* c : causes_in_order(p.causes)) {
        ImGui::PushID(c);
        ImGui::SeparatorText(c->name.c_str());
        if (c->last_resort)
            loc::TextColored(rgb(theme_.accent_hover), "%s", tr(T::HelpLastResort));
        ImGui::TextUnformatted(c->explain.c_str());
        if (!c->check.empty())
            loc::TextDisabled("%s", c->check.c_str());
        for (std::size_t i = 0; i < c->actions.size(); ++i) {
            ImGui::PushID(int(i));
            help_action(c->actions[i]);
            ImGui::PopID();
        }
        ImGui::PopID();
    }
}

// What a cause's action offers (rules 7 and 8).
void App::help_action(const KnowledgeAction& a) {
    const Knowledge& k = *knowledge_;
    if (a.type == "setting") {
        const std::optional<SettingAction> action = setting_action(a);
        std::optional<SettingChange> change;
        try {
            if (action) {
                const auto& docs = setting_docs();
                const auto doc = docs.find(a.key);
                change = plan_setting(settings_, *action, doc == docs.end() ? SettingRange{} : parse_range(doc->second.range));
            }
        } catch (const std::exception&) {
            change.reset();
        }
        if (!change)
            return; // a key the engine does not have: no button (rule 7)
        ImGui::Bullet();
        ImGui::TextUnformatted((key_label(a.key) + ": " + change->before + " » " + change->after).c_str());
        if (!a.why.empty())
            loc::TextDisabled("%s", a.why.c_str());
        if (ImGui::SmallButton(tr(T::HelpApply))) {
            help_change_ = change;
            help_change_open_ = true;
            help_change_guide_.clear();
        }
    } else if (a.type == "procedure") {
        for (std::size_t i = 0; i < k.procedures.size(); ++i)
            if (k.procedures[i].id == a.id && ImGui::SmallButton((std::string(tr(T::HelpOpen)) + " " + k.procedures[i].name).c_str())) {
                help_page_ = HelpPage::Procedure, help_item_ = i, help_tab_ = 1, help_select_ = true;
                help_steps_done_.assign(k.procedures[i].steps.size(), false);
            }
    } else if (a.type == "problem") {
        for (std::size_t i = 0; i < k.problems.size(); ++i)
            if (k.problems[i].id == a.id && ImGui::SmallButton((std::string(tr(T::HelpSee)) + " " + k.problems[i].name).c_str())) {
                help_back_.push_back(help_item_);
                help_item_ = i;
            }
    } else if (a.type == "calibration") {
        if (ImGui::SmallButton(tr(T::HelpOpenCalibration))) {
            if (find_guide(k, a.id))
                open_guide(a.id);
            else
                tab_ = Tab::Calibration;
        }
    } else if (!a.text.empty()) {
        ImGui::Bullet();
        ImGui::TextUnformatted(a.text.c_str());
    }
}

// The guide with this id open in the help panel (rule 20).
void App::open_guide(std::string_view id) {
    if (!knowledge_)
        load_help_data();
    for (std::size_t i = 0; i < knowledge_->calibrations.size(); ++i)
        if (knowledge_->calibrations[i].id == id) {
            help_open_ = true;
            help_tab_ = 4, help_select_ = true, help_page_ = HelpPage::Guide, help_item_ = i;
            help_steps_done_.assign(knowledge_->calibrations[i].steps.size(), false);
            guide_input_ = {};
            guide_proposed_.reset();
            // Start, step and band from the last test of this guide the
            // engine generated (rule 23); the person types them otherwise.
            if (const auto kind = calibration_kind_from_name(test_for_guide(id))) {
                const TowerRange& r = ranges_[int(*kind)];
                if (r.start != 0.0 || r.end != 0.0)
                    guide_input_.start = r.start, guide_input_.step = r.step, guide_input_.band = r.segment;
            }
            help_query_.clear();
        }
}

// The guides done on this printer (rule 22): those with a date in its
// Calibration section, by their test's name or by their own id.
std::set<std::string> App::guides_done() const {
    const std::string& printer = chosen_profile_[std::size_t(ProfileKind::Printer)];
    std::set<std::string> dated = calibrations_dated(settings_, printer);
    if (const auto records = calibration_records_.find(printer); records != calibration_records_.end())
        for (const auto& [name, record] : records->second)
            if (!record.date.empty())
                dated.insert(name);
    std::set<std::string> done;
    for (const std::string& name : dated) {
        done.insert(name);
        // A test dated on the calibration page does its guides.
        if (knowledge_)
            for (const KnowledgeCalibration& g : knowledge_->calibrations)
                if (test_for_guide(g.id) == name)
                    done.insert(g.id);
    }
    return done;
}

// One calibration guide (rules 21 to 23): what it is for, when to repeat
// it, what goes before, its steps, how to read the print, the result and
// tips, and at its foot what it is based on.
void App::help_guide(const KnowledgeCalibration& g) {
    const Knowledge& k = *knowledge_;
    if (ImGui::SmallButton(tr(T::HelpBack))) {
        help_page_ = HelpPage::List;
        return;
    }
    ImGui::PushFont(nullptr, kTitleSize * 0.8f);
    ImGui::TextUnformatted(g.name.c_str());
    ImGui::PopFont();
    ImGui::TextUnformatted(g.why.c_str());
    if (!g.repeat.empty())
        loc::TextDisabled(tr(T::GuideRepeat), g.repeat.c_str());
    // Rule 22: what is not done yet goes before; it warns, it does not stop.
    for (const std::string& id : guides_missing(g.before, guides_done())) {
        const KnowledgeCalibration* before = find_guide(k, id);
        ImGui::PushID(id.c_str());
        loc::TextColored(rgb(kWarningColour), tr(T::GuideBefore), before ? before->name.c_str() : id.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton(tr(T::HelpOpen))) {
            if (const std::string_view test = test_for_guide(id); !test.empty()) {
                if (const auto kind = calibration_kind_from_name(test)) {
                    set_tab(Tab::Calibration);
                    create_calibration(*kind, false);
                }
            } else {
                open_guide(id);
            }
            ImGui::PopID();
            return;
        }
        ImGui::PopID();
    }
    const std::string_view test = test_for_guide(g.id);
    if (!test.empty() && ImGui::Button(tr(T::GuideCalibrate))) {
        if (const auto kind = calibration_kind_from_name(test)) {
            set_tab(Tab::Calibration);
            create_calibration(*kind, false);
        }
    }
    if (!g.steps.empty()) {
        ImGui::SeparatorText(tr(T::GuideSteps));
        help_steps_done_.resize(g.steps.size(), false);
        for (std::size_t i = 0; i < g.steps.size(); ++i) {
            ImGui::PushID(int(i));
            bool done = help_steps_done_[i];
            const std::string label = g.steps[i].title.empty() ? std::to_string(i + 1) + "." : g.steps[i].title;
            if (ImGui::Checkbox(label.c_str(), &done))
                help_steps_done_[i] = done;
            ImGui::Indent();
            loc::TextDisabled("%s", g.steps[i].text.c_str());
            ImGui::Unindent();
            ImGui::PopID();
        }
    }
    if (!g.read.empty() && ImGui::BeginTable("##read", 2, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn(tr(T::GuideLook));
        ImGui::TableSetupColumn(tr(T::GuideMeans));
        ImGui::TableHeadersRow();
        for (const KnowledgeCalibration::Reading& r : g.read) {
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(r.look.c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(r.means.c_str());
        }
        ImGui::EndTable();
    }
    // Rule 23: the setting it fixes with its value now, the formula, and
    // with what was measured the new value to apply; or the printer's.
    ImGui::SeparatorText(tr(T::GuideResult));
    if (!g.result_text.empty())
        ImGui::TextUnformatted(g.result_text.c_str());
    if (g.result_where == "printer_firmware") {
        loc::TextColored(rgb(kWarningColour), "%s", tr(T::GuideFirmware));
        // AS18: with the printer connected, its firmware's command.
        if (firmware_command(g.id, settings_.flavor, 1.0)) {
            ImGui::SetNextItemWidth(ImGui::GetFontSize() * 7.0f);
            ImGui::InputDouble(tr(T::GuideFrequency), &guide_input_.measured, 0.0, 0.0, "%g");
            if (const std::optional<std::string> command =
                    firmware_command(g.id, settings_.flavor, guide_input_.measured)) {
                loc::TextDisabled("%s", command->c_str());
                const bool connected = printer_ && printer_->status().state != device::PrinterState::Disconnected &&
                                       printer_->status().state != device::PrinterState::Error;
                if (connected && ImGui::Button(tr(T::GuideSend))) {
                    if (printer_->send(*command)) {
                        record_calibration(g.id);
                        settings_changed();
                        notify(format(tr(T::GuideSent), command->c_str()));
                    }
                }
            }
        }
    } else if (const std::optional<GuideFormula> f = guide_formula(g.id)) {
        // The setting comes from rule 23's table, in the engine's names.
        const std::string key(f->key);
        std::optional<SettingChange> now;
        try {
            now = plan_setting(settings_, {key, SettingOp::Multiply, "1", {}});
        } catch (const std::exception&) {
        }
        if (now)
            loc::TextDisabled(tr(T::GuideNow), (key_label(key) + " " + now->before).c_str());
        const float field = ImGui::GetFontSize() * 7.0f;
        const auto input = [&](T label, double& v) {
            ImGui::SetNextItemWidth(field);
            ImGui::InputDouble(tr(label), &v, 0.0, 0.0, "%g");
        };
        if (f->measure == GuideMeasure::Height) {
            input(T::GuideStart, guide_input_.start);
            input(T::GuideStepPerBand, guide_input_.step);
            input(T::GuideBand, guide_input_.band);
        }
        input(f->measure == GuideMeasure::Height   ? T::GuideHeight
              : f->measure == GuideMeasure::Layers ? T::GuideLayers
                                                   : T::GuideValue,
              guide_input_.measured);
        const std::optional<GuideOutcome> r = guide_result(g.id, guide_input_, settings_.layer_height);
        if (r && guide_input_.measured > 0.0) {
            // The proposal follows what was written, and can be corrected before Apply.
            if (!guide_proposed_ || guide_from_.start != guide_input_.start || guide_from_.step != guide_input_.step ||
                guide_from_.band != guide_input_.band || guide_from_.measured != guide_input_.measured) {
                guide_proposed_ = r->proposed;
                guide_from_ = guide_input_;
            }
            char value[32];
            std::snprintf(value, sizeof value, "%s", format("%g", std::round(r->value * 1e4) / 1e4).c_str());
            loc::TextColored(rgb(theme_.accent_hover), tr(T::GuideReadValue), value);
            ImGui::SetNextItemWidth(field);
            ImGui::InputDouble(tr(T::GuideProposed), &*guide_proposed_, 0.0, 0.0, "%g");
            std::optional<SettingChange> change;
            try {
                char proposed[32];
                std::snprintf(proposed, sizeof proposed, "%.6g", *guide_proposed_);
                const auto& docs = setting_docs();
                const auto doc = docs.find(key);
                change = plan_setting(settings_, {key, SettingOp::Set, proposed, {}},
                                      doc == docs.end() ? SettingRange{} : parse_range(doc->second.range));
            } catch (const std::exception&) {
            }
            if (change && ImGui::Button(tr(T::HelpApply))) {
                help_change_ = change;
                help_change_open_ = true;
                help_change_guide_ = g.id;
            }
        } else {
            guide_proposed_.reset();
        }
    }
    if (!g.tips.empty()) {
        ImGui::SeparatorText(tr(T::GuideTips));
        for (const std::string& t : g.tips)
            loc::BulletText("%s", t.c_str());
    }
    if (!g.open.empty())
        for (std::size_t i = 0; i < k.procedures.size(); ++i)
            if (k.procedures[i].id == g.open &&
                ImGui::SmallButton((std::string(tr(T::HelpOpen)) + " " + k.procedures[i].name).c_str())) {
                help_page_ = HelpPage::Procedure, help_item_ = i, help_tab_ = 1, help_select_ = true;
                help_steps_done_.assign(k.procedures[i].steps.size(), false);
            }
    // Rule 21: at its foot, what it is based on.
    if (!g.sources.empty()) {
        ImGui::Spacing();
        ImGui::PushFont(nullptr, ImGui::GetFontSize() * 0.85f);
        loc::TextDisabled("%s", tr(T::BasedOn));
        ImGui::PushStyleColor(ImGuiCol_TextLink, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        for (const KnowledgeSource& src : g.sources) {
            const std::string label = src.author.empty() ? src.title : src.title + " (" + src.author + ")";
            ImGui::Bullet();
            if (!src.url.empty())
                ImGui::TextLinkOpenURL(label.c_str(), src.url.c_str());
            else
                loc::TextDisabled("%s", label.c_str());
        }
        ImGui::PopStyleColor();
        ImGui::PopFont();
    }
}

// One procedure (rule 10): its warnings first, its steps to tick, then how to
// avoid it and its notes.
void App::help_procedure(const KnowledgeProcedure& p) {
    if (ImGui::SmallButton(tr(T::HelpBack))) {
        help_page_ = HelpPage::List;
        return;
    }
    ImGui::PushFont(nullptr, kTitleSize * 0.8f);
    ImGui::TextUnformatted(p.name.c_str());
    ImGui::PopFont();
    if (!p.applies_to.empty())
        loc::TextDisabled("%s", p.applies_to.c_str());
    for (const std::string& w : p.warnings)
        loc::TextColored(rgb(kWarningColour), "%s", w.c_str());
    help_steps_done_.resize(p.steps.size(), false);
    for (std::size_t i = 0; i < p.steps.size(); ++i) {
        ImGui::PushID(int(i));
        bool done = help_steps_done_[i];
        if (ImGui::Checkbox(p.steps[i].title.c_str(), &done))
            help_steps_done_[i] = done;
        ImGui::Indent();
        loc::TextDisabled("%s", p.steps[i].text.c_str());
        ImGui::Unindent();
        ImGui::PopID();
    }
    if (!p.prevention.empty()) {
        ImGui::SeparatorText(tr(T::HelpPrevention));
        for (const std::string& t : p.prevention)
            loc::BulletText("%s", t.c_str());
    }
    if (!p.notes.empty()) {
        ImGui::SeparatorText(tr(T::HelpNotes));
        for (const std::string& t : p.notes)
            loc::BulletText("%s", t.c_str());
    }
}

const KnowledgeMaterial* App::current_material() const {
    if (!knowledge_ || knowledge_->materials.empty())
        return nullptr;
    std::vector<MaterialInfo> infos;
    for (const KnowledgeMaterial& m : knowledge_->materials)
        infos.push_back(material_info(m));
    const MaterialInfo* found =
        find_material(infos, {}, chosen_profile_[std::size_t(ProfileKind::Filament)]);
    return found ? &knowledge_->materials[std::size_t(found - infos.data())] : nullptr;
}

// The material's card under the filament's profile (rules 13 and 14): its
// temperatures, bed, chamber, fan, surfaces, retraction and drying, and what
// this printer lacks for it.
void App::material_card() {
    const KnowledgeMaterial* m = current_material();
    if (!m || !assistant_material_)
        return;
    const auto range = [](const KnowledgeRange& r, const char* unit) {
        if (!r.known)
            return std::string("—");
        char text[64];
        if (r.min == r.max)
            std::snprintf(text, sizeof text, "%s", format("%g %s", r.min, unit).c_str());
        else
            std::snprintf(text, sizeof text, "%s", format("%g%s%g %s", r.min, tr(T::RangeTo), r.max, unit).c_str());
        return std::string(text);
    };
    const std::vector<MaterialWarning> warnings = material_warnings(material_info(*m), settings_);
    const std::string title = format(tr(T::MaterialCard), m->name.c_str()) + "###material_card";
    if (!warnings.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, rgb(kWarningColour));
        ImGui::SetNextItemOpen(true, ImGuiCond_Once); // open while it has something to say
    }
    const bool open = ImGui::CollapsingHeader(title.c_str());
    if (!warnings.empty())
        ImGui::PopStyleColor();
    if (!open)
        return;
    ImGui::PushTextWrapPos(0.0f);
    for (const MaterialWarning w : warnings)
        loc::TextColored(rgb(kWarningColour), "%s", material_warning_text(w, material_info(*m)).c_str());
    loc::TextDisabled("%s · %s", m->family.c_str(), m->category.c_str());
    loc::Text("%s: %s", tr(T::NozzleTemp), range(m->nozzle_temp, "°C").c_str());
    loc::Text("%s: %s", tr(T::BedTemp), range(m->bed_temp, "°C").c_str());
    if (m->base && m->base->chamber_temp)
        loc::Text("%s: %g °C", tr(T::MaterialChamber), *m->base->chamber_temp);
    if (m->fan.known)
        loc::Text("%s: %s", tr(T::MaterialFan), range(m->fan, "%").c_str());
    if (!m->enclosure.empty())
        loc::Text("%s: %s", tr(T::MaterialEnclosure), m->enclosure.c_str());
    loc::Text("%s: %s", tr(T::MaterialNozzle), m->nozzle_material.c_str());
    if (!m->surfaces.empty()) {
        std::string list;
        for (const std::string& s : m->surfaces)
            list += (list.empty() ? "" : ", ") + s;
        loc::Text("%s: %s", tr(T::BedSurfaces), list.c_str());
    }
    // Direct or Bowden: the engine does not know the printer's extruder yet,
    // so both are shown.
    if (m->base && (m->base->retraction_direct || m->base->retraction_bowden)) {
        std::string r;
        if (m->base->retraction_direct)
            r += format(tr(T::MaterialDirect), *m->base->retraction_direct);
        if (m->base->retraction_bowden)
            r += (r.empty() ? "" : ", ") + format(tr(T::MaterialBowden), *m->base->retraction_bowden);
        loc::Text("%s: %s", tr(T::MaterialRetraction), r.c_str());
    }
    if (m->drying) {
        std::string d = range(m->drying->temp, "°C");
        if (m->drying->hours_before_printing)
            d += ", " + std::to_string(*m->drying->hours_before_printing) + " h";
        loc::Text("%s: %s", tr(T::MaterialDrying), d.c_str());
    }
    ImGui::PopTextWrapPos();
    ImGui::Spacing();
}

void App::anchor(const char* name) { anchor_rect(name, ImGui::GetItemRectMin(), ImGui::GetItemRectMax()); }

void App::anchor_rect(const char* name, ImVec2 min, ImVec2 max) { anchors_[name] = {min.x, min.y, max.x, max.y}; }

// Something the tutorials wait for happened (rule 16, «Eventos»).
void App::fire(std::string_view event) {
    if (!guide_ || !knowledge_)
        return;
    if (guide_event(*knowledge_, *guide_, event))
        end_guide(true);
}

void App::offer_tutorials(std::string_view when) {
    if (!knowledge_ || !assistant_tutorials_)
        return;
    for (const std::size_t i : tutorials_to_offer(*knowledge_, when, guide_memory_))
        if (std::find(guide_offers_.begin(), guide_offers_.end(), i) == guide_offers_.end())
            guide_offers_.push_back(i);
}

void App::end_guide(bool finished) {
    if (guide_ && knowledge_)
        remember(guide_memory_, *knowledge_, *guide_, finished);
    guide_.reset();
}

// The tutorial going on, or one offered (rules 15 to 17): the part its step
// names stays lit and the rest goes dark, with a bubble beside it; a part
// that is not on screen opens its panel, and without one the bubble sits in
// the middle. The dark layer takes no clicks, so the step can be done.
void App::guide_overlay() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float width = 360.0f * scale_;
    if (!guide_ && !guide_offers_.empty() && knowledge_) {
        // «¿Te muestro cómo?»: accept, later or never.
        const std::size_t t = guide_offers_.front();
        ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x - 16.0f * scale_, vp->Pos.y + vp->Size.y - 60.0f * scale_),
                                ImGuiCond_Always, ImVec2(1.0f, 1.0f));
        ImGui::SetNextWindowSize(ImVec2(width, 0.0f));
        ImGui::Begin("##guide_offer", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
        loc::TextColored(rgb(theme_.accent_hover), "%s", tr(T::GuideOffer));
        ImGui::TextUnformatted(knowledge_->tutorials[t].name.c_str());
        loc::TextDisabled("%s", format(tr(T::HelpTutorialSteps), int(knowledge_->tutorials[t].steps.size()),
                                         knowledge_->tutorials[t].minutes)
                                      .c_str());
        if (ImGui::Button(tr(T::GuideAccept))) {
            guide_ = start_guide(*knowledge_, t, guide_memory_);
            guide_offers_.erase(guide_offers_.begin());
        }
        ImGui::SameLine();
        if (ImGui::Button(tr(T::GuideLater)))
            guide_offers_.erase(guide_offers_.begin());
        ImGui::SameLine();
        if (ImGui::Button(tr(T::GuideNever))) {
            guide_memory_.dismissed.insert(knowledge_->tutorials[t].id);
            guide_offers_.erase(guide_offers_.begin());
        }
        ImGui::End();
        return;
    }
    if (!guide_ || !knowledge_ || guide_->tutorial >= knowledge_->tutorials.size())
        return;
    const KnowledgeTutorial& tutorial = knowledge_->tutorials[guide_->tutorial];
    if (guide_->step >= int(tutorial.steps.size())) {
        end_guide(true);
        return;
    }
    const KnowledgeTutorial::Step& step = tutorial.steps[std::size_t(guide_->step)];
    // A part not on screen opens its panel (rule 17).
    auto found = anchors_.find(step.anchor);
    if (found == anchors_.end() && !guide_opened_panel_) {
        guide_opened_panel_ = true;
        if (step.anchor == "filament_selector")
            panel_tab_ = SettingsTab::Material;
        else if (step.anchor == "help_problems" || step.anchor == "help_procedures")
            help_open_ = true;
        else if (step.anchor.starts_with("engineering_"))
            set_tab(Tab::Analysis);
        else if (step.anchor == "calibrations")
            set_tab(Tab::Calibration);
    } else if (found != anchors_.end()) {
        guide_opened_panel_ = false;
    }
    // The dark layer around the part.
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::Begin("##guide_dim", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 dark = IM_COL32(0, 0, 0, 140);
    const ImVec2 lo = vp->Pos, hi(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y);
    ImVec2 a, b;
    if (found != anchors_.end()) {
        const float pad = 4.0f * scale_;
        a = ImVec2(found->second[0] - pad, found->second[1] - pad), b = ImVec2(found->second[2] + pad, found->second[3] + pad);
        dl->AddRectFilled(lo, ImVec2(hi.x, a.y), dark);
        dl->AddRectFilled(ImVec2(lo.x, b.y), hi, dark);
        dl->AddRectFilled(ImVec2(lo.x, a.y), ImVec2(a.x, b.y), dark);
        dl->AddRectFilled(ImVec2(b.x, a.y), ImVec2(hi.x, b.y), dark);
        dl->AddRect(a, b, ImGui::GetColorU32(rgb(theme_.accent_hover)), 6.0f * scale_, 2.5f * scale_);
    } else {
        dl->AddRectFilled(lo, hi, dark);
    }
    ImGui::End();
    // The bubble: beside the part where it fits, else in the middle.
    ImVec2 at(vp->Pos.x + 0.5f * vp->Size.x, vp->Pos.y + 0.5f * vp->Size.y), pivot(0.5f, 0.5f);
    if (found != anchors_.end()) {
        if (b.x + 12.0f * scale_ + width < hi.x)
            at = ImVec2(b.x + 12.0f * scale_, std::max(a.y, lo.y + 60.0f * scale_)), pivot = ImVec2(0.0f, 0.0f);
        else if (a.x - 12.0f * scale_ - width > lo.x)
            at = ImVec2(a.x - 12.0f * scale_, std::max(a.y, lo.y + 60.0f * scale_)), pivot = ImVec2(1.0f, 0.0f);
        else if (b.y + 200.0f * scale_ < hi.y)
            at = ImVec2(std::min(a.x, hi.x - width - 8.0f * scale_), b.y + 12.0f * scale_), pivot = ImVec2(0.0f, 0.0f);
    }
    ImGui::SetNextWindowPos(at, ImGuiCond_Always, pivot);
    ImGui::SetNextWindowSize(ImVec2(width, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f * scale_);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f * scale_, 12.0f * scale_));
    ImGui::PushStyleColor(ImGuiCol_Border, rgb(theme_.accent));
    ImGui::Begin("##guide", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
    loc::TextDisabled("%s · %d / %d", tutorial.name.c_str(), guide_->step + 1, int(tutorial.steps.size()));
    ImGui::PushFont(nullptr, kTitleSize * 0.9f);
    loc::TextColored(rgb(theme_.accent_hover), "%s", step.title.c_str());
    ImGui::PopFont();
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(step.text.c_str());
    if (!step.wait_for.empty())
        loc::TextDisabled("%s", tr(T::GuideWaits));
    ImGui::PopTextWrapPos();
    // A step that opens something (rule 16).
    if (!step.open_type.empty() && ImGui::Button(tr(T::HelpOpen))) {
        if (step.open_type == "procedure") {
            for (std::size_t i = 0; i < knowledge_->procedures.size(); ++i)
                if (knowledge_->procedures[i].id == step.open_id) {
                    help_open_ = true;
                    help_tab_ = 1, help_select_ = true, help_page_ = HelpPage::Procedure, help_item_ = i;
                    help_steps_done_.assign(knowledge_->procedures[i].steps.size(), false);
                }
        } else if (tutorial.id == "new_material" && !next_material_guide(guides_done()).empty()) {
            // Rule 24: the calibrations in the guides' order, the first not done.
            open_guide(next_material_guide(guides_done()));
        } else if (find_guide(*knowledge_, step.open_id)) {
            open_guide(step.open_id);
        } else {
            set_tab(Tab::Calibration);
        }
    }
    ImGui::Spacing();
    const bool last = guide_->step + 1 == int(tutorial.steps.size());
    if (guide_->step > 0) {
        if (ImGui::Button(tr(T::Back)))
            guide_back(*guide_);
        ImGui::SameLine();
    }
    if (ImGui::Button(tr(last ? T::Finish : T::Next)) && guide_next(*knowledge_, *guide_)) {
        end_guide(true);
    } else {
        ImGui::SameLine();
        if (ImGui::Button(tr(T::GuideExit)))
            end_guide(false);
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

// Applying a setting from the help (rule 7): the setting, its value now and
// after, the limit when it was cut, and Undo afterwards.
void App::help_apply_dialog() {
    if (help_change_open_) {
        ImGui::OpenPopup("###help_apply");
        help_change_open_ = false;
    }
    const std::string title = std::string(tr(T::HelpApply)) + "###help_apply";
    if (!ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;
    if (help_change_) {
        const SettingChange& c = *help_change_;
        ImGui::TextUnformatted(key_label(c.key).c_str());
        loc::Text("%s » %s", c.before.c_str(), c.after.c_str());
        if (c.clamped)
            loc::TextColored(rgb(kWarningColour), "%s", format(tr(T::HelpLimitUsed), c.after.c_str()).c_str());
        if (ImGui::Button(tr(T::HelpApply), ImVec2(120.0f * scale_, 0))) {
            try {
                const Settings before = settings_;
                settings_ = apply_setting(settings_, c);
                // A guide's result dates the guide and its test, as the
                // calibration page does (rule 22); Undo takes the date back too.
                if (!help_change_guide_.empty()) {
                    record_calibration(help_change_guide_);
                    if (const std::string_view test = test_for_guide(help_change_guide_);
                        calibration_kind_from_name(test))
                        record_calibration(std::string(test));
                }
                settings_changed();
                notify_undo(key_label(c.key) + ": " + c.before + " » " + c.after, [this, before] {
                    settings_ = before;
                    settings_changed();
                });
            } catch (const std::exception& e) {
                notify(e.what(), true);
            }
            help_change_.reset();
            help_change_guide_.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
    }
    if (ImGui::Button(tr(T::Cancel), ImVec2(120.0f * scale_, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        help_change_.reset();
        help_change_guide_.clear();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

// More › Credits and sources (credits.md, rule 4): the functions taken from
// other sources, and the third-party components with their licenses and the
// notices those licenses ask to hand out with the program.
void App::credits_dialog() {
    if (credits_open_) {
        ImGui::OpenPopup("###credits");
        credits_open_ = false;
    }
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(std::min(980.0f * scale_, vp->Size.x * 0.92f), vp->Size.y * 0.85f),
                             ImGuiCond_Appearing);
    const std::string title = std::string(tr(T::Credits)) + "###credits";
    if (!ImGui::BeginPopupModal(title.c_str()))
        return;
    const Credits& all = credits();
    loc::TextWrapped("%s", tr(T::CreditsIntro));
    const float bottom = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
    ImGui::BeginChild("##credits_body", ImVec2(0, -bottom));
    constexpr ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                                      ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX;
    ImGui::SeparatorText(tr(T::CreditsFeatures));
    if (ImGui::BeginTable("##features", 4, flags)) {
        ImGui::TableSetupColumn(tr(T::CreditsFunction), ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn(tr(T::CreditsSource), ImGuiTableColumnFlags_WidthStretch, 1.6f);
        ImGui::TableSetupColumn(tr(T::CreditsAuthor), ImGuiTableColumnFlags_WidthStretch, 0.9f);
        ImGui::TableSetupColumn(tr(T::CreditsLicense), ImGuiTableColumnFlags_WidthStretch, 0.9f);
        ImGui::TableHeadersRow();
        for (const FeatureCredit& c : all.features) {
            ImGui::PushID(&c);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            loc::TextWrapped("%s", c.feature.now().c_str());
            ImGui::TableNextColumn();
            // A link that wraps: a paper's title is long.
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextLink));
            loc::TextWrapped("%s", c.source.c_str());
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                loc::SetTooltip("%s", c.url.c_str());
            }
            if (ImGui::IsItemClicked() && ImGui::GetPlatformIO().Platform_OpenInShellFn)
                ImGui::GetPlatformIO().Platform_OpenInShellFn(ImGui::GetCurrentContext(), c.url.c_str());
            ImGui::TableNextColumn();
            loc::TextWrapped("%s", c.author.c_str());
            ImGui::TableNextColumn();
            loc::TextWrapped("%s", c.license.now().c_str());
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::Spacing();
    ImGui::SeparatorText(tr(T::CreditsLibraries));
    if (ImGui::BeginTable("##libraries", 4, flags)) {
        ImGui::TableSetupColumn(tr(T::CreditsComponent), ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn(tr(T::CreditsVersion), ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn(tr(T::CreditsLibraryLicense), ImGuiTableColumnFlags_WidthStretch, 0.9f);
        ImGui::TableSetupColumn(tr(T::CreditsNotice), ImGuiTableColumnFlags_WidthStretch, 0.9f);
        ImGui::TableHeadersRow();
        for (std::size_t i = 0; i < all.libraries.size(); ++i) {
            const LibraryCredit& lib = all.libraries[i];
            ImGui::PushID(int(i));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextLinkOpenURL(lib.name.c_str(), lib.url.c_str());
            if (ImGui::IsItemHovered())
                loc::SetTooltip("%s", lib.url.c_str());
            ImGui::TableNextColumn();
            // A commit is long: its first twelve digits say which.
            ImGui::TextUnformatted(lib.version.size() > 24 ? (lib.version.substr(0, 12) + "…").c_str()
                                                             : lib.version.c_str());
            ImGui::TableNextColumn();
            loc::TextWrapped("%s", lib.license.c_str());
            ImGui::TableNextColumn();
            const bool shown = credits_notice_ == int(i);
            if (ImGui::Selectable(tr(T::CreditsShowNotice), shown))
                credits_notice_ = shown ? -1 : int(i);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (credits_notice_ >= 0 && credits_notice_ < int(all.libraries.size())) {
        const LibraryCredit& lib = all.libraries[std::size_t(credits_notice_)];
        ImGui::SeparatorText((lib.name + " — " + tr(T::CreditsNotice)).c_str());
        for (const std::string& notice : lib.notices) {
            ImGui::PushID(notice.c_str());
            ImGui::BeginChild("##notice", ImVec2(0, 220.0f * scale_), ImGuiChildFlags_Borders);
            ImGui::PushFont(nullptr, ImGui::GetFontSize() * 0.85f);
            const std::string_view text = embedded(notice);
            ImGui::TextUnformatted(text.data(), text.data() + text.size());
            ImGui::PopFont();
            ImGui::EndChild();
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
    if (ImGui::Button(tr(T::Close), ImVec2(120.0f * scale_, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
        ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

// A section in a column of sections (settings_panel.md, rule 1): its icon
// and name, the chosen one in violet, and a dot where something differs from
// the profile. True when clicked; the caller's tooltip asks IsItemHovered.
bool App::section_button(std::string_view id, const char* name, const char* icon_name, bool selected, bool marked,
                         float column) {
    ImGui::PushID(id.data(), id.data() + id.size());
    const ImVec2 at = ImGui::GetCursorScreenPos();
    const float icon_size = 22.0f * scale_, small = ImGui::GetFontSize() * 0.78f;
    const float tall = icon_size + small + 16.0f * scale_;
    const bool clicked = ImGui::InvisibleButton("##section", ImVec2(column - 6.0f * scale_, tall)) && !selected;
    const bool hovered = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (selected || hovered)
        dl->AddRectFilled(at, ImVec2(at.x + column - 6.0f * scale_, at.y + tall),
                          ImGui::GetColorU32(rgb(selected ? theme_.accent : theme_.frame_hover, selected ? 0.28f : 0.6f)),
                          7.0f * scale_);
    const ImU32 tint = ImGui::GetColorU32(rgb(selected ? theme_.lavender : theme_.text_dim));
    const float cx = at.x + (column - 6.0f * scale_) * 0.5f;
    dl->AddImage(icon(icon_name), ImVec2(cx - icon_size * 0.5f, at.y + 6.0f * scale_),
                 ImVec2(cx + icon_size * 0.5f, at.y + 6.0f * scale_ + icon_size), ImVec2(0, 0), ImVec2(1, 1), tint);
    const ImVec2 text = ImGui::GetFont()->CalcTextSizeA(small, FLT_MAX, 0.0f, name);
    dl->AddText(ImGui::GetFont(), small, ImVec2(cx - text.x * 0.5f, at.y + 10.0f * scale_ + icon_size), tint, name);
    if (marked)
        dl->AddCircleFilled(ImVec2(at.x + column - 16.0f * scale_, at.y + 8.0f * scale_), 3.5f * scale_,
                            ImGui::GetColorU32(rgb(theme_.accent_hover)));
    ImGui::PopID();
    return clicked;
}

// A section's cards: each with its rows when open, its Reset (rule 8), and
// at its foot what the level hides (rule 11) or, in Custom, the settings to
// add back (rule 16).
void App::section_cards(const SectionLayout& section, bool& changed, bool& bed) {
    bool first = true;
    for (const SubsectionLayout& sub : section.subsections) {
        if (!card_shown(sub))
            continue;
        const int changes = int(std::count_if(sub.settings.begin(), sub.settings.end(), [&](const SettingEntry& e) {
            return changed_keys_.contains(std::string(e.key));
        }));
        const ImVec2 start = ImGui::GetCursorScreenPos();
        const float card_width = ImGui::GetContentRegionAvail().x;
        bool reset = false;
        const bool open = settings_card(sub, first, changes, subsection_summary(sub.id, settings_), reset);
        if (reset) {
            // Back to the saved profile, this card's settings only (rule 8).
            std::vector<std::string> keys;
            for (const SettingEntry& e : sub.settings)
                keys.emplace_back(e.key);
            try {
                settings_ = with_keys(settings_, saved_settings(), keys);
                changed = true;
                bed |= sub.id == "bed_volume";
            } catch (const std::exception& e) {
                fail(T::CannotOpen, e.what());
            }
        }
        if (open) {
            ImGui::PushID(sub.id.data(), sub.id.data() + sub.id.size());
            changed |= card_rows(sub.id, bed);
            if (view_level_ == ViewLevel::Custom) {
                // Settings of this card Custom leaves out, to add back (rule 16).
                std::vector<std::string_view> hidden;
                for (const SettingEntry& e : sub.settings)
                    if (setting_applies(e.key) && !setting_shown(e.key, sub.id))
                        hidden.push_back(e.key);
                if (!hidden.empty()) {
                    ImGui::Spacing();
                    if (ImGui::SmallButton(tr(T::AddSetting)))
                        ImGui::OpenPopup("##add_setting");
                    if (ImGui::BeginPopup("##add_setting")) {
                        for (const std::string_view key : hidden)
                            if (ImGui::Selectable((key_label(key) + "##" + std::string(key)).c_str())) {
                                custom_view_.visible.insert(std::string(key));
                                custom_view_.known.insert(std::string(key));
                            }
                        ImGui::EndPopup();
                    }
                }
            } else {
                // Basic tells what it hides (rule 11).
                int hidden = 0;
                for (const SettingEntry& e : sub.settings)
                    hidden += setting_applies(e.key) && !setting_shown(e.key, sub.id);
                if (hidden > 0) {
                    ImGui::Spacing();
                    loc::TextDisabled("%s", format(tr(view_level_ == ViewLevel::Basic ? T::MoreSettings
                                                                                        : T::MoreSettingsAdvanced),
                                                     hidden)
                                                  .c_str());
                    ImGui::SameLine();
                    if (ImGui::SmallButton(tr(T::ShowAll)))
                        show_all_cards_.insert(std::string(sub.id));
                }
            }
            const float pad = 9.0f * scale_;
            card_credits(sub.id, start.x + pad, start.x + card_width - pad);
            ImGui::PopID();
        }
        const float end = ImGui::GetCursorScreenPos().y + 4.0f * scale_;
        card_heights_[std::string(sub.id)] = end - start.y;
        ImGui::SetCursorScreenPos(ImVec2(start.x, end + 7.0f * scale_));
        ImGui::Dummy(ImVec2(0, 0));
        first = false;
    }
}

// The settings panel (docs/spec/features/settings_panel.md): the Print,
// Material and Printer tabs, each with its profile, its sections in a column
// of icons and the chosen section's cards; a level (Basic, Normal, Advanced)
// says how many settings show. The level never changes the G-code.
void App::settings_panel() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float top = kTopBar * scale_, bottom = kStatusBar * scale_;
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x, vp->Pos.y + top));
    ImGui::SetNextWindowSize(ImVec2(kPanel * scale_, vp->Size.y - top - bottom));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, rgb(theme_.panel));
    ImGui::Begin("##settings", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);
    anchor_rect("quick_settings", ImGui::GetWindowPos(),
                ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowSize().x, ImGui::GetWindowPos().y + ImGui::GetWindowSize().y));
    refresh_profiles();
    bool changed = false, bed = false;

    // A setting asked with --info opens its tab (or the printer's page),
    // section and card.
    if (!g_open_info.empty())
        if (const std::optional<SettingPlace> place = setting_place(g_open_info)) {
            if (place->tab == SettingsTab::Printer)
                printer_page_ = true, printer_section_ = std::string(place->section->id);
            else
                panel_tab_ = place->tab;
            panel_section_[std::size_t(place->tab)] = std::string(place->section->id);
            card_open_[std::string(place->subsection->id)] = true;
            view_level_ = ViewLevel::Advanced;
        }
    // On top, the printer's card (rule 32); the printer's settings have a
    // page of their own, and the panel the Print and Material tabs.
    ImGui::BeginGroup();
    printer_card();
    ImGui::EndGroup();
    anchor("printer_selector");
    if (panel_tab_ == SettingsTab::Printer)
        panel_tab_ = SettingsTab::Print;

    // The tabs, and the level on their right.
    {
        const float gap = ImGui::GetStyle().ItemSpacing.x;
        // The level's chip, as wide as its longest name.
        float level_width = 0.0f;
        for (const T name : {T::LevelBasic, T::LevelNormal, T::LevelAdvanced, T::LevelCustom})
            level_width = std::max(level_width, ImGui::CalcTextSize((std::string("    ") + tr(name)).c_str()).x);
        level_width += ImGui::GetFrameHeight() + 2.0f * ImGui::GetStyle().FramePadding.x;
        const float tab_width = (ImGui::GetContentRegionAvail().x - level_width - 3.0f * gap) / 3.0f;
        // Print, Material and the plate's Objects (settings_levels.md, rule 13).
        const std::array<std::pair<SettingsTab, T>, 3> tabs = {
            {{SettingsTab::Print, T::Print}, {SettingsTab::Material, T::Material}, {SettingsTab::Printer, T::Objects}}};
        for (const auto& [tab, title] : tabs) {
            const bool objects = title == T::Objects;
            const bool chosen = objects ? objects_tab_ : !objects_tab_ && panel_tab_ == tab;
            ImGui::PushStyleColor(ImGuiCol_Button, chosen ? rgb(theme_.accent) : rgb(theme_.frame, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, chosen ? rgb(theme_.accent_hover) : rgb(theme_.frame_hover));
            ImGui::PushStyleColor(ImGuiCol_Text, chosen ? rgb(0xFFFFFF) : rgb(theme_.text_dim));
            if (ImGui::Button(tr(title), ImVec2(tab_width, 0))) {
                objects_tab_ = objects;
                if (!objects)
                    panel_tab_ = tab;
            }
            ImGui::PopStyleColor(3);
            ImGui::SameLine(0, gap);
        }
        // The level: a dot of its colour and its name.
        constexpr std::array<std::tuple<ViewLevel, T, T, std::uint32_t>, 4> levels = {
            {{ViewLevel::Basic, T::LevelBasic, T::LevelBasicTip, 0x3FBF7Fu},
             {ViewLevel::Normal, T::LevelNormal, T::LevelNormalTip, 0xE8B83Du},
             {ViewLevel::Advanced, T::LevelAdvanced, T::LevelAdvancedTip, 0xE5484Du},
             {ViewLevel::Custom, T::LevelCustom, T::LevelCustomTip, 0x9B6BFFu}}};
        std::uint32_t dot = 0xE8B83D;
        const char* name = tr(T::LevelNormal);
        for (const auto& [level, title, tip, colour] : levels)
            if (level == view_level_)
                dot = colour, name = tr(title);
        ImGui::SetNextItemWidth(level_width);
        const std::string preview = std::string("    ") + name;
        const ImVec2 at = ImGui::GetCursorScreenPos();
        if (ImGui::BeginCombo("##level", preview.c_str(), ImGuiComboFlags_HeightLarge)) {
            for (const auto& [level, title, tip, colour] : levels) {
                const ImVec2 row = ImGui::GetCursorScreenPos();
                const std::string item = std::string("    ") + tr(title) + "##" + std::to_string(int(level));
                if (ImGui::Selectable(item.c_str(), level == view_level_))
                    choose_level(level);
                if (ImGui::IsItemHovered())
                    loc::SetTooltip("%s", tr(tip));
                ImGui::GetWindowDrawList()->AddCircleFilled(
                    ImVec2(row.x + 6.0f * scale_, row.y + ImGui::GetTextLineHeight() * 0.5f), 4.0f * scale_,
                    ImGui::GetColorU32(rgb(colour)));
            }
            ImGui::Separator();
            if (ImGui::Selectable(tr(T::ChooseSettings))) {
                choose_level(ViewLevel::Custom);
                choose_draft_ = custom_view_;
                choose_open_ = true;
            }
            ImGui::EndCombo();
        }
        if (ImGui::IsItemHovered())
            loc::SetTooltip("%s", tr(T::LevelTip));
        ImGui::GetWindowDrawList()->AddCircleFilled(
            ImVec2(at.x + ImGui::GetStyle().FramePadding.x + 4.0f * scale_, at.y + ImGui::GetFrameHeight() * 0.5f),
            4.0f * scale_, ImGui::GetColorU32(rgb(dot)));
    }
    // Ctrl+Alt+1 to 4 choose the level (rule 14).
    if (ImGui::GetIO().KeyCtrl && ImGui::GetIO().KeyAlt) {
        if (ImGui::IsKeyPressed(ImGuiKey_1))
            choose_level(ViewLevel::Basic);
        if (ImGui::IsKeyPressed(ImGuiKey_2))
            choose_level(ViewLevel::Normal);
        if (ImGui::IsKeyPressed(ImGuiKey_3))
            choose_level(ViewLevel::Advanced);
        if (ImGui::IsKeyPressed(ImGuiKey_4))
            choose_level(ViewLevel::Custom);
    }
    if (objects_tab_) {
        objects_list();
        ImGui::End();
        ImGui::PopStyleColor();
        return;
    }
    profile_row(panel_tab_ == SettingsTab::Material ? ProfileKind::Filament : ProfileKind::Print);
    if (panel_tab_ == SettingsTab::Material)
        anchor("filament_selector");
    if (panel_tab_ == SettingsTab::Material)
        material_card();
    // «See all together» (rule 4), and in Custom «Choose settings…» (rule 16).
    if (ImGui::SmallButton(tr(single_column_ ? T::SeeBySection : T::SeeAllTogether)))
        single_column_ = !single_column_;
    if (view_level_ == ViewLevel::Custom) {
        ImGui::SameLine();
        if (ImGui::SmallButton(tr(T::ChooseSettings))) {
            choose_draft_ = custom_view_;
            choose_open_ = true;
        }
    }
    ImGui::Spacing();

    // The setting the search went to stays lit for 2 s.
    if (!highlight_key_.empty() && ImGui::GetTime() > highlight_until_)
        highlight_key_.clear();
    g_highlight_key = highlight_key_;
    g_highlight_colour = ImGui::GetColorU32(rgb(theme_.accent, 0.35f));
    g_scroll_highlight = scroll_to_highlight_;
    scroll_to_highlight_ = false;
    g_custom_rows = view_level_ == ViewLevel::Custom;

    const std::vector<SectionLayout>& layout = settings_layout(panel_tab_);
    std::string& chosen = panel_section_[std::size_t(panel_tab_)];
    // The sections with something to show at this level.
    std::vector<const SectionLayout*> sections;
    for (const SectionLayout& section : layout)
        if (std::any_of(section.subsections.begin(), section.subsections.end(),
                        [&](const SubsectionLayout& sub) { return card_shown(sub); }))
            sections.push_back(&section);
    if (!sections.empty() &&
        std::none_of(sections.begin(), sections.end(), [&](const SectionLayout* x) { return x->id == chosen; }))
        chosen = std::string(sections.front()->id);

    const float footer = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
    const float column = 70.0f * scale_;
    // The column of sections (all together, none).
    if (!single_column_) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::BeginChild("##sections", ImVec2(column, -footer), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
        for (const SectionLayout* section : sections) {
            const bool marked = std::any_of(section->subsections.begin(), section->subsections.end(), [&](const SubsectionLayout& sub) {
                return std::any_of(sub.settings.begin(), sub.settings.end(),
                                   [&](const SettingEntry& e) { return changed_keys_.contains(std::string(e.key)); });
            });
            const std::string icon_name(section->icon.substr(0, section->icon.size() - 4));
            if (section_button(section->id, tr(section->title), icon_name.c_str(), section->id == chosen, marked, column)) {
                chosen = std::string(section->id);
                show_all_cards_.clear();
                revealed_keys_.clear();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::TextUnformatted(tr(section->title));
                for (const SubsectionLayout& sub : section->subsections)
                    if (card_shown(sub))
                        loc::BulletText("%s", tr(sub.title));
                ImGui::EndTooltip();
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::SameLine(0, 4.0f * scale_);
    }

    // The chosen section's cards, or every section's one below the other.
    ImGui::BeginChild("##cards", ImVec2(0, -footer));
    for (const SectionLayout* section : sections) {
        if (!single_column_ && section->id != chosen)
            continue;
        if (single_column_) {
            ImGui::PushFont(nullptr, ImGui::GetFontSize() * 1.15f);
            loc::TextColored(rgb(theme_.lavender), "%s", tr(section->title));
            ImGui::PopFont();
            ImGui::Spacing();
        }
        section_cards(*section, changed, bed);
        if (single_column_)
            ImGui::Spacing();
    }
    ImGui::EndChild();
    g_custom_rows = false;

    // Settings files.
    ImGui::Separator();
    const float third = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 2.0f) / 3.0f;
    if (ImGui::Button(tr(T::LoadSettings), ImVec2(third, 0))) {
        purpose_ = Purpose::LoadSettings;
        browser_.open(FileBrowser::Mode::Open, tr(T::LoadSettingsTitle), {".json"}, last_folder_);
    }
    ImGui::SameLine();
    if (ImGui::Button(tr(T::SaveSettings), ImVec2(third, 0))) {
        purpose_ = Purpose::SaveSettings;
        browser_.open(FileBrowser::Mode::Save, tr(T::SaveSettingsTitle), {".json"}, last_folder_, "tisma.json");
    }
    ImGui::SameLine();
    if (ImGui::Button(tr(T::ResetSettings), ImVec2(third, 0))) {
        settings_ = default_settings();
        for (const ProfileKind k : kProfileKinds)
            chosen_profile_[std::size_t(k)] = builtin_profiles_[std::size_t(k)].front().name;
        bed = changed = true;
    }
    ImGui::End();
    ImGui::PopStyleColor();
    if (bed)
        bed_changed();
    if (changed)
        settings_changed();
}

// The rows of one card, those the panel shows.
bool App::card_rows(std::string_view id, bool& bed) {
    Settings& s = settings_;
    bool changed = false;
    // A row shows if the panel shows its setting; one shown only because it
    // differs from the profile carries its level.
    const auto shown = [&](const char* key) {
        // An object's window shows its own settings' rows, whatever the level.
        if (!row_only_.empty()) {
            if (!row_only_.contains(key))
                return false;
            g_row_key = key;
            return true;
        }
        if (!setting_shown(key, id))
            return false;
        const std::optional<SettingPlace> place = setting_place(key);
        if (place && !view_shows(key) && !show_all_cards_.contains(std::string(id)))
            g_row_tag = tr(place->level == SettingLevel::Basic    ? T::LevelBasic
                           : place->level == SettingLevel::Normal ? T::LevelNormal
                                                                  : T::LevelAdvanced);
        g_row_key = key;
        return true;
    };
    if (!ImGui::BeginTable("##rows", 2, ImGuiTableFlags_SizingStretchProp,
                           ImVec2(ImGui::GetContentRegionAvail().x - 9.0f * scale_, 0)))
        return false;
    ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthStretch, 1.25f);
    ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    const auto mm = [&](T label, const char* key, double& v, double min, double max, const char* fmt, float speed,
                        const char* tip = nullptr) {
        if (shown(key))
            changed |= row(label, v, min, max, fmt, speed, tip);
    };
    const auto whole = [&](T label, const char* key, int& v, int min, int max, const char* fmt = "%d",
                           const char* tip = nullptr) {
        if (shown(key))
            changed |= row(label, v, min, max, fmt, tip);
    };
    const auto flag = [&](T label, const char* key, bool& v, const char* tip = nullptr) {
        if (shown(key))
            changed |= row(label, v, tip);
    };
    // A speed in mm/s or a share of another (as the file gives it); 0: the other's.
    const auto share_row = [&](T label, const char* key, Speed& v) {
        if (shown(key))
            changed |= row(label, v.value, 0.0, 1000.0, v.value <= 0.0 ? "—" : v.percent ? "%.0f %%" : "%.0f mm/s", 0.5f);
    };
    const auto count = [&](T label, const char* key, std::size_t n) {
        if (!shown(key))
            return;
        label_cell(label, tr(T::ListEditTip));
        loc::TextDisabled("%s", format(tr(T::ListCount), int(n)).c_str());
    };

    // Print: Quality.
    if (id == "layer_height") {
        mm(T::LayerHeight, "layers.height", s.layer_height, 0.04, 1.0, "%.2f mm", 0.002f);
        mm(T::FirstLayerHeight, "layers.first_height", s.first_layer_height, 0.04, 1.0, "%.2f mm", 0.002f);
    } else if (id == "variable_height") {
        if (shown("layers.heights"))
            changed |= height_steps_rows(s.layer_heights, s.layer_height);
        mm(T::CuspHeight, "layers.cusp_height", s.cusp_height, 0.0, 1.0, "%.3f mm", 0.001f, tr(T::CuspHeightTip));
    } else if (id == "fine_detail") {
        mm(T::DetailHeight, "layers.detail_height", s.detail_height, 0.0, 1.0, "%.2f mm", 0.002f, tr(T::DetailHeightTip));
        mm(T::DetailTolerance, "layers.detail_tolerance", s.detail_tolerance, 0.001, 1.0, "%.3f mm", 0.001f,
           tr(T::DetailToleranceTip));
        mm(T::DetailMargin, "layers.detail_margin", s.detail_margin, 0.0, 10.0, "%.2f mm", 0.01f, tr(T::DetailMarginTip));
    } else if (id == "line_width") {
        mm(T::LineWidth, "walls.width", s.wall_width, 0.1, 2.0, "%.2f mm", 0.002f);
        mm(T::OuterWidth, "walls.outer_width", s.outer_wall_width, 0.0, 2.0, s.outer_wall_width > 0.0 ? "%.2f mm" : "—",
           0.002f);
        if (shown("layers.first_line_width"))
            changed |= row(T::FirstLineWidth, s.first_line_width.value, 0.0, s.first_line_width.percent ? 1000.0 : 40.0,
                           s.first_line_width.percent ? "%.0f %%" : "%.2f mm", s.first_line_width.percent ? 0.5f : 0.01f);
        mm(T::TopWidth, "surfaces.top_width", s.top_width, 0.0, 2.0, s.top_width > 0.0 ? "%.2f mm" : "—", 0.002f);
        mm(T::SolidWidth, "surfaces.solid_width", s.solid_width, 0.0, 2.0, s.solid_width > 0.0 ? "%.2f mm" : "—", 0.002f);
        mm(T::InfillWidth, "infill.width", s.infill_width, 0.0, 2.0, s.infill_width > 0.0 ? "%.2f mm" : "—", 0.002f);
        if (shown("walls.seam_gap"))
            changed |= row(T::SeamGap, s.wall_seam_gap.value, 0.0, s.wall_seam_gap.percent ? 1000.0 : 40.0,
                           s.wall_seam_gap.percent ? "%.0f %%" : "%.2f mm", s.wall_seam_gap.percent ? 0.5f : 0.01f);
    } else if (id == "seam") {
        if (shown("walls.seam"))
            changed |= row(T::Seam, s.seam,
                           {{SeamMode::Aligned, tr(T::SeamAligned)},
                            {SeamMode::Nearest, tr(T::SeamNearest)},
                            {SeamMode::Random, tr(T::SeamRandom)},
                            {SeamMode::Rear, tr(T::SeamRear)}});
        if (shown("walls.scarf"))
            changed |= row(T::ScarfMode, s.scarf,
                           {{ScarfMode::Nowhere, tr(T::ScarfNowhere)},
                            {ScarfMode::Contours, tr(T::ScarfContours)},
                            {ScarfMode::Everywhere, tr(T::ScarfEverywhere)}});
        mm(T::ScarfLength, "walls.scarf_length", s.scarf_length, 0.5, 1000.0, "%.1f mm", 0.1f, tr(T::ScarfTip));
        flag(T::ScarfEntire, "walls.scarf_entire_loop", s.scarf_entire_loop, tr(T::ScarfTip));
        mm(T::ScarfStart, "walls.scarf_start_height", s.scarf_start_height, 0.0, 100.0, "%.0f %%", 0.5f);
        mm(T::ScarfSegment, "walls.scarf_max_segment", s.scarf_max_segment, 0.05, 1000.0, "%.2f mm", 0.01f);
        flag(T::ScarfSmooth, "walls.scarf_only_smooth", s.scarf_only_smooth);
        flag(T::ScarfInner, "walls.scarf_inner", s.scarf_inner);
    } else if (id == "precision") {
        mm(T::XyOffset, "slicing.xy_offset", s.xy_offset, -2.0, 2.0, "%+.3f mm", 0.001f);
        mm(T::FirstLayerShrink, "slicing.first_layer_shrink", s.first_layer_shrink, 0.0, 2.0, "%.3f mm", 0.001f);
        mm(T::ScaleXRow, "compensation.scale_x", s.scale_x, 50.0, 150.0, "%.3f %%", 0.01f);
        mm(T::ScaleYRow, "compensation.scale_y", s.scale_y, 50.0, 150.0, "%.3f %%", 0.01f);
        mm(T::ScaleZRow, "compensation.scale_z", s.scale_z, 50.0, 150.0, "%.3f %%", 0.01f);
        mm(T::HoleCorrection, "compensation.hole_offset", s.hole_offset, -2.0, 2.0, "%+.3f mm", 0.001f);
    } else if (id == "ironing") {
        flag(T::IroningEnabled, "ironing.enabled", s.ironing);
        if (shown("ironing.surfaces"))
            changed |= row(T::IroningSurfacesRow, s.ironing_surfaces,
                           {{IroningSurfaces::Top, tr(T::IroningTop)},
                            {IroningSurfaces::Topmost, tr(T::IroningTopmost)},
                            {IroningSurfaces::Solid, tr(T::IroningSolid)}});
        mm(T::IroningFlow, "ironing.flow", s.ironing_flow, 1.0, 100.0, "%.0f %%", 0.5f);
        mm(T::IroningSpacing, "ironing.spacing", s.ironing_spacing, 0.01, 2.0, "%.2f mm", 0.005f);
        mm(T::IroningSpeed, "ironing.speed", s.ironing_speed, 1.0, 1000.0, "%.0f mm/s", 0.5f);
    } else if (id == "slopes") {
        flag(T::Zaa, "zaa.enabled", s.zaa, tr(T::ZaaTip));
        mm(T::ZaaMinThickness, "zaa.min_thickness", s.zaa_min_thickness, 1.0, 100.0, "%.0f %%", 0.5f,
           tr(T::ZaaMinThicknessTip));
        flag(T::ZaaRaise, "zaa.raise", s.zaa_raise, tr(T::ZaaRaiseTip));
    }
    // Print: Strength.
    else if (id == "walls") {
        whole(T::Walls, "walls.count", s.wall_count, 0, 50);
        flag(T::DetectOverhangs, "walls.detect_overhangs", s.detect_overhangs);
        if (shown("walls.generator"))
            changed |= row(T::WallGeneratorRow, s.wall_generator,
                           {{WallGenerator::Variable, tr(T::WallVariable)}, {WallGenerator::Fixed, tr(T::WallFixed)}});
        flag(T::WallGapFill, "walls.gap_fill", s.wall_gap_fill);
        flag(T::WallsClockwise, "walls.clockwise", s.walls_clockwise);
        flag(T::WallsOuterFirst, "walls.outer_first", s.walls_outer_first);
    } else if (id == "walls_thin") {
        if (shown("walls.min_line_width"))
            changed |= row(T::MinLineWidth, s.wall_min_line_width.value, 0.0, s.wall_min_line_width.percent ? 1000.0 : 40.0,
                           s.wall_min_line_width.percent ? "%.0f %%" : "%.2f mm", s.wall_min_line_width.percent ? 0.5f : 0.01f);
        if (shown("walls.min_detail"))
            changed |= row(T::MinDetail, s.wall_min_detail.value, 0.0, s.wall_min_detail.percent ? 1000.0 : 40.0,
                           s.wall_min_detail.percent ? "%.0f %%" : "%.2f mm", s.wall_min_detail.percent ? 0.5f : 0.01f);
        whole(T::SpreadCount, "walls.spread_count", s.wall_spread_count, 1, 20);
        if (shown("walls.transition_length"))
            changed |= row(T::TransitionLength, s.wall_transition_length.value, 0.0,
                           s.wall_transition_length.percent ? 1000.0 : 40.0,
                           s.wall_transition_length.percent ? "%.0f %%" : "%.2f mm",
                           s.wall_transition_length.percent ? 0.5f : 0.01f);
        mm(T::TransitionAngle, "walls.transition_angle", s.wall_transition_angle, 0.0, 179.0, "%.0f°", 0.5f);
    } else if (id == "walls_more") {
        flag(T::StaggeredSeams, "walls.staggered_inner_seams", s.staggered_inner_seams);
        flag(T::WallsStaggered, "walls.staggered", s.walls_staggered);
        if (shown("walls.top_one_wall"))
            changed |= row(T::TopOneWallRow, s.top_one_wall,
                           {{TopOneWall::None, tr(T::TopOneWallNone)},
                            {TopOneWall::Top, tr(T::TopOneWallTop)},
                            {TopOneWall::Topmost, tr(T::TopOneWallTopmost)}});
        flag(T::FirstLayerOneWall, "walls.first_layer_one_wall", s.first_layer_one_wall);
        flag(T::ExtraIfNeeded, "walls.extra_if_needed", s.extra_if_needed);
        flag(T::ExtraOnOverhangs, "walls.extra_on_overhangs", s.extra_on_overhangs);
        flag(T::AvoidCurledOverhangs, "walls.avoid_curled_overhangs", s.avoid_curled_overhangs);
    } else if (id == "top_bottom") {
        whole(T::TopLayers, "surfaces.top_layers", s.top_layers, 0, 100);
        whole(T::BottomLayers, "surfaces.bottom_layers", s.bottom_layers, 0, 100);
        mm(T::TopMinThickness, "surfaces.top_min_thickness", s.top_min_thickness, 0.0, 10.0, "%.2f mm", 0.01f);
        mm(T::BottomMinThickness, "surfaces.bottom_min_thickness", s.bottom_min_thickness, 0.0, 10.0, "%.2f mm", 0.01f);
        if (shown("surfaces.top_pattern"))
            changed |= row(T::TopPattern, s.monotonic_top, {{true, tr(T::Monotonic)}, {false, tr(T::Rectilinear)}});
        if (shown("surfaces.bottom_pattern"))
            changed |= row(T::BottomPattern, s.monotonic_bottom, {{true, tr(T::Monotonic)}, {false, tr(T::Rectilinear)}});
        mm(T::BridgeAngle, "surfaces.bridge_angle", s.bridge_angle, -360.0, 360.0, s.bridge_angle != 0.0 ? "%.0f°" : "—",
           0.5f);
        flag(T::InterfaceShells, "surfaces.interface_shells", s.interface_shells);
    } else if (id == "infill") {
        mm(T::InfillDensity, "infill.density", s.infill_density, 0.0, 100.0, "%.0f %%", 0.2f);
        if (shown("infill.pattern"))
            changed |= row(T::InfillPattern, s.infill_pattern,
                           {{InfillPattern::Rectilinear, tr(T::Rectilinear)},
                            {InfillPattern::AlignedRectilinear, tr(T::AlignedRectilinear)},
                            {InfillPattern::Zigzag, tr(T::Zigzag)},
                            {InfillPattern::Line, tr(T::LineInfill)},
                            {InfillPattern::Grid, tr(T::Grid)},
                            {InfillPattern::Gyroid, tr(T::Gyroid)},
                            {InfillPattern::Triangles, tr(T::TrianglesInfill)},
                            {InfillPattern::Stars, tr(T::Stars)},
                            {InfillPattern::Cubic, tr(T::Cubic)},
                            {InfillPattern::AdaptiveCubic, tr(T::AdaptiveCubic)},
                            {InfillPattern::SupportCubic, tr(T::SupportCubic)},
                            {InfillPattern::Lightning, tr(T::Lightning)},
                            {InfillPattern::Honeycomb, tr(T::Honeycomb)},
                            {InfillPattern::Concentric, tr(T::ConcentricInfill)},
                            {InfillPattern::Hilbert, tr(T::HilbertInfill)},
                            {InfillPattern::Archimedean, tr(T::ArchimedeanInfill)},
                            {InfillPattern::Octagram, tr(T::OctagramInfill)}});
        mm(T::InfillAngle, "infill.angle", s.infill_angle, -180.0, 180.0, "%.0f°", 0.5f);
    } else if (id == "infill_more") {
        mm(T::WallOverlap, "infill.wall_overlap", s.infill_wall_overlap, 0.0, 100.0, "%.0f %%", 0.5f);
        flag(T::InfillBeforeWalls, "infill.before_walls", s.infill_before_walls);
        mm(T::MaxLink, "infill.max_link", s.infill_max_link, 0.0, 1000.0, s.infill_max_link > 0.0 ? "%.0f mm" : "—", 0.5f);
        whole(T::SolidEvery, "infill.solid_every", s.infill_solid_every, 0, 100, "%d", tr(T::SolidEveryTip));
        mm(T::SolidBelowArea, "infill.solid_below_area", s.infill_solid_below_area, 0.0, 1000.0, "%.0f mm²", 1.0f,
           tr(T::SolidBelowAreaTip));
        flag(T::DenseUnderTop, "infill.dense_under_top", s.infill_dense_under_top, tr(T::DenseUnderTopTip));
        mm(T::DenseDensity, "infill.dense_density", s.infill_dense_density, 1.0, 100.0, "%.0f %%", 0.5f,
           tr(T::DenseUnderTopTip));
        mm(T::StressDensity, "infill.stress_density", s.infill_stress_density, 0.0, 100.0, "%.0f %%", 0.5f,
           tr(T::StressDensityTip));
        if (s.infill_stress_density > 0.0 && shown("infill.stress_density")) {
            label_cell(T::LoadCase, tr(T::StressDensityTip));
            if (ImGui::Button(load_case_ ? load_case_name_.c_str() : tr(T::ChooseCase), ImVec2(-FLT_MIN, 0))) {
                purpose_ = Purpose::LoadCase;
                browser_.open(FileBrowser::Mode::Open, tr(T::LoadCase), {".json"}, last_folder_);
            }
        }
    } else if (id == "combine") {
        flag(T::CombineAuto, "infill.combine_auto", s.infill_combine_auto, tr(T::CombineInfillTip));
        whole(T::CombineInfill, "infill.combine_layers", s.infill_combine_layers, 1, 10, "%d", tr(T::CombineInfillTip));
        if (shown("infill.combine_max_height"))
            changed |= row(T::CombineMaxHeight, s.infill_combine_max_height.value, 1.0,
                           s.infill_combine_max_height.percent ? 100.0 : 10.0,
                           s.infill_combine_max_height.percent ? "%.0f %%" : "%.2f mm", 0.5f);
    } else if (id == "wavy") {
        flag(T::NonplanarInfill, "infill.nonplanar", s.infill_nonplanar, tr(T::NonplanarInfillTip));
        mm(T::NonplanarAmplitude, "infill.nonplanar_amplitude", s.infill_nonplanar_amplitude, 0.0, 500.0, "%.0f %%", 1.0f);
        mm(T::NonplanarWavelength, "infill.nonplanar_wavelength", s.infill_nonplanar_wavelength, 0.5, 20.0, "%.1f mm",
           0.05f);
        // The interlocking waves (interlock_infill.md): one kind at a time.
        if (shown("infill.waves") && row(T::InterlockWaves, s.infill_waves, tr(T::InterlockWavesTip))) {
            if (s.infill_waves)
                s.infill_nonplanar = false;
            changed = true;
        }
        mm(T::WaveAmplitude, "infill.wave_amplitude", s.infill_wave_amplitude, 0.0, 200.0, "%.0f %%", 1.0f);
        mm(T::WaveLength, "infill.wave_length", s.infill_wave_length, 1.0, 100.0, "%.1f mm", 0.1f);
    }
    // Print: Speed (every speed is the printer's, owner's decision, 2026-10-26).
    else if (id == "first_layer") {
        mm(T::SpeedFirstLayer, "speed.first_layer", s.first_layer_speed, 1.0, 1000.0, "%.0f mm/s", 0.5f);
        share_row(T::SpeedFirstLayerInfill, "speed.first_layer_infill", s.first_layer_infill_speed);
        share_row(T::SpeedOverRaft, "speed.first_layer_over_raft", s.first_layer_over_raft);
    } else if (id == "walls_surfaces") {
        mm(T::SpeedWalls, "speed.walls", s.wall_speed, 1.0, 1000.0, "%.0f mm/s", 0.5f);
        if (shown("speed.outer_wall"))
            changed |= row(T::SpeedOuterWall, s.outer_wall_speed.value, 1.0, s.outer_wall_speed.percent ? 100.0 : 1000.0,
                           s.outer_wall_speed.percent ? "%.0f %%" : "%.0f mm/s", 0.5f);
        mm(T::SpeedSolid, "speed.solid_infill", s.solid_infill_speed, 1.0, 1000.0, "%.0f mm/s", 0.5f);
        mm(T::SpeedTop, "speed.top_solid_infill", s.top_solid_infill_speed, 1.0, 1000.0, "%.0f mm/s", 0.5f);
        // 0: the solid infill's.
        mm(T::SpeedBridges, "speed.bridges", s.bridge_speed, 0.0, 1000.0, s.bridge_speed > 0.0 ? "%.0f mm/s" : "—", 0.5f);
        share_row(T::SpeedOverBridges, "speed.over_bridges", s.over_bridges_speed);
        share_row(T::SpeedSmallLoops, "speed.small_loops", s.small_loop_speed);
        mm(T::SpeedGapFill, "speed.gap_fill", s.gap_fill_speed, 0.0, 1000.0, s.gap_fill_speed > 0.0 ? "%.0f mm/s" : "—",
           0.5f);
    } else if (id == "infill_support") {
        mm(T::SpeedInfill, "speed.infill", s.infill_speed, 1.0, 1000.0, "%.0f mm/s", 0.5f);
        mm(T::SpeedSupport, "speed.support", s.support_speed, 1.0, 1000.0, "%.0f mm/s", 0.5f);
        share_row(T::SpeedSupportInterface, "speed.support_interface", s.support_interface_speed);
    } else if (id == "travel") {
        mm(T::SpeedTravel, "speed.travel", s.travel_speed, 1.0, 1000.0, "%.0f mm/s", 0.5f);
        mm(T::SpeedTravelZ, "speed.travel_z", s.travel_z_speed, 0.0, 1000.0, s.travel_z_speed > 0.0 ? "%.0f mm/s" : "—",
           0.5f);
        flag(T::AvoidCrossingWalls, "travel.avoid_crossing_walls", s.avoid_crossing_walls);
        if (shown("travel.max_detour"))
            changed |= row(T::MaxDetour, s.max_detour.value, 0.0, 10000.0,
                           s.max_detour.value <= 0.0 ? "—" : s.max_detour.percent ? "%.0f %%" : "%.1f mm", 0.1f);
    } else if (id == "limits") {
        mm(T::SpeedMax, "speed.max", s.max_speed, 0.0, 1000.0, s.max_speed > 0.0 ? "%.0f mm/s" : "—", 0.5f);
        mm(T::SpeedMaxFlow, "speed.max_flow", s.speed_max_flow, 0.0, 200.0,
           s.speed_max_flow > 0.0 ? "%.1f mm³/s" : "—", 0.1f);
        mm(T::FlowSlopeUp, "speed.flow_slope_up", s.flow_slope_up, 0.0, 1000.0,
           s.flow_slope_up > 0.0 ? "%.1f mm³/s²" : "—", 0.05f);
        mm(T::FlowSlopeDown, "speed.flow_slope_down", s.flow_slope_down, 0.0, 1000.0,
           s.flow_slope_down > 0.0 ? "%.1f mm³/s²" : "—", 0.05f);
    } else if (id == "overhang_speeds") {
        flag(T::DynamicOverhangs, "speed.dynamic_overhangs", s.dynamic_overhangs);
        const T labels[4] = {T::Overhang0, T::Overhang1, T::Overhang2, T::Overhang3};
        const char* keys[4] = {"speed.overhang_0", "speed.overhang_1", "speed.overhang_2", "speed.overhang_3"};
        for (std::size_t k = 0; k < 4; ++k)
            share_row(labels[k], keys[k], s.overhang_speeds[k]);
    } else if (id == "accel_kinds" || id == "accel_travel") {
        // 0: the one it falls back to, or none (speeds_accels.md, rule 10).
        const auto accel = [&](T label, const char* key, double& v) {
            mm(label, key, v, 0.0, 100000.0, v > 0.0 ? "%.0f mm/s²" : "—", 10.0f);
        };
        if (id == "accel_kinds") {
            accel(T::AccelDefault, "acceleration.default", s.accel_default);
            accel(T::AccelWalls, "acceleration.walls", s.accel_walls);
            accel(T::AccelOuterWall, "acceleration.outer_wall", s.accel_outer_wall);
            accel(T::AccelInfill, "acceleration.infill", s.accel_infill);
            accel(T::AccelSolid, "acceleration.solid_infill", s.accel_solid_infill);
            accel(T::AccelTop, "acceleration.top_solid_infill", s.accel_top_solid_infill);
            accel(T::AccelBridges, "acceleration.bridges", s.accel_bridges);
            accel(T::AccelWipeTower, "acceleration.wipe_tower", s.accel_wipe_tower);
        } else {
            accel(T::AccelFirstLayer, "acceleration.first_layer", s.accel_first_layer);
            accel(T::AccelOverRaft, "acceleration.first_layer_over_raft", s.accel_first_layer_over_raft);
            accel(T::AccelTravel, "acceleration.travel", s.accel_travel);
            accel(T::AccelShortTravel, "acceleration.short_travel", s.accel_short_travel);
        }
    }
    // Print: Supports and Adhesion.
    else if (id == "support") {
        flag(T::Supports, "support.enabled", s.support);
        flag(T::SupportAutomatic, "support.automatic", s.support_automatic, tr(T::SupportAutomaticTip));
        flag(T::SupportBuildplateOnly, "support.buildplate_only", s.support_buildplate_only);
        whole(T::SupportEnforceLayers, "support.enforce_layers", s.support_enforce_layers, 0, 1000);
        mm(T::SupportFreeBridge, "support.free_bridge_length", s.support_free_bridge_length, 0.0, 1000.0,
           s.support_free_bridge_length > 0.0 ? "%.0f mm" : "—", 0.5f);
        if (shown("support.style"))
            changed |= row(T::SupportStyle, s.support_style,
                           {{SupportStyle::Grid, tr(T::SupportGrid)},
                            {SupportStyle::Snug, tr(T::SupportSnug)},
                            {SupportStyle::Organic, tr(T::SupportOrganic)}},
                           tr(T::SupportStyleTip));
        if (s.support_style == SupportStyle::Snug)
            mm(T::SupportClosingRadius, "support.closing_radius", s.support_closing_radius, 0.0, 100.0, "%.2f mm", 0.01f);
        mm(T::SupportAngle, "support.threshold_angle", s.support_angle, 0.0, 89.0, "%.0f°", 0.5f);
    } else if (id == "tree") {
        mm(T::TreeBranchDiameter, "support.tree_branch_diameter", s.tree_branch_diameter, 0.1, 100.0, "%.1f mm", 0.05f);
        mm(T::TreeTipDiameter, "support.tree_tip_diameter", s.tree_tip_diameter, 0.1, 100.0, "%.2f mm", 0.01f);
        mm(T::TreeTopRate, "support.tree_top_rate", s.tree_top_rate, 1.0, 100.0, "%.0f %%", 0.5f);
        mm(T::TreeBranchDistance, "support.tree_branch_distance", s.tree_branch_distance, 0.1, 100.0, "%.2f", 0.01f);
        mm(T::TreeAnglePreferred, "support.tree_angle_preferred", s.tree_angle_preferred, 0.0, 85.0, "%.0f°", 0.5f);
        mm(T::TreeAngle, "support.tree_angle", s.tree_angle, 0.0, 85.0, "%.0f°", 0.5f);
        mm(T::TreeBranchAngle, "support.tree_branch_diameter_angle", s.tree_branch_diameter_angle, 0.0, 85.0, "%.1f°",
           0.1f);
        mm(T::TreeDoubleWall, "support.tree_double_wall_diameter", s.tree_double_wall_diameter, 0.0, 100.0, "%.1f mm",
           0.05f, tr(T::TreeDoubleWallTip));
    } else if (id == "gap") {
        mm(T::SupportZGap, "support.z_gap", s.support_z_gap, 0.0, 5.0, "%.2f mm", 0.01f);
        mm(T::SupportBottomZGap, "support.bottom_z_gap", s.support_bottom_z_gap, 0.0, 5.0,
           s.support_bottom_z_gap > 0.0 ? "%.2f mm" : "—", 0.01f);
        if (shown("support.xy_gap"))
            changed |= row(T::SupportXyGap, s.support_xy_gap.value, 0.0, s.support_xy_gap.percent ? 1000.0 : 10.0,
                           s.support_xy_gap.percent ? "%.0f %%" : "%.2f mm", s.support_xy_gap.percent ? 0.5f : 0.01f);
    } else if (id == "interface") {
        whole(T::SupportInterface, "support.interface_layers", s.support_interface_layers, 0, 20);
        whole(T::SupportBottomInterface, "support.bottom_interface_layers", s.support_bottom_interface_layers, -1, 20,
              s.support_bottom_interface_layers < 0 ? "—" : "%d");
        flag(T::SyncSupportLayers, "support.synchronize_layers", s.support_synchronize_layers);
        if (shown("support.interface_pattern"))
            changed |= row(T::SupportInterfacePatternRow, s.support_interface_pattern,
                           {{SupportInterfacePattern::Lines, tr(T::PatternLines)},
                            {SupportInterfacePattern::Concentric, tr(T::ConcentricInfill)},
                            {SupportInterfacePattern::Automatic, tr(T::PatternAutomatic)}});
        mm(T::SupportInterfaceSpacing, "support.interface_spacing", s.support_interface_spacing, 0.0, 100.0,
           s.support_interface_spacing > 0.0 ? "%.2f mm" : "—", 0.01f);
        flag(T::SupportContactLoops, "support.interface_contact_loops", s.support_contact_loops);
    } else if (id == "pattern") {
        mm(T::SupportSpacing, "support.spacing", s.support_spacing, 0.5, 20.0, "%.2f mm", 0.01f);
        if (shown("support.pattern"))
            changed |= row(T::SupportPatternRow, s.support_pattern,
                           {{SupportPattern::Lines, tr(T::PatternLines)},
                            {SupportPattern::Grid, tr(T::PatternGrid)},
                            {SupportPattern::Honeycomb, tr(T::Honeycomb)}});
        flag(T::SupportSheath, "support.sheath", s.support_sheath);
        mm(T::SupportLineAngle, "support.line_angle", s.support_line_angle, -360.0, 360.0, "%.0f°", 0.5f);
        if (shown("support.width"))
            changed |= row(T::SupportWidth, s.support_width.value, 0.0, s.support_width.percent ? 1000.0 : 40.0,
                           s.support_width.percent ? "%.0f %%" : "%.2f mm", s.support_width.percent ? 0.5f : 0.01f);
    } else if (id == "arcs") {
        flag(T::ArcsEnabled, "arcs.enabled", s.arcs);
        mm(T::ArcsOverlap, "arcs.overlap", s.arcs_overlap, 0.0, 90.0, "%.0f %%", 0.5f);
        mm(T::ArcsSpeed, "arcs.speed", s.arcs_speed, 0.0, 1000.0, "%.1f mm/s", 0.1f);
        mm(T::ArcsMaxDistance, "arcs.max_distance", s.arcs_max_distance, 0.0, 1000.0, "%.1f mm", 0.1f);
    } else if (id == "skirt") {
        whole(T::SkirtLoops, "adhesion.skirt_loops", s.skirt_loops, 0, 20);
        mm(T::SkirtDistance, "adhesion.skirt_distance", s.skirt_distance, 0.0, 50.0, "%.1f mm", 0.05f);
        whole(T::SkirtLayers, "adhesion.skirt_layers", s.skirt_layers, 1, 100);
        mm(T::SkirtMinFilament, "adhesion.skirt_min_filament", s.skirt_min_filament, 0.0, 100000.0,
           s.skirt_min_filament > 0.0 ? "%.0f mm" : "—", 1.0f);
        if (shown("adhesion.draft_shield"))
            changed |= row(T::DraftShieldRow, s.draft_shield,
                           {{DraftShield::Disabled, tr(T::ShieldDisabled)},
                            {DraftShield::Limited, tr(T::ShieldLimited)},
                            {DraftShield::Enabled, tr(T::ShieldEnabled)}});
    } else if (id == "brim") {
        mm(T::BrimWidth, "adhesion.brim_width", s.brim_width, 0.0, 50.0, "%.1f mm", 0.05f);
        if (shown("adhesion.brim_type"))
            changed |= row(T::BrimTypeRow, s.brim_type,
                           {{BrimType::Outer, tr(T::BrimOuter)},
                            {BrimType::Inner, tr(T::BrimInner)},
                            {BrimType::OuterAndInner, tr(T::BrimBoth)},
                            {BrimType::None, tr(T::BrimNone)}});
        mm(T::BrimGap, "adhesion.brim_gap", s.brim_gap, 0.0, 10.0, "%.2f mm", 0.01f);
    } else if (id == "raft") {
        whole(T::RaftLayers, "adhesion.raft_layers", s.raft_layers, 0, 100);
        mm(T::RaftGap, "adhesion.raft_gap", s.raft_gap, 0.0, 100.0, "%.2f mm", 0.01f);
        mm(T::RaftExpansion, "adhesion.raft_expansion", s.raft_expansion, 0.0, 100.0, "%.1f mm", 0.05f);
        mm(T::RaftFirstDensity, "adhesion.raft_first_layer_density", s.raft_first_layer_density, 5.0, 100.0, "%.0f %%",
           0.5f);
        mm(T::RaftFirstExpansion, "adhesion.raft_first_layer_expansion", s.raft_first_layer_expansion, 0.0, 100.0,
           "%.1f mm", 0.05f);
    }
    // Print: Others.
    else if (id == "special_modes") {
        flag(T::VaseMode, "vase.enabled", s.vase, tr(T::VaseModeTip));
        flag(T::SequentialRow, "sequential.enabled", s.sequential, tr(T::SequentialTip));
    } else if (id == "multi_material") {
        // Which extruder prints what (multi_extruder.md, rules 2, 4, 7 and 16); 0: the object's.
        whole(T::WallsExtruder, "extruders.walls", s.extruder_walls, 0, kMaxExtruders);
        whole(T::InfillExtruder, "extruders.infill", s.extruder_infill, 0, kMaxExtruders);
        whole(T::SolidInfillExtruder, "extruders.solid_infill", s.extruder_solid_infill, 0, kMaxExtruders);
        whole(T::SupportExtruder, "extruders.support", s.extruder_support, 0, kMaxExtruders);
        whole(T::SupportInterfaceExtruder, "extruders.support_interface", s.extruder_support_interface, 0, kMaxExtruders);
        whole(T::BedTemperatureFrom, "extruders.bed_temperature_from", s.bed_temperature_from, 0, kMaxExtruders);
        flag(T::OozePrevention, "extruders.ooze_prevention", s.ooze_prevention);
        mm(T::StandbyDelta, "extruders.standby_delta", s.standby_delta, -300.0, 300.0, "%.0f °C", 0.5f);
    } else if (id == "multi_nozzle") {
        // Different nozzles and mixed colours (multimaterial.md, rules 3, 9
        // and 11): with one nozzle for every filament, turning the mixing on
        // first says what it costs.
        flag(T::FineWallsCoarseInfill, "multi.fine_walls_coarse_infill", s.fine_walls_coarse_infill);
        if (shown("multi.color_mixing")) {
            bool mixing = color_mixing_offered(s);
            if (row(T::ColorMixingRow, mixing)) {
                // A change template that cannot render would fail the
                // export at the first blend: said now (color_mixing.md, rule 13).
                if (mixing)
                    if (const std::string error = toolchange_template_error(s); !error.empty())
                        fail(T::MixingTemplateError, error);
                if (mixing && s.single_extruder_mmu)
                    ImGui::OpenPopup("##mixing_cost");
                else {
                    s.color_mixing = mixing;
                    changed = true;
                }
            }
            if (ImGui::BeginPopup("##mixing_cost")) {
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 24.0f);
                ImGui::TextUnformatted(tr(T::ColorMixingCost));
                ImGui::PopTextWrapPos();
                if (ImGui::Button(tr(T::Cancel)))
                    ImGui::CloseCurrentPopup();
                ImGui::SameLine();
                if (ImGui::Button(tr(T::TurnOn))) {
                    s.color_mixing = true;
                    changed = true;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }
    } else if (id == "curved") {
        // Curved layers (nonplanar.md).
        if (shown("curved.mode"))
            changed |= row(T::CurvedModeRow, s.curved_mode,
                           {{CurvedMode::Disabled, tr(T::SumOff)},
                            {CurvedMode::Wave, tr(T::CurvedWave)},
                            {CurvedMode::Conical, tr(T::CurvedCone)},
                            {CurvedMode::Overhangs, tr(T::CurvedOverhangs)},
                            {CurvedMode::Stress, tr(T::CurvedStress)},
                            {CurvedMode::Surface, tr(T::CurvedSurface)},
                            {CurvedMode::Automatic, tr(T::CurvedAutomatic)}});
        if (shown("curved.pattern"))
            changed |= row(T::CurvedPatternRow, s.curved_pattern,
                           {{CurvedPattern::Egg, tr(T::CurvedEgg)},
                            {CurvedPattern::Ridges, tr(T::CurvedRidges)},
                            {CurvedPattern::Twisted, tr(T::CurvedTwisted)}});
        flag(T::CurvedFitHead, "curved.fit_head", s.curved_fit_head);
        mm(T::CurvedAmplitude, "curved.amplitude", s.curved_amplitude, 0.0, 50.0, "%.2f mm", 0.01f);
        mm(T::CurvedWavelength, "curved.wavelength", s.curved_wavelength, 1.0, 1000.0, "%.1f mm", 0.1f);
        mm(T::CurvedAngle, "curved.angle", s.curved_angle, -360.0, 360.0, "%.0f°", 0.5f);
        mm(T::CurvedTwist, "curved.twist", s.curved_twist, -360.0, 360.0, "%.1f°/mm", 0.1f);
        mm(T::CurvedConeAngle, "curved.cone_angle", s.curved_cone_angle, -60.0, 60.0, "%.1f°", 0.1f);
    } else if (id == "curved_more") {
        mm(T::CurvedFlatBelow, "curved.flat_below", s.curved_flat_below, 0.0, 1000.0, "%.2f mm", 0.01f);
        mm(T::CurvedRampHeight, "curved.ramp_height", s.curved_ramp_height, 0.1, 1000.0, "%.1f mm", 0.1f);
        flag(T::CurvedFlatTop, "curved.flat_top", s.curved_flat_top);
        mm(T::CurvedSegment, "curved.segment_length", s.curved_segment_length, 0.05, 50.0, "%.2f mm", 0.01f);
        mm(T::CurvedMaxSlope, "curved.max_slope", s.curved_max_slope, 1.0, 80.0, "%.0f°", 0.5f);
        if (shown("curved.flow_policy"))
            changed |= row(T::CurvedFlowPolicy, s.curved_flow,
                           {{CurvedFlow::Preserve, tr(T::CurvedFlowKeep)},
                            {CurvedFlow::Uniform, tr(T::CurvedFlowUniform)},
                            {CurvedFlow::Off, tr(T::CurvedFlowOff)}});
        mm(T::CurvedUniformFlow, "curved.uniform_flow", s.curved_uniform_flow, 0.0, 1000.0,
           s.curved_uniform_flow > 0.0 ? "%.1f mm³/s" : "—", 0.1f);
        mm(T::CurvedOverhangThreshold, "curved.overhang_threshold", s.curved_overhang_threshold, 10.0, 80.0, "%.0f°", 0.5f);
    } else if (id == "interlocking") {
        // Beams between materials (multi_extruder.md, rules 17 and 18).
        flag(T::InterlockBeams, "interlocking.beams", s.interlock_beams);
        mm(T::InterlockWidth, "interlocking.beam_width", s.interlock_width, 0.1, 100.0, "%.2f mm", 0.01f);
        whole(T::InterlockLayers, "interlocking.beam_layers", s.interlock_layers, 1, 100);
        mm(T::InterlockOrientation, "interlocking.orientation", s.interlock_orientation, -360.0, 360.0, "%.1f°", 0.5f);
        whole(T::InterlockDepth, "interlocking.depth", s.interlock_depth, 0, 100);
        whole(T::InterlockAvoidance, "interlocking.boundary_avoidance", s.interlock_avoidance, 0, 100);
        mm(T::PaintMaxRegionWidth, "painting.max_region_width", s.paint_max_region_width, 0.0, 1000.0, "%.1f mm", 0.1f);
        mm(T::PaintInterlockingDepth, "painting.interlocking_depth", s.paint_interlocking_depth, 0.0, 1000.0, "%.1f mm",
           0.1f);
    } else if (id == "mmu") {
        // A single-extruder MMU's change (multi_extruder.md, rule 15).
        mm(T::CoolingTubeLength, "mmu.cooling_tube_length", s.mmu_cooling_tube_length, 0.0, 1000.0, "%.1f mm", 0.1f);
        mm(T::CoolingTubePosition, "mmu.cooling_tube_position", s.mmu_cooling_tube_position, 0.0, 1000.0, "%.1f mm", 0.1f);
        mm(T::ParkingPosition, "mmu.parking_position", s.mmu_parking_position, 0.0, 1000.0, "%.1f mm", 0.1f);
        mm(T::ExtraLoadingMove, "mmu.extra_loading_move", s.mmu_extra_loading_move, -1000.0, 1000.0, "%.1f mm", 0.1f);
        flag(T::HighCurrentOnSwap, "mmu.high_current_on_swap", s.mmu_high_current_on_swap);
    } else if (id == "mmu_load") {
        mm(T::LoadingSpeed, "extrusion.loading_speed", s.loading_speed, 0.1, 1000.0, "%.1f mm/s", 0.1f);
        mm(T::LoadingSpeedStart, "extrusion.loading_speed_start", s.loading_speed_start, 0.1, 1000.0, "%.1f mm/s", 0.1f);
        mm(T::UnloadingSpeed, "extrusion.unloading_speed", s.unloading_speed, 0.1, 1000.0, "%.1f mm/s", 0.1f);
        mm(T::UnloadingSpeedStart, "extrusion.unloading_speed_start", s.unloading_speed_start, 0.1, 1000.0, "%.1f mm/s",
           0.1f);
        whole(T::CoolingMoves, "extrusion.cooling_moves", s.cooling_moves, 0, 100);
        mm(T::CoolingInitialSpeed, "extrusion.cooling_initial_speed", s.cooling_initial_speed, 0.1, 1000.0, "%.1f mm/s",
           0.1f);
        mm(T::CoolingFinalSpeed, "extrusion.cooling_final_speed", s.cooling_final_speed, 0.1, 1000.0, "%.1f mm/s", 0.1f);
        count(T::RammingRow, "extrusion.ramming", s.ramming.size());
    } else if (id == "mmu_times") {
        mm(T::ToolchangeDelay, "extrusion.toolchange_delay", s.toolchange_delay, 0.0, 1000.0, "%.1f s", 0.1f);
        mm(T::LoadTime, "extrusion.load_time", s.load_time, 0.0, 1000.0, "%.1f s", 0.1f);
        mm(T::UnloadTime, "extrusion.unload_time", s.unload_time, 0.0, 1000.0, "%.1f s", 0.1f);
    } else if (id == "wipe_tower") {
        // The wipe tower (multi_extruder.md, rules 9 to 14).
        flag(T::WipeTowerRow, "wipe_tower.enabled", s.wipe_tower);
        mm(T::WipeTowerWidth, "wipe_tower.width", s.wipe_tower_width, 1.0, 1000.0, "%.1f mm", 0.1f);
        mm(T::WipeTowerBrim, "wipe_tower.brim_width", s.wipe_tower_brim, 0.0, 100.0, "%.1f mm", 0.05f);
        mm(T::WipeTowerX, "wipe_tower.x", s.wipe_tower_x, -10000.0, 10000.0, "%.1f mm", 0.1f);
        mm(T::WipeTowerY, "wipe_tower.y", s.wipe_tower_y, -10000.0, 10000.0, "%.1f mm", 0.1f);
        mm(T::WipeTowerRotation, "wipe_tower.rotation", s.wipe_tower_rotation, -360.0, 360.0, "%.0f°", 0.5f);
        mm(T::WipeTowerBridging, "wipe_tower.bridging", s.wipe_tower_bridging, 1.0, 100.0, "%.1f mm", 0.05f);
        whole(T::WipeTowerExtruder, "extruders.wipe_tower", s.extruder_wipe_tower, 0, kMaxExtruders);
    } else if (id == "wipe_tower_more") {
        count(T::PurgeVolumesRow, "wipe_tower.purge_volumes", s.purge_volumes.size());
        mm(T::WipeTowerSpacing, "wipe_tower.extra_spacing", s.wipe_tower_extra_spacing, 50.0, 500.0, "%.0f %%", 0.5f);
        mm(T::WipeTowerFlow, "wipe_tower.extra_flow", s.wipe_tower_extra_flow, 50.0, 500.0, "%.0f %%", 0.5f);
        mm(T::WipeTowerCone, "wipe_tower.cone_angle", s.wipe_tower_cone_angle, 0.0, 90.0, "%.0f°", 0.5f);
        flag(T::WipeTowerNoSparse, "wipe_tower.no_sparse_layers", s.wipe_tower_no_sparse_layers);
    } else if (id == "fuzzy") {
        if (shown("fuzzy.mode"))
            changed |= row(T::FuzzySkin, s.fuzzy_mode,
                           {{FuzzyMode::None, tr(T::FuzzyNone)},
                            {FuzzyMode::External, tr(T::FuzzyExternal)},
                            {FuzzyMode::All, tr(T::FuzzyAll)}});
        mm(T::FuzzyThickness, "fuzzy.thickness", s.fuzzy_thickness, 0.0, 2.0, "%.2f mm", 0.005f);
        mm(T::FuzzyDistance, "fuzzy.point_distance", s.fuzzy_point_distance, 0.1, 5.0, "%.2f mm", 0.01f);
    } else if (id == "gcode_output") {
        mm(T::GcodeResolution, "gcode.resolution", s.gcode_resolution, 0.0, 1.0, "%.4f mm", 0.0005f,
           tr(T::GcodeResolutionTip));
        count(T::HeightChanges, "changes", s.changes.size());
        flag(T::VolumetricE, "gcode.volumetric_e", s.volumetric_e);
        flag(T::VerboseComments, "gcode.verbose_comments", s.verbose_comments);
        flag(T::GcodeArcs, "gcode.arcs", s.gcode_arcs);
        mm(T::CornerSmoothing, "gcode.corner_smoothing", s.corner_smoothing, 0.0, 0.05,
           s.corner_smoothing > 0.0 ? "%.3f mm" : "—", 0.0005f);
        flag(T::GcodeBinary, "gcode.binary", s.gcode_binary);
    } else if (id == "slicing") {
        if (shown("slicing.fill_mode"))
            changed |= row(T::FillModeRow, s.fill_mode,
                           {{FillMode::Union, tr(T::FillUnion)},
                            {FillMode::EvenOdd, tr(T::FillEvenOdd)},
                            {FillMode::FillHoles, tr(T::FillHoles)}});
        mm(T::GapClosing, "slicing.gap_closing_radius", s.gap_closing_radius, 0.0, 2.0, "%.3f mm", 0.001f);
        mm(T::SimplifyTolerance, "slicing.simplify_tolerance", s.simplify_tolerance, 0.0, 1.0, "%.3f mm", 0.001f);
    }
    // Material.
    else if (id == "nozzle_temp") {
        if (shown("machine.variables.temperature"))
            changed |= variable_row(T::NozzleTemp, s, "temperature", 0, 450, "%d °C", tr(T::TempTip));
    } else if (id == "bed_temp") {
        if (shown("machine.variables.bed_temperature"))
            changed |= variable_row(T::BedTemp, s, "bed_temperature", 0, 150, "%d °C", tr(T::TempTip));
    } else if (id == "chamber") {
        mm(T::ChamberTemp, "extrusion.chamber_temperature", s.chamber_temperature, 0.0, 120.0, "%.0f °C", 0.5f);
        mm(T::ChamberMinTemp, "extrusion.chamber_min_temperature", s.chamber_min_temperature, 0.0, 120.0,
           s.chamber_min_temperature > 0.0 ? "%.0f °C" : "—", 0.5f);
    } else if (id == "price") {
        mm(T::FilamentDensity, "extrusion.density", s.filament_density, 0.0, 30.0,
           s.filament_density > 0.0 ? "%.2f g/cm³" : "—", 0.005f);
        mm(T::FilamentCost, "extrusion.cost", s.filament_cost, 0.0, 1e6, "%.2f", 0.1f);
        mm(T::SpoolWeight, "extrusion.spool_weight", s.spool_weight, 0.0, 1e6, "%.0f g", 1.0f);
    } else if (id == "shrinkage") {
        mm(T::ShrinkageXY, "extrusion.shrinkage_xy", s.shrinkage_xy, 0.0, 20.0, "%.2f %%", 0.01f);
        mm(T::ShrinkageZ, "extrusion.shrinkage_z", s.shrinkage_z, 0.0, 20.0, "%.2f %%", 0.01f);
    } else if (id == "fan") {
        mm(T::FanMin, "cooling.fan_min", s.fan_min, 0.0, 100.0, "%.0f %%", 0.5f);
        mm(T::FanMax, "cooling.fan_max", s.fan_max, 0.0, 100.0, "%.0f %%", 0.5f);
        mm(T::FanBelowTime, "cooling.fan_below_time", s.fan_below_time, 0.0, 600.0, "%.0f s", 0.5f);
        flag(T::FanAlways, "cooling.fan_always", s.fan_always);
        mm(T::FanBridges, "cooling.fan_bridges", s.fan_bridges, 0.0, 100.0, "%.0f %%", 0.5f);
        whole(T::FanOffLayers, "cooling.fan_off_layers", s.fan_off_layers, 0, 100);
        whole(T::FullFanLayer, "cooling.full_fan_layer", s.full_fan_layer, 0, 1000);
    } else if (id == "overhang_fan") {
        flag(T::DynamicOverhangFan, "cooling.dynamic_overhang_fan", s.dynamic_overhang_fan);
        const T labels[4] = {T::OverhangFan0, T::OverhangFan1, T::OverhangFan2, T::OverhangFan3};
        const char* keys[4] = {"cooling.overhang_fan_0", "cooling.overhang_fan_1", "cooling.overhang_fan_2",
                               "cooling.overhang_fan_3"};
        for (std::size_t k = 0; k < 4; ++k)
            mm(labels[k], keys[k], s.overhang_fans[k], 0.0, 100.0, "%.0f %%", 0.5f);
    } else if (id == "short_layers") {
        flag(T::CoolingEnabled, "cooling.enabled", s.cooling_enabled);
        mm(T::MinLayerTime, "cooling.min_layer_time", s.min_layer_time, 0.0, 120.0, "%.0f s", 0.2f);
        mm(T::MinSpeed, "cooling.min_speed", s.min_print_speed, 0.0, 100.0, "%.0f mm/s", 0.5f);
        flag(T::KeepOuterSpeed, "cooling.keep_outer_speed", s.keep_outer_speed, tr(T::KeepOuterSpeedTip));
        if (shown("cooling.slowdown_logic"))
            changed |= row(T::SlowdownLogicRow, s.slowdown_logic,
                           {{SlowdownLogic::Uniform, tr(T::SlowdownUniform)},
                            {SlowdownLogic::ConsistentSurface, tr(T::SlowdownConsistentSurface)}});
        if (s.slowdown_logic == SlowdownLogic::ConsistentSurface)
            mm(T::TransitionDistance, "cooling.transition_distance", s.transition_distance, 0.0, 1000.0, "%.1f mm", 0.1f);
    } else if (id == "flow") {
        mm(T::FlowRow, "extrusion.flow", s.flow, 10.0, 300.0, "%.1f %%", 0.05f, tr(T::FlowTip));
        mm(T::BridgeFlowRatio, "extrusion.bridge_flow_ratio", s.bridge_flow_ratio, 0.1, 2.0, "%.2f", 0.01f);
        // The share of the purge this filament asks for (multi_extruder.md, rule 12).
        if (s.purge_multiplier.size() == 1)
            mm(T::PurgeMultiplier, "extrusion.purge_multiplier", s.purge_multiplier.front(), 0.0, 1000.0, "%.0f %%", 0.5f);
        else
            count(T::PurgeMultiplier, "extrusion.purge_multiplier", s.purge_multiplier.size());
        if (s.min_purge_on_tower.size() == 1)
            mm(T::MinPurgeOnTower, "extrusion.min_purge_on_tower", s.min_purge_on_tower.front(), 0.0, 10000.0, "%.0f mm³", 0.5f);
        else
            count(T::MinPurgeOnTower, "extrusion.min_purge_on_tower", s.min_purge_on_tower.size());
    } else if (id == "pressure") {
        mm(T::PressureAdvance, "extrusion.pressure_advance", s.pressure_advance, 0.0, 2.0, "%.3f", 0.001f);
    } else if (id == "max_flow") {
        mm(T::FilamentMaxFlow, "extrusion.max_flow", s.filament_max_flow, 0.0, 200.0,
           s.filament_max_flow > 0.0 ? "%.1f mm³/s" : "—", 0.1f);
        mm(T::InfillMaxSpeed, "extrusion.infill_max_speed", s.infill_max_speed, 0.0, 1000.0,
           s.infill_max_speed > 0.0 ? "%.0f mm/s" : "—", 0.5f);
        mm(T::InfillMaxCrossingSpeed, "extrusion.infill_max_crossing_speed", s.infill_max_crossing_speed, 0.0, 1000.0,
           s.infill_max_crossing_speed > 0.0 ? "%.0f mm/s" : "—", 0.5f);
    } else if (id == "retraction") {
        mm(T::Retraction, "retraction.length", s.retract_length, 0.0, 20.0, "%.2f mm", 0.01f);
        mm(T::RetractSpeed, "retraction.speed", s.retract_speed, 1.0, 200.0, "%.0f mm/s", 0.5f);
        mm(T::UnretractSpeed, "retraction.unretract_speed", s.unretract_speed, 0.0, 1000.0,
           s.unretract_speed > 0.0 ? "%.0f mm/s" : "—", 0.5f);
        mm(T::RestartExtra, "retraction.restart_extra", s.restart_extra, 0.0, 10.0, "%.2f mm", 0.01f);
        flag(T::RetractLayerChange, "retraction.on_layer_change", s.retract_layer_change);
        flag(T::WipeRow, "retraction.wipe", s.wipe);
        mm(T::BeforeWipe, "retraction.before_wipe", s.before_wipe, 0.0, 100.0, "%.0f %%", 0.5f);
        flag(T::FirmwareRetraction, "retraction.firmware", s.firmware_retraction);
    } else if (id == "retract_travel") {
        mm(T::RetractLift, "retraction.lift", s.retract_lift, 0.0, 5.0, "%.2f mm", 0.01f);
        mm(T::RetractMinTravel, "retraction.min_travel", s.retract_min_travel, 0.0, 50.0, "%.1f mm", 0.05f);
        mm(T::LiftAbove, "retraction.lift_above", s.lift_above, 0.0, 10000.0, s.lift_above > 0.0 ? "%.2f mm" : "—", 0.05f);
        mm(T::LiftBelow, "retraction.lift_below", s.lift_below, 0.0, 10000.0, s.lift_below > 0.0 ? "%.2f mm" : "—", 0.05f);
        flag(T::RampingLift, "travel.ramping_lift", s.ramping_lift);
        mm(T::TravelSlope, "travel.slope", s.travel_slope, 0.0, 89.0, "%.1f°", 0.1f);
        mm(T::MaxLift, "travel.max_lift", s.max_lift, 0.0, 10.0, s.max_lift > 0.0 ? "%.2f mm" : "—", 0.01f);
        flag(T::LiftBeforeObstacle, "travel.lift_before_obstacle", s.lift_before_obstacle);
    } else if (id == "coast") {
        mm(T::Coast, "retraction.coast", s.coast, 0.0, 20.0, s.coast > 0.0 ? "%.2f mm" : "—", 0.01f);
    } else if (id == "material_retraction") {
        count(T::FilamentRetractionRow, "extrusion.retraction", s.filament_retraction.size());
    } else if (id == "needs") {
        flag(T::Abrasive, "extrusion.abrasive", s.abrasive);
        flag(T::NeedsEnclosure, "extrusion.needs_enclosure", s.needs_enclosure);
    }
    // Printer.
    else if (id == "bed_volume") {
        // Filament or resin: the panel shows that kind's settings, and a
        // resin printer's plate is its display (resin.md).
        if (shown("machine.technology") &&
            row(T::Technology, s.machine_technology,
                {{MachineTechnology::Fdm, tr(T::TechnologyFdm)}, {MachineTechnology::Msla, tr(T::TechnologyMsla)}})) {
            if (s.machine_technology == MachineTechnology::Msla)
                s.bed = {{0.0, 0.0}, {s.display_width, 0.0}, {s.display_width, s.display_height}, {0.0, s.display_height}};
            bed = changed = true;
        }
        if (shown("machine.bed")) {
            double width = 0, depth = 0;
            if (rectangle_bed(s, width, depth)) {
                bool b = row(T::BedWidth, width, 20.0, 2000.0, "%.0f mm", 0.5f);
                b |= row(T::BedDepth, depth, 20.0, 2000.0, "%.0f mm", 0.5f);
                if (b) {
                    s.bed = {{0.0, 0.0}, {width, 0.0}, {width, depth}, {0.0, depth}};
                    bed = changed = true;
                }
            } else {
                label_cell(T::BedWidth);
                loc::TextDisabled("%zu", s.bed.size());
            }
        }
        mm(T::MaxHeight, "machine.max_height", s.max_height, 0.0, 2000.0, "%.0f mm", 0.5f);
        mm(T::ZOffset, "machine.z_offset", s.z_offset, -10.0, 10.0, "%.2f mm", 0.005f);
        mm(T::ClearanceRadius, "sequential.clearance_radius", s.clearance_radius, 0.0, 1000.0, "%.0f mm", 0.5f);
        mm(T::ClearanceHeight, "sequential.clearance_height", s.clearance_height, 0.0, 1000.0, "%.0f mm", 0.5f);
    } else if (id == "enclosure") {
        flag(T::Enclosed, "machine.enclosed", s.enclosed);
        mm(T::MaxChamberTemp, "machine.max_chamber_temp", s.max_chamber_temp, 0.0, 120.0, "%.0f °C", 0.5f);
    } else if (id == "head") {
        // The print head around the nozzle (nonplanar.md, rule 6).
        mm(T::HeadFreeAngleX, "head.free_angle_x", s.head_free_angle_x, 0.0, 89.0, s.head_free_angle_x > 0.0 ? "%.0f°" : "—", 0.5f);
        mm(T::HeadFreeAngleY, "head.free_angle_y", s.head_free_angle_y, 0.0, 89.0, s.head_free_angle_y > 0.0 ? "%.0f°" : "—", 0.5f);
        mm(T::HeadClearanceHeight, "head.clearance_height", s.head_clearance_height, 0.1, 1000.0, "%.1f mm", 0.1f);
        mm(T::HeadClearanceRadius, "head.clearance_radius", s.head_clearance_radius, 0.1, 1000.0, "%.1f mm", 0.1f);
        count(T::HeadProfile, "head.profile", s.head_profile.size());
    } else if (id == "belt") {
        // A belt printer (belt_printer.md).
        flag(T::BeltEnabled, "belt.enabled", s.belt_enabled, tr(T::BeltEnabledTip));
        mm(T::BeltAngle, "belt.angle", s.belt_angle, 10.0, 80.0, "%.0f°", 0.5f);
        flag(T::BeltIsZ, "belt.belt_is_z", s.belt_is_z);
    } else if (id == "resin_display") {
        // A resin printer's display (resin.md).
        whole(T::DisplayPixelsX, "machine.display_pixels_x", s.display_pixels_x, 1, 32767);
        whole(T::DisplayPixelsY, "machine.display_pixels_y", s.display_pixels_y, 1, 32767);
        const double width = s.display_width, height = s.display_height;
        mm(T::DisplayWidth, "machine.display_width", s.display_width, 1.0, 1000.0, "%.2f mm", 0.01f);
        mm(T::DisplayHeight, "machine.display_height", s.display_height, 1.0, 1000.0, "%.2f mm", 0.01f);
        if (s.display_width != width || s.display_height != height) {
            s.bed = {{0.0, 0.0}, {s.display_width, 0.0}, {s.display_width, s.display_height}, {0.0, s.display_height}};
            bed = true;
        }
        flag(T::DisplayMirrorX, "machine.display_mirror_x", s.display_mirror_x);
        flag(T::DisplayMirrorY, "machine.display_mirror_y", s.display_mirror_y);
    } else if (id == "resin_layers") {
        mm(T::ResinLayerHeight, "resin.layer_height", s.resin_layer_height, 0.005, 0.5, "%.3f mm", 0.001f);
    } else if (id == "resin_supports") {
        // Resin supports, pad and hollowing (resin_supports.md).
        flag(T::ResinSupports, "resin.supports", s.resin_supports);
        mm(T::ResinElevation, "resin.support_elevation", s.resin_support_elevation, 0.0, 50.0, "%.1f mm", 0.1f);
        mm(T::ResinSupportDensity, "resin.support_density", s.resin_support_density, 10.0, 400.0, "%.0f %%", 1.0f);
        mm(T::ResinTipDiameter, "resin.support_tip_diameter", s.resin_support_tip_diameter, 0.1, 2.0, "%.2f mm", 0.01f);
        mm(T::ResinTipPenetration, "resin.support_tip_penetration", s.resin_support_tip_penetration, 0.0, 2.0, "%.2f mm",
           0.01f);
        mm(T::ResinPillarDiameter, "resin.support_pillar_diameter", s.resin_support_pillar_diameter, 0.2, 5.0, "%.2f mm",
           0.01f);
        mm(T::ResinCriticalAngle, "resin.support_critical_angle", s.resin_support_critical_angle, 10.0, 80.0, "%.0f°",
           0.5f);
    } else if (id == "resin_pad") {
        flag(T::ResinPad, "resin.pad", s.resin_pad);
        flag(T::ResinPadAroundObject, "resin.pad_around_object", s.resin_pad_around_object);
        mm(T::ResinPadThickness, "resin.pad_thickness", s.resin_pad_thickness, 0.1, 10.0, "%.1f mm", 0.1f);
        mm(T::ResinPadMargin, "resin.pad_margin", s.resin_pad_margin, 0.0, 20.0, "%.1f mm", 0.1f);
        mm(T::ResinPadWallSlope, "resin.pad_wall_slope", s.resin_pad_wall_slope, 10.0, 90.0, "%.0f°", 0.5f);
    } else if (id == "resin_hollow") {
        flag(T::ResinHollow, "resin.hollow", s.resin_hollow);
        mm(T::ResinHollowWall, "resin.hollow_wall", s.resin_hollow_wall, 0.5, 20.0, "%.1f mm", 0.1f);
        mm(T::ResinHollowClosing, "resin.hollow_closing", s.resin_hollow_closing, 0.0, 20.0, "%.1f mm", 0.1f);
        mm(T::ResinHollowInfill, "resin.hollow_infill", s.resin_hollow_infill, 0.0, 60.0, "%.0f %%", 0.5f);
        mm(T::ResinHollowInfillCell, "resin.hollow_infill_cell", s.resin_hollow_infill_cell, 2.0, 50.0, "%.1f mm", 0.1f);
    } else if (id == "resin_exposure") {
        mm(T::ResinExposure, "resin.exposure", s.resin_exposure, 0.1, 600.0, "%.2f s", 0.01f);
        mm(T::ResinBottomExposure, "resin.bottom_exposure", s.resin_bottom_exposure, 0.1, 600.0, "%.1f s", 0.1f);
        whole(T::ResinBottomLayers, "resin.bottom_layers", s.resin_bottom_layers, 0, 1000);
        whole(T::ResinLightPwm, "resin.light_pwm", s.resin_light_pwm, 0, 255);
        whole(T::ResinBottomLightPwm, "resin.bottom_light_pwm", s.resin_bottom_light_pwm, 0, 255);
    } else if (id == "resin_moves") {
        mm(T::ResinLiftDistance, "resin.lift_distance", s.resin_lift_distance, 0.0, 100.0, "%.1f mm", 0.1f);
        mm(T::ResinLiftSpeed, "resin.lift_speed", s.resin_lift_speed, 0.01, 100.0, "%.2f mm/s", 0.01f);
        mm(T::ResinBottomLiftDistance, "resin.bottom_lift_distance", s.resin_bottom_lift_distance, 0.0, 100.0, "%.1f mm",
           0.1f);
        mm(T::ResinBottomLiftSpeed, "resin.bottom_lift_speed", s.resin_bottom_lift_speed, 0.01, 100.0, "%.2f mm/s", 0.01f);
        mm(T::ResinRetractSpeed, "resin.retract_speed", s.resin_retract_speed, 0.01, 100.0, "%.2f mm/s", 0.01f);
        mm(T::ResinWaitBeforeExposure, "resin.wait_before_exposure", s.resin_wait_before_exposure, 0.0, 600.0, "%.1f s",
           0.1f);
        mm(T::ResinWaitAfterCure, "resin.wait_after_cure", s.resin_wait_after_cure, 0.0, 600.0, "%.1f s", 0.1f);
        mm(T::ResinWaitAfterLift, "resin.wait_after_lift", s.resin_wait_after_lift, 0.0, 600.0, "%.1f s", 0.1f);
    } else if (id == "resin_corrections") {
        mm(T::ResinElephantFoot, "resin.elephant_foot", s.resin_elephant_foot, 0.0, 2.0, "%.2f mm", 0.01f);
        mm(T::ResinScaleX, "resin.scale_x", s.resin_scale_x, 0.5, 2.0, "%.4f", 0.0005f);
        mm(T::ResinScaleY, "resin.scale_y", s.resin_scale_y, 0.5, 2.0, "%.4f", 0.0005f);
        mm(T::ResinScaleZ, "resin.scale_z", s.resin_scale_z, 0.5, 2.0, "%.4f", 0.0005f);
        mm(T::ResinAbsoluteCorrection, "resin.absolute_correction", s.resin_absolute_correction, -1.0, 1.0, "%+.3f mm",
           0.001f);
        whole(T::ResinZCorrection, "resin.z_correction_layers", s.resin_z_correction_layers, 0, 100);
        mm(T::ResinGamma, "resin.gamma", s.resin_gamma, 0.0, 5.0, "%.2f", 0.01f);
        whole(T::ResinAntialiasing, "resin.antialiasing", s.resin_antialiasing, 1, 16);
    } else if (id == "resin_material") {
        mm(T::ResinDensity, "resin.density", s.resin_density, 0.01, 10.0, "%.2f g/ml", 0.01f);
        mm(T::ResinBottleCost, "resin.bottle_cost", s.resin_bottle_cost, 0.0, 1e6, "%.2f", 0.1f);
        mm(T::ResinBottleVolume, "resin.bottle_volume", s.resin_bottle_volume, 1.0, 1e6, "%.0f ml", 1.0f);
        // The bottle's weight over its volume is the density (gcode_extras.md, rule 2).
        if (shown("resin.bottle_weight") &&
            row(T::ResinBottleWeight, s.resin_bottle_weight, 0.001, 1000.0, "%.3f kg", 0.001f)) {
            s.resin_density = s.resin_bottle_weight * 1000.0 / s.resin_bottle_volume;
            changed = true;
        }
    } else if (id == "stability") {
        mm(T::NozzleForce, "review.nozzle_force", s.review_nozzle_force, 0.0, 1000.0, "%.3f N", 0.001f);
        mm(T::BedAdhesion, "review.bed_adhesion", s.review_bed_adhesion, 0.0, 1000.0, "%.3f N/mm²", 0.001f);
        mm(T::MaxDeflection, "review.max_deflection", s.review_max_deflection, 0.001, 10.0, "%.3f mm", 0.001f);
    } else if (id == "nozzle_size") {
        // One nozzle per extruder (multimaterial.md, rule 1): each under the
        // general one, which sets them all and says "different nozzles"
        // while they differ.
        if (shown("machine.nozzle_diameter")) {
            bool differ = false;
            for (int e = 2; e <= s.extruders; ++e)
                differ = differ || nozzle_of(s, e) != nozzle_of(s, 1);
            if (row(T::Nozzle, s.nozzle_diameter, 0.1, 2.0, differ ? tr(T::DifferentNozzles) : "%.2f mm", 0.005f)) {
                s.nozzle_diameters.clear();
                changed = true;
            }
            for (int e = 1; e <= s.extruders && s.extruders > 1 && !s.single_extruder_mmu; ++e) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(format(tr(T::ExtruderNozzle), e).c_str());
                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-FLT_MIN);
                double v = nozzle_of(s, e);
                const double low = 0.1, high = 2.0;
                ImGui::PushID("extruder_nozzle");
                ImGui::PushID(e);
                if (ImGui::DragScalar("##v", ImGuiDataType_Double, &v, 0.005f, &low, &high, field_text("##v", "%.2f mm", v),
                                      ImGuiSliderFlags_AlwaysClamp)) {
                    std::vector<double> nozzles;
                    for (int k = 1; k <= s.extruders; ++k)
                        nozzles.push_back(k == e ? v : nozzle_of(s, k));
                    s.nozzle_diameters = std::move(nozzles);
                    s.nozzle_diameter = s.nozzle_diameters.front();
                    changed = true;
                }
                ImGui::PopID();
                ImGui::PopID();
            }
        }
        flag(T::HardenedNozzle, "machine.hardened_nozzle", s.hardened_nozzle);
        mm(T::MinLayerHeight, "machine.min_layer_height", s.min_layer_height, 0.0, 2.0, "%.2f mm", 0.002f,
           tr(T::LayerLimitTip));
        mm(T::MaxLayerHeight, "machine.max_layer_height", s.max_layer_height, 0.0, 2.0, "%.2f mm", 0.002f,
           tr(T::LayerLimitTip));
    } else if (id == "extruder") {
        mm(T::FilamentDiameter, "machine.filament_diameter", s.filament_diameter, 1.0, 3.5, "%.2f mm", 0.005f);
        mm(T::MachineMaxFlow, "machine.max_flow", s.machine_max_flow, 0.0, 200.0,
           s.machine_max_flow > 0.0 ? "%.1f mm³/s" : "—", 0.1f);
        flag(T::RelativeE, "machine.relative_extrusion", s.relative_extrusion);
    } else if (id == "extruders") {
        // Several extruders (multi_extruder.md): a list by extruder edits
        // here while it holds one value for all.
        whole(T::ExtrudersRow, "machine.extruders", s.extruders, 1, kMaxExtruders);
        count(T::ExtruderOffsetRow, "machine.extruder_offset", s.extruder_offset.size());
        flag(T::SingleExtruderMmu, "machine.single_extruder_mmu", s.single_extruder_mmu);
        for (const auto& [label, key, list, top] :
             {std::tuple{T::ToolchangeLength, "retraction.toolchange_length", &s.toolchange_length, 200.0},
              std::tuple{T::ToolchangeRestartExtra, "retraction.toolchange_restart_extra", &s.toolchange_restart_extra, 10.0}}) {
            if (list->size() == 1)
                mm(label, key, list->front(), 0.0, top, "%.2f mm", 0.01f);
            else
                count(label, key, list->size());
        }
    } else if (id == "speeds") {
        mm(T::MaxFeedX, "machine.max_feed_x", s.max_feed_x, 0.0, 2000.0, "%.0f mm/s", 0.5f);
        mm(T::MaxFeedY, "machine.max_feed_y", s.max_feed_y, 0.0, 2000.0, "%.0f mm/s", 0.5f);
        mm(T::MaxFeedZ, "machine.max_feed_z", s.max_feed_z, 0.0, 500.0, "%.0f mm/s", 0.5f);
        mm(T::MaxFeedE, "machine.max_feed_e", s.max_feed_e, 0.0, 500.0, "%.0f mm/s", 0.5f);
        mm(T::CornerSpeed, "machine.corner_speed", s.corner_speed, 0.0, 100.0, "%.1f mm/s", 0.1f);
    } else if (id == "accelerations") {
        mm(T::AccelerationRow, "machine.acceleration", s.acceleration, 0.0, 100000.0, "%.0f mm/s²", 10.0f);
        mm(T::TravelAcceleration, "machine.travel_acceleration", s.travel_acceleration, 0.0, 100000.0, "%.0f mm/s²",
           10.0f);
        const auto accel = [&](T label, const char* key, double& v) {
            mm(label, key, v, 0.0, 100000.0, v > 0.0 ? "%.0f mm/s²" : "—", 10.0f);
        };
        accel(T::RetractAcceleration, "machine.retract_acceleration", s.retract_acceleration);
        accel(T::MaxAccelX, "machine.max_accel_x", s.max_accel_x);
        accel(T::MaxAccelY, "machine.max_accel_y", s.max_accel_y);
        accel(T::MaxAccelZ, "machine.max_accel_z", s.max_accel_z);
        accel(T::MaxAccelE, "machine.max_accel_e", s.max_accel_e);
    } else if (id == "jerk") {
        const auto feed = [&](T label, const char* key, double& v) {
            mm(label, key, v, 0.0, 1000.0, v > 0.0 ? "%.2f mm/s" : "—", 0.05f);
        };
        feed(T::JerkX, "machine.jerk_x", s.jerk_x);
        feed(T::JerkY, "machine.jerk_y", s.jerk_y);
        feed(T::JerkZ, "machine.jerk_z", s.jerk_z);
        feed(T::JerkE, "machine.jerk_e", s.jerk_e);
        mm(T::JunctionDeviation, "machine.junction_deviation", s.junction_deviation, 0.0, 1.0,
           s.junction_deviation > 0.0 ? "%.3f mm" : "—", 0.001f);
        feed(T::MinExtrudeFeed, "machine.min_extrude_feed", s.min_extrude_feed);
        feed(T::MinTravelFeed, "machine.min_travel_feed", s.min_travel_feed);
    } else if (id == "silent") {
        flag(T::SilentModeRow, "machine.silent_mode", s.silent_mode);
        count(T::SilentLimitsRow, "machine.silent", s.silent_limits.size());
    } else if (id == "max_temps") {
        mm(T::MaxNozzleTemp, "machine.max_nozzle_temp", s.max_nozzle_temp, 0.0, 600.0, "%.0f °C", 0.5f);
        mm(T::MaxBedTemp, "machine.max_bed_temp", s.max_bed_temp, 0.0, 200.0, "%.0f °C", 0.5f);
    } else if (id == "firmware") {
        if (shown("machine.flavor"))
            changed |= row(T::Flavor, s.flavor,
                           {{GcodeFlavor::Marlin, "Marlin 2"},
                            {GcodeFlavor::MarlinLegacy, "Marlin 1"},
                            {GcodeFlavor::Klipper, "Klipper"},
                            {GcodeFlavor::RepRapFirmware, "RepRapFirmware (Duet)"},
                            {GcodeFlavor::RepRap, "RepRap"},
                            {GcodeFlavor::Repetier, "Repetier"},
                            {GcodeFlavor::Smoothie, "Smoothieware"},
                            {GcodeFlavor::Teacup, "Teacup"},
                            {GcodeFlavor::MakerWare, "MakerWare (MakerBot)"},
                            {GcodeFlavor::Sailfish, "Sailfish (MakerBot)"},
                            {GcodeFlavor::Mach3, "Mach3, LinuxCNC"},
                            {GcodeFlavor::Machinekit, "Machinekit"},
                            {GcodeFlavor::NoExtrusion, tr(T::FlavorNoExtrusion)}});
        if (shown("machine.limits_usage"))
            changed |= row(T::LimitsUsageRow, s.limits_usage,
                           {{LimitsUsage::TimeOnly, tr(T::LimitsTimeOnly)},
                            {LimitsUsage::Emit, tr(T::LimitsEmit)},
                            {LimitsUsage::Ignore, tr(T::LimitsIgnore)}});
        flag(T::GcodeProgress, "gcode.progress", s.gcode_progress);
        if (shown("gcode.object_labels"))
            changed |= row(T::ObjectLabelsRow, s.object_labels,
                           {{ObjectLabels::None, tr(T::LabelsNone)},
                            {ObjectLabels::OctoPrint, tr(T::LabelsOctoPrint)},
                            {ObjectLabels::Firmware, tr(T::LabelsFirmware)}});
    } else if (id == "variables") {
        count(T::VariablesRow, "machine.variables", s.variables.size());
    } else if (id == "gcode_templates") {
        flag(T::AutoTemperatures, "gcode.auto_temperatures", s.auto_temperatures);
    } else if (id == "output_file") {
        count(T::PostProcessRow, "output.post_process", s.post_process.size());
    }
    ImGui::EndTable();
    // The file's name, full width (gcode_templates.md, rule 15).
    if (id == "output_file" && shown("output.filename_format")) {
        ImGui::TextUnformatted(tr(T::FilenameFormat));
        ImGui::SetNextItemWidth(-9.0f * scale_);
        changed |= ImGui::InputText("##filename_format", &s.filename_format);
    }
    // The names the GOO file's header carries (resin.md, rule 10), full
    // width, at most 31 bytes.
    for (const auto& [card, key, label, text] :
         {std::tuple{"resin_display", "resin.printer_name", T::ResinPrinterName, &s.resin_printer_name},
          std::tuple{"resin_material", "resin.profile_name", T::ResinProfileName, &s.resin_profile_name}})
        if (id == card && shown(key)) {
            ImGui::TextUnformatted(tr(label));
            ImGui::SetNextItemWidth(-9.0f * scale_);
            ImGui::PushID(key);
            changed |= ImGui::InputText("##name", text);
            ImGui::PopID();
            if (text->size() > 31)
                text->resize(31);
        }
    // The templates below the table, full width (gcode_templates.md).
    if (id == "gcode_templates" || id == "gcode_heights") {
        const std::pair<T, std::pair<const char*, std::string*>> texts[] = {
            {T::FilamentStartGcode, {"extrusion.start_gcode", &s.filament_start_gcode}},
            {T::FilamentEndGcode, {"extrusion.end_gcode", &s.filament_end_gcode}},
            {T::ColorChangeGcode, {"gcode.color_change", &s.gcode_color_change}},
            {T::PauseGcode, {"gcode.pause", &s.gcode_pause}},
            {T::CustomTemplateGcode, {"gcode.custom_template", &s.gcode_custom_template}},
            {T::BeforeLayerGcode, {"gcode.before_layer", &s.gcode_before_layer}},
            {T::AfterLayerGcode, {"gcode.after_layer", &s.gcode_after_layer}},
            {T::BetweenObjectsGcode, {"gcode.between_objects", &s.gcode_between_objects}}};
        for (const auto& [label, key_text] : texts) {
            // Each card its own: the colour change, the pause and the custom G-code by height.
            const std::string_view key = key_text.first;
            const bool by_height = key == "gcode.color_change" || key == "gcode.pause" || key == "gcode.custom_template";
            if (by_height != (id == "gcode_heights"))
                continue;
            if (shown(key_text.first)) {
                ImGui::TextUnformatted(tr(label));
                ImGui::PushID(key_text.first);
                changed |= ImGui::InputTextMultiline("##text", key_text.second,
                                                     ImVec2(-9.0f * scale_, ImGui::GetTextLineHeight() * 3));
                ImGui::PopID();
            }
        }
    }
    // The tool change's G-code below the table, full width (multi_extruder.md, rule 5).
    if (id == "extruders" && shown("gcode.toolchange")) {
        ImGui::TextUnformatted(tr(T::ToolchangeGcode));
        changed |= ImGui::InputTextMultiline("##toolchange", &s.gcode_toolchange,
                                             ImVec2(-9.0f * scale_, ImGui::GetTextLineHeight() * 3));
    }
    // The start and end G-code below the table, full width.
    if (id == "start_end") {
        if (shown("machine.start_gcode")) {
            ImGui::TextUnformatted(tr(T::StartGcode));
            changed |= ImGui::InputTextMultiline("##start", &s.start_gcode,
                                                 ImVec2(-9.0f * scale_, ImGui::GetTextLineHeight() * 7));
        }
        if (shown("machine.end_gcode")) {
            ImGui::TextUnformatted(tr(T::EndGcode));
            changed |= ImGui::InputTextMultiline("##end", &s.end_gcode,
                                                 ImVec2(-9.0f * scale_, ImGui::GetTextLineHeight() * 6));
        }
    }
    return changed;
}

void App::status_bar() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float height = kStatusBar * scale_;
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x, vp->Pos.y + vp->Size.y - height));
    ImGui::SetNextWindowSize(ImVec2(vp->Size.x, height));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, rgb(theme_.panel));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f * scale_, 0));
    ImGui::Begin("##status", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav);
    ImGui::SetCursorPosY((height - ImGui::GetTextLineHeight()) * 0.5f);
    if (mesh_) {
        const Vec3f size{placed_.max.x - placed_.min.x, placed_.max.y - placed_.min.y, placed_.max.z - placed_.min.z};
        const std::string name = objects_.size() > 1 ? std::to_string(objects_.size()) + " " + tr(T::Models)
                                                     : utf8(model_path_.filename());
        loc::Text("%s  ·  %s %s  ·  %.1f × %.1f × %.1f mm", name.c_str(),
                    number_text(mesh_->triangle_count()).c_str(), tr(T::Triangles), double(size.x), double(size.y),
                    double(size.z));
    } else {
        loc::TextDisabled("%s", tr(T::NoModel));
    }
    ImGui::SameLine(0, 30.0f * scale_);
    if (slice_job_.running()) {
        loc::TextColored(rgb(theme_.accent_hover), "%s %.1f s", tr(T::Slicing), glfwGetTime() - slice_started_);
    } else if (result_ && viewer_) {
        loc::TextDisabled("%s", tr(T::ViewerOnly));
    } else if (result_ && stale_) {
        loc::TextColored(rgb(0xE8A33D), "%s", tr(T::Stale));
    } else if (result_) {
        const double total = result_->slice_seconds + result_->gcode_seconds + result_->check_seconds;
        loc::Text("%s", format(tr(result_->staged ? T::GcodeAgainIn : T::SlicedIn), total).c_str());
        ImGui::SameLine(0, 16.0f * scale_);
        if (result_->resin)
            loc::TextColored(rgb(0x3FBF7F), "%s",
                               format(tr(T::ResinSliced), int(result_->resin->layers.size()), result_->resin->volume / 1000.0,
                                      result_->resin->weight)
                                   .c_str());
        else if (result_->check.ok())
            loc::TextColored(rgb(0x3FBF7F), "%s",
                               format(tr(T::GcodeOk), number_text(result_->check.stats.lines).c_str()).c_str());
        else
            loc::TextColored(rgb(0xE5484D), "%s", format(tr(T::GcodeBad), int(result_->check.problem_count)).c_str());
        // The slicing's warnings (thin details, short layers, what the
        // machine cannot give the filament), listed when hovered.
        if (!result_->warnings.empty()) {
            ImGui::SameLine(0, 16.0f * scale_);
            loc::TextColored(rgb(0xE8A33D), "%s", format(tr(T::Warnings), int(result_->warnings.size())).c_str());
            // A click lists them with a link to the help's problem of each (assistant.md, rule 9).
            if (ImGui::IsItemClicked())
                ImGui::OpenPopup("##review");
            if (ImGui::BeginPopup("##review")) {
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 36.0f);
                for (std::size_t i = 0; i < result_->warnings.size() && i < 20; ++i) {
                    const Warning& w = result_->warnings[i];
                    ImGui::PushID(int(i));
                    loc::BulletText("%s", w.message.c_str());
                    const std::string_view problem = review_problem(w.kind);
                    if (!problem.empty() && knowledge_)
                        for (std::size_t p = 0; p < knowledge_->problems.size(); ++p)
                            if (knowledge_->problems[p].id == problem) {
                                ImGui::SameLine();
                                if (ImGui::SmallButton(tr(T::ReviewFix))) {
                                    help_open_ = true, help_query_.clear(), help_back_.clear();
                                    help_tab_ = 0, help_select_ = true, help_page_ = HelpPage::Problem, help_item_ = p;
                                    ImGui::CloseCurrentPopup();
                                }
                            }
                    ImGui::PopID();
                }
                ImGui::PopTextWrapPos();
                ImGui::EndPopup();
            } else if (ImGui::IsItemHovered() && ImGui::BeginTooltip()) {
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 36.0f);
                for (std::size_t i = 0; i < result_->warnings.size() && i < 20; ++i)
                    loc::BulletText("%s", result_->warnings[i].message.c_str());
                if (result_->warnings.size() > 20)
                    loc::TextDisabled("…");
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }
        }
    } else {
        loc::TextDisabled("%s", tr(T::Ready));
    }
    // The chosen material's warnings (assistant.md, rule 14).
    if (const KnowledgeMaterial* m = assistant_material_ ? current_material() : nullptr) {
        const MaterialInfo info = material_info(*m);
        const std::vector<MaterialWarning> warnings = material_warnings(info, settings_);
        if (!warnings.empty()) {
            ImGui::SameLine(0, 16.0f * scale_);
            loc::TextColored(rgb(kWarningColour), "%s", material_warning_text(warnings.front(), info).c_str());
            if (warnings.size() > 1 && ImGui::IsItemHovered() && ImGui::BeginTooltip()) {
                for (const MaterialWarning w : warnings)
                    loc::BulletText("%s", material_warning_text(w, info).c_str());
                ImGui::EndTooltip();
            }
        }
    }
    // On the right: the 3D view's time per frame on the GPU, and what sliced.
    std::string right;
    if (view_ms_ > 0.0)
        right = format(tr(T::ViewTime), view_ms_, 1000.0 / std::max(view_ms_, 0.01));
    if (result_ && !viewer_)
        right += (right.empty() ? "" : "  ·  ") + (result_->gpu.empty() ? std::string("CPU") : "GPU: " + result_->gpu);
    if (!right.empty()) {
        ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::CalcTextSize(right.c_str()).x - 12.0f * scale_);
        loc::TextDisabled("%s", right.c_str());
        if (view_ms_ > 0.0 && ImGui::IsItemHovered())
            loc::SetTooltip("%s", tr(T::ViewTimeTip));
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void App::viewport() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float top = kTopBar * scale_, bottom = kStatusBar * scale_, left = kPanel * scale_;
    view_pos_ = ImVec2(vp->Pos.x + left, vp->Pos.y + top);
    view_size_ = ImVec2(std::max(vp->Size.x - left, 1.0f), std::max(vp->Size.y - top - bottom, 1.0f));
    ImGui::SetNextWindowPos(view_pos_);
    ImGui::SetNextWindowSize(view_size_);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##view", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNav);
    ImGui::InvisibleButton("##canvas", view_size_,
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
                               ImGuiButtonFlags_MouseButtonMiddle);
    const ImGuiIO& io = ImGui::GetIO();
    // The plate's models (docs/UI.md, «Herramientas de la placa»): a click
    // chooses one (or lays it on the face clicked, with that tool); dragging a
    // model moves it on the bed, dragging elsewhere turns the view.
    const bool editing = tab_ == Tab::Prepare && !objects_.empty() && !calibration_;
    const auto bed_point = [&](float z) -> std::optional<Vec3> {
        const Vec3 d = mouse_ray(), e = camera_.eye();
        if (std::abs(d.z) < 1e-6f)
            return std::nullopt;
        const float t = (z - e.z) / d.z;
        if (!(t > 0.0f))
            return std::nullopt;
        return e + d * t;
    };
    // The Paint tool: the brush follows the face under the mouse, and a
    // stroke paints the model it started on.
    const bool painting = editing && tool_ == PlateTool::Paint;
    brush_depth_.reset();
    if (painting && ImGui::IsItemHovered())
        if (const auto hit = pick(placed_meshes_, plate_indexes(), camera_.eye(), mouse_ray()))
            brush_depth_ = float(hit->distance) * dot(mouse_ray(), camera_.forward());
    if (painting && ImGui::IsItemActivated() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        painting_object_ = -1;
        painted_ = false;
        if (const auto hit = pick(placed_meshes_, plate_indexes(), camera_.eye(), mouse_ray())) {
            selected_ = int(hit->object);
            painting_object_ = int(hit->object);
            brush_stroke(hit->object);
        }
    } else if (painting && painting_object_ >= 0 && ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
               (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f)) {
        brush_stroke(std::size_t(painting_object_));
    }
    if (painting && ImGui::IsItemDeactivated()) {
        if (painted_) { // the slice no longer matches the paint
            ++revision_.model;
            if (result_ && !viewer_)
                stale_ = true;
        }
        painting_object_ = -1;
        painted_ = false;
    }
    if (brush_depth_ && view_size_.y > 0.0f) {
        const float pixels = brush_radius_ / (*brush_depth_ * std::tan(0.5f * camera_.lens_fov())) * 0.5f * view_size_.y;
        ImGui::GetWindowDrawList()->AddCircle(io.MousePos, pixels, IM_COL32(255, 255, 255, 200), 48, 1.5f * scale_);
    }
    if (editing && !painting && ImGui::IsItemActivated() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        pressed_object_ = -1;
        dragged_ = false;
        const Vec3 d = mouse_ray(), e = camera_.eye();
        if (const auto hit = pick(placed_meshes_, plate_indexes(), e, d)) {
            selected_ = int(hit->object);
            if (tool_ == PlateTool::Measure) {
                // A face's plane, or with Shift its nearest vertex; another
                // object starts again (rule 20: within one object).
                const TriangleMesh& placed = placed_meshes_[hit->object];
                const float t = float(hit->distance);
                const Point3 at{e.x + d.x * t, e.y + d.y * t, e.z + d.z * t};
                if (int(hit->object) != measure_object_ || measure_picks_.size() >= 2)
                    measure_picks_.clear();
                measure_object_ = int(hit->object);
                measure_picks_.push_back(io.KeyShift ? point_feature(placed, hit->triangle, at)
                                                     : surface_feature(placed, hit->triangle));
            } else if (tool_ == PlateTool::LayFlat) {
                PlateObject& o = objects_[hit->object];
                o.place.rotation = laid_on(o.place.rotation, triangle_normal(placed_meshes_[hit->object], hit->triangle));
                plate_changed();
            } else if (const auto p = bed_point(e.z + d.z * float(hit->distance))) {
                pressed_object_ = int(hit->object);
                drag_anchor_ = *p;
            }
        }
    }
    if (ImGui::IsItemActive()) {
        if (pressed_object_ >= 0 && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2.0f)) {
            if (const auto p = bed_point(drag_anchor_.z)) {
                // The view shifts the model; its mesh is made again once, when
                // the drag ends (a million triangles took 200 ms a frame).
                PlateObject& o = objects_[std::size_t(pressed_object_)];
                o.place.x += double(p->x - drag_anchor_.x);
                o.place.y += double(p->y - drag_anchor_.y);
                drag_offset_.x += p->x - drag_anchor_.x;
                drag_offset_.y += p->y - drag_anchor_.y;
                drag_anchor_ = *p;
                dragged_ = true;
            }
        } else if (pressed_object_ < 0 && painting_object_ < 0 && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
            camera_.orbit(io.MouseDelta.x, io.MouseDelta.y);
            moved_frame_ = frames_;
        }
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.0f) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f)) {
            camera_.pan(io.MouseDelta.x, io.MouseDelta.y, view_size_.y);
            moved_frame_ = frames_;
        }
    }
    if (ImGui::IsItemDeactivated() && editing) {
        if (dragged_)
            plate_changed(); // the move is over: now the slice is out of date
        else if (pressed_object_ < 0 && tool_ != PlateTool::LayFlat && tool_ != PlateTool::Paint &&
                 ImGui::IsMouseReleased(ImGuiMouseButton_Left) &&
                 std::abs(io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Left]) < 9.0f)
            selected_ = -1; // a click on the empty bed
        pressed_object_ = -1;
        dragged_ = false;
    }
    if (ImGui::IsItemHovered()) {
        if (io.MouseWheel != 0.0f) {
            camera_.zoom(io.MouseWheel);
            moved_frame_ = frames_;
        }
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            frame_all();
    }
    // A right click on a model, not a drag: its menu (settings_levels.md, rule 10).
    if (editing && ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right) &&
        io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Right] < 9.0f)
        if (const auto hit = pick(placed_meshes_, plate_indexes(), camera_.eye(), mouse_ray())) {
            selected_ = int(hit->object);
            ImGui::OpenPopup("##object_menu");
        }
    object_menu();
    ImGui::End();
    ImGui::PopStyleVar();
}

// The camera's buttons, a row at the top of the view.
void App::view_tools() {
    struct Tool {
        const char* icon;
        T tip;
        int action; // a StandardView, or -1/-2/-3 for zoom in, out and frame
    };
    const Tool tools[] = {{"tisma_view", T::ViewIso, int(StandardView::Iso)},
                          {"tisma_view_top", T::ViewTop, int(StandardView::Top)},
                          {"tisma_view_front", T::ViewFront, int(StandardView::Front)},
                          {"tisma_view_back", T::ViewBack, int(StandardView::Back)},
                          {"tisma_view_left", T::ViewLeft, int(StandardView::Left)},
                          {"tisma_view_right", T::ViewRight, int(StandardView::Right)},
                          {"tisma_zoom_in", T::ZoomIn, -1},
                          {"tisma_zoom_out", T::ZoomOut, -2},
                          {"tisma_zoom_fit", T::ZoomFit, -3}};
    const float size = 32.0f * scale_;
    ImGui::SetNextWindowPos(ImVec2(view_pos_.x + view_size_.x * 0.5f, view_pos_.y + 10.0f * scale_), ImGuiCond_Always,
                            ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.85f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f * scale_);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f * scale_, 4.0f * scale_));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f * scale_, 0));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, rgb(theme_.panel));
    ImGui::Begin("##view_tools", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    for (const Tool& t : tools) {
        if (t.action == -1)
            ImGui::SameLine(0, 10.0f * scale_);
        if (icon_button(t.icon, t.icon, tr(t.tip), size)) {
            if (t.action >= 0) {
                camera_.set_view(static_cast<StandardView>(t.action));
                frame_all();
            } else if (t.action == -1) {
                camera_.zoom(1.0f);
            } else if (t.action == -2) {
                camera_.zoom(-1.0f);
            } else {
                frame_all();
            }
        }
        ImGui::SameLine();
    }

    // How the view is drawn.
    ImGui::SameLine(0, 10.0f * scale_);
    const T modes[] = {T::RenderFast, T::RenderAmbient, T::RenderRay};
    const T mode_tips[] = {T::RenderFastTip, T::RenderAmbientTip, T::RenderRayTip};
    if (button("##render", nullptr, tr(modes[int(render_mode_)]), false, false, true, tr(T::RenderTip), size))
        ImGui::OpenPopup("##render_menu");
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f * scale_, 7.0f * scale_));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f * scale_, 12.0f * scale_));
    if (ImGui::BeginPopup("##render_menu")) {
        loc::TextDisabled("%s", tr(T::Render));
        for (int m = 0; m < 3; ++m) {
            if (ImGui::RadioButton(tr(modes[m]), int(render_mode_) == m))
                render_mode_ = static_cast<RenderMode>(m);
            loc::SetItemTooltip("%s", tr(mode_tips[m]));
        }
        if (render_mode_ != RenderMode::Fast) {
            ImGui::SetNextItemWidth(200.0f * scale_);
            ImGui::SliderFloat(tr(T::AoStrength), &ao_strength_, 0.0f, 1.5f, field_text(tr(T::AoStrength), "%.2f", ao_strength_));
        }
        if (render_mode_ == RenderMode::RayTraced) {
            if (ray_failed_)
                loc::TextColored(rgb(0xE5484D), "%s", tr(T::RayTooBig));
            else if (!renderer_.has_ray_scene())
                loc::TextColored(rgb(theme_.accent_hover), "%s", tr(T::RayPreparing));
            else
                loc::TextDisabled(tr(T::RaySamples), renderer_.ray_samples(), 256);
        }
        ImGui::Separator();
        loc::TextDisabled("%s", tr(T::Antialiasing));
        std::vector<int> choices = {1};
        for (int n = 2; n <= std::min(renderer_.max_samples(), 8); n *= 2)
            choices.push_back(n);
        const auto name = [](int n) { return n == 1 ? std::string(tr(T::MsaaOff)) : "MSAA " + std::to_string(n) + "×"; };
        ImGui::SetNextItemWidth(200.0f * scale_);
        if (ImGui::BeginCombo("##msaa", name(std::min(msaa_, choices.back())).c_str())) {
            for (int n : choices)
                if (ImGui::Selectable(name(n).c_str(), n == msaa_))
                    msaa_ = n;
            ImGui::EndCombo();
        }
        ImGui::Checkbox(tr(T::Fxaa), &fxaa_);
        ImGui::Checkbox(tr(T::MoireFilter), &moire_);
        if (ImGui::IsItemHovered())
            loc::SetTooltip("%s", tr(T::MoireFilterTip));
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2);
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

// The plate's tools (docs/UI.md, «Herramientas de la placa»): a column at
// the left of the view, and next to it the chosen tool's numbers.
// A model's menu (settings_levels.md, rule 10): at the Normal and Advanced
// levels, shortcuts that give it settings of its own, starting from the
// profile's values, and any other setting it may have.
void App::object_menu() {
    if (!ImGui::BeginPopup("##object_menu"))
        return;
    if (selected_ < 0 || std::size_t(selected_) >= objects_.size()) {
        ImGui::EndPopup();
        return;
    }
    PlateObject& o = objects_[std::size_t(selected_)];
    loc::TextDisabled("%s", o.name.c_str());
    ImGui::Separator();
    if (view_level_ == ViewLevel::Basic) {
        loc::TextDisabled("%s", tr(T::ObjectSettingsBasic));
        ImGui::EndPopup();
        return;
    }
    const auto add = [&](const std::vector<std::string_view>& keys) {
        for (const std::string_view key : keys)
            o.settings = with_own_key(o.settings, settings_, key);
        object_settings_ = selected_;
        plate_changed();
    };
    if (ImGui::MenuItem(tr(T::ObjectInfill)))
        add({"infill.density", "infill.pattern"});
    if (ImGui::MenuItem(tr(T::ObjectWalls)))
        add({"walls.count"});
    if (ImGui::MenuItem(tr(T::ObjectSurfaces)))
        add({"surfaces.top_layers", "surfaces.bottom_layers"});
    if (ImGui::BeginMenu(tr(T::ObjectAddSetting))) {
        const std::vector<std::string> mine = own_keys(o.settings);
        for (const std::string& key : own_setting_keys(settings_))
            if (setting_place(key) && std::find(mine.begin(), mine.end(), key) == mine.end() &&
                ImGui::MenuItem((key_label(key) + "##" + key).c_str()))
                add({key});
        ImGui::EndMenu();
    }
    ImGui::Separator();
    // A modifier, a negative volume or a height range (model_tools.md, rules
    // 2, 3 and 6), over the middle of the model, in its own measures.
    const std::array<double, 3> size = model_size(o);
    const std::array<double, 3> half{std::max(1.0, size[0] / 2.0), std::max(1.0, size[1] / 2.0), std::max(1.0, size[2] / 2.0)};
    if (ImGui::MenuItem(tr(T::AddModifier))) {
        o.volumes.push_back({PlateVolume::Kind::Modifier, PlateVolume::Shape::Box, {0.0, 0.0, size[2] / 4.0}, half,
                             with_own_key("", settings_, "infill.density")});
        object_settings_ = selected_;
        plate_changed();
    }
    if (ImGui::MenuItem(tr(T::AddNegativeVolume))) {
        o.volumes.push_back({PlateVolume::Kind::Negative, PlateVolume::Shape::Cylinder, {0.0, 0.0, size[2] / 4.0}, half, {}});
        object_settings_ = selected_;
        plate_changed();
    }
    if (ImGui::MenuItem(tr(T::AddHeightRange))) {
        // From 0 to 2 mm with the profile's layer height (rule 6).
        o.ranges.push_back({0.0, 2.0, with_own_key("", settings_, "layers.height")});
        object_settings_ = selected_;
        plate_changed();
    }
    if ((!o.settings.empty() || !o.volumes.empty() || !o.ranges.empty()) && ImGui::MenuItem(tr(T::ObjectSettingsOpen)))
        object_settings_ = selected_;
    ImGui::EndPopup();
}

// The window with a model's own settings (rule 11): the panel's rows for its
// keys, on its values, each with a cross to give it back to the profile.
// Own settings over `under` (a JSON object of groups over the profile's):
// the panel's rows for own's keys on those values, a cross per key, and an
// Add button; with `layers`, a height range's layer height may be one. True
// when own changed.
bool App::own_settings_editor(std::string& own, const std::string& under, bool layers, std::string_view where) {
    bool any = false;
    const std::vector<std::string> keys = own_keys(own);
    if (keys.empty())
        loc::TextDisabled("%s", tr(T::ObjectSettingsNone));
    // Its cards, each once, in the order of its keys.
    std::vector<const SubsectionLayout*> cards;
    for (const std::string& key : keys)
        if (const std::optional<SettingPlace> place = setting_place(key))
            if (std::find(cards.begin(), cards.end(), place->subsection) == cards.end())
                cards.push_back(place->subsection);
    Settings mine = settings_;
    try {
        mine = own_settings(own_settings(settings_, under, where), own, where, layers);
    } catch (const std::exception&) {
    }
    bool changed = false, bed = false;
    row_only_ = std::set<std::string>(keys.begin(), keys.end());
    std::swap(settings_, mine);
    for (const SubsectionLayout* card : cards) {
        ImGui::SeparatorText(tr(card->title));
        ImGui::PushID(card->id.data(), card->id.data() + card->id.size());
        changed |= card_rows(card->id, bed);
        ImGui::PopID();
    }
    std::swap(settings_, mine);
    row_only_.clear();
    if (changed) {
        own = own_values(own, mine);
        any = true;
    }
    ImGui::Spacing();
    for (const std::string& key : keys) {
        ImGui::PushID(key.c_str());
        if (ImGui::SmallButton(("× " + key_label(key)).c_str())) {
            own = without_own_key(own, key);
            any = true;
        }
        if (ImGui::IsItemHovered())
            loc::SetTooltip("%s", tr(T::ObjectSettingsRemove));
        ImGui::PopID();
    }
    if (ImGui::SmallButton(tr(T::ObjectAddSetting)))
        ImGui::OpenPopup("##add_own");
    if (ImGui::BeginPopup("##add_own")) {
        std::vector<std::string> choices = own_setting_keys(settings_);
        if (layers)
            choices.insert(choices.begin(), "layers.height");
        Settings base = settings_;
        try {
            base = own_settings(settings_, under, where);
        } catch (const std::exception&) {
        }
        for (const std::string& key : choices)
            if (setting_place(key) && std::find(keys.begin(), keys.end(), key) == keys.end() &&
                ImGui::Selectable((key_label(key) + "##" + key).c_str())) {
                own = with_own_key(own, base, key);
                any = true;
            }
        ImGui::EndPopup();
    }
    return any;
}

// The window with a model's own settings (rule 11), and its modifiers,
// negative volumes and height ranges (model_tools.md, rules 2, 3 and 6).
void App::object_settings_window() {
    if (object_settings_ < 0 || std::size_t(object_settings_) >= objects_.size()) {
        object_settings_ = -1;
        return;
    }
    PlateObject& o = objects_[std::size_t(object_settings_)];
    bool open = true, changed = false;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x - 400.0f * scale_, vp->Pos.y + 120.0f * scale_),
                            ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImVec2(380.0f * scale_, 0.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(300.0f * scale_, 0.0f), ImVec2(FLT_MAX, vp->Size.y * 0.8f));
    const std::string title = format(tr(T::ObjectSettingsTitle), o.name.c_str()) + "###object_settings";
    if (ImGui::Begin(title.c_str(), &open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::PushTextWrapPos(0.0f);
        loc::TextDisabled("%s", tr(T::ObjectSettingsHint));
        ImGui::PopTextWrapPos();
        ImGui::PushID("own");
        changed |= own_settings_editor(o.settings, {}, false, o.name);
        ImGui::PopID();
        // Its modifiers and negative volumes (rules 2 and 3).
        for (std::size_t k = 0; k < o.volumes.size(); ++k) {
            ImGui::PushID(int(k) + 1000);
            const bool negative = o.volumes[k].kind == PlateVolume::Kind::Negative;
            const std::string head = format(tr(negative ? T::NegativeVolumeItem : T::ModifierItem), int(k) + 1);
            bool removed = false;
            if (ImGui::CollapsingHeader(head.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                changed |= volume_editor(o, k, removed);
            ImGui::PopID();
            if (removed)
                break;
        }
        // Its height ranges (rule 6).
        for (std::size_t k = 0; k < o.ranges.size(); ++k) {
            ImGui::PushID(int(k) + 2000);
            const std::string head = format(tr(T::RangeItem), o.ranges[k].from, o.ranges[k].to);
            bool removed = false;
            if (ImGui::CollapsingHeader((head + "###range").c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                changed |= range_editor(o, k, removed);
            ImGui::PopID();
            if (removed)
                break;
        }
    }
    ImGui::End();
    if (changed)
        plate_changed();
    if (!open)
        object_settings_ = -1;
}

// A modifier's or negative volume's shape, place, size and settings; true
// when something changed (`removed`: the volume was taken away).
bool App::volume_editor(PlateObject& o, std::size_t k, bool& removed) {
    PlateVolume& v = o.volumes[k];
    bool changed = false;
    const bool negative = v.kind == PlateVolume::Kind::Negative;
    int shape = int(v.shape);
    const char* shapes[] = {tr(T::ShapeBox), tr(T::ShapeCylinder)};
    ImGui::SetNextItemWidth(140.0f * scale_);
    if (ImGui::Combo("##shape", &shape, shapes, 2)) {
        v.shape = PlateVolume::Shape(shape);
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(tr(T::RemoveVolume))) {
        o.volumes.erase(o.volumes.begin() + std::ptrdiff_t(k));
        removed = true;
        return true;
    }
    const float field = 70.0f * scale_;
    const auto three = [&](const char* id, T label, std::array<double, 3>& values, double lo) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(tr(label));
        for (int c = 0; c < 3; ++c) {
            ImGui::SameLine(110.0f * scale_ + float(c) * (field + 4.0f * scale_));
            ImGui::SetNextItemWidth(field);
            ImGui::PushID(c);
            changed |= ImGui::DragScalar(id, ImGuiDataType_Double, &values[std::size_t(c)], 0.1f, &lo, nullptr, field_text(id, "%.1f", values[std::size_t(c)]));
            ImGui::PopID();
        }
    };
    three("##offset", T::VolumeOffset, v.offset, -1000.0);
    three("##size", T::VolumeSize, v.size, 0.1);
    if (!negative)
        changed |= own_settings_editor(v.settings, o.settings, false,
                                       format(tr(T::ModifierItem), int(k) + 1));
    return changed;
}

// A height range's heights and settings; true when something changed.
bool App::range_editor(PlateObject& o, std::size_t k, bool& removed) {
    PlateRange& r = o.ranges[k];
    bool changed = false;
    const float field = 70.0f * scale_;
    const double lo = 0.0;
    ImGui::SetNextItemWidth(field);
    changed |= ImGui::DragScalar(tr(T::HeightFrom), ImGuiDataType_Double, &r.from, 0.1f, &lo, &r.to, field_text(tr(T::HeightFrom), "%.2f mm", r.from));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(field);
    changed |= ImGui::DragScalar(tr(T::HeightTo), ImGuiDataType_Double, &r.to, 0.1f, &r.from, nullptr, field_text(tr(T::HeightTo), "%.2f mm", r.to));
    ImGui::SameLine();
    if (ImGui::SmallButton(tr(T::RemoveVolume))) {
        o.ranges.erase(o.ranges.begin() + std::ptrdiff_t(k));
        removed = true;
        return true;
    }
    changed |= own_settings_editor(r.settings, o.settings, true, format(tr(T::RangeItem), r.from, r.to));
    return changed;
}

// The plate's objects (settings_levels.md, rules 11 and 13): each with a row
// below it for each group of its own settings, each modifier or negative
// volume and each height range. The row chosen shows its settings under the
// list, from the profile's values.
void App::objects_list() {
    if (objects_.empty()) {
        loc::TextDisabled("%s", tr(T::ObjectsEmpty));
        return;
    }
    if (list_object_ >= int(objects_.size()))
        list_object_ = -1;
    bool changed = false;
    const float list_height = std::min(ImGui::GetContentRegionAvail().y * 0.4f,
                                       ImGui::GetFrameHeightWithSpacing() * float(4 + 3 * objects_.size()));
    ImGui::BeginChild("##object_list", ImVec2(0, list_height), ImGuiChildFlags_Borders);
    for (std::size_t i = 0; i < objects_.size(); ++i) {
        const PlateObject& o = objects_[i];
        ImGui::PushID(int(i));
        const bool here = list_object_ == int(i);
        if (ImGui::Selectable(o.name.c_str(), selected_ == int(i) && (!here || !list_row_))) {
            selected_ = list_object_ = int(i);
            list_row_.reset();
        }
        ImGui::Indent();
        for (const ObjectRow& row : object_rows(o)) {
            std::string label;
            if (row.kind == ObjectRow::Kind::Settings)
                label = row.group == "walls"    ? tr(T::ObjectWalls)
                        : row.group == "infill" ? tr(T::ObjectInfill)
                        : row.group == "surfaces" ? tr(T::ObjectSurfaces)
                                                  : row.group;
            else if (row.kind == ObjectRow::Kind::Volume)
                label = format(tr(o.volumes[row.index].kind == PlateVolume::Kind::Negative ? T::NegativeVolumeItem
                                                                                            : T::ModifierItem),
                               int(row.index) + 1);
            else
                label = format(tr(T::RangeItem), o.ranges[row.index].from, o.ranges[row.index].to);
            const bool chosen = here && list_row_ == row;
            if (ImGui::Selectable((label + "##" + std::to_string(int(row.kind)) + row.group + std::to_string(row.index)).c_str(),
                                  chosen)) {
                selected_ = list_object_ = int(i);
                list_row_ = row;
            }
        }
        ImGui::Unindent();
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::PushTextWrapPos(0.0f);
    loc::TextDisabled("%s", tr(T::ObjectsHint));
    ImGui::PopTextWrapPos();
    if (list_object_ < 0)
        return;
    PlateObject& o = objects_[std::size_t(list_object_)];
    // The row may be gone (removed in the object's window).
    if (const std::vector<ObjectRow> rows = object_rows(o);
        list_row_ && std::find(rows.begin(), rows.end(), *list_row_) == rows.end())
        list_row_.reset();
    ImGui::Separator();
    loc::TextColored(rgb(theme_.lavender), "%s", tr(T::ObjectsPicked));
    ImGui::BeginChild("##object_pick", ImVec2(0, 0));
    ImGui::PushID("pick");
    bool removed = false;
    if (!list_row_ || list_row_->kind == ObjectRow::Kind::Settings)
        changed |= own_settings_editor(o.settings, {}, false, o.name);
    else if (list_row_->kind == ObjectRow::Kind::Volume)
        changed |= volume_editor(o, list_row_->index, removed);
    else
        changed |= range_editor(o, list_row_->index, removed);
    if (removed)
        list_row_.reset();
    ImGui::PopID();
    ImGui::EndChild();
    if (changed)
        plate_changed();
}

void App::plate_tools() {
    const float size = 34.0f * scale_, margin = 12.0f * scale_;
    ImGui::SetNextWindowPos(ImVec2(view_pos_.x + margin, view_pos_.y + 60.0f * scale_));
    ImGui::SetNextWindowBgAlpha(0.88f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f * scale_);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f * scale_, 6.0f * scale_));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 4.0f * scale_));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, rgb(theme_.panel));
    ImGui::Begin("##plate_tools", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    const bool chosen = selected_ >= 0 && selected_ < int(objects_.size());
    if (button("##add", "tool_add", nullptr, false, false, true, tr(T::ToolAddTip), size)) {
        purpose_ = Purpose::AddModel;
        browser_.open(FileBrowser::Mode::Open, tr(T::ToolAdd), kModelExtensions, last_folder_);
    }
    if (button("##remove", "tool_remove", nullptr, false, false, chosen, tr(T::ToolRemoveTip), size)) {
        objects_.erase(objects_.begin() + selected_);
        selected_ = objects_.empty() ? -1 : std::min(selected_, int(objects_.size()) - 1);
        if (objects_.empty()) {
            result_.reset();
            renderer_.clear_preview();
            vk_view_.clear_preview();
        }
        plate_changed();
    }
    if (button("##duplicate", "tool_duplicate", nullptr, false, false, chosen, tr(T::ToolDuplicateTip), size)) {
        objects_.push_back(objects_[std::size_t(selected_)]);
        selected_ = int(objects_.size()) - 1;
        arrange_plate();
    }
    ImGui::Separator();
    const struct {
        const char* id;
        const char* icon;
        T tip;
        PlateTool tool;
    } tools[] = {{"##move", "tool_move", T::ToolMoveTip, PlateTool::Move},
                 {"##rotate", "tool_rotate", T::ToolRotateTip, PlateTool::Rotate},
                 {"##scale", "tool_scale", T::ToolScaleTip, PlateTool::Scale},
                 {"##lay", "tool_lay_flat", T::ToolLayFlatTip, PlateTool::LayFlat},
                 {"##cut", "tool_cut", T::ToolCutTip, PlateTool::Cut},
                 {"##measure", "tool_measure", T::ToolMeasureTip, PlateTool::Measure}};
    for (const auto& t : tools)
        if (button(t.id, t.icon, nullptr, false, tool_ == t.tool, true, tr(t.tip), size))
            tool_ = tool_ == t.tool ? PlateTool::Select : t.tool;
    ImGui::Separator();
    if (button("##arrange", "tool_arrange", nullptr, false, false, true, tr(T::ToolArrangeTip), size)) {
        arrange_plate();
        tip("arrange");
    }
    ImGui::Separator();
    if (button("##paint", "tool_paint", nullptr, false, tool_ == PlateTool::Paint, true, tr(T::ToolPaintTip), size))
        tool_ = tool_ == PlateTool::Paint ? PlateTool::Select : PlateTool::Paint;
    if (button("##material", "tool_material", nullptr, false, tool_ == PlateTool::Material, true, tr(T::ToolMaterialTip),
               size))
        tool_ = tool_ == PlateTool::Material ? PlateTool::Select : PlateTool::Material;
    const ImVec2 strip_min = ImGui::GetWindowPos(), strip_size = ImGui::GetWindowSize();
    anchor_rect("plate_tools", strip_min, ImVec2(strip_min.x + strip_size.x, strip_min.y + strip_size.y));
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);

    // Shortcuts, when no field takes the keys: M move, R rotate, S scale,
    // F lay flat, A arrange, Delete removes.
    if (!ImGui::GetIO().WantTextInput) {
        if (ImGui::IsKeyPressed(ImGuiKey_M, false))
            tool_ = PlateTool::Move;
        if (ImGui::IsKeyPressed(ImGuiKey_R, false) && !ImGui::GetIO().KeyCtrl)
            tool_ = PlateTool::Rotate;
        if (ImGui::IsKeyPressed(ImGuiKey_S, false) && !ImGui::GetIO().KeyCtrl)
            tool_ = PlateTool::Scale;
        if (ImGui::IsKeyPressed(ImGuiKey_F, false))
            tool_ = PlateTool::LayFlat;
        if (ImGui::IsKeyPressed(ImGuiKey_C, false) && !ImGui::GetIO().KeyCtrl)
            tool_ = PlateTool::Cut;
        if (ImGui::IsKeyPressed(ImGuiKey_U, false) && !ImGui::GetIO().KeyCtrl)
            tool_ = PlateTool::Measure;
        if (ImGui::IsKeyPressed(ImGuiKey_P, false) && !ImGui::GetIO().KeyCtrl)
            tool_ = PlateTool::Paint;
        if (ImGui::IsKeyPressed(ImGuiKey_A, false) && !ImGui::GetIO().KeyCtrl) {
            arrange_plate();
            tip("arrange");
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            tool_ = PlateTool::Select;
        if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) && chosen) {
            objects_.erase(objects_.begin() + selected_);
            selected_ = -1;
            if (objects_.empty()) {
                result_.reset();
                renderer_.clear_preview();
                vk_view_.clear_preview();
            }
            plate_changed();
        }
    }
    if (tool_ == PlateTool::Paint) {
        paint_card(ImVec2(strip_min.x + strip_size.x + 8.0f * scale_, strip_min.y));
        return;
    }
    if (tool_ == PlateTool::Select || !chosen)
        return;

    // The chosen tool's card.
    PlateObject& o = objects_[std::size_t(selected_)];
    ImGui::SetNextWindowPos(ImVec2(strip_min.x + strip_size.x + 8.0f * scale_, strip_min.y));
    ImGui::SetNextWindowBgAlpha(0.92f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f * scale_);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f * scale_, 12.0f * scale_));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, rgb(theme_.panel));
    ImGui::Begin("##plate_tool", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    loc::TextColored(rgb(theme_.accent_hover), "%s", o.name.c_str());
    const float field = 110.0f * scale_;
    bool changed = false;
    int mirror_axis = -1; // done after the card, as they change the plate's objects
    bool cut = false, split = false;
    const auto number = [&](const char* label, double& v, double speed, double lo, double hi, const char* fmt) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::SameLine(70.0f * scale_);
        ImGui::SetNextItemWidth(field);
        const bool edited = ImGui::DragScalar((std::string("##") + label).c_str(), ImGuiDataType_Double, &v, float(speed), &lo,
                                              &hi, field_text((std::string("##") + label).c_str(), fmt, v), ImGuiSliderFlags_AlwaysClamp);
        changed |= edited;
        return edited;
    };
    if (tool_ == PlateTool::Move) {
        loc::TextDisabled("%s", tr(T::ToolMoveHint));
        number("X", o.place.x, 0.1, -10000.0, 10000.0, "%.2f mm");
        number("Y", o.place.y, 0.1, -10000.0, 10000.0, "%.2f mm");
    } else if (tool_ == PlateTool::Rotate) {
        std::array<double, 3> angles = euler_degrees(o.place.rotation);
        bool turned = false;
        const char* axes[3] = {"X", "Y", "Z"};
        for (int k = 0; k < 3; ++k) {
            ImGui::PushID(k);
            turned |= number(axes[k], angles[std::size_t(k)], 0.5, -360.0, 360.0, "%.1f°");
            ImGui::SameLine();
            if (ImGui::SmallButton("-90")) {
                o.place.rotation = rotated(o.place.rotation, k, -90.0);
                changed = true;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("+90")) {
                o.place.rotation = rotated(o.place.rotation, k, 90.0);
                changed = true;
            }
            ImGui::PopID();
        }
        if (turned)
            o.place.rotation = from_euler_degrees(angles);
        if (ImGui::Button(tr(T::ResetRotation))) {
            o.place.rotation = kNoRotation;
            changed = true;
        }
    } else if (tool_ == PlateTool::Scale) {
        const BoundingBox box = o.mesh.bounding_box();
        const double own[3] = {double(box.max.x - box.min.x), double(box.max.y - box.min.y), double(box.max.z - box.min.z)};
        double percent[3], mm[3];
        for (int k = 0; k < 3; ++k)
            percent[k] = o.place.scale[std::size_t(k)] * 100.0, mm[k] = own[k] * o.place.scale[std::size_t(k)];
        changed |= ImGui::Checkbox(tr(T::ScaleTogether), &keep_scale_);
        const char* axes[3] = {"X", "Y", "Z"};
        for (int k = 0; k < 3; ++k) {
            ImGui::PushID(k);
            const double before_percent = percent[k], before_mm = mm[k];
            number(axes[k], percent[k], 0.5, 0.1, 100000.0, "%.1f %%");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(field);
            double lo = 0.001, hi = 100000.0;
            changed |= ImGui::DragScalar("##mm", ImGuiDataType_Double, &mm[k], 0.1f, &lo, &hi, field_text("##mm", "%.2f mm", mm[k]),
                                         ImGuiSliderFlags_AlwaysClamp);
            double factor = 1.0;
            if (percent[k] != before_percent)
                factor = percent[k] / before_percent;
            else if (mm[k] != before_mm && own[k] > 0.0)
                factor = mm[k] / before_mm;
            if (factor != 1.0 && std::isfinite(factor) && factor > 0.0) {
                for (int j = 0; j < 3; ++j)
                    if (keep_scale_ || j == k)
                        o.place.scale[std::size_t(j)] = std::max(o.place.scale[std::size_t(j)] * factor, 1e-3);
                changed = true;
            }
            ImGui::PopID();
        }
        if (ImGui::Button(tr(T::ResetScale))) {
            o.place.scale = {1.0, 1.0, 1.0};
            changed = true;
        }
        // Mirror on an axis of the bed (rule 16).
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(tr(T::Mirror));
        for (int k = 0; k < 3; ++k) {
            ImGui::SameLine();
            if (ImGui::Button(axes[k], ImVec2(36.0f * scale_, 0)))
                mirror_axis = k;
        }
    } else if (tool_ == PlateTool::LayFlat) {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 260.0f * scale_);
        loc::TextDisabled("%s", tr(T::ToolLayFlatHint));
        ImGui::PopTextWrapPos();
    } else if (tool_ == PlateTool::Measure) {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 260.0f * scale_);
        loc::TextDisabled("%s", tr(T::ToolMeasureHint));
        if (measure_object_ != selected_)
            measure_picks_.clear();
        // What each pick is, with a round surface's diameter or a cone's angle.
        for (std::size_t i = 0; i < measure_picks_.size(); ++i) {
            const MeasureFeature& f = measure_picks_[i];
            using Kind = MeasureFeature::Kind;
            if (f.kind == Kind::Circle)
                loc::BulletText("%s", format(tr(f.hole ? T::MeasureHole : T::MeasureCylinder), f.diameter).c_str());
            else if (f.kind == Kind::Cone)
                loc::BulletText("%s", format(tr(T::MeasureCone), f.angle, std::max(f.diameter, f.diameter_end),
                                               std::min(f.diameter, f.diameter_end))
                                            .c_str());
            else
                loc::BulletText("%s", tr(f.kind == Kind::Plane ? T::MeasurePlane : T::MeasurePoint));
        }
        // One hole or cylinder: writing the diameter it should have scales the model to it.
        if (measure_picks_.size() == 1 && measure_picks_[0].kind == MeasureFeature::Kind::Circle &&
            measure_picks_[0].diameter > 0.0) {
            const double now = measure_picks_[0].diameter;
            if (measure_wanted_ <= 0.0)
                measure_wanted_ = now;
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(tr(T::MeasureShouldBe));
            ImGui::SameLine();
            ImGui::SetNextItemWidth(field);
            const double lo = 0.001, hi = 100000.0;
            ImGui::DragScalar("##wanted_d", ImGuiDataType_Double, &measure_wanted_, 0.05f, &lo, &hi, field_text("##wanted_d", "Ø %.3f mm", measure_wanted_),
                              ImGuiSliderFlags_AlwaysClamp);
            ImGui::SameLine();
            if (ImGui::Button(tr(T::MeasureScale))) {
                const double factor = measure_wanted_ / now;
                for (double& s : o.place.scale)
                    s = std::max(s * factor, 1e-3);
                measure_picks_.clear();
                measure_wanted_ = 0.0;
                changed = true;
            }
        }
        if (measure_picks_.size() == 2) {
            const Measurement m = measure(measure_picks_[0], measure_picks_[1]);
            if (m.distance) {
                loc::Text("%s: %.3f mm", tr(T::MeasureDistance), *m.distance);
                // Writing what it should measure scales the model evenly.
                if (measure_wanted_ <= 0.0)
                    measure_wanted_ = *m.distance;
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(tr(T::MeasureShouldBe));
                ImGui::SameLine();
                ImGui::SetNextItemWidth(field);
                const double lo = 0.001, hi = 100000.0;
                ImGui::DragScalar("##wanted", ImGuiDataType_Double, &measure_wanted_, 0.1f, &lo, &hi, field_text("##wanted", "%.3f mm", measure_wanted_),
                                  ImGuiSliderFlags_AlwaysClamp);
                ImGui::SameLine();
                if (ImGui::Button(tr(T::MeasureScale)) && *m.distance > 0.0) {
                    const double factor = measure_wanted_ / *m.distance;
                    for (double& s : o.place.scale)
                        s = std::max(s * factor, 1e-3);
                    measure_picks_.clear();
                    measure_wanted_ = 0.0;
                    changed = true;
                }
            } else if (m.angle) {
                loc::Text("%s: %.2f°", tr(T::MeasureAngle), *m.angle);
            }
        } else if (measure_picks_.size() != 1) {
            measure_wanted_ = 0.0;
        }
        ImGui::PopTextWrapPos();
    } else if (tool_ == PlateTool::Cut) {
        // A horizontal plane at a height over the bed (rule 13): two closed
        // objects, side by side.
        const double tall = placed_size(o)[2];
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 260.0f * scale_);
        loc::TextDisabled("%s", tr(T::ToolCutHint));
        ImGui::PopTextWrapPos();
        cut_height_ = std::clamp(cut_height_, 0.0, tall);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(tr(T::CutHeight));
        ImGui::SameLine(70.0f * scale_);
        ImGui::SetNextItemWidth(field);
        const double lo = 0.0;
        ImGui::DragScalar("##cut_z", ImGuiDataType_Double, &cut_height_, 0.1f, &lo, &tall, field_text("##cut_z", "%.2f mm", cut_height_),
                          ImGuiSliderFlags_AlwaysClamp);
        ImGui::Checkbox(tr(T::CutFlipLower), &cut_flip_lower_);
        ImGui::BeginDisabled(!(cut_height_ > 0.0 && cut_height_ < tall));
        if (ImGui::Button(tr(T::CutButton), ImVec2(120.0f * scale_, 0)))
            cut = true;
        ImGui::EndDisabled();
        // Its loose pieces, each a model (rule 14).
        ImGui::Separator();
        if (ImGui::Button(tr(T::SplitButton)))
            split = true;
        if (ImGui::IsItemHovered())
            loc::SetTooltip("%s", tr(T::SplitTip));
    } else if (tool_ == PlateTool::Material) {
        // The object's extruder, one of the machine's or a mixed colour's
        // (multi_extruder.md, rule 1; color_mixing.md), and where a filament
        // change may purge (rule 13).
        bool material = false;
        const auto name_of = [&](int e) {
            return e <= settings_.extruders ? format(tr(T::ExtruderItem), e) : format(tr(T::VirtualExtruder), e);
        };
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(tr(T::ObjectExtruder));
        ImGui::SameLine(70.0f * scale_);
        ImGui::SetNextItemWidth(170.0f * scale_);
        if (ImGui::BeginCombo("##extruder", name_of(o.extruder).c_str())) {
            std::vector<int> choices;
            for (int e = 1; e <= std::max(settings_.extruders, 1); ++e)
                choices.push_back(e);
            for (const ColorMix& m : mixes_)
                if (mix_usable(m, settings_.extruders))
                    choices.push_back(m.id);
            for (const int e : choices)
                if (ImGui::Selectable(name_of(e).c_str(), e == o.extruder) && e != o.extruder) {
                    o.extruder = e;
                    material = true;
                }
            ImGui::EndCombo();
        }
        material |= ImGui::Checkbox(tr(T::WipeIntoInfill), &o.wipe_into_infill);
        material |= ImGui::Checkbox(tr(T::WipeIntoObject), &o.wipe_into_object);
        // A project that brings blends keeps the button to change them (rule 9).
        if (settings_.extruders >= 2 && (color_mixing_offered(settings_) || !mixes_.empty()) &&
            ImGui::Button(tr(T::ColorMixingDialog))) {
            mixing_open_ = true;
            mixes_before_ = mixes_;
            mix_edit_ = -1;
        }
        if (material)
            objects_material_changed();
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
    if (tool_ == PlateTool::Cut)
        cut_plane_overlay();
    if (tool_ == PlateTool::Measure)
        measure_overlay();
    if (changed)
        plate_changed();
    if (mirror_axis >= 0)
        mirror_selected(mirror_axis);
    if (cut)
        cut_selected();
    if (split)
        split_selected();
}

void App::split_selected() {
    const std::size_t index = std::size_t(selected_);
    const PlateObject& o = objects_[index];
    std::vector<TriangleMesh> pieces = split_mesh(placed_mesh(o));
    if (pieces.size() < 2) {
        notify(tr(T::SplitOnePiece));
        return;
    }
    std::vector<PlateObject> parts;
    for (std::size_t k = 0; k < pieces.size(); ++k) {
        PlateObject p{o.name + " (" + std::to_string(k + 1) + ")", std::move(pieces[k]), {}, {}};
        p.extruder = o.extruder;
        p.wipe_into_infill = o.wipe_into_infill, p.wipe_into_object = o.wipe_into_object;
        p.settings = o.settings;
        parts.push_back(std::move(p));
    }
    objects_.erase(objects_.begin() + std::ptrdiff_t(index));
    objects_.insert(objects_.begin() + std::ptrdiff_t(index), std::make_move_iterator(parts.begin()),
                    std::make_move_iterator(parts.end()));
    tool_ = PlateTool::Select;
    arrange_plate();
}

void App::mirror_selected(int axis) {
    PlateObject& o = objects_[std::size_t(selected_)];
    const BoundingBox from = o.mesh.bounding_box();
    // Its modifiers and negative volumes go where the mirror puts them: the
    // map from the model's old frame to its new one.
    Affine map;
    if (o.place.rotation == kNoRotation) {
        // On the model's own axis, which is the bed's: its scale stays.
        mirror_mesh(o.mesh, axis);
    } else {
        // Turned, the model takes its turn and size into its mesh first.
        TriangleMesh m = placed_mesh(o);
        map = placing(o, m);
        mirror_mesh(m, axis);
        o.mesh = std::move(m);
        o.place.rotation = kNoRotation;
        o.place.scale = {1.0, 1.0, 1.0};
    }
    // Then the mirror itself, about the centre of the mesh's box (mirror_mesh).
    const BoundingBox to = o.mesh.bounding_box();
    const double centre = axis == 0 ? 0.5 * (double(to.min.x) + to.max.x)
                          : axis == 1 ? 0.5 * (double(to.min.y) + to.max.y)
                                      : 0.5 * (double(to.min.z) + to.max.z);
    const std::size_t a = std::size_t(axis);
    for (std::size_t j = 0; j < 3; ++j)
        map.a[3 * a + j] = -map.a[3 * a + j];
    map.b[a] = 2.0 * centre - map.b[a];
    for (PlateVolume& v : o.volumes)
        v = carried_volume(v, from, map, to);
    plate_changed();
}

void App::cut_selected() {
    const std::size_t index = std::size_t(selected_);
    const PlateObject& o = objects_[index];
    CutResult halves = cut_mesh(placed_mesh(o), cut_height_);
    if (halves.below.empty() || halves.above.empty())
        return;
    if (cut_flip_lower_)
        rotate_mesh(halves.below, 180.0, 0.0, 0.0); // its cut face down on the bed
    // The halves keep the object's extruder; its painting was by triangle and
    // stays with the whole.
    PlateObject below{o.name + " (1)", std::move(halves.below), {}, {}};
    PlateObject above{o.name + " (2)", std::move(halves.above), {}, {}};
    for (PlateObject* half : {&below, &above}) {
        half->extruder = o.extruder;
        half->wipe_into_infill = o.wipe_into_infill, half->wipe_into_object = o.wipe_into_object;
        half->settings = o.settings;
    }
    objects_.erase(objects_.begin() + std::ptrdiff_t(index));
    objects_.insert(objects_.begin() + std::ptrdiff_t(index), {std::move(below), std::move(above)});
    tool_ = PlateTool::Select;
    arrange_plate();
}

// The Cut tool's plane over the chosen object, drawn over the 3D view.
void App::cut_plane_overlay() {
    if (!(view_size_.x > 1.0f) || !(view_size_.y > 1.0f) || selected_ < 0 ||
        std::size_t(selected_) >= placed_meshes_.size())
        return;
    const BoundingBox box = placed_meshes_[std::size_t(selected_)].bounding_box();
    if (box.empty)
        return;
    const Mat4 vp = camera_.projection(view_size_.x / view_size_.y) * camera_.view();
    const auto project = [&](float x, float y, float z, ImVec2& out) {
        const float cx = vp[0] * x + vp[4] * y + vp[8] * z + vp[12];
        const float cy = vp[1] * x + vp[5] * y + vp[9] * z + vp[13];
        const float w = vp[3] * x + vp[7] * y + vp[11] * z + vp[15];
        if (!(w > 1e-6f))
            return false;
        out = ImVec2(view_pos_.x + (cx / w * 0.5f + 0.5f) * view_size_.x, view_pos_.y + (0.5f - cy / w * 0.5f) * view_size_.y);
        return true;
    };
    const float mx = 0.15f * (box.max.x - box.min.x) + 2.0f, my = 0.15f * (box.max.y - box.min.y) + 2.0f;
    const float z = box.min.z + float(cut_height_);
    ImVec2 c[4];
    if (!project(box.min.x - mx, box.min.y - my, z, c[0]) || !project(box.max.x + mx, box.min.y - my, z, c[1]) ||
        !project(box.max.x + mx, box.max.y + my, z, c[2]) || !project(box.min.x - mx, box.max.y + my, z, c[3]))
        return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    dl->PushClipRect(view_pos_, ImVec2(view_pos_.x + view_size_.x, view_pos_.y + view_size_.y), true);
    dl->AddConvexPolyFilled(c, 4, ImGui::GetColorU32(rgb(theme_.accent, 0.28f)));
    dl->AddPolyline(c, 4, ImGui::GetColorU32(rgb(theme_.accent_hover, 0.9f)), 2.0f * scale_, ImDrawFlags_Closed);
    dl->PopClipRect();
}

// The models' modifiers (orange) and negative volumes (grey) over the 3D
// view, as wire shapes (model_tools.md, rules 2 and 3).
void App::volumes_overlay() {
    if (!(view_size_.x > 1.0f) || !(view_size_.y > 1.0f))
        return;
    const Mat4 vp = camera_.projection(view_size_.x / view_size_.y) * camera_.view();
    const auto project = [&](const Vec3f& p, ImVec2& out) {
        const float cx = vp[0] * p.x + vp[4] * p.y + vp[8] * p.z + vp[12];
        const float cy = vp[1] * p.x + vp[5] * p.y + vp[9] * p.z + vp[13];
        const float w = vp[3] * p.x + vp[7] * p.y + vp[11] * p.z + vp[15];
        if (!(w > 1e-6f))
            return false;
        out = ImVec2(view_pos_.x + (cx / w * 0.5f + 0.5f) * view_size_.x, view_pos_.y + (0.5f - cy / w * 0.5f) * view_size_.y);
        return true;
    };
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    dl->PushClipRect(view_pos_, ImVec2(view_pos_.x + view_size_.x, view_pos_.y + view_size_.y), true);
    for (std::size_t i = 0; i < objects_.size() && i < placed_meshes_.size(); ++i) {
        if (objects_[i].volumes.empty())
            continue;
        for (const PlateVolume& v : objects_[i].volumes) {
            const ImU32 colour = v.kind == PlateVolume::Kind::Negative ? IM_COL32(170, 170, 180, 220) : IM_COL32(240, 150, 40, 230);
            const TriangleMesh shape = volume_mesh(v, objects_[i], placed_meshes_[i]);
            // The outline of every face but the cylinder's caps' spokes.
            for (std::size_t t = 0; t < shape.triangle_count(); ++t) {
                ImVec2 a, b, c;
                if (!project(shape.corner(t, 0), a) || !project(shape.corner(t, 1), b) || !project(shape.corner(t, 2), c))
                    continue;
                if (v.shape == PlateVolume::Shape::Box)
                    dl->AddTriangle(a, b, c, colour, 1.2f * scale_);
                else if (t % 4 >= 2)
                    dl->AddLine(a, b, colour, 1.2f * scale_);
            }
        }
    }
    dl->PopClipRect();
}

// The Measure tool's picks over the 3D view: a dot at a vertex or at the
// face's centre, and a line between the two.
void App::measure_overlay() {
    if (!(view_size_.x > 1.0f) || !(view_size_.y > 1.0f) || measure_picks_.empty())
        return;
    const Mat4 vp = camera_.projection(view_size_.x / view_size_.y) * camera_.view();
    const auto project = [&](const Point3& p, ImVec2& out) {
        const float x = float(p[0]), y = float(p[1]), z = float(p[2]);
        const float cx = vp[0] * x + vp[4] * y + vp[8] * z + vp[12];
        const float cy = vp[1] * x + vp[5] * y + vp[9] * z + vp[13];
        const float w = vp[3] * x + vp[7] * y + vp[11] * z + vp[15];
        if (!(w > 1e-6f))
            return false;
        out = ImVec2(view_pos_.x + (cx / w * 0.5f + 0.5f) * view_size_.x, view_pos_.y + (0.5f - cy / w * 0.5f) * view_size_.y);
        return true;
    };
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    dl->PushClipRect(view_pos_, ImVec2(view_pos_.x + view_size_.x, view_pos_.y + view_size_.y), true);
    const ImU32 colour = ImGui::GetColorU32(rgb(theme_.accent_hover));
    ImVec2 at[2];
    std::size_t shown = 0;
    for (const MeasureFeature& f : measure_picks_)
        if (project(f.point, at[shown])) {
            dl->AddCircleFilled(at[shown], 5.0f * scale_, colour);
            ++shown;
        }
    if (shown == 2)
        dl->AddLine(at[0], at[1], colour, 2.0f * scale_);
    dl->PopClipRect();
}

// A change of what the objects print with: sliced again, the plate as it was.
void App::objects_material_changed() {
    ++revision_.model;
    if (result_ && !viewer_)
        stale_ = true;
}

// Filament `extruder`'s colour (from 1): the project's, else one of eight.
std::string App::filament_color(int extruder) const {
    static const char* const kColors[] = {"#FCE94F", "#204A87", "#CC0000", "#4E9A06",
                                          "#F57900", "#75507B", "#C17D11", "#555753"};
    if (extruder >= 1 && std::size_t(extruder) <= filament_colors_.size() && !filament_colors_[std::size_t(extruder - 1)].empty())
        return filament_colors_[std::size_t(extruder - 1)];
    return kColors[std::size_t(std::max(extruder - 1, 0)) % 8];
}

// The project's mixed colours (color_mixing.md, rules 1 to 3): the list on
// the left, each with its colour and shares; on the right the filaments and
// their two-colour blends, a click adds one at 1 : 1; below, the blend being
// edited, with a slider between two filaments and its layers.
void App::mixing_dialog() {
    if (!mixing_open_)
        return;
    const auto color_of = [](const std::string& hex) {
        unsigned v = 0x808080;
        if (hex.size() == 7 && hex[0] == '#')
            std::sscanf(hex.c_str() + 1, "%6x", &v);
        return ImVec4(float((v >> 16) & 0xFF) / 255.0f, float((v >> 8) & 0xFF) / 255.0f, float(v & 0xFF) / 255.0f, 1.0f);
    };
    const int filaments = std::max(settings_.extruders, 1);
    const auto recolor = [&](ColorMix& m) {
        std::vector<std::string> colors;
        std::vector<double> shares;
        for (const ColorMix::Component& c : m.components)
            colors.push_back(filament_color(c.extruder)), shares.push_back(c.share);
        m.color = mix_color(colors, shares);
    };
    const auto add = [&](int a, int b) {
        ColorMix m;
        m.id = filaments + 1;
        for (const ColorMix& other : mixes_)
            m.id = std::max(m.id, other.id + 1);
        m.components = {{a, 0.5}, {b, 0.5}};
        recolor(m);
        mixes_.push_back(std::move(m));
        mix_edit_ = int(mixes_.size()) - 1;
    };
    ImGui::SetNextWindowSize(ImVec2(720.0f * scale_, 480.0f * scale_), ImGuiCond_Appearing);
    bool open = true;
    if (!ImGui::Begin(tr(T::ColorMixingDialog), &open, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        mixing_open_ = open;
        return;
    }
    // The list.
    ImGui::BeginChild("##mixes", ImVec2(260.0f * scale_, -ImGui::GetFrameHeightWithSpacing() * 1.5f), ImGuiChildFlags_Borders);
    for (std::size_t k = 0; k < mixes_.size(); ++k) {
        ColorMix& m = mixes_[k];
        ImGui::PushID(int(k));
        ImGui::ColorButton("##color", color_of(m.color), ImGuiColorEditFlags_NoTooltip, ImVec2(18.0f * scale_, 18.0f * scale_));
        ImGui::SameLine();
        std::string shares;
        for (const ColorMix::Component& c : m.components)
            shares += (shares.empty() ? "" : ", ") + std::to_string(c.extruder) + " - " +
                      std::to_string(int(std::lround(c.share * 100.0))) + " %";
        if (ImGui::Selectable((format(tr(T::VirtualExtruder), m.id) + "\n" + shares).c_str(), mix_edit_ == int(k), 0,
                              ImVec2(170.0f * scale_, 0)))
            mix_edit_ = int(k);
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) {
            mixes_.erase(mixes_.begin() + std::ptrdiff_t(k));
            mix_edit_ = -1;
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }
    if (ImGui::Button(tr(T::AddBlend)) && filaments >= 2)
        add(1, 2);
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("##blends", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 1.5f));
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(tr(T::MixingIntro));
    ImGui::PopTextWrapPos();
    // The filaments, each with its colour.
    filament_colors_.resize(std::size_t(filaments));
    for (int e = 1; e <= filaments; ++e) {
        ImGui::PushID(e);
        ImVec4 c = color_of(filament_color(e));
        float rgb3[3] = {c.x, c.y, c.z};
        if (ImGui::ColorEdit3("##filament", rgb3, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel)) {
            char hex[8];
            std::snprintf(hex, sizeof hex, "#%02X%02X%02X", unsigned(std::lround(rgb3[0] * 255.0f)),
                          unsigned(std::lround(rgb3[1] * 255.0f)), unsigned(std::lround(rgb3[2] * 255.0f)));
            filament_colors_[std::size_t(e - 1)] = hex;
            for (ColorMix& m : mixes_)
                recolor(m);
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(std::to_string(e).c_str());
        if (e < filaments)
            ImGui::SameLine();
        ImGui::PopID();
    }
    ImGui::SeparatorText(tr(T::TwoColorBlends));
    for (int a = 1; a <= filaments; ++a)
        for (int b = a + 1; b <= filaments; ++b) {
            ImGui::PushID(a * 64 + b);
            const std::string c = mix_color({filament_color(a), filament_color(b)}, {0.5, 0.5});
            if (ImGui::ColorButton("##pair", color_of(c), 0, ImVec2(36.0f * scale_, 36.0f * scale_)))
                add(a, b);
            loc::SetItemTooltip("%d+%d", a, b);
            ImGui::SameLine();
            ImGui::TextUnformatted((std::to_string(a) + "+" + std::to_string(b)).c_str());
            ImGui::SameLine();
            ImGui::PopID();
        }
    ImGui::NewLine();
    // The blend being edited.
    if (mix_edit_ >= 0 && mix_edit_ < int(mixes_.size())) {
        ColorMix& m = mixes_[std::size_t(mix_edit_)];
        ImGui::SeparatorText(format(tr(T::VirtualExtruder), m.id).c_str());
        ImGui::PushTextWrapPos(0.0f);
        loc::TextDisabled("%s", tr(T::MixingEditorHint));
        ImGui::PopTextWrapPos();
        bool edited = false;
        const auto pick = [&](const char* id, int& extruder) {
            ImGui::SetNextItemWidth(110.0f * scale_);
            if (ImGui::BeginCombo(id, format(tr(T::ExtruderItem), extruder).c_str())) {
                for (int e = 1; e <= filaments; ++e)
                    if (ImGui::Selectable(format(tr(T::ExtruderItem), e).c_str(), e == extruder))
                        extruder = e, edited = true;
                ImGui::EndCombo();
            }
        };
        if (m.components.size() >= 2) {
            pick("##a", m.components[0].extruder);
            ImGui::SameLine();
            int share = int(std::lround(m.components[1].share * 100.0));
            ImGui::SetNextItemWidth(200.0f * scale_);
            if (ImGui::SliderInt("##share", &share, 0, 100, "%d %%")) {
                const double rest = 1.0 - share / 100.0, others = m.components[0].share + (m.components.size() > 2 ? m.components[2].share : 0.0);
                m.components[1].share = share / 100.0;
                for (std::size_t c : {std::size_t(0), std::size_t(2)})
                    if (c < m.components.size())
                        m.components[c].share = others > 0.0 ? rest * m.components[c].share / others
                                                              : rest / double(m.components.size() - 1);
                edited = true;
            }
            ImGui::SameLine();
            pick("##b", m.components[1].extruder);
            loc::Text("%d %% / %d %%", int(std::lround(m.components[0].share * 100.0)),
                        int(std::lround(m.components[1].share * 100.0)));
        }
        bool third = m.components.size() > 2;
        ImGui::BeginDisabled(filaments < 3);
        if (ImGui::Checkbox(tr(T::AddThirdExtruder), &third)) {
            if (third) {
                for (ColorMix::Component& c : m.components)
                    c.share *= 2.0 / 3.0;
                m.components.push_back({3, 1.0 / 3.0});
            } else if (m.components.size() > 2) {
                const double gone = m.components[2].share;
                m.components.pop_back();
                for (ColorMix::Component& c : m.components)
                    c.share += gone / 2.0;
            }
            edited = true;
        }
        ImGui::EndDisabled();
        if (third && m.components.size() > 2)
            pick("##c", m.components[2].extruder);
        if (edited)
            recolor(m);
        ImGui::TextUnformatted(tr(T::DisplayColor));
        ImGui::SameLine();
        ImGui::ColorButton("##shown", color_of(m.color), 0, ImVec2(48.0f * scale_, 18.0f * scale_));
        // The layers of its cycle, as they repeat.
        ImGui::TextUnformatted(tr(T::LayerSequencePreview));
        std::vector<double> shares;
        std::vector<int> used;
        for (const ColorMix::Component& c : m.components)
            if (c.share > 0.0)
                shares.push_back(c.share), used.push_back(c.extruder);
        if (!shares.empty()) {
            const std::vector<int> order = mix_order(mix_cycle(shares));
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 at = ImGui::GetCursorScreenPos();
            const float w = 12.0f * scale_, h = 18.0f * scale_;
            for (int k = 0; k < 24; ++k) {
                const int e = used[std::size_t(order[std::size_t(k) % order.size()])];
                dl->AddRectFilled(ImVec2(at.x + float(k) * w, at.y), ImVec2(at.x + float(k + 1) * w - 1.0f, at.y + h),
                                  ImGui::GetColorU32(color_of(filament_color(e))));
            }
            ImGui::Dummy(ImVec2(24.0f * w, h));
        }
    }
    ImGui::EndChild();
    if (ImGui::Button(tr(T::Cancel))) {
        mixes_ = mixes_before_;
        open = false;
    }
    ImGui::SameLine();
    if (ImGui::Button(tr(T::OkButton))) {
        open = false;
        if (mixes_ != mixes_before_)
            objects_material_changed();
    }
    ImGui::End();
    mixing_open_ = open;
}

// The Paint tool's card: what to paint, the mark, the brush's size.
void App::paint_card(ImVec2 at) {
    ImGui::SetNextWindowPos(at);
    ImGui::SetNextWindowBgAlpha(0.92f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f * scale_);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f * scale_, 12.0f * scale_));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, rgb(theme_.panel));
    ImGui::Begin("##paint_tool", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    loc::TextColored(rgb(theme_.accent_hover), "%s", tr(T::ToolPaint));
    // Fuzzy skin and colours come with the engine's fuzzy skin and extruders.
    const struct {
        T label;
        PaintChannel channel;
        bool ready;
    } channels[] = {{T::PaintSupports, PaintChannel::Support, true},
                    {T::PaintSeam, PaintChannel::Seam, true},
                    {T::PaintFuzzy, PaintChannel::Fuzzy, true},
                    {T::PaintColors, PaintChannel::Color, false}};
    for (const auto& c : channels) {
        ImGui::BeginDisabled(!c.ready);
        if (ImGui::RadioButton(tr(c.label), paint_channel_ == c.channel)) {
            paint_channel_ = c.channel;
            paint_dirty_ = true;
        }
        ImGui::EndDisabled();
        if (!c.ready)
            loc::SetItemTooltip("%s", tr(T::PaintLater));
    }
    ImGui::Separator();
    if (paint_channel_ != PaintChannel::Fuzzy) { // fuzzy skin is painted or not
        if (ImGui::RadioButton(tr(T::PaintEnforce), paint_mark_ == kPaintEnforce))
            paint_mark_ = kPaintEnforce;
        ImGui::SameLine();
        if (ImGui::RadioButton(tr(T::PaintBlock), paint_mark_ == kPaintBlock))
            paint_mark_ = kPaintBlock;
    }
    ImGui::SetNextItemWidth(170.0f * scale_);
    ImGui::SliderFloat(tr(T::BrushSize), &brush_radius_, 0.2f, 20.0f, field_text(tr(T::BrushSize), "%.1f mm", brush_radius_), ImGuiSliderFlags_Logarithmic);
    const bool chosen = selected_ >= 0 && selected_ < int(objects_.size());
    ImGui::BeginDisabled(!chosen || objects_[std::size_t(std::max(selected_, 0))].painting.empty(paint_channel_));
    if (ImGui::Button(tr(T::PaintClear))) {
        objects_[std::size_t(selected_)].painting.clear(paint_channel_);
        paint_dirty_ = true;
        ++revision_.model;
        if (result_ && !viewer_)
            stale_ = true;
    }
    ImGui::EndDisabled();
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 260.0f * scale_);
    loc::TextDisabled("%s", tr(T::PaintHint));
    ImGui::PopTextWrapPos();
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

// Paints the face under the mouse, on this model only; Shift erases.
void App::brush_stroke(std::size_t object) {
    const Vec3 d = mouse_ray(), e = camera_.eye();
    const auto hit = pick(placed_meshes_, plate_indexes(), e, d);
    if (!hit || hit->object != object)
        return;
    Brush brush;
    const Vec3 c = e + d * float(hit->distance);
    brush.centre = {c.x, c.y, c.z};
    brush.radius = brush_radius_;
    brush.detail = std::max(0.05, double(brush_radius_) / 8.0);
    brush.view = {d.x, d.y, d.z};
    brush.mark = ImGui::GetIO().KeyShift                ? kPaintNone
                 : paint_channel_ == PaintChannel::Fuzzy ? kPaintEnforce
                                                         : paint_mark_;
    const Vec3f lo{c.x - brush_radius_, c.y - brush_radius_, c.z - brush_radius_};
    const Vec3f hi{c.x + brush_radius_, c.y + brush_radius_, c.z + brush_radius_};
    const std::vector<std::uint32_t> near = indexes_[object].near(lo, hi);
    if (paint(objects_[object].painting, paint_channel_, placed_meshes_[object], brush, &near) > 0) {
        painted_ = true;
        paint_dirty_ = true;
    }
}

// The view's copy of the painted pieces of the channel in use.
void App::update_paint_view() {
    if (!paint_dirty_)
        return;
    paint_dirty_ = false;
    const PaintMesh m = paint_mesh(objects_, placed_meshes_, paint_channel_);
    std::vector<Renderer::PaintGroup> groups;
    for (const PaintMesh::Group& g : m.groups)
        groups.push_back({g.first, g.count, vec(g.mark == kPaintBlock ? theme_.paint_block : theme_.paint_enforce)});
    renderer_.set_paint(m.vertices, groups);
    vk_view_.set_paint(m.vertices, groups);
}

namespace {

struct TipText {
    T title, text;
};

const std::map<std::string, TipText>& tip_texts() {
    static const std::map<std::string, TipText> tips = {
        {"move", {T::TipMoveTitle, T::TipMove}},
        {"rotate", {T::TipRotateTitle, T::TipRotate}},
        {"scale", {T::TipScaleTitle, T::TipScale}},
        {"lay", {T::TipLayTitle, T::TipLay}},
        {"arrange", {T::TipArrangeTitle, T::TipArrange}},
        {"paint", {T::TipPaintTitle, T::TipPaint}},
        {"slice", {T::TipSliceTitle, T::TipSlice}},
        {"preview", {T::TipPreviewTitle, T::TipPreview}},
        {"analysis", {T::TipAnalysisTitle, T::TipAnalysis}},
        {"calibration", {T::TipCalibrationTitle, T::TipCalibration}},
        {"device", {T::TipDeviceTitle, T::TipDevice}}};
    return tips;
}

// The assistant's steps (docs/UI.md, «Ayuda»).
constexpr std::array<std::pair<T, T>, 7> kTour = {{{T::Tour0Title, T::Tour0},
                                                   {T::Tour1Title, T::Tour1},
                                                   {T::Tour2Title, T::Tour2},
                                                   {T::Tour3Title, T::Tour3},
                                                   {T::Tour4Title, T::Tour4},
                                                   {T::Tour5Title, T::Tour5},
                                                   {T::Tour6Title, T::Tour6}}};

} // namespace

// The tip of a function, the first time it is used (unless tips are off or
// the assistant is on screen). A tip still on screen counts as seen.
void App::tip(const std::string& id) {
    if (!tips_ || tour_step_ >= 0 || tips_seen_.count(id) || !tip_texts().count(id))
        return;
    if (!tip_.empty())
        tips_seen_.insert(tip_);
    tip_ = id;
}

// The assistant, or else the tip on screen: a card at the bottom of the view.
void App::help_windows() {
    const bool tour = tour_step_ >= 0 && tour_step_ < int(kTour.size());
    if (!tour && tip_.empty())
        return;
    const float width = 420.0f * scale_;
    ImGui::SetNextWindowPos(ImVec2(view_pos_.x + 0.5f * view_size_.x, view_pos_.y + view_size_.y - 16.0f * scale_),
                            ImGuiCond_Always, ImVec2(0.5f, 1.0f));
    ImGui::SetNextWindowSize(ImVec2(width, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.96f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f * scale_);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f * scale_, 14.0f * scale_));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, rgb(theme_.panel));
    ImGui::PushStyleColor(ImGuiCol_Border, rgb(theme_.accent));
    ImGui::Begin("##help", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    const T title = tour ? kTour[std::size_t(tour_step_)].first : tip_texts().at(tip_).title;
    const T text = tour ? kTour[std::size_t(tour_step_)].second : tip_texts().at(tip_).text;
    ImGui::PushFont(nullptr, kTitleSize);
    loc::TextColored(rgb(theme_.accent_hover), "%s", tr(title));
    ImGui::PopFont();
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(tr(text));
    ImGui::PopTextWrapPos();
    ImGui::Spacing();
    if (tour) {
        // The welcome has no number; the steps, as their titles (UI1).
        if (tour_step_ > 0)
            loc::TextDisabled("%d / %d", tour_step_, int(kTour.size()) - 1);
        else
            ImGui::Dummy(ImVec2(0.0f, 0.0f));
        ImGui::SameLine();
        const bool last = tour_step_ + 1 == int(kTour.size());
        const float buttons = (last ? 1.0f : 2.0f) * 100.0f * scale_ + (tour_step_ > 0 ? 100.0f * scale_ : 0.0f);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, ImGui::GetContentRegionAvail().x - buttons));
        if (tour_step_ > 0 && ImGui::Button(tr(T::Back), ImVec2(92.0f * scale_, 0)))
            --tour_step_;
        if (tour_step_ > 0)
            ImGui::SameLine();
        if (!last && ImGui::Button(tr(T::SkipTour), ImVec2(92.0f * scale_, 0)))
            tour_step_ = -1;
        if (!last)
            ImGui::SameLine();
        if (ImGui::Button(tr(last ? T::Finish : T::Next), ImVec2(92.0f * scale_, 0)))
            tour_step_ = last ? -1 : tour_step_ + 1;
        // The first start's tutorials, once its assistant is over (rule 15).
        if (tour_step_ < 0 && first_run_)
            offer_tutorials("first_start");
    } else {
        if (ImGui::Button(tr(T::GotIt))) {
            tips_seen_.insert(tip_);
            tip_.clear();
        }
        ImGui::SameLine();
        if (ImGui::Button(tr(T::NoMoreTips))) {
            tips_ = false;
            tip_.clear();
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

void App::empty_hint() {
    const char* text = tr(T::DropHint);
    ImGui::PushFont(nullptr, kTitleSize);
    const ImVec2 size = ImGui::CalcTextSize(text);
    ImGui::PopFont();
    ImGui::SetNextWindowPos(ImVec2(view_pos_.x + (view_size_.x - size.x) * 0.5f - 24.0f * scale_,
                                   view_pos_.y + view_size_.y * 0.5f - size.y));
    ImGui::SetNextWindowBgAlpha(0.8f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f * scale_);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24.0f * scale_, 16.0f * scale_));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, rgb(theme_.panel));
    ImGui::Begin("##hint", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav);
    ImGui::PushFont(nullptr, kTitleSize);
    loc::TextColored(rgb(theme_.text_dim), "%s", text);
    ImGui::PopFont();
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

// What the icon row shows over the paths (preview.md, rule 11), drawn over
// the 3D view: the marks of the layers drawn, the wipes, the centre of
// gravity of what is drawn and the nozzle at its last extrusion.
void App::preview_overlay(const GcodePreview& p) {
    if (!(view_size_.x > 1.0f) || !(view_size_.y > 1.0f))
        return;
    const Mat4 vp = camera_.projection(view_size_.x / view_size_.y) * camera_.view();
    const auto project = [&](float x, float y, float z, ImVec2& out) {
        const float cx = vp[0] * x + vp[4] * y + vp[8] * z + vp[12];
        const float cy = vp[1] * x + vp[5] * y + vp[9] * z + vp[13];
        const float w = vp[3] * x + vp[7] * y + vp[11] * z + vp[15];
        if (!(w > 1e-6f))
            return false;
        out = ImVec2(view_pos_.x + (cx / w * 0.5f + 0.5f) * view_size_.x, view_pos_.y + (0.5f - cy / w * 0.5f) * view_size_.y);
        return true;
    };
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    dl->PushClipRect(view_pos_, ImVec2(view_pos_.x + view_size_.x, view_pos_.y + view_size_.y), true);
    const std::uint32_t lo = std::uint32_t(bottom_layer_), hi = std::uint32_t(top_layer_);
    ImVec2 a, b;
    if (show_wipes_)
        for (std::uint32_t l = lo; l <= hi && l < p.layers.size(); ++l)
            for (std::size_t i = p.layers[l].first; i < p.layers[l].first + p.layers[l].count; ++i)
                if (const PreviewMove& m = p.moves[i];
                    m.wipe && project(m.x0, m.y0, m.z0, a) && project(m.x1, m.y1, m.z1, b))
                    dl->AddLine(a, b, ImGui::GetColorU32(rgb(kWipeColour)), 2.0f * scale_);
    if (std::any_of(show_marks_.begin(), show_marks_.end(), [](bool on) { return on; }))
        for (const PreviewMark& m : p.marks)
            if (m.layer >= lo && m.layer <= hi && show_marks_[std::size_t(m.kind)] && project(m.x, m.y, m.z, a))
                dl->AddCircleFilled(a, 3.5f * scale_, ImGui::GetColorU32(rgb(kMarkColours[std::size_t(m.kind)])));
    if (show_gravity_) {
        // Each extrusion's middle, by its volume.
        double sum = 0.0, cx = 0.0, cy = 0.0, cz = 0.0;
        for (std::uint32_t l = lo; l <= hi && l < p.layers.size(); ++l)
            for (std::size_t e = p.layers[l].first_extrusion; e < p.layers[l].first_extrusion + p.layers[l].extrusion_count; ++e) {
                const PreviewMove& m = p.moves[p.extrusions[e]];
                const double length = std::hypot(double(m.x1) - m.x0, double(m.y1) - m.y0, double(m.z1) - m.z0);
                const double v = double(m.width) * m.height * length;
                sum += v;
                cx += v * 0.5 * (double(m.x0) + m.x1), cy += v * 0.5 * (double(m.y0) + m.y1);
                cz += v * (0.5 * (double(m.z0) + m.z1) - 0.5 * m.height);
            }
        if (sum > 0.0 && project(float(cx / sum), float(cy / sum), float(cz / sum), a)) {
            const float r = 7.0f * scale_;
            const ImU32 ink = ImGui::GetColorU32(rgb(0xFFFFFF)), shade = ImGui::GetColorU32(ImVec4(0, 0, 0, 0.6f));
            dl->AddCircleFilled(a, r, shade);
            dl->AddCircle(a, r, ink, 20, 1.8f * scale_);
            dl->AddLine(ImVec2(a.x - r * 1.5f, a.y), ImVec2(a.x + r * 1.5f, a.y), ink, 1.5f * scale_);
            dl->AddLine(ImVec2(a.x, a.y - r * 1.5f), ImVec2(a.x, a.y + r * 1.5f), ink, 1.5f * scale_);
        }
    }
    if (show_nozzle_ && hi < p.layers.size() && p.layers[hi].count > 0) {
        // At the end of the last move drawn (the move slider's).
        const std::size_t cut = move_cut_layer_ == int(hi) && move_cut_ > 0 ? std::size_t(move_cut_) : p.layers[hi].count;
        const PreviewMove& m = p.moves[p.layers[hi].first + std::min(cut, p.layers[hi].count) - 1];
        if (project(m.x1, m.y1, m.z1, a)) {
            // The nozzle standing on the point, and where it is when the mouse is on it.
            const float s = 16.0f * scale_;
            const ImU32 c = ImGui::GetColorU32(rgb(theme_.accent_hover)), edge = ImGui::GetColorU32(ImVec4(0, 0, 0, 0.7f));
            dl->AddRectFilled(ImVec2(a.x - s * 0.35f, a.y - s * 1.6f), ImVec2(a.x + s * 0.35f, a.y - s * 0.7f), c);
            dl->AddTriangleFilled(ImVec2(a.x - s * 0.4f, a.y - s * 0.7f), ImVec2(a.x + s * 0.4f, a.y - s * 0.7f), a, c);
            dl->AddTriangle(ImVec2(a.x - s * 0.4f, a.y - s * 0.7f), ImVec2(a.x + s * 0.4f, a.y - s * 0.7f), a, edge, 1.2f);
            const ImVec2 mouse = ImGui::GetIO().MousePos;
            if (!ImGui::GetIO().WantCaptureMouse && std::abs(mouse.x - a.x) < s * 0.5f && mouse.y > a.y - s * 1.7f &&
                mouse.y < a.y + 2.0f)
                loc::SetTooltip(tr(T::NozzleAt), double(m.x1), double(m.y1), double(m.z1), double(m.feed));
        }
    }
    dl->PopClipRect();
}

// A colour change, a pause (with its message) or G-code at the start of a
// layer (preview.md, rule 12): a change by height (gcode_templates.md, rule
// 13) halfway between that layer's top and the one below it, so it starts
// that layer; the G-code changes when sliced again.
void App::add_change_at(int layer, ChangeKind kind, std::string text) {
    if (!result_ || layer < 0 || std::size_t(layer) >= result_->preview.layers.size())
        return;
    const auto& layers = result_->preview.layers;
    const double top = layers[std::size_t(layer)].z;
    const double below = layer > 0 ? layers[std::size_t(layer) - 1].z : 0.0;
    HeightChange change;
    change.z = 0.5 * (top + below);
    change.color_change = kind == ChangeKind::Color;
    change.pause = kind == ChangeKind::Pause;
    if (kind == ChangeKind::Pause && !text.empty())
        change.gcode = "M117 " + text; // the message the printer shows
    else if (kind == ChangeKind::Gcode)
        change.gcode = std::move(text);
    settings_.changes.push_back(std::move(change));
    std::stable_sort(settings_.changes.begin(), settings_.changes.end(),
                     [](const HeightChange& a, const HeightChange& b) { return a.z < b.z; });
    settings_changed();
    notify(format(tr(kind == ChangeKind::Color ? T::ColorChangeAt : kind == ChangeKind::Pause ? T::PauseAt : T::GcodeAt),
                  layer + 1));
}

// Each part's name over it (E, preview.md, rule 10).
void App::part_labels() {
    if (!(view_size_.x > 1.0f) || !(view_size_.y > 1.0f))
        return;
    const Mat4 vp = camera_.projection(view_size_.x / view_size_.y) * camera_.view();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    for (std::size_t i = 0; i < objects_.size() && i < placed_meshes_.size(); ++i) {
        if (placed_meshes_[i].empty())
            continue;
        Vec3 lo{1e30f, 1e30f, 1e30f}, hi{-1e30f, -1e30f, -1e30f};
        for (const Vec3f& c : placed_meshes_[i].corners()) {
            lo = Vec3{std::min(lo.x, c.x), std::min(lo.y, c.y), std::min(lo.z, c.z)};
            hi = Vec3{std::max(hi.x, c.x), std::max(hi.y, c.y), std::max(hi.z, c.z)};
        }
        const Vec3 top{(lo.x + hi.x) * 0.5f, (lo.y + hi.y) * 0.5f, hi.z};
        const float w = vp[3] * top.x + vp[7] * top.y + vp[11] * top.z + vp[15];
        if (!(w > 1e-6f))
            continue;
        const float x = (vp[0] * top.x + vp[4] * top.y + vp[8] * top.z + vp[12]) / w;
        const float y = (vp[1] * top.x + vp[5] * top.y + vp[9] * top.z + vp[13]) / w;
        const ImVec2 at(view_pos_.x + (x * 0.5f + 0.5f) * view_size_.x, view_pos_.y + (0.5f - y * 0.5f) * view_size_.y);
        const ImVec2 size = ImGui::CalcTextSize(objects_[i].name.c_str());
        const ImVec2 box(at.x - size.x * 0.5f - 6.0f * scale_, at.y - size.y - 14.0f * scale_);
        dl->AddRectFilled(box, ImVec2(box.x + size.x + 12.0f * scale_, box.y + size.y + 6.0f * scale_),
                          ImGui::GetColorU32(rgb(theme_.panel, 0.85f)), 5.0f * scale_);
        dl->AddText(ImVec2(box.x + 6.0f * scale_, box.y + 3.0f * scale_), ImGui::GetColorU32(rgb(theme_.text)),
                    objects_[i].name.c_str());
    }
}

void App::preview_panels() {
    if (!result_)
        return;
    const GcodePreview& p = result_->preview;
    const int layers = int(p.layers.size());
    if (layers == 0)
        return;
    top_layer_ = std::clamp(top_layer_, 0, layers - 1);
    bottom_layer_ = std::clamp(bottom_layer_, 0, top_layer_);

    // Arrow keys move through the layers when no text field has the keyboard.
    if (!ImGui::GetIO().WantTextInput) {
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) || ImGui::IsKeyPressed(ImGuiKey_PageUp))
            top_layer_ = std::min(top_layer_ + 1, layers - 1);
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) || ImGui::IsKeyPressed(ImGuiKey_PageDown))
            top_layer_ = std::max(top_layer_ - 1, 0);
        if (ImGui::IsKeyPressed(ImGuiKey_Home))
            top_layer_ = 0;
        if (ImGui::IsKeyPressed(ImGuiKey_End))
            top_layer_ = layers - 1;
    }

    // The layer slider, on the right (rules 4, 9 and 12): the top and the
    // bottom layer drawn, each with its height and number; a «+» by the top
    // one adds a colour change there (a right click: a pause or G-code); the
    // changes by height are marks on it.
    const float margin = 14.0f * scale_;
    const float slider_top = 64.0f * scale_;
    const ImGuiStyle& style = ImGui::GetStyle();
    const float line_h = ImGui::GetTextLineHeightWithSpacing();
    const float slider_h = std::max(view_size_.y - slider_top - margin - 4.0f * line_h - 2.0f * style.WindowPadding.y, 80.0f * scale_);
    ImGui::SetNextWindowPos(ImVec2(view_pos_.x + view_size_.x - margin, view_pos_.y + slider_top), ImGuiCond_Always,
                            ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.85f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f * scale_);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, rgb(theme_.panel));
    ImGui::Begin("##layers", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    const auto layer_text = [&](int l) {
        return format("%.2f (%d)", p.layers[std::size_t(l)].z, l + 1);
    };
    const float text_w = ImGui::CalcTextSize(layer_text(layers - 1).c_str()).x + ImGui::GetFrameHeight();
    {
        const std::string top_text = layer_text(top_layer_);
        ImGui::TextUnformatted(top_text.c_str());
        // The «+» (rule 12), after the widest label; SameLine counts from
        // the window's edge, not from its padding (the spec team's UI2).
        ImGui::SameLine(style.WindowPadding.x + text_w - ImGui::GetFrameHeight() + style.ItemSpacing.x);
        if (!viewer_ && !result_->resin) {
            if (ImGui::SmallButton("+"))
                add_change_at(top_layer_, ChangeKind::Color);
            if (ImGui::IsItemHovered())
                loc::SetTooltip("%s", tr(T::AddAtLayer));
            if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
                ImGui::OpenPopup("##add_change");
            if (ImGui::BeginPopup("##add_change")) {
                if (ImGui::MenuItem(tr(T::AddColorChange)))
                    add_change_at(top_layer_, ChangeKind::Color);
                if (ImGui::MenuItem(tr(T::AddPause)))
                    change_layer_ = top_layer_, change_kind_ = ChangeKind::Pause, change_text_.clear(), change_dialog_ = true;
                if (ImGui::MenuItem(tr(T::AddCustomGcode)))
                    change_layer_ = top_layer_, change_kind_ = ChangeKind::Gcode, change_text_.clear(), change_dialog_ = true;
                ImGui::EndPopup();
            }
        } else {
            ImGui::Dummy(ImVec2(ImGui::GetFrameHeight(), 1));
        }
    }
    const int shown_lo = only_layer_ ? top_layer_ : bottom_layer_;
    {
        // The track and its two handles.
        const float track_w = text_w;
        const ImVec2 at = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##layer_track", ImVec2(track_w, slider_h));
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float cx = at.x + track_w * 0.5f, grab = 9.0f * scale_;
        const float y0 = at.y + grab, y1 = at.y + slider_h - grab;
        const auto y_of = [&](int l) { return layers > 1 ? y1 - (y1 - y0) * float(l) / float(layers - 1) : y1; };
        const auto layer_at = [&](float y) {
            return layers > 1 ? std::clamp(int(std::lround((y1 - y) / (y1 - y0) * float(layers - 1))), 0, layers - 1) : 0;
        };
        if (ImGui::IsItemActivated()) {
            const float my = ImGui::GetIO().MousePos.y;
            dragging_bottom_ = !only_layer_ && std::abs(my - y_of(bottom_layer_)) < std::abs(my - y_of(top_layer_));
        }
        if (ImGui::IsItemActive()) {
            const int l = layer_at(ImGui::GetIO().MousePos.y);
            if (dragging_bottom_)
                bottom_layer_ = std::min(l, top_layer_);
            else
                top_layer_ = std::max(l, only_layer_ ? 0 : bottom_layer_);
        }
        if (ImGui::IsItemHovered() && ImGui::GetIO().MouseWheel != 0.0f)
            top_layer_ = std::clamp(top_layer_ + int(ImGui::GetIO().MouseWheel), bottom_layer_, layers - 1);
        const ImU32 track = ImGui::GetColorU32(rgb(theme_.frame)), fill = ImGui::GetColorU32(rgb(theme_.accent, 0.8f));
        dl->AddRectFilled(ImVec2(cx - 3.0f * scale_, y0), ImVec2(cx + 3.0f * scale_, y1), track, 3.0f * scale_);
        dl->AddRectFilled(ImVec2(cx - 3.0f * scale_, y_of(top_layer_)), ImVec2(cx + 3.0f * scale_, y_of(shown_lo)), fill,
                          3.0f * scale_);
        // The changes by height, as marks: what each is when the mouse is on it,
        // and a click to remove it.
        for (std::size_t c = 0; c < settings_.changes.size(); ++c) {
            const HeightChange& h = settings_.changes[c];
            if (!h.color_change && !h.pause && h.gcode.empty())
                continue;
            int l = 0;
            while (l + 1 < layers && p.layers[std::size_t(l)].z < h.z)
                ++l;
            const float y = y_of(l);
            const std::uint32_t colour = h.color_change ? kMarkColours[std::size_t(MarkKind::ColorChange)]
                                         : h.pause      ? kMarkColours[std::size_t(MarkKind::Pause)]
                                                        : kMarkColours[std::size_t(MarkKind::Custom)];
            const ImVec2 m0(cx + 6.0f * scale_, y - 3.0f * scale_), m1(cx + 14.0f * scale_, y + 3.0f * scale_);
            dl->AddRectFilled(m0, m1, ImGui::GetColorU32(rgb(colour)), 2.0f * scale_);
            if (ImGui::IsMouseHoveringRect(m0, m1)) {
                loc::SetTooltip("%s", (h.color_change ? format(tr(T::ColorChangeAt), l + 1)
                                         : h.pause      ? format(tr(T::PauseAt), l + 1) + (h.gcode.empty() ? "" : "\n" + h.gcode)
                                                        : format(tr(T::GcodeAt), l + 1) + "\n" + h.gcode)
                                            .c_str());
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                    change_edit_ = int(c), ImGui::OpenPopup("##edit_change");
            }
        }
        if (ImGui::BeginPopup("##edit_change")) {
            if (change_edit_ >= 0 && std::size_t(change_edit_) < settings_.changes.size()) {
                HeightChange& h = settings_.changes[std::size_t(change_edit_)];
                if (h.pause || !h.gcode.empty()) {
                    ImGui::SetNextItemWidth(260.0f * scale_);
                    if (ImGui::InputTextMultiline("##change_text", &h.gcode, ImVec2(260.0f * scale_, 3.0f * line_h)))
                        settings_changed();
                }
                if (ImGui::Button(tr(T::RemoveChange))) {
                    settings_.changes.erase(settings_.changes.begin() + change_edit_);
                    change_edit_ = -1;
                    settings_changed();
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndPopup();
        }
        for (const int l : {shown_lo, top_layer_}) {
            const float y = y_of(l);
            dl->AddCircleFilled(ImVec2(cx, y), grab, ImGui::GetColorU32(rgb(theme_.accent_hover)));
            dl->AddCircle(ImVec2(cx, y), grab, ImGui::GetColorU32(rgb(0xFFFFFF)), 20, 1.5f * scale_);
        }
    }
    loc::TextDisabled("%s", layer_text(shown_lo).c_str());
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    if (only_layer_)
        bottom_layer_ = top_layer_;
    // The move slider (rule 4), at the bottom: the last move drawn in the
    // top layer, with its number; left and right step it.
    {
        const PreviewLayer& top = p.layers[std::size_t(top_layer_)];
        if (move_cut_layer_ != top_layer_)
            move_cut_ = int(top.count), move_cut_layer_ = top_layer_;
        move_cut_ = std::clamp(move_cut_, 1, std::max(int(top.count), 1));
        if (top.count > 1) {
            const float w = std::min(420.0f * scale_, view_size_.x * 0.3f);
            // Right of the legend, left of the layer slider.
            ImGui::SetNextWindowPos(ImVec2(view_pos_.x + view_size_.x - margin - 150.0f * scale_,
                                           view_pos_.y + view_size_.y - margin),
                                    ImGuiCond_Always, ImVec2(1.0f, 1.0f));
            ImGui::SetNextWindowBgAlpha(0.85f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f * scale_);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, rgb(theme_.panel));
            ImGui::Begin("##moves", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
            ImGui::SetNextItemWidth(w);
            ImGui::SliderInt("##move_cut", &move_cut_, 1, int(top.count), "");
            ImGui::SameLine();
            loc::TextDisabled(tr(T::MoveN), move_cut_, int(top.count));
            ImGui::End();
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
            if (!ImGui::GetIO().WantTextInput) {
                if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
                    move_cut_ = std::max(move_cut_ - 1, 1);
                if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
                    move_cut_ = std::min(move_cut_ + 1, int(top.count));
            }
        }
    }
    preview_overlay(p);
    if (!show_legend_)
        return;

    // The legend and the numbers, bottom left.
    ImGui::SetNextWindowPos(ImVec2(view_pos_.x + margin, view_pos_.y + view_size_.y - margin), ImGuiCond_Always,
                            ImVec2(0.0f, 1.0f));
    ImGui::SetNextWindowBgAlpha(0.88f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f * scale_);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, rgb(theme_.panel));
    ImGui::Begin("##legend", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    const PreviewLayer& layer = p.layers[std::size_t(top_layer_)];
    loc::Text(tr(T::LayerOf), top_layer_ + 1, layers, layer.z);
    if (calibration_ && !viewer_)
        for (const CalibrationSegment& seg : calibration_->segments)
            if (layer.z > seg.z0 + 1e-6 && layer.z <= seg.z1 + 1e-6) {
                const std::string value = format("%g %s", seg.value, calibration_->unit == "C" ? "°C" : calibration_->unit.c_str());
                loc::TextColored(rgb(theme_.accent_hover), tr(T::SegmentAt), value.c_str());
            }
    ImGui::Checkbox(tr(T::OnlyThisLayer), &only_layer_);
    ImGui::Separator();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(tr(T::ColorBy));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::CalcTextSize(tr(T::ByLayerTimeLog)).x + ImGui::GetFrameHeight() * 2.0f);
    if (ImGui::BeginCombo("##colour_mode", tr(kViewInfo[int(preview_view_)].name), ImGuiComboFlags_HeightLargest)) {
        for (int v = 0; v < kPreviewViews; ++v)
            if (ImGui::Selectable(tr(kViewInfo[v].name), v == int(preview_view_)) && v != int(preview_view_)) {
                preview_view_ = static_cast<PreviewView>(v);
                color_mode_ = kViewInfo[v].shader;
                upload_preview(); // the instances carry the view's number
            }
        ImGui::EndCombo();
    }
    const float swatch = ImGui::GetTextLineHeight();
    if (preview_view_ == PreviewView::Kind && !p.typed) {
        // A G-code without the kinds' comments (rule 13, PV10).
        loc::TextDisabled("%s", tr(T::NoTypes));
    } else if (preview_view_ == PreviewView::Kind) {
        // Each kind: its colour, its time and share of the moves' time, and
        // its filament (rule 2).
        const double all = p.moves_seconds;
        const double area = 3.14159265358979 * 0.25 * result_->settings.filament_diameter * result_->settings.filament_diameter;
        const double density = result_->settings.filament_density > 0.0 ? result_->settings.filament_density : 1.24;
        if (ImGui::BeginTable("##kinds", 4, ImGuiTableFlags_SizingFixedFit)) {
            for (std::size_t k = 1; k < kLineKinds; ++k) {
                if (p.kind_moves[k] == 0)
                    continue;
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                bool visible = (kind_mask_ >> k) & 1u;
                ImGui::PushID(int(k));
                if (ImGui::Checkbox("##visible", &visible))
                    kind_mask_ = visible ? kind_mask_ | (1u << k) : kind_mask_ & ~(1u << k);
                ImGui::SameLine();
                const ImVec2 at = ImGui::GetCursorScreenPos();
                const float y = at.y + (ImGui::GetFrameHeight() - swatch) * 0.5f;
                ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(at.x, y), ImVec2(at.x + swatch, y + swatch),
                                                          ImGui::GetColorU32(rgb(kind_colour(LineKind(k)))), 3.0f * scale_);
                ImGui::Dummy(ImVec2(swatch, swatch));
                ImGui::SameLine();
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(tr(kind_text(LineKind(k))));
                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                if (all > 0.0)
                    loc::TextDisabled("%s", format_duration(p.kind_seconds[k]).c_str());
                ImGui::TableNextColumn();
                if (all > 0.0) {
                    const double share = p.kind_seconds[k] / all;
                    const ImVec2 b = ImGui::GetCursorScreenPos();
                    const float bar = 40.0f * scale_, by = b.y + ImGui::GetFrameHeight() * 0.5f - 2.0f * scale_;
                    ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(b.x, by), ImVec2(b.x + bar, by + 4.0f * scale_),
                                                              ImGui::GetColorU32(rgb(theme_.frame)));
                    ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(b.x, by), ImVec2(b.x + bar * float(share), by + 4.0f * scale_),
                                                              ImGui::GetColorU32(rgb(kind_colour(LineKind(k)))));
                    ImGui::Dummy(ImVec2(bar, 1));
                    ImGui::SameLine();
                    ImGui::AlignTextToFramePadding();
                    loc::TextDisabled("%.1f %%", 100.0 * share);
                }
                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                if (p.kind_filament[k] > 0.0)
                    loc::TextDisabled("%.2f m · %.2f g", p.kind_filament[k] / 1000.0,
                                        p.kind_filament[k] * area / 1000.0 * density);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    } else {
        // Eleven steps from the least to the most (rule 7), or each tool or
        // colour.
        const PreviewView v = preview_view_;
        const float* range = v == PreviewView::Height ? height_range_ : v == PreviewView::Width ? width_range_ : feed_range_;
        const bool log = v == PreviewView::LayerTimeLog;
        const bool discrete = v == PreviewView::Tool || v == PreviewView::ColorPrint;
        std::vector<double> steps;
        if (discrete)
            for (int i = int(std::lround(range[0])); i <= int(std::lround(range[1])); ++i)
                steps.push_back(i);
        else
            steps = legend_steps(range[0], range[1], log);
        const auto text_of = [&](double value) -> std::string {
            switch (v) {
            case PreviewView::Height:
            case PreviewView::Width: return format("%.3f mm", value);
            case PreviewView::Speed:
            case PreviewView::ActualSpeed: return format("%.1f mm/s", value);
            case PreviewView::Fan: return format("%.0f %%", value);
            case PreviewView::Temperature: return format("%.0f °C", value);
            case PreviewView::Flow:
            case PreviewView::ActualFlow: return format("%.2f mm³/s", value);
            case PreviewView::LayerTime:
            case PreviewView::LayerTimeLog: return format_duration(value);
            case PreviewView::Tool: return format(tr(T::ToolN), int(value) + 1);
            case PreviewView::ColorPrint: return format(tr(T::ColorN), int(value) + 1);
            case PreviewView::Kind: break;
            }
            return {};
        };
        // The shader's ramp, from blue to red.
        const auto ramp = [](double s) {
            static const float stops[5][3] = {
                {0.17f, 0.33f, 0.95f}, {0.10f, 0.80f, 0.85f}, {0.30f, 0.85f, 0.35f}, {0.98f, 0.85f, 0.20f}, {0.92f, 0.22f, 0.20f}};
            const float u = float(std::clamp(s, 0.0, 1.0)) * 4.0f;
            const int i = std::min(int(u), 3);
            const float f = u - float(i);
            return ImGui::GetColorU32(ImVec4(stops[i][0] + (stops[i + 1][0] - stops[i][0]) * f,
                                             stops[i][1] + (stops[i + 1][1] - stops[i][1]) * f,
                                             stops[i][2] + (stops[i + 1][2] - stops[i][2]) * f, 1.0f));
        };
        // Largest first, as a scale reads from top to bottom.
        for (auto it = steps.rbegin(); it != steps.rend(); ++it) {
            const double step = *it;
            const double value = log ? std::log10(std::max(step, 0.01)) : step;
            const double s = range[1] > range[0] ? (value - range[0]) / (range[1] - range[0]) : 0.0;
            const ImVec2 at = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(at, ImVec2(at.x + swatch, at.y + swatch), ramp(s), 3.0f * scale_);
            ImGui::Dummy(ImVec2(swatch, swatch));
            ImGui::SameLine();
            ImGui::TextUnformatted(text_of(step).c_str());
        }
    }
    // The icon row (rule 11): each shows or hides something over the paths.
    {
        const float size = ImGui::GetFrameHeight();
        const ImU32 off = ImGui::GetColorU32(rgb(theme_.frame)), on = ImGui::GetColorU32(rgb(theme_.accent, 0.45f));
        int n = 0;
        const auto icon_toggle = [&](const char* id, bool& value, T tip, const std::function<void(ImDrawList*, ImVec2, float)>& draw) {
            if (n++ % 6 != 0)
                ImGui::SameLine(0, 4.0f * scale_);
            const ImVec2 at = ImGui::GetCursorScreenPos();
            if (ImGui::InvisibleButton(id, ImVec2(size, size)))
                value = !value;
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(at, ImVec2(at.x + size, at.y + size), value ? on : off, 5.0f * scale_);
            draw(dl, at, size);
            if (ImGui::IsItemHovered())
                loc::SetTooltip("%s", tr(tip));
        };
        icon_toggle("##travels", show_travels_, T::IconTravels, [&](ImDrawList* dl, ImVec2 a, float s) {
            dl->AddLine(ImVec2(a.x + s * 0.2f, a.y + s * 0.7f), ImVec2(a.x + s * 0.8f, a.y + s * 0.3f),
                        ImGui::GetColorU32(rgb(kind_colour(LineKind::Travel))), 2.0f * scale_);
        });
        icon_toggle("##wipes", show_wipes_, T::IconWipes, [&](ImDrawList* dl, ImVec2 a, float s) {
            const ImVec2 pts[4] = {ImVec2(a.x + s * 0.2f, a.y + s * 0.6f), ImVec2(a.x + s * 0.4f, a.y + s * 0.35f),
                                   ImVec2(a.x + s * 0.6f, a.y + s * 0.65f), ImVec2(a.x + s * 0.8f, a.y + s * 0.4f)};
            dl->AddPolyline(pts, 4, ImGui::GetColorU32(rgb(kWipeColour)), 2.0f * scale_, ImDrawFlags_None);
        });
        static constexpr T mark_tips[kMarkKinds] = {T::IconRetracts, T::IconUnretracts, T::IconSeams, T::IconToolChanges,
                                                    T::IconColorChanges, T::IconPauses, T::IconCustom};
        for (std::size_t k = 0; k < kMarkKinds; ++k) {
            const std::string id = "##mark" + std::to_string(k);
            icon_toggle(id.c_str(), show_marks_[k], mark_tips[k], [&](ImDrawList* dl, ImVec2 a, float s) {
                dl->AddCircleFilled(ImVec2(a.x + s * 0.5f, a.y + s * 0.5f), s * 0.18f, ImGui::GetColorU32(rgb(kMarkColours[k])));
            });
        }
        icon_toggle("##gravity", show_gravity_, T::IconGravity, [&](ImDrawList* dl, ImVec2 a, float s) {
            const ImU32 c = ImGui::GetColorU32(rgb(theme_.text));
            dl->AddCircle(ImVec2(a.x + s * 0.5f, a.y + s * 0.5f), s * 0.22f, c, 16, 1.5f);
            dl->AddLine(ImVec2(a.x + s * 0.2f, a.y + s * 0.5f), ImVec2(a.x + s * 0.8f, a.y + s * 0.5f), c, 1.2f);
            dl->AddLine(ImVec2(a.x + s * 0.5f, a.y + s * 0.2f), ImVec2(a.x + s * 0.5f, a.y + s * 0.8f), c, 1.2f);
        });
        icon_toggle("##nozzle", show_nozzle_, T::IconNozzle, [&](ImDrawList* dl, ImVec2 a, float s) {
            const ImU32 c = ImGui::GetColorU32(rgb(theme_.accent_hover));
            dl->AddRectFilled(ImVec2(a.x + s * 0.32f, a.y + s * 0.18f), ImVec2(a.x + s * 0.68f, a.y + s * 0.42f), c);
            dl->AddTriangleFilled(ImVec2(a.x + s * 0.3f, a.y + s * 0.42f), ImVec2(a.x + s * 0.7f, a.y + s * 0.42f),
                                  ImVec2(a.x + s * 0.5f, a.y + s * 0.8f), c);
        });
    }
    ImGui::Separator();
    if (ImGui::BeginTable("##numbers", 2)) {
        auto line = [](const char* name, const std::string& value) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            loc::TextDisabled("%s", name);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(value.c_str());
        };
        if (p.estimated_seconds >= 0.0)
            line(tr(T::EstimatedTime), format_duration(p.estimated_seconds));
        if (p.first_layer_seconds > 0.0)
            line(tr(T::FirstLayerTime), format_duration(p.first_layer_seconds));
        if (result_->resin) {
            // A resin print: the resin it takes, by its bottle (gcode_extras.md, rule 2).
            line(tr(T::ResinUsed), format("%.1f ml  ·  %.1f g", result_->resin->volume / 1000.0, result_->resin->weight));
            if (result_->resin->cost > 0.0)
                line(tr(T::ResinCost), format("%.2f", result_->resin->cost));
            line(tr(T::Layers), std::to_string(layers));
        } else {
            const double area = 3.14159265358979 * 0.25 * result_->settings.filament_diameter *
                                result_->settings.filament_diameter;
            line(tr(T::FilamentUsed),
                 format("%.2f m  ·  %.1f cm³", p.filament / 1000.0, p.filament * area / 1000.0));
            line(tr(T::Layers), std::to_string(layers));
            line(tr(T::Lines), number_text(p.lines));
        }
        ImGui::EndTable();
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void App::analysis_panel() {
    const float margin = 12.0f * scale_;
    ImGui::SetNextWindowPos(ImVec2(view_pos_.x + margin, view_pos_.y + 60.0f * scale_));
    ImGui::SetNextWindowSizeConstraints(ImVec2(360.0f * scale_, 0), ImVec2(460.0f * scale_, FLT_MAX));
    ImGui::SetNextWindowBgAlpha(0.92f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f * scale_);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f * scale_, 14.0f * scale_));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, rgb(theme_.panel));
    ImGui::Begin("##analysis", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    ImGui::PushFont(nullptr, kTitleSize * 0.9f);
    ImGui::TextUnformatted(tr(T::AnalysisTitle));
    ImGui::PopFont();
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 420.0f * scale_);
    loc::TextDisabled("%s", tr(T::AnalysisHint));
    ImGui::Separator();
    if (!mesh_) {
        ImGui::TextUnformatted(tr(T::AnalysisNeedsModel));
    } else if (voxel_job_.running() || !voxels_) {
        loc::TextColored(rgb(theme_.accent_hover), "%s", tr(T::Estimating));
    } else if (!voxels_->ok) {
        loc::TextColored(rgb(0xE5484D), "%s", voxels_->error.c_str());
    } else {
        const auto card = [&](const char* id, T title, const VoxelCount& c) {
            ImGui::PushID(id);
            loc::TextColored(rgb(theme_.accent_hover), "%s", tr(title));
            if (!c.ok) {
                loc::TextColored(rgb(0xE5484D), "%s", c.error.c_str());
                ImGui::PopID();
                return;
            }
            const bool counted = c.printed_voxels > 0;
            ImGui::PushFont(nullptr, kTitleSize * 1.25f);
            ImGui::TextUnformatted(number_text(counted ? c.printed_voxels : c.grid_voxels).c_str());
            ImGui::PopFont();
            loc::TextDisabled("%s", tr(counted ? T::Voxels : T::GridVoxels));
            if (counted && ImGui::BeginTable("##card", 2)) {
                auto line = [](const char* name, const std::string& value) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    loc::TextDisabled("%s", name);
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(value.c_str());
                };
                line(tr(T::GridVoxels), number_text(c.grid_voxels));
                line(tr(T::VoxelSize), format("%.3f mm  ·  %s", c.voxel, format(tr(T::PerLayer), c.rows_per_layer).c_str()));
                line(tr(T::Memory), format("%.0f MB", c.memory_mb));
                line(tr(T::ApproxTime), format_duration(c.seconds));
                ImGui::EndTable();
            }
            if (c.over_limit)
                loc::TextColored(rgb(0xE8A33D), "%s", format(tr(T::OverLimit), number_text(c.limit).c_str()).c_str());
            ImGui::PopID();
        };
        card("normal", T::QualityNormal, voxels_->normal);
        ImGui::Separator();
        card("high", T::QualityHigh, voxels_->high);
    }
    ImGui::Separator();
    loc::TextDisabled("%s", tr(T::AnalysisCli));
    ImGui::TextDisabled("tisma-engine analyze");
    ImGui::PopTextWrapPos();
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

// The guided calibration: create (the part is built with ranges from the
// settings and sliced), print, then pick the segment that came out best or
// enter the measures.
void App::calibration_panel() {
    const float margin = 12.0f * scale_;
    ImGui::SetNextWindowPos(ImVec2(view_pos_.x + margin, view_pos_.y + 60.0f * scale_));
    ImGui::SetNextWindowSize(ImVec2(450.0f * scale_, view_size_.y - 72.0f * scale_));
    ImGui::SetNextWindowBgAlpha(0.95f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f * scale_);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f * scale_, 14.0f * scale_));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, rgb(theme_.panel));
    ImGui::Begin("##calibration", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    ImGui::BeginChild("##calibration_scroll", ImVec2(0, 0));
    ImGui::PushFont(nullptr, kTitleSize * 0.9f);
    ImGui::TextUnformatted(tr(T::CalibrationTitle));
    ImGui::PopFont();
    ImGui::PushTextWrapPos(0.0f);
    loc::TextDisabled("%s", tr(T::CalibrationHint));
    const float field = 110.0f * scale_;
    const auto number = [&](T label, double& v, double lo, double hi, const char* fmt, float speed) {
        ImGui::SetNextItemWidth(field);
        ImGui::PushID(&v);
        ImGui::DragScalar(tr(label), ImGuiDataType_Double, &v, speed, &lo, &hi, field_text(tr(label), fmt, v), ImGuiSliderFlags_AlwaysClamp);
        ImGui::PopID();
    };
    const auto unit_text = [](const CalibrationPrint& c, double v) {
        char text[32];
        if (c.unit.empty())
            std::snprintf(text, sizeof text, "%s", format("%g", v).c_str());
        else
            std::snprintf(text, sizeof text, "%s", format("%g %s", v, c.unit == "C" ? "°C" : c.unit.c_str()).c_str());
        return std::string(text);
    };

    struct Card {
        CalibrationKind kind;
        T title, what, hint;
    };
    const Card cards[] = {
        {CalibrationKind::Dimensions, T::CalDimensions, T::CalDimensionsWhat, T::CalDimensionsHint},
        {CalibrationKind::ElephantFoot, T::CalElephantFoot, T::CalElephantFootWhat, T::CalElephantFootHint},
        {CalibrationKind::FirstLayer, T::CalFirstLayer, T::CalFirstLayerWhat, T::CalFirstLayerHint},
        {CalibrationKind::Temperature, T::CalTemperature, T::CalTemperatureWhat, T::CalTemperatureHint},
        {CalibrationKind::Flow, T::CalFlow, T::CalFlowWhat, T::CalFlowHint},
        {CalibrationKind::Retraction, T::CalRetraction, T::CalRetractionWhat, T::CalRetractionHint},
        {CalibrationKind::RetractionSpeed, T::CalRetractionSpeed, T::CalRetractionSpeedWhat, T::CalRetractionSpeedHint},
        {CalibrationKind::PressureAdvance, T::CalPressureAdvance, T::CalPressureAdvanceWhat, T::CalPressureAdvanceHint},
        {CalibrationKind::Speed, T::CalSpeed, T::CalSpeedWhat, T::CalSpeedHint},
        {CalibrationKind::MaxFlow, T::CalMaxFlow, T::CalMaxFlowWhat, T::CalMaxFlowHint},
        {CalibrationKind::Acceleration, T::CalAcceleration, T::CalAccelerationWhat, T::CalAccelerationHint},
        {CalibrationKind::Fit, T::CalFit, T::CalFitWhat, T::CalFitHint},
        {CalibrationKind::ResinExposure, T::CalResinExposure, T::CalResinExposureWhat, T::CalResinExposureHint},
        {CalibrationKind::ResinMeasure, T::CalResinMeasure, T::CalResinMeasureWhat, T::CalResinMeasureHint},
    };
    const float button_h = 30.0f * scale_;
    // A card's title and what it is for, with its button on the right.
    const auto card_head = [&](T title, T what, bool active, const char* label, bool enabled) {
        const float create_w = ImGui::CalcTextSize(label).x + button_h * 1.6f;
        const float row_right = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
        ImGui::BeginGroup();
        ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x - create_w - 8.0f * scale_);
        loc::TextColored(rgb(active ? theme_.accent_hover : theme_.text), "%s", tr(title));
        loc::TextDisabled("%s", tr(what));
        ImGui::PopTextWrapPos();
        ImGui::EndGroup();
        ImGui::SameLine(row_right - create_w);
        return button("##create", "tisma_calibrate", label, !active, false, enabled, nullptr, button_h);
    };
    // Segment chips, bottom to top as printed (left to right for the fit test).
    const auto chips = [&](const CalibrationPrint& c) {
        const float chip_w = 86.0f * scale_;
        const int per_row = std::max(1, int(ImGui::GetContentRegionAvail().x / (chip_w + 6.0f * scale_)));
        for (std::size_t k = 0; k < c.segments.size(); ++k) {
            const double v = c.segments[k].value;
            const bool chosen = chosen_value_ && std::abs(*chosen_value_ - v) < 1e-9;
            if (k % std::size_t(per_row) != 0)
                ImGui::SameLine(0, 6.0f * scale_);
            ImGui::PushID(int(k));
            ImGui::PushStyleColor(ImGuiCol_Button, chosen ? rgb(theme_.accent) : rgb(theme_.frame));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, chosen ? rgb(theme_.accent_hover) : rgb(theme_.frame_hover));
            ImGui::PushStyleColor(ImGuiCol_Text, chosen ? ImVec4(1, 1, 1, 1) : rgb(theme_.text));
            if (ImGui::Button(unit_text(c, v).c_str(), ImVec2(chip_w, button_h)))
                chosen_value_ = v;
            ImGui::PopStyleColor(3);
            ImGui::PopID();
        }
    };
    for (const Card& card : cards) {
        // With a resin printer, only the resin tests (interface.md); else the others.
        if (is_resin_calibration(card.kind) != (settings_.machine_technology == MachineTechnology::Msla))
            continue;
        ImGui::PushID(int(card.kind));
        ImGui::Separator();
        const bool active = calibration_ && calibration_kind_ == card.kind;
        if (card_head(card.title, card.what, active, tr(T::Create), !slice_job_.running()))
            create_calibration(card.kind, false);
        // Its guide, when the pack has one (assistant.md, rule 20).
        if (const std::string_view guide = guide_for_test(calibration_kind_name(card.kind));
            !guide.empty() && knowledge_ && find_guide(*knowledge_, guide) && ImGui::SmallButton(tr(T::GuideHow)))
            open_guide(guide);

        if (active) {
            const CalibrationPrint& c = *calibration_;
            // 1: created and sliced.
            loc::TextColored(rgb(theme_.accent_hover), "%s", tr(T::StepCreated));
            ImGui::SameLine();
            if (slice_job_.running())
                loc::TextDisabled("%s", tr(T::Slicing));
            else if (result_ && !stale_ && !viewer_) {
                const std::string time = result_->preview.estimated_seconds >= 0.0
                                             ? format_duration(result_->preview.estimated_seconds)
                                             : std::string("-");
                if (is_tower(card.kind))
                    loc::TextDisabled(tr(T::TowerReady), int(c.segments.size()), time.c_str());
                else
                    loc::TextDisabled(tr(T::PartReady), time.c_str());
            }
            const bool can_export = result_ && result_->ok && !viewer_ && !stale_ && result_->check.ok();
            if (button("##export_calibration", "tisma_save", tr(result_ && result_->resin ? T::ExportGoo : T::Export), false,
                       false, can_export, tr(T::ExportTip),
                       button_h))
                begin_export();
            // 2: print and look.
            loc::TextColored(rgb(theme_.accent_hover), "%s", tr(T::StepPrint));
            loc::TextDisabled("%s", tr(card.hint));
            if (card.kind == CalibrationKind::Dimensions) {
                // 3: measure.
                loc::TextColored(rgb(theme_.accent_hover), "%s", tr(T::StepMeasure));
                number(T::MeasuredX, measured_.x, 1.0, 1000.0, "%.3f mm", 0.005f);
                number(T::MeasuredY, measured_.y, 1.0, 1000.0, "%.3f mm", 0.005f);
                number(T::MeasuredZ, measured_.z, 1.0, 1000.0, "%.3f mm", 0.005f);
                number(T::MeasuredHole, measured_.hole, 0.5, 1000.0, "%.3f mm", 0.005f);
                const Compensation next = compensation_from(dimension_test_, measured_, settings_);
                if (ImGui::BeginTable("##compensation", 3, ImGuiTableFlags_SizingStretchProp)) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TableNextColumn();
                    loc::TextDisabled("%s", tr(T::Current));
                    ImGui::TableNextColumn();
                    loc::TextDisabled("%s", tr(T::New));
                    const auto line = [](const char* name, double now, double then, const char* fmt) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(name);
                        ImGui::TableNextColumn();
                        loc::Text(fmt, now);
                        ImGui::TableNextColumn();
                        loc::Text(fmt, then);
                    };
                    line("X", settings_.scale_x, next.scale_x, "%.3f %%");
                    line("Y", settings_.scale_y, next.scale_y, "%.3f %%");
                    line("Z", settings_.scale_z, next.scale_z, "%.3f %%");
                    line(tr(T::HoleCorrection), settings_.hole_offset, next.hole_offset, "%+.3f mm");
                    ImGui::EndTable();
                }
                if (button("##apply", nullptr, tr(T::ApplyCompensation), true, false, true, nullptr, button_h)) {
                    settings_.scale_x = next.scale_x, settings_.scale_y = next.scale_y;
                    settings_.scale_z = next.scale_z, settings_.hole_offset = next.hole_offset;
                    // The block printed with these values would now measure as designed.
                    measured_ = {dimension_test_.x, dimension_test_.y, dimension_test_.z, dimension_test_.hole};
                    record_calibration(CalibrationKind::Dimensions);
                    settings_changed();
                    notify(format(tr(T::CompensationApplied), next.scale_x, next.scale_y, next.scale_z,
                                  next.hole_offset));
                }
                ImGui::SameLine();
                if (button("##reset", nullptr, tr(T::ResetCompensation), false, false, true, nullptr, button_h)) {
                    settings_.scale_x = settings_.scale_y = settings_.scale_z = 100.0;
                    settings_.hole_offset = 0.0;
                    settings_changed();
                }
            } else if (card.kind == CalibrationKind::ElephantFoot) {
                // 3: measure the bottom against the body.
                loc::TextColored(rgb(theme_.accent_hover), "%s", tr(T::StepMeasure));
                number(T::MeasuredBottom, elephant_bottom_, 1.0, 1000.0, "%.3f mm", 0.005f);
                number(T::MeasuredBody, elephant_body_, 1.0, 1000.0, "%.3f mm", 0.005f);
                const double next = elephant_foot_from(elephant_bottom_, elephant_body_, settings_);
                loc::Text("%s: %.3f mm (%s) · %.3f mm (%s)", tr(T::FirstLayerShrink), settings_.first_layer_shrink,
                            tr(T::Current), next, tr(T::New));
                if (button("##apply_shrink", nullptr, tr(T::ApplyShrink), true, false, true, nullptr, button_h)) {
                    settings_.first_layer_shrink = next;
                    elephant_bottom_ = elephant_body_; // printed with this shrink, the bottom would match the body
                    record_calibration(CalibrationKind::ElephantFoot);
                    settings_changed();
                    notify(format(tr(T::ShrinkApplied), next));
                }
            } else if (card.kind == CalibrationKind::ResinMeasure) {
                // 3: measure the block; the scale corrections follow (resin.md, rule 19).
                loc::TextColored(rgb(theme_.accent_hover), "%s", tr(T::StepMeasure));
                number(T::MeasuredX, resin_measured_[0], 1.0, 1000.0, "%.3f mm", 0.005f);
                number(T::MeasuredY, resin_measured_[1], 1.0, 1000.0, "%.3f mm", 0.005f);
                number(T::MeasuredZ, resin_measured_[2], 1.0, 1000.0, "%.3f mm", 0.005f);
                const std::array<double, 3> next =
                    resin_scale_from(settings_, resin_measured_[0], resin_measured_[1], resin_measured_[2]);
                loc::Text("%s: %.4f · %.4f · %.4f", tr(T::New), next[0], next[1], next[2]);
                if (button("##apply_scale", nullptr, tr(T::ApplyCompensation), true, false, true, nullptr, button_h)) {
                    settings_.resin_scale_x = std::clamp(next[0], 0.5, 2.0);
                    settings_.resin_scale_y = std::clamp(next[1], 0.5, 2.0);
                    settings_.resin_scale_z = std::clamp(next[2], 0.5, 2.0);
                    resin_measured_[0] = resin_measured_[1] = 30.0;
                    resin_measured_[2] = 10.0;
                    record_calibration(CalibrationKind::ResinMeasure);
                    settings_changed();
                    notify(tr(T::ResinScaleApplied));
                }
            } else if (card.kind == CalibrationKind::FirstLayer) {
                // 3: what each look means; the height is set on the printer.
                loc::TextColored(rgb(theme_.accent_hover), "%s", tr(T::StepJudge));
                ImGui::TextUnformatted(tr(T::FirstLayerJudge));
            } else if (card.kind == CalibrationKind::Fit) {
                // 3: the least clearance that comes free, for the user's designs.
                loc::TextColored(rgb(theme_.accent_hover), "%s", tr(T::StepChooseFit));
                chips(c);
                if (chosen_value_)
                    loc::TextColored(rgb(theme_.accent_hover), tr(T::FitAdvice), unit_text(c, *chosen_value_).c_str());
            } else {
                // 3: pick the best segment and use its value.
                loc::TextColored(rgb(theme_.accent_hover), "%s", tr(T::StepChoose));
                chips(c);
                if (chosen_value_) {
                    const std::string value = unit_text(c, *chosen_value_);
                    if (card.kind == CalibrationKind::Speed)
                        loc::TextDisabled(tr(T::VolumetricFlow), value.c_str(),
                                            volumetric_flow(settings_, *chosen_value_));
                    if (button("##use", nullptr, format(tr(T::UseValue), value.c_str()).c_str(), true, false, true,
                               nullptr, button_h)) {
                        apply_calibration(card.kind, *chosen_value_, settings_);
                        record_calibration(card.kind);
                        settings_changed();
                        notify(format(tr(T::ValueApplied), value.c_str()));
                        calibration_.reset();
                        chosen_value_.reset();
                    }
                }
                // The range, for whoever wants to change it.
                if (calibration_ && ImGui::TreeNode(tr(T::AdjustRange))) {
                    TowerRange& r = ranges_[int(card.kind)];
                    double lo = 0.0, hi = 600.0, step_lo = 0.05;
                    float drag = 0.1f;
                    const char* fmt = "%.1f";
                    switch (card.kind) {
                    case CalibrationKind::Temperature: lo = 150.0, hi = 400.0, fmt = "%.0f"; break;
                    case CalibrationKind::Retraction: hi = 20.0, fmt = "%.2f", drag = 0.01f; break;
                    case CalibrationKind::RetractionSpeed: lo = 1.0, hi = 200.0, fmt = "%.0f"; break;
                    case CalibrationKind::Flow: lo = 10.0, hi = 300.0; break;
                    case CalibrationKind::Speed: lo = 5.0, hi = 1000.0, fmt = "%.0f"; break;
                    case CalibrationKind::PressureAdvance: hi = 2.0, fmt = "%.3f", step_lo = 0.001, drag = 0.001f; break;
                    case CalibrationKind::MaxFlow: lo = 0.5, hi = 1000.0, step_lo = 0.5; break;
                    case CalibrationKind::Acceleration: lo = 100.0, hi = 100000.0, fmt = "%.0f", step_lo = 100.0, drag = 10.0f; break;
                    default: break;
                    }
                    number(T::TowerStart, r.start, lo, hi, fmt, drag);
                    number(T::TowerEnd, r.end, lo, hi, fmt, drag);
                    number(T::TowerStep, r.step, step_lo, hi, fmt, drag);
                    number(T::TowerSegment, r.segment, 2.0, 30.0, "%.1f mm", 0.05f);
                    if (button("##recreate", nullptr, tr(T::Recreate), false, false, !slice_job_.running(), nullptr,
                               button_h))
                        create_calibration(card.kind, true);
                    ImGui::TreePop();
                }
            }
        }
        ImGui::PopID();
    }

    // The extruder's steps: no part to slice, a G-code to run and a sum (FDM only).
    if (settings_.machine_technology != MachineTechnology::Msla) {
        ImGui::PushID("extruder");
        ImGui::Separator();
        if (card_head(T::CalExtruder, T::CalExtruderWhat, extruder_open_, tr(T::Open), true)) {
            extruder_open_ = !extruder_open_;
            extruder_scroll_ = extruder_open_;
        }
        if (extruder_scroll_) { // the card opens at the bottom of the list: bring it up
            ImGui::SetScrollHereY(0.0f);
            extruder_scroll_ = false;
        }
        if (extruder_open_) {
            const bool klipper = settings_.flavor == GcodeFlavor::Klipper;
            loc::TextColored(rgb(theme_.accent_hover), "%s", tr(T::ExtruderStep1));
            if (button("##export_test", "tisma_save", tr(T::ExportTest), false, false, true, nullptr, button_h)) {
                purpose_ = Purpose::SaveExtruderTest;
                browser_.open(FileBrowser::Mode::Save, tr(T::ExportTest), {".gcode"}, last_folder_,
                              "tisma_extruder_steps.gcode");
            }
            loc::TextColored(rgb(theme_.accent_hover), "%s", tr(T::ExtruderStep2));
            number(klipper ? T::CurrentRotation : T::CurrentSteps, extruder_current_, 0.1, 10000.0, "%.3f", 0.01f);
            number(T::Remaining, extruder_left_, 0.0, 120.0, "%.1f mm", 0.05f);
            const double extruded = 120.0 - extruder_left_;
            const double value = extruder_steps_from(settings_.flavor, extruder_current_, 100.0, extruded);
            loc::TextColored(rgb(theme_.accent_hover), "%s", tr(T::ExtruderStep3));
            loc::Text(tr(T::ExtruderResult), extruded, value);
            std::string text = extruder_steps_text(settings_.flavor, value);
            const float two_lines =
                ImGui::GetTextLineHeight() * 2.0f + ImGui::GetStyle().FramePadding.y * 2.0f + 4.0f * scale_;
            ImGui::InputTextMultiline("##steps_text", &text, ImVec2(-FLT_MIN, two_lines), ImGuiInputTextFlags_ReadOnly);
            if (button("##copy", nullptr, tr(T::Copy), false, false, true, nullptr, button_h)) {
                ImGui::SetClipboardText(text.c_str());
                notify(tr(T::Copied));
            }
            loc::TextDisabled("%s", tr(T::ExtruderWhere));
        }
        ImGui::PopID();
    }
    ImGui::Separator();
    if (ImGui::CollapsingHeader(tr(T::MoreCalibrations)))
        loc::TextDisabled("%s", tr(T::MoreCalibrationsList));
    ImGui::PopTextWrapPos();
    ImGui::EndChild();
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

// Builds the calibration part (with the range of ranges_ when `custom`, else
// one chosen from the settings), puts it on the bed with its changes and
// slices it.
void App::create_calibration(CalibrationKind kind, bool custom) {
    const TowerRange& r = ranges_[int(kind)];
    CalibrationPrint print = custom && is_tower(kind) ? calibration_tower(kind, r.start, r.end, r.step, r.segment)
                                                      : automatic_calibration(kind, settings_);
    // The range shown for adjusting is the one used.
    if (is_tower(kind) && !print.segments.empty()) {
        TowerRange& used = ranges_[int(kind)];
        used.start = print.segments.front().value;
        used.end = print.segments.back().value;
        used.step = print.segments.size() > 1 ? std::abs(print.segments[1].value - print.segments[0].value) : used.step;
        used.segment = print.segments.front().z1 - print.segments.front().z0;
    }
    calibration_kind_ = kind;
    chosen_value_.reset();
    if (kind == CalibrationKind::Dimensions)
        measured_ = {dimension_test_.x, dimension_test_.y, dimension_test_.z, dimension_test_.hole};
    if (kind == CalibrationKind::ElephantFoot)
        elephant_bottom_ = elephant_body_ = 20.0;
    load_calibration(std::move(print));
    start_slice();
}

void App::toasts() {
    std::function<void()> undo; // pressed: run after the toasts are drawn
    const double now = ImGui::GetTime();
    toasts_.erase(std::remove_if(toasts_.begin(), toasts_.end(), [&](const Toast& t) { return t.until < now; }),
                  toasts_.end());
    float y = view_pos_.y + view_size_.y - 12.0f * scale_;
    int i = 0;
    for (auto it = toasts_.rbegin(); it != toasts_.rend(); ++it, ++i) {
        ImGui::SetNextWindowPos(ImVec2(view_pos_.x + view_size_.x - 60.0f * scale_, y), ImGuiCond_Always,
                                ImVec2(1.0f, 1.0f));
        ImGui::SetNextWindowBgAlpha(0.95f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f * scale_);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, rgb(it->error ? 0x7A1F2B : theme_.accent_active));
        const std::string id = "##toast" + std::to_string(i);
        ImGui::Begin(id.c_str(), nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                         ImGuiWindowFlags_NoNav | (it->undo ? 0 : ImGuiWindowFlags_NoInputs));
        ImGui::AlignTextToFramePadding();
        loc::TextColored(ImVec4(1, 1, 1, 1), "%s", it->text.c_str());
        if (it->undo) {
            ImGui::SameLine();
            if (ImGui::SmallButton(tr(T::Undo))) {
                undo = std::move(it->undo);
                it->until = 0.0;
            }
        }
        y -= ImGui::GetWindowHeight() + 8.0f * scale_;
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    }
    if (undo)
        undo();
}

void App::dialogs() {
    std::filesystem::path chosen;
    if (browser_.draw(chosen, icon("tisma_folder"), rgb(theme_.accent_hover))) {
        switch (purpose_) {
        case Purpose::Open: open_path(chosen); break;
        case Purpose::AddModel: add_model(chosen); break;
        case Purpose::SaveGcode: export_gcode_to(chosen); break;
        case Purpose::LoadSettings: load_settings_file(chosen); break;
        case Purpose::SaveSettings: save_settings_file(chosen); break;
        case Purpose::ExportView: {
            // The Custom view, or the level's as one (rule 17).
            const CustomView view = view_level_ == ViewLevel::Custom ? custom_view_ : custom_view_from(view_level_);
            std::ofstream out(chosen, std::ios::binary);
            out << custom_view_json(view);
            out.close();
            if (!out)
                fail(T::CannotSave, utf8(chosen));
            else
                notify(format(tr(T::ViewExported), utf8(chosen.filename()).c_str()));
            break;
        }
        case Purpose::ImportView: {
            std::ifstream in(chosen, std::ios::binary);
            std::ostringstream text;
            text << in.rdbuf();
            try {
                custom_view_ = parse_custom_view(text.str());
                choose_level(ViewLevel::Custom);
                notify(format(tr(T::ViewImported), int(custom_view_.visible.size())));
            } catch (const ViewError& e) {
                fail(T::CannotOpen, e.what());
            }
            break;
        }
        case Purpose::SaveProject: save_project_to(chosen); break;
        case Purpose::LoadCase: load_case_file(chosen); break;
        case Purpose::SaveExtruderTest: {
            std::ofstream out(chosen, std::ios::binary);
            out << extruder_test_gcode(settings_);
            out.close();
            if (!out)
                fail(T::CannotSave, utf8(chosen));
            else
                notify(format(tr(T::TestExported), utf8(chosen.filename()).c_str()));
            break;
        }
        }
    }
    if (!browser_.is_open() && !browser_.folder().empty())
        last_folder_ = browser_.folder();

    if (error_open_) {
        ImGui::OpenPopup("###error");
        error_open_ = false;
    }
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(420.0f * scale_, 0), ImVec2(vp->Size.x * 0.7f, vp->Size.y * 0.8f));
    const std::string title = error_title_ + "###error";
    if (ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 560.0f * scale_);
        ImGui::TextUnformatted(error_message_.c_str());
        ImGui::PopTextWrapPos();
        if (!error_details_.empty() && ImGui::CollapsingHeader(tr(T::Details))) {
            ImGui::BeginChild("##details", ImVec2(560.0f * scale_, 160.0f * scale_), ImGuiChildFlags_Borders);
            for (const std::string& d : error_details_)
                ImGui::TextUnformatted(d.c_str());
            ImGui::EndChild();
        }
        if (ImGui::Button(tr(T::Close), ImVec2(120.0f * scale_, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // Saving the settings as a new profile: its name.
    if (save_profile_open_) {
        ImGui::OpenPopup("###save_profile");
        save_profile_open_ = false;
    }
    if (save_profile_kind_) {
        const ProfileKind kind = *save_profile_kind_;
        const T titles[] = {T::SaveProfilePrinter, T::SaveProfileFilament, T::SaveProfilePrint};
        const std::string heading = std::string(tr(titles[std::size_t(kind)])) + "###save_profile";
        ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal(heading.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            if (ImGui::IsWindowAppearing())
                ImGui::SetKeyboardFocusHere();
            ImGui::SetNextItemWidth(360.0f * scale_);
            const bool enter = ImGui::InputTextWithHint("##name", tr(T::FileName), &profile_name_,
                                                        ImGuiInputTextFlags_EnterReturnsTrue);
            std::string name = profile_name_;
            while (!name.empty() && name.back() == ' ')
                name.pop_back();
            while (!name.empty() && name.front() == ' ')
                name.erase(name.begin());
            bool builtin = false;
            const bool exists = find_profile(kind, name, &builtin) != nullptr;
            if (exists && builtin)
                loc::TextColored(rgb(0xE5484D), "%s", tr(T::ProfileIsBuiltin));
            else if (exists)
                loc::TextDisabled("%s", tr(T::ProfileReplaces));
            const bool ok = !name.empty() && !(exists && builtin);
            ImGui::BeginDisabled(!ok);
            if (ImGui::Button(tr(T::SaveButton), ImVec2(120.0f * scale_, 0)) || (enter && ok)) {
                save_profile_as(kind, name);
                save_profile_kind_.reset();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button(tr(T::Cancel), ImVec2(120.0f * scale_, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                save_profile_kind_.reset();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    catalog_dialog();
    choose_settings_dialog();
    credits_dialog();
    // A pause's message, or the G-code for a layer (preview.md, rule 12).
    if (change_dialog_) {
        ImGui::OpenPopup("###layer_change");
        change_dialog_ = false;
    }
    if (ImGui::BeginPopupModal(
            (std::string(tr(change_kind_ == ChangeKind::Pause ? T::AddPause : T::AddCustomGcode)) + "###layer_change").c_str(),
            nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        loc::Text(tr(T::ChangeAtLayer), change_layer_ + 1);
        loc::TextDisabled("%s", tr(change_kind_ == ChangeKind::Pause ? T::PauseMessage : T::CustomGcodeText));
        if (ImGui::IsWindowAppearing())
            ImGui::SetKeyboardFocusHere();
        if (change_kind_ == ChangeKind::Pause)
            ImGui::InputText("##change_text", &change_text_);
        else
            ImGui::InputTextMultiline("##change_text", &change_text_, ImVec2(360.0f * scale_, 6.0f * ImGui::GetTextLineHeight()));
        if (ImGui::Button(tr(T::Accept), ImVec2(120.0f * scale_, 0))) {
            add_change_at(change_layer_, change_kind_, change_text_);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(tr(T::Cancel), ImVec2(120.0f * scale_, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (about_open_) {
        ImGui::OpenPopup("###about");
        about_open_ = false;
    }
    ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    const std::string about = std::string(tr(T::About)) + "###about";
    if (ImGui::BeginPopupModal(about.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (logo_)
            ImGui::Image(ImTextureRef(logo_), ImVec2(72.0f * scale_, 72.0f * scale_));
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::PushFont(nullptr, kTitleSize);
        ImGui::TextUnformatted("TISMA");
        ImGui::PopFont();
        loc::Text(tr(T::AboutText), TISMA_VERSION);
        ImGui::EndGroup();
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 480.0f * scale_);
        loc::TextDisabled("%s", tr(T::AboutLibraries));
        ImGui::PopTextWrapPos();
        if (ImGui::Button(tr(T::Close), ImVec2(120.0f * scale_, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

void App::layout() {
    anchors_.clear();
    viewport();
    anchor_rect("plate", view_pos_, ImVec2(view_pos_.x + view_size_.x, view_pos_.y + view_size_.y));
    top_bar();
    // The printer's page covers the panel and the view (settings_panel.md,
    // rule 37) until it goes back to the plate.
    if (printer_page_) {
        printer_page();
        status_bar();
    } else {
        settings_panel();
        if (help_open_)
            help_panel();
        status_bar();
        view_tools();
        if (tab_ == Tab::Prepare && !objects_.empty() && !calibration_ && !viewer_) {
            plate_tools();
            volumes_overlay();
            object_settings_window();
        }
        mixing_dialog();
        if (!mesh_ && !result_ && tab_ != Tab::Calibration && tab_ != Tab::Device) // those panels have their own buttons
            empty_hint();
        if (tab_ == Tab::Preview)
            preview_panels();
        if (show_labels_ && (tab_ == Tab::Prepare || tab_ == Tab::Preview))
            part_labels();
        if (tab_ == Tab::Analysis)
            analysis_panel();
        if (tab_ == Tab::Calibration)
            calibration_panel();
        if (tab_ == Tab::Device)
            device_panel();
    }
    // Each function's tip the first time it is used.
    if (tool_ != last_tool_) {
        last_tool_ = tool_;
        static const std::map<PlateTool, const char*> tool_tips = {
            {PlateTool::Move, "move"}, {PlateTool::Rotate, "rotate"}, {PlateTool::Scale, "scale"},
            {PlateTool::LayFlat, "lay"}, {PlateTool::Paint, "paint"}};
        if (const auto it = tool_tips.find(tool_); it != tool_tips.end())
            tip(it->second);
    }
    if (tab_ != last_tab_) {
        last_tab_ = tab_;
        static const std::map<Tab, const char*> tab_tips = {{Tab::Preview, "preview"}, {Tab::Analysis, "analysis"},
                                                            {Tab::Calibration, "calibration"}, {Tab::Device, "device"}};
        if (const auto it = tab_tips.find(tab_); it != tab_tips.end())
            tip(it->second);
        if (tab_ == Tab::Analysis && !guide_memory_.done.contains("opened:engineering")) {
            guide_memory_.done.insert("opened:engineering");
            offer_tutorials("first_open:engineering");
        }
    }
    if (result_ && !had_result_ && !viewer_) {
        had_result_ = true;
        tip("slice");
    }
    help_windows();
    guide_overlay();
    toasts();
    dialogs();

    // Shortcuts.
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_O, ImGuiInputFlags_RouteGlobal)) {
        purpose_ = Purpose::Open;
        browser_.open(FileBrowser::Mode::Open, tr(T::OpenFile), kOpenExtensions, last_folder_);
    }
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_F, ImGuiInputFlags_RouteGlobal))
        search_focus_ = true;
    if (ImGui::Shortcut(ImGuiKey_F1, ImGuiInputFlags_RouteGlobal))
        help_open_ = !help_open_;
    // The view's keys (preview.md, rule 10) while no text field has the keyboard.
    const ImGuiIO& keys = ImGui::GetIO();
    if (!keys.WantTextInput && !keys.KeyCtrl && !keys.KeyAlt && !keys.KeySuper && !printer_page_ &&
        tab_ != Tab::Calibration && tab_ != Tab::Device) {
        static constexpr std::pair<ImGuiKey, StandardView> fixed[] = {
            {ImGuiKey_0, StandardView::Iso},  {ImGuiKey_1, StandardView::Top},  {ImGuiKey_2, StandardView::Bottom},
            {ImGuiKey_3, StandardView::Front}, {ImGuiKey_4, StandardView::Back}, {ImGuiKey_5, StandardView::Left},
            {ImGuiKey_6, StandardView::Right}};
        for (const auto& [key, view] : fixed)
            if (ImGui::IsKeyPressed(key, false) ||
                ImGui::IsKeyPressed(ImGuiKey(int(ImGuiKey_Keypad0) + (int(key) - int(ImGuiKey_0))), false)) {
                camera_.set_view(view);
                frame_all();
            }
        if (ImGui::IsKeyPressed(ImGuiKey_K, false))
            camera_.orthographic = !camera_.orthographic;
        if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
            // The chosen part, or everything.
            if (tab_ == Tab::Prepare && selected_ >= 0 && std::size_t(selected_) < placed_meshes_.size() &&
                !placed_meshes_[std::size_t(selected_)].empty()) {
                Vec3 lo{1e30f, 1e30f, 1e30f}, hi{-1e30f, -1e30f, -1e30f};
                for (const Vec3f& c : placed_meshes_[std::size_t(selected_)].corners()) {
                    lo = Vec3{std::min(lo.x, c.x), std::min(lo.y, c.y), std::min(lo.z, c.z)};
                    hi = Vec3{std::max(hi.x, c.x), std::max(hi.y, c.y), std::max(hi.z, c.z)};
                }
                camera_.fit(lo, hi, view_size_.x / std::max(view_size_.y, 1.0f));
            } else {
                frame_all();
            }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_E, false))
            show_labels_ = !show_labels_;
        if (ImGui::IsKeyPressed(ImGuiKey_L, false))
            show_legend_ = !show_legend_;
    }
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_RouteGlobal))
        begin_save_project(false);
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S, ImGuiInputFlags_RouteGlobal))
        begin_save_project(true);
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_R, ImGuiInputFlags_RouteGlobal))
        start_slice();
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_E, ImGuiInputFlags_RouteGlobal) && result_ && result_->ok && !viewer_ &&
        !stale_)
        begin_export();
}

// The frame of the 3D view: where it is, the camera, the lights (they follow
// the camera: the key from above, left and in front, the fill from the right)
// and what to show.
ViewFrame App::view_frame(int fb_w, int fb_h) {
    const ImGuiIO& io = ImGui::GetIO();
    const float sx = io.DisplayFramebufferScale.x, sy = io.DisplayFramebufferScale.y;
    ViewFrame f;
    f.x = int(view_pos_.x * sx);
    f.width = std::max(int(view_size_.x * sx), 1);
    f.height = std::max(int(view_size_.y * sy), 1);
    f.y = fb_h - int((view_pos_.y + view_size_.y) * sy);
    (void)fb_w;
    f.view = camera_.view();
    f.projection = camera_.projection(float(f.width) / float(f.height));
    f.eye = camera_.eye();
    const Vec3 forward = camera_.forward(), right = camera_.right();
    Vec3 back{-forward.x, -forward.y, 0.0f};
    if (length(back) < 1e-3f) // looking straight down: "back" is the bottom of the screen
        back = Vec3{-camera_.up().x, -camera_.up().y, 0.0f};
    back = normalize(back);
    f.key = normalize(right * -0.45f + back * 0.45f + Vec3{0, 0, 0.8f});
    f.fill = normalize(right * 0.6f + back * 0.25f + Vec3{0, 0, 0.3f});
    f.sky = vec(theme_.sky);
    f.ground = vec(theme_.ground);
    f.background_top = vec(theme_.view_top);
    f.background_bottom = vec(theme_.view_bottom);
    f.plate = vec(theme_.plate);
    f.model = vec(theme_.model);
    f.model_selected = f.model * 0.6f + Vec3{1, 1, 1} * 0.4f;
    if (tab_ == Tab::Prepare && objects_.size() > 1 && selected_ >= 0 &&
        std::size_t(selected_) + 1 < object_vertices_.size()) {
        f.selected_first = object_vertices_[std::size_t(selected_)];
        f.selected_count = object_vertices_[std::size_t(selected_) + 1] - f.selected_first;
    } else if (tab_ == Tab::Prepare && objects_.size() == 1 && selected_ == 0 && tool_ != PlateTool::Select) {
        f.selected_first = 0;
        f.selected_count = object_vertices_.size() > 1 ? object_vertices_[1] : 0;
    }
    if (dragged_ && pressed_object_ >= 0 && std::size_t(pressed_object_) + 1 < object_vertices_.size()) {
        if (f.selected_count == 0) { // nothing is highlighted: the dragged model keeps its colour
            f.selected_first = object_vertices_[std::size_t(pressed_object_)];
            f.selected_count = object_vertices_[std::size_t(pressed_object_) + 1] - f.selected_first;
            f.model_selected = f.model;
        }
        f.selected_offset = drag_offset_;
    }
    // While the camera moves, a large plate's coarse copy (V6), the same
    // model highlighted in it.
    f.coarse = (options_.coarse || frames_ - moved_frame_ <= 2) && coarse_version_ == scene_version_ &&
               !coarse_object_vertices_.empty();
    if (f.coarse && f.selected_count > 0)
        for (std::size_t k = 0; k + 1 < object_vertices_.size() && k + 1 < coarse_object_vertices_.size(); ++k)
            if (object_vertices_[k] == f.selected_first) {
                f.coarse_selected_first = coarse_object_vertices_[k];
                f.coarse_selected_count = coarse_object_vertices_[k + 1] - coarse_object_vertices_[k];
                break;
            }
    // The calibration tab shows its sliced part, up to the chosen segment.
    const bool calibration_preview = tab_ == Tab::Calibration && calibration_ && result_ && !stale_ && !viewer_;
    const bool preview = (tab_ == Tab::Preview && result_) || calibration_preview;
    f.show_model = mesh_.has_value() && !preview;
    f.show_paint = f.show_model && tab_ == Tab::Prepare && tool_ == PlateTool::Paint;
    if (f.show_paint)
        update_paint_view();
    f.show_preview = preview;
    f.show_travels = show_travels_;
    f.first_layer = std::size_t(bottom_layer_);
    // The move slider: the top layer's extrusions among its first moves, and
    // which of them the part hides (they are drawn apart).
    if (preview && tab_ == Tab::Preview && result_ && move_cut_layer_ == top_layer_ && move_cut_ > 0 &&
        std::size_t(top_layer_) < result_->preview.layers.size()) {
        const GcodePreview& p = result_->preview;
        const PreviewLayer& top = p.layers[std::size_t(top_layer_)];
        if (std::size_t(move_cut_) < top.count) {
            std::size_t extrusions = 0;
            for (std::size_t i = top.first; i < top.first + std::size_t(move_cut_); ++i)
                extrusions += p.moves[i].kind != LineKind::Travel;
            std::size_t hidden = 0;
            if (std::size_t(top_layer_) < preview_buffers_.layer_extrusions.size())
                for (std::size_t j = 0; j < extrusions; ++j) {
                    const std::size_t e = preview_buffers_.layer_extrusions[std::size_t(top_layer_)] + j;
                    hidden += e < preview_buffers_.hidden.size() && preview_buffers_.hidden[e];
                }
            f.top_shown = extrusions - hidden;
            f.top_hidden = hidden;
        }
    }
    f.last_layer = std::size_t(top_layer_);
    if (calibration_preview) {
        f.first_layer = 0;
        f.last_layer = result_->preview.layers.empty() ? 0 : result_->preview.layers.size() - 1;
        if (chosen_value_)
            for (const CalibrationSegment& seg : calibration_->segments)
                if (std::abs(seg.value - *chosen_value_) < 1e-9)
                    for (std::size_t l = 0; l < result_->preview.layers.size(); ++l)
                        if (result_->preview.layers[l].z <= seg.z1 + 1e-6)
                            f.last_layer = l;
    }
    f.kind_mask = kind_mask_;
    f.color_mode = color_mode_;
    for (std::size_t k = 0; k < kLineKinds; ++k)
        f.kind_colours[k] = vec(kind_colour(LineKind(k)));
    f.mode = render_mode_;
    f.samples = msaa_;
    f.fxaa = fxaa_;
    f.moire_filter = moire_;
    f.ao_strength = ao_strength_;
    static const bool debug = [] {
        const char* v = std::getenv("TISMA_DEBUG_VISIBILITY");
        return v && *v == '1';
    }();
    f.debug_visibility = debug;
    // Occlusion reaches about a line's width or two in the preview, and a
    // part of the model's size before slicing.
    const bool ray = render_mode_ == RenderMode::RayTraced;
    if (preview) {
        const auto& p = result_->preview;
        const float size = length(Vec3{p.max[0] - p.min[0], p.max[1] - p.min[1], p.max[2] - p.min[2]});
        f.ao_radius = ray ? std::clamp(size * 0.04f, 1.5f, 20.0f) : std::clamp(size * 0.03f, 1.0f, 10.0f);
    } else {
        const float size = mesh_ ? length(Vec3{placed_.max.x - placed_.min.x, placed_.max.y - placed_.min.y,
                                               placed_.max.z - placed_.min.z})
                                 : 100.0f;
        f.ao_radius = ray ? std::clamp(size * 0.12f, 2.0f, 60.0f) : std::clamp(size * 0.05f, 1.0f, 25.0f);
    }
    return f;
}

// A large plate's coarse copy for moving the view (PLAN.md V6): from a
// million triangles, about 400 000 of them shared among the objects by their
// size, clustered on another thread and uploaded when ready for the plate
// as it is now.
void App::update_coarse_model() {
    constexpr std::size_t kCoarseFrom = 1000000, kCoarseTarget = 400000;
    if (std::optional<CoarseBuild> b = coarse_job_.take(); b && b->version == scene_version_) {
        renderer_.set_model_coarse(b->vertices);
        coarse_object_vertices_ = std::move(b->objects);
        coarse_version_ = b->version;
    }
    if (coarse_version_ == scene_version_ || coarse_job_.running())
        return;
    std::size_t total = 0;
    for (const TriangleMesh& m : placed_meshes_)
        total += m.triangle_count();
    if (total < kCoarseFrom) { // small enough to draw as it is
        renderer_.set_model_coarse({});
        coarse_object_vertices_.clear();
        coarse_version_ = scene_version_;
        return;
    }
    coarse_job_.start([meshes = placed_meshes_, offset = offset_, version = scene_version_, total]() {
        CoarseBuild out;
        out.version = version;
        for (const TriangleMesh& m : meshes) {
            out.objects.push_back(out.vertices.size() / 6);
            const std::size_t share = std::max<std::size_t>(1000, kCoarseTarget * m.triangle_count() / total);
            const std::vector<float> v = mesh_vertices(clustered(m, share), offset);
            out.vertices.insert(out.vertices.end(), v.begin(), v.end());
        }
        out.objects.push_back(out.vertices.size() / 6);
        return out;
    });
}

// Keeps the ray tracing scene in step with what the view shows: the model
// (with the plate) or the preview's extrusions (with the plate), built on
// another thread and uploaded when ready.
void App::update_ray_scene() {
    if (render_mode_ != RenderMode::RayTraced)
        return;
    const bool want_preview = tab_ == Tab::Preview && result_.has_value();
    if (std::optional<RayBuild> b = ray_job_.take(); b && b->version == scene_version_ && b->preview == want_preview) {
        ray_version_ = b->version;
        ray_preview_ = b->preview;
        // A scene cut at RayScene::kMaxPrimitives would light the rest wrong:
        // too big, as one the GPU cannot hold.
        ray_failed_ = (b->scene && b->scene->truncated()) || !renderer_.set_ray_scene(b->nodes, b->primitives);
        ray_scene_ = ray_failed_ ? nullptr : b->scene;
        if (ray_failed_)
            notify(tr(T::RayTooBig), true);
    }
    if ((ray_version_ == scene_version_ && ray_preview_ == want_preview) || ray_job_.running())
        return;
    renderer_.clear_ray_scene();
    ray_scene_.reset();
    std::vector<RayPrimitive> primitives;
    if (want_preview)
        primitives = preview_primitives(preview_buffers_);
    else if (mesh_)
        primitives = model_primitives(*mesh_, offset_);
    const std::vector<RayPrimitive> plate = plate_primitives(settings_.bed);
    primitives.insert(primitives.end(), plate.begin(), plate.end());
    const std::uint64_t version = scene_version_;
    ray_job_.start([primitives = std::move(primitives), version, want_preview]() mutable {
        RayBuild b;
        b.version = version;
        b.preview = want_preview;
        b.scene = std::make_shared<RayScene>();
        b.scene->build(std::move(primitives));
        b.nodes = b.scene->node_texels();
        b.primitives = b.scene->primitive_texels();
        return b;
    });
}

// The GPU's shadow rays of the first ray traced frame against the CPU's
// (RayScene, the reference): the same surfaces, read back from the G-buffer,
// the same origins and the same direction to the light.
void App::check_rays(const ViewFrame& f) {
    ray_checked_ = true;
    const RayReadback r = renderer_.read_back();
    const Mat4 inverse = rigid_inverse(f.view);
    const Mat4& m = f.projection;
    RayFilter filter;
    filter.first_layer = f.show_preview ? std::uint32_t(f.first_layer) : 0;
    filter.last_layer = f.show_preview ? std::uint32_t(f.last_layer) : 0;
    filter.kind_mask = f.kind_mask;
    std::size_t compared = 0, same = 0, shadowed = 0;
    for (int y = 0; y < r.height; y += 3)
        for (int x = 0; x < r.width; x += 3) {
            const std::size_t i = std::size_t(y) * std::size_t(r.width) + std::size_t(x);
            const float depth = r.depth[i];
            if (depth >= 1.0f)
                continue;
            const float ndc = depth * 2.0f - 1.0f;
            const float z = -m[14] / (ndc + m[10]);
            const float nx = (float(x) + 0.5f) / float(r.width) * 2.0f - 1.0f;
            const float ny = (float(y) + 0.5f) / float(r.height) * 2.0f - 1.0f;
            const Vec3 pv{nx * -z / m[0], ny * -z / m[5], z};
            const Vec3 nv = normalize(Vec3{r.normal[i * 4] / 255.0f * 2.0f - 1.0f, r.normal[i * 4 + 1] / 255.0f * 2.0f - 1.0f,
                                           r.normal[i * 4 + 2] / 255.0f * 2.0f - 1.0f});
            const Vec3 position = transform_point(inverse, pv);
            const Vec3 n = normalize(Vec3{inverse[0] * nv.x + inverse[4] * nv.y + inverse[8] * nv.z,
                                          inverse[1] * nv.x + inverse[5] * nv.y + inverse[9] * nv.z,
                                          inverse[2] * nv.x + inverse[6] * nv.y + inverse[10] * nv.z});
            const Vec3 origin = position + n * (0.0015f * -pv.z + 0.002f);
            float light = 1.0f;
            if (dot(n, f.key) > 0.0f)
                light = ray_scene_->occluded({origin, f.key}, filter) ? 0.0f : 1.0f;
            const float gpu = r.visibility[i * 4];
            ++compared;
            same += std::abs(gpu - light) < 0.5f;
            shadowed += light < 0.5f;
        }
    const double agreement = compared ? double(same) / double(compared) : 0.0;
    std::printf("ray check: %zu pixels, %zu in shadow, GPU and CPU agree on %.3f %%\n", compared, shadowed,
                agreement * 100.0);
    if (compared < 1000 || shadowed == 0 || agreement < 0.99) {
        std::fprintf(stderr, "error: the GPU's rays do not match the CPU's\n");
        failed_ = true;
    }
}

// The Device page (docs/DEVICE.md): the port, the connection, the state and
// the temperatures, printing what is sliced (or the opened G-code), pausing
// and cancelling, moving and extruding, and the events.
void App::device_connect(const std::string& port) {
    device_disconnect();
    std::shared_ptr<device::ByteStream> stream;
    if (port == "sim") {
        auto [host_end, printer_end] = device::make_pipe();
        device::SimulatorOptions sim;
        sim.tick = std::chrono::milliseconds(50);
        simulator_ = std::make_unique<device::SimulatedPrinter>(printer_end, sim);
        stream = host_end;
    } else {
        auto serial = std::make_shared<device::SerialPort>();
        std::string error;
        if (!serial->open(port, device_baud_, error)) {
            notify(format(tr(T::DeviceCannotOpen), error.c_str()), true);
            device_log_.push_back(error);
            return;
        }
        stream = serial;
    }
    printer_ = std::make_unique<device::GcodeHost>(stream);
    printer_->connect();
}

void App::device_disconnect() {
    if (printer_)
        printer_->disconnect();
    printer_.reset();
    simulator_.reset();
}

void App::device_poll() {
    if (!printer_)
        return;
    using K = device::PrinterEvent::Kind;
    for (const device::PrinterEvent& e : printer_->take_events()) {
        std::string line;
        switch (e.kind) {
        case K::Connected: line = format(tr(T::DeviceConnected), e.text.c_str()); notify(line); break;
        case K::Started: line = format(tr(T::DevicePrintStarted), e.text.c_str()); break;
        case K::Finished:
            line = format(tr(T::DevicePrintFinished), e.text.c_str());
            notify(line);
            if (options_.device_print)
                device_print_done_ = true;
            break;
        case K::Cancelled:
            line = format(tr(T::DevicePrintCancelled), e.text.c_str());
            notify(line, true);
            if (options_.device_print) {
                std::fprintf(stderr, "error: the print was cancelled: %s\n", printer_->status().error.c_str());
                failed_ = true;
                device_print_done_ = true;
            }
            break;
        case K::Error: line = format(tr(T::DevicePrinterError), e.text.c_str()); notify(line, true); break;
        case K::Paused: line = tr(T::DeviceStatePaused); break;
        case K::Resumed: line = tr(T::DeviceStatePrinting); break;
        case K::Disconnected: line = tr(T::DeviceStateDisconnected); break;
        case K::Message: line = e.text; break;
        }
        device_log_.push_back(std::move(line));
    }
    if (device_log_.size() > 200)
        device_log_.erase(device_log_.begin(), device_log_.end() - 200);
}

bool App::device_can_print() const {
    return result_ && !result_->gcode.empty() && (viewer_ || (result_->ok && result_->check.ok() && !stale_));
}

void App::device_print() {
    if (!printer_ || !device_can_print())
        return;
    const std::string name = viewer_ && !gcode_path_.empty() ? utf8(gcode_path_.filename())
                             : model_path_.empty()          ? std::string("tisma.gcode")
                                                            : utf8(model_path_.stem()) + ".gcode";
    printer_->start_print(result_->gcode, name);
}

void App::device_panel() {
    const float margin = 12.0f * scale_;
    ImGui::SetNextWindowPos(ImVec2(view_pos_.x + margin, view_pos_.y + 60.0f * scale_));
    ImGui::SetNextWindowSize(ImVec2(450.0f * scale_, view_size_.y - 72.0f * scale_));
    ImGui::SetNextWindowBgAlpha(0.95f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f * scale_);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f * scale_, 14.0f * scale_));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, rgb(theme_.panel));
    ImGui::Begin("##device", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    ImGui::BeginChild("##device_scroll", ImVec2(0, 0));
    ImGui::PushFont(nullptr, kTitleSize * 0.9f);
    ImGui::TextUnformatted(tr(T::DeviceTitle));
    ImGui::PopFont();
    ImGui::PushTextWrapPos(0.0f);
    loc::TextDisabled("%s", tr(T::DeviceHint));
    const float button_h = 30.0f * scale_;
    const float field = 150.0f * scale_;

    const device::PrinterStatus s = printer_ ? printer_->status() : device::PrinterStatus{};
    using State = device::PrinterState;
    const bool connected = printer_ && s.state != State::Disconnected && s.state != State::Error;

    // 1: the port and the connection.
    ImGui::Separator();
    const auto port_name = [&](const std::string& p) { return p == "sim" ? std::string(tr(T::DeviceSimulated)) : p; };
    ImGui::BeginDisabled(connected);
    ImGui::SetNextItemWidth(field * 1.4f);
    if (ImGui::BeginCombo(tr(T::DevicePort), port_name(device_port_).c_str())) {
        for (const std::string& p : device_ports_)
            if (ImGui::Selectable(p.c_str(), p == device_port_))
                device_port_ = p;
        if (ImGui::Selectable(tr(T::DeviceSimulated), device_port_ == "sim"))
            device_port_ = "sim";
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(tr(T::DeviceRefresh)))
        device_ports_ = device::list_serial_ports();
    if (device_port_ != "sim") {
        ImGui::SetNextItemWidth(field);
        const int bauds[] = {115200, 250000, 57600, 230400};
        if (ImGui::BeginCombo(tr(T::DeviceBaud), std::to_string(device_baud_).c_str())) {
            for (int b : bauds)
                if (ImGui::Selectable(std::to_string(b).c_str(), b == device_baud_))
                    device_baud_ = b;
            ImGui::EndCombo();
        }
    }
    if (device_ports_.empty())
        loc::TextDisabled("%s", tr(T::DeviceNoPorts));
    ImGui::EndDisabled();
    if (!connected) {
        if (button("##connect", "tisma_printer", tr(T::DeviceConnect), true, false, true, nullptr, button_h))
            device_connect(device_port_);
    } else if (button("##disconnect", nullptr, tr(T::DeviceDisconnect), false, false, true, nullptr, button_h)) {
        device_disconnect();
    }

    // 2: the state and the temperatures.
    ImGui::Separator();
    const T state_text[] = {T::DeviceStateDisconnected, T::DeviceStateConnecting, T::DeviceStateIdle,
                            T::DeviceStatePrinting,     T::DeviceStatePaused,     T::DeviceStateError};
    const std::uint32_t state_colour = s.state == State::Error      ? 0xE5484D
                                       : s.state == State::Printing ? theme_.accent_hover
                                       : s.state == State::Idle     ? 0x30A46C
                                                                    : 0x8B8D98;
    {
        const float r = ImGui::GetTextLineHeight() * 0.28f;
        const ImVec2 at = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(at.x + r, at.y + ImGui::GetTextLineHeight() * 0.5f), r,
                                                    ImGui::GetColorU32(rgb(state_colour)));
        ImGui::Dummy(ImVec2(r * 2.0f, ImGui::GetTextLineHeight()));
        ImGui::SameLine(0, 6.0f * scale_);
        loc::TextColored(rgb(state_colour), "%s", tr(state_text[int(s.state)]));
    }
    if (!s.firmware.empty()) {
        ImGui::SameLine();
        loc::TextDisabled(tr(T::DeviceFirmware), s.firmware.c_str());
    }
    if (!s.error.empty())
        loc::TextColored(rgb(0xE5484D), "%s", s.error.c_str());
    if (connected) {
        const auto heater = [&](T label, const device::Temperature& t, double& target, bool nozzle) {
            ImGui::PushID(nozzle ? "nozzle" : "bed");
            loc::Text("%s  %.1f / %.0f °C", tr(label), t.actual, t.target);
            ImGui::SetNextItemWidth(field * 0.7f);
            double lo = 0.0, hi = nozzle ? 400.0 : 150.0;
            ImGui::DragScalar("##target", ImGuiDataType_Double, &target, 1.0f, &lo, &hi, field_text("##target", "%.0f °C", target),
                              ImGuiSliderFlags_AlwaysClamp);
            ImGui::SameLine();
            if (ImGui::SmallButton(tr(T::DeviceSet)))
                nozzle ? printer_->set_nozzle(target) : printer_->set_bed(target);
            ImGui::SameLine();
            if (ImGui::SmallButton(tr(T::DeviceOff)))
                nozzle ? printer_->set_nozzle(0.0) : printer_->set_bed(0.0);
            ImGui::PopID();
        };
        heater(T::DeviceNozzle, s.nozzle, device_nozzle_, true);
        heater(T::DeviceBed, s.bed, device_bed_, false);
    }

    // 3: the job.
    ImGui::Separator();
    const bool busy = s.state == State::Printing || s.state == State::Paused;
    if (busy || (s.lines_total > 0 && s.progress > 0.0)) {
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, rgb(theme_.accent));
        ImGui::ProgressBar(float(s.progress), ImVec2(-FLT_MIN, 0.0f));
        ImGui::PopStyleColor();
        loc::Text(tr(T::DeviceProgress), s.job.c_str(), s.progress * 100.0, number_text(s.lines_sent).c_str(),
                    number_text(s.lines_total).c_str());
        const double total = result_ && result_->preview.estimated_seconds > 0.0 ? result_->preview.estimated_seconds
                             : s.progress > 0.01                                ? s.elapsed / s.progress
                                                                                : 0.0;
        if (busy)
            loc::TextDisabled(tr(T::DeviceTimes), format_duration(s.elapsed).c_str(),
                                format_duration(std::max(0.0, total * (1.0 - s.progress))).c_str());
        if (s.resends > 0)
            loc::TextDisabled(tr(T::DeviceResends), s.resends);
    }
    if (busy) {
        if (s.state == State::Paused) {
            if (button("##resume", nullptr, tr(T::DeviceResume), true, false, true, nullptr, button_h))
                printer_->resume();
        } else if (button("##pause", nullptr, tr(T::DevicePause), false, false, true, nullptr, button_h)) {
            printer_->pause();
        }
        ImGui::SameLine();
        if (button("##cancel", nullptr, tr(T::DeviceCancel), false, false, true, nullptr, button_h))
            printer_->cancel();
    } else if (device_can_print()) {
        const std::string what = viewer_ && !gcode_path_.empty() ? utf8(gcode_path_.filename())
                                 : model_path_.empty()          ? std::string("G-code")
                                                                : utf8(model_path_.filename());
        loc::TextDisabled(tr(T::DevicePrintWhat), what.c_str());
        if (button("##print", "tisma_print", tr(T::DevicePrint), true, false, s.state == State::Idle, nullptr,
                   button_h))
            device_print();
    } else {
        loc::TextDisabled("%s", tr(T::DeviceNothingToPrint));
    }

    // 4: moving and extruding, when idle.
    if (s.state == State::Idle && ImGui::CollapsingHeader(tr(T::DeviceControls))) {
        if (ImGui::SmallButton(tr(T::DeviceHome)))
            printer_->home();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(field * 0.6f);
        double lo = 0.1, hi = 100.0;
        ImGui::DragScalar(tr(T::DeviceStep), ImGuiDataType_Double, &device_step_, 0.1f, &lo, &hi, field_text(tr(T::DeviceStep), "%.1f mm", device_step_),
                          ImGuiSliderFlags_AlwaysClamp);
        for (const char axis : {'X', 'Y', 'Z'}) {
            ImGui::PushID(axis);
            const double feed = axis == 'Z' ? 10.0 : 50.0;
            const std::string minus = std::string("-") + axis, plus = std::string("+") + axis;
            if (ImGui::Button(minus.c_str(), ImVec2(56.0f * scale_, 0)))
                printer_->move(axis, -device_step_, feed);
            ImGui::SameLine();
            if (ImGui::Button(plus.c_str(), ImVec2(56.0f * scale_, 0)))
                printer_->move(axis, device_step_, feed);
            ImGui::PopID();
            if (axis != 'Z')
                ImGui::SameLine(0, 16.0f * scale_);
        }
        if (ImGui::SmallButton(tr(T::DeviceExtrude)))
            printer_->extrude(5.0, 5.0);
        ImGui::SameLine();
        if (ImGui::SmallButton(tr(T::DeviceRetract)))
            printer_->extrude(-5.0, 20.0);
        ImGui::SetNextItemWidth(field * 0.6f);
        double flo = 0.0, fhi = 100.0;
        ImGui::DragScalar("##fan", ImGuiDataType_Double, &device_fan_, 1.0f, &flo, &fhi, field_text("##fan", "%.0f %%", device_fan_),
                          ImGuiSliderFlags_AlwaysClamp);
        ImGui::SameLine();
        if (ImGui::SmallButton(tr(T::DeviceFan)))
            printer_->fan(device_fan_);
    }

    // 5: what happened.
    if (!device_log_.empty() && ImGui::CollapsingHeader(tr(T::DeviceLog))) {
        for (auto it = device_log_.rbegin(); it != device_log_.rend() && it - device_log_.rbegin() < 30; ++it)
            loc::TextDisabled("%s", it->c_str());
    }
    ImGui::PopTextWrapPos();
    ImGui::EndChild();
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

// The moiré filter against the plain picture: the preview from far enough
// that its layers are finer than a pixel, drawn both ways without
// multisampling. Where the filter changes pixels, the picture must get
// smoother: neighbouring pixels differ less in luma, because the fine lines
// blend instead of aliasing into bands.
void App::check_moire(int fb_w, int fb_h) {
    moire_checked_ = true;
    const float distance = camera_.distance;
    camera_.distance *= 2.0f;
    std::vector<std::uint8_t> picture[2];
    int w = 0, h = 0;
    for (int pass = 0; pass < 2; ++pass) {
        ViewFrame f = view_frame(fb_w, fb_h);
        f.samples = 1;
        f.moire_filter = pass == 1;
        renderer_.draw(f);
        picture[pass] = renderer_.read_colour();
        w = f.width, h = f.height;
    }
    camera_.distance = distance;
    // Both pictures next to the screenshot, as evidence.
    for (int pass = 0; pass < 2; ++pass) {
        std::vector<std::uint8_t> flipped(picture[pass].size());
        const std::size_t row = std::size_t(w) * 4;
        for (int yy = 0; yy < h; ++yy)
            std::copy_n(picture[pass].begin() + std::ptrdiff_t(std::size_t(h - 1 - yy) * row), row,
                        flipped.begin() + std::ptrdiff_t(std::size_t(yy) * row));
        for (std::size_t i = 3; i < flipped.size(); i += 4)
            flipped[i] = 255;
        std::filesystem::path out = from_utf8(options_.screenshot);
        out.replace_extension();
        write_png(out.string() + (pass == 0 ? "_moire_off.png" : "_moire_on.png"), w, h, flipped.data());
    }
    const auto luma = [&](int pass, int x, int y) {
        const std::uint8_t* p = &picture[pass][(std::size_t(y) * std::size_t(w) + std::size_t(x)) * 4];
        return 0.299 * p[0] + 0.587 * p[1] + 0.114 * p[2];
    };
    // The pixels the filter changed, and their neighbours.
    std::vector<std::uint8_t> changed(std::size_t(w) * std::size_t(h), 0);
    std::size_t region = 0;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            if (std::abs(luma(0, x, y) - luma(1, x, y)) > 4.0)
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                        if (x + dx >= 0 && x + dx < w && y + dy >= 0 && y + dy < h)
                            changed[std::size_t(y + dy) * std::size_t(w) + std::size_t(x + dx)] = 1;
    double energy[2] = {0.0, 0.0};
    for (int y = 0; y + 1 < h; ++y)
        for (int x = 0; x + 1 < w; ++x) {
            if (!changed[std::size_t(y) * std::size_t(w) + std::size_t(x)])
                continue;
            ++region;
            for (int pass = 0; pass < 2; ++pass)
                energy[pass] += std::abs(luma(pass, x + 1, y) - luma(pass, x, y)) +
                                std::abs(luma(pass, x, y + 1) - luma(pass, x, y));
        }
    for (double& e : energy)
        e /= double(std::max<std::size_t>(region, 1));
    std::printf("moire check: %zu pixels; neighbours differ by %.2f without the filter, %.2f with it\n", region,
                energy[0], energy[1]);
    if (region < 200 || energy[1] > energy[0] * 0.85) {
        std::fprintf(stderr, "error: the moire filter does not smooth the fine lines\n");
        failed_ = true;
    }
}

// --compare-view: the 3D view against another screenshot's, drawn by the
// other graphics (OpenGL against Vulkan, PLAN.md «Vista con Vulkan»). The
// drivers rasterize the edges of lines and triangles a little differently,
// so a few pixels along them may differ; the surfaces must not.
void App::compare_view(const std::vector<std::uint8_t>& picture, int fb_w, int fb_h) {
    int w = 0, h = 0;
    std::vector<std::uint8_t> other;
    if (!read_png(from_utf8(options_.compare_view), w, h, other) || w != fb_w || h != fb_h) {
        std::fprintf(stderr, "error: cannot read %s as a %dx%d screenshot\n", options_.compare_view.c_str(), fb_w, fb_h);
        failed_ = true;
        return;
    }
    const ImGuiIO& io = ImGui::GetIO();
    const float sx = io.DisplayFramebufferScale.x, sy = io.DisplayFramebufferScale.y;
    const PictureDifference d =
        compare_pictures(picture.data(), other.data(), w, h, int(view_pos_.x * sx), int(view_pos_.y * sy),
                         int(view_size_.x * sx), int(view_size_.y * sy), 24);
    std::printf("view compare (%s): %zu of %zu pixels differ, %zu by more than 24 levels, at most by %d\n",
                vulkan_ ? "Vulkan" : "OpenGL", d.differing, d.pixels, d.over, d.largest);
    if (d.pixels == 0 || d.over * 200 > d.pixels) {
        std::fprintf(stderr, "error: the 3D view differs from %s\n", options_.compare_view.c_str());
        failed_ = true;
    }
}

void App::frame() {
    if (frames_ < 8) {
        static char step[32];
        std::snprintf(step, sizeof step, "frame %d", frames_);
        timing(step);
    }
    // With nothing moving, sleep until an event (or a tenth of a second)
    // instead of drawing at the screen's rate.
    const bool busy = !options_.screenshot.empty() || slice_job_.running() || voxel_job_.running() ||
                      ray_job_.running() || !toasts_.empty() || ImGui::IsAnyItemActive() ||
                      (render_mode_ == RenderMode::RayTraced && renderer_.has_ray_scene() &&
                       renderer_.ray_samples() < 256);
    if (busy)
        glfwPollEvents();
    else
        glfwWaitEventsTimeout(0.1);
    // Files dropped while a file dialog is open wait for it to close: opening
    // one would change the model under a pending export.
    if (!browser_.is_open())
        for (const std::filesystem::path& path : std::exchange(dropped_, {})) {
            // A model dropped on a plate joins it, as Add does.
            if (is_model(path) && !objects_.empty() && !calibration_ && !viewer_)
                add_model(path);
            else
                open_path(path);
        }
    poll_jobs();
    device_poll();

#ifdef TISMA_VULKAN_WINDOW
    if (vulkan_)
        ImGui_ImplVulkan_NewFrame();
    else
#endif
        ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    layout();
    // With Vulkan the 3D view is a picture under the panels, drawn below
    // (vk_view.*) before them.
    if (vulkan_ && vk_view_.texture())
        ImGui::GetBackgroundDrawList()->AddImage(vk_view_.texture(), view_pos_,
                                                 ImVec2(view_pos_.x + view_size_.x, view_pos_.y + view_size_.y));
    ImGui::Render();
    if (frame_pending_ && view_size_.x > 10.0f && view_size_.y > 10.0f)
        frame_now();

    int fb_w = 0, fb_h = 0;
    glfwGetFramebufferSize(window_, &fb_w, &fb_h);
    const Vec3 clear = vec(theme_.window);
    ViewFrame f = view_frame(fb_w, fb_h);
    if (vulkan_) {
        // The 3D view first, into its picture, then the window and the panels.
        const float colour[3] = {clear.x, clear.y, clear.z};
        vk_.frame(ImGui::GetDrawData(), colour, !options_.screenshot.empty(),
                  [&](void* commands, int in_flight) { vk_view_.record(commands, in_flight, f); });
        if (double ms = 0.0; vk_view_.take_time(ms))
            view_ms_ = view_ms_ > 0.0 ? 0.9 * view_ms_ + 0.1 * ms : ms; // smoothed
        if (options_.bench_frames > 0 && !benched_ && settled_frames_ >= 2) {
            const double ms = vk_view_.bench(f, options_.bench_frames);
            std::printf("view: %.2f ms per frame (%d frames, %zu extrusions drawn, Vulkan); preview buffers %.0f ms\n",
                        ms, options_.bench_frames, vk_view_.drawn_extrusions(), preview_ms_);
            std::fflush(stdout);
            benched_ = true;
        }
    } else {
        glViewport(0, 0, fb_w, fb_h);
        glClearColor(clear.x, clear.y, clear.z, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // The 3D view, under the panels.
        update_ray_scene();
        update_coarse_model();
        const bool check_now = options_.ray_check && !ray_checked_ && render_mode_ == RenderMode::RayTraced &&
                               renderer_.has_ray_scene() && ray_scene_;
        if (check_now)
            f.sun_radius = 0.0f; // one exact direction, so the CPU can shoot the same rays
        // The view's time on the GPU (a timer query), read two frames later so
        // nothing waits for it; for the status bar.
        if (!view_queries_[0])
            glGenQueries(3, view_queries_);
        const bool timed = !view_timing_[view_query_ % 3];
        if (timed)
            glBeginQuery(GL_TIME_ELAPSED, view_queries_[view_query_ % 3]);
        renderer_.draw(f);
        if (timed) {
            glEndQuery(GL_TIME_ELAPSED);
            view_timing_[view_query_ % 3] = true;
        }
        view_query_ = (view_query_ + 1) % 3;
        for (int q = 0; q < 3; ++q) {
            GLint ready = 0;
            if (!view_timing_[q])
                continue;
            glGetQueryObjectiv(view_queries_[q], GL_QUERY_RESULT_AVAILABLE, &ready);
            if (!ready)
                continue;
            GLuint64 ns = 0;
            glGetQueryObjectui64v(view_queries_[q], GL_QUERY_RESULT, &ns);
            view_timing_[q] = false;
            const double ms = double(ns) / 1e6;
            view_ms_ = view_ms_ > 0.0 ? 0.9 * view_ms_ + 0.1 * ms : ms; // smoothed
        }
        // --bench-frames: the ready view drawn that many times more, timed to the
        // end of the GPU's work (docs/UI.md, «Modelos grandes»).
        if (options_.bench_frames > 0 && !benched_ && settled_frames_ >= 2 && f.mode != RenderMode::RayTraced) {
            glFinish();
            const auto start = std::chrono::steady_clock::now();
            for (int i = 0; i < options_.bench_frames; ++i)
                renderer_.draw(f);
            glFinish();
            const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            std::printf("view: %.2f ms per frame (%d frames, %zu extrusions drawn); preview buffers %.0f ms\n",
                        ms / options_.bench_frames, options_.bench_frames, renderer_.drawn_extrusions(), preview_ms_);
            std::fflush(stdout);
            benched_ = true;
        }
        if (check_now && renderer_.ray_samples() == 1)
            check_rays(f);

        glViewport(0, 0, fb_w, fb_h);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    // Screenshot mode: once everything asked for is ready, read the frame back.
    if (!options_.screenshot.empty()) {
        ++frames_;
        // A preview waits for its slice, unless the slice failed (the error is the picture then).
        const bool slice_pending =
            (options_.view == "preview" || options_.device_print) && mesh_ && !result_ && error_title_.empty();
        // Ray tracing waits for its scene and for enough samples.
        const bool rays_pending = render_mode_ == RenderMode::RayTraced && !ray_failed_ &&
                                  (ray_job_.running() || !renderer_.has_ray_scene() ||
                                   renderer_.ray_samples() < std::min(options_.ray_samples, 256) ||
                                   (options_.ray_check && !ray_checked_));
        if (options_.moire_check && !moire_checked_ && frames_ > 3 && f.show_preview && !slice_job_.running() &&
            !rays_pending)
            check_moire(fb_w, fb_h);
        // --device-print: connect, print what was sliced and wait for the end.
        if (options_.device_print && !device_print_sent_ && printer_ &&
            printer_->status().state == device::PrinterState::Idle && device_can_print()) {
            device_print();
            device_print_sent_ = true;
        }
        if (options_.device_print && printer_ && !device_print_done_) {
            const device::PrinterState s = printer_->status().state;
            if (s == device::PrinterState::Error) {
                std::fprintf(stderr, "error: the printer: %s\n", printer_->status().error.c_str());
                failed_ = true;
                device_print_done_ = true;
            }
        }
        const bool device_pending =
            options_.device_print && !device_print_done_ && (printer_ != nullptr || !options_.device.empty());
        const bool waiting = slice_job_.running() || voxel_job_.running() || slice_pending || rays_pending ||
                             (options_.coarse && (coarse_job_.running() || coarse_version_ != scene_version_)) ||
                             (options_.moire_check && !moire_checked_) || device_pending;
        if (frames_ == 2) {
            if (options_.view == "analysis")
                set_tab(Tab::Analysis);
            else if (options_.view == "preview" && result_)
                set_tab(Tab::Preview);
            else if (options_.view == "prepare")
                set_tab(Tab::Prepare);
            else if (options_.view == "calibration")
                set_tab(Tab::Calibration);
            else if (options_.view == "device")
                set_tab(Tab::Device);
        }
        settled_frames_ = waiting ? 0 : settled_frames_ + 1;
        if (frames_ > 3 && settled_frames_ >= 3) {
            if (!options_.save_project.empty())
                save_project_to(from_utf8(options_.save_project));
            std::vector<std::uint8_t> pixels(std::size_t(fb_w) * std::size_t(fb_h) * 4);
            if (vulkan_) {
                int w = 0, h = 0;
                std::vector<std::uint8_t> kept = vk_.read_back(w, h);
                if (w == fb_w && h == fb_h)
                    pixels = std::move(kept);
            } else {
                glPixelStorei(GL_PACK_ALIGNMENT, 1);
                glReadPixels(0, 0, fb_w, fb_h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            }
            // OpenGL's rows go up from the bottom; the PNG's go down.
            std::vector<std::uint8_t> flipped(pixels.size());
            const std::size_t row = std::size_t(fb_w) * 4;
            for (int yy = 0; yy < fb_h; ++yy)
                std::copy_n(pixels.begin() + std::ptrdiff_t(std::size_t(fb_h - 1 - yy) * row), row,
                            flipped.begin() + std::ptrdiff_t(std::size_t(yy) * row));
            for (std::size_t i = 3; i < flipped.size(); i += 4)
                flipped[i] = 255;
            if (!write_png(from_utf8(options_.screenshot), fb_w, fb_h, flipped.data())) {
                std::fprintf(stderr, "error: cannot write %s\n", options_.screenshot.c_str());
                failed_ = true;
            }
            if (!options_.compare_view.empty())
                compare_view(flipped, fb_w, fb_h);
            if (!error_title_.empty())
                std::fprintf(stderr, "%s: %s\n", error_title_.c_str(), error_message_.c_str());
            quit_ = true;
        }
        if (frames_ > 6000) { // about 100 s: something never finished
            std::fprintf(stderr, "error: the screenshot timed out\n");
            failed_ = quit_ = true;
        }
    }
    if (!vulkan_)
        glfwSwapBuffers(window_);
}

int App::run() {
    std::string error;
    if (!init(error)) {
        std::fprintf(stderr, "error: %s\n", error.c_str());
        shutdown();
        return 1;
    }
    while (!glfwWindowShouldClose(window_) && !quit_)
        frame();
    shutdown();
    return failed_ ? 1 : 0;
}

} // namespace

int run_app(const AppOptions& options) {
    App app(options);
    return app.run();
}

} // namespace tisma::ui
