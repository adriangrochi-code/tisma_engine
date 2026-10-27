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
#include "gcode/export.hpp"
#include "gcode/print_time.hpp"
#include "io/model.hpp"
#include "io/stl.hpp"
#include "project/project.hpp"
#include "ui/camera.hpp"
#include "ui/files.hpp"
#include "ui/icons.hpp"
#include "ui/help.hpp"
#include "ui/plate.hpp"
#include "ui/png.hpp"
#include "ui/presets.hpp"
#include "ui/raytrace.hpp"
#include "ui/scene.hpp"
#include "ui/settings_layout.hpp"
#include "ui/slicing.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>
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
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string_view>
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

constexpr const char* kIcons[] = {
    "tisma_plate",      "tisma_print",     "tisma_filament", "tisma_printer",     "tisma_more",      "tisma_folder",
    "tisma_save",       "tisma_slice",     "tisma_send",     "tisma_engineering", "tisma_calibrate", "tisma_vibration",
    "tisma_view",       "tisma_view_back", "tisma_view_front", "tisma_view_left", "tisma_view_right", "tisma_view_top",
    "tisma_zoom_fit",   "tisma_zoom_in",   "tisma_zoom_out",
    "tool_add",         "tool_remove",     "tool_duplicate", "tool_move",         "tool_rotate",     "tool_scale",
    "tool_lay_flat",    "tool_arrange",     "tool_paint",
    "section_quality",  "section_strength", "section_speed", "section_support",  "section_adhesion", "section_other",
    "section_temperature", "section_cooling", "section_flow", "section_retraction", "section_needs",
    "section_machine",  "section_nozzle",  "section_limits",  "section_gcode",
};

const std::vector<std::string> kOpenExtensions = {".stl", ".obj", ".3mf", ".tisma", ".gcode", ".gco", ".g", ".json"};
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

std::string number_text(std::size_t n) {
    // Thousands with thin gaps: 297 412.
    std::string digits = std::to_string(n), out;
    for (std::size_t i = 0; i < digits.size(); ++i) {
        if (i > 0 && (digits.size() - i) % 3 == 0)
            out += ' ';
        out += digits[i];
    }
    return out;
}

std::string format(const char* fmt, ...) {
    char text[1024];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(text, sizeof text, fmt, args);
    va_end(args);
    return text;
}

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
enum class Purpose { Open, AddModel, SaveGcode, LoadSettings, SaveSettings, SaveProject, SaveExtruderTest, LoadCase };

// The plate tool in use (docs/UI.md, «Herramientas de la placa»).
enum class PlateTool { Select, Move, Rotate, Scale, LayFlat, Paint };

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
    void paint_card(ImVec2 at);
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
    void update_ray_scene();
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
    bool settings_card(const SubsectionLayout& sub, bool first, int changes, const std::string& summary);
    bool card_rows(std::string_view id, bool& bed);
    bool setting_shown(std::string_view key, std::string_view card) const;
    bool setting_applies(std::string_view key) const;
    bool card_shown(const SubsectionLayout& sub) const;

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
    ViewLevel view_level_ = ViewLevel::Normal;
    std::array<std::string, 3> panel_section_{"quality", "temperature", "machine"};
    std::map<std::string, bool> card_open_;
    std::map<std::string, float> card_heights_;
    std::set<std::string> show_all_cards_;
    std::set<std::string> changed_keys_;
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
    bool show_travels_ = false;
    ColorMode color_mode_ = ColorMode::Kind;
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
    };
    std::vector<Toast> toasts_;
    bool error_open_ = false;
    std::string error_title_, error_message_;
    std::vector<std::string> error_details_;
    bool about_open_ = false;
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
        ImGui::SetItemTooltip("%s", tip);

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

// The setting each row of the panel edits, for its info button.
const char* row_key(T label) {
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
        {T::InfillDensity, "infill.density"}, {T::InfillPattern, "infill.pattern"}, {T::InfillAngle, "infill.angle"},
        {T::SolidEvery, "infill.solid_every"}, {T::SolidBelowArea, "infill.solid_below_area"},
        {T::NonplanarInfill, "infill.nonplanar"}, {T::NonplanarAmplitude, "infill.nonplanar_amplitude"},
        {T::NonplanarWavelength, "infill.nonplanar_wavelength"}, {T::CombineInfill, "infill.combine_layers"},
        {T::CombineAuto, "infill.combine_auto"}, {T::DenseUnderTop, "infill.dense_under_top"},
        {T::DenseDensity, "infill.dense_density"}, {T::StressDensity, "infill.stress_density"},
        {T::Supports, "support.enabled"}, {T::SupportAutomatic, "support.automatic"},
        {T::SupportAngle, "support.threshold_angle"}, {T::SkirtLoops, "adhesion.skirt_loops"},
        {T::SkirtDistance, "adhesion.skirt_distance"}, {T::BrimWidth, "adhesion.brim_width"},
        {T::SpeedWalls, "speed.walls"}, {T::SpeedOuterWall, "speed.outer_wall"}, {T::SpeedInfill, "speed.infill"},
        {T::SpeedSolid, "speed.solid_infill"}, {T::SpeedTop, "speed.top_solid_infill"},
        {T::SpeedSupport, "speed.support"}, {T::SpeedTravel, "speed.travel"},
        {T::SpeedFirstLayer, "speed.first_layer"}, {T::SpeedMax, "speed.max"},
        {T::SpeedBridges, "speed.bridges"}, {T::SpeedOverBridges, "speed.over_bridges"},
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
        {T::ScarfMode, "walls.scarf"}, {T::ScarfLength, "walls.scarf_length"},
        {T::ScarfEntire, "walls.scarf_entire_loop"}, {T::ScarfStart, "walls.scarf_start_height"},
        {T::ScarfSegment, "walls.scarf_max_segment"}, {T::ScarfSmooth, "walls.scarf_only_smooth"},
        {T::ScarfInner, "walls.scarf_inner"}, {T::DetectOverhangs, "walls.detect_overhangs"},
        {T::MinLineWidth, "walls.min_line_width"}, {T::MinDetail, "walls.min_detail"},
        {T::WallGeneratorRow, "walls.generator"}, {T::WallGapFill, "walls.gap_fill"},
        {T::ZOffset, "machine.z_offset"}, {T::VolumetricE, "gcode.volumetric_e"},
        {T::VerboseComments, "gcode.verbose_comments"}, {T::GcodeArcs, "gcode.arcs"}, {T::GcodeBinary, "gcode.binary"},
        {T::ChamberMinTemp, "extrusion.chamber_min_temperature"}, {T::FilamentDensity, "extrusion.density"},
        {T::FilamentCost, "extrusion.cost"}, {T::SpoolWeight, "extrusion.spool_weight"},
        {T::ShrinkageXY, "extrusion.shrinkage_xy"}, {T::ShrinkageZ, "extrusion.shrinkage_z"},
        {T::ColorChangeGcode, "gcode.color_change"}, {T::PauseGcode, "gcode.pause"},
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
        {T::SupportZGap, "support.z_gap"}, {T::SupportXyGap, "support.xy_gap"},
        {T::SupportSpacing, "support.spacing"}, {T::SupportInterface, "support.interface_layers"},
        {T::SupportLineAngle, "support.line_angle"},
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
        {T::PressureAdvance, "extrusion.pressure_advance"}, {T::FilamentMaxFlow, "extrusion.max_flow"},
        {T::RetractMinTravel, "retraction.min_travel"}, {T::Abrasive, "extrusion.abrasive"},
        {T::UnretractSpeed, "retraction.unretract_speed"}, {T::RestartExtra, "retraction.restart_extra"},
        {T::RetractLayerChange, "retraction.on_layer_change"}, {T::WipeRow, "retraction.wipe"},
        {T::BeforeWipe, "retraction.before_wipe"}, {T::FirmwareRetraction, "retraction.firmware"},
        {T::LiftAbove, "retraction.lift_above"}, {T::LiftBelow, "retraction.lift_below"},
        {T::RampingLift, "travel.ramping_lift"}, {T::TravelSlope, "travel.slope"}, {T::MaxLift, "travel.max_lift"},
        {T::LiftBeforeObstacle, "travel.lift_before_obstacle"}, {T::AvoidCrossingWalls, "travel.avoid_crossing_walls"},
        {T::MaxDetour, "travel.max_detour"}, {T::WallsClockwise, "walls.clockwise"},
        {T::FilamentRetractionRow, "extrusion.retraction"},
        {T::StaggeredSeams, "walls.staggered_inner_seams"}, {T::TopOneWallRow, "walls.top_one_wall"},
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
        {T::BetweenObjectsGcode, "gcode.between_objects"}};
    const auto it = keys.find(label);
    return it == keys.end() ? nullptr : it->second;
}

// The setting whose info card is kept open (--info, for screenshots).
std::string g_open_info;

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
    if (hovered)
        ImGui::SetTooltip("%s", tr(T::Info));
    if (ImGui::BeginPopup("##info_card")) {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 24.0f);
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark), "%s", tr(label));
        const auto& docs = setting_docs();
        const auto doc = docs.find(key);
        if (label == T::NozzleTemp || label == T::BedTemp)
            ImGui::TextUnformatted(tr(T::InfoTemperatures));
        else if (doc != docs.end())
            ImGui::TextUnformatted(doc->second.meaning.c_str());
        ImGui::Spacing();
        ImGui::TextDisabled("%s: %s", tr(T::InfoKey), key);
        if (doc != docs.end() && !doc->second.default_value.empty())
            ImGui::TextDisabled("%s: %s", tr(T::InfoDefault), doc->second.default_value.c_str());
        if (doc != docs.end() && !doc->second.range.empty())
            ImGui::TextDisabled("%s: %s", tr(T::InfoRange), doc->second.range.c_str());
        if (language() == Language::English)
            ImGui::TextDisabled("%s", tr(T::InfoSpanish));
        ImGui::PopTextWrapPos();
        ImGui::EndPopup();
    }
    ImGui::PopID();
    ImGui::PopID();
}

// A gray tag after the next row's label: the level of a changed setting that
// the panel's level would hide (settings_panel.md, rule 12).
const char* g_row_tag = nullptr;

// Rows of the settings tables: a label (with a tooltip and, for a setting of
// the engine, its info button) and a control.
void label_cell(T label, const char* tip = nullptr) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(tr(label));
    if (tip && ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", tip);
    if (const char* key = row_key(label))
        info_button(label, key);
    if (g_row_tag) {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", g_row_tag);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", tr(T::HiddenChanged));
        g_row_tag = nullptr;
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
        ImGui::DragScalar("##v", ImGuiDataType_Double, &v, speed, &min, &max, fmt, ImGuiSliderFlags_AlwaysClamp);
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
        changed |= ImGui::DragScalar("##z", ImGuiDataType_Double, &steps[i][0], 0.05f, &z_min, &z_max, "Z %.2f mm",
                                     ImGuiSliderFlags_AlwaysClamp);
        ImGui::TableNextColumn();
        const float button = ImGui::GetFrameHeight();
        ImGui::SetNextItemWidth(-(button + ImGui::GetStyle().ItemSpacing.x));
        changed |= ImGui::DragScalar("##h", ImGuiDataType_Double, &steps[i][1], 0.002f, &h_min, &h_max, "%.3f mm",
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
        view_level_ = options_.level == "basic" ? ViewLevel::Basic : options_.level == "advanced" ? ViewLevel::Advanced
                                                                                                 : ViewLevel::Normal;
    if (const std::size_t slash = options_.section.find('/'); slash != std::string::npos) {
        const std::string tab = options_.section.substr(0, slash);
        panel_tab_ = tab == "material" ? SettingsTab::Material : tab == "printer" ? SettingsTab::Printer : SettingsTab::Print;
        panel_section_[std::size_t(panel_tab_)] = options_.section.substr(slash + 1);
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
    if (options_.color >= 0)
        color_mode_ = static_cast<ColorMode>(options_.color);
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
                                                                                         : PlateTool::LayFlat;
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
        if (j.contains("tips_seen") && j.at("tips_seen").is_array())
            for (const auto& t : j.at("tips_seen"))
                if (t.is_string())
                    tips_seen_.insert(t.get<std::string>());
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
            const std::string level = ui.value("level", std::string("normal"));
            view_level_ = level == "basic" ? ViewLevel::Basic : level == "advanced" ? ViewLevel::Advanced : ViewLevel::Normal;
            if (ui.contains("section") && ui.at("section").is_array())
                for (std::size_t t = 0; t < std::min<std::size_t>(3, ui.at("section").size()); ++t)
                    if (ui.at("section")[t].is_string())
                        panel_section_[t] = ui.at("section")[t].get<std::string>();
            if (ui.contains("cards") && ui.at("cards").is_object())
                for (const auto& [card, open] : ui.at("cards").items())
                    if (open.is_boolean())
                        card_open_[card] = open.get<bool>();
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
        j["tips_seen"] = tips_seen_;
        j["ui"]["level"] = view_level_ == ViewLevel::Basic ? "basic" : view_level_ == ViewLevel::Advanced ? "advanced" : "normal";
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
    toasts_.push_back({std::move(text), error, ImGui::GetTime() + kToastSeconds});
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
    else if (ext == ".gcode" || ext == ".gco" || ext == ".g")
        load_gcode(path);
    else if (ext == ".json")
        load_settings_file(path);
    else
        fail(T::CannotOpen, utf8(path) + ": only .stl, .obj, .3mf, .tisma, .gcode and .json files");
}

void App::load_model(const std::filesystem::path& path) {
    try {
        ModelFile stl = read_model(path);
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
        const BoundingBox box = placed_mesh(o).bounding_box();
        const Vec3 at = centre_on_bed(box, settings_.bed);
        o.place.x = 0.5 * (double(box.min.x) + double(box.max.x)) + at.x;
        o.place.y = 0.5 * (double(box.min.y) + double(box.max.y)) + at.y;
    }
    rebuild_plate();
}

// The placed meshes, the plate's mesh and the view's copy, from objects_.
void App::rebuild_plate() {
    placed_meshes_.clear();
    object_vertices_.clear();
    std::size_t vertices = 0;
    for (const PlateObject& o : objects_) {
        placed_meshes_.push_back(placed_mesh(o));
        object_vertices_.push_back(vertices);
        vertices += 3 * placed_meshes_.back().triangle_count();
    }
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
        const std::vector<float> model = mesh_vertices(*mesh_, offset_);
        renderer_.set_model(model);
        vk_view_.set_model(model);
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
    const float half = std::tan(0.5f * camera_.fov), aspect = view_size_.x / std::max(view_size_.y, 1.0f);
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
    r.preview = read_gcode_preview(r.gcode, settings_.filament_diameter);
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
    bed_changed();
    settings_changed();
    if (!p.models.empty()) {
        // Each model as the project placed it (its mesh moved by its offset).
        std::vector<PlateObject> objects;
        for (ProjectModel& m : p.models) {
            PlateObject o{m.name, translated(m.mesh, {float(m.offset[0]), float(m.offset[1]), float(m.offset[2])}), {},
                          std::move(m.painting)};
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
    for (std::size_t i = 0; i < objects_.size() && i < placed_meshes_.size(); ++i)
        p.models.push_back({objects_[i].name, placed_meshes_[i], {0.0, 0.0, 0.0}, objects_[i].painting});
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
        Settings saved = settings_;
        for (const ProfileKind kind : kProfileKinds)
            if (const Profile* p = find_profile(kind, chosen_profile_[std::size_t(kind)]))
                saved = with_profile(saved, kind, p->settings);
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
        ImGui::SetTooltip("%s", tr(T::ProfileModifiedTip));
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
        ImGui::TextDisabled("%s", tr(T::CatalogHint));
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
        ImGui::TextDisabled(tr(T::CatalogCount), found, total);

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
            ImGui::TextDisabled("%s", tr(T::CatalogEmpty));
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("##catalog_entry", ImVec2(0, -footer), ImGuiChildFlags_Borders);
        ImGui::PushTextWrapPos(0.0f);
        const auto maybe = [](const std::optional<double>& v, const char* unit) {
            char text[32];
            if (!v)
                return std::string("-");
            std::snprintf(text, sizeof text, "%g %s", *v, unit);
            return std::string(text);
        };
        const auto range = [](const std::optional<std::array<double, 2>>& r, const char* unit) {
            char text[48];
            if (!r)
                return std::string("-");
            std::snprintf(text, sizeof text, "%g–%g %s", (*r)[0], (*r)[1], unit);
            return std::string(text);
        };
        const std::vector<std::string>* sources = nullptr;
        if (catalog_pick_ >= 0 && printers) {
            const PrinterModel& p = *found_printers[std::size_t(catalog_pick_)];
            ImGui::PushFont(nullptr, kTitleSize * 0.9f);
            ImGui::Text("%s %s", p.vendor.c_str(), p.model.c_str());
            ImGui::PopFont();
            ImGui::Text("%s: %g × %g × %g mm", tr(T::BuildVolume), p.build_volume[0], p.build_volume[1], p.build_volume[2]);
            if (p.circular)
                ImGui::TextDisabled(tr(T::BedRound), p.build_volume[0]);
            const char* firmware = p.firmware == "marlin"    ? "Marlin"
                                   : p.firmware == "klipper" ? "Klipper"
                                   : p.firmware == "reprap"  ? "RepRapFirmware"
                                   : p.firmware == "bambu"   ? "Bambu Lab"
                                   : p.firmware == "other"   ? tr(T::OwnFirmware)
                                                             : "-";
            ImGui::Text("%s: %s · %s: %s%s", tr(T::Firmware), firmware, tr(T::Extruder),
                        p.direct_drive ? tr(T::DirectDrive) : tr(T::Bowden),
                        p.enclosed ? (std::string(" · ") + tr(T::Enclosed)).c_str() : "");
            ImGui::Text(tr(T::MaxTemps), maybe(p.max_nozzle_temp, "°C").c_str(), maybe(p.max_bed_temp, "°C").c_str());
            ImGui::Text(tr(T::MaxMotion), maybe(p.max_speed, "mm/s").c_str(), maybe(p.max_acceleration, "mm/s²").c_str());
            if (!p.multi_material.empty())
                ImGui::Text("%s: %s", tr(T::MultiMaterial), p.multi_material.c_str());
            // The capabilities in detail, those the maker states.
            const auto join = [](const std::vector<std::string>& items) {
                std::string out;
                for (const std::string& i : items)
                    out += (out.empty() ? "" : ", ") + i;
                return out;
            };
            const auto line = [](T label, const std::string& value) {
                if (!value.empty())
                    ImGui::Text("%s: %s", tr(label), value.c_str());
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
                ImGui::TextColored(rgb(0xE8A33D), "%s", tr(T::OwnStartGcode));
            catalog_nozzle_ = std::min(catalog_nozzle_, p.nozzles.size() - 1);
            ImGui::SetNextItemWidth(120.0f * scale_);
            char nozzle[24];
            std::snprintf(nozzle, sizeof nozzle, "%g mm", p.nozzles[catalog_nozzle_]);
            if (ImGui::BeginCombo(tr(T::NozzleSize), nozzle)) {
                for (std::size_t n = 0; n < p.nozzles.size(); ++n) {
                    std::snprintf(nozzle, sizeof nozzle, "%g mm", p.nozzles[n]);
                    if (ImGui::Selectable(nozzle, n == catalog_nozzle_))
                        catalog_nozzle_ = n;
                }
                ImGui::EndCombo();
            }
            sources = &p.sources;
        } else if (catalog_pick_ >= 0) {
            const FilamentModel& f = *found_filaments[std::size_t(catalog_pick_)];
            ImGui::PushFont(nullptr, kTitleSize * 0.9f);
            ImGui::Text("%s %s", f.vendor.c_str(), f.name.c_str());
            ImGui::PopFont();
            ImGui::Text("%s: %s · %g mm", tr(T::Material), f.material.c_str(), f.diameter);
            ImGui::Text("%s: %s%s", tr(T::NozzleRange), range(f.nozzle_temp, "°C").c_str(),
                        f.nozzle_recommended ? (" · " + maybe(f.nozzle_recommended, "°C") + " " + tr(T::Recommended)).c_str() : "");
            ImGui::Text("%s: %s%s", tr(T::BedRange), range(f.bed_temp, "°C").c_str(),
                        f.bed_recommended ? (" · " + maybe(f.bed_recommended, "°C") + " " + tr(T::Recommended)).c_str() : "");
            if (f.fan)
                ImGui::Text("%s: %s", tr(T::FanMin), range(f.fan, "%").c_str());
            if (f.density)
                ImGui::Text("%s: %s", tr(T::Density), maybe(f.density, "g/cm³").c_str());
            if (f.enclosure.value_or(false))
                ImGui::TextColored(rgb(0xE8A33D), "%s", tr(T::NeedsEnclosure));
            if (is_abrasive(f))
                ImGui::TextColored(rgb(0xE8A33D), "%s", tr(T::AbrasiveFilament));
            if (f.max_flow)
                ImGui::Text("%s: %s", tr(T::MaxFlow), maybe(f.max_flow, "mm³/s").c_str());
            if (f.chamber_temp)
                ImGui::Text("%s: %s", tr(T::ChamberHeating), range(f.chamber_temp, "°C").c_str());
            if (f.drying_temp)
                ImGui::Text("%s: %s%s", tr(T::Drying), maybe(f.drying_temp, "°C").c_str(),
                            f.drying_hours ? (", " + maybe(f.drying_hours, "h")).c_str() : "");
            if (f.soluble.value_or(false))
                ImGui::Text("%s", tr(T::Soluble));
            if (!f.hardness.empty())
                ImGui::Text("%s: %s", tr(T::Hardness), f.hardness.c_str());
            if (f.glass_transition)
                ImGui::Text("Tg: %s", maybe(f.glass_transition, "°C").c_str());
            if (f.heat_deflection)
                ImGui::Text("HDT: %s", maybe(f.heat_deflection, "°C").c_str());
            if (f.shrinkage)
                ImGui::Text("%s: %s", tr(T::Shrinkage), maybe(f.shrinkage, "%").c_str());
            if (!f.bed_surfaces.empty()) {
                std::string surfaces;
                for (const std::string& b : f.bed_surfaces)
                    surfaces += (surfaces.empty() ? "" : ", ") + b;
                ImGui::Text("%s: %s", tr(T::BedSurfaces), surfaces.c_str());
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
    if (!result_ || !result_->ok || !result_->model || viewer_ || stale_) {
        fail(T::ExportFailed, tr(T::Stale));
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
    const std::string name = model_path_.empty() ? "tisma.gcode" : utf8(model_path_.stem()) + ".gcode";
    browser_.open(FileBrowser::Mode::Save, tr(T::SaveGcode), {".gcode"}, last_folder_, name);
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
    Painting painting = plate_painting(objects_);
    // Each model's triangles in the plate's mesh, as plate_mesh joins them.
    std::vector<MeshObject> objects;
    for (std::size_t i = 0, first = 0; i < objects_.size() && i < placed_meshes_.size(); ++i) {
        objects.push_back({objects_[i].name, first, placed_meshes_[i].triangle_count(), i, 0});
        first += placed_meshes_[i].triangle_count();
    }
    slice_started_ = glfwGetTime();
    slice_made_ = revision_;
    slice_job_.start([this, placed = std::move(placed), settings = std::move(settings), load = load_case_,
                      painting = std::move(painting), objects = std::move(objects)]() {
        return slicer_.run(placed, settings, load ? &*load : nullptr, painting.empty() ? nullptr : &painting,
                           objects.size() > 1 ? &objects : nullptr);
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
    const auto start = std::chrono::steady_clock::now();
    preview_buffers_ = preview_buffers(result_->preview);
    preview_ms_ = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    const PreviewBuffers& buffers = preview_buffers_;
    ++scene_version_;
    feed_range_[0] = buffers.min_feed, feed_range_[1] = buffers.max_feed;
    width_range_[0] = buffers.min_width, width_range_[1] = buffers.max_width;
    height_range_[0] = buffers.min_height, height_range_[1] = buffers.max_height;
    renderer_.set_preview(buffers);
    vk_view_.set_preview(buffers);
    top_layer_ = int(result_->preview.layers.size()) - 1;
    bottom_layer_ = 0;
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
    ImGui::SameLine(0, gap);
    ImGui::SetCursorPosY(top);
    const bool can_export = result_ && result_->ok && !viewer_ && !stale_ && result_->check.ok();
    if (button("##export", "tisma_save", tr(T::Export), false, false, can_export,
               can_export ? tr(T::ExportTip) : tr(T::ExportNeedsSlice), button_h))
        begin_export();
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
        if (ImGui::MenuItem(tr(T::Tutorial)))
            tour_step_ = 0;
        if (ImGui::MenuItem(tr(T::TipsOn), nullptr, tips_))
            tips_ = !tips_;
        if (ImGui::MenuItem(tr(T::TipsAgain))) {
            tips_seen_.clear();
            tips_ = true;
        }
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

// Whether a setting does anything with the others as they are: the ones
// that hang on a switch hide while it is off (settings_panel.md, rule 6).
bool App::setting_applies(std::string_view key) const {
    const Settings& s = settings_;
    if (key.starts_with("support.tree_"))
        return s.support && s.support_style == SupportStyle::Organic;
    if (key.starts_with("support.") && key != "support.enabled")
        return s.support;
    if (key.starts_with("zaa.") && key != "zaa.enabled")
        return s.zaa;
    if (key == "infill.nonplanar_amplitude" || key == "infill.nonplanar_wavelength")
        return s.infill_nonplanar;
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
    const std::optional<SettingPlace> place = setting_place(key);
    const SettingLevel level = place ? place->level : SettingLevel::Advanced;
    return level_shows(view_level_, level) || show_all_cards_.contains(std::string(card)) ||
           changed_keys_.contains(std::string(key));
}

bool App::card_shown(const SubsectionLayout& sub) const {
    return std::any_of(sub.settings.begin(), sub.settings.end(),
                       [&](const SettingEntry& e) { return setting_shown(e.key, sub.id); });
}

// A card of the panel: its title, «N changes» in violet, and, folded, the
// line that sums it up; a click on the title folds or opens it. Its
// background is as tall as the card was in the last frame.
bool App::settings_card(const SubsectionLayout& sub, bool first, int changes, const std::string& summary) {
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
    if (ImGui::InvisibleButton("##head", ImVec2(width, header + 2.0f * pad * 0.85f))) {
        open = !open;
        card_open_[id] = open;
    }
    const bool hovered = ImGui::IsItemHovered();
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
    if (changes > 0) {
        const std::string count = changes == 1 ? std::string(tr(T::OneChangeCount)) : format(tr(T::ChangesCount), changes);
        const float w = ImGui::CalcTextSize(count.c_str()).x;
        dl->AddText(ImVec2(start.x + width - pad - w, start.y + pad), violet, count.c_str());
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
    refresh_profiles();
    bool changed = false, bed = false;

    // A setting asked with --info opens its tab, section and card.
    if (!g_open_info.empty())
        if (const std::optional<SettingPlace> place = setting_place(g_open_info)) {
            panel_tab_ = place->tab;
            panel_section_[std::size_t(place->tab)] = std::string(place->section->id);
            card_open_[std::string(place->subsection->id)] = true;
            view_level_ = ViewLevel::Advanced;
        }

    // The tabs, and the level on their right.
    {
        const float gap = ImGui::GetStyle().ItemSpacing.x;
        const float level_width = 118.0f * scale_;
        const float tab_width = (ImGui::GetContentRegionAvail().x - level_width - 3.0f * gap) / 3.0f;
        const std::array<std::pair<SettingsTab, T>, 3> tabs = {
            {{SettingsTab::Print, T::Print}, {SettingsTab::Material, T::Material}, {SettingsTab::Printer, T::Printer}}};
        for (const auto& [tab, title] : tabs) {
            const bool chosen = panel_tab_ == tab;
            ImGui::PushStyleColor(ImGuiCol_Button, chosen ? rgb(theme_.accent) : rgb(theme_.frame, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, chosen ? rgb(theme_.accent_hover) : rgb(theme_.frame_hover));
            ImGui::PushStyleColor(ImGuiCol_Text, chosen ? rgb(0xFFFFFF) : rgb(theme_.text_dim));
            if (ImGui::Button(tr(title), ImVec2(tab_width, 0)))
                panel_tab_ = tab;
            ImGui::PopStyleColor(3);
            ImGui::SameLine(0, gap);
        }
        // The level: a dot of its colour and its name.
        constexpr std::array<std::tuple<ViewLevel, T, T, std::uint32_t>, 3> levels = {
            {{ViewLevel::Basic, T::LevelBasic, T::LevelBasicTip, 0x3FBF7Fu},
             {ViewLevel::Normal, T::LevelNormal, T::LevelNormalTip, 0xE8B83Du},
             {ViewLevel::Advanced, T::LevelAdvanced, T::LevelAdvancedTip, 0xE5484Du}}};
        std::uint32_t dot = 0xE8B83D;
        const char* name = tr(T::LevelNormal);
        for (const auto& [level, title, tip, colour] : levels)
            if (level == view_level_)
                dot = colour, name = tr(title);
        ImGui::SetNextItemWidth(level_width);
        const std::string preview = std::string("    ") + name;
        const ImVec2 at = ImGui::GetCursorScreenPos();
        if (ImGui::BeginCombo("##level", preview.c_str())) {
            for (const auto& [level, title, tip, colour] : levels) {
                const ImVec2 row = ImGui::GetCursorScreenPos();
                const std::string item = std::string("    ") + tr(title) + "##" + std::to_string(int(level));
                if (ImGui::Selectable(item.c_str(), level == view_level_))
                    view_level_ = level, show_all_cards_.clear();
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", tr(tip));
                ImGui::GetWindowDrawList()->AddCircleFilled(
                    ImVec2(row.x + 6.0f * scale_, row.y + ImGui::GetTextLineHeight() * 0.5f), 4.0f * scale_,
                    ImGui::GetColorU32(rgb(colour)));
            }
            ImGui::EndCombo();
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", tr(T::LevelTip));
        ImGui::GetWindowDrawList()->AddCircleFilled(
            ImVec2(at.x + ImGui::GetStyle().FramePadding.x + 4.0f * scale_, at.y + ImGui::GetFrameHeight() * 0.5f),
            4.0f * scale_, ImGui::GetColorU32(rgb(dot)));
    }
    // Ctrl+Alt+1, 2 and 3 choose the level (rule 14).
    if (ImGui::GetIO().KeyCtrl && ImGui::GetIO().KeyAlt) {
        if (ImGui::IsKeyPressed(ImGuiKey_1))
            view_level_ = ViewLevel::Basic, show_all_cards_.clear();
        if (ImGui::IsKeyPressed(ImGuiKey_2))
            view_level_ = ViewLevel::Normal, show_all_cards_.clear();
        if (ImGui::IsKeyPressed(ImGuiKey_3))
            view_level_ = ViewLevel::Advanced, show_all_cards_.clear();
    }
    const ProfileKind kind = panel_tab_ == SettingsTab::Print      ? ProfileKind::Print
                             : panel_tab_ == SettingsTab::Material ? ProfileKind::Filament
                                                                   : ProfileKind::Printer;
    profile_row(kind);
    ImGui::Spacing();

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
    // The column of sections: an icon and its name, the chosen one in violet;
    // a dot where something differs from the profile.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::BeginChild("##sections", ImVec2(column, -footer), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
    for (const SectionLayout* section : sections) {
        const bool selected = section->id == chosen;
        ImGui::PushID(section->id.data(), section->id.data() + section->id.size());
        const ImVec2 at = ImGui::GetCursorScreenPos();
        const float icon_size = 22.0f * scale_, small = ImGui::GetFontSize() * 0.78f;
        const float tall = icon_size + small + 16.0f * scale_;
        if (ImGui::InvisibleButton("##section", ImVec2(column - 6.0f * scale_, tall)) && !selected) {
            chosen = std::string(section->id);
            show_all_cards_.clear();
        }
        const bool hovered = ImGui::IsItemHovered();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        if (selected || hovered)
            dl->AddRectFilled(at, ImVec2(at.x + column - 6.0f * scale_, at.y + tall),
                              ImGui::GetColorU32(rgb(selected ? theme_.accent : theme_.frame_hover, selected ? 0.28f : 0.6f)),
                              7.0f * scale_);
        const ImU32 tint = ImGui::GetColorU32(rgb(selected ? theme_.lavender : theme_.text_dim));
        const float cx = at.x + (column - 6.0f * scale_) * 0.5f;
        dl->AddImage(icon(std::string(section->icon.substr(0, section->icon.size() - 4)).c_str()),
                     ImVec2(cx - icon_size * 0.5f, at.y + 6.0f * scale_),
                     ImVec2(cx + icon_size * 0.5f, at.y + 6.0f * scale_ + icon_size), ImVec2(0, 0), ImVec2(1, 1), tint);
        const char* name = tr(section->title);
        const ImVec2 text = ImGui::GetFont()->CalcTextSizeA(small, FLT_MAX, 0.0f, name);
        dl->AddText(ImGui::GetFont(), small, ImVec2(cx - text.x * 0.5f, at.y + 10.0f * scale_ + icon_size), tint, name);
        const bool marked = std::any_of(section->subsections.begin(), section->subsections.end(), [&](const SubsectionLayout& sub) {
            return std::any_of(sub.settings.begin(), sub.settings.end(),
                               [&](const SettingEntry& e) { return changed_keys_.contains(std::string(e.key)); });
        });
        if (marked)
            dl->AddCircleFilled(ImVec2(at.x + column - 16.0f * scale_, at.y + 8.0f * scale_), 3.5f * scale_,
                                ImGui::GetColorU32(rgb(theme_.accent_hover)));
        if (hovered) {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(name);
            for (const SubsectionLayout& sub : section->subsections)
                if (card_shown(sub))
                    ImGui::BulletText("%s", tr(sub.title));
            ImGui::EndTooltip();
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::SameLine(0, 4.0f * scale_);

    // The chosen section's cards.
    ImGui::BeginChild("##cards", ImVec2(0, -footer));
    for (const SectionLayout* section : sections) {
        if (section->id != chosen)
            continue;
        bool first = true;
        for (const SubsectionLayout& sub : section->subsections) {
            if (!card_shown(sub))
                continue;
            const int changes = int(std::count_if(sub.settings.begin(), sub.settings.end(), [&](const SettingEntry& e) {
                return changed_keys_.contains(std::string(e.key));
            }));
            const ImVec2 start = ImGui::GetCursorScreenPos();
            if (settings_card(sub, first, changes, subsection_summary(sub.id, settings_))) {
                ImGui::PushID(sub.id.data(), sub.id.data() + sub.id.size());
                changed |= card_rows(sub.id, bed);
                // Basic tells what it hides (rule 11).
                int hidden = 0;
                for (const SettingEntry& e : sub.settings)
                    hidden += setting_applies(e.key) && !setting_shown(e.key, sub.id);
                if (hidden > 0) {
                    ImGui::Spacing();
                    ImGui::TextDisabled("%s", format(tr(view_level_ == ViewLevel::Basic ? T::MoreSettings
                                                                                        : T::MoreSettingsAdvanced),
                                                     hidden)
                                                  .c_str());
                    ImGui::SameLine();
                    if (ImGui::SmallButton(tr(T::ShowAll)))
                        show_all_cards_.insert(std::string(sub.id));
                }
                ImGui::PopID();
            }
            const float end = ImGui::GetCursorScreenPos().y + 4.0f * scale_;
            card_heights_[std::string(sub.id)] = end - start.y;
            ImGui::SetCursorScreenPos(ImVec2(start.x, end + 7.0f * scale_));
            ImGui::Dummy(ImVec2(0, 0));
            first = false;
        }
    }
    ImGui::EndChild();

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
        if (!setting_shown(key, id))
            return false;
        const std::optional<SettingPlace> place = setting_place(key);
        if (place && !level_shows(view_level_, place->level) && !show_all_cards_.contains(std::string(id)))
            g_row_tag = tr(place->level == SettingLevel::Normal ? T::LevelNormal : T::LevelAdvanced);
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
        ImGui::TextDisabled("%s", format(tr(T::ListCount), int(n)).c_str());
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
        if (shown("walls.min_line_width"))
            changed |= row(T::MinLineWidth, s.wall_min_line_width.value, 0.0, s.wall_min_line_width.percent ? 1000.0 : 40.0,
                           s.wall_min_line_width.percent ? "%.0f %%" : "%.2f mm", s.wall_min_line_width.percent ? 0.5f : 0.01f);
        if (shown("walls.min_detail"))
            changed |= row(T::MinDetail, s.wall_min_detail.value, 0.0, s.wall_min_detail.percent ? 1000.0 : 40.0,
                           s.wall_min_detail.percent ? "%.0f %%" : "%.2f mm", s.wall_min_detail.percent ? 0.5f : 0.01f);
        if (shown("walls.generator"))
            changed |= row(T::WallGeneratorRow, s.wall_generator,
                           {{WallGenerator::Variable, tr(T::WallVariable)}, {WallGenerator::Fixed, tr(T::WallFixed)}});
        flag(T::WallGapFill, "walls.gap_fill", s.wall_gap_fill);
        flag(T::WallsClockwise, "walls.clockwise", s.walls_clockwise);
    } else if (id == "walls_more") {
        flag(T::StaggeredSeams, "walls.staggered_inner_seams", s.staggered_inner_seams);
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
        mm(T::SpeedGapFill, "speed.gap_fill", s.gap_fill_speed, 0.0, 1000.0, s.gap_fill_speed > 0.0 ? "%.0f mm/s" : "—",
           0.5f);
    } else if (id == "infill_support") {
        mm(T::SpeedInfill, "speed.infill", s.infill_speed, 1.0, 1000.0, "%.0f mm/s", 0.5f);
        mm(T::SpeedSupport, "speed.support", s.support_speed, 1.0, 1000.0, "%.0f mm/s", 0.5f);
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
        if (shown("support.style"))
            changed |= row(T::SupportStyle, s.support_style,
                           {{SupportStyle::Grid, tr(T::SupportGrid)}, {SupportStyle::Organic, tr(T::SupportOrganic)}},
                           tr(T::SupportStyleTip));
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
        if (shown("support.xy_gap"))
            changed |= row(T::SupportXyGap, s.support_xy_gap.value, 0.0, s.support_xy_gap.percent ? 1000.0 : 10.0,
                           s.support_xy_gap.percent ? "%.0f %%" : "%.2f mm", s.support_xy_gap.percent ? 0.5f : 0.01f);
    } else if (id == "interface") {
        whole(T::SupportInterface, "support.interface_layers", s.support_interface_layers, 0, 20);
    } else if (id == "pattern") {
        mm(T::SupportSpacing, "support.spacing", s.support_spacing, 0.5, 20.0, "%.2f mm", 0.01f);
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
    } else if (id == "short_layers") {
        mm(T::MinLayerTime, "cooling.min_layer_time", s.min_layer_time, 0.0, 120.0, "%.0f s", 0.2f);
        mm(T::MinSpeed, "cooling.min_speed", s.min_print_speed, 0.0, 100.0, "%.0f mm/s", 0.5f);
        flag(T::KeepOuterSpeed, "cooling.keep_outer_speed", s.keep_outer_speed, tr(T::KeepOuterSpeedTip));
    } else if (id == "flow") {
        mm(T::FlowRow, "extrusion.flow", s.flow, 10.0, 300.0, "%.1f %%", 0.05f, tr(T::FlowTip));
        mm(T::BridgeFlowRatio, "extrusion.bridge_flow_ratio", s.bridge_flow_ratio, 0.1, 2.0, "%.2f", 0.01f);
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
    } else if (id == "material_retraction") {
        count(T::FilamentRetractionRow, "extrusion.retraction", s.filament_retraction.size());
    } else if (id == "needs") {
        flag(T::Abrasive, "extrusion.abrasive", s.abrasive);
        flag(T::NeedsEnclosure, "extrusion.needs_enclosure", s.needs_enclosure);
    }
    // Printer.
    else if (id == "bed_volume") {
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
                ImGui::TextDisabled("%zu", s.bed.size());
            }
        }
        mm(T::MaxHeight, "machine.max_height", s.max_height, 0.0, 2000.0, "%.0f mm", 0.5f);
        mm(T::ZOffset, "machine.z_offset", s.z_offset, -10.0, 10.0, "%.2f mm", 0.005f);
        mm(T::ClearanceRadius, "sequential.clearance_radius", s.clearance_radius, 0.0, 1000.0, "%.0f mm", 0.5f);
        mm(T::ClearanceHeight, "sequential.clearance_height", s.clearance_height, 0.0, 1000.0, "%.0f mm", 0.5f);
    } else if (id == "enclosure") {
        flag(T::Enclosed, "machine.enclosed", s.enclosed);
        mm(T::MaxChamberTemp, "machine.max_chamber_temp", s.max_chamber_temp, 0.0, 120.0, "%.0f °C", 0.5f);
    } else if (id == "stability") {
        mm(T::NozzleForce, "review.nozzle_force", s.review_nozzle_force, 0.0, 1000.0, "%.3f N", 0.001f);
        mm(T::BedAdhesion, "review.bed_adhesion", s.review_bed_adhesion, 0.0, 1000.0, "%.3f N/mm²", 0.001f);
        mm(T::MaxDeflection, "review.max_deflection", s.review_max_deflection, 0.001, 10.0, "%.3f mm", 0.001f);
    } else if (id == "nozzle_size") {
        mm(T::Nozzle, "machine.nozzle_diameter", s.nozzle_diameter, 0.1, 2.0, "%.2f mm", 0.005f);
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
    // The templates below the table, full width (gcode_templates.md).
    if (id == "gcode_templates") {
        const std::pair<T, std::pair<const char*, std::string*>> texts[] = {
            {T::FilamentStartGcode, {"extrusion.start_gcode", &s.filament_start_gcode}},
            {T::FilamentEndGcode, {"extrusion.end_gcode", &s.filament_end_gcode}},
            {T::ColorChangeGcode, {"gcode.color_change", &s.gcode_color_change}},
            {T::PauseGcode, {"gcode.pause", &s.gcode_pause}},
            {T::BeforeLayerGcode, {"gcode.before_layer", &s.gcode_before_layer}},
            {T::AfterLayerGcode, {"gcode.after_layer", &s.gcode_after_layer}},
            {T::BetweenObjectsGcode, {"gcode.between_objects", &s.gcode_between_objects}}};
        for (const auto& [label, key_text] : texts)
            if (shown(key_text.first)) {
                ImGui::TextUnformatted(tr(label));
                ImGui::PushID(key_text.first);
                changed |= ImGui::InputTextMultiline("##text", key_text.second,
                                                     ImVec2(-9.0f * scale_, ImGui::GetTextLineHeight() * 3));
                ImGui::PopID();
            }
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
        ImGui::Text("%s  ·  %s %s  ·  %.1f × %.1f × %.1f mm", name.c_str(),
                    number_text(mesh_->triangle_count()).c_str(), tr(T::Triangles), double(size.x), double(size.y),
                    double(size.z));
    } else {
        ImGui::TextDisabled("%s", tr(T::NoModel));
    }
    ImGui::SameLine(0, 30.0f * scale_);
    if (slice_job_.running()) {
        ImGui::TextColored(rgb(theme_.accent_hover), "%s %.1f s", tr(T::Slicing), glfwGetTime() - slice_started_);
    } else if (result_ && viewer_) {
        ImGui::TextDisabled("%s", tr(T::ViewerOnly));
    } else if (result_ && stale_) {
        ImGui::TextColored(rgb(0xE8A33D), "%s", tr(T::Stale));
    } else if (result_) {
        const double total = result_->slice_seconds + result_->gcode_seconds + result_->check_seconds;
        ImGui::Text("%s", format(tr(result_->staged ? T::GcodeAgainIn : T::SlicedIn), total).c_str());
        ImGui::SameLine(0, 16.0f * scale_);
        if (result_->check.ok())
            ImGui::TextColored(rgb(0x3FBF7F), "%s",
                               format(tr(T::GcodeOk), number_text(result_->check.stats.lines).c_str()).c_str());
        else
            ImGui::TextColored(rgb(0xE5484D), "%s", format(tr(T::GcodeBad), int(result_->check.problem_count)).c_str());
        // The slicing's warnings (thin details, short layers, what the
        // machine cannot give the filament), listed when hovered.
        if (!result_->warnings.empty()) {
            ImGui::SameLine(0, 16.0f * scale_);
            ImGui::TextColored(rgb(0xE8A33D), "%s", format(tr(T::Warnings), int(result_->warnings.size())).c_str());
            if (ImGui::IsItemHovered() && ImGui::BeginTooltip()) {
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 36.0f);
                for (std::size_t i = 0; i < result_->warnings.size() && i < 20; ++i)
                    ImGui::BulletText("%s", result_->warnings[i].message.c_str());
                if (result_->warnings.size() > 20)
                    ImGui::TextDisabled("…");
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }
        }
    } else {
        ImGui::TextDisabled("%s", tr(T::Ready));
    }
    // On the right: the 3D view's time per frame on the GPU, and what sliced.
    std::string right;
    if (view_ms_ > 0.0)
        right = format(tr(T::ViewTime), view_ms_, 1000.0 / std::max(view_ms_, 0.01));
    if (result_ && !viewer_)
        right += (right.empty() ? "" : "  ·  ") + (result_->gpu.empty() ? std::string("CPU") : "GPU: " + result_->gpu);
    if (!right.empty()) {
        ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::CalcTextSize(right.c_str()).x - 12.0f * scale_);
        ImGui::TextDisabled("%s", right.c_str());
        if (view_ms_ > 0.0 && ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", tr(T::ViewTimeTip));
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
        const float pixels = brush_radius_ / (*brush_depth_ * std::tan(0.5f * camera_.fov)) * 0.5f * view_size_.y;
        ImGui::GetWindowDrawList()->AddCircle(io.MousePos, pixels, IM_COL32(255, 255, 255, 200), 48, 1.5f * scale_);
    }
    if (editing && !painting && ImGui::IsItemActivated() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        pressed_object_ = -1;
        dragged_ = false;
        const Vec3 d = mouse_ray(), e = camera_.eye();
        if (const auto hit = pick(placed_meshes_, plate_indexes(), e, d)) {
            selected_ = int(hit->object);
            if (tool_ == PlateTool::LayFlat) {
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
        }
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.0f) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f))
            camera_.pan(io.MouseDelta.x, io.MouseDelta.y, view_size_.y);
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
        if (io.MouseWheel != 0.0f)
            camera_.zoom(io.MouseWheel);
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            frame_all();
    }
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
        ImGui::TextDisabled("%s", tr(T::Render));
        for (int m = 0; m < 3; ++m) {
            if (ImGui::RadioButton(tr(modes[m]), int(render_mode_) == m))
                render_mode_ = static_cast<RenderMode>(m);
            ImGui::SetItemTooltip("%s", tr(mode_tips[m]));
        }
        if (render_mode_ != RenderMode::Fast) {
            ImGui::SetNextItemWidth(200.0f * scale_);
            ImGui::SliderFloat(tr(T::AoStrength), &ao_strength_, 0.0f, 1.5f, "%.2f");
        }
        if (render_mode_ == RenderMode::RayTraced) {
            if (ray_failed_)
                ImGui::TextColored(rgb(0xE5484D), "%s", tr(T::RayTooBig));
            else if (!renderer_.has_ray_scene())
                ImGui::TextColored(rgb(theme_.accent_hover), "%s", tr(T::RayPreparing));
            else
                ImGui::TextDisabled(tr(T::RaySamples), renderer_.ray_samples(), 256);
        }
        ImGui::Separator();
        ImGui::TextDisabled("%s", tr(T::Antialiasing));
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
            ImGui::SetTooltip("%s", tr(T::MoireFilterTip));
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2);
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

// The plate's tools (docs/UI.md, «Herramientas de la placa»): a column at
// the left of the view, and next to it the chosen tool's numbers.
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
                 {"##lay", "tool_lay_flat", T::ToolLayFlatTip, PlateTool::LayFlat}};
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
    const ImVec2 strip_min = ImGui::GetWindowPos(), strip_size = ImGui::GetWindowSize();
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
    ImGui::TextColored(rgb(theme_.accent_hover), "%s", o.name.c_str());
    const float field = 110.0f * scale_;
    bool changed = false;
    const auto number = [&](const char* label, double& v, double speed, double lo, double hi, const char* fmt) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::SameLine(70.0f * scale_);
        ImGui::SetNextItemWidth(field);
        const bool edited = ImGui::DragScalar((std::string("##") + label).c_str(), ImGuiDataType_Double, &v, float(speed), &lo,
                                              &hi, fmt, ImGuiSliderFlags_AlwaysClamp);
        changed |= edited;
        return edited;
    };
    if (tool_ == PlateTool::Move) {
        ImGui::TextDisabled("%s", tr(T::ToolMoveHint));
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
            changed |= ImGui::DragScalar("##mm", ImGuiDataType_Double, &mm[k], 0.1f, &lo, &hi, "%.2f mm",
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
    } else if (tool_ == PlateTool::LayFlat) {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 260.0f * scale_);
        ImGui::TextDisabled("%s", tr(T::ToolLayFlatHint));
        ImGui::PopTextWrapPos();
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
    if (changed)
        plate_changed();
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
    ImGui::TextColored(rgb(theme_.accent_hover), "%s", tr(T::ToolPaint));
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
            ImGui::SetItemTooltip("%s", tr(T::PaintLater));
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
    ImGui::SliderFloat(tr(T::BrushSize), &brush_radius_, 0.2f, 20.0f, "%.1f mm", ImGuiSliderFlags_Logarithmic);
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
    ImGui::TextDisabled("%s", tr(T::PaintHint));
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
    ImGui::TextColored(rgb(theme_.accent_hover), "%s", tr(title));
    ImGui::PopFont();
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(tr(text));
    ImGui::PopTextWrapPos();
    ImGui::Spacing();
    if (tour) {
        ImGui::TextDisabled("%d / %d", tour_step_ + 1, int(kTour.size()));
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
    ImGui::TextColored(rgb(theme_.text_dim), "%s", text);
    ImGui::PopFont();
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
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

    // The layer slider, on the right.
    const float margin = 14.0f * scale_;
    const float slider_top = 64.0f * scale_;
    const ImGuiStyle& style = ImGui::GetStyle();
    const float slider_h = view_size_.y - slider_top - margin - 2.0f * ImGui::GetTextLineHeightWithSpacing() -
                           2.0f * style.WindowPadding.y - style.ItemSpacing.y;
    ImGui::SetNextWindowPos(ImVec2(view_pos_.x + view_size_.x - margin, view_pos_.y + slider_top), ImGuiCond_Always,
                            ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.85f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f * scale_);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, rgb(theme_.panel));
    ImGui::Begin("##layers", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    const std::string top_text = std::to_string(top_layer_ + 1);
    const float slider_w = 30.0f * scale_;
    const float text_w = ImGui::CalcTextSize("00000").x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (text_w - ImGui::CalcTextSize(top_text.c_str()).x) * 0.5f);
    ImGui::TextUnformatted(top_text.c_str());
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (text_w - slider_w) * 0.5f);
    int shown = top_layer_ + 1;
    ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, 18.0f * scale_);
    if (ImGui::VSliderInt("##top_layer", ImVec2(slider_w, std::max(slider_h, 80.0f * scale_)), &shown, 1, layers, ""))
        top_layer_ = shown - 1;
    ImGui::PopStyleVar();
    if (ImGui::IsItemHovered() && ImGui::GetIO().MouseWheel != 0.0f)
        top_layer_ = std::clamp(top_layer_ + int(ImGui::GetIO().MouseWheel), 0, layers - 1);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (text_w - ImGui::CalcTextSize("1").x) * 0.5f);
    ImGui::TextDisabled("1");
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    if (only_layer_)
        bottom_layer_ = top_layer_;
    else
        bottom_layer_ = 0;

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
    ImGui::Text(tr(T::LayerOf), top_layer_ + 1, layers, layer.z);
    if (calibration_ && !viewer_)
        for (const CalibrationSegment& seg : calibration_->segments)
            if (layer.z > seg.z0 + 1e-6 && layer.z <= seg.z1 + 1e-6) {
                const std::string value = format("%g %s", seg.value, calibration_->unit == "C" ? "°C" : calibration_->unit.c_str());
                ImGui::TextColored(rgb(theme_.accent_hover), tr(T::SegmentAt), value.c_str());
            }
    ImGui::Checkbox(tr(T::OnlyThisLayer), &only_layer_);
    ImGui::Separator();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(tr(T::ColorBy));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::CalcTextSize(tr(T::ByKind)).x + ImGui::GetFrameHeight() * 2.0f);
    int mode = int(color_mode_);
    const char* modes[] = {tr(T::ByKind), tr(T::BySpeed), tr(T::ByWidth), tr(T::ByHeight)};
    if (ImGui::Combo("##colour_mode", &mode, modes, 4))
        color_mode_ = static_cast<ColorMode>(mode);
    const float swatch = ImGui::GetTextLineHeight();
    if (color_mode_ == ColorMode::Kind) {
        for (std::size_t k = 1; k < kLineKinds; ++k) {
            if (p.kind_moves[k] == 0)
                continue;
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
            ImGui::PopID();
        }
    } else {
        // The colour ramp with its ends.
        const float w = 200.0f * scale_;
        const ImVec2 at = ImGui::GetCursorScreenPos();
        const ImU32 stops[5] = {IM_COL32(43, 84, 242, 255), IM_COL32(26, 204, 217, 255), IM_COL32(77, 217, 89, 255),
                                IM_COL32(250, 217, 51, 255), IM_COL32(235, 56, 51, 255)};
        for (int i = 0; i < 4; ++i)
            ImGui::GetWindowDrawList()->AddRectFilledMultiColor(
                ImVec2(at.x + w * float(i) / 4.0f, at.y), ImVec2(at.x + w * float(i + 1) / 4.0f, at.y + swatch),
                stops[i], stops[i + 1], stops[i + 1], stops[i]);
        ImGui::Dummy(ImVec2(w, swatch));
        const bool speed = color_mode_ == ColorMode::Speed;
        const float* range = speed ? feed_range_ : color_mode_ == ColorMode::Height ? height_range_ : width_range_;
        const char* unit = speed ? "%.0f mm/s" : "%.2f mm";
        const std::string low = format(unit, double(range[0]));
        const std::string high = format(unit, double(range[1]));
        const float left = ImGui::GetCursorPosX();
        ImGui::TextUnformatted(low.c_str());
        ImGui::SameLine(left + w - ImGui::CalcTextSize(high.c_str()).x);
        ImGui::TextUnformatted(high.c_str());
    }
    bool travel = show_travels_;
    if (ImGui::Checkbox("##travel", &travel))
        show_travels_ = travel;
    ImGui::SameLine();
    {
        const ImVec2 at = ImGui::GetCursorScreenPos();
        const float y = at.y + ImGui::GetFrameHeight() * 0.5f;
        ImGui::GetWindowDrawList()->AddLine(ImVec2(at.x, y), ImVec2(at.x + swatch, y),
                                            ImGui::GetColorU32(rgb(kind_colour(LineKind::Travel))), 2.0f * scale_);
        ImGui::Dummy(ImVec2(swatch, swatch));
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(tr(T::KindTravel));
    }
    ImGui::Separator();
    if (ImGui::BeginTable("##numbers", 2)) {
        auto line = [](const char* name, const std::string& value) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%s", name);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(value.c_str());
        };
        if (p.estimated_seconds >= 0.0)
            line(tr(T::EstimatedTime), format_duration(p.estimated_seconds));
        const double area = 3.14159265358979 * 0.25 * result_->settings.filament_diameter *
                            result_->settings.filament_diameter;
        line(tr(T::FilamentUsed),
             format("%.2f m  ·  %.1f cm³", p.filament / 1000.0, p.filament * area / 1000.0));
        line(tr(T::Layers), std::to_string(layers));
        line(tr(T::Lines), number_text(p.lines));
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
    ImGui::TextDisabled("%s", tr(T::AnalysisHint));
    ImGui::Separator();
    if (!mesh_) {
        ImGui::TextUnformatted(tr(T::AnalysisNeedsModel));
    } else if (voxel_job_.running() || !voxels_) {
        ImGui::TextColored(rgb(theme_.accent_hover), "%s", tr(T::Estimating));
    } else if (!voxels_->ok) {
        ImGui::TextColored(rgb(0xE5484D), "%s", voxels_->error.c_str());
    } else {
        const auto card = [&](const char* id, T title, const VoxelCount& c) {
            ImGui::PushID(id);
            ImGui::TextColored(rgb(theme_.accent_hover), "%s", tr(title));
            if (!c.ok) {
                ImGui::TextColored(rgb(0xE5484D), "%s", c.error.c_str());
                ImGui::PopID();
                return;
            }
            const bool counted = c.printed_voxels > 0;
            ImGui::PushFont(nullptr, kTitleSize * 1.25f);
            ImGui::TextUnformatted(number_text(counted ? c.printed_voxels : c.grid_voxels).c_str());
            ImGui::PopFont();
            ImGui::TextDisabled("%s", tr(counted ? T::Voxels : T::GridVoxels));
            if (counted && ImGui::BeginTable("##card", 2)) {
                auto line = [](const char* name, const std::string& value) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextDisabled("%s", name);
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(value.c_str());
                };
                line(tr(T::GridVoxels), number_text(c.grid_voxels));
                line(tr(T::VoxelSize), format("%.3f mm  ·  %d / %s", c.voxel, c.rows_per_layer, tr(T::Layer)));
                line(tr(T::Memory), format("%.0f MB", c.memory_mb));
                line(tr(T::ApproxTime), format_duration(c.seconds));
                ImGui::EndTable();
            }
            if (c.over_limit)
                ImGui::TextColored(rgb(0xE8A33D), "%s", format(tr(T::OverLimit), number_text(c.limit).c_str()).c_str());
            ImGui::PopID();
        };
        card("normal", T::QualityNormal, voxels_->normal);
        ImGui::Separator();
        card("high", T::QualityHigh, voxels_->high);
    }
    ImGui::Separator();
    ImGui::TextDisabled("%s", tr(T::AnalysisCli));
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
    ImGui::TextDisabled("%s", tr(T::CalibrationHint));
    const float field = 110.0f * scale_;
    const auto number = [&](T label, double& v, double lo, double hi, const char* fmt, float speed) {
        ImGui::SetNextItemWidth(field);
        ImGui::PushID(&v);
        ImGui::DragScalar(tr(label), ImGuiDataType_Double, &v, speed, &lo, &hi, fmt, ImGuiSliderFlags_AlwaysClamp);
        ImGui::PopID();
    };
    const auto unit_text = [](const CalibrationPrint& c, double v) {
        char text[32];
        if (c.unit.empty())
            std::snprintf(text, sizeof text, "%g", v);
        else
            std::snprintf(text, sizeof text, "%g %s", v, c.unit == "C" ? "°C" : c.unit.c_str());
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
    };
    const float button_h = 30.0f * scale_;
    // A card's title and what it is for, with its button on the right.
    const auto card_head = [&](T title, T what, bool active, const char* label, bool enabled) {
        const float create_w = ImGui::CalcTextSize(label).x + button_h * 1.6f;
        const float row_right = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
        ImGui::BeginGroup();
        ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x - create_w - 8.0f * scale_);
        ImGui::TextColored(rgb(active ? theme_.accent_hover : theme_.text), "%s", tr(title));
        ImGui::TextDisabled("%s", tr(what));
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
        ImGui::PushID(int(card.kind));
        ImGui::Separator();
        const bool active = calibration_ && calibration_kind_ == card.kind;
        if (card_head(card.title, card.what, active, tr(T::Create), !slice_job_.running()))
            create_calibration(card.kind, false);

        if (active) {
            const CalibrationPrint& c = *calibration_;
            // 1: created and sliced.
            ImGui::TextColored(rgb(theme_.accent_hover), "%s", tr(T::StepCreated));
            ImGui::SameLine();
            if (slice_job_.running())
                ImGui::TextDisabled("%s", tr(T::Slicing));
            else if (result_ && !stale_ && !viewer_) {
                const std::string time = result_->preview.estimated_seconds >= 0.0
                                             ? format_duration(result_->preview.estimated_seconds)
                                             : std::string("-");
                if (is_tower(card.kind))
                    ImGui::TextDisabled(tr(T::TowerReady), int(c.segments.size()), time.c_str());
                else
                    ImGui::TextDisabled(tr(T::PartReady), time.c_str());
            }
            const bool can_export = result_ && result_->ok && !viewer_ && !stale_ && result_->check.ok();
            if (button("##export_calibration", "tisma_save", tr(T::Export), false, false, can_export, tr(T::ExportTip),
                       button_h))
                begin_export();
            // 2: print and look.
            ImGui::TextColored(rgb(theme_.accent_hover), "%s", tr(T::StepPrint));
            ImGui::TextDisabled("%s", tr(card.hint));
            if (card.kind == CalibrationKind::Dimensions) {
                // 3: measure.
                ImGui::TextColored(rgb(theme_.accent_hover), "%s", tr(T::StepMeasure));
                number(T::MeasuredX, measured_.x, 1.0, 1000.0, "%.3f mm", 0.005f);
                number(T::MeasuredY, measured_.y, 1.0, 1000.0, "%.3f mm", 0.005f);
                number(T::MeasuredZ, measured_.z, 1.0, 1000.0, "%.3f mm", 0.005f);
                number(T::MeasuredHole, measured_.hole, 0.5, 1000.0, "%.3f mm", 0.005f);
                const Compensation next = compensation_from(dimension_test_, measured_, settings_);
                if (ImGui::BeginTable("##compensation", 3, ImGuiTableFlags_SizingStretchProp)) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TableNextColumn();
                    ImGui::TextDisabled("%s", tr(T::Current));
                    ImGui::TableNextColumn();
                    ImGui::TextDisabled("%s", tr(T::New));
                    const auto line = [](const char* name, double now, double then, const char* fmt) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(name);
                        ImGui::TableNextColumn();
                        ImGui::Text(fmt, now);
                        ImGui::TableNextColumn();
                        ImGui::Text(fmt, then);
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
                ImGui::TextColored(rgb(theme_.accent_hover), "%s", tr(T::StepMeasure));
                number(T::MeasuredBottom, elephant_bottom_, 1.0, 1000.0, "%.3f mm", 0.005f);
                number(T::MeasuredBody, elephant_body_, 1.0, 1000.0, "%.3f mm", 0.005f);
                const double next = elephant_foot_from(elephant_bottom_, elephant_body_, settings_);
                ImGui::Text("%s: %.3f mm (%s) · %.3f mm (%s)", tr(T::FirstLayerShrink), settings_.first_layer_shrink,
                            tr(T::Current), next, tr(T::New));
                if (button("##apply_shrink", nullptr, tr(T::ApplyShrink), true, false, true, nullptr, button_h)) {
                    settings_.first_layer_shrink = next;
                    elephant_bottom_ = elephant_body_; // printed with this shrink, the bottom would match the body
                    settings_changed();
                    notify(format(tr(T::ShrinkApplied), next));
                }
            } else if (card.kind == CalibrationKind::FirstLayer) {
                // 3: what each look means; the height is set on the printer.
                ImGui::TextColored(rgb(theme_.accent_hover), "%s", tr(T::StepJudge));
                ImGui::TextUnformatted(tr(T::FirstLayerJudge));
            } else if (card.kind == CalibrationKind::Fit) {
                // 3: the least clearance that comes free, for the user's designs.
                ImGui::TextColored(rgb(theme_.accent_hover), "%s", tr(T::StepChooseFit));
                chips(c);
                if (chosen_value_)
                    ImGui::TextColored(rgb(theme_.accent_hover), tr(T::FitAdvice), unit_text(c, *chosen_value_).c_str());
            } else {
                // 3: pick the best segment and use its value.
                ImGui::TextColored(rgb(theme_.accent_hover), "%s", tr(T::StepChoose));
                chips(c);
                if (chosen_value_) {
                    const std::string value = unit_text(c, *chosen_value_);
                    if (card.kind == CalibrationKind::Speed)
                        ImGui::TextDisabled(tr(T::VolumetricFlow), value.c_str(),
                                            volumetric_flow(settings_, *chosen_value_));
                    if (button("##use", nullptr, format(tr(T::UseValue), value.c_str()).c_str(), true, false, true,
                               nullptr, button_h)) {
                        apply_calibration(card.kind, *chosen_value_, settings_);
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

    // The extruder's steps: no part to slice, a G-code to run and a sum.
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
        ImGui::TextColored(rgb(theme_.accent_hover), "%s", tr(T::ExtruderStep1));
        if (button("##export_test", "tisma_save", tr(T::ExportTest), false, false, true, nullptr, button_h)) {
            purpose_ = Purpose::SaveExtruderTest;
            browser_.open(FileBrowser::Mode::Save, tr(T::ExportTest), {".gcode"}, last_folder_,
                          "tisma_extruder_steps.gcode");
        }
        ImGui::TextColored(rgb(theme_.accent_hover), "%s", tr(T::ExtruderStep2));
        number(klipper ? T::CurrentRotation : T::CurrentSteps, extruder_current_, 0.1, 10000.0, "%.3f", 0.01f);
        number(T::Remaining, extruder_left_, 0.0, 120.0, "%.1f mm", 0.05f);
        const double extruded = 120.0 - extruder_left_;
        const double value = extruder_steps_from(settings_.flavor, extruder_current_, 100.0, extruded);
        ImGui::TextColored(rgb(theme_.accent_hover), "%s", tr(T::ExtruderStep3));
        ImGui::Text(tr(T::ExtruderResult), extruded, value);
        std::string text = extruder_steps_text(settings_.flavor, value);
        const float two_lines = ImGui::GetTextLineHeight() * 2.0f + ImGui::GetStyle().FramePadding.y * 2.0f + 4.0f * scale_;
        ImGui::InputTextMultiline("##steps_text", &text, ImVec2(-FLT_MIN, two_lines), ImGuiInputTextFlags_ReadOnly);
        if (button("##copy", nullptr, tr(T::Copy), false, false, true, nullptr, button_h)) {
            ImGui::SetClipboardText(text.c_str());
            notify(tr(T::Copied));
        }
        ImGui::TextDisabled("%s", tr(T::ExtruderWhere));
    }
    ImGui::PopID();
    ImGui::Separator();
    if (ImGui::CollapsingHeader(tr(T::MoreCalibrations)))
        ImGui::TextDisabled("%s", tr(T::MoreCalibrationsList));
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
                         ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs);
        ImGui::TextColored(ImVec4(1, 1, 1, 1), "%s", it->text.c_str());
        y -= ImGui::GetWindowHeight() + 8.0f * scale_;
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    }
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
                ImGui::TextColored(rgb(0xE5484D), "%s", tr(T::ProfileIsBuiltin));
            else if (exists)
                ImGui::TextDisabled("%s", tr(T::ProfileReplaces));
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
        ImGui::Text(tr(T::AboutText), TISMA_VERSION);
        ImGui::EndGroup();
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 480.0f * scale_);
        ImGui::TextDisabled("%s", tr(T::AboutLibraries));
        ImGui::PopTextWrapPos();
        if (ImGui::Button(tr(T::Close), ImVec2(120.0f * scale_, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

void App::layout() {
    viewport();
    top_bar();
    settings_panel();
    status_bar();
    view_tools();
    if (tab_ == Tab::Prepare && !objects_.empty() && !calibration_ && !viewer_)
        plate_tools();
    if (!mesh_ && !result_ && tab_ != Tab::Calibration && tab_ != Tab::Device) // those panels have their own buttons
        empty_hint();
    if (tab_ == Tab::Preview)
        preview_panels();
    if (tab_ == Tab::Analysis)
        analysis_panel();
    if (tab_ == Tab::Calibration)
        calibration_panel();
    if (tab_ == Tab::Device)
        device_panel();
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
    }
    if (result_ && !had_result_ && !viewer_) {
        had_result_ = true;
        tip("slice");
    }
    help_windows();
    toasts();
    dialogs();

    // Shortcuts.
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_O, ImGuiInputFlags_RouteGlobal)) {
        purpose_ = Purpose::Open;
        browser_.open(FileBrowser::Mode::Open, tr(T::OpenFile), kOpenExtensions, last_folder_);
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
        ray_failed_ = !renderer_.set_ray_scene(b->nodes, b->primitives);
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
    ImGui::TextDisabled("%s", tr(T::DeviceHint));
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
        ImGui::TextDisabled("%s", tr(T::DeviceNoPorts));
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
        ImGui::TextColored(rgb(state_colour), "%s", tr(state_text[int(s.state)]));
    }
    if (!s.firmware.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled(tr(T::DeviceFirmware), s.firmware.c_str());
    }
    if (!s.error.empty())
        ImGui::TextColored(rgb(0xE5484D), "%s", s.error.c_str());
    if (connected) {
        const auto heater = [&](T label, const device::Temperature& t, double& target, bool nozzle) {
            ImGui::PushID(nozzle ? "nozzle" : "bed");
            ImGui::Text("%s  %.1f / %.0f °C", tr(label), t.actual, t.target);
            ImGui::SetNextItemWidth(field * 0.7f);
            double lo = 0.0, hi = nozzle ? 400.0 : 150.0;
            ImGui::DragScalar("##target", ImGuiDataType_Double, &target, 1.0f, &lo, &hi, "%.0f °C",
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
        ImGui::Text(tr(T::DeviceProgress), s.job.c_str(), s.progress * 100.0, number_text(s.lines_sent).c_str(),
                    number_text(s.lines_total).c_str());
        const double total = result_ && result_->preview.estimated_seconds > 0.0 ? result_->preview.estimated_seconds
                             : s.progress > 0.01                                ? s.elapsed / s.progress
                                                                                : 0.0;
        if (busy)
            ImGui::TextDisabled(tr(T::DeviceTimes), format_duration(s.elapsed).c_str(),
                                format_duration(std::max(0.0, total * (1.0 - s.progress))).c_str());
        if (s.resends > 0)
            ImGui::TextDisabled(tr(T::DeviceResends), s.resends);
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
        ImGui::TextDisabled(tr(T::DevicePrintWhat), what.c_str());
        if (button("##print", "tisma_print", tr(T::DevicePrint), true, false, s.state == State::Idle, nullptr,
                   button_h))
            device_print();
    } else {
        ImGui::TextDisabled("%s", tr(T::DeviceNothingToPrint));
    }

    // 4: moving and extruding, when idle.
    if (s.state == State::Idle && ImGui::CollapsingHeader(tr(T::DeviceControls))) {
        if (ImGui::SmallButton(tr(T::DeviceHome)))
            printer_->home();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(field * 0.6f);
        double lo = 0.1, hi = 100.0;
        ImGui::DragScalar(tr(T::DeviceStep), ImGuiDataType_Double, &device_step_, 0.1f, &lo, &hi, "%.1f mm",
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
        ImGui::DragScalar("##fan", ImGuiDataType_Double, &device_fan_, 1.0f, &flo, &fhi, "%.0f %%",
                          ImGuiSliderFlags_AlwaysClamp);
        ImGui::SameLine();
        if (ImGui::SmallButton(tr(T::DeviceFan)))
            printer_->fan(device_fan_);
    }

    // 5: what happened.
    if (!device_log_.empty() && ImGui::CollapsingHeader(tr(T::DeviceLog))) {
        for (auto it = device_log_.rbegin(); it != device_log_.rend() && it - device_log_.rbegin() < 30; ++it)
            ImGui::TextDisabled("%s", it->c_str());
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
