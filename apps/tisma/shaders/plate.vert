#version 450
#extension GL_GOOGLE_include_directive : require
#include "view_common.glsl"
// The plate: flat, facing up.
layout(location = 0) in vec3 a_position;
layout(location = 0) out vec3 v_position;
void main() {
    v_position = a_position;
    gl_Position = to_vulkan(frame.view_projection * vec4(a_position, 1.0));
}
