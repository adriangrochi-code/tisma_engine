#version 450
#extension GL_GOOGLE_include_directive : require
#include "view_common.glsl"
// The bed's grid, as renderer.cpp's: with the moiré filter, a line fades as
// its neighbours come closer than a few pixels on the screen; the major lines,
// further apart, stay longer.
layout(location = 0) in vec4 v_colour;
layout(location = 1) in vec3 v_position;
layout(location = 0) out vec4 colour;
vec2 screen(vec3 p) {
    vec4 c = frame.view_projection * vec4(p, 1.0);
    return c.xy / max(c.w, 1e-6) * 0.5 * frame.target.xy;
}
void main() {
    float alpha = v_colour.a;
    if (frame.flags.z != 0) {
        float ax = abs(dFdx(v_position.x)) + abs(dFdy(v_position.x));
        float ay = abs(dFdx(v_position.y)) + abs(dFdy(v_position.y));
        bool x_stays = ax < ay;
        vec3 across = x_stays ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0);
        vec3 along = x_stays ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
        float step = frame.grid.x;
        int major = int(frame.grid.y);
        int index = int(abs(round((x_stays ? v_position.x : v_position.y) / step)));
        float spacing = step * (index % major == 0 ? float(major) : 1.0);
        vec2 s = screen(v_position);
        vec2 d = screen(v_position + along * spacing) - s;
        d /= max(length(d), 1e-6);
        vec2 n = screen(v_position + across * spacing) - s;
        alpha *= smoothstep(2.0, 6.0, abs(d.x * n.y - d.y * n.x));
    }
    colour = vec4(v_colour.rgb, alpha);
}
