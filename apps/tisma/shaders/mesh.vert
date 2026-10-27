#version 450
#extension GL_GOOGLE_include_directive : require
#include "view_common.glsl"
// The model: flat-shaded triangles, lit from both sides.
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 0) out vec3 v_position;
layout(location = 1) out vec3 v_normal;
void main() {
    v_position = a_position + draw.extra.xyz;
    v_normal = a_normal;
    gl_Position = to_vulkan(frame.view_projection * vec4(v_position, 1.0));
}
