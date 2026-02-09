// =============================================================================
// shader.vert
// Vertex shader principal del renderer.
//
// Recibe los atributos de cada vértice (posición y color) y los transforma
// desde espacio local del objeto hasta espacio de clip de Vulkan, aplicando
// las tres matrices de transformación del Uniform Buffer Object:
//
//   gl_Position = proyección × vista × modelo × posición
//
// El color del vértice se pasa directamente al fragment shader, donde será
// interpolado automáticamente por el rasterizador entre los tres vértices
// de cada triángulo (interpolación baricéntrica).
// =============================================================================

#version 450

// Atributos de entrada por vértice, vinculados al binding 0 del vertex buffer.
// Las ubicaciones (location) corresponden a las VkVertexInputAttributeDescription
// configuradas en el pipeline gráfico.
layout(location = 0) in vec3 inPosition;  // Posición en espacio local (x, y, z)
layout(location = 1) in vec3 inColor;     // Color RGB del vértice

// Variable de salida hacia el fragment shader.
// El rasterizador interpola este valor entre los vértices del triángulo.
layout(location = 0) out vec3 fragColor;

// Uniform Buffer Object (UBO) vinculado al binding 0 del descriptor set.
// Contiene las matrices de transformación actualizadas cada frame por la CPU.
// El layout std140 es implícito en Vulkan cuando se usa alignas(16) en C++.
layout(binding = 0) uniform UniformBufferObject {
    mat4 model;  // Espacio local → espacio del mundo (posición, rotación, escala)
    mat4 view;   // Espacio del mundo → espacio de la cámara
    mat4 proj;   // Espacio de la cámara → espacio de clip (perspectiva)
} ubo;

void main() {
    // Multiplicar las matrices en orden: primero modelo (local→mundo),
    // luego vista (mundo→cámara), finalmente proyección (cámara→clip).
    // vec4(..., 1.0) convierte la posición 3D en coordenada homogénea.
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);

    // Pasar el color al fragment shader sin modificación.
    fragColor = inColor;
}