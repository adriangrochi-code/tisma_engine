#version 450
#extension GL_GOOGLE_include_directive : require
#include "view_common.glsl"
#include "view_lighting.glsl"
layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_normal;
void main() {
    // The section (cad_design.md, rule 11 novies): only the models are cut.
    bool cut = draw.range.w > 0.5 && dot(frame.section.xyz, frame.section.xyz) > 0.0;
    if (cut && dot(frame.section.xyz, v_position) > frame.section.w)
        discard;
    vec3 n = normalize(v_normal);
    vec3 base = draw.colour.rgb;
    // The cap: the inside's back faces seen through the cut, drawn flat as
    // the plane and a little darker, so the cut looks solid.
    if (cut && !gl_FrontFacing) {
        n = normalize(frame.section.xyz);
        base = draw.colour.rgb * 0.72;
    }
    if (gbuffer(n))
        return;
    colour = vec4(shade(to_linear(base), n, v_position, 64.0, 0.5), 1.0);
}
