#version 450

// Atributos de entrada (posición, color)
layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;

// Salida al fragment shader
layout(location = 0) out vec3 fragColor;

// UNIFORM BUFFER
layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

void main() {
    // Proj * View * Model * Posición
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 0.0, 1.0);
    fragColor = inColor;
}