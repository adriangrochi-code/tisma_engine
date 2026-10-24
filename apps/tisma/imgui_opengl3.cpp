// Dear ImGui's OpenGL 3 backend (MIT, see THIRD_PARTY.md), compiled against the
// interface's own OpenGL loader instead of the one it ships.
#include "gl.hpp"

#define IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#include "imgui_impl_opengl3.cpp"
