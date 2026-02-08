#include "vulkan_renderer.hpp"
#include <cstring>
#include <stdexcept>

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

static bool areBindingsEqual(const VkVertexInputBindingDescription& a, const VkVertexInputBindingDescription& b) {
    return a.binding == b.binding && a.stride == b.stride && a.inputRate == b.inputRate;
}

static bool areAttributesEqual(const std::vector<VkVertexInputAttributeDescription>& a, const std::vector<VkVertexInputAttributeDescription>& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); i++) {
        if (a[i].binding != b[i].binding ||
            a[i].location != b[i].location ||
            a[i].format != b[i].format ||
            a[i].offset != b[i].offset) {
            return false;
        }
    }
    return true;
}

GeometryData VulkanRenderer::createDefaultGeometry() {
    GeometryData data{};
    data.bindingDescription = Vertex::getBindingDescription();
    auto attributeArray = Vertex::getAttributeDescriptions();
    data.attributeDescriptions = std::vector<VkVertexInputAttributeDescription>(attributeArray.begin(), attributeArray.end());
    data.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    data.indexType = VK_INDEX_TYPE_UINT16;

    std::vector<Vertex> defaultVertices = {
        {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
        {{ 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
        {{ 0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},
        {{-0.5f,  0.5f}, {1.0f, 1.0f, 1.0f}}
    };

    data.vertexData.resize(defaultVertices.size() * sizeof(Vertex));
    std::memcpy(data.vertexData.data(), defaultVertices.data(), data.vertexData.size());

    std::vector<uint16_t> defaultIndices = { 0, 1, 2, 2, 3, 0 };
    data.indexData.resize(defaultIndices.size() * sizeof(uint16_t));
    std::memcpy(data.indexData.data(), defaultIndices.data(), data.indexData.size());

    data.vertexCount = static_cast<uint32_t>(defaultVertices.size());
    data.indexCount = static_cast<uint32_t>(defaultIndices.size());
    return data;
}

bool VulkanRenderer::isVertexLayoutDifferent(const GeometryData& other) const {
    if (!areBindingsEqual(geometry.bindingDescription, other.bindingDescription)) {
        return true;
    }
    return !areAttributesEqual(geometry.attributeDescriptions, other.attributeDescriptions);
}

void VulkanRenderer::setGeometry(const GeometryData& newGeometry) {
    GeometryData validated = newGeometry;

    if (validated.bindingDescription.stride == 0) {
        throw std::runtime_error("La geometría debe definir un stride válido.");
    }
    if (validated.vertexData.empty()) {
        throw std::runtime_error("La geometría debe tener vertexData.");
    }
    if (validated.vertexData.size() % validated.bindingDescription.stride != 0) {
        throw std::runtime_error("El tamaño de vertexData no coincide con el stride.");
    }

    validated.vertexCount = static_cast<uint32_t>(validated.vertexData.size() / validated.bindingDescription.stride);

    if (!validated.indexData.empty()) {
        uint32_t indexStride = (validated.indexType == VK_INDEX_TYPE_UINT32) ? sizeof(uint32_t) : sizeof(uint16_t);
        if (validated.indexData.size() % indexStride != 0) {
            throw std::runtime_error("El tamaño de indexData no coincide con el tipo de índice.");
        }
        validated.indexCount = static_cast<uint32_t>(validated.indexData.size() / indexStride);
    }
    else {
        validated.indexCount = 0;
    }

    bool recreatePipeline = isVertexLayoutDifferent(validated) || geometry.topology != validated.topology;

    vkDeviceWaitIdle(device);

    if (vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, vertexBuffer, nullptr);
        vertexBuffer = VK_NULL_HANDLE;
    }
    if (vertexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, vertexBufferMemory, nullptr);
        vertexBufferMemory = VK_NULL_HANDLE;
    }
    if (indexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, indexBuffer, nullptr);
        indexBuffer = VK_NULL_HANDLE;
    }
    if (indexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, indexBufferMemory, nullptr);
        indexBufferMemory = VK_NULL_HANDLE;
    }

    geometry = std::move(validated);

    if (recreatePipeline) {
        recreateGraphicsPipeline();
    }

    createVertexBuffer();
    if (geometry.indexCount > 0) {
        createIndexBuffer();
    }
}