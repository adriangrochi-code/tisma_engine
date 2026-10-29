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
    // And turned and scaled by this (an affine map of its mesh's points): a
    // large model while its rotation or scale field is dragged, before its
    // mesh is redone.
    Mat4 selected_model = identity();
    bool show_model = true;
    // The coarse copy of the model while the view moves (set_model_coarse,
    // PLAN.md V6), with the chosen model's vertices in it.
    bool coarse = false;
    std::size_t coarse_selected_first = 0, coarse_selected_count = 0;
    bool show_paint = false; // the painted faces (set_paint) over the model
    bool show_preview = false;
    bool show_travels = false;
    std::size_t first_layer = 0, last_layer = 0; // the preview's layers drawn, inclusive
    // The top layer's extrusions drawn up to the move slider (shown, hidden).
    std::size_t top_shown = std::size_t(-1), top_hidden = std::size_t(-1);
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

// Everything in a frame that changes its picture: two frames with the same
// key and the same data draw the same picture.
std::vector<double> frame_key(const ViewFrame& f);

// The preview's extrusions are prisms of which the vertex shader draws only
// the faces turned to the eye: two sides and an end, 18 vertices each.
inline constexpr int kPreviewVertices = 18;
// What those faces face: the eye (w 1), or the direction towards it for an
// orthographic camera (w 0).
std::array<float, 4> preview_face_eye(const ViewFrame& f);

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
    // A coarse copy (clustered, then mesh_vertices) drawn instead while the
    // view moves; empty: none.
    void set_model_coarse(const std::vector<float>& vertices);
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

    // Draws the frame: the scene only if the frame or the data changed since
    // the last one drawn (or rays are still averaging), else the last picture
    // again (the QA team's preview plan, step 2).
    void draw(const ViewFrame& frame);
    // The next draw draws the scene even if nothing changed (--bench-frames).
    void redraw() { drawn_.clear(); }

    // After a ray traced frame: its G-buffer and visibility.
    RayReadback read_back();

    // After a frame: the lit scene before FXAA, RGBA rows from the bottom.
    std::vector<std::uint8_t> read_colour();

    // The preview's extrusions the last frame drew (lit pass).
    std::size_t drawn_extrusions() const { return drawn_extrusions_; }
    int drawn_group() const { return drawn_group_; } // the last frame's group of layers drawn as one (merged_layers)

    // What the view keeps on the GPU, in bytes (--draw-stats): the preview's
    // instances and travels, the meshes (model, its coarse copy, paint, bed),
    // and the targets (G-buffer, occlusion, rays, colour, multisampling, the
    // shadow map). The ray scene's textures are not counted.
    struct Bytes {
        std::size_t instances = 0, travels = 0, meshes = 0, targets = 0;
    };
    Bytes bytes() const;

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
    // casters: only what casts a shadow (not the bed), for the shadow map.
    void draw_scene(const ViewFrame& f, const Mat4& vp, bool gbuffer, unsigned visibility, bool casters = false);
    void lit_uniforms(unsigned program, const ViewFrame& f, const Mat4& vp, bool gbuffer, unsigned visibility);
    void draw_preview(const ViewFrame& f, const Mat4& vp, bool gbuffer, unsigned visibility);
    unsigned ambient_pass(const ViewFrame& f);
    void shadow_pass(const ViewFrame& f);
    unsigned ray_pass(const ViewFrame& f);

    bool ready_ = false; // init succeeded: without it (a Vulkan window), every call does nothing
    Programs p_;
    unsigned empty_vao_ = 0, white_ = 0;
    Buffer plate_, grid_, model_, model_coarse_, paint_, extrusions_, travels_;
    std::vector<PaintGroup> paint_groups_;
    int grid_outline_ = 0;                    // the outline's vertices, after the grid's in grid_
    float grid_step_ = 10.0f;                 // mm between grid lines
    int grid_major_every_ = 5;
    std::vector<std::size_t> layer_extrusions_, layer_travels_;
    // The extrusions in drawing order, and as seen from far (docs/LARGE_MODELS.md); their instances moved to the GPU.
    PreviewOrders orders_;
    float layer_height_ = 0.0f; // the preview's mean layer height, mm
    int drawn_group_ = 1;       // the last frame's group of layers drawn as one
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

    // The key light's shadow map (the ambient occlusion mode): the depth of
    // the model and the preview seen from the light, drawn when the occlusion
    // is; the box they fill, for the light's camera.
    static constexpr int kShadowMapSize = 2048;
    unsigned shadow_fbo_ = 0, shadow_depth_ = 0;
    int shadow_size_ = 0;
    Mat4 shadow_vp_ = identity();
    float shadow_texel_ = 0.0f; // mm a texel of the map covers
    float shadow_bias_ = 0.0f;  // 0.6 mm in the map's depth
    bool shadowed_ = false;
    Vec3 model_lo_, model_hi_, preview_lo_, preview_hi_;
    bool model_box_ = false, preview_box_ = false;

    unsigned ray_nodes_ = 0, ray_prims_ = 0;
    int ray_samples_ = 0, ray_current_ = 0;
    std::uint32_t ray_frame_ = 0;
    std::vector<float> ray_signature_;
    std::vector<double> drawn_; // the frame in colour_ (frame_key); empty: none
    std::array<float, 4> face_eye_{0, 0, 1, 0}; // the preview's faces drawn face this (preview_face_eye)
    void scene_changed();       // the data changed: averaging, occlusion and picture start again
};

} // namespace tisma::ui
