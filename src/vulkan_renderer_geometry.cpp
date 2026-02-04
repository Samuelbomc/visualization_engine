#include "main_app.hpp"

// Geometría de un cuadrado centrado en el origen.
const std::vector<Vertex> vertices = {
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}}, // 0: Arriba-Izquierda (Rojo)
    {{ 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}}, // 1: Arriba-Derecha (Verde)
    {{ 0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}}, // 2: Abajo-Derecha (Azul)
    {{-0.5f,  0.5f}, {1.0f, 1.0f, 1.0f}}  // 3: Abajo-Izquierda (Blanco)
};

const std::vector<uint16_t> indices = {
    0, 1, 2, // Primer triángulo
    2, 3, 0  // Segundo triángulo
};