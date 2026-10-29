// What every shader of the Vulkan view shares (apps/tisma/vk_view.cpp): the
// frame's values and each draw's. The matrices are OpenGL's (y up, depth -1
// to 1); to_vulkan turns a position into Vulkan's clip space (y down, depth 0
// to 1), so the depth buffer holds what OpenGL's does. (Flipping the viewport
// instead would do the same, but lavapipe then clips the lines that pass
// behind the camera wrongly.)
layout(set = 0, binding = 0, std140) uniform Frame {
    mat4 view_projection;
    mat4 view;
    vec4 eye, key, fill;  // xyz: the eye, towards the key and the fill lights
    vec4 sky, ground;     // rgb: the ambient light's colours, sRGB
    vec4 target;          // xy: the view's size in pixels; z: the ambient occlusion's strength
    ivec4 flags;          // x: write the G-buffer, y: show the visibility, z: the moiré filter
    vec4 grid;            // x: mm between grid lines, y: every how many lines a major one
    vec4 colours[16];     // the line kinds' colours
} frame;

layout(push_constant) uniform Draw {
    vec4 colour;   // a surface's colour; the background's top
    vec4 extra;    // xyz: the model drawn this far from its mesh; the background's bottom
    uvec4 preview; // x: the line kinds drawn (bit k), y: colour by (0 kind, 1 speed, 2 width, 3 height),
                   // z, w: the first and the last layer shown
    vec4 range;    // xy: the colour ramp's range; z: 1 with the beads' grooves
} draw;

vec4 to_vulkan(vec4 p) {
    p.y = -p.y;
    p.z = 0.5 * (p.z + p.w);
    return p;
}
