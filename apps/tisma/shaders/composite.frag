#version 450
// The lit view into the window, with FXAA when asked, as renderer.cpp's
// kCompositeFragment.
layout(set = 0, binding = 0) uniform sampler2D source;
layout(push_constant) uniform Composite {
    vec2 size; // the view's size in pixels
    int fxaa;
} pc;
layout(location = 0) out vec4 colour;
float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }
float luma_at(vec2 uv) { return luma(texture(source, uv).rgb); }
void main() {
    vec2 texel = 1.0 / pc.size;
    vec2 uv = gl_FragCoord.xy * texel;
    vec3 c = texture(source, uv).rgb;
    if (pc.fxaa == 0) {
        colour = vec4(c, 1.0);
        return;
    }
    float m = luma(c);
    float n = luma_at(uv + vec2(0.0, texel.y)), s = luma_at(uv - vec2(0.0, texel.y));
    float e = luma_at(uv + vec2(texel.x, 0.0)), w = luma_at(uv - vec2(texel.x, 0.0));
    float hi = max(max(max(n, s), max(e, w)), m), lo = min(min(min(n, s), min(e, w)), m);
    float range = hi - lo;
    if (range < max(0.0312, hi * 0.125)) {
        colour = vec4(c, 1.0);
        return;
    }
    float ne = luma_at(uv + texel), sw = luma_at(uv - texel);
    float nw = luma_at(uv + vec2(-texel.x, texel.y)), se = luma_at(uv + vec2(texel.x, -texel.y));
    float average = (2.0 * (n + s + e + w) + ne + nw + se + sw) / 12.0;
    float sub = clamp(abs(average - m) / range, 0.0, 1.0);
    sub = (-2.0 * sub + 3.0) * sub * sub;
    float sub_blend = sub * sub * 0.75;
    float across_y = abs(n + s - 2.0 * m) * 2.0 + abs(ne + se - 2.0 * e) + abs(nw + sw - 2.0 * w);
    float across_x = abs(e + w - 2.0 * m) * 2.0 + abs(ne + nw - 2.0 * n) + abs(se + sw - 2.0 * s);
    bool along_x = across_y >= across_x;
    float side_a = along_x ? n : e, side_b = along_x ? s : w;
    float step = along_x ? texel.y : texel.x;
    float edge, gradient;
    if (abs(side_a - m) >= abs(side_b - m)) {
        edge = 0.5 * (side_a + m);
        gradient = abs(side_a - m);
    } else {
        edge = 0.5 * (side_b + m);
        gradient = abs(side_b - m);
        step = -step;
    }
    vec2 on = uv + (along_x ? vec2(0.0, step * 0.5) : vec2(step * 0.5, 0.0));
    vec2 dir = along_x ? vec2(texel.x, 0.0) : vec2(0.0, texel.y);
    float threshold = gradient * 0.25;
    vec2 pa = on - dir, pb = on + dir;
    float la = luma_at(pa) - edge, lb = luma_at(pb) - edge;
    bool done_a = abs(la) >= threshold, done_b = abs(lb) >= threshold;
    const float kSteps[10] = float[10](1.0, 1.0, 1.0, 1.5, 2.0, 2.0, 2.0, 2.0, 4.0, 8.0);
    for (int i = 0; i < 10 && !(done_a && done_b); ++i) {
        if (!done_a) {
            pa -= dir * kSteps[i];
            la = luma_at(pa) - edge;
            done_a = abs(la) >= threshold;
        }
        if (!done_b) {
            pb += dir * kSteps[i];
            lb = luma_at(pb) - edge;
            done_b = abs(lb) >= threshold;
        }
    }
    float da = along_x ? uv.x - pa.x : uv.y - pa.y;
    float db = along_x ? pb.x - uv.x : pb.y - uv.y;
    bool a_closer = da < db;
    float offset = 0.5 - min(da, db) / (da + db);
    bool right_way = ((a_closer ? la : lb) < 0.0) != (m < edge);
    float blend = max(right_way ? offset : 0.0, sub_blend);
    vec2 final_uv = uv + (along_x ? vec2(0.0, blend * step) : vec2(blend * step, 0.0));
    colour = vec4(texture(source, final_uv).rgb, 1.0);
}
