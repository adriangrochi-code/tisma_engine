#version 450
// A triangle that covers the screen.
void main() {
    vec2 p = vec2(float((gl_VertexIndex & 1) << 2) - 1.0, float((gl_VertexIndex & 2) << 1) - 1.0);
    gl_Position = vec4(p, 0.0, 1.0);
}
