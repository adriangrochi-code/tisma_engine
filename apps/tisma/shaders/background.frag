#version 450
#extension GL_GOOGLE_include_directive : require
#include "view_common.glsl"
layout(location = 0) out vec4 colour;
// Top to bottom; Vulkan's window rows go down.
void main() { colour = vec4(mix(draw.extra.rgb, draw.colour.rgb, 1.0 - gl_FragCoord.y / frame.target.y), 1.0); }
