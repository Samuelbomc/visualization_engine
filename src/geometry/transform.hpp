// =============================================================================
// transform.hpp
// Estructura que agrupa las tres matrices de transformación 4x4 necesarias
// para posicionar y proyectar la geometría en el espacio de la pantalla.
//
// Estas matrices se copian al Uniform Buffer Object (UBO) cada frame y son
// leídas por el vertex shader para transformar cada vértice:
//   gl_Position = proj * view * model * vec4(posición, 1.0)
// =============================================================================

#pragma once

#include <glm/glm.hpp>

struct TransformData {
    // Matriz de modelo: posiciona y orienta el objeto en el espacio del mundo.
    // Incluye translación, rotación y escala del objeto.
    glm::mat4 model;

    // Matriz de vista: posiciona la cámara en el mundo. Transforma las
    // coordenadas del mundo a coordenadas relativas a la cámara.
    // Se genera típicamente con glm::lookAt(posición, objetivo, arriba).
    glm::mat4 view;

    // Matriz de proyección: define cómo se proyecta la escena 3D en la
    // pantalla 2D (perspectiva o ortográfica). En Vulkan, el eje Y se
    // invierte (proj[1][1] *= -1) porque el clip space tiene Y hacia abajo.
    glm::mat4 proj;
};