#pragma once

#include "ui/math.hpp"
#include "ui/scene.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace tisma::ui {

enum class ColorMode : int { Kind = 0, Speed = 1, Width = 2 };

// What one frame of the 3D view shows, and how.
struct ViewFrame {
    int x = 0, y = 0, width = 1, height = 1; // the view's rectangle in framebuffer pixels, from the bottom left
    Mat4 view = identity(), projection = identity();
    Vec3 light{0, 0, 1};     // towards the light, world space
    Vec3 background_top{0.17f, 0.17f, 0.19f}, background_bottom{0.10f, 0.10f, 0.12f};
    Vec3 plate{0.20f, 0.20f, 0.23f};
    Vec3 model{0.62f, 0.37f, 0.86f};
    bool show_model = true;
    bool show_preview = false;
    bool show_travels = false;
    std::size_t first_layer = 0, last_layer = 0; // the preview's layers drawn, inclusive
    std::uint32_t kind_mask = 0xFFFFFFFFu;        // bit k: LineKind k is drawn
    ColorMode color_mode = ColorMode::Kind;
    std::array<Vec3, kLineKinds> kind_colours{};
};

// Draws the bed, the model and the G-code preview with OpenGL 3.3. Every
// method needs the OpenGL context current. Own design; see PROVENANCE.md.
class Renderer {
public:
    // Compiles the shaders and makes the buffers; false with a message if the
    // driver refuses them.
    bool init(std::string& error);
    void shutdown();

    void set_bed(const BedPolygon& bed, const BedLook& look);
    void set_model(const std::vector<float>& vertices); // mesh_vertices
    void set_preview(const PreviewBuffers& preview);
    void clear_preview();

    void draw(const ViewFrame& frame);

private:
    struct Buffer {
        unsigned vao = 0, vbo = 0;
        int count = 0;
    };
    void draw_preview(const ViewFrame& frame, const Mat4& view_projection);

    unsigned background_program_ = 0, flat_program_ = 0, line_program_ = 0, mesh_program_ = 0, preview_program_ = 0;
    unsigned empty_vao_ = 0;
    Buffer plate_, grid_, model_, extrusions_, travels_;
    std::vector<std::size_t> layer_extrusions_, layer_travels_;
    float min_feed_ = 0, max_feed_ = 0, min_width_ = 0, max_width_ = 0;
};

} // namespace tisma::ui
