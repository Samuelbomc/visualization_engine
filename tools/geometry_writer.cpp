// =============================================================================
// geometry_writer.cpp
// Proceso externo que escribe geometría y transformaciones en memoria compartida
// para que el renderer Vulkan las consuma mediante IPC (Inter-Process Communication).
//
// Flujo de comunicación:
//   1. Crea un mapeo de memoria compartida (CreateFileMappingW).
//   2. Escribe la geometría del cubo (vértices + índices) una sola vez.
//   3. En un bucle infinito, actualiza la transformación (rotación animada)
//      y la escribe en la memoria compartida.
//   4. El renderer lee la memoria compartida cada frame con tryRead().
//
// Protocolo de escritura atómica:
//   - Se usa un número de secuencia (sequence) con semántica seqlock:
//     a. Se incrementa a un valor impar (indica escritura en curso).
//     b. Se escribe el payload (geometría + transformación).
//     c. Se incrementa a un valor par (indica escritura completada).
//   - El lector solo acepta datos cuando sequence es par y ha cambiado.
//   - MemoryBarrier() asegura el ordenamiento de las escrituras.
// =============================================================================

#include "ipc/shared_geometry.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <windows.h>
#include <chrono>
#include <thread>
#include <vector>
#include <cstring>
#include <iostream>

// -----------------------------------------------------------------------------
// writeSharedGeometry: escribe una actualización completa en el buffer compartido.
// Si writeGeometry es true, incluye los datos de vértices/índices con su
// descripción de layout (binding, atributos, topología, tipo de índice).
// Siempre escribe la transformación (modelo, vista, proyección).
// Usa el protocolo seqlock para garantizar lecturas consistentes sin mutex.
// -----------------------------------------------------------------------------
static void writeSharedGeometry(SharedGeometryBuffer* buffer,
    const std::vector<Vertex>& vertices,
    const std::vector<uint16_t>& indices,
    const TransformData& transform,
    bool writeGeometry) {

    // Marcar secuencia como impar → escritura en curso
    const uint32_t seq = buffer->header.sequence;
    buffer->header.sequence = seq + 1;
    MemoryBarrier();

    buffer->header.magic = SharedGeometryMagic;
    buffer->header.version = SharedGeometryVersion;
    buffer->header.hasGeometry = writeGeometry ? 1u : 0u;
    buffer->header.hasTransform = 1;

    if (writeGeometry) {
        // Configurar la descripción del layout de vértices
        buffer->header.vertexStride = sizeof(Vertex);
        buffer->header.vertexCount = static_cast<uint32_t>(vertices.size());
        buffer->header.indexCount = static_cast<uint32_t>(indices.size());
        buffer->header.indexType = VK_INDEX_TYPE_UINT16;
        buffer->header.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        // Descripción del binding: un solo binding con stride de Vertex
        buffer->header.attributeCount = 2;
        buffer->header.bindingDescription.binding = 0;
        buffer->header.bindingDescription.stride = sizeof(Vertex);
        buffer->header.bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        // Atributo 0: posición (vec3 float, offset 0)
        buffer->header.attributes[0].location = 0;
        buffer->header.attributes[0].binding = 0;
        buffer->header.attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        buffer->header.attributes[0].offset = offsetof(Vertex, pos);

        // Atributo 1: color (vec3 float, offset después de pos)
        buffer->header.attributes[1].location = 1;
        buffer->header.attributes[1].binding = 0;
        buffer->header.attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        buffer->header.attributes[1].offset = offsetof(Vertex, color);

        // Copiar datos crudos de vértices e índices
        std::memcpy(buffer->vertexData, vertices.data(), vertices.size() * sizeof(Vertex));
        std::memcpy(buffer->indexData, indices.data(), indices.size() * sizeof(uint16_t));
    }
    else {
        buffer->header.vertexStride = 0;
        buffer->header.vertexCount = 0;
        buffer->header.indexCount = 0;
        buffer->header.attributeCount = 0;
    }

    // Copiar las matrices de transformación como arrays de 16 floats
    std::memcpy(buffer->header.model, glm::value_ptr(transform.model), sizeof(float) * 16);
    std::memcpy(buffer->header.view, glm::value_ptr(transform.view), sizeof(float) * 16);
    std::memcpy(buffer->header.proj, glm::value_ptr(transform.proj), sizeof(float) * 16);

    // Marcar secuencia como par → escritura completada
    MemoryBarrier();
    buffer->header.sequence = seq + 2;
}

// -----------------------------------------------------------------------------
// main: punto de entrada del proceso escritor.
// Crea la memoria compartida, define un cubo con 8 vértices y 36 índices
// (6 caras × 2 triángulos × 3 vértices), y anima una rotación continua
// sobre el eje Y a 45°/s.
//
// La geometría se escribe una sola vez (hasta que el consumidor la confirme
// via consumerSequence); después, solo se actualiza la transformación para
// reducir el ancho de banda de la memoria compartida.
// -----------------------------------------------------------------------------
int main() {
    HANDLE mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
        sizeof(SharedGeometryBuffer), SharedGeometryMappingName);
    if (!mapping) {
        std::cerr << "Failed to create shared memory.\n";
        return 1;
    }

    auto* buffer = static_cast<SharedGeometryBuffer*>(
        MapViewOfFile(mapping, FILE_MAP_WRITE, 0, 0, sizeof(SharedGeometryBuffer)));

    if (!buffer) {
        CloseHandle(mapping);
        std::cerr << "Failed to map shared memory.\n";
        return 1;
    }

    std::memset(buffer, 0, sizeof(SharedGeometryBuffer));

    // Definición del cubo: 8 vértices con posición y color
    std::vector<Vertex> vertices = {
        {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
        {{ 0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}},
        {{-0.5f,  0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}},
        {{-0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 1.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 1.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {1.0f, 1.0f, 1.0f}},
        {{-0.5f,  0.5f,  0.5f}, {0.2f, 0.2f, 0.2f}}
    };

    // Índices del cubo: 6 caras × 2 triángulos, con winding CCW visto desde
    // fuera del cubo para funcionar con backface culling (CULL_MODE_BACK_BIT).
    std::vector<uint16_t> indices = {
        0, 2, 1, 0, 3, 2,   // Cara frontal (z = -0.5, vista desde -Z)
        4, 5, 6, 4, 6, 7,   // Cara trasera (z = +0.5, vista desde +Z)
        0, 4, 7, 0, 7, 3,   // Cara izquierda (x = -0.5, vista desde -X)
        1, 2, 6, 1, 6, 5,   // Cara derecha (x = +0.5, vista desde +X)
        3, 7, 6, 3, 6, 2,   // Cara superior (y = +0.5, vista desde +Y)
        0, 1, 5, 0, 5, 4    // Cara inferior (y = -0.5, vista desde -Y)
    };

    auto start = std::chrono::high_resolution_clock::now();
    auto last = start;
    float angle = 0.0f;
    const float rotationSpeed = glm::radians(45.0f);

    bool geometryAcked = false;
    uint32_t lastGeometrySequence = 0;

    while (true) {
        // Calcular delta time para rotación independiente del framerate
        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(now - last).count();
        last = now;

        angle += rotationSpeed * dt;
        if (angle > glm::two_pi<float>()) {
            angle -= glm::two_pi<float>();
        }

        // Construir la transformación: rotación sobre Y, cámara fija, perspectiva
        TransformData transform{};
        transform.model = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0.0f, 1.0f, 0.0f));
        transform.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 1.0f));
        transform.proj = glm::perspective(glm::radians(45.0f), 16.0f / 9.0f, 0.1f, 10.0f);
        transform.proj[1][1] *= -1; // Corrección del eje Y de Vulkan

        // Escribir geometría hasta que el consumidor la confirme,
        // luego solo actualizar la transformación
        if (!geometryAcked) {
            writeSharedGeometry(buffer, vertices, indices, transform, true);
            lastGeometrySequence = buffer->header.sequence;
            MemoryBarrier();
            geometryAcked = buffer->header.consumerSequence == lastGeometrySequence;
        }
        else {
            writeSharedGeometry(buffer, vertices, indices, transform, false);
        }

        // Limitar a ~60 actualizaciones por segundo
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}