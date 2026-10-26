#version 450
#extension GL_GOOGLE_include_directive : require
#include "view_common.glsl"
#include "view_lighting.glsl"
layout(location = 0) in vec3 v_position;
void main() {
    if (gbuffer(vec3(0.0, 0.0, 1.0)))
        return;
    colour = vec4(shade(to_linear(draw.colour.rgb), vec3(0.0, 0.0, 1.0), v_position, 16.0, 0.15), 1.0);
}
