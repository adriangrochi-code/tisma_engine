#version 450
#extension GL_GOOGLE_include_directive : require
#include "view_common.glsl"
// Lines with a colour per vertex; the colour comes as bytes B, G, R, A.
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec4 a_colour;
layout(location = 0) out vec4 v_colour;
layout(location = 1) out vec3 v_position;
void main() {
    v_colour = a_colour.zyxw;
    v_position = a_position;
    gl_Position = to_vulkan(frame.view_projection * vec4(a_position, 1.0));
}
