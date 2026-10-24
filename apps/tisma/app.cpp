// The interface: window, panels, 3D view and the actions behind the buttons.
// Own design; see PROVENANCE.md.

#include "app.hpp"

#include "embedded.hpp"
#include "file_browser.hpp"
#include "gl.hpp"
#include "renderer.hpp"

#include "calibration/calibration.hpp"
#include "config/settings.hpp"
#include "gcode/export.hpp"
#include "gcode/print_time.hpp"
#include "io/stl.hpp"
#include "ui/camera.hpp"
#include "ui/files.hpp"
#include "ui/icons.hpp"
#include "ui/png.hpp"
#include "ui/raytrace.hpp"
#include "ui/scene.hpp"
#include "ui/slicing.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <misc/cpp/imgui_stdlib.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <utility>

namespace tisma::ui {
namespace {

// Sizes in screen units before the monitor's scale.
constexpr float kTopBar = 56.0f;
constexpr float kStatusBar = 30.0f;
constexpr float kPanel = 350.0f;
constexpr float kFontSize = 15.0f;
constexpr float kTitleSize = 21.0f;
constexpr double kToastSeconds = 6.0;

constexpr const char* kIcons[] = {
    "tisma_plate",      "tisma_print",     "tisma_filament", "tisma_printer",     "tisma_more",      "tisma_folder",
    "tisma_save",       "tisma_slice",     "tisma_send",     "tisma_engineering", "tisma_calibrate", "tisma_vibration",
    "tisma_view",       "tisma_view_back", "tisma_view_front", "tisma_view_left", "tisma_view_right", "tisma_view_top",
    "tisma_zoom_fit",   "tisma_zoom_in",   "tisma_zoom_out",
};

const std::vector<std::string> kOpenExtensions = {".stl", ".gcode", ".gco", ".g", ".json"};

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

enum class Tab { Prepare, Preview, Analysis, Calibration };
enum class Purpose { Open, SaveGcode, LoadSettings, SaveSettings };

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
    void load_calibration(CalibrationPrint print);
    void empty_hint();
    void toasts();
    void dialogs();

    // Actions.
    void open_path(const std::filesystem::path& path);
    void load_model(const std::filesystem::path& path);
    void load_gcode(const std::filesystem::path& path);
    void load_settings_file(const std::filesystem::path& path);
    void save_settings_file(const std::filesystem::path& path);
    void export_gcode_to(const std::filesystem::path& path);
    void start_slice();
    void poll_jobs();
    void show_preview();
    void update_ray_scene();
    void check_rays(const ViewFrame& f);
    ViewFrame view_frame(int fb_w, int fb_h);
    void settings_changed();
    void bed_changed();
    void place_model();
    void frame_all();
    void set_tab(Tab tab);
    void notify(std::string text, bool error = false);
    void fail(T title, std::string message, std::vector<std::string> details = {});
    void load_preferences();
    void save_preferences();
    void apply_theme();

    // Widgets.
    ImTextureRef icon(const char* name) const;
    bool button(const char* id, const char* icon_name, const char* label, bool primary, bool selected, bool enabled,
                const char* tip, float height);
    bool icon_button(const char* id, const char* icon_name, const char* tip, float size);
    bool section(const char* id, const char* icon_name, T title, bool open_by_default = true);

    AppOptions options_;
    GLFWwindow* window_ = nullptr;
    Renderer renderer_;
    OrbitCamera camera_;
    float scale_ = 1.0f;
    Theme theme_ = dark_theme();
    ImFont* font_ = nullptr;
    std::map<std::string, unsigned> icons_;
    unsigned logo_ = 0;
    ImVec2 view_pos_{0, 0}, view_size_{1, 1};

    Tab tab_ = Tab::Prepare;
    Settings settings_ = default_settings();

    // The model, as read and where it sits on the bed.
    std::optional<TriangleMesh> mesh_;
    std::filesystem::path model_path_;
    Vec3 offset_{};
    BoundingBox placed_;

    Slicer slicer_;
    Background<SliceResult> slice_job_;
    std::optional<SliceResult> result_;
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
    float feed_range_[2] = {0, 0}, width_range_[2] = {0, 0};
    PreviewBuffers preview_buffers_;

    // How the view is drawn, and the scene its rays are traced against.
    RenderMode render_mode_ = RenderMode::Ambient;
    int msaa_ = 4;
    bool fxaa_ = true;
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

    // Calibration: the dimensional test, what was measured on it, the towers'
    // ranges, and the test loaded last (its segments, without the mesh).
    DimensionTest dimension_test_;
    Measurements measured_{30.0, 20.0, 15.0, 10.0};
    struct TowerRange {
        double start, end, step, segment;
    };
    TowerRange temperature_range_{230.0, 190.0, 5.0, 8.0}, retraction_range_{0.4, 2.4, 0.4, 5.0},
        flow_range_{90.0, 110.0, 5.0, 5.0}, speed_range_{100.0, 300.0, 25.0, 5.0};
    std::optional<CalibrationPrint> calibration_;

    // Screenshot mode.
    int frames_ = 0, settled_frames_ = 0;
    bool quit_ = false;
    bool gl_loaded_ = false;
    bool failed_ = false; // the screenshot or its export failed
};

ImTextureRef App::icon(const char* name) const {
    const auto it = icons_.find(name);
    return ImTextureRef(static_cast<ImTextureID>(it == icons_.end() ? 0 : it->second));
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

// A collapsing section of the settings panel with its icon.
bool App::section(const char* id, const char* icon_name, T title, bool open_by_default) {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, 8.0f * scale_));
    const std::string label = std::string("        ") + tr(title) + "###" + id;
    const bool open = ImGui::CollapsingHeader(label.c_str(), open_by_default ? ImGuiTreeNodeFlags_DefaultOpen : 0);
    ImGui::PopStyleVar();
    const ImVec2 min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
    const float size = ImGui::GetFontSize() * 1.15f;
    const float x = min.x + ImGui::GetStyle().FramePadding.x + ImGui::GetFontSize() + 4.0f * scale_;
    const float y = (min.y + max.y - size) * 0.5f;
    ImGui::GetWindowDrawList()->AddImage(icon(icon_name), ImVec2(x, y), ImVec2(x + size, y + size), ImVec2(0, 0),
                                         ImVec2(1, 1), ImGui::GetColorU32(rgb(theme_.accent_hover)));
    return open;
}

// Rows of the settings tables: a label (with a tooltip) and a control.
void label_cell(T label, const char* tip = nullptr) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(tr(label));
    if (tip && ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", tip);
    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(-FLT_MIN);
}

bool row(T label, double& v, double min, double max, const char* fmt, float speed = 0.01f, const char* tip = nullptr) {
    label_cell(label, tip);
    ImGui::PushID(&v);
    const bool changed =
        ImGui::DragScalar("##v", ImGuiDataType_Double, &v, speed, &min, &max, fmt, ImGuiSliderFlags_AlwaysClamp);
    ImGui::PopID();
    return changed;
}

bool row(T label, int& v, int min, int max, const char* fmt = "%d", const char* tip = nullptr) {
    label_cell(label, tip);
    ImGui::PushID(&v);
    const bool changed = ImGui::DragInt("##v", &v, 0.1f, min, max, fmt, ImGuiSliderFlags_AlwaysClamp);
    ImGui::PopID();
    return changed;
}

bool row(T label, bool& v) {
    label_cell(label);
    ImGui::PushID(&v);
    const bool changed = ImGui::Checkbox("##v", &v);
    ImGui::PopID();
    return changed;
}

template <class Enum>
bool row(T label, Enum& v, std::initializer_list<std::pair<Enum, const char*>> options) {
    label_cell(label);
    const char* current = "";
    for (const auto& [value, name] : options)
        if (value == v)
            current = name;
    bool changed = false;
    ImGui::PushID(&v);
    if (ImGui::BeginCombo("##v", current)) {
        for (const auto& [value, name] : options)
            if (ImGui::Selectable(name, value == v)) {
                changed = value != v;
                v = value;
            }
        ImGui::EndCombo();
    }
    ImGui::PopID();
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
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, options_.screenshot.empty() ? GLFW_TRUE : GLFW_FALSE);
    window_ = glfwCreateWindow(options_.width, options_.height, "Tisma", nullptr, nullptr);
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
    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    if (!renderer_.init(error))
        return false;

    // Icons drawn white at twice their size; the buttons tint them.
    const int icon_pixels = int(std::lround(48.0f * scale_));
    for (const char* name : kIcons) {
        const Image image = rasterize_svg(embedded(std::string(name) + ".svg"), icon_pixels, 0xFFFFFF);
        if (image.rgba.empty())
            continue;
        unsigned tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     image.rgba.data());
        icons_[name] = tex;
    }
    const Image logo = rasterize_svg(embedded("tisma.svg"), int(std::lround(96.0f * scale_)));
    if (!logo.rgba.empty()) {
        glGenTextures(1, &logo_);
        glBindTexture(GL_TEXTURE_2D, logo_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, logo.width, logo.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, logo.rgba.data());
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    if (options_.screenshot.empty())
        load_preferences();
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
    apply_theme();
    bed_changed();
    camera_.set_view(StandardView::Iso);
    frame_all();
    for (const std::string& file : options_.files)
        open_path(from_utf8(file));
    if (const std::string& c = options_.calibration; !c.empty()) {
        if (c == "dimensions")
            load_calibration(dimension_test(dimension_test_));
        else if (c == "temperature")
            load_calibration(temperature_tower(temperature_range_.start, temperature_range_.end, temperature_range_.step,
                                               temperature_range_.segment));
        else if (c == "retraction")
            load_calibration(retraction_tower(retraction_range_.start, retraction_range_.end, retraction_range_.step,
                                              retraction_range_.segment));
        else if (c == "flow")
            load_calibration(flow_tower(flow_range_.start, flow_range_.end, flow_range_.step, flow_range_.segment));
        else if (c == "speed")
            load_calibration(speed_tower(speed_range_.start, speed_range_.end, speed_range_.step, speed_range_.segment));
    }
    if (!options_.screenshot.empty() && (options_.view == "preview" || !options_.export_path.empty()) && mesh_ &&
        !result_)
        start_slice();
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
}

void App::shutdown() {
    slice_job_.wait();
    voxel_job_.wait();
    if (options_.screenshot.empty() && gl_loaded_)
        save_preferences();
    if (gl_loaded_) {
        for (const auto& [name, tex] : icons_)
            glDeleteTextures(1, &tex);
        if (logo_)
            glDeleteTextures(1, &logo_);
        renderer_.shutdown();
    }
    if (ImGui::GetCurrentContext()) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
    if (window_)
        glfwDestroyWindow(window_);
    glfwTerminate();
}

void App::load_preferences() {
    const std::filesystem::path path = preferences_path();
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return;
    try {
        const nlohmann::json j = nlohmann::json::parse(in);
        if (j.value("language", "es") == "en")
            set_language(Language::English);
        if (j.value("theme", "dark") == "light")
            theme_ = light_theme();
        last_folder_ = from_utf8(j.value("folder", std::string()));
        render_mode_ = static_cast<RenderMode>(std::clamp(j.value("render", 1), 0, 2));
        msaa_ = std::clamp(j.value("msaa", 4), 1, 16);
        fxaa_ = j.value("fxaa", true);
        ao_strength_ = std::clamp(j.value("ao_strength", 1.0f), 0.0f, 1.5f);
        if (j.contains("settings"))
            settings_ = parse_settings(j.at("settings").dump());
    } catch (const std::exception&) {
        // Unreadable preferences: start from the defaults.
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
        j["ao_strength"] = ao_strength_;
        j["settings"] = nlohmann::json::parse(settings_json(settings_));
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        std::ofstream out(path, std::ios::binary);
        out << j.dump(2) << "\n";
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
    if (ext == ".stl")
        load_model(path);
    else if (ext == ".gcode" || ext == ".gco" || ext == ".g")
        load_gcode(path);
    else if (ext == ".json")
        load_settings_file(path);
    else
        fail(T::CannotOpen, utf8(path) + ": only .stl, .gcode and .json files");
}

void App::load_model(const std::filesystem::path& path) {
    try {
        StlFile stl = read_stl(path);
        if (stl.mesh.empty()) {
            fail(T::CannotOpen, utf8(path) + ": the file has no triangles");
            return;
        }
        calibration_.reset();
        use_mesh(std::move(stl.mesh), path, Tab::Prepare);
    } catch (const std::exception& e) {
        fail(T::CannotOpen, e.what());
    }
}

void App::use_mesh(TriangleMesh mesh, const std::filesystem::path& name, Tab tab) {
    mesh_ = std::move(mesh);
    model_path_ = name;
    viewer_ = false;
    result_.reset();
    renderer_.clear_preview();
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

void App::place_model() {
    if (!mesh_)
        return;
    offset_ = centre_on_bed(mesh_->bounding_box(), settings_.bed);
    const BoundingBox box = mesh_->bounding_box();
    placed_ = box;
    placed_.min = {box.min.x + offset_.x, box.min.y + offset_.y, box.min.z + offset_.z};
    placed_.max = {box.max.x + offset_.x, box.max.y + offset_.y, box.max.z + offset_.z};
    renderer_.set_model(mesh_vertices(*mesh_, offset_));
    ++scene_version_;
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
    } catch (const std::exception& e) {
        fail(T::CannotOpen, e.what());
    }
}

void App::save_settings_file(const std::filesystem::path& path) {
    std::ofstream out(path, std::ios::binary);
    out << settings_json(settings_);
    out.close();
    if (!out)
        fail(T::CannotOpen, utf8(path));
    else
        notify(format(tr(T::SettingsSaved), utf8(path.filename()).c_str()));
}

void App::export_gcode_to(const std::filesystem::path& path) {
    if (!result_ || !result_->ok || viewer_)
        return;
    std::vector<Warning> warnings;
    const GcodeExport e = export_gcode(path, result_->model, result_->settings, &warnings);
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

void App::start_slice() {
    if (!mesh_ || slice_job_.running())
        return;
    TriangleMesh placed = translated(*mesh_, offset_);
    Settings settings = settings_;
    slice_started_ = glfwGetTime();
    slice_job_.start([this, placed = std::move(placed), settings = std::move(settings)]() {
        return slicer_.run(placed, settings);
    });
}

void App::poll_jobs() {
    if (std::optional<SliceResult> r = slice_job_.take()) {
        if (!r->ok) {
            fail(T::CannotSlice, r->error);
            failed_ |= !options_.export_path.empty();
            return;
        }
        result_ = std::move(*r);
        viewer_ = false;
        stale_ = false;
        show_preview();
        if (options_.layer > 0)
            top_layer_ = std::clamp(options_.layer - 1, 0, top_layer_);
        set_tab(Tab::Preview);
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
    if (std::optional<VoxelEstimate> v = voxel_job_.take())
        voxels_ = std::move(*v);
}

void App::show_preview() {
    preview_buffers_ = preview_buffers(result_->preview);
    const PreviewBuffers& buffers = preview_buffers_;
    ++scene_version_;
    feed_range_[0] = buffers.min_feed, feed_range_[1] = buffers.max_feed;
    width_range_[0] = buffers.min_width, width_range_[1] = buffers.max_width;
    renderer_.set_preview(buffers);
    top_layer_ = int(result_->preview.layers.size()) - 1;
    bottom_layer_ = 0;
}

void App::settings_changed() {
    if (result_ && !viewer_)
        stale_ = true;
    voxels_stale_ = true;
}

void App::bed_changed() {
    apply_theme(); // remakes the bed's lines
    place_model();
    ++scene_version_;
}

void App::frame_all() {
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
}

void App::set_tab(Tab tab) {
    tab_ = tab;
    if (tab == Tab::Analysis && mesh_ && voxels_stale_ && !voxel_job_.running()) {
        voxels_stale_ = false;
        TriangleMesh placed = translated(*mesh_, offset_);
        Settings settings = settings_;
        voxel_job_.start([placed = std::move(placed), settings = std::move(settings)]() {
            return estimate_voxels(placed, settings);
        });
    }
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
        ImGui::Image(ImTextureRef(static_cast<ImTextureID>(logo_)), ImVec2(logo, logo));
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
                              {Tab::Calibration, "tisma_calibrate", T::TabCalibration}};
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
               can_export ? tr(T::ExportTip) : tr(T::ExportNeedsSlice), button_h)) {
        purpose_ = Purpose::SaveGcode;
        const std::string name = model_path_.empty() ? "tisma.gcode" : utf8(model_path_.stem()) + ".gcode";
        browser_.open(FileBrowser::Mode::Save, tr(T::SaveGcode), {".gcode"}, last_folder_, name);
    }
    ImGui::SameLine(0, gap);
    ImGui::SetCursorPosY(top);
    if (icon_button("##more", "tisma_more", tr(T::More), button_h))
        ImGui::OpenPopup("##more_menu");
    if (ImGui::BeginPopup("##more_menu")) {
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

void App::settings_panel() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float top = kTopBar * scale_, bottom = kStatusBar * scale_;
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x, vp->Pos.y + top));
    ImGui::SetNextWindowSize(ImVec2(kPanel * scale_, vp->Size.y - top - bottom));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, rgb(theme_.panel));
    ImGui::Begin("##settings", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);
    const float footer = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
    ImGui::BeginChild("##settings_scroll", ImVec2(0, -footer));
    Settings& s = settings_;
    bool changed = false, bed = false;
    const ImGuiTableFlags table = ImGuiTableFlags_SizingStretchProp;
    auto begin_rows = [&](const char* id) {
        if (!ImGui::BeginTable(id, 2, table))
            return false;
        ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthStretch, 1.15f);
        ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        return true;
    };

    if (section("print", "tisma_print", T::Print) && begin_rows("##print")) {
        changed |= row(T::LayerHeight, s.layer_height, 0.04, 1.0, "%.2f mm", 0.002f);
        changed |= row(T::FirstLayerHeight, s.first_layer_height, 0.04, 1.0, "%.2f mm", 0.002f);
        changed |= row(T::Walls, s.wall_count, 0, 50);
        changed |= row(T::LineWidth, s.wall_width, 0.1, 2.0, "%.2f mm", 0.002f);
        changed |= row(T::Seam, s.seam,
                       {{SeamMode::Aligned, tr(T::SeamAligned)},
                        {SeamMode::Nearest, tr(T::SeamNearest)},
                        {SeamMode::Random, tr(T::SeamRandom)},
                        {SeamMode::Rear, tr(T::SeamRear)}});
        changed |= row(T::TopLayers, s.top_layers, 0, 100);
        changed |= row(T::BottomLayers, s.bottom_layers, 0, 100);
        changed |= row(T::TopPattern, s.monotonic_top, {{true, tr(T::Monotonic)}, {false, tr(T::Rectilinear)}});
        changed |= row(T::InfillDensity, s.infill_density, 0.0, 100.0, "%.0f %%", 0.2f);
        changed |= row(T::InfillPattern, s.infill_pattern,
                       {{InfillPattern::Rectilinear, tr(T::Rectilinear)},
                        {InfillPattern::Grid, tr(T::Grid)},
                        {InfillPattern::Gyroid, tr(T::Gyroid)}});
        changed |= row(T::InfillAngle, s.infill_angle, -180.0, 180.0, "%.0f°", 0.5f);
        changed |= row(T::Supports, s.support);
        if (s.support)
            changed |= row(T::SupportAngle, s.support_angle, 0.0, 89.0, "%.0f°", 0.5f);
        changed |= row(T::SkirtLoops, s.skirt_loops, 0, 20);
        changed |= row(T::SkirtDistance, s.skirt_distance, 0.0, 50.0, "%.1f mm", 0.05f);
        changed |= row(T::BrimWidth, s.brim_width, 0.0, 50.0, "%.1f mm", 0.05f);
        ImGui::EndTable();
        if (ImGui::TreeNode(tr(T::Speeds)) ) {
            if (begin_rows("##speeds")) {
                changed |= row(T::SpeedWalls, s.wall_speed, 1.0, 1000.0, "%.0f mm/s", 0.5f);
                changed |= row(T::SpeedOuterWall, s.outer_wall_speed.value, 1.0, s.outer_wall_speed.percent ? 100.0 : 1000.0,
                               s.outer_wall_speed.percent ? "%.0f %%" : "%.0f mm/s", 0.5f);
                changed |= row(T::SpeedInfill, s.infill_speed, 1.0, 1000.0, "%.0f mm/s", 0.5f);
                changed |= row(T::SpeedSolid, s.solid_infill_speed, 1.0, 1000.0, "%.0f mm/s", 0.5f);
                changed |= row(T::SpeedTop, s.top_solid_infill_speed, 1.0, 1000.0, "%.0f mm/s", 0.5f);
                changed |= row(T::SpeedSupport, s.support_speed, 1.0, 1000.0, "%.0f mm/s", 0.5f);
                changed |= row(T::SpeedTravel, s.travel_speed, 1.0, 1000.0, "%.0f mm/s", 0.5f);
                changed |= row(T::SpeedFirstLayer, s.first_layer_speed, 1.0, 1000.0, "%.0f mm/s", 0.5f);
                changed |= row(T::SpeedMax, s.max_speed, 1.0, 1000.0, "%.0f mm/s", 0.5f);
                ImGui::EndTable();
            }
            ImGui::TreePop();
        }
    }
    if (section("filament", "tisma_filament", T::Filament) && begin_rows("##filament")) {
        changed |= row(T::FilamentDiameter, s.filament_diameter, 1.0, 3.5, "%.2f mm", 0.005f);
        changed |= variable_row(T::NozzleTemp, s, "temperature", 0, 450, "%d °C", tr(T::TempTip));
        changed |= variable_row(T::BedTemp, s, "bed_temperature", 0, 150, "%d °C", tr(T::TempTip));
        changed |= row(T::Retraction, s.retract_length, 0.0, 20.0, "%.2f mm", 0.01f);
        changed |= row(T::RetractSpeed, s.retract_speed, 1.0, 200.0, "%.0f mm/s", 0.5f);
        changed |= row(T::RetractLift, s.retract_lift, 0.0, 5.0, "%.2f mm", 0.01f);
        changed |= row(T::FanMin, s.fan_min, 0.0, 100.0, "%.0f %%", 0.5f);
        changed |= row(T::FanMax, s.fan_max, 0.0, 100.0, "%.0f %%", 0.5f);
        changed |= row(T::MinLayerTime, s.min_layer_time, 0.0, 120.0, "%.0f s", 0.2f);
        ImGui::EndTable();
    }
    if (section("printer", "tisma_printer", T::Printer, false) && begin_rows("##printer")) {
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
        changed |= row(T::MaxHeight, s.max_height, 0.0, 2000.0, "%.0f mm", 0.5f);
        changed |= row(T::Nozzle, s.nozzle_diameter, 0.1, 2.0, "%.2f mm", 0.005f);
        changed |= row(T::Flavor, s.flavor,
                       {{GcodeFlavor::Marlin, "Marlin"}, {GcodeFlavor::Klipper, "Klipper"}, {GcodeFlavor::RepRap, "RepRap"}});
        changed |= row(T::RelativeE, s.relative_extrusion);
        ImGui::EndTable();
        ImGui::TextUnformatted(tr(T::StartGcode));
        changed |= ImGui::InputTextMultiline("##start", &s.start_gcode, ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 7));
        ImGui::TextUnformatted(tr(T::EndGcode));
        changed |= ImGui::InputTextMultiline("##end", &s.end_gcode, ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 6));
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
        bed = changed = true;
    }
    ImGui::End();
    ImGui::PopStyleColor();
    if (bed)
        bed_changed();
    if (changed)
        settings_changed();
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
        ImGui::Text("%s  ·  %s %s  ·  %.1f × %.1f × %.1f mm", utf8(model_path_.filename()).c_str(),
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
        ImGui::Text("%s", format(tr(T::SlicedIn), total).c_str());
        ImGui::SameLine(0, 16.0f * scale_);
        if (result_->check.ok())
            ImGui::TextColored(rgb(0x3FBF7F), "%s",
                               format(tr(T::GcodeOk), number_text(result_->check.stats.lines).c_str()).c_str());
        else
            ImGui::TextColored(rgb(0xE5484D), "%s", format(tr(T::GcodeBad), int(result_->check.problem_count)).c_str());
    } else {
        ImGui::TextDisabled("%s", tr(T::Ready));
    }
    if (result_ && !viewer_) {
        const std::string device = result_->gpu.empty() ? std::string("CPU") : "GPU: " + result_->gpu;
        ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::CalcTextSize(device.c_str()).x - 12.0f * scale_);
        ImGui::TextDisabled("%s", device.c_str());
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
    if (ImGui::IsItemActive()) {
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f))
            camera_.orbit(io.MouseDelta.x, io.MouseDelta.y);
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.0f) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f))
            camera_.pan(io.MouseDelta.x, io.MouseDelta.y, view_size_.y);
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
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2);
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
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
    const char* modes[] = {tr(T::ByKind), tr(T::BySpeed), tr(T::ByWidth)};
    if (ImGui::Combo("##colour_mode", &mode, modes, 3))
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
        const char* unit = speed ? "%.0f mm/s" : "%.2f mm";
        const std::string low = format(unit, double(speed ? feed_range_[0] : width_range_[0]));
        const std::string high = format(unit, double(speed ? feed_range_[1] : width_range_[1]));
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

void App::calibration_panel() {
    const float margin = 12.0f * scale_;
    ImGui::SetNextWindowPos(ImVec2(view_pos_.x + margin, view_pos_.y + 60.0f * scale_));
    ImGui::SetNextWindowSize(ImVec2(440.0f * scale_, view_size_.y - 72.0f * scale_));
    ImGui::SetNextWindowBgAlpha(0.94f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f * scale_);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f * scale_, 14.0f * scale_));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, rgb(theme_.panel));
    ImGui::Begin("##calibration", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    ImGui::BeginChild("##calibration_scroll", ImVec2(0, 0), 0, ImGuiWindowFlags_AlwaysVerticalScrollbar);
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

    // The loaded test and its segments.
    if (calibration_ && !calibration_->segments.empty()) {
        ImGui::Separator();
        const T names[] = {T::CalTemperature, T::CalRetraction, T::CalFlow, T::CalSpeed};
        const char* kinds[] = {"temperature", "retraction", "flow", "speed"};
        const char* name = calibration_->kind.c_str();
        for (int k = 0; k < 4; ++k)
            if (calibration_->kind == kinds[k])
                name = tr(names[k]);
        ImGui::TextColored(rgb(theme_.accent_hover), tr(T::TowerLoaded), name, int(calibration_->segments.size()));
        if (ImGui::BeginTable("##segments", 2, ImGuiTableFlags_SizingStretchProp)) {
            for (auto it = calibration_->segments.rbegin(); it != calibration_->segments.rend(); ++it) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextDisabled("Z %.1f – %.1f mm", it->z0, it->z1);
                ImGui::TableNextColumn();
                ImGui::Text("%g %s", it->value, calibration_->unit == "C" ? "°C" : calibration_->unit.c_str());
            }
            ImGui::EndTable();
        }
    }
    if (!settings_.changes.empty()) {
        ImGui::TextColored(rgb(0xE8A33D), tr(T::ChangesActive), int(settings_.changes.size()));
        ImGui::SameLine();
        if (ImGui::SmallButton(tr(T::ClearChanges))) {
            settings_.changes.clear();
            calibration_.reset();
            settings_changed();
        }
    }

    // Dimensions: measure the block, compute and apply the compensation.
    ImGui::Separator();
    if (ImGui::CollapsingHeader(tr(T::CalDimensions), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextDisabled("%s", tr(T::CalDimensionsHint));
        if (ImGui::Button(tr(T::LoadPart)))
            load_calibration(dimension_test(dimension_test_));
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
        if (ImGui::Button(tr(T::ApplyCompensation))) {
            settings_.scale_x = next.scale_x, settings_.scale_y = next.scale_y, settings_.scale_z = next.scale_z;
            settings_.hole_offset = next.hole_offset;
            // The block printed with these values would now measure as designed.
            measured_ = {dimension_test_.x, dimension_test_.y, dimension_test_.z, dimension_test_.hole};
            settings_changed();
            notify(format(tr(T::CompensationApplied), next.scale_x, next.scale_y, next.scale_z, next.hole_offset));
        }
        ImGui::SameLine();
        if (ImGui::Button(tr(T::ResetCompensation))) {
            settings_.scale_x = settings_.scale_y = settings_.scale_z = 100.0;
            settings_.hole_offset = 0.0;
            settings_changed();
        }
    }

    // The towers.
    struct Tower {
        T title, hint;
        TowerRange* range;
        double lo, hi;
        const char* fmt;
        float speed;
    };
    const Tower towers[] = {
        {T::CalTemperature, T::CalTemperatureHint, &temperature_range_, 150.0, 350.0, "%.0f °C", 0.5f},
        {T::CalRetraction, T::CalRetractionHint, &retraction_range_, 0.0, 10.0, "%.2f mm", 0.01f},
        {T::CalFlow, T::CalFlowHint, &flow_range_, 50.0, 150.0, "%.1f %%", 0.1f},
        {T::CalSpeed, T::CalSpeedHint, &speed_range_, 20.0, 600.0, "%.0f %%", 1.0f},
    };
    for (int i = 0; i < 4; ++i) {
        const Tower& t = towers[i];
        ImGui::PushID(i);
        if (ImGui::CollapsingHeader(tr(t.title))) {
            ImGui::TextDisabled("%s", tr(t.hint));
            number(T::TowerStart, t.range->start, t.lo, t.hi, t.fmt, t.speed);
            number(T::TowerEnd, t.range->end, t.lo, t.hi, t.fmt, t.speed);
            number(T::TowerStep, t.range->step, t.speed, t.hi - t.lo, t.fmt, t.speed);
            number(T::TowerSegment, t.range->segment, 2.0, 30.0, "%.1f mm", 0.05f);
            if (i == 2) {
                double flow = settings_.flow;
                number(T::FlowAll, flow, 10.0, 300.0, "%.1f %%", 0.1f);
                if (flow != settings_.flow) {
                    settings_.flow = flow;
                    settings_changed();
                }
            }
            if (ImGui::Button(tr(T::LoadTower))) {
                const TowerRange& r = *t.range;
                load_calibration(i == 0   ? temperature_tower(r.start, r.end, r.step, r.segment)
                                 : i == 1 ? retraction_tower(r.start, r.end, r.step, r.segment)
                                 : i == 2 ? flow_tower(r.start, r.end, r.step, r.segment)
                                          : speed_tower(r.start, r.end, r.step, r.segment));
            }
        }
        ImGui::PopID();
    }
    if (ImGui::CollapsingHeader(tr(T::MoreCalibrations)))
        ImGui::TextDisabled("%s", tr(T::MoreCalibrationsList));
    ImGui::PopTextWrapPos();
    ImGui::EndChild();
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
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
        case Purpose::SaveGcode: export_gcode_to(chosen); break;
        case Purpose::LoadSettings: load_settings_file(chosen); break;
        case Purpose::SaveSettings: save_settings_file(chosen); break;
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

    if (about_open_) {
        ImGui::OpenPopup("###about");
        about_open_ = false;
    }
    ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    const std::string about = std::string(tr(T::About)) + "###about";
    if (ImGui::BeginPopupModal(about.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (logo_)
            ImGui::Image(ImTextureRef(static_cast<ImTextureID>(logo_)), ImVec2(72.0f * scale_, 72.0f * scale_));
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
    if (!mesh_ && !result_)
        empty_hint();
    if (tab_ == Tab::Preview)
        preview_panels();
    if (tab_ == Tab::Analysis)
        analysis_panel();
    if (tab_ == Tab::Calibration)
        calibration_panel();
    toasts();
    dialogs();

    // Shortcuts.
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_O, ImGuiInputFlags_RouteGlobal)) {
        purpose_ = Purpose::Open;
        browser_.open(FileBrowser::Mode::Open, tr(T::OpenFile), kOpenExtensions, last_folder_);
    }
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_R, ImGuiInputFlags_RouteGlobal))
        start_slice();
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_E, ImGuiInputFlags_RouteGlobal) && result_ && result_->ok && !viewer_ &&
        !stale_) {
        purpose_ = Purpose::SaveGcode;
        const std::string name = model_path_.empty() ? "tisma.gcode" : utf8(model_path_.stem()) + ".gcode";
        browser_.open(FileBrowser::Mode::Save, tr(T::SaveGcode), {".gcode"}, last_folder_, name);
    }
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
    const bool preview = tab_ == Tab::Preview && result_;
    f.show_model = mesh_.has_value() && !preview;
    f.show_preview = preview;
    f.show_travels = show_travels_;
    f.first_layer = std::size_t(bottom_layer_);
    f.last_layer = std::size_t(top_layer_);
    f.kind_mask = kind_mask_;
    f.color_mode = color_mode_;
    for (std::size_t k = 0; k < kLineKinds; ++k)
        f.kind_colours[k] = vec(kind_colour(LineKind(k)));
    f.mode = render_mode_;
    f.samples = msaa_;
    f.fxaa = fxaa_;
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
    for (const std::filesystem::path& path : std::exchange(dropped_, {}))
        open_path(path);
    poll_jobs();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    layout();
    ImGui::Render();

    int fb_w = 0, fb_h = 0;
    glfwGetFramebufferSize(window_, &fb_w, &fb_h);
    glViewport(0, 0, fb_w, fb_h);
    const Vec3 clear = vec(theme_.window);
    glClearColor(clear.x, clear.y, clear.z, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // The 3D view, under the panels.
    update_ray_scene();
    ViewFrame f = view_frame(fb_w, fb_h);
    const bool check_now = options_.ray_check && !ray_checked_ && render_mode_ == RenderMode::RayTraced &&
                           renderer_.has_ray_scene() && ray_scene_;
    if (check_now)
        f.sun_radius = 0.0f; // one exact direction, so the CPU can shoot the same rays
    renderer_.draw(f);
    if (check_now && renderer_.ray_samples() == 1)
        check_rays(f);

    glViewport(0, 0, fb_w, fb_h);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Screenshot mode: once everything asked for is ready, read the frame back.
    if (!options_.screenshot.empty()) {
        ++frames_;
        // A preview waits for its slice, unless the slice failed (the error is the picture then).
        const bool slice_pending = options_.view == "preview" && mesh_ && !result_ && error_title_.empty();
        // Ray tracing waits for its scene and for enough samples.
        const bool rays_pending = render_mode_ == RenderMode::RayTraced && !ray_failed_ &&
                                  (ray_job_.running() || !renderer_.has_ray_scene() ||
                                   renderer_.ray_samples() < std::min(options_.ray_samples, 256) ||
                                   (options_.ray_check && !ray_checked_));
        const bool waiting = slice_job_.running() || voxel_job_.running() || slice_pending || rays_pending;
        if (frames_ == 2) {
            if (options_.view == "analysis")
                set_tab(Tab::Analysis);
            else if (options_.view == "preview" && result_)
                set_tab(Tab::Preview);
            else if (options_.view == "prepare")
                set_tab(Tab::Prepare);
            else if (options_.view == "calibration")
                set_tab(Tab::Calibration);
        }
        settled_frames_ = waiting ? 0 : settled_frames_ + 1;
        if (frames_ > 3 && settled_frames_ >= 3) {
            std::vector<std::uint8_t> pixels(std::size_t(fb_w) * std::size_t(fb_h) * 4);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadPixels(0, 0, fb_w, fb_h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
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
            if (!error_title_.empty())
                std::fprintf(stderr, "%s: %s\n", error_title_.c_str(), error_message_.c_str());
            quit_ = true;
        }
        if (frames_ > 6000) { // about 100 s: something never finished
            std::fprintf(stderr, "error: the screenshot timed out\n");
            failed_ = quit_ = true;
        }
    }
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
