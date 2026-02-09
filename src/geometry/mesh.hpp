// =============================================================================
// mesh.hpp
// Definición de las estructuras de datos de geometría y la clase Mesh.
//
// Vertex: estructura de vértice con posición (vec3) y color (vec3), incluyendo
// métodos estáticos para generar las descripciones de Vulkan del layout.
//
// GeometryData: contenedor genérico que almacena datos de geometría en formato
// crudo (bytes), junto con la descripción del layout de vértices, topología,
// datos de índices y sus conteos. Esto permite transportar cualquier formato
// de vértice sin acoplarse a una estructura concreta.
//
// Mesh: envoltorio simple sobre GeometryData que proporciona semántica de
// copia y movimiento para pasar geometría al renderer.
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <array>
#include <cstdint>
#include <glm/glm.hpp>

// Estructura de vértice con posición 3D y color RGB.
// Los métodos estáticos generan las descripciones de Vulkan necesarias para
// configurar el vertex input del pipeline gráfico.
struct Vertex {
    glm::vec3 pos;   // Posición del vértice en espacio local
    glm::vec3 color; // Color RGB del vértice (interpolado entre vértices)

    // Describe cómo están organizados los vértices en memoria: un solo binding
    // (0) con stride igual al tamaño de Vertex, avanzando por vértice.
    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }

    // Describe la ubicación de cada atributo dentro del vértice:
    //   - Location 0: posición (vec3 float, offset 0 bytes desde el inicio)
    //   - Location 1: color (vec3 float, offset después de pos)
    // Estas locations corresponden a las declaraciones "layout(location = N)"
    // en el vertex shader GLSL.
    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        return attributeDescriptions;
    }
};

// Contenedor genérico de geometría que almacena los datos de vértices e índices
// como vectores de bytes crudos, junto con toda la metadata necesaria para
// configurar el pipeline de Vulkan.
// Esta abstracción permite que el renderer acepte cualquier formato de vértice
// sin conocer la estructura concreta: solo necesita el binding description,
// los attribute descriptions y la topología.
struct GeometryData {
    // Datos de vértices en formato crudo (bytes). El tamaño total debe ser
    // divisible por bindingDescription.stride.
    std::vector<uint8_t> vertexData;

    // Descripción del binding: stride (bytes entre vértices consecutivos),
    // binding index y input rate (por vértice o por instancia).
    VkVertexInputBindingDescription bindingDescription{};

    // Descripciones de los atributos: location, formato, offset dentro del vértice.
    // Define cómo el vertex shader lee cada campo (posición, color, normal, etc.).
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions;

    // Topología de la geometría: cómo se interpretan los vértices
    // (TRIANGLE_LIST, TRIANGLE_STRIP, LINE_LIST, etc.).
    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // Datos de índices en formato crudo. Si está vacío, se usa dibujo directo
    // (vkCmdDraw) en lugar de indexado (vkCmdDrawIndexed).
    std::vector<uint8_t> indexData;

    // Tipo de los índices: UINT16 (máx 65535 vértices) o UINT32 (máx ~4 mil millones).
    VkIndexType indexType = VK_INDEX_TYPE_UINT16;

    // Número de vértices, calculado como vertexData.size() / stride.
    uint32_t vertexCount = 0;

    // Número de índices, calculado como indexData.size() / sizeof(tipo_indice).
    uint32_t indexCount = 0;
};

// Envoltorio sobre GeometryData que proporciona semántica de valor con
// copia y movimiento. El renderer recibe objetos Mesh y extrae su
// GeometryData para crear los buffers de GPU.
class Mesh {
public:
    Mesh() = default;

    // Construye una malla copiando los datos de geometría.
    explicit Mesh(const GeometryData& data);

    // Construye una malla moviendo los datos de geometría (evita copias
    // innecesarias de los vectores de vértices/índices).
    explicit Mesh(GeometryData&& data);

    // Reemplaza los datos de geometría (copia).
    void setData(const GeometryData& data);

    // Reemplaza los datos de geometría (movimiento).
    void setData(GeometryData&& data);

    // Acceso de solo lectura a los datos de geometría almacenados.
    const GeometryData& getData() const { return data; }

private:
    GeometryData data;
};