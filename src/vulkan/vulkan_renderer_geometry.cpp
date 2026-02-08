#include "vulkan_renderer.hpp"
#include <cstring>
#include <stdexcept>

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

bool VulkanRenderer::isVertexLayoutDifferent(const GeometryData& other) const {
    const auto& current = mesh.getData();
    if (!areBindingsEqual(current.bindingDescription, other.bindingDescription)) {
        return true;
    }
    return !areAttributesEqual(current.attributeDescriptions, other.attributeDescriptions);
}

void VulkanRenderer::setMesh(const Mesh& newMesh) {
    GeometryData validated = newMesh.getData();

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

    bool recreatePipeline = isVertexLayoutDifferent(validated) || mesh.getData().topology != validated.topology;

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

    mesh = Mesh(std::move(validated));

    if (graphicsPipeline == VK_NULL_HANDLE) {
        createGraphicsPipeline();
    }
    else if (recreatePipeline) {
        recreateGraphicsPipeline();
    }

    createVertexBuffer();
    if (mesh.getData().indexCount > 0) {
        createIndexBuffer();
    }
}