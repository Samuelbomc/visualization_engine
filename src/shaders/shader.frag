// =============================================================================
// shader.frag
// Fragment shader principal del renderer.
//
// Recibe el color interpolado desde el vertex shader (el rasterizador interpola
// linealmente entre los tres vértices de cada triángulo) y lo escribe en el
// framebuffer como color final del píxel.
//
// Con MSAA activo, este shader se ejecuta una vez por fragmento pero el
// resultado se aplica a múltiples muestras según la cobertura del triángulo,
// produciendo bordes suavizados.
// =============================================================================

#version 450

// Color interpolado recibido del vertex shader.
// El rasterizador calcula la interpolación baricéntrica automáticamente.
layout(location = 0) in vec3 fragColor;

// Color de salida que se escribe en el attachment de color del framebuffer.
// Location 0 corresponde al primer (y único) color attachment del render pass.
layout(location = 0) out vec4 outColor;

void main() {
    // Escribir el color RGB con alpha = 1.0 (completamente opaco).
    // El blending está desactivado en el pipeline, así que este valor
    // reemplaza directamente lo que hubiera en el framebuffer.
    outColor = vec4(fragColor, 1.0);
}