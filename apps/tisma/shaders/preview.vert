#version 450
#extension GL_GOOGLE_include_directive : require
#include "view_common.glsl"
// One extrusion per instance, expanded into a prism around the move, as
// renderer.cpp's kPreviewVertex: the cross-section a rhombus as wide as the
// line and as high as the layer, hanging under the nozzle's path; a joined end
// on the mitre plane, a free end half a width past the point with its cap. 18
// vertices: the two sides and the end turned to the eye, two triangles each.
layout(location = 0) in vec3 a_p0;
layout(location = 1) in vec3 a_p1;
layout(location = 2) in vec2 a_size;
layout(location = 3) in float a_feed;
layout(location = 4) in uint a_kind_layer;
layout(location = 5) in vec4 a_join; // the mitre planes' normals at p0 (xy) and p1 (zw); zero at a free end
layout(location = 0) out vec3 v_base;
layout(location = 1) out vec3 v_normal;
layout(location = 2) out vec3 v_position;
layout(location = 3) out vec2 v_section;    // where in the cross-section: -1 to 1 across, -1 to 1 up
layout(location = 4) flat out vec3 v_side;
layout(location = 5) flat out vec3 v_up;
layout(location = 6) flat out vec2 v_size;  // the line's width and height
layout(location = 7) flat out float v_cap;  // 1 on the end caps, 0 on the sides
layout(location = 8) flat out vec2 v_neighbours; // 1 where a bead of the layer above (x) and below (y) touches this one

// The kinds that usually have a bead of the layer above on them, and those
// that lie on one of the layer below (not the top surface, a bridge or an
// overhang): the grooves come only between them, inside the layers shown.
const uint kAbove = 0x39BEu; // walls, overhang walls, infill, bridges, gap fill, support, its interface, wipe tower
const uint kBelow = 0x3F76u; // walls, infill, top surface, gap fill, ironing, skirt, support, its interface, wipe tower

const vec2 kCorner[4] = vec2[4](vec2(1.0, 0.0), vec2(0.0, 1.0), vec2(-1.0, 0.0), vec2(0.0, -1.0));
const ivec2 kQuad[6] = ivec2[6](ivec2(0, 0), ivec2(1, 0), ivec2(1, 1), ivec2(0, 0), ivec2(1, 1), ivec2(0, 1));
const int kCap[6] = int[6](0, 1, 2, 0, 2, 3);

vec3 ramp(float t) {
    t = clamp(t, 0.0, 1.0);
    vec3 a = vec3(0.17, 0.33, 0.95), b = vec3(0.10, 0.80, 0.85), c = vec3(0.30, 0.85, 0.35);
    vec3 d = vec3(0.98, 0.85, 0.20), e = vec3(0.92, 0.22, 0.20);
    if (t < 0.25) return mix(a, b, t * 4.0);
    if (t < 0.5) return mix(b, c, t * 4.0 - 1.0);
    if (t < 0.75) return mix(c, d, t * 4.0 - 2.0);
    return mix(d, e, t * 4.0 - 3.0);
}

void main() {
    uint kind = a_kind_layer & 0xFFu;
    vec3 along = a_p1 - a_p0;
    float len = length(along);
    v_base = vec3(0.0);
    v_normal = vec3(0.0, 0.0, 1.0);
    v_position = a_p0;
    v_section = vec2(0.0);
    v_side = vec3(1.0, 0.0, 0.0);
    v_up = vec3(0.0, 0.0, 1.0);
    v_size = a_size;
    v_cap = 0.0;
    v_neighbours = vec2(0.0);
    bool joined0 = dot(a_join.xy, a_join.xy) > 0.25, joined1 = dot(a_join.zw, a_join.zw) > 0.25;
    int id = gl_VertexIndex;
    if ((draw.preview.x & (1u << kind)) == 0u || len <= 0.0) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0); // outside the view: the triangle is dropped
        return;
    }
    vec3 dir = along / len;
    vec3 side = cross(dir, vec3(0.0, 0.0, 1.0));
    side = dot(side, side) > 1e-8 ? normalize(side) : vec3(1.0, 0.0, 0.0);
    vec3 up = cross(side, dir);
    float w = a_size.x, h = a_size.y;

    // Of the convex prism, only what faces the eye (frame.face_eye: a point
    // with w 1, a direction towards it with w 0): at most two of the four sides,
    // which are parallel in pairs, and one end; the others are behind them.
    // Vertices 0 to 11: those sides; 12 to 17: that end, if it is free (a
    // joined end has no cap: the next move's side goes on from it).
    vec3 mid = 0.5 * (a_p0 + a_p1);
    int face = -1, end = 0;
    if (id < 12) {
        int seen = 0;
        for (int k = 0; k < 4; ++k) {
            vec2 a = kCorner[k], b = kCorner[(k + 1) % 4];
            vec3 n = side * ((a.x + b.x) * h) + up * ((a.y + b.y) * w);
            vec3 on = mid + side * (a.x * 0.5 * w) + up * (a.y * 0.5 * h - 0.5 * h);
            vec3 to = frame.face_eye.w > 0.5 ? frame.face_eye.xyz - on : frame.face_eye.xyz;
            if (dot(n, to) > 0.0) {
                if (seen == id / 6)
                    face = k;
                ++seen;
            }
        }
    } else {
        for (int e = 0; e < 2 && face < 0; ++e) {
            vec3 outwards = e == 0 ? -dir : dir;
            vec3 on = (e == 0 ? a_p0 : a_p1) + outwards * (0.5 * w);
            vec3 to = frame.face_eye.w > 0.5 ? frame.face_eye.xyz - on : frame.face_eye.xyz;
            if (!(e == 0 ? joined0 : joined1) && dot(outwards, to) > 0.0) {
                face = 4;
                end = e;
            }
        }
    }
    if (face < 0) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        return;
    }

    vec2 c;
    vec3 normal;
    if (face < 4) {
        ivec2 q = kQuad[id % 6];
        c = kCorner[(face + q.y) % 4];
        end = q.x;
        normal = normalize(side * c.x * h + up * c.y * w);
    } else {
        c = kCorner[kCap[(id - 12) % 6]];
        normal = end == 0 ? -dir : dir;
        v_cap = 1.0;
    }
    vec3 offset = side * (c.x * 0.5 * w) + up * (c.y * 0.5 * h - 0.5 * h);
    vec3 point = end == 0 ? a_p0 : a_p1;
    bool joined = end == 0 ? joined0 : joined1;
    float t;
    if (joined) {
        vec3 n = vec3(end == 0 ? a_join.xy : a_join.zw, 0.0);
        t = -dot(offset, n) / max(dot(dir, n), 1e-3);
    } else {
        t = (end == 0 ? -0.5 : 0.5) * w;
    }
    vec3 position = point + offset + dir * t;

    v_section = c;
    v_side = side;
    v_up = up;
    v_size = vec2(w, h);
    vec3 base;
    uint mode = draw.preview.y;
    vec2 range = draw.range.xy;
    if (mode == 1u)
        base = ramp((a_feed - range.x) / max(range.y - range.x, 1e-6));
    else if (mode == 2u)
        base = ramp((w - range.x) / max(range.y - range.x, 1e-6));
    else if (mode == 3u)
        base = ramp((h - range.x) / max(range.y - range.x, 1e-6));
    else
        base = frame.colours[kind].rgb;
    v_base = pow(base, vec3(2.2));
    v_normal = normal;
    v_position = position;
    // draw.preview.zw: the first and the last layer shown; draw.range.z: the
    // grooves on (not with ray tracing, whose rays see the neighbours).
    uint layer = a_kind_layer >> 8;
    bool grooves = draw.range.z > 0.5 && v_cap < 0.5;
    v_neighbours = vec2(grooves && layer < draw.preview.w && (kAbove & (1u << kind)) != 0u ? 1.0 : 0.0,
                        grooves && layer > draw.preview.z && (kBelow & (1u << kind)) != 0u ? 1.0 : 0.0);
    gl_Position = to_vulkan(frame.view_projection * vec4(position, 1.0));
}
