// The interface's 3D view in OpenGL 3.3. Own design; see PROVENANCE.md.

#include "renderer.hpp"

#include "gl.hpp"

#include <algorithm>

namespace tisma::ui {
namespace {

// A full-screen triangle with a vertical gradient.
constexpr const char* kBackgroundVertex = R"(#version 330 core
out float v_y;
void main() {
    vec2 p = vec2(float((gl_VertexID & 1) << 2) - 1.0, float((gl_VertexID & 2) << 1) - 1.0);
    v_y = p.y * 0.5 + 0.5;
    gl_Position = vec4(p, 0.0, 1.0);
}
)";
constexpr const char* kBackgroundFragment = R"(#version 330 core
in float v_y;
uniform vec3 u_top;
uniform vec3 u_bottom;
out vec4 colour;
void main() { colour = vec4(mix(u_bottom, u_top, v_y), 1.0); }
)";

// Positions in one colour (the plate).
constexpr const char* kFlatVertex = R"(#version 330 core
layout(location = 0) in vec3 a_position;
uniform mat4 u_view_projection;
void main() { gl_Position = u_view_projection * vec4(a_position, 1.0); }
)";
constexpr const char* kFlatFragment = R"(#version 330 core
uniform vec3 u_colour;
out vec4 colour;
void main() { colour = vec4(u_colour, 1.0); }
)";

// Lines with a colour per vertex; the colour comes as bytes B, G, R, A.
constexpr const char* kLineVertex = R"(#version 330 core
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec4 a_colour;
uniform mat4 u_view_projection;
out vec4 v_colour;
void main() {
    v_colour = a_colour.zyxw;
    gl_Position = u_view_projection * vec4(a_position, 1.0);
}
)";
constexpr const char* kLineFragment = R"(#version 330 core
in vec4 v_colour;
out vec4 colour;
void main() { colour = v_colour; }
)";

// The model: flat-shaded triangles, lit from both sides.
constexpr const char* kMeshVertex = R"(#version 330 core
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
uniform mat4 u_view_projection;
out vec3 v_normal;
void main() {
    v_normal = a_normal;
    gl_Position = u_view_projection * vec4(a_position, 1.0);
}
)";
constexpr const char* kMeshFragment = R"(#version 330 core
in vec3 v_normal;
uniform vec3 u_colour;
uniform vec3 u_light;
out vec4 colour;
void main() {
    vec3 n = normalize(gl_FrontFacing ? v_normal : -v_normal);
    float diffuse = max(dot(n, u_light), 0.0);
    float sky = 0.5 + 0.5 * n.z;
    colour = vec4(u_colour * (0.22 + 0.62 * diffuse + 0.16 * sky), 1.0);
}
)";

// One extrusion per instance, expanded into a prism around the move: the
// cross-section is a rhombus as wide as the line and as high as the layer,
// hanging under the nozzle's path, with normals of an ellipse so it shades
// round; each end grows by half a width so corners close. 36 vertices: four
// sides of two triangles and two ends of two triangles.
constexpr const char* kPreviewVertex = R"(#version 330 core
layout(location = 0) in vec3 a_p0;
layout(location = 1) in vec3 a_p1;
layout(location = 2) in vec2 a_size;
layout(location = 3) in float a_feed;
layout(location = 4) in uint a_kind_layer;
uniform mat4 u_view_projection;
uniform vec3 u_light;
uniform vec3 u_colours[16];
uniform uint u_kind_mask;
uniform int u_colour_mode;
uniform vec2 u_range;
out vec3 v_colour;

const vec2 kCorner[4] = vec2[4](vec2(1.0, 0.0), vec2(0.0, 1.0), vec2(-1.0, 0.0), vec2(0.0, -1.0));
const ivec2 kQuad[6] = ivec2[6](ivec2(0, 0), ivec2(1, 0), ivec2(1, 1), ivec2(0, 0), ivec2(1, 1), ivec2(0, 1));
const int kCap[6] = int[6](0, 1, 2, 0, 2, 3);

vec3 ramp(float t) {
    t = clamp(t, 0.0, 1.0);
    vec3 a = vec3(0.17, 0.33, 0.95), b = vec3(0.10, 0.80, 0.85), c = vec3(0.30, 0.85, 0.35);
    vec3 d = vec3(0.98, 0.85, 0.20), e = vec3(0.92, 0.22, 0.20);
    if (t < 0.25) return mix(a, b, t * 4.0);
    if (t < 0.5) return mix(b, c, t * 4.0 - 1.0);
    if (t < 0.75) return mix(c, d, t * 4.0 - 2.0);
    return mix(d, e, t * 4.0 - 3.0);
}

void main() {
    uint kind = a_kind_layer & 0xFFu;
    vec3 along = a_p1 - a_p0;
    float len = length(along);
    if ((u_kind_mask & (1u << kind)) == 0u || len <= 0.0) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0); // outside the view: the triangle is dropped
        v_colour = vec3(0.0);
        return;
    }
    vec3 dir = along / len;
    vec3 side = cross(dir, vec3(0.0, 0.0, 1.0));
    side = dot(side, side) > 1e-8 ? normalize(side) : vec3(1.0, 0.0, 0.0);
    vec3 up = cross(side, dir);
    float w = a_size.x, h = a_size.y;
    vec3 a = a_p0 - dir * (0.5 * w) - up * (0.5 * h);
    vec3 b = a_p1 + dir * (0.5 * w) - up * (0.5 * h);

    vec3 position, normal;
    int id = gl_VertexID;
    if (id < 24) {
        ivec2 q = kQuad[id % 6];
        vec2 c = kCorner[(id / 6 + q.y) % 4];
        position = (q.x == 0 ? a : b) + side * (c.x * 0.5 * w) + up * (c.y * 0.5 * h);
        normal = normalize(side * c.x * h + up * c.y * w);
    } else {
        int end = (id - 24) / 6;
        vec2 c = kCorner[kCap[(id - 24) % 6]];
        position = (end == 0 ? a : b) + side * (c.x * 0.5 * w) + up * (c.y * 0.5 * h);
        normal = end == 0 ? -dir : dir;
    }

    vec3 base;
    if (u_colour_mode == 1)
        base = ramp((a_feed - u_range.x) / max(u_range.y - u_range.x, 1e-6));
    else if (u_colour_mode == 2)
        base = ramp((w - u_range.x) / max(u_range.y - u_range.x, 1e-6));
    else
        base = u_colours[kind];
    float diffuse = max(dot(normal, u_light), 0.0);
    float sky = 0.5 + 0.5 * normal.z;
    v_colour = base * (0.25 + 0.6 * diffuse + 0.2 * sky);
    gl_Position = u_view_projection * vec4(position, 1.0);
}
)";
constexpr const char* kPreviewFragment = R"(#version 330 core
in vec3 v_colour;
out vec4 colour;
void main() { colour = vec4(v_colour, 1.0); }
)";

unsigned compile(GLenum type, const char* source, std::string& error) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048] = {};
        glGetShaderInfoLog(shader, sizeof log, nullptr, log);
        error = std::string("shader: ") + log;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

unsigned program(const char* vertex, const char* fragment, std::string& error) {
    const unsigned v = compile(GL_VERTEX_SHADER, vertex, error);
    if (!v)
        return 0;
    const unsigned f = compile(GL_FRAGMENT_SHADER, fragment, error);
    if (!f) {
        glDeleteShader(v);
        return 0;
    }
    const GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    glDetachShader(p, v);
    glDetachShader(p, f);
    glDeleteShader(v);
    glDeleteShader(f);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048] = {};
        glGetProgramInfoLog(p, sizeof log, nullptr, log);
        error = std::string("program: ") + log;
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

void upload(unsigned& vao, unsigned& vbo, const void* data, std::size_t bytes) {
    if (!vao)
        glGenVertexArrays(1, &vao);
    if (!vbo)
        glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(bytes), data, GL_STATIC_DRAW);
}

const void* offset(std::size_t bytes) { return reinterpret_cast<const void*>(bytes); }

void uniform(unsigned p, const char* name, const Mat4& m) {
    glUniformMatrix4fv(glGetUniformLocation(p, name), 1, GL_FALSE, m.data());
}
void uniform(unsigned p, const char* name, Vec3 v) { glUniform3f(glGetUniformLocation(p, name), v.x, v.y, v.z); }

} // namespace

bool Renderer::init(std::string& error) {
    background_program_ = program(kBackgroundVertex, kBackgroundFragment, error);
    flat_program_ = background_program_ ? program(kFlatVertex, kFlatFragment, error) : 0;
    line_program_ = flat_program_ ? program(kLineVertex, kLineFragment, error) : 0;
    mesh_program_ = line_program_ ? program(kMeshVertex, kMeshFragment, error) : 0;
    preview_program_ = mesh_program_ ? program(kPreviewVertex, kPreviewFragment, error) : 0;
    if (!preview_program_)
        return false;
    glGenVertexArrays(1, &empty_vao_);
    return true;
}

void Renderer::shutdown() {
    for (unsigned p : {background_program_, flat_program_, line_program_, mesh_program_, preview_program_})
        if (p)
            glDeleteProgram(p);
    for (Buffer* b : {&plate_, &grid_, &model_, &extrusions_, &travels_}) {
        if (b->vbo)
            glDeleteBuffers(1, &b->vbo);
        if (b->vao)
            glDeleteVertexArrays(1, &b->vao);
        *b = {};
    }
    if (empty_vao_)
        glDeleteVertexArrays(1, &empty_vao_);
    empty_vao_ = 0;
}

void Renderer::set_bed(const BedPolygon& bed, const BedLook& look) {
    const std::vector<float> plate = bed_triangles(bed);
    upload(plate_.vao, plate_.vbo, plate.data(), plate.size() * sizeof(float));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), offset(0));
    plate_.count = static_cast<int>(plate.size() / 3);

    const std::vector<LineVertex> lines = bed_lines(bed, look);
    upload(grid_.vao, grid_.vbo, lines.data(), lines.size() * sizeof(LineVertex));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex), offset(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(LineVertex), offset(offsetof(LineVertex, argb)));
    grid_.count = static_cast<int>(lines.size());
}

void Renderer::set_model(const std::vector<float>& vertices) {
    upload(model_.vao, model_.vbo, vertices.data(), vertices.size() * sizeof(float));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), offset(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), offset(3 * sizeof(float)));
    model_.count = static_cast<int>(vertices.size() / 6);
}

void Renderer::set_preview(const PreviewBuffers& p) {
    upload(extrusions_.vao, extrusions_.vbo, p.extrusions.data(), p.extrusions.size() * sizeof(PreviewInstance));
    extrusions_.count = static_cast<int>(p.extrusions.size());
    upload(travels_.vao, travels_.vbo, p.travels.data(), p.travels.size() * sizeof(LineVertex));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex), offset(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(LineVertex), offset(offsetof(LineVertex, argb)));
    travels_.count = static_cast<int>(p.travels.size());
    layer_extrusions_ = p.layer_extrusions;
    layer_travels_ = p.layer_travels;
    min_feed_ = p.min_feed, max_feed_ = p.max_feed;
    min_width_ = p.min_width, max_width_ = p.max_width;
}

void Renderer::clear_preview() {
    extrusions_.count = travels_.count = 0;
    layer_extrusions_.clear();
    layer_travels_.clear();
}

void Renderer::draw(const ViewFrame& f) {
    glViewport(f.x, f.y, f.width, f.height);
    glEnable(GL_SCISSOR_TEST);
    glScissor(f.x, f.y, f.width, f.height);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glEnable(GL_MULTISAMPLE);
    glDepthMask(GL_TRUE);
    glClear(GL_DEPTH_BUFFER_BIT);

    glDisable(GL_DEPTH_TEST);
    glUseProgram(background_program_);
    uniform(background_program_, "u_top", f.background_top);
    uniform(background_program_, "u_bottom", f.background_bottom);
    glBindVertexArray(empty_vao_);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    const Mat4 vp = f.projection * f.view;
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    if (plate_.count > 0) {
        // Pushed back a little so the grid, and the first layer, draw over it.
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.0f, 1.0f);
        glUseProgram(flat_program_);
        uniform(flat_program_, "u_view_projection", vp);
        uniform(flat_program_, "u_colour", f.plate);
        glBindVertexArray(plate_.vao);
        glDrawArrays(GL_TRIANGLES, 0, plate_.count);
        glDisable(GL_POLYGON_OFFSET_FILL);
    }
    if (grid_.count > 0) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        glUseProgram(line_program_);
        uniform(line_program_, "u_view_projection", vp);
        glBindVertexArray(grid_.vao);
        glDrawArrays(GL_LINES, 0, grid_.count);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
    if (f.show_model && model_.count > 0) {
        glUseProgram(mesh_program_);
        uniform(mesh_program_, "u_view_projection", vp);
        uniform(mesh_program_, "u_colour", f.model);
        uniform(mesh_program_, "u_light", f.light);
        glBindVertexArray(model_.vao);
        glDrawArrays(GL_TRIANGLES, 0, model_.count);
    }
    if (f.show_preview)
        draw_preview(f, vp);
    glBindVertexArray(0);
    glUseProgram(0);
    glDisable(GL_SCISSOR_TEST);
}

void Renderer::draw_preview(const ViewFrame& f, const Mat4& vp) {
    if (layer_extrusions_.size() < 2)
        return;
    const std::size_t layers = layer_extrusions_.size() - 1;
    const std::size_t lo = std::min(f.first_layer, layers - 1), hi = std::clamp(f.last_layer, lo, layers - 1);
    const std::size_t first = layer_extrusions_[lo], count = layer_extrusions_[hi + 1] - first;
    if (count > 0) {
        glUseProgram(preview_program_);
        uniform(preview_program_, "u_view_projection", vp);
        uniform(preview_program_, "u_light", f.light);
        glUniform3fv(glGetUniformLocation(preview_program_, "u_colours"), static_cast<GLsizei>(kLineKinds),
                     &f.kind_colours[0].x);
        glUniform1ui(glGetUniformLocation(preview_program_, "u_kind_mask"), f.kind_mask);
        glUniform1i(glGetUniformLocation(preview_program_, "u_colour_mode"), static_cast<GLint>(f.color_mode));
        if (f.color_mode == ColorMode::Speed)
            glUniform2f(glGetUniformLocation(preview_program_, "u_range"), min_feed_, max_feed_);
        else
            glUniform2f(glGetUniformLocation(preview_program_, "u_range"), min_width_, max_width_);
        // The instances of the first layer drawn start the attribute arrays.
        glBindVertexArray(extrusions_.vao);
        glBindBuffer(GL_ARRAY_BUFFER, extrusions_.vbo);
        const std::size_t base = first * sizeof(PreviewInstance);
        const GLsizei stride = sizeof(PreviewInstance);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, offset(base + offsetof(PreviewInstance, x0)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, offset(base + offsetof(PreviewInstance, x1)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, offset(base + offsetof(PreviewInstance, width)));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, offset(base + offsetof(PreviewInstance, feed)));
        glEnableVertexAttribArray(4);
        glVertexAttribIPointer(4, 1, GL_UNSIGNED_INT, stride, offset(base + offsetof(PreviewInstance, kind_layer)));
        for (GLuint a = 0; a < 5; ++a)
            glVertexAttribDivisor(a, 1);
        glDrawArraysInstanced(GL_TRIANGLES, 0, 36, static_cast<GLsizei>(count));
    }
    if (f.show_travels && travels_.count > 0 && layer_travels_.size() > hi + 1) {
        const std::size_t t0 = layer_travels_[lo], tn = layer_travels_[hi + 1] - t0;
        if (tn > 0) {
            glUseProgram(line_program_);
            uniform(line_program_, "u_view_projection", vp);
            glBindVertexArray(travels_.vao);
            glDrawArrays(GL_LINES, static_cast<GLint>(t0), static_cast<GLsizei>(tn));
        }
    }
}

} // namespace tisma::ui
