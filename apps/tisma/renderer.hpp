#pragma once

#include "ui/math.hpp"
#include "ui/raytrace.hpp"
#include "ui/scene.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace tisma::ui {

enum class ColorMode : int { Kind = 0, Speed = 1, Width = 2, Height = 3 };

// How the 3D view is lit: light only; light with screen-space ambient
// occlusion; light with shadows and ambient occlusion traced as rays against
// the scene, refined frame after frame.
enum class RenderMode : int { Fast = 0, Ambient = 1, RayTraced = 2 };

// What one frame of the 3D view shows, and how.
struct ViewFrame {
    int x = 0, y = 0, width = 1, height = 1; // the view's rectangle in framebuffer pixels, from the bottom left
    Mat4 view = identity(), projection = identity();
    Vec3 eye{0, 0, 1};
    Vec3 key{0, 0, 1}, fill{0, 0, 1}; // towards the key and fill lights, world space, unit
    Vec3 sky{0.80f, 0.82f, 0.95f}, ground{0.30f, 0.28f, 0.27f}; // sRGB colours of the ambient light
    Vec3 background_top{0.17f, 0.17f, 0.19f}, background_bottom{0.10f, 0.10f, 0.12f};
    Vec3 plate{0.20f, 0.20f, 0.23f};
    Vec3 model{0.62f, 0.37f, 0.86f};
    Vec3 model_selected{0.80f, 0.62f, 0.97f}; // the chosen model of the plate
    std::size_t selected_first = 0, selected_count = 0; // its vertices in the model's mesh
    Vec3 selected_offset{0, 0, 0}; // drawn this far from its mesh: a model being dragged, before its mesh is redone
    bool show_model = true;
    bool show_paint = false; // the painted faces (set_paint) over the model
    bool show_preview = false;
    bool show_travels = false;
    std::size_t first_layer = 0, last_layer = 0; // the preview's layers drawn, inclusive
    std::uint32_t kind_mask = 0xFFFFFFFFu;        // bit k: LineKind k is drawn
    ColorMode color_mode = ColorMode::Kind;
    std::array<Vec3, kLineKinds> kind_colours{};

    RenderMode mode = RenderMode::Ambient;
    int samples = 4;          // multisampling; 1: none
    bool fxaa = true;         // the edge smoothing pass after it
    bool moire_filter = true; // fades detail finer than a pixel: the preview's lines and the bed's grid
    float ao_radius = 2.0f;   // mm: how far ambient occlusion looks
    float ao_strength = 1.0f; // 0: none
    float sun_radius = 0.05f; // radians: the key light's size, for soft ray traced shadows
    int max_ray_samples = 256;
    bool debug_visibility = false; // paint the visibility instead of the light (TISMA_DEBUG_VISIBILITY=1)
};

// The ray traced frame's inputs and output, read back for checking the GPU
// against RayScene (the CPU reference).
struct RayReadback {
    int width = 0, height = 0;
    std::vector<float> depth;          // window depth per pixel, rows from the bottom
    std::vector<std::uint8_t> normal;  // view-space normal * 0.5 + 0.5, RGBA
    std::vector<float> visibility;     // RGBA: r direct light, g ambient
};

// Draws the bed, the model and the G-code preview with OpenGL 3.3 into the
// view's rectangle. Every method needs the OpenGL context current. Own
// design on the cited methods; see PROVENANCE.md.
class Renderer {
public:
    // Compiles the shaders and makes the buffers; false with a message if the
    // driver refuses them. Before it, or after it fails, the other methods do
    // nothing (the window may be Vulkan's, PLAN.md «Vista con Vulkan»).
    bool init(std::string& error);
    void shutdown();

    void set_bed(const BedPolygon& bed, const BedLook& look);
    void set_model(const std::vector<float>& vertices); // mesh_vertices
    // The painted pieces over the model (PaintMesh::vertices), drawn in groups
    // of `count` corners from `first`, each in its colour.
    struct PaintGroup {
        std::size_t first = 0, count = 0;
        Vec3 colour;
    };
    void set_paint(const std::vector<float>& vertices, const std::vector<PaintGroup>& groups);
    void set_preview(const PreviewBuffers& preview);
    void clear_preview();

    // The scene the rays are traced against (RayScene::node_texels and
    // primitive_texels); false if this GPU's textures are too small for it.
    bool set_ray_scene(const std::vector<float>& node_texels, const std::vector<float>& primitive_texels);
    void clear_ray_scene();
    bool has_ray_scene() const { return ray_nodes_ != 0; }
    int ray_samples() const { return ray_samples_; }
    int max_samples() const { return max_samples_; }

    void draw(const ViewFrame& frame);

    // After a ray traced frame: its G-buffer and visibility.
    RayReadback read_back();

    // After a frame: the lit scene before FXAA, RGBA rows from the bottom.
    std::vector<std::uint8_t> read_colour();

    // The preview's extrusions the last frame drew (lit pass).
    std::size_t drawn_extrusions() const { return drawn_extrusions_; }

private:
    struct Buffer {
        unsigned vao = 0, vbo = 0;
        int count = 0;
    };
    struct Programs {
        unsigned background = 0, line = 0, grid = 0, plate = 0, mesh = 0, preview = 0;
        unsigned ao = 0, blur = 0, ray = 0, composite = 0;
    };
    void resize(int width, int height, int samples);
    void release_targets();
    void draw_scene(const ViewFrame& f, const Mat4& vp, bool gbuffer, unsigned visibility);
    void lit_uniforms(unsigned program, const ViewFrame& f, const Mat4& vp, bool gbuffer, unsigned visibility);
    void draw_preview(const ViewFrame& f, const Mat4& vp, bool gbuffer, unsigned visibility);
    unsigned ambient_pass(const ViewFrame& f);
    unsigned ray_pass(const ViewFrame& f);

    bool ready_ = false; // init succeeded: without it (a Vulkan window), every call does nothing
    Programs p_;
    unsigned empty_vao_ = 0, white_ = 0;
    Buffer plate_, grid_, model_, paint_, extrusions_, travels_;
    std::vector<PaintGroup> paint_groups_;
    int grid_outline_ = 0;                    // the outline's vertices, after the grid's in grid_
    float grid_step_ = 10.0f;                 // mm between grid lines
    int grid_major_every_ = 5;
    std::vector<std::size_t> layer_extrusions_, layer_travels_;
    // The extrusions in drawing order: layer l's that can show are
    // [layer_shown_[l], layer_shown_[l + 1]), and the ones the whole part hides
    // [layer_hidden_[l], layer_hidden_[l + 1]) (docs/LARGE_MODELS.md).
    std::vector<std::size_t> layer_shown_, layer_hidden_;
    std::uint32_t hidden_kinds_ = 0; // bit k: some extrusion of LineKind k is hidden
    std::size_t drawn_extrusions_ = 0;
    float min_feed_ = 0, max_feed_ = 0, min_width_ = 0, max_width_ = 0, min_height_ = 0, max_height_ = 0;
    int max_samples_ = 1, max_texture_ = 1024;

    // Offscreen targets, the size of the view.
    int width_ = 0, height_ = 0, samples_ = 0;
    unsigned g_fbo_ = 0, g_normal_ = 0, g_depth_ = 0;          // normals and depth
    unsigned ao_fbo_[3] = {}, ao_tex_[3] = {};                  // raw occlusion, blurred once, final
    unsigned ray_fbo_[2] = {}, ray_tex_[2] = {};                // ray traced visibility, ping-pong
    unsigned msaa_fbo_ = 0, msaa_colour_ = 0, msaa_depth_ = 0;  // the lit scene, multisampled
    unsigned colour_fbo_ = 0, colour_ = 0, colour_depth_ = 0;   // the lit scene, resolved

    unsigned ray_nodes_ = 0, ray_prims_ = 0;
    int ray_samples_ = 0, ray_current_ = 0;
    std::uint32_t ray_frame_ = 0;
    std::vector<float> ray_signature_;
};

} // namespace tisma::ui
