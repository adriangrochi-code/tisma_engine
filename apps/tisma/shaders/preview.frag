#version 450
#extension GL_GOOGLE_include_directive : require
#include "view_common.glsl"
#include "view_lighting.glsl"
// The bead's light and the moiré filter, as renderer.cpp's kPreviewFragment:
// the normal follows a printed bead's profile (flat on top and bottom, round at
// the sides, Comminal et al. 2018); where a line covers less than two pixels,
// the normal fades to the mean of its profile as seen from the eye and the
// highlight widens (Toksvig 2005).
layout(location = 0) in vec3 v_base;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec3 v_position;
layout(location = 3) in vec2 v_section;
layout(location = 4) flat in vec3 v_side;
layout(location = 5) flat in vec3 v_up;
layout(location = 6) flat in vec2 v_size;
layout(location = 7) flat in float v_cap;
layout(location = 8) flat in vec2 v_neighbours;

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

void main() {
    vec3 n = normalize(v_normal);
    vec2 seen = vec2(1.0);
    if (v_cap < 0.5) {
        vec2 b = bead_normal(v_section, v_size);
        n = normalize(v_side * b.x + v_up * b.y);
        seen = bead_seen(v_section, v_size, v_side, v_up, frame.key.xyz, v_neighbours);
    }
    float shininess = 40.0, specular = 0.35;
    if (frame.flags.z != 0) {
        vec2 footprint = fwidth(v_section);
        float k = smoothstep(0.3, 0.8, max(footprint.x, footprint.y));
        // The grooves are a thin part of the bead: they average sooner, by
        // some five pixels a bead.
        float kg = smoothstep(0.1, 0.4, max(footprint.x, footprint.y));
        if (kg > 0.0) {
            vec3 v = normalize(frame.eye.xyz - v_position);
            vec3 sum = vec3(0.0);
            vec2 seen_sum = vec2(0.0);
            float weight = 0.0;
            for (int i = 0; i < 16; ++i) {
                float a = (float(i) + 0.5) / 16.0 * 6.2831853;
                vec2 c = vec2(cos(a), sin(a));
                c /= abs(c.x) + abs(c.y);
                vec2 b = bead_normal(c, v_size);
                vec3 m = normalize(v_side * b.x + v_up * b.y);
                float facing = max(dot(m, v), 0.0);
                sum += m * facing;
                seen_sum += bead_seen(c, v_size, v_side, v_up, frame.key.xyz, v_neighbours) * facing;
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
