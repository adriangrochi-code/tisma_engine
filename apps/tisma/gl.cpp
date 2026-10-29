// The interface's OpenGL loader. Own design; see PROVENANCE.md.

#include "gl.hpp"

#define TISMA_GL_DEFINE(ret, name, params) PFN_##name tisma_##name = nullptr;
TISMA_GL_FUNCTIONS(TISMA_GL_DEFINE)
#undef TISMA_GL_DEFINE

namespace tisma::ui {

const char* load_gl(GlGetProc get) {
    const char* missing = nullptr;
#define TISMA_GL_LOAD(ret, name, params)                                                                             \
    tisma_##name = reinterpret_cast<PFN_##name>(get(#name));                                                         \
    if (!tisma_##name && !missing)                                                                                   \
        missing = #name;
    TISMA_GL_FUNCTIONS(TISMA_GL_LOAD)
#undef TISMA_GL_LOAD
    return missing;
}

namespace {

DrawStats g_stats;
PFN_glDrawArrays g_draw_arrays = nullptr;
PFN_glDrawArraysInstanced g_draw_arrays_instanced = nullptr;
PFN_glDrawElements g_draw_elements = nullptr;
PFN_glDrawElementsBaseVertex g_draw_elements_base = nullptr;

void TISMA_GLAPI counted_arrays(GLenum mode, GLint first, GLsizei count) {
    ++g_stats.calls, g_stats.vertices += count;
    g_draw_arrays(mode, first, count);
}
void TISMA_GLAPI counted_arrays_instanced(GLenum mode, GLint first, GLsizei count, GLsizei instances) {
    ++g_stats.calls, g_stats.vertices += static_cast<long long>(count) * instances;
    g_draw_arrays_instanced(mode, first, count, instances);
}
void TISMA_GLAPI counted_elements(GLenum mode, GLsizei count, GLenum type, const void* indices) {
    ++g_stats.calls, g_stats.vertices += count;
    g_draw_elements(mode, count, type, indices);
}
void TISMA_GLAPI counted_elements_base(GLenum mode, GLsizei count, GLenum type, const void* indices, GLint base) {
    ++g_stats.calls, g_stats.vertices += count;
    g_draw_elements_base(mode, count, type, indices, base);
}

} // namespace

void count_draws() {
    if (g_draw_arrays)
        return;
    g_draw_arrays = tisma_glDrawArrays, tisma_glDrawArrays = counted_arrays;
    g_draw_arrays_instanced = tisma_glDrawArraysInstanced, tisma_glDrawArraysInstanced = counted_arrays_instanced;
    g_draw_elements = tisma_glDrawElements, tisma_glDrawElements = counted_elements;
    g_draw_elements_base = tisma_glDrawElementsBaseVertex, tisma_glDrawElementsBaseVertex = counted_elements_base;
}

DrawStats take_draw_stats() {
    const DrawStats s = g_stats;
    g_stats = {};
    return s;
}

} // namespace tisma::ui
