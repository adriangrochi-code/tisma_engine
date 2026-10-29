// The interface's 3D view in OpenGL 3.3. Own design on the cited methods; see
// PROVENANCE.md.
//
// A frame, in the view's rectangle:
// 1. (ambient occlusion or ray tracing) the scene's view-space normals and
//    depth, one sample per pixel;
// 2. how much of the key light and of the ambient light reaches each pixel:
//    screen-space ambient occlusion from those normals and depth, or shadow
//    and occlusion rays traced against RayScene and averaged over frames;
// 3. the lit scene, multisampled, with that visibility in its light;
// 4. resolved, smoothed (FXAA) and copied into the window.

#include "renderer.hpp"

#include "gl.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

namespace tisma::ui {
namespace {

constexpr GLint kVisibilityUnit = 3; // the texture unit of the visibility the lit shaders read

const std::string kVersion = "#version 330 core\n";

// A triangle that covers the screen.
const std::string kScreenVertex = kVersion + R"(
void main() {
    vec2 p = vec2(float((gl_VertexID & 1) << 2) - 1.0, float((gl_VertexID & 2) << 1) - 1.0);
    gl_Position = vec4(p, 0.0, 1.0);
}
)";

const std::string kBackgroundFragment = kVersion + R"(
uniform vec3 u_top;
uniform vec3 u_bottom;
uniform vec2 u_target;
out vec4 colour;
void main() { colour = vec4(mix(u_bottom, u_top, gl_FragCoord.y / u_target.y), 1.0); }
)";

// Lines with a colour per vertex; the colour comes as bytes B, G, R, A.
const std::string kLineVertex = kVersion + R"(
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec4 a_colour;
uniform mat4 u_view_projection;
out vec4 v_colour;
void main() {
    v_colour = a_colour.zyxw;
    gl_Position = u_view_projection * vec4(a_position, 1.0);
}
)";
const std::string kLineFragment = kVersion + R"(
in vec4 v_colour;
out vec4 colour;
void main() { colour = v_colour; }
)";

// The bed's grid. With the moiré filter, a line fades as its neighbours come
// closer than a few pixels on the screen (towards the horizon, or from far
// away), before they are too close for the pixels to tell apart and alias
// into bands. The major lines, further apart, stay longer.
const std::string kGridVertex = kVersion + R"(
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec4 a_colour;
uniform mat4 u_view_projection;
out vec4 v_colour;
out vec3 v_position;
void main() {
    v_colour = a_colour.zyxw;
    v_position = a_position;
    gl_Position = u_view_projection * vec4(a_position, 1.0);
}
)";
const std::string kGridFragment = kVersion + R"(
in vec4 v_colour;
in vec3 v_position;
uniform mat4 u_view_projection;
uniform vec2 u_target;
uniform float u_step;
uniform int u_major_every;
uniform int u_moire;
out vec4 colour;
vec2 screen(vec3 p) {
    vec4 c = u_view_projection * vec4(p, 1.0);
    return c.xy / max(c.w, 1e-6) * 0.5 * u_target;
}
void main() {
    float alpha = v_colour.a;
    if (u_moire != 0) {
        // Along a line one coordinate stays: x on the lines across X, y on the others.
        float ax = abs(dFdx(v_position.x)) + abs(dFdy(v_position.x));
        float ay = abs(dFdx(v_position.y)) + abs(dFdy(v_position.y));
        bool x_stays = ax < ay;
        vec3 across = x_stays ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0);
        vec3 along = x_stays ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
        int index = int(abs(round((x_stays ? v_position.x : v_position.y) / u_step)));
        float spacing = u_step * (index % u_major_every == 0 ? float(u_major_every) : 1.0);
        // How far the next line of its kind is on the screen, square to this one.
        vec2 s = screen(v_position);
        vec2 d = screen(v_position + along * spacing) - s;
        d /= max(length(d), 1e-6);
        vec2 n = screen(v_position + across * spacing) - s;
        alpha *= smoothstep(2.0, 6.0, abs(d.x * n.y - d.y * n.x));
    }
    colour = vec4(v_colour.rgb, alpha);
}
)";

// The light every lit surface shares, in linear colour: a sky and a ground
// (hemisphere light), a key light that ray tracing can shadow, a weak fill
// light from the other side, Blinn-Phong highlights with Schlick's Fresnel
// term (energy-normalized with (n + 8) / 8 pi), a faint rim, an exponential
// tone curve and the sRGB gamma. The visibility texture holds, per pixel,
// how much of the key light (r) and of the ambient light (g) arrives. With
// u_gbuffer set, the shaders write the view-space normal instead.
const std::string kLighting = R"(
uniform vec3 u_eye;
uniform vec3 u_key;
uniform vec3 u_fill;
uniform vec3 u_sky;
uniform vec3 u_ground;
uniform sampler2D u_visibility;
uniform vec2 u_target;
uniform int u_gbuffer;
uniform mat4 u_view;
uniform float u_ao_strength;
uniform int u_debug;
out vec4 colour;

vec3 to_linear(vec3 c) { return pow(max(c, vec3(0.0)), vec3(2.2)); }

bool gbuffer(vec3 n) {
    if (u_gbuffer == 0)
        return false;
    vec3 nv = normalize(mat3(u_view) * n);
    if (nv.z < 0.0)
        nv = -nv;
    colour = vec4(nv * 0.5 + 0.5, 1.0);
    return true;
}

// seen: how much of the ambient (x) and of the key light (y) the surface's
// own neighbours leave (the bead's grooves); 1 elsewhere.
vec3 shade(vec3 base, vec3 n, vec3 p, float shininess, float specular, vec2 seen) {
    vec4 vis = texture(u_visibility, gl_FragCoord.xy / u_target);
    if (u_debug != 0)
        return vec3(vis.g, vis.r * vis.g, vis.g);
    float direct = vis.r * seen.y;
    // A strength past 1 darkens beyond the occlusion: never below no light
    // (a negative light made pow() a NaN, drawn black), and a little light
    // stays even in a closed crease, as bounced light would (no ray follows it).
    float ambient = max(mix(1.0, vis.g, u_ao_strength) * seen.x, 0.04);
    vec3 v = normalize(u_eye - p);
    if (dot(n, v) < 0.0)
        n = -n;
    float ndl = max(dot(n, u_key), 0.0);
    vec3 h = normalize(u_key + v);
    float fresnel = 0.04 + 0.96 * pow(1.0 - max(dot(h, v), 0.0), 5.0);
    float highlight = specular * (shininess + 8.0) / 25.1327 * pow(max(dot(n, h), 0.0), shininess) * fresnel;
    vec3 hemisphere = mix(to_linear(u_ground), to_linear(u_sky), 0.5 + 0.5 * n.z);
    vec3 c = base * hemisphere * 0.32 * ambient
           + (base * 0.95 + vec3(highlight)) * ndl * direct
           + base * max(dot(n, u_fill), 0.0) * 0.18 * ambient
           + to_linear(u_sky) * pow(1.0 - max(dot(n, v), 0.0), 4.0) * 0.06 * ambient;
    // The exponential tone curve on the brightest channel, the others scaled
    // with it: a bright saturated colour keeps its hue and saturation instead
    // of washing out towards white; greys come out as before.
    float m = max(max(c.r, c.g), c.b);
    if (m > 0.0)
        c *= (1.0 - exp(-m * 1.25)) / m;
    return pow(c, vec3(1.0 / 2.2));
}

vec3 shade(vec3 base, vec3 n, vec3 p, float shininess, float specular) {
    return shade(base, n, p, shininess, specular, vec2(1.0));
}
)";

// The plate: flat, facing up.
const std::string kPlateVertex = kVersion + R"(
layout(location = 0) in vec3 a_position;
uniform mat4 u_view_projection;
out vec3 v_position;
void main() {
    v_position = a_position;
    gl_Position = u_view_projection * vec4(a_position, 1.0);
}
)";
const std::string kPlateFragment = kVersion + kLighting + R"(
in vec3 v_position;
uniform vec3 u_colour;
void main() {
    if (gbuffer(vec3(0.0, 0.0, 1.0)))
        return;
    colour = vec4(shade(to_linear(u_colour), vec3(0.0, 0.0, 1.0), v_position, 16.0, 0.15), 1.0);
}
)";

// The model: flat-shaded triangles, lit from both sides.
const std::string kMeshVertex = kVersion + R"(
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
uniform mat4 u_view_projection;
uniform vec3 u_offset;
out vec3 v_position;
out vec3 v_normal;
void main() {
    v_position = a_position + u_offset;
    v_normal = a_normal;
    gl_Position = u_view_projection * vec4(v_position, 1.0);
}
)";
const std::string kMeshFragment = kVersion + kLighting + R"(
in vec3 v_position;
in vec3 v_normal;
uniform vec3 u_colour;
void main() {
    vec3 n = normalize(v_normal);
    if (gbuffer(n))
        return;
    colour = vec4(shade(to_linear(u_colour), n, v_position, 64.0, 0.5), 1.0);
}
)";

// One extrusion per instance, expanded into a prism around the move: the
// cross-section is a rhombus as wide as the line and as high as the layer,
// hanging under the nozzle's path. Where the bead goes on into the next move
// (a join, PreviewInstance), the end lies on the plane halfway between the two
// directions and both moves share its corners (a mitre); a free end grows by
// half a width, as the bead does past the nozzle, and gets its cap. 36
// vertices: four sides of two triangles and two ends of two triangles.
//
// The rhombus only gives the depth (two neighbours meet along a line, never on
// a shared face, so nothing flickers); the light follows the profile of a
// printed bead, found per pixel from where in the cross-section it lies: flat
// on top and bottom where the bead is pressed, round at the sides with the
// radius of half its height (the shape of a bead laid at its height, Comminal
// et al. 2018).
const std::string kBeadProfile = R"(
// The bead's normal at a point of the rhombus' outline, c (-1 to 1 across and
// up), as (across, up), for a bead of size (width, height).
vec2 bead_normal(vec2 c, vec2 size) {
    float r = 0.5 * size.y, flat_half = max(0.5 * size.x - r, 0.0);
    float x = abs(c.x) * 0.5 * size.x;
    float s = r > 0.0 ? clamp((x - flat_half) / r, 0.0, 1.0) : 1.0;
    return vec2(sign(c.x) * s, (c.y >= 0.0 ? 1.0 : -1.0) * sqrt(max(1.0 - s * s, 0.0)));
}

// How much of the sky (x) and of the key light (y) a point of the bead sees
// past the beads of the layers above and below it, where there are
// (neighbours: x above, y below): in the cross-section, the horizons to the
// touching neighbours of the same profile, the beads taken as endless along
// the move; the sky cosine-weighted between them, as horizon-based ambient
// occlusion does (Bavoil, Sainz and Dimitrov 2008), here exact for this
// shape. It darkens the grooves between the layers of a wall, as on a
// printed part.
vec2 bead_seen(vec2 c, vec2 size, vec3 side, vec3 up, vec3 key, vec2 neighbours) {
    float r = 0.5 * size.y, flat_half = max(0.5 * size.x - r, 0.0);
    if (r <= 0.0 || neighbours.x + neighbours.y < 0.5)
        return vec2(1.0);
    // Up the round side by the height in the section: what a wall shows from
    // the side. (Its flat top is under the bead above, never seen.)
    float phi = asin(clamp(c.y, -1.0, 1.0));       // the normal's angle
    vec2 p = vec2(flat_half + r * cos(phi), r * sin(phi)); // from the bead's middle, outwards and up
    float hi = phi + 1.5707963, lo = phi - 1.5707963;
    if (neighbours.x > 0.5) {                      // the bead above: its round side's centre
        vec2 d = vec2(flat_half, 2.0 * r) - p;
        hi = min(hi, atan(d.y, d.x) - asin(clamp(r / max(length(d), 1e-9), 0.0, 1.0)));
    }
    if (neighbours.y > 0.5) {                      // and the one below
        vec2 d = vec2(flat_half, -2.0 * r) - p;
        lo = max(lo, atan(d.y, d.x) + asin(clamp(r / max(length(d), 1e-9), 0.0, 1.0)));
    }
    float sky = hi > lo ? clamp(0.5 * (sin(hi - phi) - sin(lo - phi)), 0.0, 1.0) : 0.0;
    // The key light, in the cross-section: a soft light some 35 degrees
    // across, so a neighbour's shadow comes in over a penumbra.
    vec3 outwards = side * (c.x >= 0.0 ? 1.0 : -1.0);
    float a = atan(dot(key, up), dot(key, outwards));
    float light = smoothstep(lo - 0.3, lo + 0.3, a) * (1.0 - smoothstep(hi - 0.3, hi + 0.3, a));
    return vec2(sky, light);
}
)";
const std::string kPreviewVertex = kVersion + R"(
layout(location = 0) in vec3 a_p0;
layout(location = 1) in vec3 a_p1;
layout(location = 2) in vec2 a_size;
layout(location = 3) in float a_feed;
layout(location = 4) in uint a_kind_layer;
layout(location = 5) in vec4 a_join; // the mitre planes' normals at p0 (xy) and p1 (zw); zero at a free end
uniform mat4 u_view_projection;
uniform vec3 u_colours[16];
uniform uint u_kind_mask;
uniform int u_colour_mode;
uniform vec2 u_range;
uniform uvec2 u_layers; // the first and the last layer shown
uniform int u_grooves;  // the beads' grooves (not with ray tracing, whose rays see the neighbours)
out vec3 v_base;
out vec3 v_normal;
out vec3 v_position;
out vec2 v_section;    // where in the cross-section: -1 to 1 across (side), -1 to 1 up
flat out vec3 v_side;
flat out vec3 v_up;
flat out vec2 v_size;  // the line's width and height
flat out float v_cap;  // 1 on the end caps, 0 on the sides
flat out vec2 v_neighbours; // 1 where a bead of the layer above (x) and below (y) touches this one

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
    bool joined0 = dot(a_join.xy, a_join.xy) > 0.25, joined1 = dot(a_join.zw, a_join.zw) > 0.25;
    int id = gl_VertexID;
    bool hidden = (u_kind_mask & (1u << kind)) == 0u || len <= 0.0;
    // A joined end has no cap: the next move's side goes on from it.
    if (id >= 24 && ((id - 24) / 6 == 0 ? joined0 : joined1))
        hidden = true;
    if (hidden) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0); // outside the view: the triangle is dropped
        return;
    }
    vec3 dir = along / len;
    vec3 side = cross(dir, vec3(0.0, 0.0, 1.0));
    side = dot(side, side) > 1e-8 ? normalize(side) : vec3(1.0, 0.0, 0.0);
    vec3 up = cross(side, dir);
    float w = a_size.x, h = a_size.y;

    vec2 c;
    int end;
    vec3 normal;
    if (id < 24) {
        ivec2 q = kQuad[id % 6];
        c = kCorner[(id / 6 + q.y) % 4];
        end = q.x;
        normal = normalize(side * c.x * h + up * c.y * w);
    } else {
        end = (id - 24) / 6;
        c = kCorner[kCap[(id - 24) % 6]];
        normal = end == 0 ? -dir : dir;
        v_cap = 1.0;
    }
    // The corner around the end's point, then along the move to its end plane:
    // the mitre plane at a join, half a width past the point at a free end.
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
    if (u_colour_mode == 1)
        base = ramp((a_feed - u_range.x) / max(u_range.y - u_range.x, 1e-6));
    else if (u_colour_mode == 2)
        base = ramp((w - u_range.x) / max(u_range.y - u_range.x, 1e-6));
    else if (u_colour_mode == 3)
        base = ramp((h - u_range.x) / max(u_range.y - u_range.x, 1e-6));
    else
        base = u_colours[kind];
    v_base = pow(base, vec3(2.2));
    v_normal = normal;
    v_position = position;
    uint layer = a_kind_layer >> 8;
    bool grooves = u_grooves != 0 && v_cap < 0.5;
    v_neighbours = vec2(grooves && layer < u_layers.y && (kAbove & (1u << kind)) != 0u ? 1.0 : 0.0,
                        grooves && layer > u_layers.x && (kBelow & (1u << kind)) != 0u ? 1.0 : 0.0);
    gl_Position = u_view_projection * vec4(position, 1.0);
}
)";
// The moiré filter. The cross-section's coordinates run 2 units across a
// line, so where they change by more than about 1 from one pixel to the next
// a line covers less than two pixels and its facets can no longer be told
// apart (Nyquist): the lines of a wall or a surface alias into bands. What a
// pixel then shows is the average of the line's profile as seen from the eye:
// the bead's normals around the outline, each weighted by how much of it
// faces the eye, summed. The normal fades to that mean, and the highlight
// widens as the normals it stands for spread (Toksvig 2005: the exponent from
// the mean normal's length).
const std::string kPreviewFragment = kVersion + kLighting + kBeadProfile + R"(
in vec3 v_base;
in vec3 v_normal;
in vec3 v_position;
in vec2 v_section;
flat in vec3 v_side;
flat in vec3 v_up;
flat in vec2 v_size;
flat in float v_cap;
flat in vec2 v_neighbours;
uniform int u_moire;
void main() {
    vec3 n = normalize(v_normal);
    vec2 seen = vec2(1.0);
    if (v_cap < 0.5) {
        vec2 b = bead_normal(v_section, v_size);
        n = normalize(v_side * b.x + v_up * b.y);
        seen = bead_seen(v_section, v_size, v_side, v_up, u_key, v_neighbours);
    }
    float shininess = 40.0, specular = 0.35;
    if (u_moire != 0) {
        vec2 footprint = fwidth(v_section);
        float k = smoothstep(0.3, 0.8, max(footprint.x, footprint.y));
        // The grooves are a thin part of the bead: they average sooner, by
        // some five pixels a bead.
        float kg = smoothstep(0.1, 0.4, max(footprint.x, footprint.y));
        if (kg > 0.0) {
            vec3 v = normalize(u_eye - v_position);
            vec3 sum = vec3(0.0);
            vec2 seen_sum = vec2(0.0);
            float weight = 0.0;
            for (int i = 0; i < 16; ++i) {
                float a = (float(i) + 0.5) / 16.0 * 6.2831853;
                vec2 c = vec2(cos(a), sin(a));
                c /= abs(c.x) + abs(c.y); // on the rhombus
                vec2 b = bead_normal(c, v_size);
                vec3 m = normalize(v_side * b.x + v_up * b.y);
                float facing = max(dot(m, v), 0.0);
                sum += m * facing;
                seen_sum += bead_seen(c, v_size, v_side, v_up, u_key, v_neighbours) * facing;
                weight += facing;
            }
            if (weight > 0.0)
                seen = mix(seen, seen_sum / weight, kg); // the grooves, averaged as the normals
            if (weight > 0.0 && dot(sum, sum) > 1e-8) {
                float mean_length = mix(1.0, length(sum) / weight, k);
                n = normalize(mix(n, sum / length(sum), k));
                float ft = mean_length / (mean_length + shininess * (1.0 - mean_length));
                specular *= (1.0 + ft * shininess) / (1.0 + shininess);
                shininess *= ft;
            }
        }
    }
    if (gbuffer(n))
        return;
    colour = vec4(shade(v_base, n, v_position, shininess, specular, seen), 1.0);
}
)";

// View-space positions back from the depth buffer of a perspective camera:
// u_projection holds the matrix's elements (0,0), (1,1), (2,2) and (2,3).
const std::string kViewPosition = R"(
uniform sampler2D u_depth;
uniform vec4 u_projection;
uniform vec2 u_size;
vec3 view_position(ivec2 p) {
    float ndc = texelFetch(u_depth, p, 0).r * 2.0 - 1.0;
    float z = -u_projection.w / (ndc + u_projection.z);
    vec2 xy = (vec2(p) + 0.5) / u_size * 2.0 - 1.0;
    return vec3(xy.x * -z / u_projection.x, xy.y * -z / u_projection.y, z);
}
)";

// Screen-space ambient occlusion after McGuire, Osman, Bukowski and Hennessy
// 2011 ("The Alchemy screen-space ambient obscurance algorithm"): points of
// the depth buffer on a spiral within the radius around the pixel, each one
// counting by how much it rises above the surface's plane (the cosine to the
// normal), fading out towards the radius. Own normalization of their sum.
const std::string kAmbientFragment = kVersion + kViewPosition + R"(
uniform sampler2D u_normal;
uniform float u_radius;
uniform sampler2D u_shadow; // the key light's depth map
uniform mat4 u_light;       // world to the light's clip space
uniform mat4 u_world;       // view to world
uniform int u_shadowed;
uniform float u_texel;      // mm a texel of the map covers
uniform float u_bias;       // the map's depth of 0.6 mm: no shadow of a layer's step on the next
out vec4 colour;
const int kSamples = 16;
// The key light at a surface point: its shadow map compared over a disc of a
// few texels around the point (percentage-closer filtering, Reeves, Salesin
// and Cook 1987), so the shadow's edge comes soft; the point moved out along
// its normal by two texels, and compared 0.6 mm nearer the light: the steps
// between layers (a fraction of a millimetre, about a texel of the map) would
// shadow each other and alias into rings, and the beads' grooves are worked
// out exactly where they are drawn. The part's shadow on the bed, and under
// overhangs, stays.
const vec2 kDisc[12] = vec2[12](vec2(0.0, 0.0), vec2(0.53, 0.0), vec2(-0.27, 0.46), vec2(-0.27, -0.46),
                                vec2(1.0, 0.0), vec2(0.5, 0.87), vec2(-0.5, 0.87), vec2(-1.0, 0.0),
                                vec2(-0.5, -0.87), vec2(0.5, -0.87), vec2(0.0, 0.76), vec2(0.0, -0.76));
float key_light(vec3 c, vec3 n) {
    vec3 p = (u_world * vec4(c, 1.0)).xyz;
    if (u_shadowed == 0 || p.z > 0.05)
        return 1.0; // only the bed takes it, for now: the part's steps shadowed each other into rings
    p += normalize(mat3(u_world) * n) * u_texel * 2.0;
    vec3 s = (u_light * vec4(p, 1.0)).xyz * 0.5 + 0.5;
    if (s.x <= 0.0 || s.y <= 0.0 || s.x >= 1.0 || s.y >= 1.0 || s.z >= 1.0)
        return 1.0;
    vec2 texel = 1.0 / vec2(textureSize(u_shadow, 0));
    float lit = 0.0;
    for (int i = 0; i < 12; ++i)
        lit += s.z - u_bias <= texture(u_shadow, s.xy + kDisc[i] * 2.5 * texel).r ? 1.0 : 0.0;
    return lit / 12.0;
}
void main() {
    ivec2 p = ivec2(gl_FragCoord.xy);
    if (texelFetch(u_depth, p, 0).r >= 1.0) {
        colour = vec4(1.0);
        return;
    }
    vec3 c = view_position(p);
    vec3 n = normalize(texelFetch(u_normal, p, 0).xyz * 2.0 - 1.0);
    float key = key_light(c, n);
    float pixels = min(u_radius * u_projection.y * 0.5 * u_size.y / -c.z, 80.0);
    if (pixels < 1.5) {
        colour = vec4(1.0, key, 0.0, 1.0);
        return;
    }
    float turn = fract(sin(dot(vec2(p), vec2(12.9898, 78.233))) * 43758.5453) * 6.2831853;
    float sum = 0.0;
    for (int i = 0; i < kSamples; ++i) {
        float a = (float(i) + 0.5) / float(kSamples);
        float angle = a * 6.2831853 * 7.0 + turn;
        ivec2 q = clamp(p + ivec2(round(vec2(cos(angle), sin(angle)) * a * pixels)), ivec2(0), ivec2(u_size) - 1);
        if (texelFetch(u_depth, q, 0).r >= 1.0)
            continue;
        vec3 v = view_position(q) - c;
        float vv = dot(v, v);
        float r2 = u_radius * u_radius;
        if (vv > r2 || vv < 1e-8)
            continue;
        float rise = dot(v, n) * inversesqrt(vv) - 0.05;
        // Nothing nearer than a couple of layers: the steps between printed
        // layers would alias into rings, and the beads' own grooves are
        // worked out exactly where they are drawn.
        float near = smoothstep(0.15, 0.45, sqrt(vv));
        sum += max(rise, 0.0) * (1.0 - vv / r2) * near;
    }
    colour = vec4(clamp(1.0 - 2.0 * sum / float(kSamples), 0.0, 1.0), key, 0.0, 1.0);
}
)";

// A bilateral blur of the occlusion (r) and the key light (g) along one axis:
// neighbours at another depth (across an edge) do not count. The last pass
// writes the visibility: r the key light, g the ambient.
const std::string kBlurFragment = kVersion + kViewPosition + R"(
uniform sampler2D u_source;
uniform ivec2 u_step;
uniform int u_final;
out vec4 colour;
void main() {
    ivec2 p = ivec2(gl_FragCoord.xy);
    float z = view_position(p).z;
    vec2 sum = vec2(0.0);
    float weight = 0.0;
    for (int i = -4; i <= 4; ++i) {
        ivec2 q = clamp(p + u_step * i, ivec2(0), ivec2(u_size) - 1);
        float zq = view_position(q).z;
        float w = exp(-float(i * i) / 8.0) * max(0.0, 1.0 - abs(zq - z) / (0.02 * abs(z) + 0.01));
        sum += texelFetch(u_source, q, 0).rg * w;
        weight += w;
    }
    vec2 v = weight > 0.0 ? sum / weight : vec2(1.0);
    colour = u_final != 0 ? vec4(v.g, v.r, 0.0, 1.0) : vec4(v, 0.0, 1.0);
}
)";

// Ray tracing, the GPU copy of RayScene (src/ui/raytrace.cpp, the reference):
// from each pixel's surface, a shadow ray towards a point of the key light
// (a disc of u_sun radians, for soft shadows) and two ambient rays (cosine
// distributed, Malley's method) as long as the occlusion radius, against the
// bounding volume hierarchy in u_nodes and u_primitives. Each frame's answer
// is averaged with the frames before. Random numbers: the PCG hash of
// Jarzynski and Olano 2020 ("Hash functions for GPU rendering").
const std::string kRayFragment = kVersion + kViewPosition + R"(
uniform sampler2D u_normal;
uniform sampler2D u_previous;
uniform sampler2D u_nodes;
uniform sampler2D u_primitives;
uniform mat4 u_inverse_view;
uniform vec3 u_key;
uniform float u_sun;
uniform float u_radius;
uniform uint u_frame;
uniform float u_blend;
uniform vec2 u_layers;
uniform uint u_kind_mask;
uniform int u_ambient_rays;
out vec4 colour;

vec4 node(int i) { return texelFetch(u_nodes, ivec2(i & 4095, i >> 12), 0); }
vec4 primitive(int i) { return texelFetch(u_primitives, ivec2(i & 4095, i >> 12), 0); }

float hit_triangle(vec3 o, vec3 d, vec3 a, vec3 e1, vec3 e2) {
    vec3 p = cross(d, e2);
    float det = dot(e1, p);
    if (abs(det) < 1e-12)
        return -1.0;
    float inv = 1.0 / det;
    vec3 s = o - a;
    float u = dot(s, p) * inv;
    if (u < 0.0 || u > 1.0)
        return -1.0;
    vec3 q = cross(s, e1);
    float v = dot(d, q) * inv;
    if (v < 0.0 || u + v > 1.0)
        return -1.0;
    float t = dot(e2, q) * inv;
    return t > 0.0 ? t : -1.0;
}

float hit_box(vec3 o, vec3 d, vec3 centre, vec3 hx, vec3 hy, vec3 hz) {
    vec3 to = centre - o;
    float near = -1e30, far = 1e30;
    vec3 axes[3] = vec3[3](hx, hy, hz);
    for (int k = 0; k < 3; ++k) {
        float l = length(axes[k]);
        if (l <= 0.0)
            return -1.0;
        vec3 a = axes[k] / l;
        float e = dot(a, to), f = dot(a, d);
        if (abs(f) > 1e-12) {
            float t1 = (e + l) / f, t2 = (e - l) / f;
            near = max(near, min(t1, t2));
            far = min(far, max(t1, t2));
            if (near > far || far < 0.0)
                return -1.0;
        } else if (-e - l > 0.0 || -e + l < 0.0) {
            return -1.0;
        }
    }
    return near > 0.0 ? near : -1.0;
}

// The bead of the move p0 to p1 (src/ui/raytrace.cpp, intersect_bead): six
// planes clip the ray, all from p0.
float hit_bead(vec3 o, vec3 d, vec3 p0, vec3 p1, vec3 size, float end_angle) {
    vec3 along = p1 - p0;
    float len = length(along), w = size.x, h = size.y;
    if (len <= 0.0 || w <= 0.0 || h <= 0.0)
        return -1.0;
    vec3 dir = along / len;
    vec3 side = cross(dir, vec3(0.0, 0.0, 1.0));
    side = dot(side, side) > 1e-8 ? normalize(side) : vec3(1.0, 0.0, 0.0);
    vec3 up = cross(side, dir);
    vec3 q = o - p0;
    float near = -1e30, far = 1e30;
    vec3 n[6];
    float k[6];
    vec3 c = up * (-0.5 * h);
    for (int i = 0; i < 4; ++i) {
        float s = (i & 1) == 0 ? -1.0 : 1.0, u = (i & 2) == 0 ? -1.0 : 1.0;
        n[i] = side * (s * 2.0 / w) + up * (u * 2.0 / h);
        k[i] = dot(c, n[i]) + 1.0;
    }
    if (size.z < 50.0) {
        n[4] = -vec3(cos(size.z), sin(size.z), 0.0);
        k[4] = 0.0;
    } else {
        n[4] = -dir;
        k[4] = 0.5 * w;
    }
    if (end_angle < 50.0) {
        n[5] = vec3(cos(end_angle), sin(end_angle), 0.0);
        k[5] = dot(along, n[5]);
    } else {
        n[5] = dir;
        k[5] = dot(along, dir) + 0.5 * w;
    }
    for (int i = 0; i < 6; ++i) {
        float denom = dot(d, n[i]), num = k[i] - dot(q, n[i]);
        if (abs(denom) < 1e-12) {
            if (num < 0.0)
                return -1.0;
            continue;
        }
        float t = num / denom;
        if (denom > 0.0)
            far = min(far, t);
        else
            near = max(near, t);
        if (near > far)
            return -1.0;
    }
    if (far < 0.0)
        return -1.0;
    return near > 0.0 ? near : -1.0;
}

bool occluded(vec3 o, vec3 d, float tmax) {
    vec3 safe = mix(d, sign(d) * 1e-12 + vec3(equal(d, vec3(0.0))) * 1e-12, lessThan(abs(d), vec3(1e-12)));
    vec3 inv = 1.0 / safe;
    int stack[64];
    int sp = 0;
    int at = 0;
    for (int guard = 0; guard < 16384; ++guard) {
        vec4 lo = node(at * 3), hi = node(at * 3 + 1), layers = node(at * 3 + 2);
        vec3 t0 = (lo.xyz - o) * inv, t1 = (hi.xyz - o) * inv;
        vec3 tn = min(t0, t1), tf = max(t0, t1);
        float near = max(max(tn.x, tn.y), tn.z), far = min(min(tf.x, tf.y), tf.z);
        bool in_layers = layers.y >= u_layers.x && layers.x <= u_layers.y;
        if (in_layers && near <= far && far >= 0.0 && near <= tmax) {
            int count = int(hi.w);
            if (count > 0) {
                int first = int(lo.w);
                for (int k = 0; k < 4; ++k) {
                    if (k >= count)
                        break;
                    int base = (first + k) * 4;
                    vec4 p0 = primitive(base), p1 = primitive(base + 1), p2 = primitive(base + 2);
                    if (p0.w < u_layers.x || p0.w > u_layers.y)
                        continue;
                    uint kind = uint(p2.w);
                    if (kind != 31u && ((u_kind_mask >> kind) & 1u) == 0u)
                        continue;
                    float t = p1.w > 1.5   ? hit_bead(o, d, p0.xyz, p1.xyz, p2.xyz, primitive(base + 3).x)
                              : p1.w > 0.5 ? hit_box(o, d, p0.xyz, p1.xyz, p2.xyz, primitive(base + 3).xyz)
                                           : hit_triangle(o, d, p0.xyz, p1.xyz, p2.xyz);
                    if (t >= 0.0 && t < tmax)
                        return true;
                }
            } else if (sp < 64) {
                stack[sp++] = int(lo.w) + 1;
                at = int(lo.w);
                continue;
            }
        }
        if (sp == 0)
            return false;
        at = stack[--sp];
    }
    return false;
}

uint pcg(uint v) {
    uint state = v * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}
float random(inout uint seed) {
    seed = pcg(seed);
    return float(seed) * (1.0 / 4294967296.0);
}
// A unit vector around n: (r cos phi, r sin phi, z) in a frame whose third axis is n.
vec3 around(vec3 n, float cos_theta, float phi) {
    vec3 t = normalize(cross(abs(n.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0), n));
    vec3 b = cross(n, t);
    float sin_theta = sqrt(max(0.0, 1.0 - cos_theta * cos_theta));
    return t * (sin_theta * cos(phi)) + b * (sin_theta * sin(phi)) + n * cos_theta;
}

void main() {
    ivec2 p = ivec2(gl_FragCoord.xy);
    vec4 previous = texelFetch(u_previous, p, 0);
    if (texelFetch(u_depth, p, 0).r >= 1.0) {
        colour = vec4(1.0, 1.0, 0.0, 1.0);
        return;
    }
    vec3 pv = view_position(p);
    vec3 nv = normalize(texelFetch(u_normal, p, 0).xyz * 2.0 - 1.0);
    vec3 position = (u_inverse_view * vec4(pv, 1.0)).xyz;
    vec3 n = normalize(mat3(u_inverse_view) * nv);
    vec3 o = position + n * (0.0015 * -pv.z + 0.002);
    uint seed = pcg(uint(p.x) + pcg(uint(p.y) + pcg(u_frame)));

    float light = 1.0;
    if (dot(n, u_key) > 0.0) {
        float cos_theta = mix(1.0, cos(u_sun), random(seed));
        vec3 l = around(u_key, cos_theta, 6.2831853 * random(seed));
        light = occluded(o, l, 1e30) ? 0.0 : 1.0;
    }
    float open = 0.0;
    for (int i = 0; i < u_ambient_rays; ++i) {
        float r = random(seed);
        vec3 d = around(n, sqrt(1.0 - r), 6.2831853 * random(seed));
        open += occluded(o, d * u_radius, 1.0) ? 0.0 : 1.0;
    }
    open /= float(max(u_ambient_rays, 1));
    colour = vec4(mix(previous.rg, vec2(light, open), u_blend), 0.0, 1.0);
}
)";

// The lit scene into the window, smoothed with an own version of FXAA (Lottes
// 2009, "FXAA", NVIDIA white paper): where the luma contrast around a pixel is
// high, find the edge's direction and its ends along it, and sample across
// the edge in proportion to where the pixel sits on it, plus a blend for
// sub-pixel detail.
const std::string kCompositeFragment = kVersion + R"(
uniform sampler2D u_colour;
uniform vec2 u_origin;
uniform vec2 u_size;
uniform int u_fxaa;
out vec4 colour;
float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }
float luma_at(vec2 uv) { return luma(texture(u_colour, uv).rgb); }
void main() {
    vec2 texel = 1.0 / u_size;
    vec2 uv = (gl_FragCoord.xy - u_origin) * texel;
    vec3 c = texture(u_colour, uv).rgb;
    if (u_fxaa == 0) {
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

    // Sub-pixel blend: how far the pixel is from its neighbours' average.
    float average = (2.0 * (n + s + e + w) + ne + nw + se + sw) / 12.0;
    float sub = clamp(abs(average - m) / range, 0.0, 1.0);
    sub = (-2.0 * sub + 3.0) * sub * sub;
    float sub_blend = sub * sub * 0.75;

    // The edge runs along x when the change is mostly across y.
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

    // Walk both ways along the edge until its luma leaves the edge's.
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
    // Only blend when the nearer end changes the way the pixel does.
    bool right_way = ((a_closer ? la : lb) < 0.0) != (m < edge);
    float blend = max(right_way ? offset : 0.0, sub_blend);
    vec2 final_uv = uv + (along_x ? vec2(0.0, blend * step) : vec2(blend * step, 0.0));
    colour = vec4(texture(u_colour, final_uv).rgb, 1.0);
}
)";

unsigned compile(GLenum type, const std::string& source, std::string& error) {
    const GLuint shader = glCreateShader(type);
    const char* text = source.c_str();
    glShaderSource(shader, 1, &text, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096] = {};
        glGetShaderInfoLog(shader, sizeof log, nullptr, log);
        error = std::string("shader: ") + log;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

unsigned program(const std::string& vertex, const std::string& fragment, std::string& error) {
    const unsigned v = compile(GL_VERTEX_SHADER, vertex, error);
    if (!v)
        return 0;
    const unsigned f = compile(GL_FRAGMENT_SHADER, fragment, error);
    if (!f) {
        glDeleteShader(v);
        return 0;
    }
    const GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    glDetachShader(p, v);
    glDetachShader(p, f);
    glDeleteShader(v);
    glDeleteShader(f);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096] = {};
        glGetProgramInfoLog(p, sizeof log, nullptr, log);
        error = std::string("program: ") + log;
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

void upload(unsigned& vao, unsigned& vbo, const void* data, std::size_t bytes) {
    if (!vao)
        glGenVertexArrays(1, &vao);
    if (!vbo)
        glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(bytes), data, GL_STATIC_DRAW);
}

const void* offset(std::size_t bytes) { return reinterpret_cast<const void*>(bytes); }

GLint at(unsigned p, const char* name) { return glGetUniformLocation(p, name); }
void uniform(unsigned p, const char* name, const Mat4& m) { glUniformMatrix4fv(at(p, name), 1, GL_FALSE, m.data()); }
void uniform(unsigned p, const char* name, Vec3 v) { glUniform3f(at(p, name), v.x, v.y, v.z); }
void uniform(unsigned p, const char* name, float v) { glUniform1f(at(p, name), v); }
void uniform(unsigned p, const char* name, int v) { glUniform1i(at(p, name), v); }
void uniform(unsigned p, const char* name, float x, float y) { glUniform2f(at(p, name), x, y); }

unsigned make_texture(GLint internal, int width, int height, GLenum format, GLenum type, GLint filter,
                      const void* data = nullptr) {
    unsigned tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, internal, width, height, 0, format, type, data);
    return tex;
}

unsigned make_framebuffer(unsigned colour, unsigned depth_texture = 0, unsigned depth_buffer = 0) {
    unsigned fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    if (colour)
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colour, 0);
    if (depth_texture)
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_texture, 0);
    if (depth_buffer)
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_buffer);
    return fbo;
}

unsigned make_renderbuffer(int samples, GLenum format, int width, int height) {
    unsigned rb = 0;
    glGenRenderbuffers(1, &rb);
    glBindRenderbuffer(GL_RENDERBUFFER, rb);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, format, width, height);
    return rb;
}

void bind_texture(int unit, unsigned tex) {
    glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + unit));
    glBindTexture(GL_TEXTURE_2D, tex);
}

void screen_pass(unsigned empty_vao) {
    glBindVertexArray(empty_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void put(std::vector<float>& v, const Mat4& m) { v.insert(v.end(), m.begin(), m.end()); }
void put(std::vector<float>& v, Vec3 p) { v.insert(v.end(), {p.x, p.y, p.z}); }


} // namespace

std::vector<double> frame_key(const ViewFrame& f) {
    std::vector<double> k;
    const auto add = [&](std::initializer_list<double> values) { k.insert(k.end(), values); };
    const auto vec = [&](Vec3 p) { add({p.x, p.y, p.z}); };
    add({double(f.x), double(f.y), double(f.width), double(f.height)});
    k.insert(k.end(), f.view.begin(), f.view.end());
    k.insert(k.end(), f.projection.begin(), f.projection.end());
    for (const Vec3 p : {f.eye, f.key, f.fill, f.sky, f.ground, f.background_top, f.background_bottom, f.plate, f.model,
                         f.model_selected, f.selected_offset})
        vec(p);
    for (const Vec3 p : f.kind_colours)
        vec(p);
    add({double(f.selected_first), double(f.selected_count), double(f.show_model), double(f.coarse),
         double(f.coarse_selected_first), double(f.coarse_selected_count), double(f.show_paint),
         double(f.show_preview), double(f.show_travels), double(f.first_layer), double(f.last_layer),
         double(f.top_shown), double(f.top_hidden), double(f.kind_mask), double(f.color_mode), double(f.mode),
         double(f.samples), double(f.fxaa), double(f.moire_filter), f.ao_radius, f.ao_strength, f.sun_radius,
         double(f.max_ray_samples), double(f.debug_visibility)});
    return k;
}

bool Renderer::init(std::string& error) {
    struct Source {
        unsigned* target;
        const std::string* vertex;
        const std::string* fragment;
    };
    const Source sources[] = {
        {&p_.background, &kScreenVertex, &kBackgroundFragment},
        {&p_.line, &kLineVertex, &kLineFragment},
        {&p_.grid, &kGridVertex, &kGridFragment},
        {&p_.plate, &kPlateVertex, &kPlateFragment},
        {&p_.mesh, &kMeshVertex, &kMeshFragment},
        {&p_.preview, &kPreviewVertex, &kPreviewFragment},
        {&p_.ao, &kScreenVertex, &kAmbientFragment},
        {&p_.blur, &kScreenVertex, &kBlurFragment},
        {&p_.ray, &kScreenVertex, &kRayFragment},
        {&p_.composite, &kScreenVertex, &kCompositeFragment},
    };
    for (const Source& s : sources) {
        *s.target = program(*s.vertex, *s.fragment, error);
        if (!*s.target)
            return false;
    }
    glGenVertexArrays(1, &empty_vao_);
    const std::uint8_t white[4] = {255, 255, 255, 255};
    white_ = make_texture(GL_RGBA8, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, GL_NEAREST, white);
    glGetIntegerv(GL_MAX_SAMPLES, &max_samples_);
    max_samples_ = std::max(max_samples_, 1);
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_);
    shadow_size_ = std::min(kShadowMapSize, max_texture_);
    shadow_depth_ =
        make_texture(GL_DEPTH_COMPONENT24, shadow_size_, shadow_size_, GL_DEPTH_COMPONENT, GL_FLOAT, GL_NEAREST);
    shadow_fbo_ = make_framebuffer(0, shadow_depth_);
    glDrawBuffer(GL_NONE); // depth only
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    ready_ = true;
    return true;
}

void Renderer::release_targets() {
    const auto textures = [](std::initializer_list<unsigned*> list) {
        for (unsigned* t : list)
            if (*t) {
                glDeleteTextures(1, t);
                *t = 0;
            }
    };
    const auto framebuffers = [](std::initializer_list<unsigned*> list) {
        for (unsigned* f : list)
            if (*f) {
                glDeleteFramebuffers(1, f);
                *f = 0;
            }
    };
    textures({&g_normal_, &g_depth_, &ao_tex_[0], &ao_tex_[1], &ao_tex_[2], &ray_tex_[0], &ray_tex_[1], &colour_});
    framebuffers({&g_fbo_, &ao_fbo_[0], &ao_fbo_[1], &ao_fbo_[2], &ray_fbo_[0], &ray_fbo_[1], &msaa_fbo_, &colour_fbo_});
    for (unsigned* rb : {&msaa_colour_, &msaa_depth_, &colour_depth_})
        if (*rb) {
            glDeleteRenderbuffers(1, rb);
            *rb = 0;
        }
    width_ = height_ = samples_ = 0;
}

void Renderer::resize(int width, int height, int samples) {
    if (width == width_ && height == height_ && samples == samples_)
        return;
    release_targets();
    width_ = width, height_ = height, samples_ = samples;
    g_normal_ = make_texture(GL_RGBA8, width, height, GL_RGBA, GL_UNSIGNED_BYTE, GL_NEAREST);
    g_depth_ = make_texture(GL_DEPTH_COMPONENT24, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, GL_NEAREST);
    g_fbo_ = make_framebuffer(g_normal_, g_depth_);
    for (int i = 0; i < 3; ++i) {
        ao_tex_[i] = i < 2 ? make_texture(GL_RG8, width, height, GL_RG, GL_UNSIGNED_BYTE, GL_NEAREST)
                           : make_texture(GL_RGBA8, width, height, GL_RGBA, GL_UNSIGNED_BYTE, GL_NEAREST);
        ao_fbo_[i] = make_framebuffer(ao_tex_[i]);
    }
    for (int i = 0; i < 2; ++i) {
        ray_tex_[i] = make_texture(GL_RGBA16F, width, height, GL_RGBA, GL_HALF_FLOAT, GL_NEAREST);
        ray_fbo_[i] = make_framebuffer(ray_tex_[i]);
    }
    colour_ = make_texture(GL_RGBA8, width, height, GL_RGBA, GL_UNSIGNED_BYTE, GL_LINEAR);
    if (samples > 1) {
        msaa_colour_ = make_renderbuffer(samples, GL_RGBA8, width, height);
        msaa_depth_ = make_renderbuffer(samples, GL_DEPTH_COMPONENT24, width, height);
        glGenFramebuffers(1, &msaa_fbo_);
        glBindFramebuffer(GL_FRAMEBUFFER, msaa_fbo_);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msaa_colour_);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, msaa_depth_);
        colour_fbo_ = make_framebuffer(colour_);
    } else {
        colour_depth_ = make_renderbuffer(0, GL_DEPTH_COMPONENT24, width, height);
        colour_fbo_ = make_framebuffer(colour_, 0, colour_depth_);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    scene_changed();
}

void Renderer::scene_changed() {
    ray_signature_.clear();
    drawn_.clear();
}

void Renderer::shutdown() {
    if (!ready_)
        return;
    release_targets();
    clear_ray_scene();
    for (unsigned* p : {&p_.background, &p_.line, &p_.grid, &p_.plate, &p_.mesh, &p_.preview, &p_.ao, &p_.blur,
                        &p_.ray, &p_.composite})
        if (*p) {
            glDeleteProgram(*p);
            *p = 0;
        }
    for (Buffer* b : {&plate_, &grid_, &model_, &model_coarse_, &paint_, &extrusions_, &travels_}) {
        if (b->vbo)
            glDeleteBuffers(1, &b->vbo);
        if (b->vao)
            glDeleteVertexArrays(1, &b->vao);
        *b = {};
    }
    if (empty_vao_)
        glDeleteVertexArrays(1, &empty_vao_);
    if (white_)
        glDeleteTextures(1, &white_);
    if (shadow_depth_)
        glDeleteTextures(1, &shadow_depth_);
    if (shadow_fbo_)
        glDeleteFramebuffers(1, &shadow_fbo_);
    empty_vao_ = white_ = shadow_depth_ = shadow_fbo_ = 0;
    ready_ = false;
}

void Renderer::set_bed(const BedPolygon& bed, const BedLook& look) {
    if (!ready_)
        return;
    const std::vector<float> plate = bed_triangles(bed);
    upload(plate_.vao, plate_.vbo, plate.data(), plate.size() * sizeof(float));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), offset(0));
    plate_.count = static_cast<int>(plate.size() / 3);

    const std::vector<LineVertex> lines = bed_lines(bed, look);
    upload(grid_.vao, grid_.vbo, lines.data(), lines.size() * sizeof(LineVertex));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex), offset(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(LineVertex), offset(offsetof(LineVertex, argb)));
    grid_.count = static_cast<int>(lines.size());
    grid_outline_ = lines.empty() ? 0 : static_cast<int>(2 * bed.size()); // bed_lines puts it last
    grid_step_ = float(look.step);
    grid_major_every_ = std::max(look.major_every, 1);
    scene_changed();
}

void Renderer::set_model_coarse(const std::vector<float>& vertices) {
    if (!ready_)
        return;
    upload(model_coarse_.vao, model_coarse_.vbo, vertices.data(), vertices.size() * sizeof(float));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), offset(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), offset(3 * sizeof(float)));
    model_coarse_.count = static_cast<int>(vertices.size() / 6);
    scene_changed();
}

void Renderer::set_model(const std::vector<float>& vertices) {
    if (!ready_)
        return;
    upload(model_.vao, model_.vbo, vertices.data(), vertices.size() * sizeof(float));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), offset(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), offset(3 * sizeof(float)));
    model_.count = static_cast<int>(vertices.size() / 6);
    model_box_ = model_.count > 0;
    model_lo_ = Vec3{1e30f, 1e30f, 1e30f}, model_hi_ = Vec3{-1e30f, -1e30f, -1e30f};
    for (std::size_t i = 0; i + 5 < vertices.size(); i += 6) {
        model_lo_ = Vec3{std::min(model_lo_.x, vertices[i]), std::min(model_lo_.y, vertices[i + 1]),
                         std::min(model_lo_.z, vertices[i + 2])};
        model_hi_ = Vec3{std::max(model_hi_.x, vertices[i]), std::max(model_hi_.y, vertices[i + 1]),
                         std::max(model_hi_.z, vertices[i + 2])};
    }
    scene_changed();
}

void Renderer::set_paint(const std::vector<float>& vertices, const std::vector<PaintGroup>& groups) {
    if (!ready_)
        return;
    upload(paint_.vao, paint_.vbo, vertices.data(), vertices.size() * sizeof(float));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), offset(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), offset(3 * sizeof(float)));
    paint_.count = static_cast<int>(vertices.size() / 6);
    paint_groups_ = groups;
    scene_changed();
}

void Renderer::set_preview(const PreviewBuffers& p) {
    if (!ready_)
        return;
    // Drawing order (docs/LARGE_MODELS.md): what can show, then what the
    // part hides when all of it is drawn.
    order_ = preview_draw_order(p);
    std::vector<PreviewInstance>& ordered = order_.instances;
    preview_box_ = !ordered.empty();
    preview_lo_ = Vec3{1e30f, 1e30f, 1e30f}, preview_hi_ = Vec3{-1e30f, -1e30f, -1e30f};
    for (const PreviewInstance& e : ordered) {
        const float r = std::max(e.width, e.height);
        preview_lo_ = Vec3{std::min({preview_lo_.x, e.x0 - r, e.x1 - r}), std::min({preview_lo_.y, e.y0 - r, e.y1 - r}),
                           std::min({preview_lo_.z, e.z0 - r, e.z1 - r})};
        preview_hi_ = Vec3{std::max({preview_hi_.x, e.x0 + r, e.x1 + r}), std::max({preview_hi_.y, e.y0 + r, e.y1 + r}),
                           std::max({preview_hi_.z, e.z0 + r, e.z1 + r})};
    }
    upload(extrusions_.vao, extrusions_.vbo, ordered.data(), ordered.size() * sizeof(PreviewInstance));
    extrusions_.count = static_cast<int>(ordered.size());
    ordered = {}; // on the GPU now
    upload(travels_.vao, travels_.vbo, p.travels.data(), p.travels.size() * sizeof(LineVertex));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex), offset(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(LineVertex), offset(offsetof(LineVertex, argb)));
    travels_.count = static_cast<int>(p.travels.size());
    layer_extrusions_ = p.layer_extrusions;
    layer_travels_ = p.layer_travels;
    min_feed_ = p.min_feed, max_feed_ = p.max_feed;
    min_width_ = p.min_width, max_width_ = p.max_width;
    min_height_ = p.min_height, max_height_ = p.max_height;
    scene_changed();
}

void Renderer::clear_preview() {
    extrusions_.count = travels_.count = 0;
    layer_extrusions_.clear();
    order_ = {};
    layer_travels_.clear();
    scene_changed();
}

bool Renderer::set_ray_scene(const std::vector<float>& nodes, const std::vector<float>& primitives) {
    if (!ready_)
        return false;
    clear_ray_scene();
    const std::size_t row = std::size_t(RayScene::kTextureWidth) * 4;
    const int node_rows = int(nodes.size() / row), primitive_rows = int(primitives.size() / row);
    if (node_rows == 0 || primitive_rows == 0 || RayScene::kTextureWidth > max_texture_ || node_rows > max_texture_ ||
        primitive_rows > max_texture_)
        return false;
    ray_nodes_ = make_texture(GL_RGBA32F, RayScene::kTextureWidth, node_rows, GL_RGBA, GL_FLOAT, GL_NEAREST, nodes.data());
    ray_prims_ = make_texture(GL_RGBA32F, RayScene::kTextureWidth, primitive_rows, GL_RGBA, GL_FLOAT, GL_NEAREST,
                              primitives.data());
    scene_changed();
    return true;
}

void Renderer::clear_ray_scene() {
    for (unsigned* t : {&ray_nodes_, &ray_prims_})
        if (*t) {
            glDeleteTextures(1, t);
            *t = 0;
        }
    ray_samples_ = 0;
    scene_changed();
}

void Renderer::lit_uniforms(unsigned p, const ViewFrame& f, const Mat4& vp, bool gbuffer, unsigned visibility) {
    glUseProgram(p);
    uniform(p, "u_view_projection", vp);
    uniform(p, "u_view", f.view);
    uniform(p, "u_eye", f.eye);
    uniform(p, "u_key", f.key);
    uniform(p, "u_fill", f.fill);
    uniform(p, "u_sky", f.sky);
    uniform(p, "u_ground", f.ground);
    uniform(p, "u_target", float(width_), float(height_));
    uniform(p, "u_gbuffer", gbuffer ? 1 : 0);
    uniform(p, "u_ao_strength", f.mode == RenderMode::Fast ? 0.0f : f.ao_strength);
    uniform(p, "u_visibility", kVisibilityUnit);
    uniform(p, "u_debug", f.debug_visibility ? 1 : 0);
    bind_texture(kVisibilityUnit, visibility);
}

void Renderer::draw_scene(const ViewFrame& f, const Mat4& vp, bool gbuffer, unsigned visibility, bool casters) {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    if (plate_.count > 0 && !casters) {
        // Pushed back a little so the grid, and the first layer, draw over it.
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.0f, 1.0f);
        lit_uniforms(p_.plate, f, vp, gbuffer, visibility);
        uniform(p_.plate, "u_colour", f.plate);
        glBindVertexArray(plate_.vao);
        glDrawArrays(GL_TRIANGLES, 0, plate_.count);
        glDisable(GL_POLYGON_OFFSET_FILL);
    }
    if (!gbuffer && grid_.count > 0) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        glBindVertexArray(grid_.vao);
        const int lines = grid_.count - grid_outline_;
        if (lines > 0) {
            glUseProgram(p_.grid);
            uniform(p_.grid, "u_view_projection", vp);
            uniform(p_.grid, "u_target", float(width_), float(height_));
            uniform(p_.grid, "u_step", grid_step_);
            uniform(p_.grid, "u_major_every", grid_major_every_);
            uniform(p_.grid, "u_moire", f.moire_filter ? 1 : 0);
            glDrawArrays(GL_LINES, 0, lines);
        }
        glUseProgram(p_.line);
        uniform(p_.line, "u_view_projection", vp);
        glDrawArrays(GL_LINES, lines, grid_outline_);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
    if (f.show_model && model_.count > 0) {
        lit_uniforms(p_.mesh, f, vp, gbuffer, visibility);
        // While the view moves, the coarse copy if there is one (V6).
        const bool coarse = f.coarse && model_coarse_.count > 0;
        const Buffer& model = coarse ? model_coarse_ : model_;
        glBindVertexArray(model.vao);
        // The chosen model in its own colour, the rest before and after it.
        const GLint count = model.count;
        const GLint first =
            GLint(std::min<std::size_t>(coarse ? f.coarse_selected_first : f.selected_first, std::size_t(count)));
        const GLint chosen = GLint(
            std::min<std::size_t>(coarse ? f.coarse_selected_count : f.selected_count, std::size_t(count - first)));
        uniform(p_.mesh, "u_colour", f.model);
        uniform(p_.mesh, "u_offset", Vec3{0, 0, 0});
        if (first > 0)
            glDrawArrays(GL_TRIANGLES, 0, first);
        if (first + chosen < count)
            glDrawArrays(GL_TRIANGLES, first + chosen, count - first - chosen);
        if (chosen > 0) {
            uniform(p_.mesh, "u_colour", f.model_selected);
            uniform(p_.mesh, "u_offset", f.selected_offset);
            glDrawArrays(GL_TRIANGLES, first, chosen);
            uniform(p_.mesh, "u_offset", Vec3{0, 0, 0});
        }
        // The paint on the same faces, pulled towards the eye in depth so it
        // wins over them.
        if (f.show_paint && paint_.count > 0) {
            glBindVertexArray(paint_.vao);
            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(-1.0f, -4.0f);
            for (const PaintGroup& g : paint_groups_) {
                if (g.first + g.count > std::size_t(paint_.count))
                    continue;
                uniform(p_.mesh, "u_colour", g.colour);
                glDrawArrays(GL_TRIANGLES, GLint(g.first), GLsizei(g.count));
            }
            glDisable(GL_POLYGON_OFFSET_FILL);
        }
    }
    if (f.show_preview)
        draw_preview(f, vp, gbuffer, visibility);
}

void Renderer::draw_preview(const ViewFrame& f, const Mat4& vp, bool gbuffer, unsigned visibility) {
    if (!gbuffer)
        drawn_extrusions_ = 0;
    if (layer_extrusions_.size() < 2)
        return;
    const std::size_t layers = layer_extrusions_.size() - 1;
    const std::size_t lo = std::min(f.first_layer, layers - 1), hi = std::clamp(f.last_layer, lo, layers - 1);
    // All of the part drawn: what it hides stays out (docs/LARGE_MODELS.md);
    // the nearest layers first, for the depth test.
    const std::vector<PreviewRange> draws =
        preview_draws(order_, lo, hi, f.kind_mask, f.eye.z, f.top_shown, f.top_hidden);
    if (!draws.empty()) {
        const unsigned p = p_.preview;
        lit_uniforms(p, f, vp, gbuffer, visibility);
        glUniform3fv(at(p, "u_colours"), static_cast<GLsizei>(kLineKinds), &f.kind_colours[0].x);
        glUniform1ui(at(p, "u_kind_mask"), f.kind_mask);
        uniform(p, "u_colour_mode", static_cast<int>(f.color_mode));
        uniform(p, "u_moire", f.moire_filter ? 1 : 0);
        uniform(p, "u_grooves", f.mode == RenderMode::RayTraced ? 0 : 1);
        glUniform2ui(at(p, "u_layers"), static_cast<GLuint>(lo), static_cast<GLuint>(hi));
        if (f.color_mode == ColorMode::Speed)
            uniform(p, "u_range", min_feed_, max_feed_);
        else if (f.color_mode == ColorMode::Height)
            uniform(p, "u_range", min_height_, max_height_);
        else
            uniform(p, "u_range", min_width_, max_width_);
        glBindVertexArray(extrusions_.vao);
        glBindBuffer(GL_ARRAY_BUFFER, extrusions_.vbo);
    }
    for (const PreviewRange& range : draws) {
        const std::size_t first = range.first, count = range.count;
        // The draw's first instance starts the attribute arrays.
        const std::size_t base = first * sizeof(PreviewInstance);
        const GLsizei stride = sizeof(PreviewInstance);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, offset(base + offsetof(PreviewInstance, x0)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, offset(base + offsetof(PreviewInstance, x1)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, offset(base + offsetof(PreviewInstance, width)));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, offset(base + offsetof(PreviewInstance, feed)));
        glEnableVertexAttribArray(4);
        glVertexAttribIPointer(4, 1, GL_UNSIGNED_INT, stride, offset(base + offsetof(PreviewInstance, kind_layer)));
        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, stride, offset(base + offsetof(PreviewInstance, join0_x)));
        for (GLuint a = 0; a < 6; ++a)
            glVertexAttribDivisor(a, 1);
        glDrawArraysInstanced(GL_TRIANGLES, 0, 36, static_cast<GLsizei>(count));
        if (!gbuffer)
            drawn_extrusions_ += count;
    }
    if (!gbuffer && f.show_travels && travels_.count > 0 && layer_travels_.size() > hi + 1) {
        const std::size_t t0 = layer_travels_[lo], tn = layer_travels_[hi + 1] - t0;
        if (tn > 0) {
            glUseProgram(p_.line);
            uniform(p_.line, "u_view_projection", vp);
            glBindVertexArray(travels_.vao);
            glDrawArrays(GL_LINES, static_cast<GLint>(t0), static_cast<GLsizei>(tn));
        }
    }
}

// The key light's shadow map (Williams 1978): the model and the preview seen
// from the light, an orthographic camera around what is shown down to the bed.
void Renderer::shadow_pass(const ViewFrame& f) {
    shadowed_ = false;
    const bool model = f.show_model && model_box_, preview = f.show_preview && preview_box_;
    if (!shadow_fbo_ || (!model && !preview))
        return;
    Vec3 lo = model ? model_lo_ : preview_lo_, hi = model ? model_hi_ : preview_hi_;
    if (model && preview) {
        lo = Vec3{std::min(lo.x, preview_lo_.x), std::min(lo.y, preview_lo_.y), std::min(lo.z, preview_lo_.z)};
        hi = Vec3{std::max(hi.x, preview_hi_.x), std::max(hi.y, preview_hi_.y), std::max(hi.z, preview_hi_.z)};
    }
    lo.z = std::min(lo.z, 0.0f); // down to the bed, which takes the shadow
    // A margin around the box, so the soft edges' samples stay in the map.
    const float margin = 0.05f * std::max({hi.x - lo.x, hi.y - lo.y, hi.z - lo.z, 1.0f});
    lo = lo - Vec3{margin, margin, margin}, hi = hi + Vec3{margin, margin, margin};
    shadow_vp_ = shadow_view_projection(f.key, lo, hi);
    shadow_texel_ = 2.0f * length(hi - lo) / float(std::max(shadow_size_, 1));
    shadow_bias_ = 0.6f / std::max(length(hi - lo), 1.0f); // the box's depth is at most its diagonal
    glBindFramebuffer(GL_FRAMEBUFFER, shadow_fbo_);
    glViewport(0, 0, shadow_size_, shadow_size_);
    glClear(GL_DEPTH_BUFFER_BIT);
    draw_scene(f, shadow_vp_, true, white_, true);
    glViewport(0, 0, width_, height_);
    shadowed_ = true;
}

unsigned Renderer::ambient_pass(const ViewFrame& f) {
    const Mat4& m = f.projection;
    const auto common = [&](unsigned p) {
        glUseProgram(p);
        uniform(p, "u_depth", 0);
        glUniform4f(at(p, "u_projection"), m[0], m[5], m[10], m[14]);
        uniform(p, "u_size", float(width_), float(height_));
    };
    glDisable(GL_DEPTH_TEST);
    bind_texture(0, g_depth_);
    bind_texture(1, g_normal_);
    glBindFramebuffer(GL_FRAMEBUFFER, ao_fbo_[0]);
    common(p_.ao);
    uniform(p_.ao, "u_normal", 1);
    uniform(p_.ao, "u_radius", f.ao_radius);
    bind_texture(3, shadow_depth_);
    uniform(p_.ao, "u_shadow", 3);
    uniform(p_.ao, "u_shadowed", shadowed_ ? 1 : 0);
    uniform(p_.ao, "u_light", shadow_vp_);
    uniform(p_.ao, "u_world", rigid_inverse(f.view));
    uniform(p_.ao, "u_texel", shadow_texel_);
    uniform(p_.ao, "u_bias", shadow_bias_);
    screen_pass(empty_vao_);
    for (int pass = 0; pass < 2; ++pass) {
        glBindFramebuffer(GL_FRAMEBUFFER, ao_fbo_[pass + 1]);
        bind_texture(2, ao_tex_[pass]);
        common(p_.blur);
        uniform(p_.blur, "u_source", 2);
        glUniform2i(at(p_.blur, "u_step"), pass == 0 ? 1 : 0, pass == 0 ? 0 : 1); // across, then down
        uniform(p_.blur, "u_final", pass);
        screen_pass(empty_vao_);
    }
    return ao_tex_[2];
}

unsigned Renderer::ray_pass(const ViewFrame& f) {
    const Mat4& m = f.projection;
    const int previous = ray_current_, next = 1 - ray_current_;
    glDisable(GL_DEPTH_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, ray_fbo_[next]);
    const unsigned p = p_.ray;
    glUseProgram(p);
    bind_texture(0, g_depth_);
    bind_texture(1, g_normal_);
    bind_texture(2, ray_tex_[previous]);
    bind_texture(3, ray_nodes_);
    bind_texture(4, ray_prims_);
    uniform(p, "u_depth", 0);
    uniform(p, "u_normal", 1);
    uniform(p, "u_previous", 2);
    uniform(p, "u_nodes", 3);
    uniform(p, "u_primitives", 4);
    glUniform4f(at(p, "u_projection"), m[0], m[5], m[10], m[14]);
    uniform(p, "u_size", float(width_), float(height_));
    uniform(p, "u_inverse_view", rigid_inverse(f.view));
    uniform(p, "u_key", f.key);
    uniform(p, "u_sun", f.sun_radius);
    uniform(p, "u_radius", f.ao_radius);
    glUniform1ui(at(p, "u_frame"), ray_frame_++);
    uniform(p, "u_blend", 1.0f / float(ray_samples_ + 1));
    uniform(p, "u_layers", float(f.show_preview ? f.first_layer : 0), float(f.show_preview ? f.last_layer : 0));
    glUniform1ui(at(p, "u_kind_mask"), f.kind_mask);
    uniform(p, "u_ambient_rays", 2);
    screen_pass(empty_vao_);
    ray_current_ = next;
    ++ray_samples_;
    return ray_tex_[ray_current_];
}

void Renderer::draw(const ViewFrame& f) {
    if (!ready_)
        return;
    const int samples = std::clamp(f.samples, 1, max_samples_);
    resize(f.width, f.height, samples);
    const Mat4 vp = f.projection * f.view;
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glViewport(0, 0, width_, height_);

    // Nothing changed since the last picture (and no rays averaging): that
    // picture again, without the scene.
    std::vector<double> key = frame_key(f);
    const bool averaging = f.mode == RenderMode::RayTraced && has_ray_scene() && ray_samples_ < f.max_ray_samples;
    if (key != drawn_ || averaging) {
        // Whatever changes the picture starts the averaging of rays again, and
        // makes the occlusion be computed again.
        std::vector<float> signature;
        put(signature, f.view);
        put(signature, f.projection);
        put(signature, f.key);
        signature.insert(signature.end(),
                         {float(f.mode), float(f.first_layer), float(f.last_layer), float(f.kind_mask & 0xFFFF),
                          float(f.kind_mask >> 16), f.ao_radius, f.sun_radius, float(f.show_model),
                          float(f.show_preview), float(width_), float(height_), float(has_ray_scene()),
                          float(f.moire_filter)});
        const bool changed = signature != ray_signature_;
        if (changed) {
            ray_signature_ = signature;
            ray_samples_ = 0;
        }

        unsigned visibility = white_;
        const bool ray = f.mode == RenderMode::RayTraced && has_ray_scene();
        const bool ambient = f.mode == RenderMode::Ambient || (f.mode == RenderMode::RayTraced && !ray);
        const bool refresh = ray ? ray_samples_ < f.max_ray_samples : ambient && changed;
        if (refresh) {
            glBindFramebuffer(GL_FRAMEBUFFER, g_fbo_);
            glClearColor(0.5f, 0.5f, 1.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            draw_scene(f, vp, true, white_);
        }
        if (ray) {
            visibility = refresh ? ray_pass(f) : ray_tex_[ray_current_];
        } else if (ambient) {
            if (refresh)
                shadow_pass(f);
            visibility = refresh ? ambient_pass(f) : ao_tex_[2];
        }

        // The lit scene.
        glBindFramebuffer(GL_FRAMEBUFFER, samples > 1 ? msaa_fbo_ : colour_fbo_);
        glViewport(0, 0, width_, height_);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);
        glUseProgram(p_.background);
        uniform(p_.background, "u_top", f.background_top);
        uniform(p_.background, "u_bottom", f.background_bottom);
        uniform(p_.background, "u_target", float(width_), float(height_));
        screen_pass(empty_vao_);
        draw_scene(f, vp, false, visibility);
        if (samples > 1) {
            glBindFramebuffer(GL_READ_FRAMEBUFFER, msaa_fbo_);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, colour_fbo_);
            glBlitFramebuffer(0, 0, width_, height_, 0, 0, width_, height_, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        }

        drawn_ = std::move(key);
    }

    // Into the window.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(f.x, f.y, f.width, f.height);
    glEnable(GL_SCISSOR_TEST);
    glScissor(f.x, f.y, f.width, f.height);
    glDisable(GL_DEPTH_TEST);
    glUseProgram(p_.composite);
    bind_texture(0, colour_);
    uniform(p_.composite, "u_colour", 0);
    uniform(p_.composite, "u_origin", float(f.x), float(f.y));
    uniform(p_.composite, "u_size", float(width_), float(height_));
    uniform(p_.composite, "u_fxaa", f.fxaa ? 1 : 0);
    screen_pass(empty_vao_);

    glDisable(GL_SCISSOR_TEST);
    glBindVertexArray(0);
    glUseProgram(0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

std::vector<std::uint8_t> Renderer::read_colour() {
    if (!ready_)
        return {};
    std::vector<std::uint8_t> out(std::size_t(width_) * std::size_t(height_) * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glBindFramebuffer(GL_FRAMEBUFFER, colour_fbo_);
    glReadPixels(0, 0, width_, height_, GL_RGBA, GL_UNSIGNED_BYTE, out.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return out;
}

RayReadback Renderer::read_back() {
    if (!ready_)
        return {};
    RayReadback r;
    r.width = width_, r.height = height_;
    const std::size_t n = std::size_t(width_) * std::size_t(height_);
    r.depth.resize(n);
    r.normal.resize(n * 4);
    r.visibility.resize(n * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glBindFramebuffer(GL_FRAMEBUFFER, g_fbo_);
    glReadPixels(0, 0, width_, height_, GL_DEPTH_COMPONENT, GL_FLOAT, r.depth.data());
    glReadPixels(0, 0, width_, height_, GL_RGBA, GL_UNSIGNED_BYTE, r.normal.data());
    glBindFramebuffer(GL_FRAMEBUFFER, ray_fbo_[ray_current_]);
    glReadPixels(0, 0, width_, height_, GL_RGBA, GL_FLOAT, r.visibility.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return r;
}

} // namespace tisma::ui
