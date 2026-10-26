// The light every lit surface shares, as renderer.cpp's kLighting: a sky and
// a ground (hemisphere light), a key light that ray tracing can shadow, a weak
// fill light from the other side, Blinn-Phong highlights with Schlick's
// Fresnel term (energy-normalized with (n + 8) / 8 pi), a faint rim, an
// exponential tone curve and the sRGB gamma. The visibility texture holds, per
// pixel, how much of the key light (r) and of the ambient light (g) arrives.
// With the G-buffer flag, the shaders write the view-space normal instead.
layout(set = 0, binding = 1) uniform sampler2D visibility;
layout(location = 0) out vec4 colour;

vec3 to_linear(vec3 c) { return pow(max(c, vec3(0.0)), vec3(2.2)); }

bool gbuffer(vec3 n) {
    if (frame.flags.x == 0)
        return false;
    vec3 nv = normalize(mat3(frame.view) * n);
    if (nv.z < 0.0)
        nv = -nv;
    colour = vec4(nv * 0.5 + 0.5, 1.0);
    return true;
}

vec3 shade(vec3 base, vec3 n, vec3 p, float shininess, float specular) {
    vec4 vis = texture(visibility, gl_FragCoord.xy / frame.target.xy);
    if (frame.flags.y != 0)
        return vec3(vis.g, vis.r * vis.g, vis.g);
    float direct = vis.r;
    float ambient = mix(1.0, vis.g, frame.target.z);
    vec3 v = normalize(frame.eye.xyz - p);
    if (dot(n, v) < 0.0)
        n = -n;
    vec3 key = frame.key.xyz;
    float ndl = max(dot(n, key), 0.0);
    vec3 h = normalize(key + v);
    float fresnel = 0.04 + 0.96 * pow(1.0 - max(dot(h, v), 0.0), 5.0);
    float highlight = specular * (shininess + 8.0) / 25.1327 * pow(max(dot(n, h), 0.0), shininess) * fresnel;
    vec3 sky = to_linear(frame.sky.rgb), ground = to_linear(frame.ground.rgb);
    vec3 hemisphere = mix(ground, sky, 0.5 + 0.5 * n.z);
    vec3 c = base * hemisphere * 0.32 * ambient
           + (base * 0.95 + vec3(highlight)) * ndl * direct
           + base * max(dot(n, frame.fill.xyz), 0.0) * 0.18 * ambient
           + sky * pow(1.0 - max(dot(n, v), 0.0), 4.0) * 0.06 * ambient;
    c = vec3(1.0) - exp(-c * 1.25);
    return pow(c, vec3(1.0 / 2.2));
}
