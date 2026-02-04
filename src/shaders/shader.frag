#version 450

// Input from Vertex Shader
layout(location = 0) in vec3 fragColor;

// Output to Framebuffer (Location 0)
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(fragColor, 1.0); // R, G, B, Alpha (1.0 = opaque)
}