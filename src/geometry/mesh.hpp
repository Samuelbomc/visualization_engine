#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <array>
#include <cstdint>
#include <glm/glm.hpp>

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;

    // Ayudante: describe la separación de los vértices en memoria.
    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }

    // Ayudante: describe cómo se organizan los atributos (pos, color).
    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

        // Posición (Location 0 en el shader)
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT; // vec3
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        // Color (Location 1 en el shader)
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT; // vec3
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        return attributeDescriptions;
    }
};

struct GeometryData {
    std::vector<uint8_t> vertexData;
    VkVertexInputBindingDescription bindingDescription{};
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    std::vector<uint8_t> indexData;
    VkIndexType indexType = VK_INDEX_TYPE_UINT16;
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
};

class Mesh {
public:
    Mesh() = default;
    explicit Mesh(const GeometryData& data);
    explicit Mesh(GeometryData&& data);

    void setData(const GeometryData& data);
    void setData(GeometryData&& data);
    const GeometryData& getData() const { return data; }

private:
    GeometryData data;
};