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

} // namespace tisma::ui
