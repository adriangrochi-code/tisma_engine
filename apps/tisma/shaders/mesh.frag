#version 450
#extension GL_GOOGLE_include_directive : require
#include "view_common.glsl"
#include "view_lighting.glsl"
layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_normal;
void main() {
    vec3 n = normalize(v_normal);
    if (gbuffer(n))
        return;
    colour = vec4(shade(to_linear(draw.colour.rgb), n, v_position, 64.0, 0.5), 1.0);
}
