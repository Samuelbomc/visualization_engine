#version 450

// Input Attributes (Position, Color)
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

// Output to Fragment Shader
layout(location = 0) out vec3 fragColor;

// UNIFORM BUFFER
layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    fragColor = inColor;
}